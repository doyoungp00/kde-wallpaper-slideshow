#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QtQmlIntegration>

#include "playlist.h"

class Library;
class ProfileSettings;

// Which item is showing, for how long, and what comes next. The current item is
// identified by its path, so it survives reshuffles and rescans.
class PlaybackController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by App")
    Q_PROPERTY(Playlist *playlist READ playlist CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentChanged)
    Q_PROPERTY(int currentRow READ currentRow NOTIFY currentChanged)
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY currentMediaChanged)
    Q_PROPERTY(QString currentKind READ currentKind NOTIFY currentMediaChanged)
    // The items either side of the current one, for preloading.
    Q_PROPERTY(QString nextPath READ nextPath NOTIFY neighborsChanged)
    Q_PROPERTY(QUrl nextUrl READ nextUrl NOTIFY neighborsChanged)
    Q_PROPERTY(QString nextKind READ nextKind NOTIFY neighborsChanged)
    Q_PROPERTY(QString previousPath READ previousPath NOTIFY neighborsChanged)
    Q_PROPERTY(QUrl previousUrl READ previousUrl NOTIFY neighborsChanged)
    Q_PROPERTY(QString previousKind READ previousKind NOTIFY neighborsChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool animationsRunning READ animationsRunning NOTIFY animationsRunningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    // Search text; while it matches anything, playback stays within the matches.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    // Whether filter is a regular expression rather than plain text.
    Q_PROPERTY(bool filterRegex READ filterRegex WRITE setFilterRegex NOTIFY filterChanged)
    // False while filter is an invalid regular expression.
    Q_PROPERTY(bool filterValid READ filterValid NOTIFY filterChanged)

public:
    PlaybackController(Library *library, ProfileSettings *settings, QObject *parent = nullptr);

    Playlist *playlist() const
    {
        return m_playlist;
    }
    QString currentPath() const
    {
        return m_path;
    }
    int currentRow() const
    {
        return m_row;
    }
    QUrl currentUrl() const
    {
        return m_url;
    }
    QString currentKind() const
    {
        return m_kind;
    }
    QString nextPath() const
    {
        return m_next.path;
    }
    QUrl nextUrl() const
    {
        return m_next.url;
    }
    QString nextKind() const
    {
        return m_next.kind;
    }
    QString previousPath() const
    {
        return m_previous.path;
    }
    QUrl previousUrl() const
    {
        return m_previous.url;
    }
    QString previousKind() const
    {
        return m_previous.kind;
    }
    bool paused() const
    {
        return m_paused;
    }
    void setPaused(bool paused);
    QString filter() const
    {
        return m_filter;
    }
    void setFilter(const QString &filter);
    bool filterRegex() const
    {
        return m_filterRegex;
    }
    void setFilterRegex(bool regex);
    bool filterValid() const
    {
        return m_filterValid;
    }
    bool animationsRunning() const;
    double progress() const;
    qint64 elapsedMs() const;

    // Stops the clock for reasons other than a user pause: stopped, hidden, locked
    // screen or fullscreen app.
    void setSuspended(bool suspended);
    // Resumes at path with elapsedMs already spent. When the file is gone, the next
    // surviving item in the order plays instead; when it is unknown, fallbackRow does.
    void restore(const QString &path, qint64 elapsedMs, int fallbackRow);
    // Cold start: shows the first file the indexer finds before the order exists.
    void showProvisional(const QString &path);
    // Asks the screens to load the items again, after their folders came back.
    void retryMedia();

    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void first();
    Q_INVOKABLE void last();
    Q_INVOKABLE void jumpTo(int row);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void reportError(const QString &path);
    Q_INVOKABLE void reportLoopLength(const QString &path, qint64 loopMs);

Q_SIGNALS:
    void currentChanged();
    void currentMediaChanged();
    void neighborsChanged();
    void pausedChanged();
    void animationsRunningChanged();
    void progressChanged();
    void filterChanged();
    // A change the state file should record.
    void stateChanged();

private:
    void setCurrent(const QString &path, qint64 elapsedMs = 0);
    void relocate();
    void updateMedia();
    void updateNeighbors();
    void updateClock();
    void schedule();
    void applyFilter();
    // Nearest row after (step 1) or before (step -1) from that isn't marked bad and
    // passes the filter.
    int findRow(int from, int step) const;
    QString kindOf(const QString &path) const;
    bool ticking() const;

    Library *m_library;
    ProfileSettings *m_settings;
    Playlist *m_playlist;
    QString m_filter;
    bool m_filterRegex = false;
    bool m_filterValid = true;

    QString m_path;
    int m_row = -1;
    QUrl m_url;
    QString m_kind;
    struct Neighbor {
        QString path;
        QUrl url;
        QString kind;
        bool operator==(const Neighbor &) const = default;
    };
    Neighbor neighbor(int step) const;
    Neighbor m_next;
    Neighbor m_previous;
    QString m_fallbackPath;

    bool m_paused = false;
    bool m_suspended = true;
    bool m_wasTicking = false;
    bool m_animationsRunning = false;
    qint64 m_durationMs = 0;
    qint64 m_accumulatedMs = 0;
    QElapsedTimer m_clock;
    QTimer m_advanceTimer;
    QTimer m_progressTimer;
};
