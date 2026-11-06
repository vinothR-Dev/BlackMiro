#pragma once

#include "Document.h"

#include <QByteArray>
#include <QColor>

// Hand-written SVG output: shapes become real <path>s, text stays real,
// selectable <text>, and images are embedded as their original file bytes
// (true lossless) rather than being re-rasterized.
namespace SvgWriter {

struct Options
{
    QRectF area;                       // world rect to export
    QColor background = Qt::white;     // transparent => no background rect
    bool minify = false;
};

QByteArray write(const Document &doc, const Options &opt);

} // namespace SvgWriter
