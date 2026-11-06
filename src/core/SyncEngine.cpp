#include "SyncEngine.h"

#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

namespace {

QByteArray compact(const BoardItem &it)
{
    return QJsonDocument(it.toJson()).toJson(QJsonDocument::Compact);
}

} // namespace

SyncEngine::SyncEngine(const QString &clientId)
    : m_client(clientId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(12) : clientId)
{
}

void SyncEngine::reset()
{
    m_clock = 0;
    m_meta.clear();
    m_assets.clear();
    m_title.clear();
    m_titleTs = 0;
    m_titleClient.clear();
    m_layers = "[]";
    m_layersTs = 0;
    m_layersClient.clear();
}

bool SyncEngine::newer(quint64 ts, const QString &client, const Meta &m) const
{
    return ts > m.ts || (ts == m.ts && client > m.client);
}

// Keep existing z keys where they are still in order; give new or moved items a key
// between their neighbours.
void SyncEngine::assignZ(const Document &doc, QHash<QString, double> &zOut)
{
    const auto &items = doc.items;
    double prev = 0;
    bool hasPrev = false;
    auto keeps = [&](const QString &id, double *z) {
        auto m = m_meta.constFind(id);
        if (m == m_meta.constEnd() || m->deleted || m->ts == 0 && m->json.isEmpty())
            return false;
        if (hasPrev && !(m->z > prev))
            return false;
        *z = m->z;
        return true;
    };
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QString &id = items[i].id;
        double z;
        if (keeps(id, &z)) {
            // Out-of-order successors get re-keyed when we reach them.
            zOut.insert(id, z);
            prev = z;
            hasPrev = true;
            continue;
        }
        // Place between the previous key and the next item that keeps its key.
        bool hasNext = false;
        double next = 0;
        for (qsizetype j = i + 1; j < items.size() && !hasNext; ++j)
            hasNext = keeps(items[j].id, &next);
        if (hasPrev && hasNext)
            z = (prev + next) / 2.0;
        else if (hasNext)
            z = next - 1.0;
        else if (hasPrev)
            z = prev + 1.0;
        else
            z = 0.0;
        zOut.insert(id, z);
        prev = z;
        hasPrev = true;
    }
}

void SyncEngine::adopt(const Document &doc)
{
    reset();
    m_title = doc.title;
    m_layers = QJsonDocument(doc.layersToJson()).toJson(QJsonDocument::Compact);
    for (auto it = doc.assets.constBegin(); it != doc.assets.constEnd(); ++it)
        m_assets.insert(it.key());
    double z = 0;
    for (const BoardItem &item : doc.items) {
        Meta &m = m_meta[item.id];
        m.json = compact(item);
        m.z = ++z;
    }
}

QJsonArray SyncEngine::diff(const Document &doc)
{
    QJsonArray ops;

    if (doc.title != m_title) {
        m_title = doc.title;
        m_titleTs = ++m_clock;
        m_titleClient = m_client;
        ops.append(QJsonObject{ { "o", "t" }, { "title", m_title }, { "ts", double(m_titleTs) }, { "c", m_client } });
    }
    const QByteArray layers = QJsonDocument(doc.layersToJson()).toJson(QJsonDocument::Compact);
    if (layers != m_layers) {
        m_layers = layers;
        m_layersTs = ++m_clock;
        m_layersClient = m_client;
        ops.append(QJsonObject{ { "o", "l" }, { "layers", doc.layersToJson() }, { "ts", double(m_layersTs) }, { "c", m_client } });
    }

    for (auto it = doc.assets.constBegin(); it != doc.assets.constEnd(); ++it) {
        if (m_assets.contains(it.key()))
            continue;
        m_assets.insert(it.key());
        ops.append(QJsonObject{ { "o", "a" }, { "key", it.key() }, { "fmt", QString::fromLatin1(it->format) },
                                { "data", QString::fromLatin1(it->data.toBase64()) } });
    }

    QHash<QString, double> z;
    assignZ(doc, z);
    QSet<QString> present;
    for (const BoardItem &item : doc.items) {
        present.insert(item.id);
        const QByteArray json = compact(item);
        const double zi = z.value(item.id);
        Meta &m = m_meta[item.id];
        if (!m.deleted && m.ts > 0 && m.json == json && qFuzzyCompare(m.z + 1.0, zi + 1.0))
            continue;
        m.ts = ++m_clock;
        m.client = m_client;
        m.deleted = false;
        m.z = zi;
        m.json = json;
        ops.append(QJsonObject{ { "o", "u" }, { "id", item.id }, { "item", item.toJson() }, { "z", zi },
                                { "ts", double(m.ts) }, { "c", m_client } });
    }
    for (auto it = m_meta.begin(); it != m_meta.end(); ++it) {
        if (it->deleted || present.contains(it.key()))
            continue;
        it->deleted = true;
        it->ts = ++m_clock;
        it->client = m_client;
        it->json.clear();
        ops.append(QJsonObject{ { "o", "d" }, { "id", it.key() }, { "ts", double(it->ts) }, { "c", m_client } });
    }
    return ops;
}

bool SyncEngine::apply(const QJsonArray &ops, Document &doc)
{
    bool changed = false;
    bool reorder = false;
    QHash<QString, int> index;
    for (int i = 0; i < doc.items.size(); ++i)
        index.insert(doc.items[i].id, i);

    for (const QJsonValue &v : ops) {
        const QJsonObject op = v.toObject();
        const QString kind = op["o"].toString();
        const quint64 ts = quint64(op["ts"].toDouble());
        const QString client = op["c"].toString();
        m_clock = qMax(m_clock, ts);

        if (kind == QLatin1String("a")) {
            const QString key = op["key"].toString();
            m_assets.insert(key);
            if (!doc.assets.contains(key)) {
                Asset a;
                a.data = QByteArray::fromBase64(op["data"].toString().toLatin1());
                a.format = op["fmt"].toString().toLatin1();
                doc.assets.insert(key, a);
                changed = true;
            }
        } else if (kind == QLatin1String("l")) {
            if (ts > m_layersTs || (ts == m_layersTs && client > m_layersClient)) {
                m_layersTs = ts;
                m_layersClient = client;
                const QJsonArray arr = op["layers"].toArray();
                m_layers = QJsonDocument(arr).toJson(QJsonDocument::Compact);
                doc.layersFromJson(arr);
                reorder = true;
                changed = true;
            }
        } else if (kind == QLatin1String("t")) {
            if (ts > m_titleTs || (ts == m_titleTs && client > m_titleClient)) {
                m_titleTs = ts;
                m_titleClient = client;
                m_title = op["title"].toString();
                if (doc.title != m_title) {
                    doc.title = m_title;
                    changed = true;
                }
            }
        } else if (kind == QLatin1String("u") || kind == QLatin1String("d")) {
            const QString id = op["id"].toString();
            Meta &m = m_meta[id];
            if (!newer(ts, client, m))
                continue;   // we already have a newer write for this item
            m.ts = ts;
            m.client = client;
            if (kind == QLatin1String("d")) {
                m.deleted = true;
                m.json.clear();
                const int i = index.value(id, -1);
                if (i >= 0) {
                    doc.items.removeAt(i);
                    index.clear();
                    for (int k = 0; k < doc.items.size(); ++k)
                        index.insert(doc.items[k].id, k);
                    changed = true;
                }
            } else {
                BoardItem item = BoardItem::fromJson(op["item"].toObject());
                item.id = id;
                m.deleted = false;
                m.z = op["z"].toDouble();
                m.json = compact(item);
                const int i = index.value(id, -1);
                if (i >= 0) {
                    doc.items[i] = item;
                } else {
                    index.insert(id, int(doc.items.size()));
                    doc.items.append(item);
                }
                reorder = true;
                changed = true;
            }
        }
    }

    if (reorder) {
        std::stable_sort(doc.items.begin(), doc.items.end(), [this](const BoardItem &a, const BoardItem &b) {
            return m_meta.value(a.id).z < m_meta.value(b.id).z;
        });
        doc.normalizeLayerOrder();
    }
    if (changed)
        doc.clearImageCache();
    return changed;
}

QJsonArray SyncEngine::snapshot(const Document &doc) const
{
    QJsonArray ops;
    ops.append(QJsonObject{ { "o", "t" }, { "title", doc.title }, { "ts", double(m_titleTs) }, { "c", m_titleClient } });
    ops.append(QJsonObject{ { "o", "l" }, { "layers", doc.layersToJson() }, { "ts", double(m_layersTs) }, { "c", m_layersClient } });
    for (auto it = doc.assets.constBegin(); it != doc.assets.constEnd(); ++it)
        ops.append(QJsonObject{ { "o", "a" }, { "key", it.key() }, { "fmt", QString::fromLatin1(it->format) },
                                { "data", QString::fromLatin1(it->data.toBase64()) } });
    for (const BoardItem &item : doc.items) {
        const Meta m = m_meta.value(item.id);
        ops.append(QJsonObject{ { "o", "u" }, { "id", item.id }, { "item", item.toJson() }, { "z", m.z },
                                { "ts", double(m.ts) }, { "c", m.client } });
    }
    // Deletions, so stale copies of removed items don't come back.
    for (auto it = m_meta.constBegin(); it != m_meta.constEnd(); ++it)
        if (it->deleted)
            ops.append(QJsonObject{ { "o", "d" }, { "id", it.key() }, { "ts", double(it->ts) }, { "c", it->client } });
    return ops;
}
