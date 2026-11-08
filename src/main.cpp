#include "app.h"
#include "dbusinterface.h"
#include "imageprovider.h"
#include "playbackcontroller.h"
#include "traycontroller.h"

#include <KAboutData>
#include <KConfigGroup>
#include <KDBusService>
#include <KGlobalAccel>
#include <KSharedConfig>
#include <KSignalHandler>
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>

#include <clocale>
#include <csignal>

namespace
{
// Moves settings from the first release's per-topic groups into the default
// profile and the General group, without overwriting anything already set there.
void migrateConfig()
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kde-wallpaper-slideshowrc"));
    KConfigGroup profile = config->group(QStringLiteral("Profile default"));
    KConfigGroup general = config->group(QStringLiteral("General"));
    const QStringList appWide{QStringLiteral("watchChanges"),
                              QStringLiteral("autostart"),
                              QStringLiteral("disabledScreens"),
                              QStringLiteral("pauseOnLock"),
                              QStringLiteral("pauseOnFullscreen")};
    bool migrated = false;
    for (const char *name : {"Library", "Playback", "Display", "SmartDuration", "Video", "Playlist", "EdgeControls", "Power"}) {
        KConfigGroup old = config->group(QLatin1String(name));
        if (!old.exists()) {
            continue;
        }
        const QMap<QString, QString> entries = old.entryMap();
        for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
            if (it.key() == QLatin1String("folder")) {
                if (!profile.hasKey("folders")) {
                    profile.writePathEntry("folders", QStringList{old.readPathEntry("folder", QString())});
                }
            } else if (it.key() == QLatin1String("sameOnAllScreens")) {
                if (!general.hasKey("separateScreens")) {
                    general.writeEntry("separateScreens", !old.readEntry("sameOnAllScreens", true));
                }
            } else {
                KConfigGroup &target = appWide.contains(it.key()) ? general : profile;
                if (!target.hasKey(it.key())) {
                    target.writeEntry(it.key(), it.value());
                }
            }
        }
        old.deleteGroup();
        migrated = true;
    }
    if (migrated) {
        config->sync();
    }
}
}

int main(int argc, char **argv)
{
    // MpvQt renders through OpenGL.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QApplication application(argc, argv);
    // libmpv requires C number formatting.
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setQuitOnLastWindowClosed(false);
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-wallpaper")));

    KAboutData about(QStringLiteral("kde-wallpaper-slideshow"),
                     QStringLiteral("Wallpaper Slideshow"),
                     QStringLiteral(PROJECT_VERSION),
                     QStringLiteral("A slideshow drawn over the KDE Plasma wallpaper"),
                     KAboutLicense::Unknown);
    about.setOrganizationDomain("github.io");
    about.setDesktopFileName(QStringLiteral("io.github.kde-wallpaper-slideshow"));
    KAboutData::setApplicationData(about);

    // A second launch hands over to the running instance and exits here.
    KDBusService service(KDBusService::Unique);
    migrateConfig();

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("media"), new MediaImageProvider);
    auto *app = new App(&engine);
    new DBusInterface(app);
    // Before anything asks for a controller, so they all resume from the state file.
    app->start();
    new TrayController(app);
    QObject::connect(&service, &KDBusService::activateRequested, app, &App::showSettings);

    KSignalHandler::self()->watchSignal(SIGTERM);
    KSignalHandler::self()->watchSignal(SIGINT);
    QObject::connect(KSignalHandler::self(), &KSignalHandler::signalReceived, app, &App::quit);

    // Registered without default keys; assign them in System Settings > Shortcuts.
    auto addShortcut = [app](const char *name, const QString &text, auto slot) {
        auto *action = new QAction(text, app);
        action->setObjectName(QLatin1String(name));
        QObject::connect(action, &QAction::triggered, app, slot);
        KGlobalAccel::setGlobalShortcut(action, QList<QKeySequence>());
    };
    addShortcut("next", QStringLiteral("Next Image"), [app] {
        if (PlaybackController *c = app->primaryController()) {
            c->next();
        }
    });
    addShortcut("previous", QStringLiteral("Previous Image"), [app] {
        if (PlaybackController *c = app->primaryController()) {
            c->previous();
        }
    });
    addShortcut("toggle-pause", QStringLiteral("Play/Pause"), &App::togglePause);
    addShortcut("toggle-hidden", QStringLiteral("Hide/Show Slideshow"), &App::toggleHidden);

    const int result = QApplication::exec();
    delete app;
    return result;
}
