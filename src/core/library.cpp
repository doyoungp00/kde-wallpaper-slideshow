#include "library.h"
#include "iogate.h"
#include "mediaprobe.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>
#include <QtConcurrentRun>

#include <algorithm>

namespace
{
constexpr quint32 CacheMagic = 0x4B575349; // "KWSI"
constexpr quint32 CacheVersion = 2;
}

Library::Library(QObject *parent)
    : QObject(parent)
{
    m_probePool.setMaxThreadCount(2);
}

Library::Kind Library::kindOf(const QString &path)
{
    const QStringView ext = QStringView(path).mid(path.lastIndexOf(QLatin1Char('.')) + 1);
    if (ext.compare(QLatin1String("mp4"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("webm"), Qt::CaseInsensitive) == 0) {
        return Kind::Video;
    }
    if (ext.compare(QLatin1String("gif"), Qt::CaseInsensitive) == 0 || ext.compare(QLatin1String("webp"), Qt::CaseInsensitive) == 0) {
        return Kind::Animated;
    }
    return Kind::Image;
}

void Library::reset(const QStringList &folders, const QStringList &sortedPaths, quint64 seed)
{
    QList<Entry> entries;
    entries.reserve(sortedPaths.size());
    for (const QString &path : sortedPaths) {
        const int old = indexOf(path);
        entries.append(old >= 0 ? m_entries.at(old) : Entry{path});
    }
    seededShuffle(entries, seed);
    m_folders = folders;
    m_seed = seed;
    m_entries = std::move(entries);
    rebuildIndex();
    Q_EMIT orderReset();
    Q_EMIT modified();
}

void Library::reshuffle(quint64 seed)
{
    std::sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) {
        return a.path < b.path;
    });
    seededShuffle(m_entries, seed);
    m_seed = seed;
    rebuildIndex();
    Q_EMIT orderReset();
    Q_EMIT modified();
}

void Library::clear(const QStringList &folders)
{
    m_folders = folders;
    m_entries.clear();
    rebuildIndex();
    Q_EMIT orderReset();
    Q_EMIT modified();
}

bool Library::merge(const QStringList &scannedPaths, const QString &anchorPath)
{
    const QSet<QString> scanned(scannedPaths.cbegin(), scannedPaths.cend());
    QSet<QString> removed;
    for (const Entry &e : std::as_const(m_entries)) {
        if (!scanned.contains(e.path)) {
            removed.insert(e.path);
        }
    }
    QStringList added;
    for (const QString &path : scannedPaths) {
        if (!m_index.contains(path)) {
            added.append(path);
        }
    }
    if (removed.isEmpty() && added.isEmpty()) {
        return false;
    }

    Q_EMIT aboutToMerge(removed);

    // New files are inserted after the anchor, or after the survivors that preceded
    // it when the anchor itself was removed.
    const int anchor = indexOf(anchorPath);
    QList<Entry> survivors;
    survivors.reserve(m_entries.size() - removed.size());
    qsizetype base = 0;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (!removed.contains(m_entries.at(i).path)) {
            survivors.append(m_entries.at(i));
        }
        if (i == anchor) {
            base = survivors.size();
        }
    }

    std::mt19937_64 rng(m_seed ^ quint64(m_entries.size()) << 32 ^ quint64(added.size()));
    const qsizetype span = survivors.size() - base + 1;
    QList<std::pair<qsizetype, QString>> inserts;
    inserts.reserve(added.size());
    for (const QString &path : std::as_const(added)) {
        inserts.append({base + qsizetype(rng() % quint64(span)), path});
    }
    std::stable_sort(inserts.begin(), inserts.end(), [](const auto &a, const auto &b) {
        return a.first < b.first;
    });

    QList<Entry> entries;
    entries.reserve(survivors.size() + inserts.size());
    auto next = inserts.cbegin();
    for (qsizetype i = 0; i <= survivors.size(); ++i) {
        for (; next != inserts.cend() && next->first == i; ++next) {
            entries.append(Entry{next->second});
        }
        if (i < survivors.size()) {
            entries.append(survivors.at(i));
        }
    }
    m_entries = std::move(entries);
    rebuildIndex();
    Q_EMIT merged();
    Q_EMIT modified();
    return true;
}

void Library::setLoopMs(int index, qint64 loopMs)
{
    if (m_entries[index].loopMs != loopMs) {
        m_entries[index].loopMs = loopMs;
        Q_EMIT entryChanged(index);
        Q_EMIT modified();
    }
}

void Library::setBad(int index, bool bad)
{
    if (m_entries[index].bad != bad) {
        m_entries[index].bad = bad;
        Q_EMIT entryChanged(index);
        Q_EMIT modified();
    }
}

void Library::clearBadMarks()
{
    for (int i = 0; i < m_entries.size(); ++i) {
        setBad(i, false);
    }
}

void Library::ensureProbed(int index)
{
    const Entry &e = m_entries.at(index);
    if (e.loopMs != LoopUnknown || kindOf(e.path) != Kind::Animated || m_probing.contains(e.path)) {
        return;
    }
    const QString path = e.path;
    m_probing.insert(path);
    QtConcurrent::run(&m_probePool, [path] {
        IoGate::Scope io;
        return MediaProbe::loopLengthMs(path);
    }).then(this, [this, path](std::optional<qint64> loop) {
        m_probing.remove(path);
        const int i = indexOf(path);
        if (i < 0) {
            return;
        }
        if (loop) {
            setLoopMs(i, *loop);
        } else {
            setBad(i, true);
        }
    });
}

bool Library::load(const QString &cacheFile, const QStringList &expectedFolders)
{
    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QDataStream in(&file);
    quint32 magic, version;
    in >> magic >> version;
    if (magic != CacheMagic || version != CacheVersion) {
        return false;
    }
    QStringList folders;
    quint64 seed;
    quint32 count;
    in >> folders >> seed >> count;
    if (folders != expectedFolders || in.status() != QDataStream::Ok) {
        return false;
    }
    QList<Entry> entries(count);
    for (Entry &e : entries) {
        in >> e.path >> e.loopMs >> e.bad;
    }
    if (in.status() != QDataStream::Ok) {
        return false;
    }
    m_folders = folders;
    m_seed = seed;
    m_entries = std::move(entries);
    rebuildIndex();
    Q_EMIT orderReset();
    return true;
}

bool Library::save(const QString &cacheFile) const
{
    QSaveFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream out(&file);
    out << CacheMagic << CacheVersion << m_folders << m_seed << quint32(m_entries.size());
    for (const Entry &e : m_entries) {
        out << e.path << e.loopMs << e.bad;
    }
    return file.commit();
}

void Library::rebuildIndex()
{
    m_index.clear();
    m_index.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        m_index.insert(m_entries.at(i).path, i);
    }
}
