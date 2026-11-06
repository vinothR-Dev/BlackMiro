#include "SvgWriter.h"

#include "Renderer.h"
#include "ShapeLibrary.h"

#include <QFontInfo>

namespace SvgWriter {

namespace {

QString num(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0')) || s.endsWith(QLatin1Char('.'))))
        s.chop(1);
    return s;
}

QString esc(const QString &s)
{
    return s.toHtmlEscaped();
}

QString colorAttr(const char *name, const QColor &c)
{
    if (c.alpha() == 0)
        return QStringLiteral(" %1=\"none\"").arg(QLatin1String(name));
    QString out = QStringLiteral(" %1=\"%2\"").arg(QLatin1String(name), c.name(QColor::HexRgb));
    if (c.alpha() < 255)
        out += QStringLiteral(" %1-opacity=\"%2\"").arg(QLatin1String(name), num(c.alphaF()));
    return out;
}

QString pathData(const QPainterPath &path)
{
    QString d;
    for (int i = 0; i < path.elementCount(); ++i) {
        const QPainterPath::Element e = path.elementAt(i);
        switch (e.type) {
        case QPainterPath::MoveToElement:
            d += QStringLiteral("M%1 %2").arg(num(e.x), num(e.y));
            break;
        case QPainterPath::LineToElement:
            d += QStringLiteral("L%1 %2").arg(num(e.x), num(e.y));
            break;
        case QPainterPath::CurveToElement: {
            const auto c2 = path.elementAt(i + 1);
            const auto end = path.elementAt(i + 2);
            d += QStringLiteral("C%1 %2 %3 %4 %5 %6")
                     .arg(num(e.x), num(e.y), num(c2.x), num(c2.y), num(end.x), num(end.y));
            i += 2;
            break;
        }
        case QPainterPath::CurveToDataElement:
            break;
        }
    }
    return d;
}

QString polyPoints(const QPolygonF &poly)
{
    QStringList parts;
    for (const QPointF &p : poly)
        parts << num(p.x()) + QLatin1Char(',') + num(p.y());
    return parts.join(QLatin1Char(' '));
}

QString mimeFor(const QByteArray &format)
{
    if (format == "jpeg" || format == "jpg") return QStringLiteral("image/jpeg");
    if (format == "svg") return QStringLiteral("image/svg+xml");
    if (format.isEmpty()) return QStringLiteral("application/octet-stream");
    return QStringLiteral("image/") + QString::fromLatin1(format);
}

void writeText(QString &out, const BoardItem &item, const QColor &color, const QString &nl)
{
    const TextLayoutResult tl = Renderer::layoutText(item);
    if (tl.lines.isEmpty())
        return;
    const QString family = QFontInfo(tl.font).family();
    out += QStringLiteral("<text font-family=\"%1, sans-serif\" font-size=\"%2\"%3%4 xml:space=\"preserve\">")
               .arg(esc(family), num(tl.font.pixelSize()),
                    item.bold ? QStringLiteral(" font-weight=\"bold\"") : QString(),
                    colorAttr("fill", color));
    for (const TextLine &line : tl.lines)
        out += QStringLiteral("<tspan x=\"%1\" y=\"%2\">%3</tspan>")
                   .arg(num(line.baseline.x()), num(line.baseline.y()), esc(line.text));
    out += QStringLiteral("</text>") + nl;
}

} // namespace

QByteArray write(const Document &doc, const Options &opt)
{
    const QString nl = opt.minify ? QString() : QStringLiteral("\n");
    const QRectF a = opt.area;
    QString out;
    out += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") + nl;
    out += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" "
                          "width=\"%1\" height=\"%2\" viewBox=\"%3 %4 %1 %2\">")
               .arg(num(a.width()), num(a.height()), num(a.x()), num(a.y())) + nl;
    out += QStringLiteral("<title>%1</title>").arg(esc(doc.title)) + nl;
    if (opt.background.alpha() > 0)
        out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"%5/>")
                   .arg(num(a.x()), num(a.y()), num(a.width()), num(a.height()))
                   .arg(colorAttr("fill", opt.background)) + nl;

    for (const BoardItem &item : doc.items) {
        if (!doc.itemVisible(item) || item.type == BoardItem::Type::Comment || !a.intersects(doc.itemBounds(item)))
            continue;
        const QString strokeAttrs = item.strokeWidth > 0
            ? colorAttr("stroke", item.stroke) + QStringLiteral(" stroke-width=\"%1\" stroke-linejoin=\"round\" stroke-linecap=\"round\"").arg(num(item.strokeWidth))
            : QStringLiteral(" stroke=\"none\"");
        const QString dashAttr = item.dash == 1 ? QStringLiteral(" stroke-dasharray=\"%1 %2\"").arg(num(item.strokeWidth * 4), num(item.strokeWidth * 3))
                               : item.dash == 2 ? QStringLiteral(" stroke-dasharray=\"0.01 %1\"").arg(num(item.strokeWidth * 2.5))
                               : QString();
        const QRectF r = item.rect;

        switch (item.type) {
        case BoardItem::Type::Rectangle:
        case BoardItem::Type::Ellipse:
        case BoardItem::Type::Diamond:
        case BoardItem::Type::Triangle:
        case BoardItem::Type::RoundedRect:
        case BoardItem::Type::Shape:
            out += QStringLiteral("<path d=\"%1\"%2%3/>")
                       .arg(pathData(shapeOutline(item)), colorAttr("fill", item.fill), strokeAttrs) + nl;
            writeText(out, item, item.textColor, nl);
            break;

        case BoardItem::Type::Sticky:
            for (int i = 3; i >= 1; --i)
                out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" fill=\"#000\" fill-opacity=\"%5\"/>")
                           .arg(num(r.x() + i + 1), num(r.y() + i + 3), num(r.width() - 1), num(r.height() - 1),
                                num(0.04 * (4 - i))) + nl;
            out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"%5/>")
                       .arg(num(r.x()), num(r.y()), num(r.width()), num(r.height()), colorAttr("fill", item.fill)) + nl;
            writeText(out, item, item.textColor, nl);
            break;

        case BoardItem::Type::Text:
            writeText(out, item, item.textColor, nl);
            break;

        case BoardItem::Type::Frame: {
            out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\"%5 stroke=\"#000\" stroke-opacity=\"0.16\"/>")
                       .arg(num(r.x()), num(r.y()), num(r.width()), num(r.height()), colorAttr("fill", item.fill)) + nl;
            BoardItem title = item;
            if (title.text.isEmpty())
                title.text = QStringLiteral("Frame");
            writeText(out, title, QColor(80, 80, 80), nl);
            break;
        }

        case BoardItem::Type::Pen:
            out += QStringLiteral("<path d=\"%1\" fill=\"none\"%2/>")
                       .arg(pathData(Renderer::penPath(item)), strokeAttrs) + nl;
            break;

        case BoardItem::Type::Connector: {
            const QPolygonF path = doc.connectorPath(item);
            if (path.size() < 2)
                break;
            out += QStringLiteral("<polyline points=\"%1\" fill=\"none\"%2/>").arg(polyPoints(path), strokeAttrs + dashAttr) + nl;
            if (item.arrowEnd)
                out += QStringLiteral("<polygon points=\"%1\"%2/>")
                           .arg(polyPoints(Renderer::arrowHead(path.last(), path[path.size() - 2], item.strokeWidth)),
                                colorAttr("fill", item.stroke)) + nl;
            if (item.arrowStart)
                out += QStringLiteral("<polygon points=\"%1\"%2/>")
                           .arg(polyPoints(Renderer::arrowHead(path.first(), path[1], item.strokeWidth)),
                                colorAttr("fill", item.stroke)) + nl;
            if (!item.text.isEmpty()) {
                const QFont f = Renderer::fontFor(item);
                const QFontMetricsF fm(f);
                const QPointF mid = Renderer::connectorLabelPoint(path);
                const double w = fm.horizontalAdvance(item.text) + 12, h = fm.height() + 4;
                out += QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" rx=\"6\" fill=\"#ffffff\"/>")
                           .arg(num(mid.x() - w / 2), num(mid.y() - h / 2), num(w), num(h)) + nl;
                out += QStringLiteral("<text x=\"%1\" y=\"%2\" text-anchor=\"middle\" dominant-baseline=\"central\" font-family=\"%3, sans-serif\" font-size=\"%4\"%5>%6</text>")
                           .arg(num(mid.x()), num(mid.y()), esc(QFontInfo(f).family()), num(f.pixelSize()),
                                colorAttr("fill", item.textColor), esc(item.text)) + nl;
            }
            break;
        }

        case BoardItem::Type::Icon:
            out += QStringLiteral("<path d=\"%1\"%2%3/>")
                       .arg(pathData(ShapeLibrary::iconPath(item.shapeId, r)), colorAttr("fill", item.fill),
                            colorAttr("stroke", item.stroke)
                                + QStringLiteral(" stroke-width=\"%1\" stroke-linejoin=\"round\" stroke-linecap=\"round\"")
                                      .arg(num(qMax(0.5, item.strokeWidth)))) + nl;
            break;

        case BoardItem::Type::Image: {
            const auto asset = doc.assets.constFind(item.assetKey);
            if (asset == doc.assets.constEnd())
                break;
            const QImage img = doc.assetImage(item.assetKey);
            const double iw = qMax(1, img.width()), ih = qMax(1, img.height());
            QString open, close;
            if (item.flipH || item.flipV) {
                const QPointF c = r.center();
                open = QStringLiteral("<g transform=\"translate(%1 %2) scale(%3 %4) translate(%5 %6)\">")
                           .arg(num(c.x()), num(c.y()), item.flipH ? QStringLiteral("-1") : QStringLiteral("1"),
                                item.flipV ? QStringLiteral("-1") : QStringLiteral("1"), num(-c.x()), num(-c.y()));
                close = QStringLiteral("</g>");
            }
            out += open;
            // Nested viewport shows exactly the cropped part of the untouched original.
            out += QStringLiteral("<svg x=\"%1\" y=\"%2\" width=\"%3\" height=\"%4\" viewBox=\"%5 %6 %7 %8\" preserveAspectRatio=\"none\">")
                       .arg(num(r.x()), num(r.y()), num(r.width()), num(r.height()),
                            num(item.crop.x() * iw), num(item.crop.y() * ih),
                            num(item.crop.width() * iw), num(item.crop.height() * ih));
            out += QStringLiteral("<image width=\"%1\" height=\"%2\" preserveAspectRatio=\"none\"%3 href=\"data:%4;base64,")
                       .arg(num(iw), num(ih),
                            item.altText.isEmpty() ? QString() : QStringLiteral(" role=\"img\" aria-label=\"%1\"").arg(esc(item.altText)),
                            mimeFor(asset->format));
            out += QString::fromLatin1(asset->data.toBase64());
            out += QStringLiteral("\"/></svg>") + close + nl;
            break;
        }
        }
    }
    out += QStringLiteral("</svg>") + nl;
    return out.toUtf8();
}

} // namespace SvgWriter
