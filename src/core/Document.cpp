#include "Document.h"

#include "ShapeLibrary.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QJsonObject>
#include <QLineF>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QtMath>

void Document::clear()
{
    items.clear();
    assets.clear();
    versions.clear();
    m_imageCache.clear();
    title = QStringLiteral("Untitled board");
    owner.clear();
    isPrivate = false;
    layers.clear();
    startView = QRectF();
}

int Document::indexOf(const QString &id) const
{
    for (int i = 0; i < items.size(); ++i)
        if (items[i].id == id)
            return i;
    return -1;
}

const BoardItem *Document::find(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &items[i];
}

BoardItem *Document::find(const QString &id)
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &items[i];
}

QString Document::addAsset(const QByteArray &bytes)
{
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    if (!assets.contains(key)) {
        QByteArray copy = bytes;
        QBuffer buf(&copy);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf);
        Asset a;
        a.data = bytes;
        a.format = reader.format();
        assets.insert(key, a);
    }
    return key;
}

QImage Document::assetImage(const QString &key) const
{
    auto cached = m_imageCache.constFind(key);
    if (cached != m_imageCache.constEnd())
        return *cached;
    const auto it = assets.constFind(key);
    if (it == assets.constEnd())
        return {};
    QImage img = QImage::fromData(it->data);
    m_imageCache.insert(key, img);
    return img;
}

QPainterPath shapeOutline(const BoardItem &item)
{
    QPainterPath path;
    const QRectF r = item.rect;
    switch (item.type) {
    case BoardItem::Type::Ellipse:
        path.addEllipse(r);
        break;
    case BoardItem::Type::Diamond: {
        QPolygonF poly;
        poly << QPointF(r.center().x(), r.top()) << QPointF(r.right(), r.center().y())
             << QPointF(r.center().x(), r.bottom()) << QPointF(r.left(), r.center().y());
        poly << poly.first();
        path.addPolygon(poly);
        break;
    }
    case BoardItem::Type::Triangle: {
        QPolygonF poly;
        poly << QPointF(r.center().x(), r.top()) << r.bottomRight() << r.bottomLeft();
        poly << poly.first();
        path.addPolygon(poly);
        break;
    }
    case BoardItem::Type::Shape: {
        const QPainterPath p = ShapeLibrary::shapePath(item.shapeId, r);
        if (!p.isEmpty())
            return p;
        path.addRect(r);
        break;
    }
    case BoardItem::Type::RoundedRect: {
        const double rad = qMin(16.0, qMin(r.width(), r.height()) / 4.0);
        path.addRoundedRect(r, rad, rad);
        break;
    }
    default:
        path.addRect(r);
        break;
    }
    return path;
}

// Point on the item's outline along the ray from its center towards `towards`.
QPointF clipToShape(const BoardItem &item, const QPointF &towards)
{
    const QRectF r = item.rect;
    const QPointF c = r.center();
    QPointF d = towards - c;
    if (qFuzzyIsNull(d.x()) && qFuzzyIsNull(d.y()))
        return c;
    const double a = qMax(1e-6, r.width() / 2.0);
    const double b = qMax(1e-6, r.height() / 2.0);
    double t;
    switch (item.type) {
    case BoardItem::Type::Ellipse:
        t = 1.0 / std::sqrt((d.x() / a) * (d.x() / a) + (d.y() / b) * (d.y() / b));
        break;
    case BoardItem::Type::Diamond:
        t = 1.0 / (std::abs(d.x()) / a + std::abs(d.y()) / b);
        break;
    default:
        t = 1.0 / qMax(std::abs(d.x()) / a, std::abs(d.y()) / b);
        break;
    }
    t = qMin(t, 1.0); // `towards` inside the shape: stop there
    return c + d * t;
}

QPointF Document::connectorStart(const BoardItem &c) const
{
    const BoardItem *from = c.fromId.isEmpty() ? nullptr : find(c.fromId);
    const BoardItem *to = c.toId.isEmpty() ? nullptr : find(c.toId);
    const QPointF other = to ? to->rect.center() : c.toPoint;
    return from ? clipToShape(*from, other) : c.fromPoint;
}

QPointF Document::connectorEnd(const BoardItem &c) const
{
    const BoardItem *from = c.fromId.isEmpty() ? nullptr : find(c.fromId);
    const BoardItem *to = c.toId.isEmpty() ? nullptr : find(c.toId);
    const QPointF other = from ? from->rect.center() : c.fromPoint;
    return to ? clipToShape(*to, other) : c.toPoint;
}

QPolygonF Document::connectorPath(const BoardItem &c) const
{
    const BoardItem *from = c.fromId.isEmpty() ? nullptr : find(c.fromId);
    const BoardItem *to = c.toId.isEmpty() ? nullptr : find(c.toId);

    if (c.connectorStyle == BoardItem::ConnectorStyle::Straight)
        return QPolygonF() << connectorStart(c) << connectorEnd(c);

    // Elbow: leave/enter on the side facing the other end along the dominant axis.
    const QPointF fromRef = from ? from->rect.center() : c.fromPoint;
    const QPointF toRef = to ? to->rect.center() : c.toPoint;
    const QPointF delta = toRef - fromRef;
    const bool horizontal = std::abs(delta.x()) >= std::abs(delta.y());

    auto sidePoint = [&](const BoardItem *it, const QPointF &ref, bool towardsPositive) {
        if (!it)
            return ref;
        const QRectF r = it->rect;
        if (horizontal)
            return QPointF(towardsPositive ? r.right() : r.left(), r.center().y());
        return QPointF(r.center().x(), towardsPositive ? r.bottom() : r.top());
    };
    const bool positive = horizontal ? delta.x() >= 0 : delta.y() >= 0;
    const QPointF s = sidePoint(from, fromRef, positive);
    const QPointF e = sidePoint(to, toRef, !positive);

    if (c.connectorStyle == BoardItem::ConnectorStyle::Curved) {
        // Smooth S-curve leaving and entering perpendicular to the sides; sampled
        // as a polyline so hit-testing, arrow heads and export reuse the same path.
        const double k = qMax(40.0, (horizontal ? std::abs(e.x() - s.x()) : std::abs(e.y() - s.y())) * 0.5);
        const QPointF dir = horizontal ? QPointF(positive ? k : -k, 0) : QPointF(0, positive ? k : -k);
        const QPointF c1 = from ? s + dir : s + (e - s) / 3.0;
        const QPointF c2 = to ? e - dir : e - (e - s) / 3.0;
        QPolygonF poly;
        const int n = 32;
        for (int i = 0; i <= n; ++i) {
            const double t = double(i) / n, u = 1 - t;
            poly << u * u * u * s + 3 * u * u * t * c1 + 3 * u * t * t * c2 + t * t * t * e;
        }
        return poly;
    }

    QPolygonF poly;
    poly << s;
    if (horizontal) {
        const double midX = (s.x() + e.x()) / 2.0;
        poly << QPointF(midX, s.y()) << QPointF(midX, e.y());
    } else {
        const double midY = (s.y() + e.y()) / 2.0;
        poly << QPointF(s.x(), midY) << QPointF(e.x(), midY);
    }
    poly << e;
    return poly;
}

QRectF Document::itemBounds(const BoardItem &item) const
{
    if (item.type == BoardItem::Type::Connector) {
        const QPolygonF p = connectorPath(item);
        const double pad = item.strokeWidth * 2 + 12; // arrow heads
        return p.boundingRect().adjusted(-pad, -pad, pad, pad);
    }
    const double pad = item.strokeWidth / 2.0 + 1.0;
    QRectF r = item.rect.adjusted(-pad, -pad, pad, pad);
    if (item.type == BoardItem::Type::Frame)
        r.setTop(r.top() - item.fontSize * 2.0); // title above the frame
    if (item.type == BoardItem::Type::Sticky)
        r.adjust(-4, -2, 8, 10);                  // drop shadow
    return r;
}

QRectF Document::contentBounds() const
{
    QRectF r;
    for (const BoardItem &it : items)
        if (it.type != BoardItem::Type::Comment)
            r = r.isNull() ? itemBounds(it) : r.united(itemBounds(it));
    return r;
}

QRectF Document::boundsOf(const QStringList &ids) const
{
    QRectF r;
    for (const QString &id : ids) {
        if (const BoardItem *it = find(id))
            r = r.isNull() ? itemBounds(*it) : r.united(itemBounds(*it));
    }
    return r;
}

namespace {

double distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    if (len2 <= 1e-9)
        return QLineF(p, a).length();
    const double t = qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0);
    return QLineF(p, a + ab * t).length();
}

double distanceToPolyline(const QPointF &p, const QPolygonF &poly)
{
    if (poly.size() == 1)
        return QLineF(p, poly.first()).length();
    double best = std::numeric_limits<double>::max();
    for (qsizetype i = 0; i + 1 < poly.size(); ++i)
        best = qMin(best, distanceToSegment(p, poly[i], poly[i + 1]));
    return best;
}

} // namespace

QString Document::itemAt(const QPointF &p, double tol) const
{
    QString frameHit;
    for (qsizetype i = items.size() - 1; i >= 0; --i) {
        const BoardItem &it = items[i];
        if (!itemPickable(it) || it.type == BoardItem::Type::Comment)
            continue;   // hidden or locked layer; comment pins are hit-tested on screen
        switch (it.type) {
        case BoardItem::Type::Connector:
            if (distanceToPolyline(p, connectorPath(it)) <= tol + it.strokeWidth)
                return it.id;
            break;
        case BoardItem::Type::Pen: {
            QPolygonF poly;
            for (const QPointF &pt : it.points)
                poly << it.rect.topLeft() + pt;
            if (distanceToPolyline(p, poly) <= tol + it.strokeWidth / 2.0)
                return it.id;
            break;
        }
        case BoardItem::Type::Frame: {
            // Title strip selects immediately; the body only if nothing else is hit.
            const QRectF title(it.rect.left(), it.rect.top() - it.fontSize * 2.0,
                               it.rect.width(), it.fontSize * 2.0);
            if (title.contains(p))
                return it.id;
            if (frameHit.isEmpty() && it.rect.adjusted(-tol, -tol, tol, tol).contains(p))
                frameHit = it.id;
            break;
        }
        default:
            if (it.isShape()) {
                if (shapeOutline(it).contains(p))
                    return it.id;
                QPainterPathStroker stroker;
                stroker.setWidth(tol * 2 + it.strokeWidth);
                if (stroker.createStroke(shapeOutline(it)).contains(p))
                    return it.id;
            } else if (it.rect.adjusted(-tol, -tol, tol, tol).contains(p)) {
                return it.id;
            }
            break;
        }
    }
    return frameHit;
}

QStringList Document::itemsIn(const QRectF &r) const
{
    QStringList out;
    for (const BoardItem &it : items)
        if (itemPickable(it) && r.contains(itemBounds(it).adjusted(4, 4, -4, -4)))
            out << it.id;
    return out;
}

QStringList Document::itemsInsideFrame(const BoardItem &frame) const
{
    QStringList out;
    for (const BoardItem &it : items) {
        if (it.id == frame.id || it.type == BoardItem::Type::Connector)
            continue;
        if (frame.rect.contains(it.rect))
            out << it.id;
    }
    return out;
}

QJsonArray Document::itemsToJson() const
{
    QJsonArray a;
    for (const BoardItem &it : items)
        a.append(it.toJson());
    return a;
}

void Document::itemsFromJson(const QJsonArray &a)
{
    items.clear();
    for (const QJsonValue &v : a)
        items.append(BoardItem::fromJson(v.toObject()));
}

// ---------------------------------------------------------------------------
// Layers

int Document::layerIndex(const QString &id) const
{
    if (id.isEmpty())
        return 0;
    for (int i = 0; i < layers.size(); ++i)
        if (layers[i].id == id)
            return i + 1;
    return 0;   // unknown layer -> default
}

bool Document::layerVisible(const QString &id) const
{
    if (id.isEmpty())
        return true;
    for (const Layer &l : layers)
        if (l.id == id)
            return l.visible;
    return true;
}

bool Document::layerLocked(const QString &id) const
{
    if (id.isEmpty())
        return false;
    for (const Layer &l : layers)
        if (l.id == id)
            return l.locked;
    return false;
}

void Document::normalizeLayerOrder()
{
    if (layers.isEmpty())
        return;
    std::stable_sort(items.begin(), items.end(), [this](const BoardItem &a, const BoardItem &b) {
        return layerIndex(a.layer) < layerIndex(b.layer);
    });
}

QJsonArray Document::layersToJson() const
{
    QJsonArray a;
    for (const Layer &l : layers)
        a.append(QJsonObject{ { "id", l.id }, { "name", l.name }, { "visible", l.visible }, { "locked", l.locked } });
    return a;
}

void Document::layersFromJson(const QJsonArray &a)
{
    layers.clear();
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        Layer l;
        l.id = o["id"].toString();
        l.name = o["name"].toString();
        l.visible = o["visible"].toBool(true);
        l.locked = o["locked"].toBool(false);
        if (!l.id.isEmpty())
            layers.append(l);
    }
}
