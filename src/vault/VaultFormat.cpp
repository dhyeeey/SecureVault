#include "VaultFormat.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <cstring>

namespace {
QByteArray leU16(quint16 v)
{
    QByteArray b(2, 0);
    b[0] = static_cast<char>(v & 0xff);
    b[1] = static_cast<char>((v >> 8) & 0xff);
    return b;
}

QByteArray leU32(quint32 v)
{
    QByteArray b(4, 0);
    b[0] = static_cast<char>(v & 0xff);
    b[1] = static_cast<char>((v >> 8) & 0xff);
    b[2] = static_cast<char>((v >> 16) & 0xff);
    b[3] = static_cast<char>((v >> 24) & 0xff);
    return b;
}

QByteArray leU64(quint64 v)
{
    QByteArray b(8, 0);
    for (int i = 0; i < 8; ++i)
        b[i] = static_cast<char>((v >> (8 * i)) & 0xff);
    return b;
}

quint16 readU16(const QByteArray &b, int off)
{
    return static_cast<quint8>(b[off]) | (static_cast<quint16>(static_cast<quint8>(b[off + 1])) << 8);
}

quint32 readU32(const QByteArray &b, int off)
{
    return static_cast<quint32>(static_cast<quint8>(b[off]))
        | (static_cast<quint32>(static_cast<quint8>(b[off + 1])) << 8)
        | (static_cast<quint32>(static_cast<quint8>(b[off + 2])) << 16)
        | (static_cast<quint32>(static_cast<quint8>(b[off + 3])) << 24);
}

quint64 readU64(const QByteArray &b, int off)
{
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v |= static_cast<quint64>(static_cast<quint8>(b[off + i])) << (8 * i);
    return v;
}

qint32 readI32(const QByteArray &b, int off)
{
    return static_cast<qint32>(readU32(b, off));
}
} // namespace

QByteArray VaultFormat::wrapAad()
{
    return QByteArrayLiteral("SVLTWRAP1");
}

QByteArray VaultFormat::indexAad()
{
    return QByteArrayLiteral("SVLTINDEX");
}

QString VaultFormat::headerPath(const QString &vaultDir)
{
    return QDir(vaultDir).filePath(QStringLiteral("vault.header"));
}

QString VaultFormat::indexPath(const QString &vaultDir)
{
    return QDir(vaultDir).filePath(QStringLiteral("index.enc"));
}

QString VaultFormat::blobsDir(const QString &vaultDir)
{
    return QDir(vaultDir).filePath(QStringLiteral("blobs"));
}

bool VaultFormat::isVaultDir(const QString &vaultDir)
{
    QFile f(headerPath(vaultDir));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray magic = f.read(4);
    return magic == QByteArrayLiteral("SVLT");
}

bool VaultFormat::readHeader(const QString &vaultDir, VaultHeader *header, QString *error)
{
    if (!header) {
        if (error)
            *error = QStringLiteral("Missing header output");
        return false;
    }
    QFile f(headerPath(vaultDir));
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Cannot open vault.header");
        return false;
    }
    const QByteArray b = f.read(kHeaderBytes);
    if (b.size() != kHeaderBytes) {
        if (error)
            *error = QStringLiteral("Truncated vault.header");
        return false;
    }
    if (b.left(4) != QByteArrayLiteral("SVLT")) {
        if (error)
            *error = QStringLiteral("Not a SecureVault folder");
        return false;
    }
    header->version = readU16(b, 4);
    if (header->version != kVersion) {
        if (error)
            *error = QStringLiteral("Unsupported vault version");
        return false;
    }
    header->kdf.opslimit = readU32(b, 8);
    header->kdf.memlimit = readU64(b, 12);
    header->kdf.alg = readI32(b, 20);
    header->salt = b.mid(24, CryptoService::kSaltBytes);
    header->wrappedMaster = b.mid(40);
    return true;
}

bool VaultFormat::writeHeader(const QString &vaultDir, const VaultHeader &header, QString *error)
{
    if (header.salt.size() != CryptoService::kSaltBytes
        || header.wrappedMaster.size() != CryptoService::kAeadNonceBytes + CryptoService::kKeyBytes
                                            + CryptoService::kAeadTagBytes) {
        if (error)
            *error = QStringLiteral("Invalid header fields");
        return false;
    }

    QByteArray b;
    b.reserve(kHeaderBytes);
    b.append("SVLT", 4);
    b.append(leU16(header.version));
    b.append(leU16(0));
    b.append(leU32(header.kdf.opslimit));
    b.append(leU64(header.kdf.memlimit));
    b.append(leU32(static_cast<quint32>(header.kdf.alg)));
    b.append(header.salt);
    b.append(header.wrappedMaster);
    if (b.size() != kHeaderBytes) {
        if (error)
            *error = QStringLiteral("Internal header size mismatch");
        return false;
    }

    QDir().mkpath(vaultDir);
    QSaveFile f(headerPath(vaultDir));
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Cannot write vault.header");
        return false;
    }
    if (f.write(b) != b.size()) {
        if (error)
            *error = QStringLiteral("Failed writing vault.header");
        return false;
    }
    if (!f.commit()) {
        if (error)
            *error = QStringLiteral("Failed to commit vault.header");
        return false;
    }
    return true;
}

bool VaultFormat::wrapMasterKey(const unsigned char *wrappingKey32,
                                const unsigned char *masterKey32,
                                QByteArray *wrappedOut,
                                QString *error)
{
    QByteArray plain(reinterpret_cast<const char *>(masterKey32), CryptoService::kKeyBytes);
    *wrappedOut = CryptoService::aeadEncrypt(wrappingKey32, plain, wrapAad(), error);
    CryptoService::wipe(plain);
    return wrappedOut->size() == CryptoService::kAeadNonceBytes + CryptoService::kKeyBytes
            + CryptoService::kAeadTagBytes;
}

bool VaultFormat::unwrapMasterKey(const unsigned char *wrappingKey32,
                                  const QByteArray &wrapped,
                                  unsigned char *masterKey32,
                                  QString *error)
{
    QByteArray plain = CryptoService::aeadDecrypt(wrappingKey32, wrapped, wrapAad(), error);
    if (plain.size() != CryptoService::kKeyBytes) {
        CryptoService::wipe(plain);
        if (error && error->isEmpty())
            *error = QStringLiteral("Wrong password or damaged header");
        else if (error)
            *error = QStringLiteral("Wrong password or damaged header");
        return false;
    }
    memcpy(masterKey32, plain.constData(), CryptoService::kKeyBytes);
    CryptoService::wipe(plain);
    return true;
}
