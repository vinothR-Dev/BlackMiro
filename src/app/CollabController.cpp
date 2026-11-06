#include "CollabController.h"

#include "Crypto.h"
#include "RelayFrame.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#if !defined(Q_OS_WASM)
#include <QNetworkInterface>
#endif
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>

#if !defined(Q_OS_WASM)
#include "RelayServer.h"
#endif

namespace {

constexpr quint16 kFirstPort = 8765;
constexpr int kKeyIterations = 60000;
constexpr qint64 kPeerTimeoutMs = 7000;

const char *kPeerColors[] = { "#f24726", "#12cdd4", "#8fd14f", "#9510ac", "#fac710", "#da0063", "#2d9bf0", "#ff9d48" };

[[maybe_unused]] QString lanAddress()
{
#if defined(Q_OS_WASM)
    return QStringLiteral("127.0.0.1");
#else
    QString fallback;
    for (const QHostAddress &a : QNetworkInterface::allAddresses()) {
        if (a.protocol() != QAbstractSocket::IPv4Protocol || a.isLoopback() || a.isLinkLocal())
            continue;
        if (a.isPrivateUse())
            return a.toString();     // 192.168.x / 10.x / 172.16-31.x: the local network
        if (fallback.isEmpty())
            fallback = a.toString();
    }
    return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
#endif
}

QString randomToken(int bytes)
{
    return QString::fromLatin1(Crypto::randomBytes(bytes).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

} // namespace

CollabController::CollabController(QObject *parent)
    : QObject(parent)
{
    m_color = QString::fromLatin1(kPeerColors[QRandomGenerator::global()->bounded(int(std::size(kPeerColors)))]);
    m_clock.start();

    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(40);
    connect(&m_flushTimer, &QTimer::timeout, this, &CollabController::flushLocal);

    m_presenceTimer.setInterval(60);
    connect(&m_presenceTimer, &QTimer::timeout, this, [this]() {
        static qint64 lastBeat = 0;
        const qint64 now = m_clock.elapsed();
        if (m_cursorDirty || now - lastBeat > 2000) {
            sendPresence();
            lastBeat = now;
            m_cursorDirty = false;
        }
        prunePeers();
    });
}

CollabController::~CollabController()
{
    leave();
}

bool CollabController::canHost() const
{
#if defined(Q_OS_WASM)
    return false;   // browsers can't listen for connections; join a desktop host instead
#else
    return true;
#endif
}

void CollabController::setCanvas(BoardCanvas *c)
{
    if (m_canvas == c)
        return;
    if (m_canvas)
        disconnect(m_canvas, nullptr, this, nullptr);
    m_canvas = c;
    if (m_canvas) {
        connect(m_canvas, &BoardCanvas::localEdit, this, [this]() {
            if (m_state == QLatin1String("live") && !m_applyingRemote && !m_flushTimer.isActive())
                m_flushTimer.start();
        });
        connect(m_canvas, &BoardCanvas::pointerWorldMoved, this, [this](double x, double y) {
            m_cursor = QPointF(x, y);
            m_hasCursor = true;
            m_cursorDirty = true;
        });
        connect(m_canvas, &BoardCanvas::selectionChanged, this, [this]() { m_cursorDirty = true; });
        connect(m_canvas, &BoardCanvas::cursorChatChanged, this, [this]() { m_cursorDirty = true; });
    }
    emit canvasChanged();
}

void CollabController::setState(const QString &state, const QString &status)
{
    m_state = state;
    m_status = status;
    emit stateChanged();
}

QVariantList CollabController::peers() const
{
    QVariantList out;
    for (const Peer &p : m_peers)
        out << QVariantMap{ { "name", p.name }, { "color", p.color } };
    return out;
}

bool CollabController::isInviteLink(const QString &text) const
{
    return text.trimmed().startsWith(QLatin1String("blackmiro://join?"));
}

bool CollabController::startHosting()
{
#if defined(Q_OS_WASM)
    emit message(QStringLiteral("The web version can join a live session but can't host one."));
    return false;
#else
    if (!m_canvas || m_state != QLatin1String("off"))
        return false;
    m_relay = new RelayServer(this);
    quint16 port = kFirstPort;
    // BLACKMIRO_RELAY_LOCALHOST=1: automated tests, no firewall prompt.
    const bool localOnly = qEnvironmentVariableIsSet("BLACKMIRO_RELAY_LOCALHOST");
    while (!m_relay->listen(port, localOnly) && port < kFirstPort + 20)
        ++port;
    if (!m_relay->port()) {
        delete m_relay;
        m_relay = nullptr;
        setState(QStringLiteral("error"), QStringLiteral("Couldn't open a network port for the session."));
        return false;
    }
    const QString room = randomToken(9);
    const QString secret = randomToken(24);
    const QString publicUrl = QStringLiteral("ws://%1:%2")
                                  .arg(localOnly ? QStringLiteral("127.0.0.1") : lanAddress())
                                  .arg(m_relay->port());

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("ws"), publicUrl);
    q.addQueryItem(QStringLiteral("room"), room);
    q.addQueryItem(QStringLiteral("key"), secret);
    m_invite = QStringLiteral("blackmiro://join?") + q.toString(QUrl::FullyEncoded);

    // The host's board becomes the session's starting state.
    m_sync.reset();
    connectTo(QStringLiteral("ws://127.0.0.1:%1").arg(m_relay->port()), room, secret);
    return true;
#endif
}

bool CollabController::join(const QString &link)
{
    if (!m_canvas || m_state != QLatin1String("off"))
        return false;
    const QUrl url(link.trimmed());
    const QUrlQuery q(url);
    const QString ws = q.queryItemValue(QStringLiteral("ws"), QUrl::FullyDecoded);
    const QString room = q.queryItemValue(QStringLiteral("room"));
    const QString secret = q.queryItemValue(QStringLiteral("key"));
    if (url.scheme() != QLatin1String("blackmiro") || ws.isEmpty() || room.isEmpty() || secret.size() < 16) {
        emit message(QStringLiteral("That isn't a BlackMiro invite link."));
        return false;
    }
    m_invite = link.trimmed();
    // Start from an empty board; the session's content arrives from the relay.
    m_canvas->newBoard();
    m_sync.adopt(m_canvas->document());
    m_awaitingFirstSync = true;
    connectTo(ws, room, secret);
    return true;
}

void CollabController::connectTo(const QString &wsUrl, const QString &room, const QString &secret)
{
    setState(QStringLiteral("connecting"), QStringLiteral("Connecting…"));
    m_key = Crypto::pbkdf2Sha256(secret.toUtf8(), QByteArray("blackmiro-room:") + room.toUtf8(), kKeyIterations, 64);
    m_peers.clear();
    m_opsSinceSnapshot = 0;
    emit peersChanged();

    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket->setMaxAllowedIncomingMessageSize(64ull << 20);
    connect(m_socket, &QWebSocket::connected, this, [this]() {
        setState(QStringLiteral("live"), isHost() ? QStringLiteral("Live — share the invite link")
                                                   : QStringLiteral("Live — editing together"));
        if (isHost()) {
            m_sync.diff(m_canvas->document());   // assign versions to the current board
            sendSnapshot();
        }
        sendPresence();
        m_presenceTimer.start();
    });
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &CollabController::onBinary);
    connect(m_socket, &QWebSocket::disconnected, this, [this]() {
        if (m_state == QLatin1String("off"))
            return;
        m_presenceTimer.stop();
        m_peers.clear();
        emit peersChanged();
        updateCursors();
        setState(QStringLiteral("error"), QStringLiteral("Disconnected from the session."));
    });
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        setState(QStringLiteral("error"), QStringLiteral("Can't reach the host: %1").arg(m_socket->errorString()));
    });
    m_socket->open(QUrl(wsUrl + QStringLiteral("/r/") + room));
}

void CollabController::leave()
{
    m_presenceTimer.stop();
    m_flushTimer.stop();
    if (m_socket) {
        disconnect(m_socket, nullptr, this, nullptr);
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
#if !defined(Q_OS_WASM)
    if (m_relay) {
        m_relay->close();
        m_relay->deleteLater();
        m_relay = nullptr;
    }
#endif
    m_peers.clear();
    m_invite.clear();
    m_key.clear();
    emit peersChanged();
    updateCursors();
    setState(QStringLiteral("off"), QString());
}

void CollabController::copyInvite()
{
    if (m_invite.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_invite);
    emit message(QStringLiteral("Invite link copied"));
}

void CollabController::sendFrame(char type, const QJsonObject &payload)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    const QByteArray plain = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    m_socket->sendBinaryMessage(QByteArray(1, type) + Crypto::seal(m_key, plain));
}

void CollabController::flushLocal()
{
    if (!m_canvas || m_state != QLatin1String("live"))
        return;
    const QJsonArray ops = m_sync.diff(m_canvas->document());
    if (ops.isEmpty())
        return;
    sendFrame(RelayFrame::Ops, QJsonObject{ { "ops", ops } });
    m_opsSinceSnapshot += int(ops.size());
}

void CollabController::sendSnapshot()
{
    sendFrame(RelayFrame::Snapshot, QJsonObject{ { "ops", m_sync.snapshot(m_canvas->document()) } });
    m_opsSinceSnapshot = 0;
}

void CollabController::sendPresence()
{
    if (!m_canvas)
        return;
    QJsonArray sel;
    for (const QString &id : m_canvas->selectedIds())
        sel.append(id);
    sendFrame(RelayFrame::Presence, QJsonObject{ { "p", QJsonObject{
        { "c", m_sync.clientId() }, { "n", m_name.isEmpty() ? QStringLiteral("Guest") : m_name },
        { "col", m_color }, { "x", m_cursor.x() }, { "y", m_cursor.y() }, { "hc", m_hasCursor },
        { "ch", m_canvas->cursorChat() },
        { "sel", sel } } } });
}

void CollabController::onBinary(const QByteArray &frame)
{
    if (frame.isEmpty() || !m_canvas)
        return;
    const char type = frame.at(0);
    if (type == RelayFrame::WantSnapshot) {
        flushLocal();
        sendSnapshot();
        return;
    }
    QByteArray plain;
    if (!Crypto::open(m_key, frame.mid(1), &plain))
        return;   // wrong key or tampered: ignore
    const QJsonObject msg = QJsonDocument::fromJson(plain).object();

    if (type == RelayFrame::Presence) {
        const QJsonObject p = msg["p"].toObject();
        const QString id = p["c"].toString();
        if (id.isEmpty() || id == m_sync.clientId())
            return;
        const bool isNew = !m_peers.contains(id);
        Peer &peer = m_peers[id];
        peer.name = p["n"].toString();
        peer.color = p["col"].toString();
        peer.cursor = QPointF(p["x"].toDouble(), p["y"].toDouble());
        peer.hasCursor = p["hc"].toBool();
        peer.chat = p["ch"].toString().left(140);
        peer.selection.clear();
        for (const QJsonValue &v : p["sel"].toArray())
            peer.selection << v.toString();
        peer.seen = m_clock.elapsed();
        if (isNew) {
            emit peersChanged();
            emit message(QStringLiteral("%1 joined").arg(peer.name));
            m_cursorDirty = true;   // introduce ourselves back
        }
        updateCursors();
        return;
    }

    if (type == RelayFrame::Ops || type == RelayFrame::Snapshot) {
        flushLocal();              // our pending edits go out before we merge theirs
        m_applyingRemote = true;
        const bool changed = m_sync.apply(msg["ops"].toArray(), m_canvas->mutableDocument());
        if (changed)
            m_canvas->remoteApplied();
        m_applyingRemote = false;
        if (changed && m_awaitingFirstSync) {
            m_awaitingFirstSync = false;
            m_canvas->clearSelection();
            m_canvas->zoomToFit();     // show the shared board when it first arrives
        }
    }
}

void CollabController::prunePeers()
{
    const qint64 now = m_clock.elapsed();
    bool removed = false;
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (now - it->seen > kPeerTimeoutMs) {
            emit message(QStringLiteral("%1 left").arg(it->name));
            it = m_peers.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed) {
        emit peersChanged();
        updateCursors();
    }
}

void CollabController::updateCursors()
{
    if (!m_canvas)
        return;
    QVariantList list;
    for (const Peer &p : std::as_const(m_peers))
        list << QVariantMap{ { "name", p.name }, { "color", p.color }, { "x", p.cursor.x() }, { "y", p.cursor.y() },
                             { "cursor", p.hasCursor }, { "sel", p.selection }, { "chat", p.chat } };
    m_canvas->setRemoteCursors(list);
}
