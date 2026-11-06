#pragma once

#include "BoardCanvas.h"
#include "Templates.h"

#include <QQuickImageProvider>

// image://templates/<id>  ->  rendered preview of a built-in template (home screen cards).
class TemplatePreviewProvider : public QQuickImageProvider
{
public:
    TemplatePreviewProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        const QSize s = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(360, 220);
        const QString name = id.section(QLatin1Char('?'), 0, 0);
        const Document doc = name.startsWith(QLatin1String("custom:")) ? BoardCanvas::loadCustomTemplate(name)
                                                                        : Templates::create(name);
        QImage img = renderPreview(doc, s, QColor(245, 246, 248));
        if (size)
            *size = img.size();
        return img;
    }
};
