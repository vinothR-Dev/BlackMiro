#pragma once

#include "Document.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QString>

// Live-collaboration merge logic (no networking here).
//
// Every item is replicated as a whole: the newest write wins, ordered by a Lamport
// clock with the client id as tie-breaker, so all peers converge on the same board.
// Z-order uses fractional keys so moving one item to the front sends one op, not N.
//
// Ops (JSON):
//   {"o":"u","id","item":{…},"z":real,"ts":n,"c":client}   upsert
//   {"o":"d","id","ts":n,"c":client}                       delete
//   {"o":"a","key","fmt","data":base64}                    image asset (original bytes)
//   {"o":"t","title","ts":n,"c":client}                    board title
class SyncEngine
{
public:
    explicit SyncEngine(const QString &clientId = {});

    QString clientId() const { return m_client; }
    void reset();

    // Treat `doc` as already in sync without sending anything (with the lowest
    // priority, so anything a peer sends wins). Used right before joining a room.
    void adopt(const Document &doc);

    // Local edits: ops that bring peers from the last synced state to `doc`.
    QJsonArray diff(const Document &doc);

    // Remote ops: merge into `doc`. Returns true if the board changed.
    bool apply(const QJsonArray &ops, Document &doc);

    // Full state, for peers that join later (replaces the relay's op log).
    QJsonArray snapshot(const Document &doc) const;

private:
    struct Meta
    {
        quint64 ts = 0;
        QString client;
        bool deleted = false;
        double z = 0;
        QByteArray json;          // last synced item JSON (compact)
    };

    bool newer(quint64 ts, const QString &client, const Meta &m) const;
    void assignZ(const Document &doc, QHash<QString, double> &zOut);

    QString m_client;
    quint64 m_clock = 0;
    QHash<QString, Meta> m_meta;
    QSet<QString> m_assets;       // asset keys already known to peers
    QString m_title;
    quint64 m_titleTs = 0;
    QString m_titleClient;
    QByteArray m_layers = "[]";
    quint64 m_layersTs = 0;
    QString m_layersClient;
};
