#pragma once

#include <QString>
#include <QtGlobal>

struct VaultEntry {
    QString id;
    QString parentId;
    QString name;
    bool isDir = false;
    QString blobId;
    qint64 size = 0;
    qint64 modifiedMs = 0;
};
