#include "BoardFile.h"

#include "Crypto.h"

#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

namespace BoardFile {

namespace {
constexpr int kFormatVersion = 1;

QString extensionFor(const QByteArray &format)
{
    if (format.isEmpty())
        return QStringLiteral("bin");
    if (format == "jpeg")
        return QStringLiteral("jpg");
    return QString::fromLatin1(format);
}
} // namespace

QByteArray save(const Document &doc, bool includeHistory, const QString &password)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    QZipWriter zip(&buf);
    zip.setCompressionPolicy(QZipWriter::AutoCompress); // deflate is lossless

    QJsonObject root;
    root["format"] = "blackmiro";   // file format id (kept from v1.0 for compatibility)
    root["version"] = kFormatVersion;
    root["title"] = doc.title;
    if (!doc.uid.isEmpty())
        root["uid"] = doc.uid;
    if (!doc.owner.isEmpty())
        root["owner"] = doc.owner;
    if (doc.isPrivate)
        root["private"] = true;
    if (!doc.layers.isEmpty())
        root["layers"] = doc.layersToJson();
    if (!doc.startView.isEmpty())
        root["startView"] = QJsonArray{ doc.startView.x(), doc.startView.y(), doc.startView.width(), doc.startView.height() };
    root["items"] = doc.itemsToJson();

    QJsonArray assetIndex;
    for (auto it = doc.assets.constBegin(); it != doc.assets.constEnd(); ++it) {
        const QString file = QStringLiteral("assets/%1.%2").arg(it.key(), extensionFor(it->format));
        QJsonObject a;
        a["key"] = it.key();
        a["format"] = QString::fromLatin1(it->format);
        a["file"] = file;
        assetIndex.append(a);
        zip.addFile(file, it->data);
    }
    root["assets"] = assetIndex;
    zip.addFile(QStringLiteral("board.json"), QJsonDocument(root).toJson(QJsonDocument::Indented));

    if (includeHistory && !doc.versions.isEmpty()) {
        QJsonArray versions;
        for (const BoardVersion &v : doc.versions) {
            QJsonObject o;
            o["timestamp"] = v.timestamp.toString(Qt::ISODateWithMs);
            o["items"] = v.items;
            versions.append(o);
        }
        zip.addFile(QStringLiteral("versions.json"), QJsonDocument(versions).toJson(QJsonDocument::Compact));
    }
    zip.close();
    // Private boards: the whole ZIP is encrypted (AES-256 + HMAC, key from the password).
    return password.isEmpty() ? out : Crypto::encryptContainer(out, password);
}

bool isEncrypted(const QByteArray &data)
{
    return Crypto::isEncryptedContainer(data);
}

bool load(const QByteArray &data, Document &doc, QString *error, const QString &password)
{
    QByteArray copy = data;
    if (Crypto::isEncryptedContainer(data)) {
        if (password.isEmpty()) {
            if (error) *error = QStringLiteral("This board is private. Enter its password.");
            return false;
        }
        if (!Crypto::decryptContainer(data, password, &copy)) {
            if (error) *error = QStringLiteral("Wrong password.");
            return false;
        }
    }
    QBuffer buf(&copy);
    buf.open(QIODevice::ReadOnly);
    QZipReader zip(&buf);
    if (!zip.isReadable() || zip.status() != QZipReader::NoError) {
        if (error) *error = QStringLiteral("Not a valid .board file.");
        return false;
    }

    QJsonParseError pe;
    const QJsonDocument jd = QJsonDocument::fromJson(zip.fileData(QStringLiteral("board.json")), &pe);
    if (pe.error != QJsonParseError::NoError || !jd.isObject()) {
        if (error) *error = QStringLiteral("board.json is missing or corrupt: %1").arg(pe.errorString());
        return false;
    }
    const QJsonObject root = jd.object();
    // "infiniteboard" = boards saved before the app was named Miru.
    const QString format = root["format"].toString();
    if (format != QLatin1String("blackmiro") && format != QLatin1String("miru") && format != QLatin1String("infiniteboard")) {
        if (error) *error = QStringLiteral("Unknown board format.");
        return false;
    }
    if (root["version"].toInt() > kFormatVersion) {
        if (error) *error = QStringLiteral("This board was saved by a newer version of the app.");
        return false;
    }

    Document loaded;
    loaded.title = root["title"].toString(loaded.title);
    loaded.uid = root["uid"].toString();
    loaded.owner = root["owner"].toString();
    loaded.layersFromJson(root["layers"].toArray());
    if (const QJsonArray sv = root["startView"].toArray(); sv.size() == 4)
        loaded.startView = QRectF(sv[0].toDouble(), sv[1].toDouble(), sv[2].toDouble(), sv[3].toDouble());
    loaded.isPrivate = root["private"].toBool() || Crypto::isEncryptedContainer(data);
    loaded.itemsFromJson(root["items"].toArray());
    for (const QJsonValue &v : root["assets"].toArray()) {
        const QJsonObject a = v.toObject();
        Asset asset;
        asset.data = zip.fileData(a["file"].toString());
        asset.format = a["format"].toString().toLatin1();
        if (!asset.data.isEmpty())
            loaded.assets.insert(a["key"].toString(), asset);
    }

    const QJsonDocument vd = QJsonDocument::fromJson(zip.fileData(QStringLiteral("versions.json")));
    for (const QJsonValue &v : vd.array()) {
        const QJsonObject o = v.toObject();
        loaded.versions.append({ QDateTime::fromString(o["timestamp"].toString(), Qt::ISODateWithMs),
                                 o["items"].toArray() });
    }

    doc = std::move(loaded);
    return true;
}

bool saveToFile(const Document &doc, const QString &path, QString *error, const QString &password)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(save(doc, true, password));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

bool loadFromFile(const QString &path, Document &doc, QString *error, const QString &password)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    return load(f.readAll(), doc, error, password);
}

} // namespace BoardFile
