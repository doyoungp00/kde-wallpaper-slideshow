#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QtQmlIntegration>

class Library;

// The library's playback order as a list model, with the search filter that
// playback also follows.
class Playlist : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by PlaybackController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QSortFilterProxyModel *filtered READ filtered CONSTANT)

public:
    enum Roles {
        NumberRole = Qt::UserRole + 1,
        NameRole,
        FolderRole,
        CurrentRole,
        BadRole,
        // Path below the library folder it was found in, which the search matches.
        SearchRole,
        // That library folder's name followed by SearchRole, e.g. "Wallpapers/sub/a.png".
        RelativePathRole,
        PathRole,
    };

    explicit Playlist(Library *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QSortFilterProxyModel *filtered()
    {
        return &m_filtered;
    }
    QString pathAt(int row) const;
    int rowOf(const QString &path) const;

    void rebuild();
    void setCurrentRow(int row);
    void entryChanged(int row);

    // Case-insensitive substring of the path below the library folder, or regular
    // expression when regex is set; empty shows everything. Returns false for an
    // invalid expression, which then matches nothing.
    bool setFilter(const QString &filter, bool regex);
    // Whether row passes the filter. A filter that matches nothing is ignored, so
    // playback never runs dry.
    bool matches(int row) const;

    // Row of sourceRow in the filtered view, or -1 when it is filtered out.
    Q_INVOKABLE int filteredRow(int sourceRow) const;

Q_SIGNALS:
    void countChanged();

private:
    // Length of the library folder prefix of path, including its trailing slash.
    qsizetype folderPrefix(const QString &path) const;

    Library *m_library;
    int m_count = 0;
    int m_currentRow = -1;
    QSortFilterProxyModel m_filtered;
};
