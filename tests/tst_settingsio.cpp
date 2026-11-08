#include "profilesettings.h"
#include "settingsio.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

// Exporting and importing profile settings through a settings file.
class TestSettingsIO : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void roundTrip()
    {
        ProfileSettings from(QStringLiteral("rt-a"));
        from.setFolders({QStringLiteral("/a"), QStringLiteral("/b")});
        from.setDurationSec(45);
        from.setScaleMode(ProfileSettings::EnumScaleMode::Fit);
        from.setLetterboxColor(QColor(QStringLiteral("#80112233")));
        from.setPlaylistOpacity(0.5);
        from.setMute(false);
        const QUrl file = url(QStringLiteral("rt.json"));
        QCOMPARE(SettingsIO::write(file, {{QStringLiteral("profiles"), QJsonObject{{QStringLiteral("rt-a"), SettingsIO::toJson(&from)}}}}), QString());

        QJsonObject root;
        QCOMPARE(SettingsIO::read(file, root), QString());
        ProfileSettings to(QStringLiteral("rt-b"));
        QCOMPARE(apply(&to, root[QStringLiteral("profiles")][QStringLiteral("rt-a")].toObject()), 0);
        QCOMPARE(to.folders(), from.folders());
        QCOMPARE(to.durationSec(), 45);
        QCOMPARE(to.scaleMode(), int(ProfileSettings::EnumScaleMode::Fit));
        QCOMPARE(to.letterboxColor(), from.letterboxColor());
        QCOMPARE(to.playlistOpacity(), 0.5);
        QCOMPARE(to.mute(), false);

        // Saved, so a fresh read of the same profile sees the imported values.
        ProfileSettings reread(QStringLiteral("rt-b"));
        reread.read();
        QCOMPARE(reread.durationSec(), 45);
    }

    void invalidEntriesAreLeftOut()
    {
        ProfileSettings s(QStringLiteral("invalid"));
        const QJsonObject values{
            {QStringLiteral("durationSec"), 999999}, // clamped to 86400
            {QStringLiteral("playlistOpacity"), 0.0}, // clamped to 0.1
            {QStringLiteral("mute"), QStringLiteral("yes")}, // wrong type
            {QStringLiteral("volume"), 12.5}, // not an integer
            {QStringLiteral("scaleMode"), 3}, // no such choice
            {QStringLiteral("letterboxColor"), QStringLiteral("not a color")},
            {QStringLiteral("folders"), QJsonArray{QStringLiteral("/a"), 1}},
            {QStringLiteral("noSuchSetting"), true},
        };
        QCOMPARE(apply(&s, values), 6);
        QCOMPARE(s.durationSec(), 86400);
        QCOMPARE(s.playlistOpacity(), 0.1);
        QCOMPARE(s.mute(), true);
        QCOMPARE(s.volume(), 50);
        QCOMPARE(s.scaleMode(), int(ProfileSettings::EnumScaleMode::Cover));
        QCOMPARE(s.letterboxColor(), QColor(Qt::black));
        QVERIFY(s.folders().isEmpty());
    }

    void rejectsOtherFiles()
    {
        QJsonObject root;
        QVERIFY(!SettingsIO::read(url(QStringLiteral("missing.json")), root).isEmpty());
        QVERIFY(!SettingsIO::read(write(QStringLiteral("bad.json"), "{ not json"), root).isEmpty());
        QVERIFY(!SettingsIO::read(write(QStringLiteral("other.json"), R"({"profiles": {}})"), root).isEmpty());
        const QByteArray newer = R"({"format": "kde-wallpaper-slideshow-settings", "version": 2})";
        QVERIFY(SettingsIO::read(write(QStringLiteral("newer.json"), newer), root).contains(QLatin1String("newer")));
    }

private:
    // As Profile::applySettings() does.
    static int apply(ProfileSettings *s, const QJsonObject &values)
    {
        return SettingsIO::apply(s,
                                 values,
                                 {
                                     {QStringLiteral("scaleMode"), ProfileSettings::EnumScaleMode::COUNT},
                                     {QStringLiteral("playlistCorner"), ProfileSettings::EnumPlaylistCorner::COUNT},
                                 });
    }
    QUrl url(const QString &name) const
    {
        return QUrl::fromLocalFile(m_dir.filePath(name));
    }
    QUrl write(const QString &name, const QByteArray &contents) const
    {
        QFile f(m_dir.filePath(name));
        if (!f.open(QIODevice::WriteOnly)) {
            return QUrl(); // read() then fails for the wrong reason, and so does the test
        }
        f.write(contents);
        return url(name);
    }

    QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(TestSettingsIO)
#include "tst_settingsio.moc"
