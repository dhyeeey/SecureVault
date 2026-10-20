#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <functional>

class QIODevice;

class MasterKey {
public:
    MasterKey();
    ~MasterKey();
    MasterKey(const MasterKey &) = delete;
    MasterKey &operator=(const MasterKey &) = delete;
    MasterKey(MasterKey &&other) noexcept;
    MasterKey &operator=(MasterKey &&other) noexcept;

    bool valid() const;
    unsigned char *data();
    const unsigned char *data() const;
    void reset();
    bool ensure();

private:
    unsigned char *m_ptr = nullptr;
};

class CryptoService {
public:
    static constexpr int kSaltBytes = 16;
    static constexpr int kKeyBytes = 32;
    static constexpr int kAeadNonceBytes = 24;
    static constexpr int kAeadTagBytes = 16;
    static constexpr int kStreamHeaderBytes = 24;
    static constexpr int kChunkSize = 64 * 1024;
    static constexpr quint8 kBlobVersion = 1;
    static constexpr quint8 kIndexVersion = 1;

    struct KdfParams {
        quint32 opslimit = 2;
        quint64 memlimit = 64ull * 1024ull * 1024ull;
        qint32 alg = 2; // crypto_pwhash_ALG_ARGON2ID13
    };

    static bool init();
    static KdfParams interactiveKdf();
    static QByteArray randomBytes(size_t n);

    static bool deriveWrappingKey(const QString &password,
                                  const QByteArray &salt,
                                  const KdfParams &params,
                                  unsigned char *outKey32,
                                  QString *error);

    // Returns nonce || ciphertext+tag.
    static QByteArray aeadEncrypt(const unsigned char *key32,
                                  const QByteArray &plaintext,
                                  const QByteArray &aad,
                                  QString *error);

    static QByteArray aeadDecrypt(const unsigned char *key32,
                                  const QByteArray &nonceAndCipher,
                                  const QByteArray &aad,
                                  QString *error);

    static void deriveFileKey(const unsigned char *masterKey32,
                              const QByteArray &blobIdLatin1,
                              unsigned char *outFileKey32);

    static bool streamEncrypt(QIODevice *in,
                              QIODevice *out,
                              const unsigned char *fileKey32,
                              QString *error,
                              const std::function<void(qint64, qint64)> &progress = {});

    static bool streamDecrypt(QIODevice *in,
                              QIODevice *out,
                              const unsigned char *fileKey32,
                              QString *error,
                              const std::function<void(qint64, qint64)> &progress = {});

    static void wipe(unsigned char *p, size_t n);
    static void wipe(QByteArray &b);
    static void wipe(QString &s);

    static bool selfTest(QString *error);
};
