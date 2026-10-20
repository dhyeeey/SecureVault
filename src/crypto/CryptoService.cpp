#include "CryptoService.h"

#include <QIODevice>
#include <cstring>

#include <sodium.h>

namespace {
void fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
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
    if (in->read(&version, 1) != 1 || static_cast<quint8>(version) != kBlobVersion) {
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
    if (!ok)
        fail(error, QStringLiteral("AEAD roundtrip mismatch"));
    return ok;
}
