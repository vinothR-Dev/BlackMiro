#include "Renderer.h"

#include "ShapeLibrary.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QLineF>
#include <QTextLayout>
#include <QtMath>

namespace Renderer {

QString emojiFamily()
{
    static const QString family = []() {
        const QStringList installed = QFontDatabase::families();
        for (const char *name : { "Segoe UI Emoji", "Apple Color Emoji", "Noto Color Emoji", "Twemoji Mozilla",
                                  "EmojiOne Color", "JoyPixels" })
            if (installed.contains(QLatin1String(name)))
                return QString::fromLatin1(name);
        const int id = QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/emoji.ttf"));
        const QStringList bundled = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
        return bundled.isEmpty() ? QString() : bundled.first();
    }();
    return family;
}

QFont fontFor(const BoardItem &item)
{
    QFont f = QGuiApplication::font();
    if (const QString emoji = emojiFamily(); !emoji.isEmpty())
        f.setFamilies({ f.family(), emoji });   // emoji glyphs fall back to the colour-emoji font
    f.setPixelSize(qMax(1, qRound(item.fontSize)));
    f.setBold(item.bold);
    f.setHintingPreference(QFont::PreferNoHinting);   // identical metrics on screen, PDF and SVG
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QRectF textRect(const BoardItem &item)
{
    const QRectF r = item.rect;
    switch (item.type) {
    case BoardItem::Type::Diamond:
        return QRectF(r.left() + r.width() / 4, r.top() + r.height() / 4, r.width() / 2, r.height() / 2);
    case BoardItem::Type::Triangle:
        return QRectF(r.left() + r.width() / 4, r.top() + r.height() / 2, r.width() / 2, r.height() / 2 - 6);
    case BoardItem::Type::Ellipse:
        return r.adjusted(r.width() * 0.15, r.height() * 0.15, -r.width() * 0.15, -r.height() * 0.15);
    case BoardItem::Type::Text:
        return r.adjusted(2, 2, -2, -2);
    case BoardItem::Type::Frame:
        return QRectF(r.left(), r.top() - item.fontSize * 1.8, r.width(), item.fontSize * 1.6);
    default:
        return r.adjusted(12, 12, -12, -12);
    }
}

TextLayoutResult layoutText(const BoardItem &item)
{
    TextLayoutResult res;
    res.font = fontFor(item);
    if (item.text.isEmpty())
        return res;

    const QRectF box = textRect(item);
    const double width = qMax(1.0, box.width());
    QString text = item.text;
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);

    QTextLayout layout(text, res.font);
    QTextOption opt;
    opt.setWrapMode(item.type == BoardItem::Type::Frame ? QTextOption::NoWrap
                                                        : QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(opt);
    layout.beginLayout();
    double y = 0;
    QList<QTextLine> lines;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(width);
        line.setPosition(QPointF(0, y));
        y += line.height();
        lines.append(line);
    }
    layout.endLayout();
    res.contentHeight = y;

    // Vertical placement: frames/text top-aligned, everything else centered.
    double top = box.top();
    const bool centerV = item.type != BoardItem::Type::Text && item.type != BoardItem::Type::Frame;
    if (centerV)
        top = box.top() + qMax(0.0, (box.height() - y) / 2.0);

    const int align = item.type == BoardItem::Type::Frame ? 1
                    : item.type == BoardItem::Type::Text && item.textAlign == 0 ? 1
                    : item.textAlign;
    for (const QTextLine &line : lines) {
        const QString s = text.mid(line.textStart(), line.textLength()).trimmed();
        const double w = line.naturalTextWidth();
        double x = box.left();
        if (align == 0)
            x = box.left() + (width - w) / 2.0;
        else if (align == 2)
            x = box.right() - w;
        res.lines.append({ s, QPointF(x, top + line.y() + line.ascent()), w });
    }
    return res;
}

QPainterPath penPath(const BoardItem &item)
{
    QPainterPath path;
    const QList<QPointF> &pts = item.points;
    if (pts.isEmpty())
        return path;
    const QPointF o = item.rect.topLeft();
    path.moveTo(o + pts.first());
    if (pts.size() == 1) {
        path.lineTo(o + pts.first() + QPointF(0.01, 0));
        return path;
    }
    // Quadratic smoothing through segment midpoints.
    for (qsizetype i = 1; i + 1 < pts.size(); ++i) {
        const QPointF mid = (pts[i] + pts[i + 1]) / 2.0;
        path.quadTo(o + pts[i], o + mid);
    }
    path.lineTo(o + pts.last());
    return path;
}

QPointF connectorLabelPoint(const QPolygonF &path)
{
    double total = 0;
    for (qsizetype i = 0; i + 1 < path.size(); ++i)
        total += QLineF(path[i], path[i + 1]).length();
    double half = total / 2;
    for (qsizetype i = 0; i + 1 < path.size(); ++i) {
        const QLineF seg(path[i], path[i + 1]);
        if (half <= seg.length())
            return seg.pointAt(seg.length() > 0 ? half / seg.length() : 0);
        half -= seg.length();
    }
    return path.isEmpty() ? QPointF() : path.last();
}

QPolygonF arrowHead(const QPointF &tip, const QPointF &from, double strokeWidth)
{
    QLineF line(tip, from);
    if (line.length() < 0.001)
        return {};
    const double size = 10.0 + strokeWidth * 2.0;
    const double angle = std::atan2(from.y() - tip.y(), from.x() - tip.x());
    const double spread = qDegreesToRadians(25.0);
    QPolygonF poly;
    poly << tip
         << tip + QPointF(std::cos(angle + spread), std::sin(angle + spread)) * size
         << tip + QPointF(std::cos(angle - spread), std::sin(angle - spread)) * size;
    return poly;
}

static void drawText(QPainter &p, const BoardItem &item, const QColor &color)
{
    const TextLayoutResult tl = layoutText(item);
    if (tl.lines.isEmpty())
        return;
    p.save();
    p.setFont(tl.font);
    p.setPen(color);
    if (item.type != BoardItem::Type::Frame && item.type != BoardItem::Type::Text)
        p.setClipRect(item.rect, Qt::IntersectClip);
    for (const TextLine &line : tl.lines)
        p.drawText(line.baseline, line.text);
    p.restore();
}

void paintItem(QPainter &p, const Document &doc, const BoardItem &item, const Options &opt)
{
    p.save();
    QPen pen(item.stroke, item.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (item.dash == 1)
        pen.setDashPattern({ 4.0, 3.0 });                // dashes, in multiples of the line width
    else if (item.dash == 2)
        pen.setDashPattern({ 0.01, 2.5 });               // round dots (RoundCap)
    if (item.strokeWidth <= 0 || item.stroke.alpha() == 0)
        pen = Qt::NoPen;

    switch (item.type) {
    case BoardItem::Type::Rectangle:
    case BoardItem::Type::Ellipse:
    case BoardItem::Type::Diamond:
    case BoardItem::Type::Triangle:
    case BoardItem::Type::RoundedRect:
    case BoardItem::Type::Shape:
        p.setPen(pen);
        p.setBrush(item.fill.alpha() ? QBrush(item.fill) : QBrush(Qt::NoBrush));
        p.drawPath(shapeOutline(item));
        drawText(p, item, item.textColor);
        break;

    case BoardItem::Type::Sticky: {
        // Soft shadow, then the note.
        p.setPen(Qt::NoPen);
        for (int i = 3; i >= 1; --i) {
            p.setBrush(QColor(0, 0, 0, 10 * (4 - i)));
            p.drawRect(item.rect.translated(i, i + 2).adjusted(1, 1, 0, 0));
        }
        p.setBrush(item.fill);
        p.drawRect(item.rect);
        drawText(p, item, item.textColor);
        break;
    }

    case BoardItem::Type::Text:
        drawText(p, item, item.textColor);
        break;

    case BoardItem::Type::Frame: {
        p.setPen(QPen(QColor(0, 0, 0, 40), 1.0));
        p.setBrush(item.fill.alpha() ? QBrush(item.fill) : QBrush(Qt::NoBrush));
        p.drawRect(item.rect);
        if (opt.drawFrameTitles) {
            BoardItem titleItem = item;
            if (titleItem.text.isEmpty())
                titleItem.text = QStringLiteral("Frame");
            drawText(p, titleItem, opt.frameTitleColor);
        }
        break;
    }

    case BoardItem::Type::Pen: {
        QPen pp(item.stroke, item.strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pp);
        p.setBrush(Qt::NoBrush);
        p.drawPath(penPath(item));
        break;
    }

    case BoardItem::Type::Connector: {
        const QPolygonF path = doc.connectorPath(item);
        if (path.size() < 2)
            break;
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(path);
        p.setPen(Qt::NoPen);
        p.setBrush(item.stroke);
        if (item.arrowEnd)
            p.drawPolygon(arrowHead(path.last(), path[path.size() - 2], item.strokeWidth));
        if (item.arrowStart)
            p.drawPolygon(arrowHead(path.first(), path[1], item.strokeWidth));
        if (!item.text.isEmpty()) {
            // Label on a small white pill at the middle of the line.
            const QFont f = fontFor(item);
            const QFontMetricsF fm(f);
            const QPointF mid = connectorLabelPoint(path);
            const QRectF box(mid.x() - fm.horizontalAdvance(item.text) / 2 - 6, mid.y() - fm.height() / 2 - 2,
                             fm.horizontalAdvance(item.text) + 12, fm.height() + 4);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::white);
            p.drawRoundedRect(box, 6, 6);
            p.setFont(f);
            p.setPen(item.textColor);
            p.drawText(box, Qt::AlignCenter, item.text);
        }
        break;
    }

    case BoardItem::Type::Icon: {
        QPen ip(item.stroke, qMax(0.5, item.strokeWidth), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(ip);
        p.setBrush(item.fill.alpha() ? QBrush(item.fill) : QBrush(Qt::NoBrush));
        p.drawPath(ShapeLibrary::iconPath(item.shapeId, item.rect));
        break;
    }

    case BoardItem::Type::Image: {
        const QImage img = doc.assetImage(item.assetKey);
        if (img.isNull()) {
            p.setPen(QPen(Qt::gray, 1, Qt::DashLine));
            p.setBrush(QColor(240, 240, 240));
            p.drawRect(item.rect);
        } else {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.setRenderHint(QPainter::LosslessImageRendering, opt.losslessImages);
            if (item.flipH || item.flipV) {
                const QPointF c = item.rect.center();
                p.translate(c);
                p.scale(item.flipH ? -1 : 1, item.flipV ? -1 : 1);
                p.translate(-c);
            }
            const QRectF src(item.crop.x() * img.width(), item.crop.y() * img.height(),
                             item.crop.width() * img.width(), item.crop.height() * img.height());
            p.drawImage(item.rect, img, src);
        }
        break;
    }
    }
    p.restore();
}

void paintDocument(QPainter &p, const Document &doc, const QRectF &worldClip, const Options &opt)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setRenderHint(QPainter::LosslessImageRendering, opt.losslessImages);
    for (const BoardItem &item : doc.items) {
        if (!doc.itemVisible(item) || item.type == BoardItem::Type::Comment)
            continue;   // hidden layer / comment pins: not drawn here, not exported
        if (!worldClip.isNull() && !worldClip.intersects(doc.itemBounds(item)))
            continue;
        if (opt.hideTextIds.contains(item.id)) {
            BoardItem noText = item;
            noText.text.clear();
            if (noText.type == BoardItem::Type::Frame)
                noText.text = QStringLiteral(" ");
            paintItem(p, doc, noText, opt);
            continue;
        }
        paintItem(p, doc, item, opt);
    }
}

} // namespace Renderer
