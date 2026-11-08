#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>

#include <random>

// Fisher-Yates with a fixed generator and modulo bound, so a seed gives the same
// order on every platform and run.
template<typename T>
void seededShuffle(QList<T> &list, quint64 seed)
{
    std::mt19937_64 rng(seed);
    for (qsizetype i = list.size() - 1; i > 0; --i) {
        list.swapItemsAt(i, qsizetype(rng() % quint64(i + 1)));
    }
}

// The media files pooled from a list of folders, held in playlist order. Owns the
// per-file probe results and bad marks, and persists all of it as the index cache.
class Library : public QObject
{
    Q_OBJECT
public:
    // Values of Entry::loopMs besides a measured loop length.
    static constexpr qint64 LoopUnknown = -2;
    static constexpr qint64 LoopStill = -1;

    enum class Kind { Image, Animated, Video };

    struct Entry {
        QString path; // absolute
        qint64 loopMs = LoopUnknown;
        bool bad = false;
    };

    explicit Library(QObject *parent = nullptr);

    static Kind kindOf(const QString &path);
    static bool isVideo(const QString &path)
    {
        return kindOf(path) == Kind::Video;
    }

    QStringList folders() const
    {
        return m_folders;
    }
    quint64 seed() const
    {
        return m_seed;
    }
    int count() const
    {
        return m_entries.size();
    }
    const Entry &at(int i) const
    {
        return m_entries.at(i);
    }
    int indexOf(const QString &path) const
    {
        return m_index.value(path, -1);
    }

    // Replaces the contents with sortedPaths shuffled by seed. Probe results and bad
    // marks of paths already known are kept.
    void reset(const QStringList &folders, const QStringList &sortedPaths, quint64 seed);
    void reshuffle(quint64 seed);
    void clear(const QStringList &folders);
    // Takes a new folder list as the cache key; the next merge() brings the files in line.
    void setFolders(const QStringList &folders)
    {
        m_folders = folders;
    }
    // Applies a rescan: removed files are dropped, new files go to random positions
    // after anchorPath. Returns false if nothing changed.
    bool merge(const QStringList &scannedPaths, const QString &anchorPath);

    void setLoopMs(int index, qint64 loopMs);
    void setBad(int index, bool bad);
    void clearBadMarks();
    // Starts a background header probe for an animation whose loop length is unknown.
    void ensureProbed(int index);

    bool load(const QString &cacheFile, const QStringList &expectedFolders);
    bool save(const QString &cacheFile) const;

Q_SIGNALS:
    void orderReset();
    void aboutToMerge(const QSet<QString> &removed);
    void merged();
    void entryChanged(int index);
    void modified();

private:
    void rebuildIndex();

    QStringList m_folders;
    quint64 m_seed = 0;
    QList<Entry> m_entries;
    QHash<QString, int> m_index;
    QSet<QString> m_probing;
    QThreadPool m_probePool;
};
