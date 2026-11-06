#pragma once

#include <QColor>
#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>

// One object on the board. Kept as a plain value type so undo snapshots,
// serialization and rendering stay simple and deterministic.
struct BoardItem
{
    enum class Type {
        Rectangle,
        Ellipse,
        Diamond,
        Triangle,
        RoundedRect,
        Sticky,
        Text,
        Pen,
        Connector,
        Frame,
        Image,
        Shape,      // any outline from ShapeLibrary (shapeId)
        Icon,       // line icon from ShapeLibrary (shapeId)
        Comment,    // comment pin: text = comment, createdBy = author (not exported)
    };

    enum class ConnectorStyle { Straight, Elbow, Curved };

    QString id;
    Type type = Type::Rectangle;
    QRectF rect;                 // world coordinates (1 unit = 1 px at 96 DPI, zoom 100%)

    QColor fill = QColor(255, 255, 255);
    QColor stroke = QColor(26, 26, 26);
    double strokeWidth = 2.0;

    QString text;
    double fontSize = 16.0;
    QColor textColor = QColor(26, 26, 26);
    bool bold = false;
    int textAlign = 0;           // 0 = center, 1 = left, 2 = right

    // Pen strokes: points relative to rect.topLeft().
    QList<QPointF> points;

    // Connectors: endpoints are either attached to items (by id) or free.
    QString fromId;
    QString toId;
    QPointF fromPoint;           // used when fromId is empty
    QPointF toPoint;             // used when toId is empty
    ConnectorStyle connectorStyle = ConnectorStyle::Straight;
    bool arrowEnd = true;
    bool arrowStart = false;
    int dash = 0;                // line pattern: 0 solid, 1 dashed, 2 dotted

    // Images: key into Document::assets (original, untouched file bytes).
    QString assetKey;
    QString altText;                     // accessibility description (exported to SVG)
    QRectF crop = QRectF(0, 0, 1, 1);    // visible part of the image, normalized 0..1
    bool flipH = false;
    bool flipV = false;

    // Library shapes and icons
    QString shapeId;

    // Any item
    QString name;                        // e.g. original image file name
    bool locked = false;                 // can't be moved, resized or deleted
    QString layer;                       // empty = default layer
    QString group;                       // items sharing a group id select and move together
    QString link;                        // URL, or "#<item id>" to jump to another object

    // Info: who created / last changed the item, and when (ms since epoch, UTC)
    QString createdBy;
    qint64 createdAt = 0;
    QString modifiedBy;
    qint64 modifiedAt = 0;

    bool isShape() const;
    bool hasText() const;
    bool isConnector() const { return type == Type::Connector; }

    QJsonObject toJson() const;
    static BoardItem fromJson(const QJsonObject &o);

    static QString typeName(Type t);
    static Type typeFromName(const QString &name);
    static QString newId();
};
