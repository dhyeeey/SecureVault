#include "ContainerBlobStore.h"

namespace {
bool notImplemented(QString *error)
{
    if (error)
        *error = QStringLiteral("Single-file .vault containers are not implemented in v1");
    return false;
}
} // namespace

bool ContainerBlobStore::openStore(const QString &location, QString *error)
{
    Q_UNUSED(location);
    return notImplemented(error);
}

void ContainerBlobStore::closeStore() {}

bool ContainerBlobStore::writeBlob(const QString &blobId,
                                   QIODevice *plaintext,
                                   const unsigned char *fileKey32,
                                   QString *error,
                                   const std::function<void(qint64, qint64)> &progress)
{
    Q_UNUSED(blobId);
    Q_UNUSED(plaintext);
    Q_UNUSED(fileKey32);
    Q_UNUSED(progress);
    return notImplemented(error);
}

bool ContainerBlobStore::readBlob(const QString &blobId,
                                  QIODevice *plaintext,
                                  const unsigned char *fileKey32,
                                  QString *error,
                                  const std::function<void(qint64, qint64)> &progress)
{
    Q_UNUSED(blobId);
    Q_UNUSED(plaintext);
    Q_UNUSED(fileKey32);
    Q_UNUSED(progress);
    return notImplemented(error);
}

bool ContainerBlobStore::removeBlob(const QString &blobId, QString *error)
{
    Q_UNUSED(blobId);
    return notImplemented(error);
}
