#include "app.h"
#include "autosave.h"
#include "core/iogate.h"
#include "settingsio.h"

#include <KConfigGroup>
#include <KNotification>
#include <LayerShellQt/Window>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QJsonObject>
#include <QMovie>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>

namespace
{
App *s_instance = nullptr;

const QString KWinScriptName = QStringLiteral("kde-wallpaper-slideshow-fullscreen");

// Takes or releases a freedesktop Inhibit(application, reason) / UnInhibit(cookie) inhibition.
void setInhibited(const QString &service, const QString &path, bool on, uint &cookie)
{
    if (on == (cookie != 0)) {
        return;
    }
    QDBusInterface iface(service, path, service);
    if (on) {
        const QDBusReply<uint> reply = iface.call(QStringLiteral("Inhibit"), QStringLiteral("Wallpaper Slideshow"), QStringLiteral("Playing a slideshow"));
        if (reply.isValid()) {
            cookie = reply.value();
        } else {
            qWarning("Cannot inhibit %s: %s", qPrintable(service), qPrintable(reply.error().message()));
        }
    } else {
        iface.call(QStringLiteral("UnInhibit"), cookie);
        cookie = 0;
    }
}
}

App::App(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
    s_instance = this;
    m_state.setWriter([this](KSharedConfig &config) {
        writeState(config);
    });

    m_elapsedSave.setInterval(5000);
    connect(&m_elapsedSave, &QTimer::timeout, this, [this] {
        if (m_running && !m_hidden) {
            m_state.markDirty();
        }
    });

    Settings *s = Settings::self();
    saveOnChange(s);
    connect(s, &Settings::separateScreensChanged, this, [this] {
        destroyAllViews();
        updateProfiles();
        updateViews();
    });
    connect(s, &Settings::disabledScreensChanged, this, &App::updateViews);
    connect(s, &Settings::watchChangesChanged, this, [this] {
        for (Profile *profile : std::as_const(m_profiles)) {
            profile->startScan(); // the scan result sets up or drops the watches
        }
    });
    connect(s, &Settings::pauseOnLockChanged, this, &App::updateSuspension);
    connect(s, &Settings::pauseOnFullscreenChanged, this, &App::updateFullscreenWatch);
    connect(s, &Settings::preventSleepChanged, this, &App::updateSuspension);
    connect(s, &Settings::preventScreenOffChanged, this, &App::updateSuspension);
    connect(s, &Settings::autostartChanged, this, &App::writeAutostart);

    connect(qApp, &QGuiApplication::screenAdded, this, [this] {
        updateProfiles();
        updateViews();
        Q_EMIT screensChanged();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this](QScreen *screen) {
        destroyView(screen);
        // Qt still lists the screen while this signal is delivered.
        QMetaObject::invokeMethod(this, [this] {
            updateProfiles();
            Q_EMIT screensChanged();
        }, Qt::QueuedConnection);
    });
    // Audio follows the primary screen.
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this] {
        destroyAllViews();
        updateViews();
        Q_EMIT profilesChanged();
    });

    QDBusConnection::sessionBus().connect(QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("/ScreenSaver"),
                                          QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("ActiveChanged"),
                                          this,
                                          SLOT(setLocked(bool)));

    QDBusConnection::systemBus().connect(QStringLiteral("org.freedesktop.login1"),
                                         QStringLiteral("/org/freedesktop/login1"),
                                         QStringLiteral("org.freedesktop.login1.Manager"),
                                         QStringLiteral("PrepareForSleep"),
                                         this,
                                         SLOT(prepareForSleep(bool)));
    takeSleepLock();

    connect(qApp, &QGuiApplication::commitDataRequest, &m_state, &StateStore::flush);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        m_state.flush();
        Settings::self()->save();
        if (m_fullscreenWatch) {
            QDBusInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"))
                .call(QStringLiteral("unloadScript"), KWinScriptName);
        }
    });
}

App::~App()
{
    destroyAllViews();
    delete m_settingsWindow;
    qDeleteAll(m_profiles);
    s_instance = nullptr;
}

App *App::create(QQmlEngine *, QJSEngine *)
{
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

void App::start()
{
    const KConfigGroup general = m_state.config().group(QStringLiteral("General"));
    m_running = general.readEntry("running", true);
    m_hidden = general.readEntry("overlayHidden", false);
    const bool windowVisible = general.readEntry("windowVisible", true);

    updateProfiles();
    updateViews();

    QQmlComponent component(m_engine, "Slideshow", "SettingsWindow");
    m_settingsWindow = qobject_cast<QQuickWindow *>(component.create());
    if (!m_settingsWindow) {
        qFatal("Cannot create the settings window: %s", qPrintable(component.errorString()));
    }
    connect(m_settingsWindow, &QWindow::visibleChanged, &m_state, &StateStore::markDirty);

    writeAutostart();
    updateFullscreenWatch();
    m_elapsedSave.start();

    const bool folderError = std::any_of(m_profiles.cbegin(), m_profiles.cend(), [](const Profile *p) {
        return !p->folderError().isEmpty();
    });
    if (m_state.isFirstLaunch() || windowVisible || folderError) {
        showSettings();
    }
    m_state.markDirty();
}

QString App::profileId(const QScreen *screen) const
{
    return Settings::separateScreens() && screen ? screen->name() : Profile::DefaultId;
}

Profile *App::profileFor(const QScreen *screen) const
{
    return m_profiles.value(profileId(screen));
}

QList<Profile *> App::profiles() const
{
    QList<Profile *> list;
    if (Profile *shared = m_profiles.value(Profile::DefaultId)) {
        list.append(shared);
    }
    for (const QScreen *screen : QGuiApplication::screens()) {
        if (Profile *own = Settings::separateScreens() ? m_profiles.value(screen->name()) : nullptr) {
            list.append(own);
        }
    }
    return list;
}

PlaybackController *App::primaryController() const
{
    const Profile *profile = profileFor(QGuiApplication::primaryScreen());
    return profile ? profile->controller() : nullptr;
}

void App::updateProfiles()
{
    QHash<QString, QString> wanted; // id -> title
    if (Settings::separateScreens()) {
        for (const QScreen *screen : QGuiApplication::screens()) {
            const QString model = screen->manufacturer().isEmpty() ? screen->model() : screen->manufacturer() + QLatin1Char(' ') + screen->model();
            wanted.insert(screen->name(), model.trimmed().isEmpty() ? screen->name() : QStringLiteral("%1 (%2)").arg(screen->name(), model.trimmed()));
        }
    } else {
        wanted.insert(Profile::DefaultId, QStringLiteral("All screens"));
    }

    bool changed = false;
    for (auto it = m_profiles.begin(); it != m_profiles.end();) {
        if (wanted.contains(it.key())) {
            ++it;
            continue;
        }
        m_state.flush(); // the profile resumes from it when it comes back
        Profile *profile = it.value();
        it = m_profiles.erase(it);
        profile->deleteLater(); // QML bindings may still hold it until profilesChanged is handled
        changed = true;
    }
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (m_profiles.contains(it.key())) {
            continue;
        }
        auto *profile = new Profile(it.key(), it.value(), m_state.config(), this);
        m_profiles.insert(it.key(), profile);
        connect(profile, &Profile::stateChanged, &m_state, &StateStore::markDirty);
        connect(profile->controller(), &PlaybackController::currentChanged, this, &App::statusChanged);
        connect(profile->controller(), &PlaybackController::pausedChanged, this, &App::statusChanged);
        connect(profile->controller(), &PlaybackController::pausedChanged, this, &App::updateSuspension);
        connect(profile, &Profile::folderErrorChanged, this, [this, profile] {
            if (!profile->folderError().isEmpty() && !profile->settings()->folders().isEmpty()) {
                KNotification::event(KNotification::Error,
                                     QStringLiteral("Wallpaper Slideshow"),
                                     profile->folderError(),
                                     QStringLiteral("preferences-desktop-wallpaper"));
                showSettings();
            }
        });
        changed = true;
    }
    if (changed) {
        Q_EMIT profilesChanged();
        Q_EMIT statusChanged();
    }
}

void App::updateViews()
{
    for (QScreen *screen : QGuiApplication::screens()) {
        auto it = m_views.find(screen);
        Profile *profile = profileFor(screen);
        if (it != m_views.end() && it->profile != profile) {
            destroyView(screen);
            it = m_views.end();
        }
        if (!m_running || !profile || Settings::disabledScreens().contains(screen->name())) {
            if (it != m_views.end()) {
                destroyView(screen);
            }
            continue;
        }
        if (it == m_views.end()) {
            ScreenView view;
            view.profile = profile;
            // Created in this order so the overlay stacks above the slideshow.
            view.slideshow = createLayerWindow("ScreenRenderer", screen, profile);
            view.overlay = createLayerWindow("OverlayUI", screen, profile);
            it = m_views.insert(screen, view);
        }
        for (QQuickWindow *window : {it->slideshow.data(), it->overlay.data()}) {
            if (window) {
                window->setVisible(!m_hidden);
            }
        }
    }
    updateSuspension();
}

void App::destroyView(QScreen *screen)
{
    const ScreenView view = m_views.take(screen);
    delete view.overlay;
    delete view.slideshow;
    updateSuspension();
}

void App::destroyAllViews()
{
    for (QScreen *screen : m_views.keys()) {
        destroyView(screen);
    }
}

QQuickWindow *App::createLayerWindow(const char *type, QScreen *screen, Profile *profile)
{
    const bool overlay = qstrcmp(type, "OverlayUI") == 0;
    QVariantMap properties{{QStringLiteral("profile"), QVariant::fromValue(profile)}};
    if (!overlay) {
        properties.insert(QStringLiteral("primary"), screen == QGuiApplication::primaryScreen());
    }
    QQmlComponent component(m_engine, "Slideshow", type);
    auto *window = qobject_cast<QQuickWindow *>(component.createWithInitialProperties(properties));
    if (!window) {
        qWarning("Cannot create %s: %s", type, qPrintable(component.errorString()));
        return nullptr;
    }
    window->setScreen(screen);
    auto *layer = LayerShellQt::Window::get(window);
    layer->setScope(QStringLiteral("kde-wallpaper-slideshow"));
    layer->setScreen(screen);
    layer->setLayer(LayerShellQt::Window::LayerBottom);
    layer->setAnchors({LayerShellQt::Window::AnchorTop, LayerShellQt::Window::AnchorBottom, LayerShellQt::Window::AnchorLeft, LayerShellQt::Window::AnchorRight});
    layer->setDesiredSize(QSize(0, 0));
    layer->setActivateOnShow(false);
    if (overlay) {
        // Stays clear of panels; takes the keyboard only when the search field is clicked.
        layer->setExclusiveZone(0);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    } else {
        // Extends under panels like a real wallpaper and never takes input.
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        window->setFlag(Qt::WindowTransparentForInput);
    }
    return window;
}

void App::updateSuspension()
{
    const bool power = m_sleeping || (Settings::pauseOnLock() && m_locked) || (Settings::pauseOnFullscreen() && m_fullscreen);
    bool playing = false;
    for (const Profile *profile : std::as_const(m_profiles)) {
        const bool shown = std::any_of(m_views.cbegin(), m_views.cend(), [profile](const ScreenView &v) {
            return v.profile == profile;
        });
        const bool suspended = !m_running || m_hidden || power || !shown;
        profile->controller()->setSuspended(suspended);
        playing = playing || (!suspended && !profile->controller()->paused());
    }
    // Plasma's screensaver inhibition also keeps the screen from dimming and turning
    // off. The power management one only blocks idle sleep, not sleep the user asks for.
    setInhibited(QStringLiteral("org.freedesktop.ScreenSaver"), QStringLiteral("/ScreenSaver"), playing && Settings::preventScreenOff(), m_screenInhibit);
    setInhibited(QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
                 QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
                 playing && Settings::preventSleep(),
                 m_sleepInhibit);
}

void App::leaveForSleepOrLock()
{
    if (Settings::pauseOnSleepOrLock()) {
        for (const Profile *profile : std::as_const(m_profiles)) {
            profile->controller()->setPaused(true);
        }
    }
    if (Settings::hideOnSleepOrLock()) {
        setOverlayHidden(true);
    }
}

void App::setRunning(bool running)
{
    if (m_running != running) {
        m_running = running;
        updateViews();
        m_state.markDirty();
        Q_EMIT runningChanged();
        Q_EMIT statusChanged();
    }
}

void App::setOverlayHidden(bool hidden)
{
    if (m_hidden != hidden) {
        m_hidden = hidden;
        updateViews();
        m_state.markDirty();
        Q_EMIT overlayHiddenChanged();
        Q_EMIT statusChanged();
    }
}

void App::toggleHidden()
{
    setOverlayHidden(!m_hidden);
}

void App::togglePause()
{
    const PlaybackController *primary = primaryController();
    const bool paused = primary ? !primary->paused() : true;
    for (const Profile *profile : std::as_const(m_profiles)) {
        profile->controller()->setPaused(paused);
    }
}

void App::showSettings()
{
    if (m_settingsWindow) {
        m_settingsWindow->show();
        m_settingsWindow->raise();
        m_settingsWindow->requestActivate();
    }
}

void App::toggleSettings()
{
    if (m_settingsWindow && m_settingsWindow->isVisible()) {
        m_settingsWindow->hide();
    } else {
        showSettings();
    }
}

void App::quit()
{
    QCoreApplication::quit();
}

bool App::animatedWebpSupported() const
{
    return QMovie::supportedFormats().contains("webp");
}

QStringList App::screenNames() const
{
    QStringList names;
    for (const QScreen *screen : QGuiApplication::screens()) {
        names.append(screen->name());
    }
    return names;
}

void App::setScreenEnabled(const QString &name, bool enabled)
{
    QStringList disabled = Settings::disabledScreens();
    disabled.removeAll(name);
    if (!enabled) {
        disabled.append(name);
    }
    Settings::setDisabledScreens(disabled);
}

QString App::exportSettings(const QUrl &file) const
{
    const QString prefix = QStringLiteral("Profile ");
    QSet<QString> ids(m_profiles.keyBegin(), m_profiles.keyEnd());
    for (const QString &group : Settings::self()->sharedConfig()->groupList()) {
        if (group.startsWith(prefix)) {
            ids.insert(group.mid(prefix.size()));
        }
    }
    QJsonObject profiles;
    for (const QString &id : std::as_const(ids)) {
        if (const Profile *profile = m_profiles.value(id)) {
            profiles.insert(id, SettingsIO::toJson(profile->settings()));
        } else {
            ProfileSettings stored(id);
            stored.read();
            profiles.insert(id, SettingsIO::toJson(&stored));
        }
    }
    return SettingsIO::write(file, {{QStringLiteral("general"), SettingsIO::toJson(Settings::self())}, {QStringLiteral("profiles"), profiles}});
}

QString App::importSettings(const QUrl &file)
{
    QJsonObject root;
    if (const QString error = SettingsIO::read(file, root); !error.isEmpty()) {
        return error;
    }
    const QJsonValue general = root.value(QLatin1String("general"));
    const QJsonValue profiles = root.value(QLatin1String("profiles"));
    if (!general.isObject() && !profiles.isObject()) {
        return QStringLiteral("%1 holds no settings.").arg(file.toLocalFile());
    }
    int ignored = 0;
    const QJsonObject profileValues = profiles.toObject();
    for (auto it = profileValues.begin(); it != profileValues.end(); ++it) {
        if (!it->isObject()) {
            ++ignored;
        } else if (Profile *profile = m_profiles.value(it.key())) {
            ignored += Profile::applySettings(profile->settings(), it->toObject());
        } else {
            ProfileSettings stored(it.key());
            stored.read();
            ignored += Profile::applySettings(&stored, it->toObject());
        }
    }
    // Last, so that turning on separate screens creates their profiles from the
    // imported settings.
    ignored += SettingsIO::apply(Settings::self(), general.toObject());
    return SettingsIO::ignoredMessage(ignored);
}

void App::revealFile(const QString &path)
{
    const QUrl url = QUrl::fromLocalFile(path);
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                          QStringLiteral("/org/freedesktop/FileManager1"),
                                                          QStringLiteral("org.freedesktop.FileManager1"),
                                                          QStringLiteral("ShowItems"));
    message << QStringList{url.toString()} << QString();
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [url](QDBusPendingCallWatcher *call) {
        if (call->isError()) {
            qWarning("Cannot reveal %s: %s", qPrintable(url.toLocalFile()), qPrintable(call->error().message()));
            QDesktopServices::openUrl(url.adjusted(QUrl::RemoveFilename));
        }
        call->deleteLater();
    });
}

void App::setInputRegion(QWindow *window, const QVariantList &rects)
{
    if (!window) {
        return;
    }
    QRegion region;
    for (const QVariant &rect : rects) {
        region += rect.toRectF().toAlignedRect();
    }
    window->setFlag(Qt::WindowTransparentForInput, region.isEmpty());
    if (!region.isEmpty()) {
        window->setMask(region);
    }
}

QString App::currentTitle() const
{
    const PlaybackController *c = primaryController();
    if (!c || c->currentPath().isEmpty()) {
        return QString();
    }
    const QString name = c->currentPath().section(QLatin1Char('/'), -1);
    return c->currentRow() >= 0 ? QStringLiteral("%1. %2").arg(c->currentRow() + 1).arg(name) : name;
}

void App::setLocked(bool locked)
{
    if (m_locked != locked) {
        m_locked = locked;
        if (locked) {
            leaveForSleepOrLock();
        }
        updateSuspension();
    }
}

// A thread blocked reading a network share can't be frozen, which makes suspend and
// hibernate fail. Before sleep, playback stops, reads of the library folders finish
// and wait, and only then does the app let logind go ahead.
void App::takeSleepLock()
{
    QDBusInterface logind(QStringLiteral("org.freedesktop.login1"),
                          QStringLiteral("/org/freedesktop/login1"),
                          QStringLiteral("org.freedesktop.login1.Manager"),
                          QDBusConnection::systemBus());
    const QDBusReply<QDBusUnixFileDescriptor> lock = logind.call(QStringLiteral("Inhibit"),
                                                                 QStringLiteral("sleep"),
                                                                 QStringLiteral("Wallpaper Slideshow"),
                                                                 QStringLiteral("Finishing reads from the library folders"),
                                                                 QStringLiteral("delay"));
    if (lock.isValid()) {
        m_sleepLock = lock.value();
    } else {
        qWarning("Cannot take a sleep delay lock: %s", qPrintable(lock.error().message()));
    }
}

void App::prepareForSleep(bool start)
{
    if (start) {
        // Before the flush, so a resume from hibernation comes back paused or hidden too.
        leaveForSleepOrLock();
        m_sleeping = true;
        updateSuspension();
        m_state.flush();
        // logind waits at most InhibitDelayMaxSec, 5 s by default.
        if (!IoGate::instance().pause(std::chrono::milliseconds(4000))) {
            qWarning("A read from the library folders is still in flight; sleep may fail");
        }
        m_sleepLock = QDBusUnixFileDescriptor();
    } else {
        IoGate::instance().resume();
        m_sleeping = false;
        updateSuspension();
        takeSleepLock();
    }
}

void App::setFullscreenActive(bool active)
{
    if (m_fullscreen != active) {
        m_fullscreen = active;
        updateSuspension();
    }
}

// KWin exposes no window list to Wayland clients, so a small KWin script reports
// whether the active window is fullscreen back over D-Bus.
void App::updateFullscreenWatch()
{
    QDBusInterface scripting(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"));
    scripting.call(QStringLiteral("unloadScript"), KWinScriptName); // also clears one left by a crash
    m_fullscreenWatch = false;
    if (!Settings::pauseOnFullscreen()) {
        setFullscreenActive(false);
        return;
    }

    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/kwin-fullscreen.js");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QStringLiteral(R"(function report() {
    const w = workspace.activeWindow;
    callDBus("%1", "/Slideshow", "io.github.kdewallpaperslideshow.Slideshow", "setFullscreenActive",
             !!(w && w.fullScreen && w.normalWindow));
}
function track(w) { w.fullScreenChanged.connect(report); }
workspace.windowList().forEach(track);
workspace.windowAdded.connect(track);
workspace.windowActivated.connect(report);
report();
)")
                   .arg(QDBusConnection::sessionBus().baseService())
                   .toUtf8());
    file.close();

    const QDBusReply<int> id = scripting.call(QStringLiteral("loadScript"), path, KWinScriptName);
    if (!id.isValid() || id.value() < 0) {
        qWarning("Cannot load the KWin fullscreen script: %s", qPrintable(id.error().message()));
        return;
    }
    QDBusInterface(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting/Script%1").arg(id.value()), QStringLiteral("org.kde.kwin.Script"))
        .call(QStringLiteral("run"));
    m_fullscreenWatch = true;
}

void App::writeAutostart()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
    const QString path = dir + QLatin1Char('/') + QGuiApplication::desktopFileName() + QStringLiteral(".desktop");
    if (!Settings::autostart()) {
        QFile::remove(path);
        return;
    }
    QDir().mkpath(dir);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QStringLiteral("[Desktop Entry]\n"
                                  "Type=Application\n"
                                  "Name=Wallpaper Slideshow\n"
                                  "Exec=%1\n"
                                  "Icon=preferences-desktop-wallpaper\n"
                                  "X-KDE-autostart-phase=2\n")
                       .arg(QCoreApplication::applicationFilePath())
                       .toUtf8());
    }
}

void App::writeState(KSharedConfig &config)
{
    KConfigGroup general = config.group(QStringLiteral("General"));
    general.writeEntry("windowVisible", m_settingsWindow && m_settingsWindow->isVisible());
    general.writeEntry("running", m_running);
    general.writeEntry("overlayHidden", m_hidden);
    for (const Profile *profile : std::as_const(m_profiles)) {
        profile->writeState(config);
    }
}
