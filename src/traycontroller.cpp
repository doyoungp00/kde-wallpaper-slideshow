#include "traycontroller.h"
#include "app.h"
#include "playbackcontroller.h"

#include <KStatusNotifierItem>
#include <QMenu>

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
    menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), QStringLiteral("Exit"), app, &App::quit);
    m_item->setContextMenu(menu);

    connect(m_item, &KStatusNotifierItem::activateRequested, app, &App::toggleSettings);
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
