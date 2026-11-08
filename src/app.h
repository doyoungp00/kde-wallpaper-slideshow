#pragma once

#include "playbackcontroller.h"
#include "profile.h"
#include "settings.h"
#include "statestore.h"

#include <QDBusUnixFileDescriptor>
#include <QHash>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
#include <QtQmlIntegration>

class QQmlEngine;
class QScreen;

// Owns the profiles and the per-screen layer-shell windows, and applies the
// app-wide settings and power events to them.
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(Settings *settings READ settings CONSTANT)
    Q_PROPERTY(QList<Profile *> profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(PlaybackController *controller READ primaryController NOTIFY profilesChanged)
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(bool overlayHidden READ overlayHidden WRITE setOverlayHidden NOTIFY overlayHiddenChanged)
    Q_PROPERTY(bool animatedWebpSupported READ animatedWebpSupported CONSTANT)
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screensChanged)

public:
    explicit App(QQmlEngine *engine, QObject *parent = nullptr);
    ~App() override;

    static App *create(QQmlEngine *, QJSEngine *);

    // Restores the saved state and shows the windows it calls for.
    void start();

    Settings *settings() const
    {
        return Settings::self();
    }
    QList<Profile *> profiles() const;
    PlaybackController *primaryController() const;
    bool running() const
    {
        return m_running;
    }
    void setRunning(bool running);
    bool overlayHidden() const
    {
        return m_hidden;
    }
    void setOverlayHidden(bool hidden);
    bool animatedWebpSupported() const;
    QStringList screenNames() const;
    // "<number>. <filename>" of the primary screen's item.
    QString currentTitle() const;

    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void toggleHidden();
    Q_INVOKABLE void showSettings();
    Q_INVOKABLE void toggleSettings();
    Q_INVOKABLE void quit();
    Q_INVOKABLE void setScreenEnabled(const QString &name, bool enabled);
    // Opens the file manager with path selected, or its folder when that fails.
    Q_INVOKABLE void revealFile(const QString &path);
    // Writes all settings to a settings file: the app-wide ones and every profile,
    // including those of screens that aren't connected. Returns an error, empty on success.
    Q_INVOKABLE QString exportSettings(const QUrl &file) const;
    // Applies a settings file. Profiles of screens that aren't connected are stored
    // for when they are. Returns an error or warning, empty on success.
    Q_INVOKABLE QString importSettings(const QUrl &file);
    // Limits pointer input on an overlay window to rects (a list of QRectF);
    // everything else falls through to the desktop.
    Q_INVOKABLE void setInputRegion(QWindow *window, const QVariantList &rects);

    void setFullscreenActive(bool active);

Q_SIGNALS:
    void profilesChanged();
    void runningChanged();
    void overlayHiddenChanged();
    void screensChanged();
    // Anything the tray shows changed.
    void statusChanged();

private Q_SLOTS:
    void setLocked(bool locked);
    void prepareForSleep(bool start);

private:
    struct ScreenView {
        QPointer<QQuickWindow> slideshow;
        QPointer<QQuickWindow> overlay;
        Profile *profile = nullptr;
    };

    QString profileId(const QScreen *screen) const;
    Profile *profileFor(const QScreen *screen) const;
    // Creates and deletes profiles to match the screens and the separateScreens setting.
    void updateProfiles();
    void updateViews();
    void destroyView(QScreen *screen);
    void destroyAllViews();
    QQuickWindow *createLayerWindow(const char *type, QScreen *screen, Profile *profile);
    // Suspends the profiles that shouldn't play, and holds the sleep and screen
    // inhibitions while any of them plays.
    void updateSuspension();
    // Applies the pause and hide options for sleep and screen lock.
    void leaveForSleepOrLock();
    void updateFullscreenWatch();
    void writeAutostart();
    void writeState(KSharedConfig &config);
    void takeSleepLock();

    QQmlEngine *m_engine;
    StateStore m_state;
    QTimer m_elapsedSave;

    QHash<QString, Profile *> m_profiles;
    QHash<QScreen *, ScreenView> m_views;
    QPointer<QQuickWindow> m_settingsWindow;

    bool m_running = true;
    bool m_hidden = false;
    bool m_locked = false;
    bool m_fullscreen = false;
    bool m_fullscreenWatch = false;
    bool m_sleeping = false;
    // logind delay lock: sleep waits until it is released.
    QDBusUnixFileDescriptor m_sleepLock;
    // Cookies of the idle sleep and screen-off/lock inhibitions; 0 when not held.
    uint m_sleepInhibit = 0;
    uint m_screenInhibit = 0;
};
