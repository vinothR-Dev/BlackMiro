#pragma once

#include <QByteArray>
#include <QString>

// Small, dependency-free crypto toolkit (works on desktop, Android and WebAssembly).
// AES is implemented here (encryption direction only: CTR and CBC-encrypt need no
// inverse cipher); hashing/HMAC come from Qt.
namespace Crypto {

// AES block encryption with a 16/24/32-byte key.
class Aes
{
public:
    explicit Aes(const QByteArray &key);
    void encryptBlock(const unsigned char in[16], unsigned char out[16]) const;
    bool isValid() const { return m_rounds > 0; }

private:
    unsigned char m_roundKeys[240] = {};
    int m_rounds = 0;
};

QByteArray randomBytes(int count);
QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength);
QByteArray aesCtr(const QByteArray &key, const QByteArray &iv16, const QByteArray &data);      // encrypt == decrypt
QByteArray aesCbcEncrypt(const QByteArray &key, const QByteArray &iv16, const QByteArray &data); // PKCS#7 padding
QByteArray rc4(const QByteArray &key, const QByteArray &data);

// Password-protected container used for private `.board` files:
//   "BMENC001" | iterations (u32 BE) | salt (16) | iv (16) | ciphertext | HMAC-SHA256 tag (32)
// Key = PBKDF2-HMAC-SHA256(password, salt) -> 64 bytes: AES-256-CTR key + HMAC key
// (encrypt-then-MAC; a wrong password or any tampering fails the tag check).
// Live sessions: key64 = 32-byte AES key + 32-byte HMAC key (derive once with pbkdf2Sha256).
// seal() output: iv (16) | AES-256-CTR ciphertext | HMAC-SHA256 tag (32).
QByteArray seal(const QByteArray &key64, const QByteArray &plain);
bool open(const QByteArray &key64, const QByteArray &sealed, QByteArray *plain);

bool isEncryptedContainer(const QByteArray &data);
QByteArray encryptContainer(const QByteArray &plain, const QString &password);
bool decryptContainer(const QByteArray &data, const QString &password, QByteArray *plain);

} // namespace Crypto
