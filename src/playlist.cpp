#include "playlist.h"
#include "core/library.h"

#include <QRegularExpression>

Playlist::Playlist(Library *library, QObject *parent)
    : QAbstractListModel(parent)
    , m_library(library)
{
    m_filtered.setSourceModel(this);
    m_filtered.setFilterRole(SearchRole);
    m_filtered.setFilterCaseSensitivity(Qt::CaseInsensitive);
    rebuild();
}

int Playlist::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_count;
}

int Playlist::count() const
{
    return m_count;
}

QVariant Playlist::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_count) {
        return {};
    }
    const Library::Entry &e = m_library->at(index.row());
    const qsizetype slash = e.path.lastIndexOf(QLatin1Char('/'));
    switch (role) {
    case NumberRole:
        return index.row() + 1;
    case Qt::DisplayRole:
    case NameRole:
        return e.path.mid(slash + 1);
    case FolderRole:
        return e.path.left(slash);
    case CurrentRole:
        return index.row() == m_currentRow;
    case BadRole:
        return e.bad;
    case SearchRole:
        return e.path.mid(folderPrefix(e.path));
    case RelativePathRole: {
        const qsizetype root = folderPrefix(e.path);
        // The folder's own name is the segment before its trailing slash.
        const qsizetype nameStart = root > 1 ? e.path.lastIndexOf(QLatin1Char('/'), root - 2) + 1 : root;
        return e.path.mid(nameStart);
    }
    case PathRole:
        return e.path;
    }
    return {};
}

qsizetype Playlist::folderPrefix(const QString &path) const
{
    for (const QString &folder : m_library->folders()) {
        const qsizetype root = folder.endsWith(QLatin1Char('/')) ? folder.size() : folder.size() + 1;
        if (path.startsWith(folder) && path.size() > root && path.at(root - 1) == QLatin1Char('/')) {
            return root;
        }
    }
    return path.lastIndexOf(QLatin1Char('/')) + 1;
}

QHash<int, QByteArray> Playlist::roleNames() const
{
    return {
        {NumberRole, "number"},
        {NameRole, "name"},
        {FolderRole, "folder"},
        {CurrentRole, "current"},
        {BadRole, "bad"},
        {RelativePathRole, "relativePath"},
        {PathRole, "path"},
    };
}

QString Playlist::pathAt(int row) const
{
    return m_library->at(row).path;
}

int Playlist::rowOf(const QString &path) const
{
    return m_library->indexOf(path);
}

void Playlist::rebuild()
{
    beginResetModel();
    m_count = m_library->count();
    m_currentRow = -1;
    endResetModel();
    Q_EMIT countChanged();
}

void Playlist::setCurrentRow(int row)
{
    if (row == m_currentRow) {
        return;
    }
    const int old = m_currentRow;
    m_currentRow = row;
    for (int r : {old, row}) {
        if (r >= 0 && r < m_count) {
            Q_EMIT dataChanged(index(r), index(r), {CurrentRole});
        }
    }
}

void Playlist::entryChanged(int row)
{
    if (row >= 0 && row < m_count) {
        Q_EMIT dataChanged(index(row), index(row), {BadRole});
    }
}

bool Playlist::setFilter(const QString &filter, bool regex)
{
    if (!regex) {
        m_filtered.setFilterFixedString(filter);
        return true;
    }
    const QRegularExpression re(filter, QRegularExpression::CaseInsensitiveOption);
    m_filtered.setFilterRegularExpression(re);
    return re.isValid();
}

bool Playlist::matches(int row) const
{
    return m_filtered.rowCount() == 0 || m_filtered.mapFromSource(index(row)).isValid();
}

int Playlist::filteredRow(int sourceRow) const
{
    if (sourceRow < 0 || sourceRow >= m_count) {
        return -1;
    }
    return m_filtered.mapFromSource(index(sourceRow)).row();
}
