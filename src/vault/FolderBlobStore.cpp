#include "FolderBlobStore.h"

#include "VaultFormat.h"
#include "crypto/CryptoService.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

bool FolderBlobStore::openStore(const QString &location, QString *error)
{
    m_root = location;
    QDir dir(VaultFormat::blobsDir(m_root));
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error)
            *error = QStringLiteral("Cannot create blobs directory");
        return false;
    }
    return true;
}

void FolderBlobStore::closeStore()
{
    m_root.clear();
}

QString FolderBlobStore::blobPath(const QString &blobId) const
{
    const QString id = blobId.toLower();
    const QString a = id.left(2);
    const QString b = id.mid(2, 2);
    QDir dir(QDir(VaultFormat::blobsDir(m_root)).filePath(a));
    dir.mkpath(b);
    return QDir(dir.filePath(b)).filePath(id);
}

bool FolderBlobStore::writeBlob(const QString &blobId,
                                QIODevice *plaintext,
                                const unsigned char *fileKey32,
                                QString *error,
                                const std::function<void(qint64, qint64)> &progress)
{
    const QString path = blobPath(blobId);

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot create blob file");
        return false;
    }
    if (!CryptoService::streamEncrypt(plaintext, &out, fileKey32, error, progress)) {
        out.cancelWriting();
        return false;
    }
    if (!out.commit()) {
        if (error)
            *error = QStringLiteral("Failed to commit blob");
        return false;
    }
    return true;
}

bool FolderBlobStore::readBlob(const QString &blobId,
                               QIODevice *plaintext,
                               const unsigned char *fileKey32,
                               QString *error,
                               const std::function<void(qint64, qint64)> &progress)
{
    QFile in(blobPath(blobId));
    if (!in.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Missing blob");
        return false;
    }
    return CryptoService::streamDecrypt(&in, plaintext, fileKey32, error, progress);
}

bool FolderBlobStore::removeBlob(const QString &blobId, QString *error)
{
    const QString path = blobPath(blobId);
    if (!QFile::exists(path))
        return true;
    if (!QFile::remove(path)) {
        if (error)
            *error = QStringLiteral("Failed to remove blob");
        return false;
    }
    return true;
}
