#include "CryptoService.h"

#include <QBuffer>
#include <QIODevice>
#include <QVariantMap>
#include <cstring>

#include <sodium.h>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#endif

namespace {
void fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

void gcmNonceWithCounter(unsigned char out[12], const unsigned char base[12], quint32 counter)
{
    memcpy(out, base, 12);
    out[8] = static_cast<unsigned char>((counter >> 24) & 0xff);
    out[9] = static_cast<unsigned char>((counter >> 16) & 0xff);
    out[10] = static_cast<unsigned char>((counter >> 8) & 0xff);
    out[11] = static_cast<unsigned char>(counter & 0xff);
}

#ifdef Q_OS_WIN
struct AesGcmCtx {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    QByteArray keyObj;

    ~AesGcmCtx()
    {
        if (key)
            BCryptDestroyKey(key);
        if (alg)
            BCryptCloseAlgorithmProvider(alg, 0);
    }

    bool init(const unsigned char *keyBytes, int keyLen, QString *error)
    {
        NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
        if (!BCRYPT_SUCCESS(st)) {
            fail(error, QStringLiteral("AES provider unavailable"));
            return false;
        }
        st = BCryptSetProperty(alg,
                               BCRYPT_CHAINING_MODE,
                               reinterpret_cast<PUCHAR>(const_cast<wchar_t *>(BCRYPT_CHAIN_MODE_GCM)),
                               sizeof(BCRYPT_CHAIN_MODE_GCM),
                               0);
        if (!BCRYPT_SUCCESS(st)) {
            fail(error, QStringLiteral("AES-GCM mode unavailable"));
            return false;
        }
        DWORD objLen = 0;
        ULONG dummy = 0;
        st = BCryptGetProperty(alg,
                               BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&objLen),
                               sizeof(objLen),
                               &dummy,
                               0);
        if (!BCRYPT_SUCCESS(st) || objLen == 0) {
            fail(error, QStringLiteral("AES key object size failed"));
            return false;
        }
        keyObj.resize(static_cast<int>(objLen));
        st = BCryptGenerateSymmetricKey(alg,
                                        &key,
                                        reinterpret_cast<PUCHAR>(keyObj.data()),
                                        objLen,
                                        const_cast<PUCHAR>(keyBytes),
                                        static_cast<ULONG>(keyLen),
                                        0);
        if (!BCRYPT_SUCCESS(st)) {
            fail(error, QStringLiteral("AES key import failed"));
            return false;
        }
        return true;
    }

    bool crypt(bool encrypt,
               const unsigned char *nonce,
               const unsigned char *in,
               int inLen,
               unsigned char *out,
               unsigned char *tag,
               QString *error)
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        BCRYPT_INIT_AUTH_MODE_INFO(info);
        info.pbNonce = const_cast<PUCHAR>(nonce);
        info.cbNonce = 12;
        info.pbTag = tag;
        info.cbTag = 16;

        ULONG result = 0;
        NTSTATUS st;
        if (encrypt) {
            st = BCryptEncrypt(key,
                               const_cast<PUCHAR>(in),
                               static_cast<ULONG>(inLen),
                               &info,
                               nullptr,
                               0,
                               out,
                               static_cast<ULONG>(inLen),
                               &result,
                               0);
        } else {
            st = BCryptDecrypt(key,
                               const_cast<PUCHAR>(in),
                               static_cast<ULONG>(inLen),
                               &info,
                               nullptr,
                               0,
                               out,
                               static_cast<ULONG>(inLen),
                               &result,
                               0);
        }
        if (!BCRYPT_SUCCESS(st) || static_cast<int>(result) != inLen) {
            fail(error, encrypt ? QStringLiteral("AES-GCM encrypt failed")
                                : QStringLiteral("AES-GCM decrypt failed (corrupt blob or wrong key)"));
            return false;
        }
        return true;
    }
};
#else
const EVP_CIPHER *aesGcmCipher(FileCipher cipher)
{
    switch (cipher) {
    case FileCipher::Aes128Gcm:
        return EVP_aes_128_gcm();
    case FileCipher::Aes192Gcm:
        return EVP_aes_192_gcm();
    case FileCipher::Aes256Gcm:
        return EVP_aes_256_gcm();
    case FileCipher::XChaCha20Poly1305:
        break;
    }
    return nullptr;
}

bool opensslGcm(bool encrypt,
                FileCipher cipher,
                const unsigned char *keyBytes,
                const unsigned char *nonce,
                const unsigned char *in,
                int inLen,
                unsigned char *out,
                unsigned char *tag,
                QString *error)
{
    const EVP_CIPHER *evp = aesGcmCipher(cipher);
    if (!evp) {
        fail(error, QStringLiteral("Unknown AES-GCM cipher"));
        return false;
    }
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fail(error, QStringLiteral("AES-GCM context failed"));
        return false;
    }
    bool ok = false;
    do {
        if (EVP_CipherInit_ex(ctx, evp, nullptr, nullptr, nullptr, encrypt ? 1 : 0) != 1)
            break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1)
            break;
        if (EVP_CipherInit_ex(ctx, nullptr, nullptr, keyBytes, nonce, encrypt ? 1 : 0) != 1)
            break;
        if (!encrypt && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, tag) != 1)
            break;
        int len = 0;
        if (inLen > 0 && EVP_CipherUpdate(ctx, out, &len, in, inLen) != 1)
            break;
        int len2 = 0;
        if (EVP_CipherFinal_ex(ctx, out + len, &len2) != 1)
            break;
        if (encrypt && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1)
            break;
        ok = true;
    } while (false);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
        fail(error, encrypt ? QStringLiteral("AES-GCM encrypt failed")
                            : QStringLiteral("AES-GCM decrypt failed (corrupt blob or wrong key)"));
    return ok;
}
#endif

bool streamEncryptAesGcm(QIODevice *in,
                         QIODevice *out,
                         const unsigned char *fileKey32,
                         FileCipher cipher,
                         QString *error,
                         const std::function<void(qint64, qint64)> &progress)
{
    const int keyLen = CryptoService::aesKeyBytes(cipher);
#ifdef Q_OS_WIN
    AesGcmCtx ctx;
    if (!ctx.init(fileKey32, keyLen, error))
        return false;
#else
    Q_UNUSED(keyLen);
#endif

    const qint64 total = in->size() >= 0 ? in->size() : 0;
    const quint8 version = CryptoService::kBlobVersion2;
    const quint8 cipherId = static_cast<quint8>(cipher);
    if (out->write(reinterpret_cast<const char *>(&version), 1) != 1
        || out->write(reinterpret_cast<const char *>(&cipherId), 1) != 1) {
        fail(error, QStringLiteral("Failed to write blob header"));
        return false;
    }

    unsigned char baseNonce[CryptoService::kGcmNonceBytes];
    randombytes_buf(baseNonce, sizeof(baseNonce));
    if (out->write(reinterpret_cast<const char *>(baseNonce), sizeof(baseNonce))
        != static_cast<qint64>(sizeof(baseNonce))) {
        fail(error, QStringLiteral("Failed to write AES nonce"));
        return false;
    }

    QByteArray plain;
    plain.resize(CryptoService::kChunkSize);
    QByteArray cipherBuf;
    cipherBuf.resize(CryptoService::kChunkSize);
    unsigned char tag[CryptoService::kGcmTagBytes];
    unsigned char nonce[CryptoService::kGcmNonceBytes];

    qint64 done = 0;
    quint32 counter = 0;
    bool ok = true;
    bool wrote = false;
    while (ok) {
        const qint64 n = in->read(plain.data(), plain.size());
        if (n < 0) {
            fail(error, QStringLiteral("Read failed during encrypt"));
            ok = false;
            break;
        }
        gcmNonceWithCounter(nonce, baseNonce, counter++);
#ifdef Q_OS_WIN
        if (!ctx.crypt(true,
                       nonce,
                       reinterpret_cast<const unsigned char *>(plain.constData()),
                       static_cast<int>(n),
                       reinterpret_cast<unsigned char *>(cipherBuf.data()),
                       tag,
                       error)) {
            ok = false;
            break;
        }
#else
        if (!opensslGcm(true,
                        cipher,
                        fileKey32,
                        nonce,
                        reinterpret_cast<const unsigned char *>(plain.constData()),
                        static_cast<int>(n),
                        reinterpret_cast<unsigned char *>(cipherBuf.data()),
                        tag,
                        error)) {
            ok = false;
            break;
        }
#endif
        if (n > 0
            && out->write(cipherBuf.constData(), n) != n) {
            fail(error, QStringLiteral("Write failed during encrypt"));
            ok = false;
            break;
        }
        if (out->write(reinterpret_cast<const char *>(tag), sizeof(tag))
            != static_cast<qint64>(sizeof(tag))) {
            fail(error, QStringLiteral("Write failed during encrypt"));
            ok = false;
            break;
        }
        wrote = true;
        done += n;
        if (progress)
            progress(done, total);
        if (in->atEnd())
            break;
    }
    if (ok && !wrote) {
        fail(error, QStringLiteral("AES-GCM produced no output"));
        ok = false;
    }

    CryptoService::wipe(plain);
    CryptoService::wipe(cipherBuf);
    sodium_memzero(baseNonce, sizeof(baseNonce));
    sodium_memzero(nonce, sizeof(nonce));
    sodium_memzero(tag, sizeof(tag));
    return ok;
}

bool streamDecryptAesGcm(QIODevice *in,
                         QIODevice *out,
                         const unsigned char *fileKey32,
                         FileCipher cipher,
                         QString *error,
                         const std::function<void(qint64, qint64)> &progress)
{
    const int keyLen = CryptoService::aesKeyBytes(cipher);
#ifdef Q_OS_WIN
    AesGcmCtx ctx;
    if (!ctx.init(fileKey32, keyLen, error))
        return false;
#else
    Q_UNUSED(keyLen);
#endif

    const qint64 total = in->size() >= 0 ? in->size() : 0;
    unsigned char baseNonce[CryptoService::kGcmNonceBytes];
    if (in->read(reinterpret_cast<char *>(baseNonce), sizeof(baseNonce))
        != static_cast<qint64>(sizeof(baseNonce))) {
        fail(error, QStringLiteral("Failed to read AES nonce"));
        return false;
    }

    const int cipherChunk = CryptoService::kChunkSize + CryptoService::kGcmTagBytes;
    QByteArray cipherBuf;
    cipherBuf.resize(cipherChunk);
    QByteArray plain;
    plain.resize(CryptoService::kChunkSize);
    unsigned char nonce[CryptoService::kGcmNonceBytes];

    qint64 done = 2 + static_cast<qint64>(sizeof(baseNonce));
    quint32 counter = 0;
    bool ok = true;
    bool sawChunk = false;
    while (ok) {
        qint64 left = -1;
        if (in->size() >= 0)
            left = in->size() - in->pos();
        if (left == 0)
            break;
        const qint64 want = (left >= 0 && left < cipherBuf.size()) ? left : cipherBuf.size();
        if (want < CryptoService::kGcmTagBytes) {
            fail(error, QStringLiteral("Truncated AES-GCM blob"));
            ok = false;
            break;
        }
        qint64 n = 0;
        while (n < want) {
            const qint64 got = in->read(cipherBuf.data() + n, want - n);
            if (got <= 0)
                break;
            n += got;
        }
        if (n != want) {
            fail(error, QStringLiteral("Unexpected end of blob"));
            ok = false;
            break;
        }
        const int plainLen = static_cast<int>(n) - CryptoService::kGcmTagBytes;
        unsigned char tag[CryptoService::kGcmTagBytes];
        memcpy(tag, cipherBuf.constData() + plainLen, sizeof(tag));
        gcmNonceWithCounter(nonce, baseNonce, counter++);
#ifdef Q_OS_WIN
        if (!ctx.crypt(false,
                       nonce,
                       reinterpret_cast<const unsigned char *>(cipherBuf.constData()),
                       plainLen,
                       reinterpret_cast<unsigned char *>(plain.data()),
                       tag,
                       error)) {
            ok = false;
            break;
        }
#else
        if (!opensslGcm(false,
                        cipher,
                        fileKey32,
                        nonce,
                        reinterpret_cast<const unsigned char *>(cipherBuf.constData()),
                        plainLen,
                        reinterpret_cast<unsigned char *>(plain.data()),
                        tag,
                        error)) {
            ok = false;
            break;
        }
#endif
        if (plainLen > 0
            && out->write(plain.constData(), plainLen) != plainLen) {
            fail(error, QStringLiteral("Write failed during decrypt"));
            ok = false;
            break;
        }
        sawChunk = true;
        done += n;
        if (progress)
            progress(done, total);
        sodium_memzero(tag, sizeof(tag));
    }
    if (ok && !sawChunk) {
        fail(error, QStringLiteral("Empty AES-GCM blob"));
        ok = false;
    }

    CryptoService::wipe(plain);
    CryptoService::wipe(cipherBuf);
    sodium_memzero(baseNonce, sizeof(baseNonce));
    sodium_memzero(nonce, sizeof(nonce));
    return ok;
}
} // namespace

MasterKey::MasterKey() = default;

MasterKey::~MasterKey()
{
    reset();
}

MasterKey::MasterKey(MasterKey &&other) noexcept
    : m_ptr(other.m_ptr)
{
    other.m_ptr = nullptr;
}

MasterKey &MasterKey::operator=(MasterKey &&other) noexcept
{
    if (this != &other) {
        reset();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
    }
    return *this;
}

bool MasterKey::valid() const
{
    return m_ptr != nullptr;
}

unsigned char *MasterKey::data()
{
    return m_ptr;
}

const unsigned char *MasterKey::data() const
{
    return m_ptr;
}

void MasterKey::reset()
{
    if (m_ptr) {
        sodium_memzero(m_ptr, CryptoService::kKeyBytes);
        sodium_free(m_ptr);
        m_ptr = nullptr;
    }
}

bool MasterKey::ensure()
{
    if (m_ptr)
        return true;
    m_ptr = static_cast<unsigned char *>(sodium_malloc(CryptoService::kKeyBytes));
    if (!m_ptr)
        return false;
    sodium_memzero(m_ptr, CryptoService::kKeyBytes);
    return true;
}

bool CryptoService::init()
{
    return sodium_init() >= 0;
}

CryptoService::KdfParams CryptoService::interactiveKdf()
{
    KdfParams p;
    p.opslimit = static_cast<quint32>(crypto_pwhash_OPSLIMIT_INTERACTIVE);
    p.memlimit = static_cast<quint64>(crypto_pwhash_MEMLIMIT_INTERACTIVE);
    p.alg = crypto_pwhash_ALG_ARGON2ID13;
    return p;
}

bool CryptoService::isFileCipher(quint16 id)
{
    return id <= static_cast<quint16>(FileCipher::Aes256Gcm);
}

FileCipher CryptoService::fileCipherFromId(int id)
{
    if (id < 0 || !isFileCipher(static_cast<quint16>(id)))
        return FileCipher::XChaCha20Poly1305;
    return static_cast<FileCipher>(id);
}

int CryptoService::aesKeyBytes(FileCipher cipher)
{
    switch (cipher) {
    case FileCipher::Aes128Gcm:
        return 16;
    case FileCipher::Aes192Gcm:
        return 24;
    case FileCipher::Aes256Gcm:
        return 32;
    case FileCipher::XChaCha20Poly1305:
        break;
    }
    return kKeyBytes;
}

QString CryptoService::fileCipherName(FileCipher cipher)
{
    switch (cipher) {
    case FileCipher::Aes128Gcm:
        return QStringLiteral("AES-128-GCM");
    case FileCipher::Aes192Gcm:
        return QStringLiteral("AES-192-GCM");
    case FileCipher::Aes256Gcm:
        return QStringLiteral("AES-256-GCM");
    case FileCipher::XChaCha20Poly1305:
        break;
    }
    return QStringLiteral("XChaCha20-Poly1305");
}

QVariantList CryptoService::cipherModel()
{
    QVariantList list;
    const auto add = [&](FileCipher id, const QString &text) {
        QVariantMap row;
        row.insert(QStringLiteral("id"), static_cast<int>(id));
        row.insert(QStringLiteral("text"), text);
        list.push_back(row);
    };
    add(FileCipher::XChaCha20Poly1305, QStringLiteral("XChaCha20-Poly1305 (recommended)"));
    add(FileCipher::Aes128Gcm, QStringLiteral("AES-128-GCM"));
    add(FileCipher::Aes192Gcm, QStringLiteral("AES-192-GCM"));
    add(FileCipher::Aes256Gcm, QStringLiteral("AES-256-GCM"));
    return list;
}

QByteArray CryptoService::randomBytes(size_t n)
{
    QByteArray out;
    out.resize(static_cast<int>(n));
    randombytes_buf(out.data(), n);
    return out;
}

bool CryptoService::deriveWrappingKey(const QString &password,
                                      const QByteArray &salt,
                                      const KdfParams &params,
                                      unsigned char *outKey32,
                                      QString *error)
{
    if (!outKey32) {
        fail(error, QStringLiteral("Missing wrapping-key buffer"));
        return false;
    }
    if (salt.size() != kSaltBytes) {
        fail(error, QStringLiteral("Invalid salt"));
        return false;
    }

    QByteArray pw = password.toUtf8();
    const int rc = crypto_pwhash(outKey32,
                                 kKeyBytes,
                                 pw.constData(),
                                 static_cast<unsigned long long>(pw.size()),
                                 reinterpret_cast<const unsigned char *>(salt.constData()),
                                 params.opslimit,
                                 static_cast<size_t>(params.memlimit),
                                 params.alg);
    wipe(pw);
    if (rc != 0) {
        fail(error, QStringLiteral("Password hashing failed"));
        return false;
    }
    return true;
}

QByteArray CryptoService::aeadEncrypt(const unsigned char *key32,
                                      const QByteArray &plaintext,
                                      const QByteArray &aad,
                                      QString *error)
{
    QByteArray nonce = randomBytes(kAeadNonceBytes);
    QByteArray out;
    out.resize(kAeadNonceBytes + plaintext.size() + kAeadTagBytes);

    memcpy(out.data(), nonce.constData(), kAeadNonceBytes);

    unsigned long long clen = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            reinterpret_cast<unsigned char *>(out.data()) + kAeadNonceBytes,
            &clen,
            reinterpret_cast<const unsigned char *>(plaintext.constData()),
            static_cast<unsigned long long>(plaintext.size()),
            reinterpret_cast<const unsigned char *>(aad.constData()),
            static_cast<unsigned long long>(aad.size()),
            nullptr,
            reinterpret_cast<const unsigned char *>(nonce.constData()),
            key32)
        != 0) {
        wipe(nonce);
        fail(error, QStringLiteral("Encryption failed"));
        return {};
    }
    wipe(nonce);
    out.resize(kAeadNonceBytes + static_cast<int>(clen));
    return out;
}

QByteArray CryptoService::aeadDecrypt(const unsigned char *key32,
                                      const QByteArray &nonceAndCipher,
                                      const QByteArray &aad,
                                      QString *error)
{
    if (nonceAndCipher.size() < kAeadNonceBytes + kAeadTagBytes) {
        fail(error, QStringLiteral("Ciphertext too short"));
        return {};
    }

    const auto *nonce = reinterpret_cast<const unsigned char *>(nonceAndCipher.constData());
    const auto *cipher = nonce + kAeadNonceBytes;
    const unsigned long long clen =
        static_cast<unsigned long long>(nonceAndCipher.size() - kAeadNonceBytes);

    QByteArray out;
    out.resize(nonceAndCipher.size() - kAeadNonceBytes - kAeadTagBytes);

    unsigned long long mlen = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(out.data()),
            &mlen,
            nullptr,
            cipher,
            clen,
            reinterpret_cast<const unsigned char *>(aad.constData()),
            static_cast<unsigned long long>(aad.size()),
            nonce,
            key32)
        != 0) {
        fail(error, QStringLiteral("Decryption failed"));
        wipe(out);
        return {};
    }
    out.resize(static_cast<int>(mlen));
    return out;
}

void CryptoService::deriveFileKey(const unsigned char *masterKey32,
                                  const QByteArray &blobIdLatin1,
                                  unsigned char *outFileKey32)
{
    crypto_generichash(outFileKey32,
                       kKeyBytes,
                       reinterpret_cast<const unsigned char *>(blobIdLatin1.constData()),
                       static_cast<size_t>(blobIdLatin1.size()),
                       masterKey32,
                       kKeyBytes);
}

bool CryptoService::streamEncrypt(QIODevice *in,
                                  QIODevice *out,
                                  const unsigned char *fileKey32,
                                  QString *error,
                                  const std::function<void(qint64, qint64)> &progress)
{
    if (!in || !out) {
        fail(error, QStringLiteral("Invalid stream"));
        return false;
    }

    const qint64 total = in->size() >= 0 ? in->size() : 0;
    const quint8 version = kBlobVersion;
    if (out->write(reinterpret_cast<const char *>(&version), 1) != 1) {
        fail(error, QStringLiteral("Failed to write blob version"));
        return false;
    }

    crypto_secretstream_xchacha20poly1305_state state;
    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    if (crypto_secretstream_xchacha20poly1305_init_push(&state, header, fileKey32) != 0) {
        fail(error, QStringLiteral("Stream init failed"));
        return false;
    }
    if (out->write(reinterpret_cast<const char *>(header), sizeof(header))
        != static_cast<qint64>(sizeof(header))) {
        sodium_memzero(&state, sizeof(state));
        fail(error, QStringLiteral("Failed to write stream header"));
        return false;
    }

    QByteArray plain;
    plain.resize(kChunkSize);
    QByteArray cipher;
    cipher.resize(kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES);

    qint64 done = 0;
    bool ok = true;
    while (ok) {
        const qint64 n = in->read(plain.data(), plain.size());
        if (n < 0) {
            fail(error, QStringLiteral("Read failed during encrypt"));
            ok = false;
            break;
        }
        const unsigned char tag = (in->atEnd() || n == 0)
            ? crypto_secretstream_xchacha20poly1305_TAG_FINAL
            : crypto_secretstream_xchacha20poly1305_TAG_MESSAGE;

        unsigned long long clen = 0;
        if (crypto_secretstream_xchacha20poly1305_push(
                &state,
                reinterpret_cast<unsigned char *>(cipher.data()),
                &clen,
                reinterpret_cast<const unsigned char *>(plain.constData()),
                static_cast<unsigned long long>(n),
                nullptr,
                0,
                tag)
            != 0) {
            fail(error, QStringLiteral("Chunk encrypt failed"));
            ok = false;
            break;
        }
        if (out->write(cipher.constData(), static_cast<qint64>(clen)) != static_cast<qint64>(clen)) {
            fail(error, QStringLiteral("Write failed during encrypt"));
            ok = false;
            break;
        }
        done += n;
        if (progress)
            progress(done, total);
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL)
            break;
    }

    wipe(plain);
    wipe(cipher);
    sodium_memzero(&state, sizeof(state));
    sodium_memzero(header, sizeof(header));
    return ok;
}

bool CryptoService::streamEncrypt(QIODevice *in,
                                  QIODevice *out,
                                  const unsigned char *fileKey32,
                                  FileCipher cipher,
                                  QString *error,
                                  const std::function<void(qint64, qint64)> &progress)
{
    if (cipher == FileCipher::XChaCha20Poly1305)
        return streamEncrypt(in, out, fileKey32, error, progress);
    if (!isFileCipher(static_cast<quint16>(cipher))) {
        fail(error, QStringLiteral("Unknown file cipher"));
        return false;
    }
    return streamEncryptAesGcm(in, out, fileKey32, cipher, error, progress);
}

bool CryptoService::streamDecrypt(QIODevice *in,
                                  QIODevice *out,
                                  const unsigned char *fileKey32,
                                  QString *error,
                                  const std::function<void(qint64, qint64)> &progress)
{
    if (!in || !out) {
        fail(error, QStringLiteral("Invalid stream"));
        return false;
    }

    const qint64 total = in->size() >= 0 ? in->size() : 0;
    char version = 0;
    if (in->read(&version, 1) != 1) {
        fail(error, QStringLiteral("Unknown blob version"));
        return false;
    }
    const auto ver = static_cast<quint8>(version);
    if (ver == kBlobVersion2) {
        char cipherByte = 0;
        if (in->read(&cipherByte, 1) != 1) {
            fail(error, QStringLiteral("Unknown blob cipher"));
            return false;
        }
        const auto cipherId = static_cast<quint8>(cipherByte);
        if (!isFileCipher(cipherId)) {
            fail(error, QStringLiteral("Unknown blob cipher"));
            return false;
        }
        const FileCipher cipher = static_cast<FileCipher>(cipherId);
        if (cipher == FileCipher::XChaCha20Poly1305) {
            // v2 XChaCha uses the same secretstream header as v1.
        } else {
            return streamDecryptAesGcm(in, out, fileKey32, cipher, error, progress);
        }
    } else if (ver != kBlobVersion) {
        fail(error, QStringLiteral("Unknown blob version"));
        return false;
    }

    unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    if (in->read(reinterpret_cast<char *>(header), sizeof(header))
        != static_cast<qint64>(sizeof(header))) {
        fail(error, QStringLiteral("Failed to read stream header"));
        return false;
    }

    crypto_secretstream_xchacha20poly1305_state state;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&state, header, fileKey32) != 0) {
        sodium_memzero(header, sizeof(header));
        fail(error, QStringLiteral("Stream header rejected"));
        return false;
    }

    const int cipherChunk = kChunkSize + crypto_secretstream_xchacha20poly1305_ABYTES;
    QByteArray cipher;
    cipher.resize(cipherChunk);
    QByteArray plain;
    plain.resize(kChunkSize);

    qint64 done = 1 + static_cast<qint64>(sizeof(header));
    bool ok = true;
    while (ok) {
        qint64 left = -1;
        if (in->size() >= 0)
            left = in->size() - in->pos();
        const qint64 want = (left >= 0 && left < cipher.size()) ? left : cipher.size();
        if (want <= 0) {
            fail(error, QStringLiteral("Unexpected end of blob"));
            ok = false;
            break;
        }
        qint64 n = 0;
        while (n < want) {
            const qint64 got = in->read(cipher.data() + n, want - n);
            if (got <= 0)
                break;
            n += got;
        }
        if (n != want) {
            fail(error, QStringLiteral("Unexpected end of blob"));
            ok = false;
            break;
        }

        unsigned long long mlen = 0;
        unsigned char tag = 0;
        if (crypto_secretstream_xchacha20poly1305_pull(
                &state,
                reinterpret_cast<unsigned char *>(plain.data()),
                &mlen,
                &tag,
                reinterpret_cast<const unsigned char *>(cipher.constData()),
                static_cast<unsigned long long>(n),
                nullptr,
                0)
            != 0) {
            fail(error, QStringLiteral("Chunk decrypt failed (corrupt blob or wrong key)"));
            ok = false;
            break;
        }
        if (mlen > 0 && out->write(plain.constData(), static_cast<qint64>(mlen))
                            != static_cast<qint64>(mlen)) {
            fail(error, QStringLiteral("Write failed during decrypt"));
            ok = false;
            break;
        }
        done += n;
        if (progress)
            progress(done, total);
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL)
            break;
    }

    wipe(plain);
    wipe(cipher);
    sodium_memzero(&state, sizeof(state));
    sodium_memzero(header, sizeof(header));
    return ok;
}

void CryptoService::wipe(unsigned char *p, size_t n)
{
    if (p && n)
        sodium_memzero(p, n);
}

void CryptoService::wipe(QByteArray &b)
{
    if (!b.isEmpty())
        sodium_memzero(b.data(), static_cast<size_t>(b.size()));
    b.clear();
}

void CryptoService::wipe(QString &s)
{
    if (!s.isEmpty()) {
        QChar *d = s.data();
        sodium_memzero(d, static_cast<size_t>(s.size()) * sizeof(QChar));
    }
    s.clear();
}

bool CryptoService::selfTest(QString *error)
{
    const QByteArray key = randomBytes(kKeyBytes);
    const QByteArray aad = QByteArrayLiteral("svlt-selftest");
    const QByteArray msg = QByteArrayLiteral("secure-vault-roundtrip");
    const QByteArray ct = aeadEncrypt(reinterpret_cast<const unsigned char *>(key.constData()),
                                      msg,
                                      aad,
                                      error);
    if (ct.isEmpty())
        return false;
    QByteArray pt = aeadDecrypt(reinterpret_cast<const unsigned char *>(key.constData()),
                                ct,
                                aad,
                                error);
    const bool ok = pt == msg;
    wipe(pt);
    if (!ok) {
        fail(error, QStringLiteral("AEAD roundtrip mismatch"));
        return false;
    }

    const FileCipher aesCiphers[] = {FileCipher::Aes128Gcm, FileCipher::Aes192Gcm,
                                     FileCipher::Aes256Gcm};
    for (FileCipher cipher : aesCiphers) {
        QByteArray src = QByteArrayLiteral("aes-gcm-chunk-roundtrip-payload-0123456789");
        QBuffer in(&src);
        if (!in.open(QIODevice::ReadOnly)) {
            fail(error, QStringLiteral("AES self-test input failed"));
            return false;
        }
        QByteArray packed;
        QBuffer packedBuf(&packed);
        if (!packedBuf.open(QIODevice::WriteOnly)) {
            fail(error, QStringLiteral("AES self-test output failed"));
            return false;
        }
        unsigned char fileKey[kKeyBytes];
        randombytes_buf(fileKey, sizeof(fileKey));
        if (!streamEncrypt(&in, &packedBuf, fileKey, cipher, error)) {
            wipe(fileKey, sizeof(fileKey));
            return false;
        }
        packedBuf.close();
        QBuffer packedIn(&packed);
        if (!packedIn.open(QIODevice::ReadOnly)) {
            wipe(fileKey, sizeof(fileKey));
            fail(error, QStringLiteral("AES self-test packed input failed"));
            return false;
        }
        QByteArray round;
        QBuffer roundBuf(&round);
        if (!roundBuf.open(QIODevice::WriteOnly)) {
            wipe(fileKey, sizeof(fileKey));
            fail(error, QStringLiteral("AES self-test round output failed"));
            return false;
        }
        const bool decOk = streamDecrypt(&packedIn, &roundBuf, fileKey, error);
        wipe(fileKey, sizeof(fileKey));
        if (!decOk)
            return false;
        if (round != src) {
            fail(error, QStringLiteral("AES-GCM roundtrip mismatch"));
            return false;
        }
    }
    return true;
}
