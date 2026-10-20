#pragma once

#include <QString>
#include <functional>

class QIODevice;

// Storage backend for encrypted file blobs. v1 uses FolderBlobStore.
// ContainerBlobStore is the later single-file (.vault) backend and is not
// wired into VaultSession yet.
class BlobStore {
public:
    virtual ~BlobStore() = default;

    virtual QString kind() const = 0;
    virtual bool openStore(const QString &location, QString *error) = 0;
    virtual void closeStore() = 0;

    virtual bool writeBlob(const QString &blobId,
                           QIODevice *plaintext,
                           const unsigned char *fileKey32,
                           QString *error,
                           const std::function<void(qint64, qint64)> &progress = {}) = 0;

    virtual bool readBlob(const QString &blobId,
                          QIODevice *plaintext,
                          const unsigned char *fileKey32,
                          QString *error,
                          const std::function<void(qint64, qint64)> &progress = {}) = 0;

    virtual bool removeBlob(const QString &blobId, QString *error) = 0;
};
