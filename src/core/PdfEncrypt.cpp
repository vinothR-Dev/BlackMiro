#include "PdfEncrypt.h"

#include "Crypto.h"

#include <QCryptographicHash>
#include <QHash>
#include <QList>
#include <QMap>
#include <QRegularExpression>
#include <QtEndian>

namespace PdfEncrypt {

namespace {

const unsigned char kPad[32] = {
    0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
    0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A,
};

constexpr qint32 kPermissions = -4; // everything allowed once the document is opened

QByteArray md5(const QByteArray &d)
{
    return QCryptographicHash::hash(d, QCryptographicHash::Md5);
}

QByteArray padPassword(const QByteArray &pw)
{
    QByteArray out = pw.left(32);
    out.append(reinterpret_cast<const char *>(kPad), 32 - out.size());
    return out;
}

QByteArray xorKey(const QByteArray &key, int i)
{
    QByteArray k = key;
    for (char &c : k)
        c = char(c ^ i);
    return k;
}

struct PdfObject
{
    int num = 0;
    int gen = 0;
    QByteArray head;          // everything between "obj" and "stream"/"endobj", strings already encrypted
    bool hasStream = false;
    QByteArray stream;        // raw (unencrypted) stream data
    int indirectLength = -1;  // object number holding /Length, if indirect
};

bool isDelimiterOrSpace(char c)
{
    return QByteArrayView("()<>[]{}/% \t\r\n\f").contains(c) || c == '\0';
}

// Decodes a literal string starting at text[pos] == '('. Returns bytes, advances pos past ')'.
QByteArray readLiteral(const QByteArray &t, qsizetype &pos)
{
    QByteArray out;
    int depth = 0;
    ++pos; // skip '('
    while (pos < t.size()) {
        const char c = t[pos++];
        if (c == '\\' && pos < t.size()) {
            const char e = t[pos++];
            switch (e) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case '\r': if (pos < t.size() && t[pos] == '\n') ++pos; break; // line continuation
            case '\n': break;
            default:
                if (e >= '0' && e <= '7') {
                    int v = e - '0';
                    for (int k = 0; k < 2 && pos < t.size() && t[pos] >= '0' && t[pos] <= '7'; ++k)
                        v = v * 8 + (t[pos++] - '0');
                    out += char(v & 0xff);
                } else {
                    out += e; // \( \) \\ and unknown escapes
                }
            }
        } else if (c == '(') {
            ++depth;
            out += c;
        } else if (c == ')') {
            if (depth == 0)
                break;
            --depth;
            out += c;
        } else {
            out += c;
        }
    }
    return out;
}

QByteArray readHex(const QByteArray &t, qsizetype &pos)
{
    ++pos; // skip '<'
    QByteArray hex;
    while (pos < t.size() && t[pos] != '>') {
        const char c = t[pos++];
        if (!QByteArrayView(" \t\r\n\f").contains(c))
            hex += c;
    }
    ++pos; // skip '>'
    if (hex.size() % 2)
        hex += '0';
    return QByteArray::fromHex(hex);
}

class Encryptor
{
public:
    QByteArray fileKey;

    QByteArray objectKey(int num, int gen) const
    {
        QByteArray k = fileKey;
        k += char(num & 0xff);
        k += char((num >> 8) & 0xff);
        k += char((num >> 16) & 0xff);
        k += char(gen & 0xff);
        k += char((gen >> 8) & 0xff);
        k += "sAlT";
        return md5(k).left(16);
    }

    QByteArray encrypt(int num, int gen, const QByteArray &data) const
    {
        const QByteArray iv = Crypto::randomBytes(16);
        return iv + Crypto::aesCbcEncrypt(objectKey(num, gen), iv, data);
    }
};

// Copies an object's head, replacing every string with an encrypted hex string.
// Stops at the `stream` or `endobj` keyword; returns position of that keyword.
qsizetype rewriteHead(const QByteArray &t, qsizetype pos, const Encryptor &enc, int num, int gen,
                      QByteArray &out, bool *hasStream)
{
    *hasStream = false;
    while (pos < t.size()) {
        const char c = t[pos];
        if (c == '(') {
            const QByteArray s = readLiteral(t, pos);
            out += '<' + enc.encrypt(num, gen, s).toHex() + '>';
        } else if (c == '<' && pos + 1 < t.size() && t[pos + 1] == '<') {
            out += "<<";
            pos += 2;
        } else if (c == '<') {
            const QByteArray s = readHex(t, pos);
            out += '<' + enc.encrypt(num, gen, s).toHex() + '>';
        } else if (c == '%') {
            while (pos < t.size() && t[pos] != '\n' && t[pos] != '\r')
                out += t[pos++];
        } else if ((c == 's' || c == 'e') && (pos == 0 || isDelimiterOrSpace(t[pos - 1]) || t[pos - 1] == '>')) {
            if (t.mid(pos, 6) == "stream" && (pos + 6 >= t.size() || isDelimiterOrSpace(t[pos + 6]))) {
                *hasStream = true;
                return pos;
            }
            if (t.mid(pos, 6) == "endobj")
                return pos;
            out += c;
            ++pos;
        } else {
            out += c;
            ++pos;
        }
    }
    return pos;
}

} // namespace

QByteArray encrypt(const QByteArray &pdf, const QString &password, QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error)
            *error = msg;
        return QByteArray();
    };

    for (const QChar ch : password)
        if (ch.unicode() > 0xff)
            return fail(QStringLiteral("PDF passwords can only use Latin letters, digits and symbols."));
    const QByteArray pw = password.toLatin1();

    // ---- Locate xref table and trailer
    const qsizetype sx = pdf.lastIndexOf("startxref");
    if (sx < 0)
        return fail(QStringLiteral("Not a PDF (no startxref)."));
    bool ok = false;
    const qsizetype xrefPos = pdf.mid(sx + 9, 24).trimmed().split('\n').first().trimmed().toLongLong(&ok);
    if (!ok || xrefPos <= 0 || xrefPos >= pdf.size() || pdf.mid(xrefPos, 4) != "xref")
        return fail(QStringLiteral("Unsupported PDF layout (xref)."));

    QMap<int, qsizetype> offsets;
    {
        qsizetype p = xrefPos + 4;
        const qsizetype trailerAt = pdf.indexOf("trailer", p);
        if (trailerAt < 0)
            return fail(QStringLiteral("Unsupported PDF layout (trailer)."));
        const QList<QByteArray> lines = pdf.mid(p, trailerAt - p).split('\n');
        int current = 0;
        for (QByteArray line : lines) {
            line = line.trimmed();
            if (line.isEmpty())
                continue;
            const QList<QByteArray> parts = line.split(' ');
            if (parts.size() == 2) {          // subsection header: first count
                current = parts[0].toInt();
            } else if (parts.size() >= 3) {
                if (parts[2] == "n")
                    offsets.insert(current, parts[0].toLongLong());
                ++current;
            }
        }
    }

    const qsizetype trailerAt = pdf.indexOf("trailer", xrefPos);
    const QByteArray trailer = pdf.mid(trailerAt, sx - trailerAt);
    static const QRegularExpression rootRe(QStringLiteral(R"(/Root\s+(\d+)\s+(\d+)\s+R)"));
    static const QRegularExpression infoRe(QStringLiteral(R"(/Info\s+(\d+)\s+(\d+)\s+R)"));
    static const QRegularExpression idRe(QStringLiteral(R"(/ID\s*\[\s*<([0-9A-Fa-f]*)>)"));
    const auto rootM = rootRe.match(QString::fromLatin1(trailer));
    if (!rootM.hasMatch())
        return fail(QStringLiteral("Unsupported PDF layout (no /Root)."));
    const auto infoM = infoRe.match(QString::fromLatin1(trailer));
    const auto idM = idRe.match(QString::fromLatin1(trailer));
    QByteArray idHex = idM.hasMatch() ? idM.captured(1).toLatin1() : QByteArray();
    if (idHex.isEmpty())
        idHex = Crypto::randomBytes(16).toHex();
    const QByteArray id0 = QByteArray::fromHex(idHex);

    // ---- Keys (Standard Security Handler, R4)
    QByteArray o;
    {
        QByteArray h = md5(padPassword(pw));       // owner password = user password
        for (int i = 0; i < 50; ++i)
            h = md5(h);
        const QByteArray key = h.left(16);
        o = Crypto::rc4(key, padPassword(pw));
        for (int i = 1; i <= 19; ++i)
            o = Crypto::rc4(xorKey(key, i), o);
    }
    Encryptor enc;
    {
        QByteArray p(4, Qt::Uninitialized);
        qToLittleEndian(kPermissions, p.data());
        QByteArray h = md5(padPassword(pw) + o + p + id0);
        for (int i = 0; i < 50; ++i)
            h = md5(h.left(16));
        enc.fileKey = h.left(16);
    }
    QByteArray u;
    {
        u = Crypto::rc4(enc.fileKey, md5(QByteArray(reinterpret_cast<const char *>(kPad), 32) + id0));
        for (int i = 1; i <= 19; ++i)
            u = Crypto::rc4(xorKey(enc.fileKey, i), u);
        u += QByteArray(16, '\0');
    }

    // ---- Pass 1: parse objects
    static const QRegularExpression headerRe(QStringLiteral(R"(^\s*(\d+)\s+(\d+)\s+obj)"));
    static const QRegularExpression lenIndRe(QStringLiteral(R"(/Length\s+(\d+)\s+(\d+)\s+R)"));
    static const QRegularExpression lenDirRe(QStringLiteral(R"(/Length\s+(\d+)(?!\s+\d+\s+R))"));

    QList<PdfObject> objects;
    QHash<int, int> indexByNum;
    for (auto it = offsets.cbegin(); it != offsets.cend(); ++it) {
        const qsizetype start = it.value();
        const QString probe = QString::fromLatin1(pdf.mid(start, 32));
        const auto hm = headerRe.match(probe);
        if (!hm.hasMatch())
            return fail(QStringLiteral("Unsupported PDF layout (object %1).").arg(it.key()));
        PdfObject obj;
        obj.num = hm.captured(1).toInt();
        obj.gen = hm.captured(2).toInt();
        qsizetype pos = start + pdf.indexOf("obj", start) - start + 3;

        // Copy the head with strings encrypted.
        bool hasStream = false;
        const qsizetype stop = rewriteHead(pdf, pos, enc, obj.num, obj.gen, obj.head, &hasStream);
        obj.hasStream = hasStream;
        if (hasStream) {
            qsizetype dataStart = stop + 6;
            if (pdf.mid(dataStart, 2) == "\r\n")
                dataStart += 2;
            else if (pdf[dataStart] == '\n' || pdf[dataStart] == '\r')
                dataStart += 1;
            const QString head = QString::fromLatin1(obj.head);
            qsizetype length = -1;
            if (const auto m = lenIndRe.match(head); m.hasMatch()) {
                obj.indirectLength = m.captured(1).toInt();
                const qsizetype lo = offsets.value(obj.indirectLength, -1);
                if (lo >= 0) {
                    const qsizetype bodyAt = pdf.indexOf("obj", lo) + 3;
                    length = pdf.mid(bodyAt, pdf.indexOf("endobj", bodyAt) - bodyAt).trimmed().toLongLong();
                }
            } else if (const auto d = lenDirRe.match(head); d.hasMatch()) {
                length = d.captured(1).toLongLong();
            }
            if (length < 0 || dataStart + length > pdf.size()) {
                const qsizetype end = pdf.indexOf("endstream", dataStart);
                if (end < 0)
                    return fail(QStringLiteral("Unsupported PDF layout (stream)."));
                length = end - dataStart;
                while (length > 0 && (pdf[dataStart + length - 1] == '\n' || pdf[dataStart + length - 1] == '\r'))
                    --length;
            }
            obj.stream = pdf.mid(dataStart, length);
        }
        indexByNum.insert(obj.num, int(objects.size()));
        objects.append(obj);
    }

    // ---- Pass 2: encrypt streams and fix lengths
    QHash<int, qsizetype> newIndirectLengths;
    for (PdfObject &obj : objects) {
        if (!obj.hasStream)
            continue;
        obj.stream = enc.encrypt(obj.num, obj.gen, obj.stream);
        if (obj.indirectLength >= 0) {
            newIndirectLengths.insert(obj.indirectLength, obj.stream.size());
        } else {
            QString head = QString::fromLatin1(obj.head);
            head.replace(lenDirRe, QStringLiteral("/Length %1").arg(obj.stream.size()));
            obj.head = head.toLatin1();
        }
    }

    // ---- Write
    int maxNum = 0;
    for (const PdfObject &obj : std::as_const(objects))
        maxNum = qMax(maxNum, obj.num);
    const int encryptNum = maxNum + 1;

    QByteArray out("%PDF-1.6\n%\xE2\xE3\xCF\xD3\n");
    QMap<int, qsizetype> newOffsets;
    for (const PdfObject &obj : std::as_const(objects)) {
        newOffsets.insert(obj.num, out.size());
        out += QByteArray::number(obj.num) + ' ' + QByteArray::number(obj.gen) + " obj";
        if (newIndirectLengths.contains(obj.num)) {
            out += '\n' + QByteArray::number(newIndirectLengths.value(obj.num)) + '\n';
        } else {
            out += obj.head;
            if (obj.hasStream)
                out += "stream\n" + obj.stream + "\nendstream\n";
        }
        out += "endobj\n";
    }
    newOffsets.insert(encryptNum, out.size());
    out += QByteArray::number(encryptNum) + " 0 obj\n<<\n/Filter /Standard\n/V 4\n/R 4\n/Length 128\n"
           "/CF << /StdCF << /Type /CryptFilter /CFM /AESV2 /AuthEvent /DocOpen /Length 16 >> >>\n"
           "/StmF /StdCF\n/StrF /StdCF\n"
           "/O <" + o.toHex() + ">\n/U <" + u.toHex() + ">\n/P " + QByteArray::number(kPermissions) + "\n>>\nendobj\n";

    const qsizetype xrefAt = out.size();
    out += "xref\n0 " + QByteArray::number(encryptNum + 1) + "\n0000000000 65535 f \n";
    for (int n = 1; n <= encryptNum; ++n) {
        if (newOffsets.contains(n))
            out += QByteArray::number(newOffsets.value(n)).rightJustified(10, '0') + " 00000 n \n";
        else
            out += "0000000000 65535 f \n";
    }
    out += "trailer\n<<\n/Size " + QByteArray::number(encryptNum + 1) + "\n/Root " + rootM.captured(1).toLatin1()
           + ' ' + rootM.captured(2).toLatin1() + " R\n";
    if (infoM.hasMatch())
        out += "/Info " + infoM.captured(1).toLatin1() + ' ' + infoM.captured(2).toLatin1() + " R\n";
    out += "/Encrypt " + QByteArray::number(encryptNum) + " 0 R\n";
    out += "/ID [ <" + idHex + "> <" + idHex + "> ]\n>>\nstartxref\n" + QByteArray::number(xrefAt) + "\n%%EOF\n";
    Q_UNUSED(indexByNum);
    return out;
}

} // namespace PdfEncrypt
