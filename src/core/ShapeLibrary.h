#pragma once

#include <QList>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QStringList>

// Built-in vector libraries.
//  - Shapes: filled outlines drawn in a 100×100 box, stretched to the item's rectangle.
//  - Icons:  stroked line icons drawn in a 24×24 box (hand-drawn for BlackMiro).
// Both are stored by id in boards, and render as true vectors on screen, PDF and SVG.
namespace ShapeLibrary {

struct Entry
{
    QString id;
    QString name;
    QString category;
    QString path;      // SVG path data
    QString keywords;  // extra search words
};

const QList<Entry> &shapes();
const QList<Entry> &icons();
QStringList shapeCategories();
QStringList iconCategories();
const Entry *findShape(const QString &id);
const Entry *findIcon(const QString &id);

QPainterPath shapePath(const QString &id, const QRectF &rect);   // empty path for unknown ids
QPainterPath iconPath(const QString &id, const QRectF &rect);    // keeps aspect, centred

// Emoji palette for the stickers panel: {category, list of emoji}
struct EmojiGroup { QString category; QStringList emoji; };
const QList<EmojiGroup> &emoji();

} // namespace ShapeLibrary
