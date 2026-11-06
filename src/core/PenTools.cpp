#include "PenTools.h"

#include <QLineF>

#include <cmath>

namespace PenTools {

namespace {

double segmentDistance(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double len2 = QPointF::dotProduct(ab, ab);
    if (len2 <= 1e-12)
        return QLineF(p, a).length();
    const double t = qBound(0.0, QPointF::dotProduct(p - a, ab) / len2, 1.0);
    return QLineF(p, a + ab * t).length();
}

void rdp(const QList<QPointF> &pts, int first, int last, double eps, QList<bool> &keep)
{
    if (last <= first + 1)
        return;
    double maxD = -1;
    int index = first;
    for (int i = first + 1; i < last; ++i) {
        const double d = segmentDistance(pts[i], pts[first], pts[last]);
        if (d > maxD) {
            maxD = d;
            index = i;
        }
    }
    if (maxD > eps) {
        keep[index] = true;
        rdp(pts, first, index, eps, keep);
        rdp(pts, index, last, eps, keep);
    }
}

} // namespace

QList<QPointF> simplify(const QList<QPointF> &points, double epsilon)
{
    if (points.size() < 3)
        return points;
    QList<bool> keep(points.size(), false);
    keep.first() = keep.last() = true;
    rdp(points, 0, int(points.size()) - 1, epsilon, keep);
    QList<QPointF> out;
    for (int i = 0; i < points.size(); ++i)
        if (keep[i])
            out << points[i];
    return out;
}

Recognized recognize(const QList<QPointF> &points)
{
    Recognized r;
    if (points.size() < 4)
        return r;
    const QRectF box = QPolygonF(points).boundingRect();
    const double diag = std::hypot(box.width(), box.height());
    if (diag < 24)
        return r;

    const bool closed = QLineF(points.first(), points.last()).length() < 0.22 * diag;
    QList<QPointF> s = simplify(points, 0.07 * diag);

    if (!closed) {
        // Straight enough to be a line?
        double maxDev = 0;
        for (const QPointF &p : points)
            maxDev = qMax(maxDev, segmentDistance(p, points.first(), points.last()));
        if (maxDev < 0.06 * diag) {
            r.kind = Recognized::Line;
            r.from = points.first();
            r.to = points.last();
        }
        return r;
    }

    if (s.size() > 2 && QLineF(s.first(), s.last()).length() < 0.22 * diag)
        s.removeLast();   // closing vertex duplicates the start

    // Ellipse: every point near the ellipse inscribed in the bounding box.
    const QPointF c = box.center();
    const double a = qMax(1.0, box.width() / 2), b = qMax(1.0, box.height() / 2);
    double dev = 0;
    for (const QPointF &p : points) {
        const double dx = (p.x() - c.x()) / a, dy = (p.y() - c.y()) / b;
        dev += std::abs(std::sqrt(dx * dx + dy * dy) - 1.0);
    }
    dev /= points.size();

    r.rect = box;
    if (s.size() == 3) {
        r.kind = Recognized::Triangle;
    } else if (s.size() == 4) {
        // Diamond if the corners sit near the middles of the box edges.
        const QPointF mids[4] = { QPointF(c.x(), box.top()), QPointF(box.right(), c.y()),
                                  QPointF(c.x(), box.bottom()), QPointF(box.left(), c.y()) };
        int nearMid = 0;
        for (const QPointF &v : s)
            for (const QPointF &m : mids)
                if (QLineF(v, m).length() < 0.2 * diag) { ++nearMid; break; }
        r.kind = nearMid >= 4 ? Recognized::Diamond : Recognized::Rectangle;
    } else if (dev < 0.16) {
        r.kind = Recognized::Ellipse;
    } else if (s.size() == 5 || s.size() == 6) {
        r.kind = Recognized::Rectangle;   // a box with a wobbly corner
    }
    return r;
}

QList<QList<QPointF>> eraseAround(const QList<QPointF> &points, const QPointF &center, double radius)
{
    QList<QList<QPointF>> pieces;
    QList<QPointF> current;
    for (qsizetype i = 0; i < points.size(); ++i) {
        const bool erased = QLineF(points[i], center).length() <= radius
            || (i > 0 && segmentDistance(center, points[i - 1], points[i]) <= radius
                && QLineF(points[i - 1], center).length() <= radius * 1.5);
        if (erased) {
            if (current.size() >= 2)
                pieces << current;
            current.clear();
        } else {
            current << points[i];
        }
    }
    if (current.size() >= 2)
        pieces << current;
    return pieces;
}

bool strokeHits(const QList<QPointF> &points, const QPointF &center, double radius)
{
    if (points.size() == 1)
        return QLineF(points.first(), center).length() <= radius;
    for (qsizetype i = 0; i + 1 < points.size(); ++i)
        if (segmentDistance(center, points[i], points[i + 1]) <= radius)
            return true;
    return false;
}

} // namespace PenTools
