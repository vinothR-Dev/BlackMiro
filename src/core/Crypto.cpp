#include "Crypto.h"

#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QtEndian>

#include <cstring>

namespace Crypto {

namespace {

const unsigned char kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

inline unsigned char xtime(unsigned char x)
{
    return static_cast<unsigned char>((x << 1) ^ ((x & 0x80) ? 0x1b : 0x00));
}

constexpr char kMagic[] = "BMENC001";
constexpr int kMagicLen = 8;
constexpr int kSaltLen = 16;
constexpr int kIvLen = 16;
constexpr int kTagLen = 32;
constexpr int kIterations = 150000;
constexpr int kHeaderLen = kMagicLen + 4 + kSaltLen + kIvLen;

} // namespace

Aes::Aes(const QByteArray &key)
{
    const int nk = int(key.size()) / 4;
    if (key.size() != 16 && key.size() != 24 && key.size() != 32)
        return;
    m_rounds = nk + 6;
    const int total = 4 * (m_rounds + 1); // words
    std::memcpy(m_roundKeys, key.constData(), size_t(key.size()));
    unsigned char rcon = 0x01;
    for (int i = nk; i < total; ++i) {
        unsigned char t[4];
        std::memcpy(t, m_roundKeys + 4 * (i - 1), 4);
        if (i % nk == 0) {
            const unsigned char first = t[0];
            t[0] = kSbox[t[1]] ^ rcon;
            t[1] = kSbox[t[2]];
            t[2] = kSbox[t[3]];
            t[3] = kSbox[first];
            rcon = xtime(rcon);
        } else if (nk > 6 && i % nk == 4) {
            for (unsigned char &b : t)
                b = kSbox[b];
        }
        for (int j = 0; j < 4; ++j)
            m_roundKeys[4 * i + j] = m_roundKeys[4 * (i - nk) + j] ^ t[j];
    }
}

void Aes::encryptBlock(const unsigned char in[16], unsigned char out[16]) const
{
    unsigned char s[16];
    for (int i = 0; i < 16; ++i)
        s[i] = in[i] ^ m_roundKeys[i];

    for (int round = 1; round <= m_rounds; ++round) {
        // SubBytes
        for (unsigned char &b : s)
            b = kSbox[b];
        // ShiftRows (state is column-major: s[col*4 + row])
        unsigned char t;
        t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
        t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
        t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
        // MixColumns (not in the final round)
        if (round != m_rounds) {
            for (int c = 0; c < 4; ++c) {
                unsigned char *col = s + 4 * c;
                const unsigned char a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                const unsigned char all = a0 ^ a1 ^ a2 ^ a3;
                col[0] ^= all ^ xtime(a0 ^ a1);
                col[1] ^= all ^ xtime(a1 ^ a2);
                col[2] ^= all ^ xtime(a2 ^ a3);
                col[3] ^= all ^ xtime(a3 ^ a0);
            }
        }
        // AddRoundKey
        const unsigned char *rk = m_roundKeys + 16 * round;
        for (int i = 0; i < 16; ++i)
            s[i] ^= rk[i];
    }
    std::memcpy(out, s, 16);
}

QByteArray randomBytes(int count)
{
    QByteArray out(count, Qt::Uninitialized);
    QRandomGenerator *rng = QRandomGenerator::system();
    for (int i = 0; i < count; ++i)
        out[i] = char(rng->generate() & 0xff);
    return out;
}

QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength)
{
    QByteArray out;
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, password);
    for (quint32 block = 1; out.size() < keyLength; ++block) {
        QByteArray counter(4, Qt::Uninitialized);
        qToBigEndian(block, counter.data());
        mac.reset();
        mac.addData(salt);
        mac.addData(counter);
        QByteArray u = mac.result();
        QByteArray t = u;
        for (int i = 1; i < iterations; ++i) {
            mac.reset();
            mac.addData(u);
            u = mac.result();
            for (int j = 0; j < t.size(); ++j)
                t[j] = char(t[j] ^ u[j]);
        }
        out += t;
    }
    return out.left(keyLength);
}

QByteArray aesCtr(const QByteArray &key, const QByteArray &iv16, const QByteArray &data)
{
    const Aes aes(key);
    QByteArray out(data.size(), Qt::Uninitialized);
    unsigned char counter[16];
    std::memcpy(counter, iv16.constData(), 16);
    unsigned char ks[16];
    for (qsizetype off = 0; off < data.size(); off += 16) {
        aes.encryptBlock(counter, ks);
        const qsizetype n = qMin<qsizetype>(16, data.size() - off);
        for (qsizetype i = 0; i < n; ++i)
            out[off + i] = char(data[off + i] ^ ks[i]);
        for (int i = 15; i >= 0; --i)   // big-endian increment
            if (++counter[i] != 0)
                break;
    }
    return out;
}

QByteArray aesCbcEncrypt(const QByteArray &key, const QByteArray &iv16, const QByteArray &data)
{
    const Aes aes(key);
    const int pad = 16 - int(data.size() % 16);
    QByteArray in = data;
    in.append(QByteArray(pad, char(pad)));
    QByteArray out(in.size(), Qt::Uninitialized);
    unsigned char prev[16];
    std::memcpy(prev, iv16.constData(), 16);
    for (qsizetype off = 0; off < in.size(); off += 16) {
        unsigned char block[16];
        for (int i = 0; i < 16; ++i)
            block[i] = static_cast<unsigned char>(in[off + i]) ^ prev[i];
        aes.encryptBlock(block, prev);
        std::memcpy(out.data() + off, prev, 16);
    }
    return out;
}

QByteArray rc4(const QByteArray &key, const QByteArray &data)
{
    unsigned char s[256];
    for (int i = 0; i < 256; ++i)
        s[i] = static_cast<unsigned char>(i);
    for (int i = 0, j = 0; i < 256; ++i) {
        j = (j + s[i] + static_cast<unsigned char>(key[i % key.size()])) & 0xff;
        std::swap(s[i], s[j]);
    }
    QByteArray out(data.size(), Qt::Uninitialized);
    for (qsizetype n = 0, i = 0, j = 0; n < data.size(); ++n) {
        i = (i + 1) & 0xff;
        j = (j + s[i]) & 0xff;
        std::swap(s[i], s[j]);
        out[n] = char(data[n] ^ s[(s[i] + s[j]) & 0xff]);
    }
    return out;
}

QByteArray seal(const QByteArray &key64, const QByteArray &plain)
{
    const QByteArray iv = randomBytes(16);
    QByteArray out = iv + aesCtr(key64.left(32), iv, plain);
    out += QMessageAuthenticationCode::hash(out, key64.mid(32), QCryptographicHash::Sha256);
    return out;
}

bool open(const QByteArray &key64, const QByteArray &sealed, QByteArray *plain)
{
    if (sealed.size() < 16 + 32 || key64.size() != 64)
        return false;
    const QByteArray body = sealed.left(sealed.size() - 32);
    const QByteArray tag = sealed.right(32);
    const QByteArray expected = QMessageAuthenticationCode::hash(body, key64.mid(32), QCryptographicHash::Sha256);
    unsigned char diff = 0;
    for (int i = 0; i < 32; ++i)
        diff |= static_cast<unsigned char>(tag[i] ^ expected[i]);
    if (diff != 0)
        return false;
    if (plain)
        *plain = aesCtr(key64.left(32), body.left(16), body.mid(16));
    return true;
}

bool isEncryptedContainer(const QByteArray &data)
{
    return data.size() >= kHeaderLen + kTagLen && data.startsWith(QByteArray(kMagic, kMagicLen));
}

QByteArray encryptContainer(const QByteArray &plain, const QString &password)
{
    const QByteArray salt = randomBytes(kSaltLen);
    const QByteArray iv = randomBytes(kIvLen);
    const QByteArray keys = pbkdf2Sha256(password.toUtf8(), salt, kIterations, 64);

    QByteArray out(kMagic, kMagicLen);
    QByteArray iter(4, Qt::Uninitialized);
    qToBigEndian(quint32(kIterations), iter.data());
    out += iter + salt + iv;
    out += aesCtr(keys.left(32), iv, plain);
    out += QMessageAuthenticationCode::hash(out, keys.mid(32), QCryptographicHash::Sha256);
    return out;
}

bool decryptContainer(const QByteArray &data, const QString &password, QByteArray *plain)
{
    if (!isEncryptedContainer(data))
        return false;
    const quint32 iterations = qFromBigEndian<quint32>(data.constData() + kMagicLen);
    if (iterations == 0 || iterations > 10'000'000)
        return false;
    const QByteArray salt = data.mid(kMagicLen + 4, kSaltLen);
    const QByteArray iv = data.mid(kMagicLen + 4 + kSaltLen, kIvLen);
    const QByteArray keys = pbkdf2Sha256(password.toUtf8(), salt, int(iterations), 64);

    const QByteArray body = data.left(data.size() - kTagLen);
    const QByteArray tag = data.right(kTagLen);
    const QByteArray expected = QMessageAuthenticationCode::hash(body, keys.mid(32), QCryptographicHash::Sha256);
    // Constant-time comparison.
    unsigned char diff = 0;
    for (int i = 0; i < kTagLen; ++i)
        diff |= static_cast<unsigned char>(tag[i] ^ expected[i]);
    if (diff != 0)
        return false;
    if (plain)
        *plain = aesCtr(keys.left(32), iv, body.mid(kHeaderLen));
    return true;
}

} // namespace Crypto
