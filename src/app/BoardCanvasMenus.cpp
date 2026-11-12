// Miro-style right-click menu actions, board settings, snapping, comments,
// links, groups and the created / modified stamps shown in Info.
#include "BoardCanvas.h"

#include "BoardFile.h"
#include "Exporter.h"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kGrid = 24.0;           // same spacing as the dot grid
constexpr double kSnapPx = 6.0;          // snap distance in screen pixels
constexpr double kMinZoomValue = 0.02;   // same limits as BoardCanvas::setZoomAt
constexpr double kMaxZoomValue = 40.0;
constexpr double kPin = 30.0;            // comment pin size in screen pixels
const QColor kGuide(236, 64, 122);

QString appDir(const QString &sub)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1Char('/') + sub;
    QDir().mkpath(dir);
    return dir;
}

QString safeFileName(QString name)
{
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]+")), QStringLiteral("-"));
    return name.isEmpty() ? QStringLiteral("Untitled") : name.left(80);
}

QString formatTime(qint64 ms)
{
    if (ms <= 0)
        return {};
    const QDateTime t = QDateTime::fromMSecsSinceEpoch(ms).toLocalTime();
    const QDateTime now = QDateTime::currentDateTime();
    const qint64 secs = t.secsTo(now);
    if (secs < 60)
        return QStringLiteral("just now");
    if (secs < 3600)
        return QStringLiteral("%1 min ago").arg(secs / 60);
    if (t.date() == now.date())
        return QStringLiteral("today, %1").arg(QLocale().toString(t.time(), QLocale::ShortFormat));
    if (t.date() == now.date().addDays(-1))
        return QStringLiteral("yesterday, %1").arg(QLocale().toString(t.time(), QLocale::ShortFormat));
    return QLocale().toString(t, QLocale::ShortFormat);
}

size_t itemSignature(const BoardItem &it)
{
    QJsonObject o = it.toJson();
    o.remove(QStringLiteral("cb"));
    o.remove(QStringLiteral("ca"));
    o.remove(QStringLiteral("mb"));
    o.remove(QStringLiteral("ma"));
    return qHash(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

// ---------------------------------------------------------------------------
// Settings

void BoardCanvas::loadSettings()
{
    QSettings s;
    m_snapToGrid = s.value(QStringLiteral("board/snapToGrid"), false).toBool();
    m_snapObjects = s.value(QStringLiteral("board/snapObjects"), true).toBool();
    m_showObjectSize = s.value(QStringLiteral("board/showObjectSize"), false).toBool();
    m_suggestSize = s.value(QStringLiteral("board/suggestSize"), true).toBool();
    m_showComments = s.value(QStringLiteral("board/showComments"), true).toBool();
    m_navMode = s.value(QStringLiteral("board/navMode"), QStringLiteral("mouse")).toString();   // scroll zooms
    m_showGrid = s.value(QStringLiteral("board/showGrid"), true).toBool();
    m_gridStyle = s.value(QStringLiteral("board/gridStyle"), QStringLiteral("dots")).toString();
}

void BoardCanvas::saveSetting(const char *key, const QVariant &value)
{
    QSettings().setValue(QStringLiteral("board/") + QLatin1String(key), value);
    emit settingsChanged();
    update();
}

void BoardCanvas::setSnapToGrid(bool on) { if (m_snapToGrid != on) { m_snapToGrid = on; saveSetting("snapToGrid", on); } }
void BoardCanvas::setSnapObjects(bool on) { if (m_snapObjects != on) { m_snapObjects = on; saveSetting("snapObjects", on); } }
void BoardCanvas::setShowObjectSize(bool on) { if (m_showObjectSize != on) { m_showObjectSize = on; saveSetting("showObjectSize", on); } }
void BoardCanvas::setSuggestSize(bool on) { if (m_suggestSize != on) { m_suggestSize = on; saveSetting("suggestSize", on); } }

void BoardCanvas::setShowComments(bool on)
{
    if (m_showComments == on)
        return;
    m_showComments = on;
    if (!on) {
        QStringList keep;
        for (const QString &id : std::as_const(m_selection))
            if (const BoardItem *it = m_doc.find(id); it && it->type != BoardItem::Type::Comment)
                keep << id;
        setSelection(keep);
    }
    saveSetting("showComments", on);
}

void BoardCanvas::setNavMode(const QString &mode)
{
    if (m_navMode == mode || (mode != QLatin1String("auto") && mode != QLatin1String("mouse") && mode != QLatin1String("trackpad")))
        return;
    m_navMode = mode;
    saveSetting("navMode", mode);
    emit message(mode == QLatin1String("mouse") ? QStringLiteral("Mouse: scroll wheel zooms, drag with right button to pan")
                 : mode == QLatin1String("trackpad") ? QStringLiteral("Trackpad: two fingers pan, pinch or Ctrl+scroll zooms")
                                                     : QStringLiteral("Navigation switches automatically between mouse and trackpad"));
}

void BoardCanvas::setCursorChat(const QString &text)
{
    const QString t = text.left(140);
    if (m_cursorChat == t)
        return;
    m_cursorChat = t;
    emit cursorChatChanged();
}

// ---------------------------------------------------------------------------
// Created / modified stamps

void BoardCanvas::stampChanges()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QString who = m_ownerEmail.isEmpty() ? QStringLiteral("Guest") : m_ownerEmail;
    for (BoardItem &it : m_doc.items) {
        const size_t sig = itemSignature(it);
        const auto old = m_sigs.constFind(it.id);
        if (old == m_sigs.constEnd()) {
            // A new id is always a new object here (loads and remote merges refresh first),
            // so pasted / duplicated copies get their own creation time.
            it.createdAt = now;
            it.createdBy = who;
            it.modifiedAt = now;
            it.modifiedBy = who;
        } else if (*old != sig) {
            it.modifiedAt = now;
            it.modifiedBy = who;
        } else {
            continue;
        }
        m_sigs.insert(it.id, sig);   // ids are never pruned: undo of a delete is not "new"
    }
}

void BoardCanvas::refreshSignatures()
{
    for (const BoardItem &it : std::as_const(m_doc.items))
        m_sigs.insert(it.id, itemSignature(it));
}

QVariantMap BoardCanvas::selectionMeta() const
{
    const auto items = selectedItems();
    QVariantMap m;
    m.insert(QStringLiteral("count"), items.size());
    m.insert(QStringLiteral("owner"), m_doc.owner.isEmpty() ? QStringLiteral("Guest") : m_doc.owner);
    if (items.isEmpty())
        return m;
    // Newest change and oldest creation across the selection.
    const BoardItem *created = items.first();
    const BoardItem *modified = items.first();
    for (const BoardItem *it : items) {
        if (it->createdAt && (!created->createdAt || it->createdAt < created->createdAt))
            created = it;
        if (it->modifiedAt > modified->modifiedAt)
            modified = it;
    }
    m.insert(QStringLiteral("createdBy"), created->createdBy.isEmpty() ? QStringLiteral("Unknown") : created->createdBy);
    m.insert(QStringLiteral("createdAt"), created->createdAt ? formatTime(created->createdAt) : QStringLiteral("before info was recorded"));
    m.insert(QStringLiteral("createdExact"), created->createdAt ? QLocale().toString(QDateTime::fromMSecsSinceEpoch(created->createdAt).toLocalTime(), QLocale::LongFormat) : QString());
    m.insert(QStringLiteral("modifiedBy"), modified->modifiedBy.isEmpty() ? QStringLiteral("Unknown") : modified->modifiedBy);
    m.insert(QStringLiteral("modifiedAt"), modified->modifiedAt ? formatTime(modified->modifiedAt) : QStringLiteral("before info was recorded"));
    m.insert(QStringLiteral("modifiedExact"), modified->modifiedAt ? QLocale().toString(QDateTime::fromMSecsSinceEpoch(modified->modifiedAt).toLocalTime(), QLocale::LongFormat) : QString());
    m.insert(QStringLiteral("type"), BoardItem::typeName(items.first()->type));
    m.insert(QStringLiteral("fileName"), items.size() == 1 ? selName() : QStringLiteral("%1 objects").arg(items.size()));
    return m;
}

// ---------------------------------------------------------------------------
// Snapping

QPointF BoardCanvas::snapMove(const QRectF &moving, const QPointF &delta)
{
    m_guides.clear();
    QRectF r = moving.translated(delta);
    if (m_snapToGrid) {
        const double dx = std::round(r.left() / kGrid) * kGrid - r.left();
        const double dy = std::round(r.top() / kGrid) * kGrid - r.top();
        return delta + QPointF(dx, dy);
    }
    if (!m_snapObjects)
        return delta;

    const double tol = kSnapPx / m_zoom;
    const QRectF view(toWorld(QPointF(0, 0)), toWorld(QPointF(width(), height())));
    double bestDx = tol + 1, bestDy = tol + 1;
    double gx = 0, gy = 0;
    QRectF mx, my;
    const double mine[3] = { r.left(), r.center().x(), r.right() };
    const double mineY[3] = { r.top(), r.center().y(), r.bottom() };
    for (const BoardItem &o : std::as_const(m_doc.items)) {
        if (m_snapExclude.contains(o.id) || o.type == BoardItem::Type::Connector || o.type == BoardItem::Type::Comment
            || !m_doc.itemVisible(o) || !view.intersects(o.rect))
            continue;
        const double xs[3] = { o.rect.left(), o.rect.center().x(), o.rect.right() };
        const double ys[3] = { o.rect.top(), o.rect.center().y(), o.rect.bottom() };
        for (double a : xs)
            for (double b : mine)
                if (std::abs(a - b) < std::abs(bestDx)) { bestDx = a - b; gx = a; mx = o.rect; }
        for (double a : ys)
            for (double b : mineY)
                if (std::abs(a - b) < std::abs(bestDy)) { bestDy = a - b; gy = a; my = o.rect; }
    }
    QPointF d = delta;
    if (std::abs(bestDx) <= tol) d.rx() += bestDx;
    if (std::abs(bestDy) <= tol) d.ry() += bestDy;
    // Equal spacing: centre the object between its left/right (top/bottom) neighbours.
    {
        const auto n = gapNeighbours(moving.translated(d));
        const QRectF m = moving.translated(d);
        if (std::abs(bestDx) > tol && !n[0].isNull() && !n[2].isNull()) {
            const double c = ((n[2].left() - m.right()) - (m.left() - n[0].right())) / 2;
            if (std::abs(c) <= tol) d.rx() += c;
        }
        if (std::abs(bestDy) > tol && !n[1].isNull() && !n[3].isNull()) {
            const double c = ((n[3].top() - m.bottom()) - (m.top() - n[1].bottom())) / 2;
            if (std::abs(c) <= tol) d.ry() += c;
        }
        // Match an existing gap: neighbour -> its own neighbour on the same side.
        const QRectF m2 = moving.translated(d);
        const auto n2 = gapNeighbours(m2);
        if (std::abs(bestDx) > tol) {
            double best = tol + 1;
            if (!n2[0].isNull()) {
                const QRectF nn = gapNeighbours(n2[0])[0];
                if (!nn.isNull()) { const double c = (n2[0].left() - nn.right()) - (m2.left() - n2[0].right()); if (std::abs(c) < std::abs(best)) best = c; }
            }
            if (!n2[2].isNull()) {
                const QRectF nn = gapNeighbours(n2[2])[2];
                if (!nn.isNull()) { const double c = (n2[2].left() - m2.right()) - (nn.left() - n2[2].right()); if (std::abs(c) < std::abs(best)) best = c; }
            }
            if (std::abs(best) <= tol) d.rx() += best;
        }
        if (std::abs(bestDy) > tol) {
            double best = tol + 1;
            if (!n2[1].isNull()) {
                const QRectF nn = gapNeighbours(n2[1])[1];
                if (!nn.isNull()) { const double c = (n2[1].top() - nn.bottom()) - (m2.top() - n2[1].bottom()); if (std::abs(c) < std::abs(best)) best = c; }
            }
            if (!n2[3].isNull()) {
                const QRectF nn = gapNeighbours(n2[3])[3];
                if (!nn.isNull()) { const double c = (n2[3].top() - m2.bottom()) - (nn.top() - n2[3].bottom()); if (std::abs(c) < std::abs(best)) best = c; }
            }
            if (std::abs(best) <= tol) d.ry() += best;
        }
    }
    r = moving.translated(d);
    if (std::abs(bestDx) <= tol)
        m_guides << QLineF(gx, qMin(r.top(), mx.top()) - 16, gx, qMax(r.bottom(), mx.bottom()) + 16);
    if (std::abs(bestDy) <= tol)
        m_guides << QLineF(qMin(r.left(), my.left()) - 16, gy, qMax(r.right(), my.right()) + 16, gy);
    return d;
}

QRectF BoardCanvas::snapResize(const QRectF &in, int h)
{
    m_guides.clear();
    QRectF r = in;
    const bool left = h == 0 || h == 6 || h == 7;
    const bool right = h == 2 || h == 3 || h == 4;
    const bool top = h == 0 || h == 1 || h == 2;
    const bool bottom = h == 4 || h == 5 || h == 6;
    if (m_snapToGrid) {
        auto g = [](double v) { return std::round(v / kGrid) * kGrid; };
        if (left) r.setLeft(qMin(g(r.left()), r.right() - kGrid));
        if (right) r.setRight(qMax(g(r.right()), r.left() + kGrid));
        if (top) r.setTop(qMin(g(r.top()), r.bottom() - kGrid));
        if (bottom) r.setBottom(qMax(g(r.bottom()), r.top() + kGrid));
        return r;
    }
    if (!m_snapObjects && !m_suggestSize)
        return r;
    const double tol = kSnapPx / m_zoom;
    const QRectF view(toWorld(QPointF(0, 0)), toWorld(QPointF(width(), height())));
    double bx = tol + 1, by = tol + 1;     // best edge correction
    double bw = tol + 1, bh = tol + 1;     // best size correction
    double gx = 0, gy = 0;
    const double ex = left ? r.left() : r.right();
    const double ey = top ? r.top() : r.bottom();
    for (const BoardItem &o : std::as_const(m_doc.items)) {
        if (m_snapExclude.contains(o.id) || o.type == BoardItem::Type::Connector || o.type == BoardItem::Type::Comment
            || !m_doc.itemVisible(o) || !view.intersects(o.rect))
            continue;
        if (m_snapObjects) {
            for (double a : { o.rect.left(), o.rect.right() })
                if ((left || right) && std::abs(a - ex) < std::abs(bx)) { bx = a - ex; gx = a; }
            for (double a : { o.rect.top(), o.rect.bottom() })
                if ((top || bottom) && std::abs(a - ey) < std::abs(by)) { by = a - ey; gy = a; }
        }
        if (m_suggestSize) {   // match the size of objects nearby
            if ((left || right) && std::abs(o.rect.width() - r.width()) < std::abs(bw)) bw = o.rect.width() - r.width();
            if ((top || bottom) && std::abs(o.rect.height() - r.height()) < std::abs(bh)) bh = o.rect.height() - r.height();
        }
    }
    if (std::abs(bx) <= tol) {
        left ? r.setLeft(r.left() + bx) : r.setRight(r.right() + bx);
        m_guides << QLineF(gx, view.top(), gx, view.bottom());
    } else if (std::abs(bw) <= tol) {
        left ? r.setLeft(r.left() - bw) : r.setRight(r.right() + bw);
    }
    if (std::abs(by) <= tol) {
        top ? r.setTop(r.top() + by) : r.setBottom(r.bottom() + by);
        m_guides << QLineF(view.left(), gy, view.right(), gy);
    } else if (std::abs(bh) <= tol) {
        top ? r.setTop(r.top() - bh) : r.setBottom(r.bottom() + bh);
    }
    return r;
}

std::array<QRectF, 4> BoardCanvas::gapNeighbours(const QRectF &r) const
{
    std::array<QRectF, 4> n;   // left, top, right, bottom
    const double eps = 0.5;
    const QRectF view(toWorld(QPointF(0, 0)), toWorld(QPointF(width(), height())));
    for (const BoardItem &o : std::as_const(m_doc.items)) {
        if (m_snapExclude.contains(o.id) || m_selection.contains(o.id) || o.type == BoardItem::Type::Connector
            || o.type == BoardItem::Type::Comment || !m_doc.itemVisible(o) || !view.intersects(o.rect))
            continue;
        const QRectF &q = o.rect;
        const bool overlapY = q.top() < r.bottom() && q.bottom() > r.top();
        const bool overlapX = q.left() < r.right() && q.right() > r.left();
        if (overlapY && q.right() <= r.left() + eps && (n[0].isNull() || q.right() > n[0].right())) n[0] = q;
        if (overlapY && q.left() >= r.right() - eps && (n[2].isNull() || q.left() < n[2].left())) n[2] = q;
        if (overlapX && q.bottom() <= r.top() + eps && (n[1].isNull() || q.bottom() > n[1].bottom())) n[1] = q;
        if (overlapX && q.top() >= r.bottom() - eps && (n[3].isNull() || q.top() < n[3].top())) n[3] = q;
    }
    return n;
}

// Gap lines with a distance label between the dragged selection and its nearest
// neighbours on each side; equal gaps on one axis are marked the same way.
void BoardCanvas::paintGapMeasurements(QPainter *p, const QRectF &r)
{
    const auto n = gapNeighbours(r);
    const double gl = n[0].isNull() ? -1 : r.left() - n[0].right();
    const double gr = n[2].isNull() ? -1 : n[2].left() - r.right();
    const double gt = n[1].isNull() ? -1 : r.top() - n[1].bottom();
    const double gb = n[3].isNull() ? -1 : n[3].top() - r.bottom();
    const bool equalX = gl >= 0 && gr >= 0 && std::abs(gl - gr) < 0.5;
    const bool equalY = gt >= 0 && gb >= 0 && std::abs(gt - gb) < 0.5;

    QFont f = p->font();
    f.setPixelSize(11);
    f.setBold(true);
    p->setFont(f);
    const QFontMetricsF fm(f);

    auto draw = [&](QPointF a, QPointF b, double gap, bool equal) {
        if (gap < 0.5)
            return;
        a = toScreen(a);
        b = toScreen(b);
        const bool horiz = std::abs(a.y() - b.y()) < 0.01;
        if ((horiz ? std::abs(b.x() - a.x()) : std::abs(b.y() - a.y())) < 6)
            return;
        p->setPen(QPen(kGuide, 1.0));
        p->drawLine(a, b);
        const QPointF t = horiz ? QPointF(0, 4) : QPointF(4, 0);    // end ticks
        p->drawLine(a - t, a + t);
        p->drawLine(b - t, b + t);
        const QString label = QString::number(qRound(gap));
        const QSizeF s(fm.horizontalAdvance(label) + 10, fm.height() + 4);
        const QPointF c = (a + b) / 2;
        const QRectF pill = horiz ? QRectF(c.x() - s.width() / 2, c.y() + 6, s.width(), s.height())
                                  : QRectF(c.x() + 6, c.y() - s.height() / 2, s.width(), s.height());
        p->setPen(Qt::NoPen);
        p->setBrush(equal ? QColor(236, 64, 122) : QColor(236, 64, 122, 220));
        p->drawRoundedRect(pill, 4, 4);
        p->setPen(Qt::white);
        p->drawText(pill, Qt::AlignCenter, equal ? label + QStringLiteral(" =") : label);
    };
    auto midY = [&](const QRectF &q) { return (qMax(q.top(), r.top()) + qMin(q.bottom(), r.bottom())) / 2; };
    auto midX = [&](const QRectF &q) { return (qMax(q.left(), r.left()) + qMin(q.right(), r.right())) / 2; };
    if (gl >= 0) draw({ n[0].right(), midY(n[0]) }, { r.left(), midY(n[0]) }, gl, equalX);
    if (gr >= 0) draw({ r.right(), midY(n[2]) }, { n[2].left(), midY(n[2]) }, gr, equalX);
    if (gt >= 0) draw({ midX(n[1]), n[1].bottom() }, { midX(n[1]), r.top() }, gt, equalY);
    if (gb >= 0) draw({ midX(n[3]), r.bottom() }, { midX(n[3]), n[3].top() }, gb, equalY);

    // Gap matched to the next pair along: show that pair's gap too, marked equal.
    auto pairMidY = [](const QRectF &a, const QRectF &b) { return (qMax(a.top(), b.top()) + qMin(a.bottom(), b.bottom())) / 2; };
    auto pairMidX = [](const QRectF &a, const QRectF &b) { return (qMax(a.left(), b.left()) + qMin(a.right(), b.right())) / 2; };
    if (gl >= 0) {
        const QRectF nn = gapNeighbours(n[0])[0];
        if (!nn.isNull() && std::abs((n[0].left() - nn.right()) - gl) < 0.5) {
            const double y = pairMidY(nn, n[0]);
            draw({ nn.right(), y }, { n[0].left(), y }, gl, true);
            draw({ n[0].right(), midY(n[0]) }, { r.left(), midY(n[0]) }, gl, true);
        }
    }
    if (gr >= 0) {
        const QRectF nn = gapNeighbours(n[2])[2];
        if (!nn.isNull() && std::abs((nn.left() - n[2].right()) - gr) < 0.5) {
            const double y = pairMidY(nn, n[2]);
            draw({ n[2].right(), y }, { nn.left(), y }, gr, true);
            draw({ r.right(), midY(n[2]) }, { n[2].left(), midY(n[2]) }, gr, true);
        }
    }
    if (gt >= 0) {
        const QRectF nn = gapNeighbours(n[1])[1];
        if (!nn.isNull() && std::abs((n[1].top() - nn.bottom()) - gt) < 0.5) {
            const double x = pairMidX(nn, n[1]);
            draw({ x, nn.bottom() }, { x, n[1].top() }, gt, true);
            draw({ midX(n[1]), n[1].bottom() }, { midX(n[1]), r.top() }, gt, true);
        }
    }
    if (gb >= 0) {
        const QRectF nn = gapNeighbours(n[3])[3];
        if (!nn.isNull() && std::abs((nn.top() - n[3].bottom()) - gb) < 0.5) {
            const double x = pairMidX(nn, n[3]);
            draw({ x, n[3].bottom() }, { x, nn.top() }, gb, true);
            draw({ midX(n[3]), r.bottom() }, { midX(n[3]), n[3].top() }, gb, true);
        }
    }
}

// ---------------------------------------------------------------------------
// Overlays: guides, size label, link badges, comment pins

QRectF BoardCanvas::linkBadgeRect(const BoardItem &it) const
{
    const QRectF r = toScreen(m_doc.itemBounds(it));
    return QRectF(r.right() + 4, r.top() - 2, 22, 22);
}

QString BoardCanvas::linkBadgeAt(const QPointF &screen) const
{
    for (qsizetype i = m_doc.items.size() - 1; i >= 0; --i) {
        const BoardItem &it = m_doc.items[i];
        if (!it.link.isEmpty() && m_doc.itemVisible(it) && linkBadgeRect(it).contains(screen))
            return it.id;
    }
    return {};
}

QString BoardCanvas::commentAt(const QPointF &screen) const
{
    if (!m_showComments)
        return {};
    for (qsizetype i = m_doc.items.size() - 1; i >= 0; --i) {
        const BoardItem &it = m_doc.items[i];
        if (it.type == BoardItem::Type::Comment && m_doc.itemVisible(it)
            && QRectF(toScreen(it.rect.topLeft()), QSizeF(kPin, kPin)).contains(screen))
            return it.id;
    }
    return {};
}

void BoardCanvas::paintMiroOverlays(QPainter *p)
{
    const QColor accent(66, 98, 255);
    const QRectF view = boundingRect();

    // Smart guides
    p->setPen(QPen(kGuide, 1.0));
    for (const QLineF &g : std::as_const(m_guides))
        p->drawLine(QLineF(toScreen(g.p1()), toScreen(g.p2())));

    // Gap measurements to neighbouring objects while moving or resizing
    if ((m_drag == Drag::Move || m_drag == Drag::Resize) && m_dragChanged) {
        const QRectF moving = selectionRect();
        if (!moving.isNull())
            paintGapMeasurements(p, moving);
    }

    // Object size (always while resizing; when the setting is on, for the selection)
    const QRectF sel = selectionRect();
    if (!sel.isNull() && (m_drag == Drag::Resize || (m_showObjectSize && m_drag == Drag::None))) {
        const QString label = QStringLiteral("%1 × %2").arg(qRound(sel.width())).arg(qRound(sel.height()));
        QFont f = p->font();
        f.setPixelSize(12);
        f.setBold(false);
        p->setFont(f);
        const QFontMetricsF fm(f);
        const QRectF s = toScreen(sel);
        const QRectF pill(s.center().x() - (fm.horizontalAdvance(label) + 16) / 2, s.bottom() + 12,
                          fm.horizontalAdvance(label) + 16, fm.height() + 8);
        p->setPen(Qt::NoPen);
        p->setBrush(accent);
        p->drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
        p->setPen(Qt::white);
        p->drawText(pill, Qt::AlignCenter, label);
    }

    // Link badges
    for (const BoardItem &it : std::as_const(m_doc.items)) {
        if (it.link.isEmpty() || !m_doc.itemVisible(it))
            continue;
        const QRectF b = linkBadgeRect(it);
        if (!view.intersects(b))
            continue;
        p->setPen(QPen(QColor(0, 0, 0, 30), 1));
        p->setBrush(Qt::white);
        p->drawEllipse(b);
        p->save();
        p->translate(b.center());
        p->rotate(-45);
        p->setPen(QPen(accent, 1.8));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(-7.5, -3, 8, 6), 3, 3);
        p->drawRoundedRect(QRectF(-0.5, -3, 8, 6), 3, 3);
        p->restore();
    }

    // Comment pins
    if (!m_showComments)
        return;
    for (const BoardItem &it : std::as_const(m_doc.items)) {
        if (it.type != BoardItem::Type::Comment || !m_doc.itemVisible(it))
            continue;
        const QRectF pin(toScreen(it.rect.topLeft()), QSizeF(kPin, kPin));
        if (!view.intersects(pin))
            continue;
        const bool selected = m_selection.contains(it.id);
        QPainterPath bubble;
        bubble.moveTo(pin.bottomLeft());
        bubble.lineTo(pin.left(), pin.top() + kPin / 2);
        bubble.arcTo(QRectF(pin.left(), pin.top(), kPin, kPin), 180, -270);
        bubble.closeSubpath();
        p->setPen(QPen(selected ? accent : QColor(0, 0, 0, 40), selected ? 2.0 : 1.0));
        p->setBrush(QColor(255, 208, 47));
        p->drawPath(bubble);
        const QString who = it.createdBy.isEmpty() ? QStringLiteral("?") : it.createdBy.left(1).toUpper();
        QFont f = p->font();
        f.setPixelSize(13);
        f.setBold(true);
        p->setFont(f);
        p->setPen(QColor(40, 40, 40));
        p->drawText(pin, Qt::AlignCenter, who);
    }
}

// ---------------------------------------------------------------------------
// Board menu

void BoardCanvas::unlockAll()
{
    int n = 0;
    for (const BoardItem &it : std::as_const(m_doc.items))
        n += it.locked ? 1 : 0;
    if (!n) {
        emit message(QStringLiteral("Nothing is locked"));
        return;
    }
    pushUndo();
    for (BoardItem &it : m_doc.items)
        it.locked = false;
    markChanged();
    emit selectionChanged();
    emit message(QStringLiteral("Unlocked %1 object(s)").arg(n));
}

static QPointF screenOrCenter(double x, double y, const QRectF &view)
{
    return (x < 0 || y < 0) ? view.center() : QPointF(x, y);
}

void BoardCanvas::addTextAt(double sx, double sy)
{
    endTextEdit();
    setTool(QStringLiteral("select"));
    const QList<BoardItem> before = m_doc.items;
    const QString id = addItem(makeItem(BoardItem::Type::Text, toWorld(screenOrCenter(sx, sy, boundingRect()))));
    setSelection({ id });
    beginTextEdit(id);
    m_editSnapshot = before;   // creation + typing = one undo step; removed if left empty
    m_editIsNew = true;
    update();
}

void BoardCanvas::addStickyAt(double sx, double sy)
{
    endTextEdit();
    setTool(QStringLiteral("select"));
    const QList<BoardItem> before = m_doc.items;
    const QString id = addItem(makeItem(BoardItem::Type::Sticky, toWorld(screenOrCenter(sx, sy, boundingRect()))));
    setSelection({ id });
    beginTextEdit(id);
    m_editSnapshot = before;
    m_editIsNew = true;
    markChanged();
}

void BoardCanvas::addCommentAt(double sx, double sy)
{
    endTextEdit();
    QPointF screen = screenOrCenter(sx, sy, boundingRect());
    if (sx < 0 && !m_selection.isEmpty()) {
        const QRectF b = toScreen(m_doc.boundsOf(m_selection));
        screen = b.topRight() + QPointF(8, -kPin / 2);
    }
    if (!m_showComments)
        setShowComments(true);
    BoardItem c;
    c.id = BoardItem::newId();
    c.type = BoardItem::Type::Comment;
    c.layer = m_activeLayer;
    c.rect = QRectF(toWorld(screen), QSizeF(kPin, kPin) / m_zoom);
    c.strokeWidth = 0;
    pushUndo();
    addItem(c);
    setSelection({ c.id });
    markChanged();
    emit commentOpened(c.id, screen.x() + kPin, screen.y());
}

void BoardCanvas::setCommentText(const QString &id, const QString &text)
{
    BoardItem *c = m_doc.find(id);
    if (!c || c->type != BoardItem::Type::Comment)
        return;
    const QString t = text.trimmed();
    if (t.isEmpty()) {   // an empty comment is discarded
        m_doc.items.removeIf([&](const BoardItem &x) { return x.id == id; });
        m_selection.removeAll(id);
        emit selectionChanged();
        markChanged();
        return;
    }
    if (c->text == t)
        return;
    pushUndo();
    c->text = t;
    markChanged();
}

QVariantMap BoardCanvas::commentInfo(const QString &id) const
{
    const BoardItem *c = m_doc.find(id);
    if (!c)
        return {};
    return { { "id", c->id }, { "text", c->text },
             { "author", c->createdBy.isEmpty() ? QStringLiteral("Guest") : c->createdBy },
             { "time", formatTime(c->modifiedAt ? c->modifiedAt : c->createdAt) } };
}

void BoardCanvas::fitWorldRect(const QRectF &content, double maxZoom)
{
    if (content.isNull() || width() <= 0 || height() <= 0)
        return;
    QRectF view = boundingRect().adjusted(76, 72, -16, -64);
    if (view.width() < 100 || view.height() < 100)
        view = boundingRect();
    m_zoom = qBound(0.02, qMin(view.width() / qMax(1.0, content.width()), view.height() / qMax(1.0, content.height())), maxZoom);
    m_pan = view.center() - content.center() * m_zoom;
    emit viewChanged();
    if (editing())
        emit editingChanged();
    update();
}

void BoardCanvas::setStartViewToCurrent()
{
    m_doc.startView = QRectF(toWorld(QPointF(76, 72)), toWorld(QPointF(width() - 16, height() - 64)));
    m_modified = true;
    emit fileChanged();
    emit localEdit();
    emit message(QStringLiteral("Start view set: the board opens here"));
}

void BoardCanvas::setStartViewToFrame(const QString &frameId)
{
    const BoardItem *f = m_doc.find(frameId);
    if (!f)
        return;
    m_doc.startView = f->rect.adjusted(-24, -24 - f->fontSize * 2, 24, 24);
    m_modified = true;
    emit fileChanged();
    emit localEdit();
    emit message(QStringLiteral("Start view set to \"%1\"").arg(f->text));
}

void BoardCanvas::clearStartView()
{
    m_doc.startView = QRectF();
    m_modified = true;
    emit fileChanged();
    emit localEdit();
    emit message(QStringLiteral("Start view removed: the board opens fitted to its content"));
}

void BoardCanvas::goToStartView()
{
    if (m_doc.startView.isEmpty())
        zoomToContent();
    else
        fitWorldRect(m_doc.startView, 40.0);
}

QVariantList BoardCanvas::frames() const
{
    QVariantList out;
    for (const BoardItem &it : m_doc.items)
        if (it.type == BoardItem::Type::Frame)
            out << QVariantMap{ { "id", it.id }, { "name", it.text.isEmpty() ? QStringLiteral("Frame") : it.text } };
    return out;
}

void BoardCanvas::zoomToContent()
{
    const QRectF c = m_doc.contentBounds();
    if (c.isNull())
        return resetZoom();
    fitWorldRect(c.adjusted(-24, -24, 24, 24), 2.0);
}

void BoardCanvas::zoomToSelection()
{
    if (m_selection.isEmpty())
        return zoomToContent();
    fitWorldRect(m_doc.boundsOf(m_selection).adjusted(-24, -24, 24, 24), 4.0);
}

void BoardCanvas::zoomToItem(const QString &id)
{
    if (!m_doc.find(id))
        return;
    setSelection({ id });
    zoomToSelection();
}

// ---------------------------------------------------------------------------
// Object menu

void BoardCanvas::createFrameAroundSelection()
{
    if (m_selection.isEmpty())
        return;
    const QRectF b = m_doc.boundsOf(m_selection).adjusted(-40, -40, 40, 40);
    BoardItem f = makeItem(BoardItem::Type::Frame, b.center());
    f.rect = b;
    f.text = QStringLiteral("Frame %1").arg(++m_frameCounter);
    pushUndo();
    // Behind the objects it holds.
    qsizetype at = m_doc.items.size();
    for (qsizetype i = 0; i < m_doc.items.size(); ++i)
        if (m_selection.contains(m_doc.items[i].id)) { at = i; break; }
    m_doc.items.insert(at, f);
    setSelection({ f.id });
    markChanged();
}

QString BoardCanvas::selLink() const
{
    const auto items = selectedItems();
    return items.size() == 1 ? items.first()->link : QString();
}

void BoardCanvas::setSelLink(const QString &link)
{
    const QString l = link.trimmed();
    applyToSelection([&](BoardItem &it) { it.link = l; });
    if (!l.isEmpty())
        emit message(QStringLiteral("Link added. Click the link icon to open it."));
}

void BoardCanvas::copyLink()
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(QStringLiteral("miru://item/") + items.first()->id);
    emit message(QStringLiteral("Link to object copied. Paste it on this board to jump to the object."));
}

void BoardCanvas::followLink(const QString &link)
{
    QString id;
    if (link.startsWith(QLatin1Char('#')))
        id = link.mid(1);
    else if (link.startsWith(QLatin1String("miru://item/")))
        id = link.mid(12);
    else if (link.startsWith(QLatin1String("blackmiro://item/")))   // links copied in v1.0
        id = link.mid(17);
    if (!id.isEmpty()) {
        if (m_doc.find(id))
            zoomToItem(id);
        else
            emit message(QStringLiteral("That object is not on this board"));
        return;
    }
    emit linkActivated(link);
}

bool BoardCanvas::selGrouped() const
{
    for (const BoardItem *it : selectedItems())
        if (!it->group.isEmpty())
            return true;
    return false;
}

QString BoardCanvas::selLayer() const
{
    const auto items = selectedItems();
    return items.isEmpty() ? QString() : items.first()->layer;
}

QStringList BoardCanvas::expandGroups(const QStringList &ids) const
{
    QSet<QString> groups;
    for (const QString &id : ids)
        if (const BoardItem *it = m_doc.find(id); it && !it->group.isEmpty())
            groups.insert(it->group);
    if (groups.isEmpty())
        return ids;
    QStringList out = ids;
    for (const BoardItem &it : m_doc.items)
        if (groups.contains(it.group) && !out.contains(it.id))
            out << it.id;
    return out;
}

void BoardCanvas::groupSelection()
{
    if (m_selection.size() < 2)
        return;
    const QString g = BoardItem::newId();
    applyToSelection([&](BoardItem &it) { it.group = g; });
    emit message(QStringLiteral("Grouped %1 objects").arg(m_selection.size()));
}

void BoardCanvas::ungroupSelection()
{
    if (!selGrouped())
        return;
    applyToSelection([](BoardItem &it) { it.group.clear(); });
    emit message(QStringLiteral("Ungrouped"));
}

void BoardCanvas::clearContents()
{
    bool any = false;
    for (const BoardItem *it : selectedItems())
        any = any || (!it->text.isEmpty() && !it->locked && it->type != BoardItem::Type::Frame);
    if (!any)
        return;
    applyToSelection([&](BoardItem &it) {
        if (it.locked || it.type == BoardItem::Type::Frame)
            return;
        it.text.clear();
        autoSizeText(it);
    });
}

void BoardCanvas::selectNext(bool backwards)
{
    QList<const BoardItem *> order;
    for (const BoardItem &it : m_doc.items)
        if (m_doc.itemPickable(it) && it.type != BoardItem::Type::Connector && it.type != BoardItem::Type::Comment)
            order << &it;
    if (order.isEmpty())
        return;
    // Reading order: rows from the top, left to right.
    std::stable_sort(order.begin(), order.end(), [](const BoardItem *a, const BoardItem *b) {
        if (std::abs(a->rect.top() - b->rect.top()) > 20)
            return a->rect.top() < b->rect.top();
        return a->rect.left() < b->rect.left();
    });
    qsizetype i = -1;
    if (!m_selection.isEmpty())
        for (qsizetype k = 0; k < order.size(); ++k)
            if (order[k]->id == m_selection.first()) { i = k; break; }
    i = backwards ? (i <= 0 ? order.size() - 1 : i - 1) : (i + 1) % order.size();
    const BoardItem *next = order[i];
    setSelection({ next->id });
    const QRectF s = toScreen(next->rect);
    if (!boundingRect().adjusted(76, 72, -16, -64).contains(s)) {
        m_pan += boundingRect().center() - s.center();
        emit viewChanged();
        update();
    }
}

void BoardCanvas::duplicateInDirection(double dx, double dy)
{
    if (m_selection.isEmpty())
        return;
    const QRectF b = m_doc.boundsOf(m_selection);
    const QPointF off(dx * (b.width() + 40), dy * (b.height() + 40));
    QHash<QString, QString> idMap;
    for (const BoardItem *it : selectedItems())
        idMap.insert(it->id, BoardItem::newId());
    pushUndo();
    QStringList copies;
    QList<BoardItem> added;
    for (const BoardItem *it : selectedItems()) {
        BoardItem c = *it;
        c.id = idMap.value(it->id);
        c.locked = false;
        c.rect.translate(off);
        if (c.type == BoardItem::Type::Connector) {
            c.fromId = idMap.value(c.fromId, c.fromId);
            c.toId = idMap.value(c.toId, c.toId);
            c.fromPoint += off;
            c.toPoint += off;
        }
        added << c;
        copies << c.id;
    }
    for (const BoardItem &c : std::as_const(added))
        addItem(c);
    setSelection(copies);
    markChanged();
}

// ---------------------------------------------------------------------------
// Custom templates ("Save as template")

static QString templateDir() { return appDir(QStringLiteral("templates")); }

bool BoardCanvas::saveSelectionAsTemplate(const QString &name)
{
    Document d;
    d.title = name.trimmed().isEmpty() ? QStringLiteral("My template") : name.trimmed();
    const QStringList ids = m_selection.isEmpty() ? QStringList() : expandGroups(m_selection);
    QStringList withChildren = ids;
    for (const QString &id : ids)
        if (const BoardItem *it = m_doc.find(id); it && it->type == BoardItem::Type::Frame)
            for (const QString &child : m_doc.itemsInsideFrame(*it))
                if (!withChildren.contains(child))
                    withChildren << child;
    for (const BoardItem &it : std::as_const(m_doc.items)) {
        if (it.type == BoardItem::Type::Comment)
            continue;
        if (!withChildren.isEmpty() && !withChildren.contains(it.id))
            continue;
        BoardItem c = it;
        c.locked = false;
        d.items << c;
        if (!c.assetKey.isEmpty() && m_doc.assets.contains(c.assetKey))
            d.assets.insert(c.assetKey, m_doc.assets.value(c.assetKey));
    }
    if (d.items.isEmpty()) {
        emit message(QStringLiteral("Nothing to save as a template"));
        return false;
    }
    const QString path = templateDir() + QLatin1Char('/') + safeFileName(d.title)
                         + QStringLiteral("-%1.board").arg(QDateTime::currentMSecsSinceEpoch());
    QString error;
    if (!BoardFile::saveToFile(d, path, &error)) {
        emit message(QStringLiteral("Could not save template: %1").arg(error));
        return false;
    }
    emit message(QStringLiteral("Saved \"%1\" to My templates").arg(d.title));
    return true;
}

Document BoardCanvas::loadCustomTemplate(const QString &id)
{
    Document d;
    if (!id.startsWith(QLatin1String("custom:")))
        return d;
    const QString file = QFileInfo(id.mid(7)).fileName();   // no path tricks
    BoardFile::loadFromFile(templateDir() + QLatin1Char('/') + file, d);
    return d;
}

bool BoardCanvas::deleteCustomTemplate(const QString &id)
{
    if (!id.startsWith(QLatin1String("custom:")))
        return false;
    return QFile::remove(templateDir() + QLatin1Char('/') + QFileInfo(id.mid(7)).fileName());
}

// ---------------------------------------------------------------------------
// Saved files

static QString savedDir() { return appDir(QStringLiteral("saved")); }

int BoardCanvas::addSelectionToSaved()
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return 0;
    int n = 0;
    bool others = false;
    for (const BoardItem *it : items) {
        if (it->type != BoardItem::Type::Image) {
            others = true;
            continue;
        }
        const auto a = m_doc.assets.constFind(it->assetKey);
        if (a == m_doc.assets.constEnd())
            continue;
        QString base = it->name.isEmpty() ? QStringLiteral("Image") : QFileInfo(it->name).completeBaseName();
        const QString ext = a->format == "jpeg" ? QStringLiteral("jpg") : QString::fromLatin1(a->format);
        QString path = savedDir() + QLatin1Char('/') + safeFileName(base) + QLatin1Char('.') + ext;
        for (int k = 2; QFile::exists(path); ++k)
            path = savedDir() + QLatin1Char('/') + safeFileName(base) + QStringLiteral(" (%1).").arg(k) + ext;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly) && f.write(a->data) == a->data.size())
            ++n;   // original bytes, untouched
    }
    if (others) {
        // Shapes, notes, drawings...: saved as a crisp PNG of the selection.
        Exporter::Options o;
        o.formats = { Exporter::Format::Png };
        o.compression = Exporter::Compression::Custom;
        o.customDpi = 192;
        o.customQuality = 100;
        o.scope = Exporter::Scope::Selection;
        for (const BoardItem *it : items)
            if (it->type != BoardItem::Type::Image)
                o.selection << it->id;
        o.padding = 12;
        o.transparentBackground = true;
        QStringList warnings;
        const auto files = Exporter::render(m_doc, o, warnings);
        if (!files.isEmpty()) {
            QFile f(savedDir() + QStringLiteral("/Selection %1.png")
                                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh.mm.ss"))));
            if (f.open(QIODevice::WriteOnly) && f.write(files.first().data) > 0)
                ++n;
        }
    }
    emit message(n ? QStringLiteral("Added %1 file(s) to Saved files").arg(n) : QStringLiteral("Could not save the file"));
    return n;
}

QVariantList BoardCanvas::savedFiles() const
{
    QVariantList out;
    const QFileInfoList list = QDir(savedDir()).entryInfoList(QDir::Files, QDir::Time);
    for (const QFileInfo &fi : list)
        out << QVariantMap{ { "url", QUrl::fromLocalFile(fi.absoluteFilePath()) }, { "name", fi.fileName() },
                            { "size", QLocale().formattedDataSize(fi.size()) } };
    return out;
}

bool BoardCanvas::insertSavedFile(const QUrl &url)
{
    return addImageFromUrl(url);
}

bool BoardCanvas::removeSavedFile(const QUrl &url)
{
    const QFileInfo fi(url.toLocalFile());
    if (fi.absolutePath() != QDir(savedDir()).absolutePath())
        return false;   // only files inside Saved files
    return QFile::remove(fi.absoluteFilePath());
}

// ---------------------------------------------------------------------------
// Grid style, interaction state, connector options

void BoardCanvas::setGridStyle(const QString &s)
{
    const QString v = s == QLatin1String("lines") ? s : QStringLiteral("dots");
    if (m_gridStyle == v)
        return;
    m_gridStyle = v;
    saveSetting("gridStyle", v);
}

void BoardCanvas::updateInteracting()
{
    const bool on = m_dragChanged
        && (m_drag == Drag::Move || m_drag == Drag::Resize || m_drag == Drag::Connector
            || m_drag == Drag::ConnectorEnd || m_drag == Drag::Create || m_drag == Drag::Draw);
    if (on != m_interacting) {
        m_interacting = on;
        emit interactingChanged();
    }
}

int BoardCanvas::selLineType() const
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return 0;
    return items.first()->connectorStyle == BoardItem::ConnectorStyle::Elbow ? 1
         : items.first()->connectorStyle == BoardItem::ConnectorStyle::Curved ? 2 : 0;
}
void BoardCanvas::setSelLineType(int t)
{
    const auto st = t == 1 ? BoardItem::ConnectorStyle::Elbow
                  : t == 2 ? BoardItem::ConnectorStyle::Curved : BoardItem::ConnectorStyle::Straight;
    applyToSelection([&](BoardItem &it) { if (it.isConnector()) it.connectorStyle = st; });
}
int BoardCanvas::selDash() const { const auto s = selectedItems(); return s.isEmpty() ? 0 : s.first()->dash; }
void BoardCanvas::setSelDash(int d) { applyToSelection([&](BoardItem &it) { it.dash = qBound(0, d, 2); }); }
bool BoardCanvas::selArrowStart() const { const auto s = selectedItems(); return !s.isEmpty() && s.first()->arrowStart; }
void BoardCanvas::setSelArrowStart(bool on) { applyToSelection([&](BoardItem &it) { if (it.isConnector()) it.arrowStart = on; }); }
bool BoardCanvas::selArrowEnd() const { const auto s = selectedItems(); return !s.isEmpty() && s.first()->arrowEnd; }
void BoardCanvas::setSelArrowEnd(bool on) { applyToSelection([&](BoardItem &it) { if (it.isConnector()) it.arrowEnd = on; }); }

void BoardCanvas::swapConnectorEnds()
{
    applyToSelection([](BoardItem &it) {
        if (!it.isConnector())
            return;
        std::swap(it.fromId, it.toId);
        std::swap(it.fromPoint, it.toPoint);
        std::swap(it.arrowStart, it.arrowEnd);
    });
}

void BoardCanvas::editSelectionText()
{
    if (m_selection.size() == 1)
        beginTextEdit(m_selection.first());
}

// ---------------------------------------------------------------------------
// Quick-connect dots (drag: draw a connector; click: add a connected copy)

QList<QPointF> BoardCanvas::connectDots() const
{
    if (m_selection.size() != 1 || m_tool != QLatin1String("select") || !m_editingId.isEmpty() || cropping())
        return {};
    const BoardItem *it = m_doc.find(m_selection.first());
    if (!it || it->locked || it->isConnector() || it->type == BoardItem::Type::Comment
        || it->type == BoardItem::Type::Frame || it->type == BoardItem::Type::Pen)
        return {};
    const QRectF s = toScreen(it->rect);
    if (s.width() < 16 || s.height() < 16)
        return {};
    const double off = 22;
    return { QPointF(s.center().x(), s.top() - off), QPointF(s.right() + off, s.center().y()),
             QPointF(s.center().x(), s.bottom() + off), QPointF(s.left() - off, s.center().y()) };
}

int BoardCanvas::connectDotAt(const QPointF &screen) const
{
    const QList<QPointF> dots = connectDots();
    for (int i = 0; i < dots.size(); ++i)
        if (QLineF(dots[i], screen).length() <= 14)   // generous for touch
            return i;
    return -1;
}

void BoardCanvas::quickConnectCreate(const QString &connectorId)
{
    BoardItem *c = m_doc.find(connectorId);
    const BoardItem *src = c ? m_doc.find(c->fromId) : nullptr;
    if (!c || !src)
        return;
    // New sibling of the same kind and size, one gap away in the dot's direction;
    // step sideways while that spot is taken (mind-map style branching).
    const double gap = 96;
    static const QPointF dirs[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    const QPointF d = dirs[qBound(0, m_quickDir, 3)];
    const QSizeF sz = src->rect.size();
    QRectF target = src->rect.translated(d.x() * (sz.width() + gap), d.y() * (sz.height() + gap));
    const QPointF side = d.x() != 0 ? QPointF(0, sz.height() + 32) : QPointF(sz.width() + 32, 0);
    for (int tries = 0; tries < 12; ++tries) {
        bool taken = false;
        for (const BoardItem &o : std::as_const(m_doc.items))
            if (!o.isConnector() && o.type != BoardItem::Type::Comment && m_doc.itemVisible(o)
                && o.rect.adjusted(-8, -8, 8, 8).intersects(target)) { taken = true; break; }
        if (!taken)
            break;
        target.translate(tries % 2 == 0 ? side * (tries / 2 + 1) : -side * (tries / 2 + 1));
    }
    BoardItem n = *src;
    n.id = BoardItem::newId();
    n.rect = target;
    n.link.clear();
    n.group.clear();
    n.locked = false;
    if (n.type == BoardItem::Type::Sticky || n.type == BoardItem::Type::Text || n.isShape())
        n.text.clear();
    const QString nid = addItem(n);
    if (BoardItem *cc = m_doc.find(connectorId)) {
        cc->toId = nid;
        cc->toPoint = target.center();
    }
    setSelection({ nid });
}

// ---------------------------------------------------------------------------
// Arrange: \ lines the selection up in a row, Shift+\ in a column, evenly spaced.

void BoardCanvas::arrangeSelection(bool vertical)
{
    QList<BoardItem *> items;
    for (BoardItem &it : m_doc.items)
        if (m_selection.contains(it.id) && !it.isConnector() && it.type != BoardItem::Type::Comment && !it.locked)
            items << &it;
    if (items.size() < 2) {
        emit message(QStringLiteral("Select two or more objects to arrange them."));
        return;
    }
    pushUndo();
    // Keep the reading order: sort along the arrange axis.
    std::sort(items.begin(), items.end(), [vertical](const BoardItem *a, const BoardItem *b) {
        return vertical ? a->rect.center().y() < b->rect.center().y() : a->rect.center().x() < b->rect.center().x();
    });
    const double gap = 32;
    double cross = 0;   // shared centre line on the other axis
    for (const BoardItem *it : std::as_const(items))
        cross += vertical ? it->rect.center().x() : it->rect.center().y();
    cross /= items.size();
    double pos = vertical ? items.first()->rect.top() : items.first()->rect.left();
    QSet<QString> moved;
    for (BoardItem *it : std::as_const(items)) {
        const QRectF old = it->rect;
        QRectF r = old;
        if (vertical)
            r.moveTo(cross - r.width() / 2, pos);
        else
            r.moveTo(pos, cross - r.height() / 2);
        it->rect = r;
        pos += (vertical ? r.height() : r.width()) + gap;
        // Frames carry their contents along.
        if (it->type == BoardItem::Type::Frame) {
            const QPointF d = r.topLeft() - old.topLeft();
            for (BoardItem &c : m_doc.items)
                if (!m_selection.contains(c.id) && !c.isConnector() && old.contains(c.rect))
                    c.rect.translate(d);
        }
    }
    markChanged();
    emit selectionGeometryChanged();
}

// ---------------------------------------------------------------------------
// Smooth view motion

void BoardCanvas::smoothZoomBy(double factor, const QPointF &anchor)
{
    if (m_zoomGoal <= 0)
        m_zoomGoal = m_zoom;
    m_zoomGoal = qBound(kMinZoomValue, m_zoomGoal * factor, kMaxZoomValue);
    m_zoomAnchor = anchor;
    m_glide = QPointF();
    if (!m_viewAnim.isActive()) {
        m_animClock.restart();
        m_viewAnim.start();
    }
}

void BoardCanvas::trackPanVelocity(const QPointF &pos)
{
    if (!m_panClock.isValid())
        m_panClock.start();
    const qint64 now = m_panClock.elapsed();
    m_panSamples.append({ now, pos });
    while (!m_panSamples.isEmpty() && now - m_panSamples.first().first > 90)
        m_panSamples.removeFirst();
}

void BoardCanvas::startGlide()
{
    if (m_panSamples.size() < 2 || !m_panClock.isValid())
        return;
    const auto &a = m_panSamples.first();
    const auto &b = m_panSamples.last();
    const qint64 idle = m_panClock.elapsed() - b.first;
    const double dt = double(b.first - a.first);
    m_panSamples.clear();
    if (dt < 8 || idle > 60)       // the pointer stopped before release: no glide
        return;
    m_glide = (b.second - a.second) / dt;
    if (std::hypot(m_glide.x(), m_glide.y()) < 0.25)
        return;
    const double maxSpeed = 6.0;   // px per ms
    const double speed = std::hypot(m_glide.x(), m_glide.y());
    if (speed > maxSpeed)
        m_glide *= maxSpeed / speed;
    m_zoomGoal = 0;
    m_animClock.restart();
    m_viewAnim.start();
}

void BoardCanvas::stopViewAnimation()
{
    m_viewAnim.stop();
    m_zoomGoal = 0;
    m_glide = QPointF();
}

void BoardCanvas::viewAnimTick()
{
    const double dt = qBound(1.0, double(m_animClock.restart()), 50.0);
    bool busy = false;
    if (m_zoomGoal > 0) {
        // Ease in log space: each frame closes ~a quarter of the remaining distance
        // (frame-rate independent), finishing in about 150 ms.
        const double remaining = std::log(m_zoomGoal / m_zoom);
        if (std::abs(remaining) < 0.002) {
            setZoomAt(m_zoomGoal, m_zoomAnchor);
            m_zoomGoal = 0;
        } else {
            const double k = 1.0 - std::pow(0.75, dt / 16.0);
            setZoomAt(m_zoom * std::exp(remaining * k), m_zoomAnchor);
            busy = true;
        }
    }
    if (!m_glide.isNull()) {
        m_pan += m_glide * dt;
        m_glide *= std::pow(0.93, dt / 16.0);      // friction: eases out over ~0.5 s
        if (std::hypot(m_glide.x(), m_glide.y()) < 0.02)
            m_glide = QPointF();
        else
            busy = true;
        emit viewChanged();
        update();
    }
    if (!busy)
        m_viewAnim.stop();
}

// ---------------------------------------------------------------------------
// Image previews: big boards open and pan smoothly because images are decoded
// in the background at screen size (longest side 2048 px). The renderer falls
// back to the original only when zoomed in past the preview's detail.

namespace {
QImage decodePreview(const QByteArray &bytes)
{
    QBuffer buf;
    buf.setData(bytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    reader.setAutoTransform(false);              // same orientation as Document::assetImage
    const QSize full = reader.size();
    const int maxSide = 2048;
    if (full.isValid() && qMax(full.width(), full.height()) > maxSide)
        reader.setScaledSize(full.scaled(maxSide, maxSide, Qt::KeepAspectRatio));
    QImage img = reader.read();
    if (!img.isNull() && img.format() != QImage::Format_ARGB32_Premultiplied)
        img.convertTo(QImage::Format_ARGB32_Premultiplied);   // fastest format to draw
    return img;
}
} // namespace

void BoardCanvas::ensurePreviews()
{
    // Forget previews of images no longer on the board.
    for (auto it = m_previews.begin(); it != m_previews.end();)
        it = m_doc.assets.contains(it.key()) ? std::next(it) : m_previews.erase(it);

    for (auto it = m_doc.assets.constBegin(); it != m_doc.assets.constEnd(); ++it) {
        const QString key = it.key();
        if (m_previews.contains(key) || m_previewPending.contains(key))
            continue;
        m_previewPending.insert(key);
        const QByteArray bytes = it->data;   // implicitly shared, safe to read on another thread
        const QPointer<BoardCanvas> self(this);
        auto deliver = [self, key](const QImage &img) {
            if (!self)
                return;
            self->m_previewPending.remove(key);
            self->m_previews.insert(key, img);   // a null image (unreadable) is not retried
            self->update();
        };
#if QT_CONFIG(thread) && !defined(Q_OS_WASM)
        QThreadPool::globalInstance()->start([bytes, deliver]() {
            const QImage img = decodePreview(bytes);
            QMetaObject::invokeMethod(qApp, [deliver, img]() { deliver(img); }, Qt::QueuedConnection);
        });
#else
        // Single-threaded (browser): one image per event-loop turn keeps the UI responsive.
        QTimer::singleShot(0, this, [bytes, deliver]() { deliver(decodePreview(bytes)); });
#endif
    }
}
