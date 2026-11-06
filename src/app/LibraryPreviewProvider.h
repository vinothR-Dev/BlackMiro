#pragma once

#include "ShapeLibrary.h"

#include <QPainter>
#include <QQuickImageProvider>
#include <QUrlQuery>

// image://shapes/<id>?fill=#ffffff&stroke=#1f2329   and   image://icons/<id>?stroke=#1f2329
// Panel previews drawn from the same vector data the board uses.
class LibraryPreviewProvider : public QQuickImageProvider
{
public:
    explicit LibraryPreviewProvider(bool icons)
        : QQuickImageProvider(QQuickImageProvider::Image), m_icons(icons) {}

    QImage requestImage(const QString &request, QSize *size, const QSize &requestedSize) override
    {
        const QSize s = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(64, 64);
        const QString id = request.section(QLatin1Char('?'), 0, 0);
        const QUrlQuery q(request.section(QLatin1Char('?'), 1));
        const QColor stroke(q.hasQueryItem("stroke") ? QLatin1Char('#') + q.queryItemValue("stroke") : QStringLiteral("#1f2329"));
        const QColor fill(q.hasQueryItem("fill") ? QLatin1Char('#') + q.queryItemValue("fill") : QStringLiteral("#ffffff"));

        QImage img(s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        const double pad = s.width() * 0.12;
        const QRectF box(pad, pad, s.width() - 2 * pad, s.height() - 2 * pad);
        if (m_icons) {
            p.setPen(QPen(stroke, qMax(1.2, s.width() / 24.0 * 1.6), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(ShapeLibrary::iconPath(id, box));
        } else {
            p.setPen(QPen(stroke, qMax(1.0, s.width() / 40.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(fill);
            p.drawPath(ShapeLibrary::shapePath(id, box));
        }
        p.end();
        if (size)
            *size = img.size();
        return img;
    }

private:
    bool m_icons;
};
