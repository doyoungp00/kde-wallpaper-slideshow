#include "core/library.h"
#include "core/libraryindexer.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// Startup budget check on a synthetic 50k-file library (F1, F2).
class BenchIndexer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // 50 top-level folders x 20 subfolders x 50 files, plus non-media noise.
        const QStringList extensions{QStringLiteral("jpg"), QStringLiteral("png"), QStringLiteral("gif"), QStringLiteral("webp"), QStringLiteral("JPEG")};
        int n = 0;
        for (int top = 0; top < 50; ++top) {
            for (int sub = 0; sub < 20; ++sub) {
                const QString dir = m_dir.path() + QStringLiteral("/album%1/set%2").arg(top).arg(sub);
                QVERIFY(QDir().mkpath(dir));
                for (int i = 0; i < 50; ++i, ++n) {
                    QVERIFY(QFile(dir + QStringLiteral("/img%1.%2").arg(i).arg(extensions.at(n % extensions.size()))).open(QIODevice::WriteOnly));
                }
                QVERIFY(QFile(dir + QStringLiteral("/notes.txt")).open(QIODevice::WriteOnly));
            }
        }
        QCOMPARE(n, 50000);
    }

    void coldIndex()
    {
        const std::atomic_bool cancel = false;
        QElapsedTimer timer;
        timer.start();
        const LibraryIndexer::Result result = LibraryIndexer::scan({m_dir.path()}, true, true, cancel);
        const qint64 scanMs = timer.elapsed();
        Library library;
        library.reset({m_dir.path()}, result.files, 1);
        qInfo("scan %lld ms, scan + shuffle %lld ms, %lld dirs", scanMs, timer.elapsed(), qlonglong(result.dirs.size()));
        QVERIFY(result.ok);
        QCOMPARE(result.files.size(), 50000);
        QCOMPARE(result.dirs.size(), 1 + 50 + 1000);
        QVERIFY2(timer.elapsed() < 15000, "cold indexing is over budget");
        QVERIFY(library.save(m_dir.filePath(QStringLiteral("index.bin"))));
    }

    void warmLoad()
    {
        QElapsedTimer timer;
        timer.start();
        Library library;
        QVERIFY(library.load(m_dir.filePath(QStringLiteral("index.bin")), {m_dir.path()}));
        qInfo("warm load %lld ms", timer.elapsed());
        QCOMPARE(library.count(), 50000);
        QVERIFY2(timer.elapsed() < 1000, "warm start is over budget");
    }

    void nonRecursive()
    {
        const std::atomic_bool cancel = false;
        const LibraryIndexer::Result result = LibraryIndexer::scan({m_dir.path() + QStringLiteral("/album0/set0")}, false, true, cancel);
        QCOMPARE(result.files.size(), 50);
    }

    void pooledFolders()
    {
        // Overlapping folders pool into one list without duplicates; a missing one
        // is reported but doesn't stop the others.
        const std::atomic_bool cancel = false;
        const QString missing = m_dir.path() + QStringLiteral("/no-such-folder");
        const LibraryIndexer::Result result = LibraryIndexer::scan(
            {m_dir.path() + QStringLiteral("/album0"), m_dir.path() + QStringLiteral("/album0/set0"), m_dir.path() + QStringLiteral("/album1"), missing},
            true,
            true,
            cancel);
        QVERIFY(result.ok);
        QCOMPARE(result.files.size(), 2000);
        QCOMPARE(result.missing, QStringList{missing});
        QVERIFY(result.files.first().startsWith(m_dir.path() + QLatin1Char('/')));
    }

private:
    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(BenchIndexer)
#include "bench_indexer.moc"
