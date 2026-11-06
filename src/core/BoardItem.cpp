#include "BoardItem.h"

#include <QJsonArray>
#include <QUuid>

namespace {

QString colorToString(const QColor &c)
{
    return c.name(QColor::HexArgb);
}

QColor colorFromJson(const QJsonValue &v, const QColor &fallback)
{
    if (!v.isString())
        return fallback;
    QColor c = QColor::fromString(v.toString());
    return c.isValid() ? c : fallback;
}

QJsonArray rectToJson(const QRectF &r)
{
    return { r.x(), r.y(), r.width(), r.height() };
}

QRectF rectFromJson(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    if (a.size() != 4)
        return {};
    return { a[0].toDouble(), a[1].toDouble(), a[2].toDouble(), a[3].toDouble() };
}

QJsonArray pointToJson(const QPointF &p)
{
    return { p.x(), p.y() };
}

QPointF pointFromJson(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    if (a.size() != 2)
        return {};
    return { a[0].toDouble(), a[1].toDouble() };
}

struct TypeName { BoardItem::Type type; const char *name; };
constexpr TypeName kTypeNames[] = {
    { BoardItem::Type::Rectangle, "rectangle" },
    { BoardItem::Type::Ellipse, "ellipse" },
    { BoardItem::Type::Diamond, "diamond" },
    { BoardItem::Type::Triangle, "triangle" },
    { BoardItem::Type::RoundedRect, "rounded" },
    { BoardItem::Type::Sticky, "sticky" },
    { BoardItem::Type::Text, "text" },
    { BoardItem::Type::Pen, "pen" },
    { BoardItem::Type::Connector, "connector" },
    { BoardItem::Type::Frame, "frame" },
    { BoardItem::Type::Image, "image" },
    { BoardItem::Type::Shape, "shape" },
    { BoardItem::Type::Icon, "icon" },
    { BoardItem::Type::Comment, "comment" },
};

} // namespace

bool BoardItem::isShape() const
{
    switch (type) {
    case Type::Rectangle:
    case Type::Ellipse:
    case Type::Diamond:
    case Type::Triangle:
    case Type::RoundedRect:
    case Type::Shape:
        return true;
    default:
        return false;
    }
}

bool BoardItem::hasText() const
{
    return isShape() || type == Type::Sticky || type == Type::Text || type == Type::Frame;
}

QString BoardItem::typeName(Type t)
{
    for (const auto &tn : kTypeNames)
        if (tn.type == t)
            return QString::fromLatin1(tn.name);
    return QStringLiteral("rectangle");
}

BoardItem::Type BoardItem::typeFromName(const QString &name)
{
    for (const auto &tn : kTypeNames)
        if (name == QLatin1String(tn.name))
            return tn.type;
    return Type::Rectangle;
}

QString BoardItem::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject BoardItem::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["type"] = typeName(type);
    o["rect"] = rectToJson(rect);
    o["fill"] = colorToString(fill);
    o["stroke"] = colorToString(stroke);
    o["strokeWidth"] = strokeWidth;
    if (!text.isEmpty())
        o["text"] = text;
    o["fontSize"] = fontSize;
    o["textColor"] = colorToString(textColor);
    if (bold)
        o["bold"] = true;
    if (textAlign != 0)
        o["textAlign"] = textAlign;

    if (!points.isEmpty()) {
        QJsonArray pts;
        for (const QPointF &p : points) {
            pts.append(p.x());
            pts.append(p.y());
        }
        o["points"] = pts;
    }

    if (dash != 0)
        o["dash"] = dash;
    if (type == Type::Connector) {
        o["fromId"] = fromId;
        o["toId"] = toId;
        o["fromPoint"] = pointToJson(fromPoint);
        o["toPoint"] = pointToJson(toPoint);
        o["connectorStyle"] = connectorStyle == ConnectorStyle::Elbow ? "elbow"
                             : connectorStyle == ConnectorStyle::Curved ? "curved" : "straight";
        o["arrowEnd"] = arrowEnd;
        o["arrowStart"] = arrowStart;
    }

    if (!assetKey.isEmpty())
        o["asset"] = assetKey;
    if (!altText.isEmpty())
        o["alt"] = altText;
    if (crop != QRectF(0, 0, 1, 1))
        o["crop"] = rectToJson(crop);
    if (flipH)
        o["flipH"] = true;
    if (flipV)
        o["flipV"] = true;
    if (!name.isEmpty())
        o["name"] = name;
    if (locked)
        o["locked"] = true;
    if (!shapeId.isEmpty())
        o["shape"] = shapeId;
    if (!layer.isEmpty())
        o["layer"] = layer;
    if (!group.isEmpty())
        o["group"] = group;
    if (!link.isEmpty())
        o["link"] = link;
    if (!createdBy.isEmpty())
        o["cb"] = createdBy;
    if (createdAt)
        o["ca"] = double(createdAt);
    if (!modifiedBy.isEmpty())
        o["mb"] = modifiedBy;
    if (modifiedAt)
        o["ma"] = double(modifiedAt);
    return o;
}

BoardItem BoardItem::fromJson(const QJsonObject &o)
{
    BoardItem it;
    it.id = o["id"].toString();
    if (it.id.isEmpty())
        it.id = newId();
    it.type = typeFromName(o["type"].toString());
    it.rect = rectFromJson(o["rect"]);
    it.fill = colorFromJson(o["fill"], it.fill);
    it.stroke = colorFromJson(o["stroke"], it.stroke);
    it.strokeWidth = o["strokeWidth"].toDouble(it.strokeWidth);
    it.text = o["text"].toString();
    it.fontSize = o["fontSize"].toDouble(it.fontSize);
    it.textColor = colorFromJson(o["textColor"], it.textColor);
    it.bold = o["bold"].toBool(false);
    it.textAlign = o["textAlign"].toInt(0);

    const QJsonArray pts = o["points"].toArray();
    for (qsizetype i = 0; i + 1 < pts.size(); i += 2)
        it.points.append(QPointF(pts[i].toDouble(), pts[i + 1].toDouble()));

    it.fromId = o["fromId"].toString();
    it.toId = o["toId"].toString();
    it.fromPoint = pointFromJson(o["fromPoint"]);
    it.toPoint = pointFromJson(o["toPoint"]);
    {
        const QString cs = o["connectorStyle"].toString();
        it.connectorStyle = cs == QLatin1String("elbow") ? ConnectorStyle::Elbow
                          : cs == QLatin1String("curved") ? ConnectorStyle::Curved : ConnectorStyle::Straight;
    }
    it.dash = qBound(0, o["dash"].toInt(0), 2);
    it.arrowEnd = o["arrowEnd"].toBool(true);
    it.arrowStart = o["arrowStart"].toBool(false);
    it.assetKey = o["asset"].toString();
    it.altText = o["alt"].toString();
    if (o.contains("crop")) {
        const QRectF c = rectFromJson(o["crop"]).intersected(QRectF(0, 0, 1, 1));
        if (c.width() > 0 && c.height() > 0)
            it.crop = c;
    }
    it.flipH = o["flipH"].toBool();
    it.flipV = o["flipV"].toBool();
    it.name = o["name"].toString();
    it.locked = o["locked"].toBool();
    it.shapeId = o["shape"].toString();
    it.layer = o["layer"].toString();
    it.group = o["group"].toString();
    it.link = o["link"].toString();
    it.createdBy = o["cb"].toString();
    it.createdAt = qint64(o["ca"].toDouble());
    it.modifiedBy = o["mb"].toString();
    it.modifiedAt = qint64(o["ma"].toDouble());
    return it;
}
