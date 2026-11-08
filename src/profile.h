#pragma once

#include "core/library.h"
#include "core/libraryindexer.h"
#include "playbackcontroller.h"
#include "profilesettings.h"

#include <KDirWatch>
#include <KSharedConfig>
#include <QTimer>
#include <QtQmlIntegration>

// One configuration of what a screen shows: its settings, its pooled library of
// folders and the playback through it. Either one profile serves every screen,
// or each screen has its own.
class Profile : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by App")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(ProfileSettings *settings READ settings CONSTANT)
    Q_PROPERTY(PlaybackController *controller READ controller CONSTANT)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(int libraryCount READ libraryCount NOTIFY libraryChanged)
    Q_PROPERTY(QString folderError READ folderError NOTIFY folderErrorChanged)

public:
    static const QString DefaultId;

    // A screen profile without settings of its own starts as a copy of the default
    // one. Playback resumes from state when the folders are unchanged.
    Profile(const QString &id, const QString &title, const KSharedConfig &state, QObject *parent = nullptr);
    ~Profile() override;

    QString id() const
    {
        return m_id;
    }
    QString title() const
    {
        return m_title;
    }
    ProfileSettings *settings() const
    {
        return m_settings;
    }
    PlaybackController *controller() const
    {
        return m_controller;
    }
    bool scanning() const
    {
        return m_indexer.isRunning();
    }
    int libraryCount() const
    {
        return m_library.count();
    }
    QString folderError() const
    {
        return m_folderError;
    }

    Q_INVOKABLE void addFolder(const QUrl &url);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void reshuffle();
    // Writes this profile's settings to a settings file, or applies the profile a file
    // holds to this one: the profile with this id, else its only profile, else its
    // "All screens" one. Return an error or warning, empty on success.
    Q_INVOKABLE QString exportSettings(const QUrl &file) const;
    Q_INVOKABLE QString importSettings(const QUrl &file);
    // Applies the values of one profile in a settings file; see SettingsIO::apply().
    static int applySettings(ProfileSettings *settings, const QJsonObject &values);
    void startScan();
    void writeState(KSharedConfig &state) const;

Q_SIGNALS:
    void scanningChanged();
    void libraryChanged();
    void folderErrorChanged();
    void stateChanged();

private:
    QStringList folders() const;
    void startLibrary();
    void onScanFinished(const LibraryIndexer::Result &result);
    void updateWatches(const QStringList &dirs);
    void setFolderError(const QString &error);
    QString cacheFile() const;

    const QString m_id;
    const QString m_title;
    ProfileSettings *m_settings;
    Library m_library;
    LibraryIndexer m_indexer;
    PlaybackController *m_controller;
    KDirWatch m_watch;
    QStringList m_watchedDirs;
    QStringList m_missing;
    QString m_folderError;
    quint64 m_seed = 0;
    QTimer m_rescanDebounce;
    QTimer m_periodicRescan;
    QTimer m_folderCheck;
    QTimer m_cacheSave;
};
