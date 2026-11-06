#include "ExportController.h"

#include "BoardCanvas.h"
#include "Exporter.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTimer>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_WASM
#include <private/qwasmlocalfileaccess_p.h>
#endif

ExportController::ExportController(QObject *parent)
    : QObject(parent)
{
}

void ExportController::setCanvas(BoardCanvas *c)
{
    if (m_canvas == c)
        return;
    m_canvas = c;
    emit canvasChanged();
}

QVariantList ExportController::formats() const
{
    QVariantList out;
    for (Exporter::Format f : Exporter::allFormats()) {
        QVariantMap m;
        m["name"] = Exporter::formatName(f);
        m["ext"] = Exporter::extension(f);
        m["available"] = Exporter::isFormatAvailable(f);
        out << m;
    }
    return out;
}

QString ExportController::describe(const QString &c, int dpi, int quality, bool sixteen) const
{
    if (c == QLatin1String("low"))
        return QStringLiteral("Lossless · best quality · largest files. Raster 600 DPI, 16-bit PNG/TIFF, "
                              "lossless WebP, original images embedded untouched, full version history.");
    if (c == QLatin1String("medium"))
        return QStringLiteral("Average quality · medium files. Raster 300 DPI, 8-bit, JPEG/WebP 85%, "
                              "embedded images resized to 150 DPI.");
    if (c == QLatin1String("high"))
        return QStringLiteral("Low quality · smallest files. Raster 150 DPI, 256-colour PNG/TIFF, JPEG/WebP 60%, "
                              "embedded images at 96 DPI, history removed.");
    return QStringLiteral("Custom: %1 DPI, quality %2%3%4.")
        .arg(dpi)
        .arg(quality)
        .arg(quality >= 100 ? QStringLiteral(" (lossless where supported)") : QString())
        .arg(sixteen ? QStringLiteral(", 16-bit") : QString());
}

namespace {

Exporter::Options optionsFrom(const QVariantMap &o, const QStringList &selection)
{
    Exporter::Options opt;
    const QStringList names = o.value("formats").toStringList();
    for (Exporter::Format f : Exporter::allFormats())
        if (names.contains(Exporter::formatName(f)))
            opt.formats << f;

    const QString c = o.value("compression").toString();
    opt.compression = c == QLatin1String("medium") ? Exporter::Compression::Medium
                    : c == QLatin1String("high") ? Exporter::Compression::High
                    : c == QLatin1String("custom") ? Exporter::Compression::Custom
                                                   : Exporter::Compression::Low;
    const QString scope = o.value("scope").toString();
    opt.scope = scope == QLatin1String("selection") ? Exporter::Scope::Selection
              : scope == QLatin1String("frames") ? Exporter::Scope::EachFrame
                                                 : Exporter::Scope::WholeBoard;
    opt.selection = selection;

    // Accepts a plain path or a file:// URL.
    QString dir = o.value("folder").toString().trimmed();
    if (dir.startsWith(QLatin1String("file:")))
        dir = QUrl(dir).toLocalFile();
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    opt.outputDir = dir;
    QString base = o.value("baseName").toString().trimmed();
    base.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("-"));
    opt.baseName = base.isEmpty() ? QStringLiteral("board") : base;
    opt.transparentBackground = o.value("transparent").toBool();
    opt.zip = o.value("zip").toBool();
    opt.customDpi = o.value("customDpi", 300).toInt();
    opt.customQuality = o.value("customQuality", 90).toInt();
    opt.custom16Bit = o.value("custom16Bit").toBool();
    opt.password = o.value("password").toString();
    return opt;
}

struct Job
{
    Exporter::Result result;
    Exporter::OutFile package;
};

} // namespace

bool ExportController::singleFileExport() const
{
#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID)
    return true;
#else
    return false;
#endif
}

QString ExportController::packageName(const QVariantMap &o) const
{
    const Exporter::Options opt = optionsFrom(o, {});
    if (opt.formats.isEmpty())
        return opt.baseName;
    // One format, one page, no zip -> the file itself; anything else -> a zip.
    const bool single = opt.formats.size() == 1 && !opt.zip && opt.scope != Exporter::Scope::EachFrame;
    return single ? opt.baseName + QLatin1Char('.') + Exporter::extension(opt.formats.first())
                  : opt.baseName + QStringLiteral("_export.zip");
}

void ExportController::start(const QVariantMap &o)
{
    run(o, singleFileExport() ? Mode::Download : Mode::Folder, {});
}

void ExportController::startToUrl(const QVariantMap &o, const QUrl &target)
{
    run(o, Mode::Url, target);
}

void ExportController::run(const QVariantMap &o, Mode mode, const QUrl &target)
{
    if (m_busy || !m_canvas)
        return;

    Exporter::Options opt = optionsFrom(o, m_canvas->selectedIds());
    // Private boards: exporting needs the board password; PDF and BOARD outputs get encrypted.
    if (m_canvas->boardPrivate()) {
        if (!m_canvas->checkPassword(opt.password)) {
            emit finished(false, QStringLiteral("Wrong password — nothing was exported."), {}, {});
            return;
        }
    } else {
        opt.password.clear();
    }
    // One deliverable file: several pages of a raster format must be zipped.
    if (mode != Mode::Folder && opt.scope == Exporter::Scope::EachFrame)
        opt.zip = true;

    // Snapshot the document so editing can continue while exporting.
    const Document doc = m_canvas->document();
    const QUrl folderUrl = mode == Mode::Folder ? QUrl::fromLocalFile(opt.outputDir) : QUrl();

    m_busy = true;
    emit busyChanged();

    auto work = [doc, opt, mode]() {
        Job job;
        if (mode == Mode::Folder) {
            job.result = Exporter::exportDocument(doc, opt);
            return job;
        }
        if (opt.formats.isEmpty()) {
            job.result.error = QStringLiteral("Choose at least one format.");
            return job;
        }
        if (doc.items.isEmpty()) {
            job.result.error = QStringLiteral("The board is empty.");
            return job;
        }
        const QList<Exporter::OutFile> files = Exporter::render(doc, opt, job.result.warnings);
        if (files.isEmpty()) {
            job.result.error = QStringLiteral("Nothing to export.");
            return job;
        }
        job.package = Exporter::bundle(files, opt);
        return job;
    };

    auto deliver = [this, folderUrl, mode, target](Job job) {
        Exporter::Result &r = job.result;
        if (r.ok() && mode == Mode::Download) {
#ifdef Q_OS_WASM
            QWasmLocalFileAccess::saveFile(job.package.data, job.package.path.toStdString());
#endif
            r.files << job.package.path;
        } else if (r.ok() && mode == Mode::Url) {
            // Android: content:// URIs from the system picker open through QFile.
            QFile f(target.isLocalFile() ? target.toLocalFile() : target.toString());
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || f.write(job.package.data) != job.package.data.size())
                r.error = QStringLiteral("Could not write file: %1").arg(f.errorString());
            else
                r.files << job.package.path;
        }

        m_busy = false;
        emit busyChanged();
        QString msg;
        if (r.ok()) {
            msg = r.files.size() == 1
                ? QStringLiteral("Exported %1").arg(QFileInfo(r.files.first()).fileName())
                : QStringLiteral("Exported %1 files").arg(r.files.size());
        } else {
            msg = r.error;
        }
        if (!r.warnings.isEmpty())
            msg += QStringLiteral("\n") + r.warnings.join(QStringLiteral("\n"));
        emit finished(r.ok(), msg, r.files, folderUrl);
    };

#if QT_CONFIG(thread) && !defined(Q_OS_WASM)
    QThread *thread = QThread::create([this, work, deliver]() {
        Job job = work();
        QMetaObject::invokeMethod(this, [deliver, job]() { deliver(job); }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
#else
    // Single-threaded web build: yield once so the busy indicator paints first.
    QTimer::singleShot(50, this, [work, deliver]() { deliver(work()); });
#endif
}
