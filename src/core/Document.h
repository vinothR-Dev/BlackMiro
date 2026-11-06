#pragma once

#include "BoardItem.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QList>
#include <QPainterPath>
#include <QPolygonF>

// Original image bytes exactly as imported. Never re-encoded unless an export
// with a lossy compression level explicitly asks for it.
struct Asset
{
    QByteArray data;
    QByteArray format;   // "png", "jpeg", "webp", ... as detected by QImageReader
};

// Layers sit above the default layer (id ""), in list order. Hidden layers are not drawn
// or exported; items on locked layers can't be selected.
struct Layer
{
    QString id;
    QString name;
    bool visible = true;
    bool locked = false;
};

struct BoardVersion
{
    QDateTime timestamp;
    QJsonArray items;
};

class Document
{
public:
    QList<BoardItem> items;                // z-order: index 0 is at the back
    QHash<QString, Asset> assets;
    QList<BoardVersion> versions;          // saved snapshots (version history)
    QString title = QStringLiteral("Untitled board");
    QString owner;                         // email of the local profile that created it
    bool isPrivate = false;                // saved encrypted; PDF exports password-protected
    QList<Layer> layers;
    QRectF startView;                      // world rect shown when the board opens (empty = fit all)

    void clear();

    // Layers
    int layerIndex(const QString &id) const;   // 0 = default layer, then 1..n
    bool layerVisible(const QString &id) const;
    bool layerLocked(const QString &id) const;
    bool itemVisible(const BoardItem &item) const { return layerVisible(item.layer); }
    bool itemPickable(const BoardItem &item) const { return layerVisible(item.layer) && !layerLocked(item.layer); }
    void normalizeLayerOrder();                // keep items grouped by layer (stable)
    QJsonArray layersToJson() const;
    void layersFromJson(const QJsonArray &a);

    int indexOf(const QString &id) const;
    const BoardItem *find(const QString &id) const;
    BoardItem *find(const QString &id);

    // Image assets
    QString addAsset(const QByteArray &bytes);           // returns key (sha256)
    QImage assetImage(const QString &key) const;         // decoded, cached
    void clearImageCache() { m_imageCache.clear(); }

    // Geometry
    QRectF itemBounds(const BoardItem &item) const;      // includes stroke/arrows
    QRectF contentBounds() const;
    QRectF boundsOf(const QStringList &ids) const;
    QPolygonF connectorPath(const BoardItem &connector) const;
    QPointF connectorStart(const BoardItem &connector) const;
    QPointF connectorEnd(const BoardItem &connector) const;

    // Hit testing (world coordinates); tolerance in world units.
    QString itemAt(const QPointF &p, double tolerance) const;
    QStringList itemsIn(const QRectF &r) const;          // fully contained
    QStringList itemsInsideFrame(const BoardItem &frame) const;

    QJsonArray itemsToJson() const;
    void itemsFromJson(const QJsonArray &a);

private:
    mutable QHash<QString, QImage> m_imageCache;
};

// Shared shape outline used by renderer, hit testing and SVG export.
QPainterPath shapeOutline(const BoardItem &item);
QPointF clipToShape(const BoardItem &item, const QPointF &towards);
