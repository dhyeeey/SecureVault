#pragma once

#include "crypto/CryptoService.h"

#include <QString>

struct VaultHeader {
    quint16 version = 1;
    CryptoService::KdfParams kdf;
    QByteArray salt;
    QByteArray wrappedMaster;
};

class VaultFormat {
public:
    static constexpr quint16 kVersion = 1;
    static constexpr int kHeaderBytes = 112;
    static QByteArray wrapAad();
    static QByteArray indexAad();

    static QString headerPath(const QString &vaultDir);
    static QString indexPath(const QString &vaultDir);
    static QString blobsDir(const QString &vaultDir);

    static bool isVaultDir(const QString &vaultDir);
    static bool readHeader(const QString &vaultDir, VaultHeader *header, QString *error);
    static bool writeHeader(const QString &vaultDir, const VaultHeader &header, QString *error);

    static bool wrapMasterKey(const unsigned char *wrappingKey32,
                              const unsigned char *masterKey32,
                              QByteArray *wrappedOut,
                              QString *error);
    static bool unwrapMasterKey(const unsigned char *wrappingKey32,
                                const QByteArray &wrapped,
                                unsigned char *masterKey32,
                                QString *error);
};
