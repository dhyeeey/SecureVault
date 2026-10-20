#pragma once

#include "BlobStore.h"

// Phase 2 storage backend: a single `.vault` container file.
//
// Planned layout (not implemented in v1):
//   [0, 512)     container header (magic, version, KDF, salt, wrapped master,
//                extent-table offset/size)
//   [index]      AEAD-encrypted JSON index (same payload as folder vault)
//   [extents]    concatenated secretstream blobs + extent table
//
// VaultSession, FileIndexModel, and CryptoService stay unchanged; only this
// class needs a real implementation later.
class ContainerBlobStore final : public BlobStore {
public:
    QString kind() const override { return QStringLiteral("container"); }

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
};
