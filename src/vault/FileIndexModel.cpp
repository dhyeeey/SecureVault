#include "FileIndexModel.h"

#include <QDateTime>

FileIndexModel::FileIndexModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileIndexModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant FileIndexModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const VaultEntry &e = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return e.id;
    case NameRole:
        return e.name;
    case IsDirRole:
        return e.isDir;
    case SizeRole:
        return e.size;
    case DateRole:
        return e.modifiedMs;
    case DisplaySizeRole:
        return e.isDir ? QString() : formatBytes(e.size);
    case DisplayDateRole:
        return QDateTime::fromMSecsSinceEpoch(e.modifiedMs).toString(QStringLiteral("dd-MM-yyyy HH:mm"));
    default:
        return {};
    }
}

QHash<int, QByteArray> FileIndexModel::roleNames() const
{
    return {
        {IdRole, "entryId"},
        {NameRole, "name"},
        {IsDirRole, "isDir"},
        {SizeRole, "size"},
        {DateRole, "modifiedMs"},
        {DisplaySizeRole, "displaySize"},
        {DisplayDateRole, "displayDate"},
    };
}

void FileIndexModel::setEntries(QVector<VaultEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

QString FileIndexModel::formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QString::number(bytes) + QStringLiteral(" B");
    static const char *const units[] = {"KB", "MB", "GB", "TB"};
    double value = bytes / 1024.0;
    int unit = 0;
    while (value >= 1024.0 && unit < 3) {
        value /= 1024.0;
        ++unit;
    }
    const int decimals = value < 10 ? 2 : (value < 100 ? 1 : 0);
    return QString::number(value, 'f', decimals) + QLatin1Char(' ') + QLatin1String(units[unit]);
}

const VaultEntry *FileIndexModel::at(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return nullptr;
    return &m_entries.at(row);
}
