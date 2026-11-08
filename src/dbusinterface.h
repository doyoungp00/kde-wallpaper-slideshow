#pragma once

#include <QObject>

class App;

// Session bus remote control at /Slideshow.
class DBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.kdewallpaperslideshow.Slideshow")
public:
    explicit DBusInterface(App *app);

public Q_SLOTS:
    Q_SCRIPTABLE void next();
    Q_SCRIPTABLE void previous();
    Q_SCRIPTABLE void first();
    Q_SCRIPTABLE void last();
    Q_SCRIPTABLE void togglePause();
    Q_SCRIPTABLE void toggleHidden();
    // n is the 1-based playlist number.
    Q_SCRIPTABLE void jumpTo(int n);
    Q_SCRIPTABLE void showSettings();
    Q_SCRIPTABLE void quit();
    // Called by the KWin script that watches for fullscreen windows.
    Q_SCRIPTABLE void setFullscreenActive(bool active);

private:
    App *m_app;
};
