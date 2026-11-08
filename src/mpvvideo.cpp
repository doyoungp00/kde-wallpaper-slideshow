#include "mpvvideo.h"

#include <MpvController>

MpvVideo::MpvVideo(QQuickItem *parent)
    : MpvAbstractItem(parent)
{
    connect(this, &MpvAbstractItem::ready, this, [this] {
        Q_EMIT observeProperty(QStringLiteral("duration"), MPV_FORMAT_DOUBLE);
        setPropertyAsync(QStringLiteral("hwdec"), QStringLiteral("auto-safe"));
        setPropertyAsync(QStringLiteral("loop-file"), QStringLiteral("inf"));
        setPropertyAsync(QStringLiteral("input-default-bindings"), false);
        setPropertyAsync(QStringLiteral("pause"), m_paused);
        setPropertyAsync(QStringLiteral("mute"), m_muted);
        setPropertyAsync(QStringLiteral("volume"), m_volume);
        applyScaleMode();
        m_ready = true;
        load();
    });
    // Events until the last loadfile's start-file belong to a file it replaced.
    connect(mpvController(), &MpvController::fileStarted, this, [this] {
        if (m_pendingStarts > 0) {
            --m_pendingStarts;
        }
    });
    connect(mpvController(), &MpvController::fileLoaded, this, [this] {
        if (m_pendingStarts == 0) {
            setLoaded(true);
        }
    });
    connect(mpvController(), &MpvController::endFile, this, [this](const QString &reason) {
        if (m_pendingStarts == 0 && reason == QLatin1String("error")) {
            setFailed(true);
        }
    });
    connect(mpvController(), &MpvController::propertyChanged, this, [this](const QString &property, const QVariant &value) {
        if (m_pendingStarts == 0 && property == QLatin1String("duration") && value.toDouble() > 0) {
            Q_EMIT durationReported(qint64(value.toDouble() * 1000));
        }
    });
}

void MpvVideo::setSource(const QUrl &source)
{
    if (m_source != source) {
        m_source = source;
        Q_EMIT sourceChanged();
        load();
    }
}

void MpvVideo::load()
{
    setLoaded(false);
    setFailed(false);
    if (!m_ready) {
        return;
    }
    if (m_source.isLocalFile()) {
        ++m_pendingStarts;
        commandAsync({QStringLiteral("loadfile"), m_source.toLocalFile()});
    } else {
        // An idle player gives back its decoder surfaces.
        commandAsync({QStringLiteral("stop")});
    }
}

void MpvVideo::setPaused(bool paused)
{
    if (m_paused != paused) {
        m_paused = paused;
        if (m_ready) {
            setPropertyAsync(QStringLiteral("pause"), paused);
        }
        Q_EMIT pausedChanged();
    }
}

void MpvVideo::setMuted(bool muted)
{
    if (m_muted != muted) {
        m_muted = muted;
        if (m_ready) {
            setPropertyAsync(QStringLiteral("mute"), muted);
        }
        Q_EMIT mutedChanged();
    }
}

void MpvVideo::setVolume(int volume)
{
    if (m_volume != volume) {
        m_volume = volume;
        if (m_ready) {
            setPropertyAsync(QStringLiteral("volume"), volume);
        }
        Q_EMIT volumeChanged();
    }
}

void MpvVideo::setScaleMode(int mode)
{
    if (m_scaleMode != mode) {
        m_scaleMode = mode;
        if (m_ready) {
            applyScaleMode();
        }
        Q_EMIT scaleModeChanged();
    }
}

void MpvVideo::setBackground(const QColor &color)
{
    if (m_background != color) {
        m_background = color;
        if (m_ready) {
            applyScaleMode();
        }
        Q_EMIT backgroundChanged();
    }
}

void MpvVideo::applyScaleMode()
{
    setPropertyAsync(QStringLiteral("keepaspect"), m_scaleMode != 2);
    setPropertyAsync(QStringLiteral("panscan"), m_scaleMode == 0 ? 1.0 : 0.0);
    setPropertyAsync(QStringLiteral("background-color"), m_background.name());
}

void MpvVideo::setLoaded(bool loaded)
{
    if (m_loaded != loaded) {
        m_loaded = loaded;
        Q_EMIT loadedChanged();
    }
}

void MpvVideo::setFailed(bool failed)
{
    if (m_failed != failed) {
        m_failed = failed;
        Q_EMIT failedChanged();
    }
}
