#include "traycontroller.h"
#include "app.h"
#include "playbackcontroller.h"
#include "settings.h"

#include <algorithm>

#include <KStatusNotifierItem>
#include <QGuiApplication>
#include <QMenu>
#include <QStyleHints>

// Both settings list the same choices, so one enum serves them.
static_assert(int(Settings::EnumTrayClick::COUNT) == int(Settings::EnumTrayDoubleClick::COUNT));

TrayController::TrayController(App *app)
    : QObject(app)
    , m_app(app)
    , m_item(new KStatusNotifierItem(QStringLiteral("kde-wallpaper-slideshow"), this))
{
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_item->setStatus(KStatusNotifierItem::Active);
    m_item->setIconByName(QStringLiteral("preferences-desktop-wallpaper"));
    m_item->setTitle(QStringLiteral("Wallpaper Slideshow"));
    m_item->setStandardActionsEnabled(false);

    auto *menu = new QMenu;
    m_playPause = menu->addAction(QString(), app, &App::togglePause);
    m_hide = menu->addAction(QString(), app, &App::toggleHidden);
    menu->addSeparator();
    menu->addAction(QIcon::fromTheme(QStringLiteral("configure")), QStringLiteral("Settings…"), app, &App::showSettings);
    menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Exit"), app, &App::quit);
    m_item->setContextMenu(menu);

    m_clickDelay.setSingleShot(true);
    connect(&m_clickDelay, &QTimer::timeout, this, [this] {
        run(Settings::trayClick());
    });
    connect(m_item, &KStatusNotifierItem::activateRequested, this, &TrayController::onActivated);
    connect(app, &App::statusChanged, this, &TrayController::update);
    update();
}

void TrayController::update()
{
    const PlaybackController *controller = m_app->primaryController();
    const bool paused = controller && controller->paused();
    m_playPause->setText(paused ? QStringLiteral("Play") : QStringLiteral("Pause"));
    m_playPause->setIcon(QIcon::fromTheme(paused ? QStringLiteral("media-playback-start") : QStringLiteral("media-playback-pause")));
    m_hide->setText(m_app->overlayHidden() ? QStringLiteral("Show") : QStringLiteral("Hide"));
    m_hide->setIcon(QIcon::fromTheme(m_app->overlayHidden() ? QStringLiteral("view-visible") : QStringLiteral("view-hidden")));

    QString status = !m_app->running() ? QStringLiteral("Stopped")
        : m_app->overlayHidden()       ? QStringLiteral("Hidden")
        : paused                       ? QStringLiteral("Paused")
                                       : QStringLiteral("Playing");
    m_item->setToolTip(QStringLiteral("preferences-desktop-wallpaper"), QStringLiteral("Wallpaper Slideshow"), m_app->currentTitle() + QStringLiteral("\n") + status);
}

void TrayController::onActivated()
{
    // The tray reports each click of a double-click, so a click waits to see if a
    // second follows, but only when a double-click does something.
    if (Settings::trayDoubleClick() == Settings::EnumTrayClick::Nothing) {
        run(Settings::trayClick());
    } else if (m_clickDelay.isActive()) {
        m_clickDelay.stop();
        run(Settings::trayDoubleClick());
    } else {
        m_clickDelay.start(QGuiApplication::styleHints()->mouseDoubleClickInterval());
    }
}

void TrayController::run(int action)
{
    using A = Settings::EnumTrayClick;
    PlaybackController *primary = m_app->primaryController();
    switch (action) {
    case A::Nothing:
        break;
    case A::OpenSettings:
        m_app->toggleSettings();
        break;
    case A::Play:
        m_app->setPaused(false);
        break;
    case A::Pause:
        m_app->setPaused(true);
        break;
    case A::TogglePause:
        m_app->togglePause();
        break;
    // Moving through the playlist acts on the primary screen, like the D-Bus calls.
    case A::Previous:
        if (primary) {
            primary->previous();
        }
        break;
    case A::Next:
        if (primary) {
            primary->next();
        }
        break;
    case A::First:
        if (primary) {
            primary->first();
        }
        break;
    case A::Last:
        if (primary) {
            primary->last();
        }
        break;
    case A::Shuffle:
        for (Profile *profile : m_app->profiles()) {
            profile->reshuffle();
        }
        break;
    case A::Rescan:
        for (Profile *profile : m_app->profiles()) {
            profile->rescan();
        }
        break;
    case A::ToggleShown:
        m_app->toggleHidden();
        break;
    case A::Show:
        m_app->setOverlayHidden(false);
        break;
    case A::Hide:
        m_app->setOverlayHidden(true);
        break;
    case A::ToggleMute: {
        // Audio comes from the primary screen, so its profile decides the new state.
        const QList<Profile *> profiles = m_app->profiles();
        const auto it = std::find_if(profiles.cbegin(), profiles.cend(), [primary](const Profile *p) {
            return p->controller() == primary;
        });
        const bool mute = it == profiles.cend() || !(*it)->settings()->mute();
        for (Profile *profile : profiles) {
            profile->settings()->setMute(mute);
        }
        break;
    }
    case A::TogglePreventSleep: {
        // On means both: no idle sleep, and no screen off or lock.
        const bool prevent = !(Settings::preventSleep() && Settings::preventScreenOff());
        Settings::setPreventSleep(prevent);
        Settings::setPreventScreenOff(prevent);
        break;
    }
    }
}
