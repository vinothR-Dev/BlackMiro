#pragma once

#include "BoardCanvas.h"
#include "SyncEngine.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QWebSocket;
class RelayServer;

// Live collaboration: hosts an in-app relay (desktop / Android) or joins one, keeps the
// board in sync through SyncEngine, and shares cursors. Everything on the wire is
// end-to-end encrypted with the secret in the invite link.
class CollabController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(BoardCanvas *canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    Q_PROPERTY(QString displayName MEMBER m_name NOTIFY peersChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)          // off | connecting | live | error
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY stateChanged)
    Q_PROPERTY(bool canHost READ canHost CONSTANT)
    Q_PROPERTY(QString inviteLink READ inviteLink NOTIFY stateChanged)
    Q_PROPERTY(QVariantList peers READ peers NOTIFY peersChanged)     // [{name, color}]
    Q_PROPERTY(QString myColor READ myColor CONSTANT)

public:
    explicit CollabController(QObject *parent = nullptr);
    ~CollabController() override;

    BoardCanvas *canvas() const { return m_canvas; }
    void setCanvas(BoardCanvas *c);
    QString state() const { return m_state; }
    QString statusText() const { return m_status; }
    bool isHost() const { return m_relay != nullptr; }
    bool canHost() const;
    QString inviteLink() const { return m_invite; }
    QVariantList peers() const;
    QString myColor() const { return m_color; }

    Q_INVOKABLE bool startHosting();
    Q_INVOKABLE bool join(const QString &link);
    Q_INVOKABLE void leave();
    Q_INVOKABLE void copyInvite();
    Q_INVOKABLE bool isInviteLink(const QString &text) const;

signals:
    void canvasChanged();
    void stateChanged();
    void peersChanged();
    void message(const QString &text);

private:
    void setState(const QString &state, const QString &status);
    void connectTo(const QString &wsUrl, const QString &room, const QString &secret);
    void onBinary(const QByteArray &frame);
    void sendFrame(char type, const QJsonObject &payload);
    void flushLocal();
    void sendSnapshot();
    void sendPresence();
    void prunePeers();
    void updateCursors();

    QPointer<BoardCanvas> m_canvas;
    QWebSocket *m_socket = nullptr;
    RelayServer *m_relay = nullptr;
    SyncEngine m_sync;
    QByteArray m_key;                 // 64 bytes derived from the invite secret
    QString m_state = QStringLiteral("off");
    QString m_status;
    QString m_invite;
    QString m_name;
    QString m_color;
    bool m_applyingRemote = false;
    int m_opsSinceSnapshot = 0;

    QTimer m_flushTimer;              // batches local edits (~25 per second while dragging)
    QTimer m_presenceTimer;           // heartbeat + throttled cursor updates
    QPointF m_cursor;
    bool m_hasCursor = false;
    bool m_cursorDirty = false;
    bool m_awaitingFirstSync = false;

    struct Peer { QString name; QString color; QString chat; QPointF cursor; bool hasCursor = false; QStringList selection; qint64 seen = 0; };
    QHash<QString, Peer> m_peers;
    QElapsedTimer m_clock;
};
