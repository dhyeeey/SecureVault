#pragma once

#include "VaultEntry.h"

#include <QAbstractListModel>
#include <QVector>

class FileIndexModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        IsDirRole,
        SizeRole,
        DateRole,
        DisplaySizeRole,
        DisplayDateRole
    };

    explicit FileIndexModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QVector<VaultEntry> entries);
    const VaultEntry *at(int row) const;

    static QString formatBytes(qint64 bytes);

private:
    QVector<VaultEntry> m_entries;
};
