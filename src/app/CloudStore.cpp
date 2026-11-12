#include "CloudStore.h"

#include "Crypto.h"

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_WASM
#include <emscripten/val.h>
#endif

namespace {
#ifndef MIRU_SHARE_URL
#define MIRU_SHARE_URL ""
#endif
#ifndef MIRU_WEB_APP_URL
#define MIRU_WEB_APP_URL "https://vinothr-dev.github.io/BlackMiro/app/"
#endif
const QString kService = QStringLiteral(MIRU_SHARE_URL);    // share service (tools/share-service)
const QString kWebApp = QStringLiteral(MIRU_WEB_APP_URL);   // where share links open in a browser
const QByteArray kMagic = "MIRUSHR1";   // shared-board file: magic | Crypto::seal(key, .board bytes)

QString b64url(const QByteArray &b) { return QString::fromLatin1(b.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)); }
QByteArray unb64url(const QString &s) { return QByteArray::fromBase64(s.toLatin1(), QByteArray::Base64UrlEncoding); }
} // namespace

CloudStore::CloudStore(QObject *parent)
    : QObject(parent)
{
}

bool CloudStore::available() const
{
    return !kService.isEmpty();
}

void CloudStore::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

void CloudStore::fail(const QString &text)
{
    setBusy(false);
    emit failed(text);
}

QString CloudStore::linkFor(BoardCanvas *canvas) const
{
    return canvas ? QSettings().value(QStringLiteral("share/%1/link").arg(canvas->boardUid())).toString() : QString();
}

void CloudStore::saveBoard(BoardCanvas *canvas)
{
    if (!canvas || m_busy)
        return;
    if (!available())
        return fail(tr("Share links aren't set up in this build of Miru."));
    const QString uid = canvas->boardUid();
    const QString prefix = QStringLiteral("share/%1/").arg(uid);
    // One key per board, remembered on this device, so saving again keeps the same link.
    QSettings s;
    QByteArray key = unb64url(s.value(prefix + QStringLiteral("key")).toString());
    if (key.size() != 64) {
        key = Crypto::randomBytes(64);
        s.setValue(prefix + QStringLiteral("key"), b64url(key));
    }
    const QString path = s.value(prefix + QStringLiteral("path")).toString();
    const QString edit = s.value(prefix + QStringLiteral("edit")).toString();
    const QByteArray sealed = kMagic + Crypto::seal(key, canvas->saveData());

    QNetworkRequest req(QUrl(kService + QStringLiteral("/v1/boards")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
    req.setRawHeader("X-Miru-User", QUrl::toPercentEncoding(m_email.trimmed().toLower(), "@.+"));
    const bool update = !path.isEmpty() && !edit.isEmpty();
    if (update) {
        req.setRawHeader("X-Miru-Path", path.toUtf8());
        req.setRawHeader("X-Miru-Edit", edit.toUtf8());
    }
    req.setTransferTimeout(60000);
    setBusy(true);
    emit message(update ? tr("Updating share link…") : tr("Creating share link…"));
    QNetworkReply *reply = m_net.post(req, sealed);
    const QPointer<BoardCanvas> board(canvas);
    connect(reply, &QNetworkReply::finished, this, [this, reply, board, prefix, key, update]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
        if (status == 403 && update) {
            // Not ours any more (e.g. settings copied from another device): share as a new board.
            QSettings s;
            s.remove(prefix + QStringLiteral("path"));
            s.remove(prefix + QStringLiteral("edit"));
            setBusy(false);
            if (board)
                saveBoard(board);
            return;
        }
        if (status != 200) {
            QString why = json.value(QStringLiteral("error")).toString();
            if (why.isEmpty())
                why = status ? tr("HTTP %1").arg(status) : reply->errorString();
            return fail(tr("Couldn't create the share link: %1").arg(why));
        }
        const QString repo = json.value(QStringLiteral("repo")).toString();
        const QString branch = json.value(QStringLiteral("branch")).toString();
        const QString path = json.value(QStringLiteral("path")).toString();
        QSettings s;
        s.setValue(prefix + QStringLiteral("path"), path);
        s.setValue(prefix + QStringLiteral("edit"), json.value(QStringLiteral("edit")).toString());
        m_lastLink = makeLink(repo, branch, path, key);
        s.setValue(prefix + QStringLiteral("link"), m_lastLink);
        setBusy(false);
        emit linkReady(m_lastLink);
    });
}

QString CloudStore::makeLink(const QString &repo, const QString &branch, const QString &path, const QByteArray &key) const
{
    return QStringLiteral("%1#b=%2/%3/%4&k=%5").arg(kWebApp, repo, branch, path, b64url(key));
}

bool CloudStore::parseLink(const QString &linkIn, Target *t, QByteArray *key)
{
    // https://…/app/#b=owner/repo/branch/boards/…/x.miru&k=KEY   or   miru://open?b=…&k=…
    const QString link = linkIn.trimmed();
    QString params;
    const int hash = link.indexOf(QLatin1Char('#'));
    if (hash >= 0)
        params = link.mid(hash + 1);
    else if (link.startsWith(QLatin1String("miru://open?")))
        params = link.mid(12);
    else
        return false;
    const QUrlQuery q(params);
    const QStringList parts = q.queryItemValue(QStringLiteral("b"), QUrl::FullyDecoded).split(QLatin1Char('/'));
    *key = unb64url(q.queryItemValue(QStringLiteral("k")));
    if (parts.size() < 4 || key->size() != 64)
        return false;
    t->owner = parts[0];
    t->name = parts[1];
    t->branch = parts[2];
    t->path = parts.mid(3).join(QLatin1Char('/'));
    return t->path.endsWith(QLatin1String(".miru"));
}

bool CloudStore::isShareLink(const QString &text) const
{
    Target t;
    QByteArray key;
    return parseLink(text, &t, &key);
}

void CloudStore::openLink(BoardCanvas *canvas, const QString &link)
{
    Target t;
    QByteArray key;
    if (!canvas || !parseLink(link, &t, &key))
        return fail(tr("That isn't a Miru share link."));
    setBusy(true);
    emit message(tr("Opening shared board…"));
    // Public file host: no API rate limit; the query skips stale CDN copies after an update.
    QNetworkRequest req(QUrl(QStringLiteral("https://raw.githubusercontent.com/%1/%2/%3/%4?t=%5")
                                 .arg(t.owner, t.name, t.branch, t.path)
                                 .arg(QDateTime::currentMSecsSinceEpoch())));
    req.setTransferTimeout(60000);
    QNetworkReply *reply = m_net.get(req);
    const QPointer<BoardCanvas> target(canvas);
    connect(reply, &QNetworkReply::finished, this, [this, reply, target, key]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 404)
            return fail(tr("This board isn't there any more (or the link was just created - try again in a minute)."));
        if (status != 200)
            return fail(tr("Couldn't download the board (%1).").arg(status ? tr("HTTP %1").arg(status) : reply->errorString()));
        QByteArray plain;
        if (!body.startsWith(kMagic) || !Crypto::open(key, body.mid(kMagic.size()), &plain))
            return fail(tr("The link's key doesn't open this board (the link may be incomplete)."));
        setBusy(false);
        if (target && target->openData(plain, QStringLiteral("Shared board")))
            emit opened(target->property("boardTitle").toString());
    });
}

QString CloudStore::startupLink() const
{
#ifdef Q_OS_WASM
    const QString hash = QString::fromStdString(emscripten::val::global("location")["hash"].as<std::string>());
    const QString link = QStringLiteral("https://x/") + hash;
    return isShareLink(link) ? link : QString();
#else
    return {};
#endif
}

void CloudStore::copyLink(const QString &link)
{
    QGuiApplication::clipboard()->setText(link);
    emit message(tr("Share link copied"));
}
