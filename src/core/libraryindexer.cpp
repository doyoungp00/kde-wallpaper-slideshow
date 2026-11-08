#include "libraryindexer.h"
#include "iogate.h"

#include <QFile>
#include <QThreadPool>
#include <QtConcurrentMap>
#include <QtConcurrentRun>

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstring>

namespace
{
bool hasMediaExtension(const char *name, bool includeVideo)
{
    const char *dot = std::strrchr(name, '.');
    if (!dot) {
        return false;
    }
    const char *ext = dot + 1;
    for (const char *e : {"png", "jpg", "jpeg", "gif", "webp"}) {
        if (strcasecmp(ext, e) == 0) {
            return true;
        }
    }
    return includeVideo && (strcasecmp(ext, "mp4") == 0 || strcasecmp(ext, "webm") == 0);
}

struct Listing {
    QStringList files;
    QStringList dirs;
};

// Lists one directory: matching files are appended as absolute paths, and
// subdirectories are returned. Hidden entries are skipped, and symlinked
// directories are not followed so the walk can't loop.
QList<QByteArray> listDir(const QByteArray &absDir, bool includeVideo, QStringList &files)
{
    IoGate::Scope io;
    QList<QByteArray> subdirs;
    DIR *dir = opendir(absDir.constData());
    if (!dir) {
        return subdirs;
    }
    while (const dirent *ent = readdir(dir)) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        const QByteArray abs = absDir + '/' + ent->d_name;
        unsigned char type = ent->d_type;
        if (type == DT_UNKNOWN || type == DT_LNK) {
            struct stat st;
            if ((type == DT_LNK ? stat(abs.constData(), &st) : lstat(abs.constData(), &st)) != 0) {
                continue;
            }
            type = S_ISREG(st.st_mode) ? DT_REG : (S_ISDIR(st.st_mode) && type != DT_LNK) ? DT_DIR : DT_UNKNOWN;
        }
        if (type == DT_REG && hasMediaExtension(ent->d_name, includeVideo)) {
            files.append(QFile::decodeName(abs));
        } else if (type == DT_DIR) {
            subdirs.append(abs);
        }
    }
    closedir(dir);
    return subdirs;
}
}

LibraryIndexer::LibraryIndexer(QObject *parent)
    : QObject(parent)
{
}

LibraryIndexer::~LibraryIndexer()
{
    cancel();
}

LibraryIndexer::Result LibraryIndexer::scan(const QStringList &folders,
                                            bool recursive,
                                            bool includeVideo,
                                            const std::atomic_bool &cancel,
                                            const std::function<void(const QString &)> &firstFound)
{
    Result result;
    std::atomic_bool reported = false;
    auto report = [&](const QStringList &files) {
        if (firstFound && !files.isEmpty() && !reported.exchange(true)) {
            firstFound(files.first());
        }
    };

    QList<QByteArray> topDirs;
    for (const QString &folder : folders) {
        const QByteArray root = QFile::encodeName(folder);
        struct stat st;
        if (stat(root.constData(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            result.missing.append(folder);
            continue;
        }
        result.ok = true;
        result.dirs.append(folder);
        topDirs.append(listDir(root, includeVideo, result.files));
        report(result.files);
    }

    if (recursive) {
        QThreadPool pool;
        pool.setMaxThreadCount(std::max(4, QThread::idealThreadCount()));
        const QList<Listing> listings = QtConcurrent::blockingMapped(&pool, topDirs, [&](const QByteArray &top) {
            Listing listing;
            QList<QByteArray> stack{top};
            while (!stack.isEmpty() && !cancel) {
                const QByteArray dir = stack.takeLast();
                listing.dirs.append(QFile::decodeName(dir));
                stack.append(listDir(dir, includeVideo, listing.files));
                report(listing.files);
            }
            return listing;
        });
        for (const Listing &listing : listings) {
            result.files.append(listing.files);
            result.dirs.append(listing.dirs);
        }
    }
    if (cancel) {
        result.ok = false;
        return result;
    }
    // Folders may overlap; a file reached twice is listed once.
    std::sort(result.files.begin(), result.files.end());
    result.files.erase(std::unique(result.files.begin(), result.files.end()), result.files.end());
    result.dirs.removeDuplicates();
    return result;
}

void LibraryIndexer::start(const QStringList &folders, bool recursive, bool includeVideo)
{
    cancel();
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto token = m_cancel;
    QtConcurrent::run([=, this] {
        return scan(folders, recursive, includeVideo, *token, [=, this](const QString &path) {
            QMetaObject::invokeMethod(this, [=, this] {
                if (!*token) {
                    Q_EMIT firstFileFound(path);
                }
            });
        });
    }).then(this, [=, this](const Result &result) {
        if (*token) {
            return;
        }
        m_running = false;
        Q_EMIT runningChanged();
        Q_EMIT finished(result);
    });
    m_running = true;
    Q_EMIT runningChanged();
}

void LibraryIndexer::cancel()
{
    if (m_cancel) {
        *m_cancel = true;
        m_cancel.reset();
    }
    if (m_running) {
        m_running = false;
        Q_EMIT runningChanged();
    }
}
