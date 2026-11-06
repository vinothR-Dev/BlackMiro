#include "RelayServer.h"

#include <QWebSocket>
#include <QWebSocketServer>

namespace {
constexpr int kMaxLogFrames = 400;              // ask for a snapshot beyond this
constexpr qint64 kMaxLogBytes = 256ll << 20;    // hard cap per room (256 MB)
constexpr qint64 kMaxFrameBytes = 64ll << 20;   // single message cap (images travel as originals)
}

RelayServer::RelayServer(QObject *parent)
    : QObject(parent)
{
    m_server = new QWebSocketServer(QStringLiteral("BlackMiro relay"), QWebSocketServer::NonSecureMode, this);
    m_server->setMaxPendingConnections(64);
    connect(m_server, &QWebSocketServer::newConnection, this, &RelayServer::onNewConnection);
}

RelayServer::~RelayServer()
{
    close();
}

bool RelayServer::listen(quint16 port, bool localOnly)
{
    return m_server->listen(localOnly ? QHostAddress::LocalHost : QHostAddress::Any, port);
}

quint16 RelayServer::port() const
{
    return m_server->serverPort();
}

void RelayServer::close()
{
    for (auto it = m_roomOf.keyBegin(); it != m_roomOf.keyEnd(); ++it)
        (*it)->close();
    m_server->close();
}

int RelayServer::clientCount() const
{
    return int(m_roomOf.size());
}

void RelayServer::onNewConnection()
{
    while (QWebSocket *socket = m_server->nextPendingConnection()) {
        const QString path = socket->requestUrl().path();
        if (!path.startsWith(QLatin1String("/r/")) || path.size() < 8 || path.size() > 80) {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("Unknown room"));
            socket->deleteLater();
            continue;
        }
        const QString room = path.mid(3);
        socket->setParent(this);
        socket->setMaxAllowedIncomingMessageSize(quint64(kMaxFrameBytes));
        m_roomOf.insert(socket, room);
        Room &r = m_rooms[room];
        r.clients.append(socket);

        // Catch the newcomer up on everything that happened so far.
        for (const QByteArray &frame : std::as_const(r.log))
            socket->sendBinaryMessage(frame);

        connect(socket, &QWebSocket::binaryMessageReceived, this,
                [this, socket](const QByteArray &frame) { onMessage(socket, frame); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket]() { onDisconnected(socket); });
        emit clientCountChanged(clientCount());
    }
}

void RelayServer::onMessage(QWebSocket *from, const QByteArray &frame)
{
    if (frame.isEmpty())
        return;
    const QString roomId = m_roomOf.value(from);
    auto roomIt = m_rooms.find(roomId);
    if (roomIt == m_rooms.end())
        return;
    Room &room = *roomIt;
    const char type = frame.at(0);

    if (type == RelayFrame::Snapshot) {
        room.log = { frame };
        room.logBytes = frame.size();
        room.snapshotRequested = false;
        return;
    }
    if (type == RelayFrame::Ops) {
        room.log.append(frame);
        room.logBytes += frame.size();
        while (room.logBytes > kMaxLogBytes && room.log.size() > 1) {
            room.logBytes -= room.log.at(1).size();
            room.log.removeAt(1);   // keep the base snapshot, drop the oldest ops
        }
        if (room.log.size() > kMaxLogFrames && !room.snapshotRequested) {
            room.snapshotRequested = true;
            from->sendBinaryMessage(QByteArray(1, RelayFrame::WantSnapshot));
        }
    } else if (type != RelayFrame::Presence) {
        return;
    }
    for (const QPointer<QWebSocket> &peer : std::as_const(room.clients))
        if (peer && peer != from)
            peer->sendBinaryMessage(frame);
}

void RelayServer::onDisconnected(QWebSocket *socket)
{
    const QString roomId = m_roomOf.take(socket);
    auto it = m_rooms.find(roomId);
    if (it != m_rooms.end()) {
        it->clients.removeIf([socket](const QPointer<QWebSocket> &p) { return !p || p == socket; });
        if (it->clients.isEmpty())
            m_rooms.erase(it);   // nobody left: forget the (encrypted) room log
    }
    socket->deleteLater();
    emit clientCountChanged(clientCount());
}
