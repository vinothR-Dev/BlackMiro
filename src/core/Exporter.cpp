#include "Exporter.h"

#include "BoardFile.h"
#include "PdfEncrypt.h"
#include "Renderer.h"
#include "SvgWriter.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QSaveFile>
#include <QtMath>

#include <private/qzipwriter_p.h>

namespace Exporter {

namespace {

// Resolved settings for one compression level.
struct Settings
{
    int dpi = 300;             // raster output resolution
    int quality = 100;         // lossy encoders; 100 = lossless where supported
    bool sixteenBit = false;   // 16 bits per channel (PNG / TIFF)
    bool palette = false;      // reduce to 256 colors (PNG / TIFF)
    int pngCompression = 100;  // Qt scale 0..100 -> zlib 0..9 (always lossless)
    int imageDpi = 0;          // embedded images: 0 = keep original file untouched
    int imageQuality = 100;    // embedded images re-encode quality
    bool keepHistory = true;
    bool minify = false;
};

Settings resolve(const Options &opt)
{
    Settings s;
    switch (opt.compression) {
    case Compression::Low:      // lossless, best quality
        s = { 600, 100, true, false, 100, 0, 100, true, false };
        break;
    case Compression::Medium:   // average quality
        s = { 300, 85, false, false, 70, 150, 85, true, false };
        break;
    case Compression::High:     // low quality, smallest files
        s = { 150, 60, false, true, 100, 96, 60, false, true };
        break;
    case Compression::Custom:
        s.dpi = qBound(24, opt.customDpi, 2400);
        s.quality = qBound(1, opt.customQuality, 100);
        s.sixteenBit = opt.custom16Bit;
        s.imageDpi = s.quality >= 100 ? 0 : s.dpi;
        s.imageQuality = s.quality;
        break;
    }
    return s;
}

struct Page
{
    QString suffix;            // empty for single-page exports
    QRectF area;
    const Document *doc = nullptr;
};

QString sanitize(QString s)
{
    s = s.trimmed();
    s.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\s]+)")), QStringLiteral("-"));
    s = s.left(60);
    return s.isEmpty() ? QStringLiteral("untitled") : s;
}

bool hasAlpha(const QImage &img)
{
    if (!img.hasAlphaChannel())
        return false;
    const QImage a = img.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < a.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        for (int x = 0; x < a.width(); ++x)
            if (qAlpha(line[x]) != 255)
                return true;
    }
    return false;
}

QByteArray encodeImage(const QImage &img, const char *format, int quality)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    QImageWriter w(&buf, format);
    w.setQuality(quality);
    if (!w.write(img))
        return {};
    return out;
}

// Downscale / re-encode embedded images for Medium & High. Low keeps originals.
void processImages(Document &doc, const Settings &s, QStringList &warnings)
{
    doc.clearImageCache();
    if (s.imageDpi <= 0)
        return;

    QHash<QString, QSizeF> displaySize;
    for (const BoardItem &it : doc.items) {
        if (it.type != BoardItem::Type::Image)
            continue;
        QSizeF &sz = displaySize[it.assetKey];
        // Size the *whole* image would have on screen, so the visible (cropped) part keeps its DPI.
        const QSizeF full(it.rect.width() / qMax(1e-6, it.crop.width()), it.rect.height() / qMax(1e-6, it.crop.height()));
        sz = sz.expandedTo(full);
    }

    const bool webp = QImageWriter::supportedImageFormats().contains("webp");
    for (auto it = displaySize.constBegin(); it != displaySize.constEnd(); ++it) {
        auto asset = doc.assets.find(it.key());
        if (asset == doc.assets.end())
            continue;
        QImage img = QImage::fromData(asset->data);
        if (img.isNull()) {
            warnings << QStringLiteral("Could not decode an embedded image; kept original.");
            continue;
        }
        const double f = s.imageDpi / 96.0;
        const int tw = qMax(1, qCeil(it.value().width() * f));
        const int th = qMax(1, qCeil(it.value().height() * f));
        const bool scaled = img.width() > tw || img.height() > th;
        if (scaled)
            img = img.scaled(qMin(tw, img.width()), qMin(th, img.height()),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QByteArray data;
        QByteArray fmt;
        if (hasAlpha(img)) {
            if (s.imageQuality < 80 && webp) {
                data = encodeImage(img, "webp", s.imageQuality);
                fmt = "webp";
            } else {
                data = encodeImage(img, "png", 0);
                fmt = "png";
            }
        } else {
            data = encodeImage(img.convertToFormat(QImage::Format_RGB32), "jpeg", s.imageQuality);
            fmt = "jpeg";
        }
        if (data.isEmpty())
            continue;
        if (!scaled && data.size() >= asset->data.size())
            continue; // re-encoding would not help; keep the original bytes
        asset->data = data;
        asset->format = fmt;
    }
}

Document subDocument(const Document &doc, const QStringList &ids)
{
    Document sub;
    sub.title = doc.title;
    for (const BoardItem &it : doc.items) {
        bool keep = ids.contains(it.id);
        if (!keep && it.type == BoardItem::Type::Connector)
            keep = !it.fromId.isEmpty() && !it.toId.isEmpty() && ids.contains(it.fromId) && ids.contains(it.toId);
        if (!keep)
            continue;
        BoardItem copy = it;
        // Detach connector ends whose targets are not exported.
        if (copy.type == BoardItem::Type::Connector) {
            if (!copy.fromId.isEmpty() && !ids.contains(copy.fromId)) {
                copy.fromPoint = doc.connectorStart(it);
                copy.fromId.clear();
            }
            if (!copy.toId.isEmpty() && !ids.contains(copy.toId)) {
                copy.toPoint = doc.connectorEnd(it);
                copy.toId.clear();
            }
        }
        sub.items.append(copy);
        if (!copy.assetKey.isEmpty() && doc.assets.contains(copy.assetKey))
            sub.assets.insert(copy.assetKey, doc.assets.value(copy.assetKey));
    }
    return sub;
}

QImage renderRaster(const Page &page, const Settings &s, bool transparent, bool forceOpaque,
                    QStringList &warnings)
{
    double scale = s.dpi / 96.0;
    const double maxPixels = s.sixteenBit ? 40e6 : 100e6;
    double pixels = page.area.width() * scale * page.area.height() * scale;
    if (pixels > maxPixels) {
        scale *= std::sqrt(maxPixels / pixels);
        warnings << QStringLiteral("Area too large for %1 DPI; rendered at %2 DPI to stay within memory limits.")
                        .arg(s.dpi).arg(qRound(scale * 96));
    }
    const double maxSide = 32000.0;
    const double side = qMax(page.area.width(), page.area.height()) * scale;
    if (side > maxSide) {
        scale *= maxSide / side;
        warnings << QStringLiteral("Area is very long; rendered at %1 DPI.").arg(qRound(scale * 96));
    }

    const QSize size(qMax(1, qCeil(page.area.width() * scale)), qMax(1, qCeil(page.area.height() * scale)));
    QImage img(size, s.sixteenBit ? QImage::Format_RGBA64_Premultiplied : QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
        warnings << QStringLiteral("Not enough memory to render %1x%2 px.").arg(size.width()).arg(size.height());
        return {};
    }
    img.fill(transparent && !forceOpaque ? Qt::transparent : Qt::white);
    const int dpm = qRound(scale * 96 / 0.0254);
    img.setDotsPerMeterX(dpm);
    img.setDotsPerMeterY(dpm);

    QPainter p(&img);
    p.scale(scale, scale);
    p.translate(-page.area.topLeft());
    Renderer::Options ro;
    ro.losslessImages = s.imageDpi == 0;
    Renderer::paintDocument(p, *page.doc, page.area, ro);
    p.end();
    return img;
}

QByteArray encodeRaster(const QImage &src, Format f, const Settings &s, QStringList &warnings)
{
    QImage img = src;
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);

    QByteArray fmt;
    switch (f) {
    case Format::Png: fmt = "png"; break;
    case Format::Tiff: fmt = "tiff"; break;
    case Format::WebP: fmt = "webp"; break;
    case Format::Jpeg: fmt = "jpeg"; break;
    case Format::Avif: fmt = "avif"; break;
    default: return {};
    }
    QImageWriter w(&buf, fmt);

    switch (f) {
    case Format::Png:
        if (s.palette)
            img = img.convertToFormat(QImage::Format_ARGB32).convertToFormat(
                QImage::Format_Indexed8, Qt::DiffuseDither | Qt::PreferDither);
        else if (s.sixteenBit)
            img = img.convertToFormat(QImage::Format_RGBA64);
        else
            img = img.convertToFormat(QImage::Format_ARGB32);
        w.setCompression(s.pngCompression);
        break;
    case Format::Tiff:
        if (s.palette)
            img = img.convertToFormat(QImage::Format_ARGB32).convertToFormat(
                QImage::Format_Indexed8, Qt::DiffuseDither | Qt::PreferDither);
        else if (s.sixteenBit)
            img = img.convertToFormat(QImage::Format_RGBA64);
        else
            img = img.convertToFormat(QImage::Format_ARGB32);
        w.setCompression(1); // LZW, lossless
        break;
    case Format::WebP:
        img = img.convertToFormat(QImage::Format_ARGB32);
        w.setQuality(s.quality >= 100 ? 100 : s.quality); // 100 => lossless WebP
        break;
    case Format::Jpeg:
        img = img.convertToFormat(QImage::Format_RGB32);
        w.setQuality(s.quality);                         // >90 disables chroma subsampling
        w.setOptimizedWrite(true);
        w.setProgressiveScanWrite(s.quality < 100);
        break;
    case Format::Avif:
        img = img.convertToFormat(QImage::Format_ARGB32);
        w.setQuality(s.quality);
        break;
    default:
        break;
    }

    if (!w.write(img)) {
        warnings << QStringLiteral("%1 encoding failed: %2").arg(QString::fromLatin1(fmt).toUpper(), w.errorString());
        return {};
    }
    return out;
}

QByteArray renderPdf(const QList<Page> &pages, const Settings &s, bool transparent, const QString &title)
{
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    QPdfWriter writer(&buf);
    writer.setCreator(QStringLiteral("Miru"));
    writer.setTitle(title);
    writer.setResolution(1200);   // vector coordinate precision, not image resolution
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));

    constexpr double kMaxPagePt = 14400.0; // PDF viewer limit (200 in)
    QPainter p;
    for (qsizetype i = 0; i < pages.size(); ++i) {
        const Page &page = pages[i];
        QSizeF pt = page.area.size() * (72.0 / 96.0);
        const double fit = qMin(1.0, kMaxPagePt / qMax(pt.width(), pt.height()));
        pt *= fit;
        writer.setPageSize(QPageSize(pt, QPageSize::Point, QString(), QPageSize::ExactMatch));
        writer.setPageMargins(QMarginsF(0, 0, 0, 0));
        if (i == 0) {
            if (!p.begin(&writer))
                return {};
        } else {
            writer.newPage();
        }
        p.resetTransform();
        const double scale = writer.resolution() / 96.0 * fit;
        p.scale(scale, scale);
        p.translate(-page.area.topLeft());
        if (!transparent)
            p.fillRect(page.area, Qt::white);
        Renderer::Options ro;
        ro.losslessImages = s.imageDpi == 0;   // Low: Flate (lossless). Otherwise JPEG.
        Renderer::paintDocument(p, *page.doc, page.area, ro);
    }
    p.end();
    return out;
}

QString csvField(QString s)
{
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
        s.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

QByteArray renderCsv(const Document &doc)
{
    QString out = QStringLiteral("type,text,x,y,width,height,fill,stroke,frame\n");
    QList<const BoardItem *> frames;
    for (const BoardItem &it : doc.items)
        if (it.type == BoardItem::Type::Frame)
            frames << &it;
    for (const BoardItem &it : doc.items) {
        if (it.type == BoardItem::Type::Pen || it.type == BoardItem::Type::Connector)
            continue;
        QString frame;
        for (const BoardItem *f : frames)
            if (f->id != it.id && f->rect.contains(it.rect))
                frame = f->text;
        out += QStringList{
            BoardItem::typeName(it.type), csvField(it.text),
            QString::number(it.rect.x(), 'f', 1), QString::number(it.rect.y(), 'f', 1),
            QString::number(it.rect.width(), 'f', 1), QString::number(it.rect.height(), 'f', 1),
            it.fill.name(QColor::HexArgb), it.stroke.name(QColor::HexArgb), csvField(frame)
        }.join(QLatin1Char(',')) + QLatin1Char('\n');
    }
    return out.toUtf8();
}

QString labelOf(const BoardItem &it)
{
    const QString t = it.text.simplified();
    return t.isEmpty() ? QStringLiteral("(%1)").arg(BoardItem::typeName(it.type)) : t;
}

QByteArray renderMarkdown(const Document &doc)
{
    QString out = QStringLiteral("# %1\n\n").arg(doc.title);
    QSet<QString> placed;
    for (const BoardItem &f : doc.items) {
        if (f.type != BoardItem::Type::Frame)
            continue;
        out += QStringLiteral("## %1\n\n").arg(f.text.isEmpty() ? QStringLiteral("Frame") : f.text);
        // Reading order: top-to-bottom, then left-to-right.
        QList<const BoardItem *> inside;
        for (const BoardItem &it : doc.items)
            if (it.id != f.id && it.hasText() && it.type != BoardItem::Type::Frame && f.rect.contains(it.rect))
                inside << &it;
        std::sort(inside.begin(), inside.end(), [](const BoardItem *a, const BoardItem *b) {
            return qFuzzyCompare(a->rect.top(), b->rect.top()) ? a->rect.left() < b->rect.left()
                                                              : a->rect.top() < b->rect.top();
        });
        for (const BoardItem *it : inside) {
            out += QStringLiteral("- %1\n").arg(labelOf(*it));
            placed.insert(it->id);
        }
        out += QLatin1Char('\n');
    }
    QString loose;
    for (const BoardItem &it : doc.items)
        if (it.hasText() && it.type != BoardItem::Type::Frame && !placed.contains(it.id) && !it.text.isEmpty())
            loose += QStringLiteral("- %1\n").arg(labelOf(it));
    if (!loose.isEmpty())
        out += QStringLiteral("## Board\n\n") + loose + QLatin1Char('\n');

    QString links;
    for (const BoardItem &c : doc.items) {
        if (c.type != BoardItem::Type::Connector || c.fromId.isEmpty() || c.toId.isEmpty())
            continue;
        const BoardItem *a = doc.find(c.fromId);
        const BoardItem *b = doc.find(c.toId);
        if (a && b)
            links += QStringLiteral("- %1 → %2%3\n").arg(labelOf(*a), labelOf(*b),
                                                         c.text.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(c.text));
    }
    if (!links.isEmpty())
        out += QStringLiteral("## Connections\n\n") + links;
    return out.toUtf8();
}

QString uniquePath(const QString &path)
{
    if (!QFileInfo::exists(path))
        return path;
    const QFileInfo fi(path);
    for (int i = 2; i < 10000; ++i) {
        const QString candidate = fi.dir().filePath(
            QStringLiteral("%1 (%2).%3").arg(fi.completeBaseName()).arg(i).arg(fi.suffix()));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return path;
}

bool writeFile(const QString &path, const QByteArray &data, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        if (error) *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

} // namespace

QList<Format> allFormats()
{
    return { Format::Board, Format::Pdf, Format::Svg, Format::Png, Format::Tiff, Format::WebP,
             Format::Jpeg, Format::Avif, Format::Csv, Format::Markdown, Format::Json };
}

QString formatName(Format f)
{
    switch (f) {
    case Format::Board: return QStringLiteral("BOARD");
    case Format::Pdf: return QStringLiteral("PDF");
    case Format::Svg: return QStringLiteral("SVG");
    case Format::Png: return QStringLiteral("PNG");
    case Format::Tiff: return QStringLiteral("TIFF");
    case Format::WebP: return QStringLiteral("WEBP");
    case Format::Jpeg: return QStringLiteral("JPEG");
    case Format::Avif: return QStringLiteral("AVIF");
    case Format::Csv: return QStringLiteral("CSV");
    case Format::Markdown: return QStringLiteral("MARKDOWN");
    case Format::Json: return QStringLiteral("JSON");
    }
    return {};
}

QString extension(Format f)
{
    switch (f) {
    case Format::Board: return QStringLiteral("board");
    case Format::Pdf: return QStringLiteral("pdf");
    case Format::Svg: return QStringLiteral("svg");
    case Format::Png: return QStringLiteral("png");
    case Format::Tiff: return QStringLiteral("tiff");
    case Format::WebP: return QStringLiteral("webp");
    case Format::Jpeg: return QStringLiteral("jpg");
    case Format::Avif: return QStringLiteral("avif");
    case Format::Csv: return QStringLiteral("csv");
    case Format::Markdown: return QStringLiteral("md");
    case Format::Json: return QStringLiteral("json");
    }
    return {};
}

bool isFormatAvailable(Format f)
{
    const auto fmts = QImageWriter::supportedImageFormats();
    switch (f) {
    case Format::Tiff: return fmts.contains("tiff");
    case Format::WebP: return fmts.contains("webp");
    case Format::Avif: return fmts.contains("avif");
    case Format::Jpeg: return fmts.contains("jpeg");
    default: return true;
    }
}

QList<OutFile> render(const Document &source, const Options &opt, QStringList &warnings)
{
    const Settings s = resolve(opt);
    QList<OutFile> files;

    // Working copy with images processed for the chosen compression level.
    Document doc = source;
    processImages(doc, s, warnings);

    Document selectionDoc;
    QList<Page> pages;
    const Document *exportDoc = &doc;

    switch (opt.scope) {
    case Scope::Selection:
        selectionDoc = subDocument(doc, opt.selection);
        if (selectionDoc.items.isEmpty()) {
            warnings << QStringLiteral("Nothing selected; exported the whole board instead.");
        } else {
            exportDoc = &selectionDoc;
        }
        break;
    case Scope::EachFrame: {
        int n = 0;
        for (const BoardItem &it : doc.items) {
            if (it.type != BoardItem::Type::Frame)
                continue;
            ++n;
            const QString name = it.text.isEmpty() ? QStringLiteral("frame-%1").arg(n) : sanitize(it.text);
            pages.append({ QStringLiteral("%1-%2").arg(n, 2, 10, QLatin1Char('0')).arg(name),
                           doc.itemBounds(it).adjusted(-8, -8, 8, 8), &doc });
        }
        if (pages.isEmpty())
            warnings << QStringLiteral("The board has no frames; exported the whole board instead.");
        break;
    }
    case Scope::WholeBoard:
        break;
    }

    if (pages.isEmpty()) {
        const QRectF bounds = exportDoc->contentBounds();
        if (bounds.isNull())
            return files;
        pages.append({ QString(), bounds.adjusted(-opt.padding, -opt.padding, opt.padding, opt.padding), exportDoc });
    }

    const bool multiFormat = opt.zip;
    auto pathFor = [&](Format f, const QString &suffix) {
        const QString name = suffix.isEmpty() ? opt.baseName : opt.baseName + QLatin1Char('_') + suffix;
        const QString file = name + QLatin1Char('.') + extension(f);
        return multiFormat ? formatName(f) + QLatin1Char('/') + file : file;
    };

    for (Format f : opt.formats) {
        if (!isFormatAvailable(f)) {
            warnings << QStringLiteral("%1 skipped: this build has no %1 encoder plugin.").arg(formatName(f));
            continue;
        }
        switch (f) {
        case Format::Board: {
            // Low keeps the source untouched (bit-exact images + full history).
            const Document &boardDoc = opt.scope == Scope::Selection && exportDoc == &selectionDoc
                                           ? selectionDoc
                                           : (opt.compression == Compression::Low ? source : doc);
            files.append({ pathFor(f, {}), BoardFile::save(boardDoc, s.keepHistory, opt.password) });
            break;
        }
        case Format::Pdf: {
            QByteArray pdf = renderPdf(pages, s, opt.transparentBackground, source.title);
            if (!pdf.isEmpty() && !opt.password.isEmpty()) {
                QString err;
                pdf = PdfEncrypt::encrypt(pdf, opt.password, &err);
                if (pdf.isEmpty()) {
                    warnings << QStringLiteral("PDF not exported: %1").arg(err);
                    break;
                }
            }
            if (pdf.isEmpty())
                warnings << QStringLiteral("PDF rendering failed.");
            else
                files.append({ pathFor(f, {}), pdf });
            break;
        }
        case Format::Svg:
            for (const Page &page : pages) {
                SvgWriter::Options so;
                so.area = page.area;
                so.background = opt.transparentBackground ? QColor(Qt::transparent) : QColor(Qt::white);
                so.minify = s.minify;
                files.append({ pathFor(f, page.suffix), SvgWriter::write(*page.doc, so) });
            }
            break;
        case Format::Png:
        case Format::Tiff:
        case Format::WebP:
        case Format::Jpeg:
        case Format::Avif:
            for (const Page &page : pages) {
                const QImage img = renderRaster(page, s, opt.transparentBackground, f == Format::Jpeg, warnings);
                if (img.isNull())
                    continue;
                const QByteArray data = encodeRaster(img, f, s, warnings);
                if (!data.isEmpty())
                    files.append({ pathFor(f, page.suffix), data });
            }
            break;
        case Format::Csv:
            files.append({ pathFor(f, {}), renderCsv(*exportDoc) });
            break;
        case Format::Markdown:
            files.append({ pathFor(f, {}), renderMarkdown(*exportDoc) });
            break;
        case Format::Json: {
            QJsonObject root;
            root["title"] = exportDoc->title;
            root["items"] = exportDoc->itemsToJson();
            files.append({ pathFor(f, {}), QJsonDocument(root).toJson(s.minify ? QJsonDocument::Compact
                                                                              : QJsonDocument::Indented) });
            break;
        }
        }
    }
    return files;
}

OutFile bundle(const QList<OutFile> &files, const Options &opt)
{
    if (files.size() == 1 && !opt.zip) {
        OutFile single = files.first();
        single.path = QFileInfo(single.path).fileName();
        return single;
    }
    QByteArray zipData;
    QBuffer buf(&zipData);
    buf.open(QIODevice::WriteOnly);
    QZipWriter zip(&buf);
    // Already-compressed formats are stored; text formats get deflated. Both lossless.
    zip.setCompressionPolicy(QZipWriter::AutoCompress);
    for (const OutFile &f : files)
        zip.addFile(f.path, f.data);
    zip.close();
    return { opt.baseName + QStringLiteral("_export.zip"), zipData };
}

Result exportDocument(const Document &doc, const Options &opt)
{
    Result res;
    if (opt.formats.isEmpty()) {
        res.error = QStringLiteral("Choose at least one format.");
        return res;
    }
    if (doc.items.isEmpty()) {
        res.error = QStringLiteral("The board is empty.");
        return res;
    }
    QDir dir(opt.outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        res.error = QStringLiteral("Cannot create folder %1").arg(opt.outputDir);
        return res;
    }

    const QList<OutFile> files = render(doc, opt, res.warnings);
    if (files.isEmpty()) {
        res.error = res.warnings.isEmpty() ? QStringLiteral("Nothing to export.") : res.warnings.join(QLatin1Char('\n'));
        return res;
    }

    if (opt.zip) {
        const OutFile zip = bundle(files, opt);
        const QString path = uniquePath(dir.filePath(zip.path));
        if (!writeFile(path, zip.data, &res.error))
            return res;
        res.files << path;
        return res;
    }

    for (const OutFile &f : files) {
        const QString path = uniquePath(dir.filePath(f.path));
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!writeFile(path, f.data, &res.error))
            return res;
        res.files << path;
    }
    return res;
}

} // namespace Exporter
