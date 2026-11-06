#include "SvgPath.h"

#include <QtMath>

#include <cmath>

namespace SvgPath {

namespace {

class Lexer
{
public:
    explicit Lexer(const QString &s) : m_s(s) {}

    void skip()
    {
        while (m_i < m_s.size() && (m_s[m_i].isSpace() || m_s[m_i] == QLatin1Char(',')))
            ++m_i;
    }
    bool atEnd() { skip(); return m_i >= m_s.size(); }
    bool nextIsNumber()
    {
        skip();
        if (m_i >= m_s.size())
            return false;
        const QChar c = m_s[m_i];
        return c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('.');
    }
    QChar command()
    {
        skip();
        return m_i < m_s.size() ? m_s[m_i++] : QChar();
    }
    double number()
    {
        skip();
        const qsizetype start = m_i;
        if (m_i < m_s.size() && (m_s[m_i] == QLatin1Char('-') || m_s[m_i] == QLatin1Char('+')))
            ++m_i;
        bool dot = false, exp = false;
        while (m_i < m_s.size()) {
            const QChar c = m_s[m_i];
            if (c.isDigit()) { ++m_i; continue; }
            if (c == QLatin1Char('.') && !dot && !exp) { dot = true; ++m_i; continue; }
            if ((c == QLatin1Char('e') || c == QLatin1Char('E')) && !exp) {
                exp = true;
                ++m_i;
                if (m_i < m_s.size() && (m_s[m_i] == QLatin1Char('-') || m_s[m_i] == QLatin1Char('+')))
                    ++m_i;
                continue;
            }
            break;
        }
        return m_s.mid(start, m_i - start).toDouble();
    }
    bool flag()   // arc flags may be written without separators: "a5 5 0 01 5 5"
    {
        skip();
        if (m_i < m_s.size() && (m_s[m_i] == QLatin1Char('0') || m_s[m_i] == QLatin1Char('1')))
            return m_s[m_i++] == QLatin1Char('1');
        return number() != 0;
    }

private:
    const QString &m_s;
    qsizetype m_i = 0;
};

// SVG elliptical arc -> cubic Béziers (SVG 1.1 implementation notes, F.6.5).
void arcTo(QPainterPath &path, QPointF p0, double rx, double ry, double phiDeg, bool largeArc, bool sweep, QPointF p1)
{
    if (qFuzzyCompare(p0.x(), p1.x()) && qFuzzyCompare(p0.y(), p1.y()))
        return;
    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx < 1e-9 || ry < 1e-9) {
        path.lineTo(p1);
        return;
    }
    const double phi = qDegreesToRadians(phiDeg);
    const double cosP = std::cos(phi), sinP = std::sin(phi);
    const double dx = (p0.x() - p1.x()) / 2, dy = (p0.y() - p1.y()) / 2;
    const double x1p = cosP * dx + sinP * dy;
    const double y1p = -sinP * dx + cosP * dy;
    const double lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (lambda > 1) {
        rx *= std::sqrt(lambda);
        ry *= std::sqrt(lambda);
    }
    const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
    const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
    double coef = den > 0 ? std::sqrt(qMax(0.0, num / den)) : 0;
    if (largeArc == sweep)
        coef = -coef;
    const double cxp = coef * (rx * y1p / ry);
    const double cyp = coef * -(ry * x1p / rx);
    const double cx = cosP * cxp - sinP * cyp + (p0.x() + p1.x()) / 2;
    const double cy = sinP * cxp + cosP * cyp + (p0.y() + p1.y()) / 2;

    auto angle = [](double ux, double uy, double vx, double vy) {
        const double a = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        return a;
    };
    const double theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    double dTheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
    if (!sweep && dTheta > 0)
        dTheta -= 2 * M_PI;
    else if (sweep && dTheta < 0)
        dTheta += 2 * M_PI;

    const int segments = qMax(1, int(std::ceil(std::abs(dTheta) / (M_PI / 2) - 1e-9)));
    const double delta = dTheta / segments;
    const double t = 4.0 / 3.0 * std::tan(delta / 4);
    double th = theta1;
    auto point = [&](double, double ex, double ey) {
        return QPointF(cx + cosP * ex - sinP * ey, cy + sinP * ex + cosP * ey);
    };
    for (int i = 0; i < segments; ++i) {
        const double cos1 = std::cos(th), sin1 = std::sin(th);
        const double th2 = th + delta;
        const double cos2 = std::cos(th2), sin2 = std::sin(th2);
        const QPointF c1 = point(th, rx * (cos1 - t * sin1), ry * (sin1 + t * cos1));
        const QPointF c2 = point(th2, rx * (cos2 + t * sin2), ry * (sin2 - t * cos2));
        const QPointF e = i == segments - 1 ? p1 : point(th2, rx * cos2, ry * sin2);
        path.cubicTo(c1, c2, e);
        th = th2;
    }
}

} // namespace

QPainterPath parse(const QString &data)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    Lexer lx(data);
    QPointF cur, start, lastCtrl;
    QChar cmd, prevCmd;
    while (!lx.atEnd()) {
        if (!lx.nextIsNumber())
            cmd = lx.command();
        else if (cmd.isNull())
            break;
        const bool rel = cmd.isLower();
        const QChar c = cmd.toUpper();
        const QPointF base = rel ? cur : QPointF();
        if (c == QLatin1Char('M')) {
            const double x = lx.number(), y = lx.number();
            cur = base + QPointF(x, y);
            path.moveTo(cur);
            start = cur;
            cmd = rel ? QLatin1Char('l') : QLatin1Char('L');   // further pairs are line-tos
        } else if (c == QLatin1Char('L')) {
            const double x = lx.number(), y = lx.number();
            cur = base + QPointF(x, y);
            path.lineTo(cur);
        } else if (c == QLatin1Char('H')) {
            const double x = lx.number();
            cur.setX(rel ? cur.x() + x : x);
            path.lineTo(cur);
        } else if (c == QLatin1Char('V')) {
            const double y = lx.number();
            cur.setY(rel ? cur.y() + y : y);
            path.lineTo(cur);
        } else if (c == QLatin1Char('C')) {
            const double x1 = lx.number(), y1 = lx.number(), x2 = lx.number(), y2 = lx.number(), x = lx.number(), y = lx.number();
            const QPointF c1 = base + QPointF(x1, y1), c2 = base + QPointF(x2, y2);
            cur = base + QPointF(x, y);
            path.cubicTo(c1, c2, cur);
            lastCtrl = c2;
        } else if (c == QLatin1Char('S')) {
            const double x2 = lx.number(), y2 = lx.number(), x = lx.number(), y = lx.number();
            const QChar p = prevCmd.toUpper();
            const QPointF c1 = (p == QLatin1Char('C') || p == QLatin1Char('S')) ? cur * 2 - lastCtrl : cur;
            const QPointF c2 = base + QPointF(x2, y2);
            cur = base + QPointF(x, y);
            path.cubicTo(c1, c2, cur);
            lastCtrl = c2;
        } else if (c == QLatin1Char('Q')) {
            const double x1 = lx.number(), y1 = lx.number(), x = lx.number(), y = lx.number();
            const QPointF c1 = base + QPointF(x1, y1);
            cur = base + QPointF(x, y);
            path.quadTo(c1, cur);
            lastCtrl = c1;
        } else if (c == QLatin1Char('T')) {
            const double x = lx.number(), y = lx.number();
            const QChar p = prevCmd.toUpper();
            const QPointF c1 = (p == QLatin1Char('Q') || p == QLatin1Char('T')) ? cur * 2 - lastCtrl : cur;
            cur = base + QPointF(x, y);
            path.quadTo(c1, cur);
            lastCtrl = c1;
        } else if (c == QLatin1Char('A')) {
            const double rx = lx.number(), ry = lx.number(), rot = lx.number();
            const bool large = lx.flag(), sweep = lx.flag();
            const double x = lx.number(), y = lx.number();
            const QPointF end = base + QPointF(x, y);
            arcTo(path, cur, rx, ry, rot, large, sweep, end);
            cur = end;
        } else if (c == QLatin1Char('Z')) {
            path.closeSubpath();
            cur = start;
            prevCmd = cmd;
            cmd = QChar();   // numbers right after Z are invalid: stop instead of looping
            continue;
        } else {
            break;   // unknown command
        }
        prevCmd = cmd;
    }
    return path;
}

} // namespace SvgPath
