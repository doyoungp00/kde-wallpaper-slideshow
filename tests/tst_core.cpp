#include "core/iogate.h"
#include "core/library.h"
#include "core/mediaprobe.h"
#include "core/smartduration.h"

#include <QBuffer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>

namespace
{
QByteArray le16(int v)
{
    const char bytes[] = {char(v & 0xff), char(v >> 8 & 0xff)};
    return QByteArray(bytes, 2);
}

QByteArray le24(int v)
{
    return le16(v) + char(v >> 16 & 0xff);
}

QByteArray le32(int v)
{
    return le24(v) + char(v >> 24 & 0xff);
}

// A 1x1 GIF with one frame per delay (in centiseconds).
QByteArray makeGif(const QList<int> &delaysCs)
{
    QByteArray gif = "GIF89a" + le16(1) + le16(1) + QByteArray("\x80\x00\x00", 3) + QByteArray(6, '\0'); // 2-entry colour table
    gif += QByteArray("\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00", 19); // loop extension, skipped
    for (int delay : delaysCs) {
        gif += QByteArray("\x21\xf9\x04\x00", 4) + le16(delay) + QByteArray("\x00\x00", 2);
        gif += '\x2c' + le16(0) + le16(0) + le16(1) + le16(1) + '\0';
        gif += QByteArray("\x02\x02\x4c\x01\x00", 5);
    }
    return gif + '\x3b';
}

QByteArray riffChunk(const QByteArray &fourcc, const QByteArray &payload)
{
    QByteArray chunk = fourcc + le32(payload.size()) + payload;
    return payload.size() % 2 ? chunk + '\0' : chunk;
}

QByteArray makeWebp(const QList<int> &durationsMs)
{
    QByteArray body = "WEBP";
    if (durationsMs.isEmpty()) {
        body += riffChunk("VP8 ", QByteArray(11, 'x'));
    } else {
        body += riffChunk("VP8X", QByteArray("\x02", 1) + QByteArray(9, '\0'));
        body += riffChunk("ANIM", QByteArray(6, '\0'));
        for (int duration : durationsMs) {
            // Odd-sized frame data exercises the chunk padding.
            body += riffChunk("ANMF", le24(0) + le24(0) + le24(0) + le24(0) + le24(duration) + '\0' + QByteArray(3, 'f'));
        }
    }
    return "RIFF" + le32(body.size()) + body;
}

std::optional<qint64> probe(std::optional<qint64> (*parser)(QIODevice &), QByteArray data)
{
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    return parser(buffer);
}

QStringList names(int n)
{
    QStringList list;
    for (int i = 0; i < n; ++i) {
        list.append(QStringLiteral("/r/dir%1/file%2.jpg").arg(i % 3).arg(i, 3, 10, QLatin1Char('0')));
    }
    list.sort();
    return list;
}

QStringList order(const Library &library)
{
    QStringList list;
    for (int i = 0; i < library.count(); ++i) {
        list.append(library.at(i).path);
    }
    return list;
}
}

class TestCore : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void gifLoop()
    {
        // 0 cs is shown as 10 cs.
        QCOMPARE(probe(MediaProbe::gifLoopMs, makeGif({5, 0, 20})), qint64(350));
        QCOMPARE(probe(MediaProbe::gifLoopMs, makeGif({1, 1})), qint64(200));
        QCOMPARE(probe(MediaProbe::gifLoopMs, makeGif({7})), MediaProbe::Still);
        QCOMPARE(probe(MediaProbe::gifLoopMs, "GIF89a"), std::nullopt);
        QCOMPARE(probe(MediaProbe::gifLoopMs, "not a gif at all"), std::nullopt);
    }

    void gifTruncated()
    {
        // A file cut off in the last frame's image data still counts that frame.
        QByteArray gif = makeGif({10, 10, 10});
        gif.chop(3);
        QCOMPARE(probe(MediaProbe::gifLoopMs, gif), qint64(300));
        // Cut inside its descriptor, only the complete frames count.
        gif.chop(8);
        QCOMPARE(probe(MediaProbe::gifLoopMs, gif), qint64(200));
        gif = makeGif({10, 10, 10});
        gif.chop(3);
        QCOMPARE(probe(MediaProbe::gifLoopMs, gif), qint64(300));
    }

    void webpLoop()
    {
        QCOMPARE(probe(MediaProbe::webpLoopMs, makeWebp({100, 250, 40})), qint64(390));
        QCOMPARE(probe(MediaProbe::webpLoopMs, makeWebp({})), MediaProbe::Still);
        QCOMPARE(probe(MediaProbe::webpLoopMs, makeWebp({100})), MediaProbe::Still);
        QCOMPARE(probe(MediaProbe::webpLoopMs, "RIFF\0\0\0\0WAVE"), std::nullopt);
    }

    void smartDuration_data()
    {
        QTest::addColumn<qint64>("loop");
        QTest::addColumn<bool>("smart");
        QTest::addColumn<bool>("roundUp");
        QTest::addColumn<qint64>("expected");
        // T = 10 s, M = 60 s
        QTest::newRow("still") << qint64(-1) << true << false << qint64(10000);
        QTest::newRow("unknown") << qint64(-2) << true << false << qint64(10000);
        QTest::newRow("short loop") << qint64(3000) << true << false << qint64(10000);
        QTest::newRow("short loop rounded") << qint64(3000) << true << true << qint64(12000);
        QTest::newRow("exact loop rounded") << qint64(5000) << true << true << qint64(10000);
        QTest::newRow("one loop") << qint64(25000) << true << false << qint64(25000);
        QTest::newRow("capped") << qint64(90000) << true << false << qint64(60000);
        QTest::newRow("smart off") << qint64(25000) << false << false << qint64(10000);
    }

    void smartDuration()
    {
        QFETCH(qint64, loop);
        QFETCH(bool, smart);
        QFETCH(bool, roundUp);
        QFETCH(qint64, expected);
        QCOMPARE(effectiveDurationMs(10000, 60000, loop, smart, roundUp), expected);
    }

    void shuffleIsDeterministic()
    {
        Library a, b, c;
        a.reset(QStringList{QStringLiteral("/r")}, names(50), 42);
        b.reset(QStringList{QStringLiteral("/r")}, names(50), 42);
        c.reset(QStringList{QStringLiteral("/r")}, names(50), 43);
        QCOMPARE(order(a), order(b));
        QVERIFY(order(a) != order(c));
        QVERIFY(order(a) != names(50));

        // Reshuffling back to the same seed gives the same order again.
        c.reshuffle(42);
        QCOMPARE(order(c), order(a));
    }

    void mergeKeepsOrder()
    {
        Library library;
        library.reset(QStringList{QStringLiteral("/r")}, names(20), 7);
        const QStringList before = order(library);
        const QString anchor = before.at(5);
        const QString removed = before.at(10);

        QStringList scanned = names(20);
        scanned.removeAll(removed);
        scanned << QStringLiteral("/r/new/a.png") << QStringLiteral("/r/new/b.png");
        scanned.sort();

        QSignalSpy aboutToMerge(&library, &Library::aboutToMerge);
        QVERIFY(library.merge(scanned, anchor));
        QCOMPARE(aboutToMerge.count(), 1);
        QCOMPARE(aboutToMerge.first().first().value<QSet<QString>>(), QSet<QString>{removed});

        const QStringList after = order(library);
        QCOMPARE(after.size(), 21);
        QCOMPARE(library.indexOf(removed), -1);
        QVERIFY(library.indexOf(QStringLiteral("/r/new/a.png")) > library.indexOf(anchor));
        QVERIFY(library.indexOf(QStringLiteral("/r/new/b.png")) > library.indexOf(anchor));

        QStringList survivors = after;
        survivors.removeAll(QStringLiteral("/r/new/a.png"));
        survivors.removeAll(QStringLiteral("/r/new/b.png"));
        QStringList expected = before;
        expected.removeAll(removed);
        QCOMPARE(survivors, expected);

        QVERIFY(!library.merge(scanned, anchor));
    }

    void cacheRoundTrip()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("index.bin"));
        Library library;
        library.reset(QStringList{QStringLiteral("/r")}, names(30), 99);
        library.setLoopMs(3, 1234);
        library.setBad(4, true);
        QVERIFY(library.save(file));

        Library loaded;
        QVERIFY(!loaded.load(file, QStringList{QStringLiteral("/other")}));
        QVERIFY(loaded.load(file, QStringList{QStringLiteral("/r")}));
        QCOMPARE(order(loaded), order(library));
        QCOMPARE(loaded.seed(), quint64(99));
        QCOMPARE(loaded.at(3).loopMs, qint64(1234));
        QVERIFY(loaded.at(4).bad);
        QCOMPARE(loaded.at(5).loopMs, Library::LoopUnknown);
    }

    void ioGatePausesReads()
    {
        using namespace std::chrono_literals;
        IoGate &gate = IoGate::instance();
        QVERIFY(gate.pause(10ms)); // nothing in flight
        gate.resume();

        std::atomic_bool release = false;
        std::atomic_int reads = 0;
        QThread *reader = QThread::create([&] {
            {
                IoGate::Scope io; // a read in flight
                while (!release) {
                    QThread::msleep(1);
                }
            }
            IoGate::Scope next; // waits while paused
            ++reads;
        });
        reader->start();
        QThread::msleep(20);
        QVERIFY(!gate.pause(30ms)); // the read is still in flight
        release = true;
        QVERIFY(gate.pause(1000ms)); // it finished; the next one waits
        QThread::msleep(20);
        QCOMPARE(reads.load(), 0);
        gate.resume();
        QVERIFY(reader->wait(1000));
        QCOMPARE(reads.load(), 1);
        delete reader;
    }

    void resetKeepsProbeResults()
    {
        Library library;
        library.reset(QStringList{QStringLiteral("/r")}, names(10), 1);
        const QString path = library.at(0).path;
        library.setLoopMs(0, 500);
        library.reset(QStringList{QStringLiteral("/r")}, names(10), 2);
        QCOMPARE(library.at(library.indexOf(path)).loopMs, qint64(500));
    }
};

QTEST_GUILESS_MAIN(TestCore)
#include "tst_core.moc"
