#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>

#include "RelayFrame.h"

class QWebSocket;
class QWebSocketServer;


// Minimal room-based relay. Clients connect to ws://host:port/r/<roomId>.
// New clients first receive the room's log (latest snapshot + later ops), then live traffic.
class RelayServer : public QObject
{
    Q_OBJECT
public:
    explicit RelayServer(QObject *parent = nullptr);
    ~RelayServer() override;

    bool listen(quint16 port, bool localOnly = false);   // all interfaces unless localOnly
    quint16 port() const;
    void close();
    int clientCount() const;

signals:
    void clientCountChanged(int count);

private:
    struct Room
    {
        QList<QPointer<QWebSocket>> clients;
        QList<QByteArray> log;
        qint64 logBytes = 0;
        bool snapshotRequested = false;
    };

    void onNewConnection();
    void onMessage(QWebSocket *from, const QByteArray &frame);
    void onDisconnected(QWebSocket *socket);

    QWebSocketServer *m_server = nullptr;
    QHash<QString, Room> m_rooms;
    QHash<QWebSocket *, QString> m_roomOf;
};
