#pragma once

#include <QObject>
#include <QTimer>

class App;
class KStatusNotifierItem;
class QAction;

// Tray icon: clicking and double-clicking run the actions chosen in the settings,
// and the menu holds Play/Pause, Hide/Show, Settings and Exit.
class TrayController : public QObject
{
    Q_OBJECT
public:
    explicit TrayController(App *app);

private:
    void update();
    void onActivated();
    // Runs a Settings::EnumTrayClick choice.
    void run(int action);

    App *m_app;
    KStatusNotifierItem *m_item;
    QAction *m_playPause;
    QAction *m_hide;
    // Holds a click back while a second one could still make it a double-click.
    QTimer m_clickDelay;
};
