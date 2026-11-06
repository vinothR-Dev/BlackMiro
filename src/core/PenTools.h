#pragma once

#include <QList>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace PenTools {

// Smart drawing: turn a freehand stroke into a clean shape when it clearly is one.
struct Recognized
{
    enum Kind { None, Line, Rectangle, Ellipse, Triangle, Diamond } kind = None;
    QRectF rect;          // shapes
    QPointF from, to;     // lines
};
Recognized recognize(const QList<QPointF> &points);

// Ramer–Douglas–Peucker simplification.
QList<QPointF> simplify(const QList<QPointF> &points, double epsilon);

// Precision eraser: remove the parts of a stroke within `radius` of `center`.
// Returns the remaining pieces (each with at least 2 points).
QList<QList<QPointF>> eraseAround(const QList<QPointF> &points, const QPointF &center, double radius);

// True if any segment of the stroke passes within `radius` of `center`.
bool strokeHits(const QList<QPointF> &points, const QPointF &center, double radius);

} // namespace PenTools
