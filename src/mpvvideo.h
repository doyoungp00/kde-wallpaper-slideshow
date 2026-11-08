#pragma once

#include <MpvAbstractItem>
#include <QColor>
#include <QUrl>
#include <QtQmlIntegration>

// A looping libmpv video for the slideshow scene. Each one is a whole libmpv
// instance with its own GPU surfaces, so reuse it by changing the source instead
// of creating one per file; an empty source stops playback.
class MpvVideo : public MpvAbstractItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    // Settings::EnumScaleMode: 0 cover, 1 fit, 2 stretch
    Q_PROPERTY(int scaleMode READ scaleMode WRITE setScaleMode NOTIFY scaleModeChanged)
    Q_PROPERTY(QColor background READ background WRITE setBackground NOTIFY backgroundChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
    Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)

public:
    explicit MpvVideo(QQuickItem *parent = nullptr);

    QUrl source() const
    {
        return m_source;
    }
    void setSource(const QUrl &source);
    bool paused() const
    {
        return m_paused;
    }
    void setPaused(bool paused);
    bool muted() const
    {
        return m_muted;
    }
    void setMuted(bool muted);
    int volume() const
    {
        return m_volume;
    }
    void setVolume(int volume);
    int scaleMode() const
    {
        return m_scaleMode;
    }
    void setScaleMode(int mode);
    QColor background() const
    {
        return m_background;
    }
    void setBackground(const QColor &color);
    bool loaded() const
    {
        return m_loaded;
    }
    bool failed() const
    {
        return m_failed;
    }

Q_SIGNALS:
    void sourceChanged();
    void pausedChanged();
    void mutedChanged();
    void volumeChanged();
    void scaleModeChanged();
    void backgroundChanged();
    void loadedChanged();
    void failedChanged();
    void durationReported(qint64 durationMs);

private:
    void applyScaleMode();
    void load();
    void setLoaded(bool loaded);
    void setFailed(bool failed);

    QUrl m_source;
    bool m_ready = false;
    bool m_paused = true;
    bool m_muted = true;
    int m_volume = 50;
    int m_scaleMode = 0;
    QColor m_background = Qt::black;
    bool m_loaded = false;
    bool m_failed = false;
    int m_pendingStarts = 0;
};
