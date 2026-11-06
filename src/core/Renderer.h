#pragma once

#include "Document.h"

#include <QFont>
#include <QList>
#include <QPainter>

struct TextLine
{
    QString text;
    QPointF baseline;   // left end of the baseline, world coordinates
    double width = 0;
};

struct TextLayoutResult
{
    QFont font;
    QList<TextLine> lines;
    double contentHeight = 0;
};

namespace Renderer {

struct Options
{
    bool drawFrameTitles = true;
    bool losslessImages = true;     // sets QPainter::LosslessImageRendering (PDF: Flate, not JPEG)
    QStringList hideTextIds;        // item currently being text-edited on screen
    QColor frameTitleColor = QColor(80, 80, 80);   // lighter on a dark screen background
};

// Draws every item intersecting `worldClip`. The painter's transform must map
// world coordinates to the device. The same code path renders the on-screen
// canvas and every export, so exports always match what you see.
void paintDocument(QPainter &p, const Document &doc, const QRectF &worldClip, const Options &opt = {});
void paintItem(QPainter &p, const Document &doc, const BoardItem &item, const Options &opt = {});

QFont fontFor(const BoardItem &item);
// Colour-emoji font family: the system's own (Segoe UI Emoji, Apple Color Emoji,
// Noto Color Emoji, ...) when installed, else BlackMiro's bundled emoji font
// (:/fonts/emoji.ttf, used by the browser build). Empty when neither exists.
QString emojiFamily();
QRectF textRect(const BoardItem &item);
TextLayoutResult layoutText(const BoardItem &item);
QPainterPath penPath(const BoardItem &item);
QPolygonF arrowHead(const QPointF &tip, const QPointF &from, double strokeWidth);
QPointF connectorLabelPoint(const QPolygonF &path);   // halfway along the line

} // namespace Renderer
