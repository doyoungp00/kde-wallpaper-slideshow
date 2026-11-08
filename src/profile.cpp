#include "profile.h"
#include "autosave.h"
#include "settings.h"
#include "settingsio.h"

#include <KConfigGroup>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>

const QString Profile::DefaultId = QStringLiteral("default");

namespace
{
QString settingsGroup(const QString &id)
{
    return QStringLiteral("Profile ") + id;
}

ProfileSettings *createSettings(const QString &id, QObject *parent)
{
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QStringLiteral("kde-wallpaper-slideshowrc"));
    if (id != Profile::DefaultId && !config->hasGroup(settingsGroup(id)) && config->hasGroup(settingsGroup(Profile::DefaultId))) {
        KConfigGroup own = config->group(settingsGroup(id));
        config->group(settingsGroup(Profile::DefaultId)).copyTo(&own);
    }
    auto *settings = new ProfileSettings(id, parent);
    settings->read();
    saveOnChange(settings);
    return settings;
}

QString stateGroup(const QString &id)
{
    return QStringLiteral("Playback-") + id;
}
}

Profile::Profile(const QString &id, const QString &title, const KSharedConfig &state, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_title(title)
    , m_settings(createSettings(id, this))
    , m_controller(new PlaybackController(&m_library, m_settings, this))
{
    auto setupTimer = [this](QTimer &timer, int ms, bool singleShot, auto slot) {
        timer.setInterval(ms);
        timer.setSingleShot(singleShot);
        connect(&timer, &QTimer::timeout, this, slot);
    };
    setupTimer(m_cacheSave, 5000, true, [this] {
        m_library.save(cacheFile());
    });
    setupTimer(m_rescanDebounce, 3000, true, &Profile::startScan);
    setupTimer(m_periodicRescan, 10 * 60 * 1000, false, &Profile::startScan);
    // Notices an unmounted or remounted folder.
    setupTimer(m_folderCheck, 30000, false, [this] {
        QStringList missing;
        for (const QString &folder : folders()) {
            if (!QFileInfo(folder).isDir()) {
                missing.append(folder);
            }
        }
        if (missing != m_missing) {
            startScan();
        }
    });
    m_folderCheck.start();

    connect(&m_library, &Library::orderReset, this, &Profile::libraryChanged);
    connect(&m_library, &Library::merged, this, &Profile::libraryChanged);
    connect(&m_library, &Library::modified, &m_cacheSave, qOverload<>(&QTimer::start));
    connect(&m_indexer, &LibraryIndexer::runningChanged, this, &Profile::scanningChanged);
    connect(&m_indexer, &LibraryIndexer::firstFileFound, this, [this](const QString &path) {
        if (m_library.count() == 0) {
            m_controller->showProvisional(path);
        }
    });
    connect(&m_indexer, &LibraryIndexer::finished, this, &Profile::onScanFinished);
    for (auto signal : {&KDirWatch::dirty, &KDirWatch::created, &KDirWatch::deleted}) {
        connect(&m_watch, signal, &m_rescanDebounce, qOverload<>(&QTimer::start));
    }
    connect(m_controller, &PlaybackController::stateChanged, this, &Profile::stateChanged);

    connect(m_settings, &ProfileSettings::foldersChanged, this, &Profile::startLibrary);
    connect(m_settings, &ProfileSettings::includeSubfoldersChanged, this, &Profile::startScan);
    connect(m_settings, &ProfileSettings::videoEnabledChanged, this, &Profile::startScan);

    const KConfigGroup saved = state.group(stateGroup(id));
    m_seed = saved.readEntry("shuffleSeed", quint64(0));
    if (m_seed == 0) {
        m_seed = QRandomGenerator::global()->generate64() | 1;
    }
    startLibrary();
    if (saved.exists() && saved.readEntry("folders", QStringList()) == folders()) {
        m_controller->setPaused(saved.readEntry("paused", false));
        m_controller->setFilterRegex(saved.readEntry("filterRegex", false));
        m_controller->setFilter(saved.readEntry("filter", QString()));
        m_controller->restore(saved.readEntry("currentPath", QString()), saved.readEntry("elapsedMs", qint64(0)), saved.readEntry("row", 0));
    } else {
        m_controller->first();
    }
}

Profile::~Profile()
{
    m_indexer.cancel();
    m_library.save(cacheFile());
    m_settings->save();
}

QStringList Profile::folders() const
{
    QStringList list;
    for (const QString &folder : m_settings->folders()) {
        if (!folder.isEmpty()) {
            list.append(QDir::cleanPath(folder));
        }
    }
    return list;
}

void Profile::addFolder(const QUrl &url)
{
    const QString folder = QDir::cleanPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
    QStringList list = m_settings->folders();
    if (!folder.isEmpty() && !list.contains(folder)) {
        list.append(folder);
        m_settings->setFolders(list);
    }
}

void Profile::removeFolder(int index)
{
    QStringList list = m_settings->folders();
    if (index >= 0 && index < list.size()) {
        list.removeAt(index);
        m_settings->setFolders(list);
    }
}

void Profile::startLibrary()
{
    m_indexer.cancel();
    const QStringList list = folders();
    if (list.isEmpty()) {
        m_library.clear({});
        updateWatches({});
        setFolderError(QStringLiteral("Add a folder of images to start the slideshow."));
        return;
    }
    if (m_library.folders() != list) {
        // At startup the cache has the index; later, a rescan merges the change in,
        // keeping the order and position.
        if (m_library.count() > 0 || !m_library.load(cacheFile(), list)) {
            m_library.setFolders(list);
        }
    }
    startScan();
}

void Profile::startScan()
{
    if (!m_library.folders().isEmpty()) {
        m_indexer.start(m_library.folders(), m_settings->includeSubfolders(), m_settings->videoEnabled());
    }
}

void Profile::onScanFinished(const LibraryIndexer::Result &result)
{
    const bool returned = result.missing.size() < m_missing.size();
    m_missing = result.missing;
    if (returned) {
        m_controller->retryMedia();
    }
    if (!result.ok) {
        setFolderError(QStringLiteral("No folder can be read: %1").arg(result.missing.join(QStringLiteral(", "))));
        return; // keep the index until the folders return
    }
    setFolderError(m_missing.isEmpty() ? QString() : QStringLiteral("Missing or unreadable: %1").arg(m_missing.join(QStringLiteral(", "))));
    if (m_library.count() == 0) {
        m_library.reset(m_library.folders(), result.files, m_seed);
    } else {
        m_library.merge(result.files, m_controller->currentPath());
    }
    updateWatches(result.dirs);
}

void Profile::updateWatches(const QStringList &dirs)
{
    m_periodicRescan.stop();
    QStringList wanted;
    if (Settings::watchChanges()) {
        QFile limitFile(QStringLiteral("/proc/sys/fs/inotify/max_user_watches"));
        const int limit = limitFile.open(QIODevice::ReadOnly) ? limitFile.readAll().trimmed().toInt() : 8192;
        // Leave half of the user's inotify watches to other programs.
        if (dirs.size() > limit / 2) {
            m_periodicRescan.start();
        } else {
            wanted = dirs;
        }
    }
    if (wanted == m_watchedDirs) {
        return;
    }
    for (const QString &dir : std::as_const(m_watchedDirs)) {
        m_watch.removeDir(dir);
    }
    for (const QString &dir : std::as_const(wanted)) {
        m_watch.addDir(dir);
    }
    m_watchedDirs = wanted;
}

void Profile::setFolderError(const QString &error)
{
    if (m_folderError != error) {
        m_folderError = error;
        Q_EMIT folderErrorChanged();
    }
}

void Profile::rescan()
{
    m_library.clearBadMarks();
    startScan();
}

void Profile::reshuffle()
{
    m_seed = QRandomGenerator::global()->generate64() | 1;
    m_library.reshuffle(m_seed);
    Q_EMIT stateChanged();
}

void Profile::writeState(KSharedConfig &state) const
{
    KConfigGroup group = state.group(stateGroup(m_id));
    group.writeEntry("folders", folders());
    group.writeEntry("shuffleSeed", m_seed);
    group.writeEntry("currentPath", m_controller->currentPath());
    group.writeEntry("elapsedMs", m_controller->elapsedMs());
    group.writeEntry("row", m_controller->currentRow());
    group.writeEntry("paused", m_controller->paused());
    group.writeEntry("filter", m_controller->filter());
    group.writeEntry("filterRegex", m_controller->filterRegex());
}

QString Profile::cacheFile() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/index-") + m_id + QStringLiteral(".bin");
}

QString Profile::exportSettings(const QUrl &file) const
{
    return SettingsIO::write(file, {{QStringLiteral("profiles"), QJsonObject{{m_id, SettingsIO::toJson(m_settings)}}}});
}

QString Profile::importSettings(const QUrl &file)
{
    QJsonObject root;
    if (const QString error = SettingsIO::read(file, root); !error.isEmpty()) {
        return error;
    }
    const QJsonObject profiles = root.value(QLatin1String("profiles")).toObject();
    QJsonValue values = profiles.value(m_id);
    if (values.isUndefined()) {
        values = profiles.size() == 1 ? *profiles.begin() : profiles.value(DefaultId);
    }
    if (!values.isObject()) {
        return QStringLiteral("%1 holds no settings for this tab.").arg(file.toLocalFile());
    }
    return SettingsIO::ignoredMessage(applySettings(m_settings, values.toObject()));
}

int Profile::applySettings(ProfileSettings *settings, const QJsonObject &values)
{
    return SettingsIO::apply(settings,
                             values,
                             {
                                 {QStringLiteral("scaleMode"), ProfileSettings::EnumScaleMode::COUNT},
                                 {QStringLiteral("playlistCorner"), ProfileSettings::EnumPlaylistCorner::COUNT},
                                 {QStringLiteral("playlistTheme"), ProfileSettings::EnumPlaylistTheme::COUNT},
                             });
}
