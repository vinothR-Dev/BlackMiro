#include "CloudStore.h"

#include "Crypto.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_WASM
#include <emscripten/val.h>
#endif

namespace {
const QByteArray kMagic = "MIRUSHR1";   // shared-board file: magic | Crypto::seal(key, .board bytes)

QString b64url(const QByteArray &b) { return QString::fromLatin1(b.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)); }
QByteArray unb64url(const QString &s) { return QByteArray::fromBase64(s.toLatin1(), QByteArray::Base64UrlEncoding); }
} // namespace

CloudStore::CloudStore(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_repo = s.value(QStringLiteral("cloud/repo")).toString();
    m_branch = s.value(QStringLiteral("cloud/branch"), m_branch).toString();
    m_token = s.value(QStringLiteral("cloud/token")).toString();
}

bool CloudStore::configure(const QString &repoIn, const QString &token, const QString &branch)
{
    QString repo = repoIn.trimmed();
    repo.remove(QRegularExpression(QStringLiteral("^https?://github\\.com/")));
    repo.remove(QRegularExpression(QStringLiteral("(\\.git)?/*$")));
    if (!QRegularExpression(QStringLiteral("^[A-Za-z0-9-]+/[A-Za-z0-9._-]+$")).match(repo).hasMatch()) {
        emit failed(tr("Enter the repository as owner/name, for example %1.").arg(QStringLiteral("octocat/boards")));
        return false;
    }
    QSettings s;
    m_repo = repo;
    s.setValue(QStringLiteral("cloud/repo"), m_repo);
    if (!branch.trimmed().isEmpty()) {
        m_branch = branch.trimmed();
        s.setValue(QStringLiteral("cloud/branch"), m_branch);
    }
    if (!token.trimmed().isEmpty()) {
        m_token = token.trimmed();
        s.setValue(QStringLiteral("cloud/token"), m_token);
    }
    emit settingsChanged();
    return true;
}

void CloudStore::forgetToken()
{
    m_token.clear();
    QSettings().remove(QStringLiteral("cloud/token"));
    emit settingsChanged();
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

QString CloudStore::userFolder() const
{
    // One folder per person: a hash of their email (the email itself is not published).
    const QString who = m_email.trimmed().toLower();
    if (who.isEmpty())
        return QStringLiteral("boards/guest");
    return QStringLiteral("boards/") + QString::fromLatin1(QCryptographicHash::hash(who.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

void CloudStore::request(const QByteArray &verb, const QString &apiPath, const QByteArray &body, Done done, const QByteArray &accept)
{
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com") + apiPath));
    req.setRawHeader("Accept", accept);
    req.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    if (!body.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(60000);
    QNetworkReply *reply = m_net.sendCustomRequest(req, verb, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, reply->readAll());
    });
}

void CloudStore::ensureBranch(std::function<void()> next)
{
    const QString base = QStringLiteral("/repos/") + m_repo;
    request("GET", base + QStringLiteral("/git/ref/heads/") + m_branch, {}, [this, base, next](int status, const QByteArray &) {
        if (status == 200)
            return next();
        if (status == 401 || status == 403)
            return fail(tr("GitHub refused the token. Check that it has \"Contents: read and write\" on %1.").arg(m_repo));
        if (status != 404)
            return fail(tr("Couldn't reach GitHub (HTTP %1).").arg(status));
        // Create the branch with no history of its own (it never mixes with the code).
        QJsonObject readme{ { "path", "README.md" }, { "mode", "100644" }, { "type", "blob" },
                            { "content", "# Miru boards\n\nEncrypted boards saved from Miru. Each file can only be "
                                         "opened with its share link; GitHub stores ciphertext only.\n" } };
        const QByteArray treeBody = QJsonDocument(QJsonObject{ { "tree", QJsonArray{ readme } } }).toJson(QJsonDocument::Compact);
        request("POST", base + QStringLiteral("/git/trees"), treeBody, [this, base, next](int st, const QByteArray &b) {
            if (st != 201)
                return fail(tr("Couldn't create the %1 branch (HTTP %2).").arg(m_branch).arg(st));
            const QString tree = QJsonDocument::fromJson(b).object().value("sha").toString();
            const QByteArray commitBody = QJsonDocument(QJsonObject{ { "message", "Start Miru boards branch" },
                                                                     { "tree", tree }, { "parents", QJsonArray{} } })
                                              .toJson(QJsonDocument::Compact);
            request("POST", base + QStringLiteral("/git/commits"), commitBody, [this, base, next](int st2, const QByteArray &b2) {
                if (st2 != 201)
                    return fail(tr("Couldn't create the %1 branch (HTTP %2).").arg(m_branch).arg(st2));
                const QString commit = QJsonDocument::fromJson(b2).object().value("sha").toString();
                const QByteArray refBody = QJsonDocument(QJsonObject{ { "ref", "refs/heads/" + m_branch }, { "sha", commit } })
                                               .toJson(QJsonDocument::Compact);
                request("POST", base + QStringLiteral("/git/refs"), refBody, [this, next](int st3, const QByteArray &) {
                    if (st3 != 201 && st3 != 422)    // 422: created meanwhile by another save
                        return fail(tr("Couldn't create the %1 branch (HTTP %2).").arg(m_branch).arg(st3));
                    next();
                });
            });
        });
    });
}

void CloudStore::upload(const QString &path, const QByteArray &bytes, const QString &message, std::function<void()> next)
{
    const QString api = QStringLiteral("/repos/") + m_repo + QStringLiteral("/contents/") + path;
    // Updating an existing file needs its current blob sha.
    request("GET", api + QStringLiteral("?ref=") + m_branch, {}, [this, api, bytes, message, next](int status, const QByteArray &body) {
        QJsonObject put{ { "message", message }, { "branch", m_branch },
                         { "content", QString::fromLatin1(bytes.toBase64()) } };
        if (status == 200)
            put["sha"] = QJsonDocument::fromJson(body).object().value("sha").toString();
        else if (status != 404)
            return fail(tr("Couldn't reach GitHub (HTTP %1).").arg(status));
        request("PUT", api, QJsonDocument(put).toJson(QJsonDocument::Compact), [this, next](int st, const QByteArray &b) {
            if (st == 200 || st == 201)
                return next();
            const QString why = QJsonDocument::fromJson(b).object().value("message").toString();
            fail(tr("GitHub didn't save the board (HTTP %1%2).").arg(st).arg(why.isEmpty() ? QString() : QStringLiteral(": ") + why));
        });
    });
}

void CloudStore::saveBoard(BoardCanvas *canvas)
{
    if (!canvas || m_busy)
        return;
    if (!configured())
        return fail(tr("Connect a GitHub repository first."));
    const QString uid = canvas->boardUid();
    // One key per board, remembered on this device, so saving again keeps the same link.
    QSettings s;
    const QString keyName = QStringLiteral("cloud/keys/") + uid;
    QByteArray key = unb64url(s.value(keyName).toString());
    if (key.size() != 64) {
        key = Crypto::randomBytes(64);
        s.setValue(keyName, b64url(key));
    }
    const QByteArray sealed = kMagic + Crypto::seal(key, canvas->saveData());
    const QString path = userFolder() + QLatin1Char('/') + uid + QStringLiteral(".miru");
    setBusy(true);
    emit message(tr("Saving to GitHub…"));
    const QPointer<BoardCanvas> board(canvas);
    ensureBranch([this, path, sealed, key, board]() {
        const QString title = board ? board->property("boardTitle").toString() : QString();
        upload(path, sealed, QStringLiteral("Save board %1").arg(title.isEmpty() ? path.section('/', -1) : title), [this, path, key]() {
            m_lastLink = makeLink(path, key);
            setBusy(false);
            emit linkReady(m_lastLink);
        });
    });
}

QString CloudStore::makeLink(const QString &path, const QByteArray &key) const
{
    const QString owner = m_repo.section('/', 0, 0);
    const QString name = m_repo.section('/', 1, 1);
    return QStringLiteral("https://%1.github.io/%2/app/#b=%3/%4/%5&k=%6")
        .arg(owner.toLower(), name, m_repo, m_branch, path, b64url(key));
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
    const QString api = QStringLiteral("/repos/%1/%2/contents/%3?ref=%4").arg(t.owner, t.name, t.path, t.branch);
    const QPointer<BoardCanvas> target(canvas);
    request("GET", api, {}, [this, target, key](int status, const QByteArray &body) {
        if (status == 404)
            return fail(tr("This board isn't there any more, or the repository is private (connect GitHub with a token that can read it)."));
        if (status != 200)
            return fail(tr("Couldn't download the board (HTTP %1).").arg(status));
        QByteArray plain;
        if (!body.startsWith(kMagic) || !Crypto::open(key, body.mid(kMagic.size()), &plain))
            return fail(tr("The link's key doesn't open this board (the link may be incomplete)."));
        setBusy(false);
        if (target && target->openData(plain, QStringLiteral("Shared board")))
            emit opened(target->property("boardTitle").toString());
    }, "application/vnd.github.raw+json");
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
