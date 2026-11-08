#include "playbackcontroller.h"
#include "core/library.h"
#include "core/smartduration.h"
#include "playlist.h"
#include "profilesettings.h"

#include <QFileInfo>

PlaybackController::PlaybackController(Library *library, ProfileSettings *settings, QObject *parent)
    : QObject(parent)
    , m_library(library)
    , m_settings(settings)
    , m_playlist(new Playlist(library, this))
{
    m_advanceTimer.setSingleShot(true);
    connect(&m_advanceTimer, &QTimer::timeout, this, &PlaybackController::next);
    m_progressTimer.setInterval(50);
    connect(&m_progressTimer, &QTimer::timeout, this, &PlaybackController::progressChanged);

    connect(m_library, &Library::orderReset, this, [this] {
        m_playlist->rebuild();
        relocate();
    });
    connect(m_library, &Library::aboutToMerge, this, [this](const QSet<QString> &removed) {
        m_fallbackPath.clear();
        if (m_row < 0 || !removed.contains(m_path)) {
            return;
        }
        for (int i = 1; i < m_playlist->count(); ++i) {
            const int row = (m_row + i) % m_playlist->count();
            const QString path = m_playlist->pathAt(row);
            if (!removed.contains(path) && !m_library->at(row).bad && m_playlist->matches(row)) {
                m_fallbackPath = path;
                break;
            }
        }
    });
    connect(m_library, &Library::merged, this, [this] {
        m_playlist->rebuild();
        relocate();
    });
    connect(m_library, &Library::entryChanged, this, [this](int row) {
        m_playlist->entryChanged(row);
        if (row == m_row) {
            if (m_library->at(row).bad) {
                next();
            } else {
                updateMedia();
            }
        } else {
            updateNeighbors(); // a probe result or bad mark may change what comes next
        }
    });

    for (auto signal : {&ProfileSettings::durationSecChanged,
                        &ProfileSettings::maxDurationSecChanged,
                        &ProfileSettings::smartDurationChanged,
                        &ProfileSettings::roundUpToLoopsChanged}) {
        connect(m_settings, signal, this, &PlaybackController::updateMedia);
    }
    connect(m_settings, &ProfileSettings::freezeAnimationsOnPauseChanged, this, &PlaybackController::updateClock);
}

void PlaybackController::setPaused(bool paused)
{
    if (m_paused != paused) {
        m_paused = paused;
        updateClock();
        Q_EMIT pausedChanged();
        Q_EMIT stateChanged();
    }
}

void PlaybackController::setFilter(const QString &filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        applyFilter();
    }
}

void PlaybackController::setFilterRegex(bool regex)
{
    if (m_filterRegex != regex) {
        m_filterRegex = regex;
        applyFilter();
    }
}

void PlaybackController::applyFilter()
{
    m_filterValid = m_playlist->setFilter(m_filter, m_filterRegex);
    // The current item plays out its time; what follows comes from the matches.
    updateNeighbors();
    Q_EMIT filterChanged();
    Q_EMIT stateChanged();
}

void PlaybackController::togglePause()
{
    setPaused(!m_paused);
}

void PlaybackController::setSuspended(bool suspended)
{
    if (m_suspended != suspended) {
        m_suspended = suspended;
        updateClock();
    }
}

bool PlaybackController::ticking() const
{
    return !m_paused && !m_suspended && !m_path.isEmpty();
}

bool PlaybackController::animationsRunning() const
{
    return m_animationsRunning;
}

qint64 PlaybackController::elapsedMs() const
{
    return m_accumulatedMs + (m_wasTicking ? m_clock.elapsed() : 0);
}

double PlaybackController::progress() const
{
    return m_durationMs > 0 ? std::clamp(double(elapsedMs()) / double(m_durationMs), 0.0, 1.0) : 0.0;
}

void PlaybackController::updateClock()
{
    const bool now = ticking();
    if (now != m_wasTicking) {
        if (m_wasTicking) {
            m_accumulatedMs += m_clock.elapsed();
            m_advanceTimer.stop();
            m_progressTimer.stop();
        }
        m_wasTicking = now;
        if (now) {
            m_clock.restart();
            m_progressTimer.start();
        }
        schedule();
        Q_EMIT progressChanged();
    }
    const bool animations = !m_suspended && !(m_paused && m_settings->freezeAnimationsOnPause());
    if (animations != m_animationsRunning) {
        m_animationsRunning = animations;
        Q_EMIT animationsRunningChanged();
    }
}

void PlaybackController::schedule()
{
    if (m_wasTicking) {
        m_advanceTimer.start(int(std::clamp<qint64>(m_durationMs - elapsedMs(), 0, std::numeric_limits<int>::max())));
    }
}

void PlaybackController::setCurrent(const QString &path, qint64 elapsedMs)
{
    m_path = path;
    m_row = path.isEmpty() ? -1 : m_playlist->rowOf(path);
    m_playlist->setCurrentRow(m_row);
    m_accumulatedMs = elapsedMs;
    m_fallbackPath.clear();
    if (m_wasTicking) {
        m_clock.restart();
    }
    updateMedia();
    updateNeighbors();
    updateClock();
    schedule();
    Q_EMIT currentChanged();
    Q_EMIT progressChanged();
    Q_EMIT stateChanged();
}

void PlaybackController::relocate()
{
    const int row = m_playlist->rowOf(m_path);
    if (row >= 0) {
        m_row = row;
        m_playlist->setCurrentRow(row);
        updateMedia();
        updateNeighbors();
        Q_EMIT currentChanged();
    } else if (!m_fallbackPath.isEmpty() && m_playlist->rowOf(m_fallbackPath) >= 0) {
        setCurrent(m_fallbackPath);
    } else {
        const int first = findRow(-1, 1);
        setCurrent(first >= 0 ? m_playlist->pathAt(first) : QString());
    }
}

QString PlaybackController::kindOf(const QString &path) const
{
    switch (Library::kindOf(path)) {
    case Library::Kind::Video:
        return QStringLiteral("video");
    case Library::Kind::Image:
        return QStringLiteral("image");
    case Library::Kind::Animated:
        break;
    }
    const int entry = m_library->indexOf(path);
    if (entry < 0) {
        return QStringLiteral("animated"); // not indexed yet; AnimatedImage shows stills too
    }
    const qint64 loop = m_library->at(entry).loopMs;
    if (loop == Library::LoopUnknown) {
        // Shown right away; the probe only refines the duration, since on a slow
        // share it reads the whole file.
        m_library->ensureProbed(entry);
        return QStringLiteral("animated");
    }
    return loop == Library::LoopStill ? QStringLiteral("image") : QStringLiteral("animated");
}

void PlaybackController::updateMedia()
{
    const QString kind = m_path.isEmpty() ? QString() : kindOf(m_path);
    const QUrl url = m_path.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_path);
    if (kind != m_kind || url != m_url) {
        m_kind = kind;
        m_url = url;
        Q_EMIT currentMediaChanged();
    }

    const int entry = m_library->indexOf(m_path);
    const qint64 loop = entry >= 0 ? m_library->at(entry).loopMs : Library::LoopUnknown;
    const qint64 duration = effectiveDurationMs(qint64(m_settings->durationSec()) * 1000,
                                                qint64(m_settings->maxDurationSec()) * 1000,
                                                loop,
                                                m_settings->smartDuration(),
                                                m_settings->roundUpToLoops());
    if (duration != m_durationMs) {
        m_durationMs = duration;
        schedule();
        Q_EMIT progressChanged();
    }
}

PlaybackController::Neighbor PlaybackController::neighbor(int step) const
{
    const int row = m_row >= 0 ? findRow(m_row, step) : -1;
    if (row < 0 || row == m_row) {
        return {};
    }
    const QString path = m_playlist->pathAt(row);
    return {path, QUrl::fromLocalFile(path), kindOf(path)};
}

void PlaybackController::updateNeighbors()
{
    const Neighbor next = neighbor(1);
    const Neighbor previous = neighbor(-1);
    if (next != m_next || previous != m_previous) {
        m_next = next;
        m_previous = previous;
        Q_EMIT neighborsChanged();
    }
}

int PlaybackController::findRow(int from, int step) const
{
    const int count = m_playlist->count();
    for (int i = 1; i <= count; ++i) {
        const int row = ((from + step * i) % count + count) % count;
        if (!m_library->at(row).bad && m_playlist->matches(row)) {
            return row;
        }
    }
    return -1;
}

void PlaybackController::next()
{
    const int row = findRow(m_row, 1);
    setCurrent(row >= 0 ? m_playlist->pathAt(row) : QString());
}

void PlaybackController::previous()
{
    const int row = findRow(m_row < 0 ? 0 : m_row, -1);
    setCurrent(row >= 0 ? m_playlist->pathAt(row) : QString());
}

void PlaybackController::first()
{
    const int row = findRow(-1, 1);
    setCurrent(row >= 0 ? m_playlist->pathAt(row) : QString());
}

void PlaybackController::last()
{
    const int row = findRow(0, -1);
    setCurrent(row >= 0 ? m_playlist->pathAt(row) : QString());
}

void PlaybackController::jumpTo(int row)
{
    if (row >= 0 && row < m_playlist->count()) {
        setCurrent(m_playlist->pathAt(row));
    }
}

void PlaybackController::restore(const QString &path, qint64 elapsedMs, int fallbackRow)
{
    const int count = m_playlist->count();
    const int row = m_playlist->rowOf(path);
    if (row < 0) {
        if (count > 0) {
            setCurrent(m_playlist->pathAt(std::clamp(fallbackRow, 0, count - 1)));
        } else if (!path.isEmpty() && QFileInfo::exists(path)) {
            setCurrent(path, elapsedMs);
        }
        return;
    }
    if (!QFileInfo(QFileInfo(path).path()).isDir()) {
        // The drive isn't mounted yet; keep the place until it is.
        setCurrent(path, elapsedMs);
        return;
    }
    for (int i = 0; i < count; ++i) {
        const QString candidate = m_playlist->pathAt((row + i) % count);
        if (QFileInfo::exists(candidate)) {
            setCurrent(candidate, i == 0 ? elapsedMs : 0);
            return;
        }
    }
    setCurrent(QString());
}

void PlaybackController::retryMedia()
{
    Q_EMIT currentMediaChanged();
    Q_EMIT neighborsChanged();
}

void PlaybackController::showProvisional(const QString &path)
{
    if (m_path.isEmpty()) {
        setCurrent(path);
    }
}

void PlaybackController::reportError(const QString &path)
{
    if (!QFileInfo(QFileInfo(path).path()).isDir()) {
        return; // an unmounted drive; keep the bad marks for files that are really broken
    }
    const int entry = m_library->indexOf(path);
    qWarning("Cannot display %s", qPrintable(path));
    if (entry >= 0) {
        m_library->setBad(entry, true); // entryChanged moves on
    } else if (path == m_path) {
        setCurrent(QString());
    }
}

void PlaybackController::reportLoopLength(const QString &path, qint64 loopMs)
{
    const int entry = m_library->indexOf(path);
    if (entry >= 0 && loopMs > 0) {
        m_library->setLoopMs(entry, loopMs);
    }
}
