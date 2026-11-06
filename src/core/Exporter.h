#pragma once

#include "Document.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Exporter {

enum class Format { Board, Pdf, Svg, Png, Tiff, WebP, Jpeg, Avif, Csv, Markdown, Json };

// Low compression = lossless / best quality, Medium = average, High = smallest / low quality.
enum class Compression { Low, Medium, High, Custom };

enum class Scope { WholeBoard, Selection, EachFrame };

struct Options
{
    QList<Format> formats;
    Compression compression = Compression::Low;
    Scope scope = Scope::WholeBoard;
    QStringList selection;                 // item ids, for Scope::Selection
    QString outputDir;
    QString baseName = QStringLiteral("board");
    bool transparentBackground = false;
    bool zip = false;                      // bundle everything into one .zip (forced for "all formats")
    double padding = 32.0;                 // world units around the exported area
    QString password;                      // private boards: PDF and BOARD outputs are encrypted

    // Used only when compression == Custom
    int customDpi = 300;
    int customQuality = 90;                // 100 = lossless where the format supports it
    bool custom16Bit = false;
};

struct Result
{
    QStringList files;                     // written paths
    QStringList warnings;
    QString error;
    bool ok() const { return error.isEmpty(); }
};

struct OutFile
{
    QString path;                          // relative path inside output dir / zip
    QByteArray data;
};

QList<Format> allFormats();
QString formatName(Format f);
QString extension(Format f);
bool isFormatAvailable(Format f);          // e.g. AVIF needs an image plugin

Result exportDocument(const Document &doc, const Options &opt);

// Packs rendered files into one deliverable: the file itself when there is just
// one (and opt.zip is off), otherwise a ZIP named "<baseName>_export.zip".
// Used for browser downloads and Android's "save as" picker.
OutFile bundle(const QList<OutFile> &files, const Options &opt);

// Lower-level entry point used by exportDocument and tests: renders all files in memory.
QList<OutFile> render(const Document &doc, const Options &opt, QStringList &warnings);

} // namespace Exporter
