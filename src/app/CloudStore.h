#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

#include "BoardCanvas.h"

class QNetworkReply;

// Share links for everyone - no GitHub account or token needed.
//
// The board is encrypted here, with its own random 64-byte key (AES-256-CTR +
// HMAC-SHA256, see Crypto::seal), and sent to the Miru share service
// (tools/share-service), which stores the ciphertext in the public boards
// repository with a token only the service holds:
//
//   <MIRU_SHARE_REPO>@<branch>:boards/<anonymous user id>/<random id>.miru
//
// The key travels only in the link's #fragment, which browsers never send to a server:
//
//   https://<owner>.github.io/<app repo>/app/#b=<owner>/<repo>/<branch>/<path>&k=<key>
//
// Opening a link downloads the file straight from raw.githubusercontent.com.
class CloudStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool available READ available CONSTANT)                       // share service built in
    Q_PROPERTY(QString userEmail MEMBER m_email NOTIFY settingsChanged)       // folder per user
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastLink READ lastLink NOTIFY linkReady)

public:
    explicit CloudStore(QObject *parent = nullptr);

    bool available() const;
    bool busy() const { return m_busy; }
    QString lastLink() const { return m_lastLink; }

    // Encrypts the canvas' board and uploads it; emits linkReady(shareLink) when done.
    // Saving the same board again updates it and keeps its link.
    Q_INVOKABLE void saveBoard(BoardCanvas *canvas);
    // The link this device already made for the canvas' board, or "".
    Q_INVOKABLE QString linkFor(BoardCanvas *canvas) const;
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

    QString makeLink(const QString &repo, const QString &branch, const QString &path, const QByteArray &key) const;
    static bool parseLink(const QString &link, Target *t, QByteArray *key);
    void setBusy(bool b);
    void fail(const QString &text);

    QNetworkAccessManager m_net;
    QString m_email;
    QString m_lastLink;
    bool m_busy = false;
};
