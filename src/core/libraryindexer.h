#pragma once

#include <QObject>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>

// Walks folder trees for media files without opening any of them.
class LibraryIndexer : public QObject
{
    Q_OBJECT
public:
    struct Result {
        bool ok = false; // at least one folder was readable
        QStringList files; // absolute paths, sorted, without duplicates
        QStringList dirs; // every directory visited, the folders included
        QStringList missing; // folders that are missing or unreadable
    };

    explicit LibraryIndexer(QObject *parent = nullptr);
    ~LibraryIndexer() override;

    // Synchronous scan; one task per top-level subfolder runs on a thread pool.
    // firstFound is called once, from a worker thread, with the first match.
    static Result scan(const QStringList &folders,
                       bool recursive,
                       bool includeVideo,
                       const std::atomic_bool &cancel,
                       const std::function<void(const QString &)> &firstFound = {});

    // Runs scan() in the background. A new start() or cancel() discards a scan in flight.
    void start(const QStringList &folders, bool recursive, bool includeVideo);
    void cancel();
    bool isRunning() const
    {
        return m_running;
    }

Q_SIGNALS:
    void runningChanged();
    void firstFileFound(const QString &path);
    void finished(const LibraryIndexer::Result &result);

private:
    std::shared_ptr<std::atomic_bool> m_cancel;
    bool m_running = false;
};
