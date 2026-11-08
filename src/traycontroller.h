#pragma once

#include <QObject>

class App;
class KStatusNotifierItem;
class QAction;

// Tray icon: left-click toggles the settings window, and the menu holds exactly
// Play/Pause, Hide/Show and Exit.
class TrayController : public QObject
{
    Q_OBJECT
public:
    explicit TrayController(App *app);

private:
    void update();

    App *m_app;
    KStatusNotifierItem *m_item;
    QAction *m_playPause;
    QAction *m_hide;
};
