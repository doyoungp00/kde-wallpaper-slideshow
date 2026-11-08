#include "core/library.h"
#include "playbackcontroller.h"
#include "profilesettings.h"

#include <QRegularExpression>
#include <QStandardPaths>
#include <QTest>

// Navigation through a library of a00.jpg, b01.jpg, a02.jpg, ... in shuffled order.
class TestPlayback : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QStringList paths;
        for (int i = 0; i < 20; ++i) {
            paths.append(QStringLiteral("/r/%1%2.jpg").arg(i % 2 ? 'b' : 'a').arg(i, 2, 10, QLatin1Char('0')));
        }
        paths.sort();
        m_library.reset({QStringLiteral("/r")}, paths, 5);
    }

    void unfilteredSteps()
    {
        PlaybackController c(&m_library, &m_settings);
        c.jumpTo(3);
        c.next();
        QCOMPARE(c.currentRow(), 4);
        c.previous();
        c.previous();
        QCOMPARE(c.currentRow(), 2);
    }

    void filteredSteps()
    {
        PlaybackController c(&m_library, &m_settings);
        c.setFilter(QStringLiteral("B"));
        c.first();
        for (int i = 0; i < 25; ++i) {
            QVERIFY2(name(c.currentPath()).startsWith(QLatin1Char('b')), qPrintable(c.currentPath()));
            QVERIFY(name(c.nextPath()).startsWith(QLatin1Char('b')));
            const int before = c.currentRow();
            i % 3 ? c.next() : c.previous();
            QVERIFY(c.currentRow() != before); // every press moves
        }
        c.last();
        QVERIFY(name(c.currentPath()).startsWith(QLatin1Char('b')));
    }

    void jumpOutsideFilterThenNext()
    {
        PlaybackController c(&m_library, &m_settings);
        int aRow = 0;
        while (!name(m_library.at(aRow).path).startsWith(QLatin1Char('a'))) {
            ++aRow;
        }
        c.jumpTo(aRow);
        c.setFilter(QStringLiteral("b"));
        // The current item stays; the next one comes from the matches.
        QCOMPARE(c.currentRow(), aRow);
        QVERIFY(name(c.nextPath()).startsWith(QLatin1Char('b')));
        c.next();
        QVERIFY(name(c.currentPath()).startsWith(QLatin1Char('b')));
    }

    void filterWithoutMatchesIsIgnored()
    {
        PlaybackController c(&m_library, &m_settings);
        c.jumpTo(0);
        c.setFilter(QStringLiteral("zzz"));
        c.next();
        QCOMPARE(c.currentRow(), 1);
    }

    void regexFilter()
    {
        PlaybackController c(&m_library, &m_settings);
        c.setFilterRegex(true);
        c.setFilter(QStringLiteral("^B0[0-4]"));
        QVERIFY(c.filterValid());
        c.first();
        for (int i = 0; i < 10; ++i) {
            QVERIFY2(QRegularExpression(QStringLiteral("^b0[1-3]\\.")).match(name(c.currentPath())).hasMatch(), qPrintable(c.currentPath()));
            c.next();
        }
        // Plain text treats the same pattern literally, so nothing matches.
        c.setFilterRegex(false);
        QVERIFY(c.filterValid());
        QCOMPARE(c.playlist()->filtered()->rowCount(), 0);
    }

    void invalidRegexIsIgnored()
    {
        PlaybackController c(&m_library, &m_settings);
        c.setFilterRegex(true);
        c.setFilter(QStringLiteral("a("));
        QVERIFY(!c.filterValid());
        c.jumpTo(0);
        c.next();
        QCOMPARE(c.currentRow(), 1);
    }

    void filterMatchesSubfolders()
    {
        Library library;
        library.reset({QStringLiteral("/root/"), QStringLiteral("/other")},
                      {QStringLiteral("/other/c.jpg"), QStringLiteral("/root/cats/a.jpg"), QStringLiteral("/root/cats/kittens/b.jpg"), QStringLiteral("/root/d.jpg")},
                      1);
        PlaybackController c(&library, &m_settings);
        c.setFilter(QStringLiteral("cats"));
        QCOMPARE(c.playlist()->filtered()->rowCount(), 2);
        c.setFilter(QStringLiteral("cats/kit"));
        QCOMPARE(c.playlist()->filtered()->rowCount(), 1);
        // The library folders themselves aren't part of what is searched.
        c.setFilter(QStringLiteral("o"));
        QCOMPARE(c.playlist()->filtered()->rowCount(), 0);
    }

    void relativePathStartsAtFolderName()
    {
        PlaybackController c(&m_library, &m_settings);
        const QModelIndex index = c.playlist()->index(0);
        QCOMPARE(index.data(Playlist::RelativePathRole).toString(), QStringLiteral("r/") + name(m_library.at(0).path));
        QCOMPARE(index.data(Playlist::PathRole).toString(), m_library.at(0).path);
    }

    void restoreKeepsPlaceWhileFolderIsMissing()
    {
        // /r doesn't exist, like a network share that isn't mounted yet at login.
        PlaybackController c(&m_library, &m_settings);
        const QString saved = m_library.at(7).path;
        c.restore(saved, 4321, 7);
        QCOMPARE(c.currentPath(), saved);
        QCOMPARE(c.elapsedMs(), qint64(4321));
    }

private:
    static QString name(const QString &path)
    {
        return path.section(QLatin1Char('/'), -1);
    }

    Library m_library;
    ProfileSettings m_settings{QStringLiteral("test")};
};

QTEST_GUILESS_MAIN(TestPlayback)
#include "tst_playback.moc"
