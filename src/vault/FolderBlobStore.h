#pragma once

#include "BlobStore.h"

class FolderBlobStore final : public BlobStore {
public:
    QString kind() const override { return QStringLiteral("folder"); }

    bool openStore(const QString &location, QString *error) override;
    void closeStore() override;

    bool writeBlob(const QString &blobId,
                   QIODevice *plaintext,
                   const unsigned char *fileKey32,
                   QString *error,
                   const std::function<void(qint64, qint64)> &progress = {}) override;

    bool readBlob(const QString &blobId,
                  QIODevice *plaintext,
                  const unsigned char *fileKey32,
                  QString *error,
                  const std::function<void(qint64, qint64)> &progress = {}) override;

    bool removeBlob(const QString &blobId, QString *error) override;

    QString root() const { return m_root; }

private:
    QString blobPath(const QString &blobId) const;

    QString m_root;
};
