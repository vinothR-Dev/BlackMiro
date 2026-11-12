#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include "BoardCanvas.h"

class QNetworkReply;

// Saves boards to a branch of a GitHub repository, encrypted, one folder per user,
// and opens them again from share links.
//
//   <branch>/boards/<user id>/<board id>.miru
//
// Every file is sealed with its own random 64-byte key (AES-256-CTR + HMAC-SHA256,
// see Crypto::seal); GitHub only ever stores ciphertext. The key travels in the
// share link's #fragment, which browsers never send to a server:
//
//   https://<owner>.github.io/<repo>/app/#b=<owner>/<repo>/<branch>/<path>&k=<key>
//
// Writing needs a GitHub token with "Contents: read and write" on the repository
// (entered once, kept in this device's settings). Opening a link to a public
// repository needs no token.
class CloudStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString repo READ repo NOTIFY settingsChanged)                 // owner/name
    Q_PROPERTY(QString branch READ branch NOTIFY settingsChanged)
    Q_PROPERTY(bool hasToken READ hasToken NOTIFY settingsChanged)
    Q_PROPERTY(bool configured READ configured NOTIFY settingsChanged)
    Q_PROPERTY(QString userEmail MEMBER m_email NOTIFY settingsChanged)       // folder per user
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastLink READ lastLink NOTIFY linkReady)

public:
    explicit CloudStore(QObject *parent = nullptr);

    QString repo() const { return m_repo; }
    QString branch() const { return m_branch; }
    bool hasToken() const { return !m_token.isEmpty(); }
    bool configured() const { return !m_repo.isEmpty() && !m_token.isEmpty(); }
    bool busy() const { return m_busy; }
    QString lastLink() const { return m_lastLink; }

    // repo "owner/name" (a github.com URL is accepted too); empty token keeps the old one.
    Q_INVOKABLE bool configure(const QString &repo, const QString &token, const QString &branch = QString());
    Q_INVOKABLE void forgetToken();

    // Encrypts the canvas' board and uploads it; emits linkReady(shareLink) when done.
    Q_INVOKABLE void saveBoard(BoardCanvas *canvas);
    // Opens a share link (web or miru:// form) into the canvas.
    Q_INVOKABLE void openLink(BoardCanvas *canvas, const QString &link);
    Q_INVOKABLE bool isShareLink(const QString &text) const;
    // Share link carried by the web page's address (#b=…&k=…), or "" when none.
    Q_INVOKABLE QString startupLink() const;
    Q_INVOKABLE void copyLink(const QString &link);

signals:
    void settingsChanged();
    void busyChanged();
    void linkReady(const QString &link);
    void opened(const QString &title);
    void message(const QString &text);
    void failed(const QString &text);

private:
    struct Target { QString owner, name, branch, path; };
    using Done = std::function<void(int status, const QByteArray &body)>;

    void request(const QByteArray &verb, const QString &apiPath, const QByteArray &body, Done done,
                 const QByteArray &accept = "application/vnd.github+json");
    void ensureBranch(std::function<void()> next);
    void upload(const QString &path, const QByteArray &bytes, const QString &message, std::function<void()> next);
    QString userFolder() const;
    QString makeLink(const QString &path, const QByteArray &key) const;
    static bool parseLink(const QString &link, Target *t, QByteArray *key);
    void setBusy(bool b);
    void fail(const QString &text);

    QNetworkAccessManager m_net;
    QString m_repo;
    QString m_branch = QStringLiteral("miru-boards");
    QString m_token;
    QString m_email;
    QString m_lastLink;
    bool m_busy = false;
};
