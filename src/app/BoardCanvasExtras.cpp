// BoardCanvas: private boards, right-click menu actions, item lock, arrange,
// copy/paste style and image tools (flip, crop, alt text, original download).

#include "BoardCanvas.h"

#include "Exporter.h"
#include "PenTools.h"
#include "ShapeLibrary.h"
#include "Renderer.h"
#include "Templates.h"

#include <QClipboard>
#include <cmath>
#include <QCryptographicHash>
#include <QDir>
#include <QPainter>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLocale>
#include <QMimeData>

#ifdef Q_OS_WASM
#include <private/qwasmlocalfileaccess_p.h>
#endif

namespace {

// Whole (uncropped) image rectangle in world coordinates, flip-aware.
QRectF fullImageRect(const BoardItem &it)
{
    const QRectF c = it.crop;
    const double fw = it.rect.width() / qMax(1e-6, c.width());
    const double fh = it.rect.height() / qMax(1e-6, c.height());
    const double cx = it.flipH ? 1.0 - c.x() - c.width() : c.x();
    const double cy = it.flipV ? 1.0 - c.y() - c.height() : c.y();
    return QRectF(it.rect.x() - cx * fw, it.rect.y() - cy * fh, fw, fh);
}

QString extensionForFormat(const QByteArray &f)
{
    if (f == "jpeg")
        return QStringLiteral("jpg");
    return f.isEmpty() ? QStringLiteral("png") : QString::fromLatin1(f);
}

} // namespace

bool BoardCanvas::anySelectedLocked() const
{
    for (const BoardItem *it : selectedItems())
        if (it->locked)
            return true;
    return false;
}

void BoardCanvas::setGridColor(const QColor &c)
{
    if (m_gridColor == c)
        return;
    m_gridColor = c;
    emit viewChanged();
    update();
}

QRectF BoardCanvas::selectionScreenRect() const
{
    if (m_selection.isEmpty())
        return {};
    return toScreen(m_doc.boundsOf(m_selection));
}

// ---------------------------------------------------------------------------
// Selection properties

QString BoardCanvas::selName() const
{
    const auto items = selectedItems();
    if (items.size() != 1)
        return items.isEmpty() ? QString() : QStringLiteral("%1 items").arg(items.size());
    const BoardItem *it = items.first();
    if (it->type == BoardItem::Type::Frame)
        return it->text.isEmpty() ? QStringLiteral("Frame") : it->text;
    if (!it->name.isEmpty())
        return it->name;
    if (it->type == BoardItem::Type::Image)
        return QStringLiteral("Image");
    QString t = BoardItem::typeName(it->type);
    t[0] = t[0].toUpper();
    return t;
}

bool BoardCanvas::selLocked() const
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return false;
    for (const BoardItem *it : items)
        if (!it->locked)
            return false;
    return true;
}

void BoardCanvas::setSelLocked(bool locked)
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    for (BoardItem *it : selectedItems())
        it->locked = locked;
    markChanged();
    emit selectionChanged();
    emit message(locked ? QStringLiteral("Locked — can't be moved, resized or deleted.")
                        : QStringLiteral("Unlocked"));
}

QString BoardCanvas::selAltText() const
{
    const auto items = selectedItems();
    return items.size() == 1 ? items.first()->altText : QString();
}

void BoardCanvas::setSelAltText(const QString &alt)
{
    const auto items = selectedItems();
    if (items.size() != 1 || items.first()->altText == alt)
        return;
    pushUndo();
    items.first()->altText = alt.trimmed();
    markChanged();
    emit selectionChanged();
}

bool BoardCanvas::selCropped() const
{
    const auto items = selectedItems();
    return items.size() == 1 && items.first()->type == BoardItem::Type::Image
           && items.first()->crop != QRectF(0, 0, 1, 1);
}

// ---------------------------------------------------------------------------
// Private boards

bool BoardCanvas::checkPassword(const QString &password) const
{
    return !password.isEmpty() && password == m_password;
}

bool BoardCanvas::makePrivate(const QString &password)
{
    if (password.size() < 4) {
        emit message(QStringLiteral("Use a password of at least 4 characters."));
        return false;
    }
    m_doc.isPrivate = true;
    m_password = password;
    m_modified = true;
    emit fileChanged();
    emit message(QStringLiteral("Board is private. Saving and PDF export will ask for this password."));
    return true;
}

bool BoardCanvas::makePublic(const QString &password)
{
    if (!checkPassword(password)) {
        emit message(QStringLiteral("Wrong password."));
        return false;
    }
    m_doc.isPrivate = false;
    m_password.clear();
    m_modified = true;
    emit fileChanged();
    emit message(QStringLiteral("Board is no longer private. Saved files won't be encrypted."));
    return true;
}

bool BoardCanvas::unlockPending(const QString &password)
{
    if (m_pendingData.isEmpty())
        return false;
    const QByteArray data = m_pendingData;
    const QString name = m_pendingName;
    if (!openData(data, name, password)) {
        m_pendingData = data;       // keep it for another attempt
        m_pendingName = name;
        return false;
    }
    if (!m_pendingPath.isEmpty()) {
        m_filePath = m_pendingPath;
        m_pendingPath.clear();
        emit fileChanged();
    }
    return true;
}

void BoardCanvas::cancelPending()
{
    m_pendingData.clear();
    m_pendingName.clear();
    m_pendingPath.clear();
}

// ---------------------------------------------------------------------------
// Context menu actions

void BoardCanvas::openContextMenuAt(const QPointF &pos)
{
    endTextEdit();
    const QString hit = m_doc.itemAt(toWorld(pos), 4.0 / m_zoom);
    if (!hit.isEmpty() && !m_selection.contains(hit))
        setSelection({ hit });
    else if (hit.isEmpty())
        setSelection({});
    emit contextMenuRequested(pos.x(), pos.y());
}

void BoardCanvas::copyAsPng()
{
    if (m_selection.isEmpty())
        return;
    Exporter::Options o;
    o.formats = { Exporter::Format::Png };
    o.compression = Exporter::Compression::Custom;
    o.customDpi = 192;             // 2x: crisp when pasted into documents and chats
    o.customQuality = 100;
    o.scope = Exporter::Scope::Selection;
    o.selection = m_selection;
    o.padding = 12;
    o.transparentBackground = true;
    QStringList warnings;
    const auto files = Exporter::render(m_doc, o, warnings);
    if (files.isEmpty())
        return;
    QGuiApplication::clipboard()->setImage(QImage::fromData(files.first().data));
    emit message(QStringLiteral("Copied as PNG image"));
}

void BoardCanvas::copyAsSvg()
{
    if (m_selection.isEmpty())
        return;
    Exporter::Options o;
    o.formats = { Exporter::Format::Svg };
    o.scope = Exporter::Scope::Selection;
    o.selection = m_selection;
    o.padding = 12;
    o.transparentBackground = true;
    QStringList warnings;
    const auto files = Exporter::render(m_doc, o, warnings);
    if (files.isEmpty())
        return;
    auto *mime = new QMimeData;
    mime->setData(QStringLiteral("image/svg+xml"), files.first().data);
    mime->setText(QString::fromUtf8(files.first().data));
    QGuiApplication::clipboard()->setMimeData(mime);
    emit message(QStringLiteral("Copied as SVG"));
}

void BoardCanvas::copyStyle()
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return;
    m_styleClipboard = *items.first();
    m_hasStyleClipboard = true;
    emit styleClipboardChanged();
    emit message(QStringLiteral("Style copied"));
}

void BoardCanvas::pasteStyle()
{
    if (!m_hasStyleClipboard || m_selection.isEmpty())
        return;
    const BoardItem &st = m_styleClipboard;
    pushUndo();
    for (BoardItem *it : selectedItems()) {
        if (it->type == BoardItem::Type::Image)
            continue;
        it->fill = st.fill;
        it->stroke = st.stroke;
        it->strokeWidth = st.strokeWidth;
        it->textColor = st.textColor;
        it->fontSize = st.fontSize;
        it->bold = st.bold;
        it->textAlign = st.textAlign;
        if (it->type == BoardItem::Type::Connector && st.type == BoardItem::Type::Connector) {
            it->connectorStyle = st.connectorStyle;
            it->arrowStart = st.arrowStart;
            it->arrowEnd = st.arrowEnd;
        }
        autoSizeText(*it);
    }
    markChanged();
    emit selectionChanged();
}

void BoardCanvas::bringForward()
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    auto &items = m_doc.items;
    for (qsizetype i = items.size() - 2; i >= 0; --i)
        if (m_selection.contains(items[i].id) && !m_selection.contains(items[i + 1].id))
            items.swapItemsAt(i, i + 1);
    markChanged();
}

void BoardCanvas::sendBackward()
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    auto &items = m_doc.items;
    for (qsizetype i = 1; i < items.size(); ++i)
        if (m_selection.contains(items[i].id) && !m_selection.contains(items[i - 1].id))
            items.swapItemsAt(i, i - 1);
    markChanged();
}

void BoardCanvas::renameSelection(const QString &name)
{
    const auto items = selectedItems();
    if (items.size() != 1)
        return;
    pushUndo();
    BoardItem *it = items.first();
    if (it->type == BoardItem::Type::Frame)
        it->text = name.trimmed();
    else
        it->name = name.trimmed();
    markChanged();
    emit selectionChanged();
}

QString BoardCanvas::selectionInfo() const
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return {};
    QStringList lines;
    const QLocale loc;
    if (items.size() > 1) {
        lines << QStringLiteral("%1 items selected").arg(items.size());
    } else {
        const BoardItem *it = items.first();
        lines << QStringLiteral("Name: %1").arg(selName());
        lines << QStringLiteral("Type: %1").arg(BoardItem::typeName(it->type));
        if (!it->text.isEmpty() && it->type != BoardItem::Type::Frame)
            lines << QStringLiteral("Text: %1 characters").arg(it->text.size());
        if (it->type == BoardItem::Type::Image) {
            const auto asset = m_doc.assets.constFind(it->assetKey);
            const QImage img = m_doc.assetImage(it->assetKey);
            if (asset != m_doc.assets.constEnd())
                lines << QStringLiteral("Original file: %1, %2 × %3 px, %4")
                             .arg(QString::fromLatin1(asset->format).toUpper())
                             .arg(img.width()).arg(img.height())
                             .arg(loc.formattedDataSize(asset->data.size()));
            if (it->crop != QRectF(0, 0, 1, 1))
                lines << QStringLiteral("Cropped to %1 × %2 px")
                             .arg(qRound(it->crop.width() * img.width()))
                             .arg(qRound(it->crop.height() * img.height()));
            if (it->flipH || it->flipV)
                lines << QStringLiteral("Flipped %1").arg(it->flipH && it->flipV ? QStringLiteral("both ways")
                                                         : it->flipH ? QStringLiteral("horizontally")
                                                                     : QStringLiteral("vertically"));
            lines << QStringLiteral("Alt text: %1").arg(it->altText.isEmpty() ? QStringLiteral("(none)") : it->altText);
        }
        if (it->locked)
            lines << QStringLiteral("Locked");
    }
    const QRectF b = m_doc.boundsOf(m_selection);
    lines << QStringLiteral("Position: %1, %2").arg(qRound(b.x())).arg(qRound(b.y()));
    lines << QStringLiteral("Size: %1 × %2").arg(qRound(b.width())).arg(qRound(b.height()));
    if (!m_doc.owner.isEmpty())
        lines << QStringLiteral("Board owner: %1").arg(m_doc.owner);
    const QVariantMap meta = selectionMeta();
    lines << QStringLiteral("Created by %1, %2").arg(meta.value("createdBy").toString(), meta.value("createdAt").toString());
    lines << QStringLiteral("Last modified by %1, %2").arg(meta.value("modifiedBy").toString(), meta.value("modifiedAt").toString());
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// Images

void BoardCanvas::flipSelection(bool horizontal)
{
    bool any = false;
    for (const BoardItem *it : selectedItems())
        any = any || (it->type == BoardItem::Type::Image && !it->locked);
    if (!any)
        return;
    pushUndo();
    for (BoardItem *it : selectedItems()) {
        if (it->type != BoardItem::Type::Image || it->locked)
            continue;
        if (horizontal)
            it->flipH = !it->flipH;
        else
            it->flipV = !it->flipV;
    }
    markChanged();
    emit selectionChanged();
}

void BoardCanvas::startCrop()
{
    const auto items = selectedItems();
    if (items.size() != 1 || items.first()->type != BoardItem::Type::Image)
        return;
    if (items.first()->locked) {
        emit message(QStringLiteral("Unlock the image to crop it."));
        return;
    }
    endTextEdit();
    m_cropId = items.first()->id;
    m_cropFull = fullImageRect(*items.first());
    m_cropSnapshot = m_doc.items;
    emit croppingChanged();
    emit message(QStringLiteral("Drag the handles to crop. Press Enter or click outside to finish."));
    update();
}

void BoardCanvas::cropDrag(const QPointF &world)
{
    BoardItem *it = m_doc.find(m_cropId);
    if (!it)
        return;
    const QRectF full = m_cropFull;
    const QPointF w(qBound(full.left(), world.x(), full.right()), qBound(full.top(), world.y(), full.bottom()));
    QRectF r = it->rect;
    constexpr double kMin = 8.0;
    const int h = m_handle;
    if (h == 0 || h == 6 || h == 7) r.setLeft(qMin(w.x(), r.right() - kMin));
    if (h == 2 || h == 3 || h == 4) r.setRight(qMax(w.x(), r.left() + kMin));
    if (h == 0 || h == 1 || h == 2) r.setTop(qMin(w.y(), r.bottom() - kMin));
    if (h == 4 || h == 5 || h == 6) r.setBottom(qMax(w.y(), r.top() + kMin));
    it->rect = r;

    // Visible rect -> normalized source rect (mirrored when flipped).
    double x = (r.left() - full.left()) / full.width();
    double y = (r.top() - full.top()) / full.height();
    const double cw = r.width() / full.width();
    const double ch = r.height() / full.height();
    if (it->flipH)
        x = 1.0 - x - cw;
    if (it->flipV)
        y = 1.0 - y - ch;
    it->crop = QRectF(x, y, cw, ch).intersected(QRectF(0, 0, 1, 1));
    update();
}

void BoardCanvas::finishCrop()
{
    if (m_cropId.isEmpty())
        return;
    const BoardItem *now = m_doc.find(m_cropId);
    bool changed = false;
    for (const BoardItem &before : std::as_const(m_cropSnapshot))
        if (now && before.id == now->id)
            changed = before.rect != now->rect || before.crop != now->crop;
    if (changed) {
        m_undo.push(m_cropSnapshot);
        emit historyChanged();
        markChanged();
    }
    m_cropId.clear();
    m_cropSnapshot.clear();
    emit croppingChanged();
    emit selectionChanged();
    update();
}

void BoardCanvas::resetCrop()
{
    if (cropping())
        finishCrop();
    bool any = false;
    for (const BoardItem *it : selectedItems())
        any = any || (it->type == BoardItem::Type::Image && it->crop != QRectF(0, 0, 1, 1) && !it->locked);
    if (!any)
        return;
    pushUndo();
    for (BoardItem *it : selectedItems()) {
        if (it->type != BoardItem::Type::Image || it->locked)
            continue;
        it->rect = fullImageRect(*it);
        it->crop = QRectF(0, 0, 1, 1);
    }
    markChanged();
    emit selectionChanged();
}

void BoardCanvas::resetImageSize()
{
    pushUndo();
    for (BoardItem *it : selectedItems()) {
        if (it->type != BoardItem::Type::Image || it->locked)
            continue;
        const QImage img = m_doc.assetImage(it->assetKey);
        if (img.isNull())
            continue;
        const QPointF c = it->rect.center();
        const QSizeF s(img.width() * it->crop.width(), img.height() * it->crop.height());
        it->rect = QRectF(c - QPointF(s.width() / 2, s.height() / 2), s);
    }
    markChanged();
    emit selectionChanged();
}

QString BoardCanvas::originalImageName() const
{
    const auto items = selectedItems();
    if (items.size() != 1 || items.first()->type != BoardItem::Type::Image)
        return {};
    const BoardItem *it = items.first();
    const QString ext = extensionForFormat(m_doc.assets.value(it->assetKey).format);
    if (!it->name.isEmpty())
        return QFileInfo(it->name).suffix().isEmpty() ? it->name + QLatin1Char('.') + ext : it->name;
    return QStringLiteral("image.") + ext;
}

bool BoardCanvas::saveOriginalImage(const QUrl &url)
{
    const auto items = selectedItems();
    if (items.size() != 1 || items.first()->type != BoardItem::Type::Image)
        return false;
    const QByteArray bytes = m_doc.assets.value(items.first()->assetKey).data;
    QFile f(url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(bytes) != bytes.size()) {
        emit message(QStringLiteral("Could not save the image: %1").arg(f.errorString()));
        return false;
    }
    emit message(QStringLiteral("Saved the original image file (unchanged, %1)")
                     .arg(QLocale().formattedDataSize(bytes.size())));
    return true;
}

void BoardCanvas::browserDownloadOriginal()
{
#ifdef Q_OS_WASM
    const auto items = selectedItems();
    if (items.size() != 1 || items.first()->type != BoardItem::Type::Image)
        return;
    QWasmLocalFileAccess::saveFile(m_doc.assets.value(items.first()->assetKey).data,
                                   originalImageName().toStdString());
#endif
}

// ---------------------------------------------------------------------------
// Live collaboration

void BoardCanvas::remoteApplied()
{
    refreshSignatures();   // collaborators' stamps arrive with their edits
    // Items a collaborator deleted disappear from our selection / editor.
    QStringList keep;
    for (const QString &id : std::as_const(m_selection))
        if (m_doc.find(id))
            keep << id;
    if (keep != m_selection) {
        m_selection = keep;
        emit selectionChanged();
    }
    if (!m_editingId.isEmpty() && !m_doc.find(m_editingId)) {
        m_editingId.clear();
        emit editingChanged();
    }
    if (!m_cropId.isEmpty() && !m_doc.find(m_cropId)) {
        m_cropId.clear();
        emit croppingChanged();
    }
    const bool wasModified = m_modified;
    m_modified = true;
    if (!wasModified)
        emit fileChanged();
    emit fileChanged();          // title may have changed
    emit layersChanged();
    emit contentChanged();
    emit selectionChanged();     // selection styles may have changed
    update();
}

void BoardCanvas::setRemoteCursors(const QVariantList &cursors)
{
    m_remoteCursors = cursors;
    emit remoteCursorsChanged();
    update();
}

// ---------------------------------------------------------------------------
// Templates and thumbnails

QVariantList BoardCanvas::templates() const
{
    QVariantList out;
    for (const Templates::Info &t : Templates::list())
        out << QVariantMap{ { "id", t.id }, { "name", t.name }, { "description", t.description }, { "category", t.category } };
    // "Save as template": the user's own templates
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/templates");
    for (const QFileInfo &fi : QDir(dir).entryInfoList({ QStringLiteral("*.board") }, QDir::Files, QDir::Time)) {
        QString name = fi.completeBaseName();
        name = name.left(name.lastIndexOf(QLatin1Char('-')) > 0 ? name.lastIndexOf(QLatin1Char('-')) : name.size());
        out << QVariantMap{ { "id", QStringLiteral("custom:") + fi.fileName() }, { "name", name },
                            { "description", QStringLiteral("Saved by you") }, { "category", QStringLiteral("My templates") },
                            { "custom", true } };
    }
    return out;
}

void BoardCanvas::newFromTemplate(const QString &id)
{
    newBoard();
    if (id.isEmpty() || id == QLatin1String("blank"))
        return;
    Document d = id.startsWith(QLatin1String("custom:")) ? loadCustomTemplate(id) : Templates::create(id);
    if (d.items.isEmpty())
        return;
    d.owner = m_ownerEmail;
    d.isPrivate = false;
    d.startView = QRectF();
    d.versions.clear();
    m_doc = std::move(d);
    m_sigs.clear();
    stampChanges();
    for (const BoardItem &it : std::as_const(m_doc.items))
        if (it.type == BoardItem::Type::Frame)
            ++m_frameCounter;
    m_modified = true;
    emit fileChanged();
    emit contentChanged();
    emit localEdit();
    zoomToFit();
}

QImage renderPreview(const Document &doc, const QSize &size, const QColor &background)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(background);
    QRectF b = doc.contentBounds();
    if (b.isNull())
        return img;
    b.adjust(-40, -40, 40, 40);
    const double s = qMin(size.width() / b.width(), size.height() / b.height());
    QPainter p(&img);
    p.translate((size.width() - b.width() * s) / 2, (size.height() - b.height() * s) / 2);
    p.scale(s, s);
    p.translate(-b.topLeft());
    Renderer::paintDocument(p, doc, b);
    return img;
}

QString BoardCanvas::saveThumbnail() const
{
    if (m_filePath.isEmpty() || m_doc.isPrivate)
        return {};   // private boards keep no unencrypted preview on disk
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/thumbs");
    QDir().mkpath(dir);
    const QString file = dir + QLatin1Char('/')
        + QString::fromLatin1(QCryptographicHash::hash(m_filePath.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".png");
    if (!renderPreview(m_doc, QSize(480, 300), QColor(245, 246, 248)).save(file, "PNG"))
        return {};
    return QUrl::fromLocalFile(file).toString();
}

// ---------------------------------------------------------------------------
// Pen tools: erasers and smart drawing

void BoardCanvas::eraseAt(const QPointF &world)
{
    const bool precise = m_tool == QLatin1String("precision-eraser");
    const double radius = (precise ? 12.0 : 8.0) / m_zoom;
    QList<BoardItem> added;
    bool changed = false;
    for (qsizetype i = m_doc.items.size() - 1; i >= 0; --i) {
        const BoardItem it = m_doc.items[i];
        if (it.type != BoardItem::Type::Pen || it.locked || !m_doc.itemPickable(it))
            continue;
        if (!it.rect.adjusted(-radius - it.strokeWidth, -radius - it.strokeWidth,
                              radius + it.strokeWidth, radius + it.strokeWidth).contains(world))
            continue;
        QList<QPointF> abs;
        for (const QPointF &pt : it.points)
            abs << it.rect.topLeft() + pt;
        const double r = radius + it.strokeWidth / 2;
        if (!PenTools::strokeHits(abs, world, r))
            continue;
        if (precise) {
            // Keep the pieces outside the eraser as separate strokes.
            for (const QList<QPointF> &piece : PenTools::eraseAround(abs, world, r)) {
                BoardItem part = it;
                part.id = BoardItem::newId();
                const QRectF b = QPolygonF(piece).boundingRect();
                part.rect = QRectF(b.topLeft(), b.size().expandedTo(QSizeF(1, 1)));
                part.points.clear();
                for (const QPointF &pt : piece)
                    part.points << pt - b.topLeft();
                added << part;
            }
        }
        m_selection.removeAll(it.id);
        m_doc.items.removeAt(i);
        changed = true;
    }
    if (!changed)
        return;
    m_doc.items.append(added);
    m_eraseChanged = true;
    emit selectionChanged();
    emit localEdit();
    update();
}

void BoardCanvas::smartReplace(const QString &penId)
{
    const int index = m_doc.indexOf(penId);
    if (index < 0)
        return;
    const BoardItem pen = m_doc.items[index];
    QList<QPointF> abs;
    for (const QPointF &pt : pen.points)
        abs << pen.rect.topLeft() + pt;
    const PenTools::Recognized r = PenTools::recognize(abs);
    if (r.kind == PenTools::Recognized::None)
        return;   // not a clear shape: keep the drawing as it is

    BoardItem out;
    out.id = pen.id;
    out.layer = pen.layer;
    out.stroke = pen.stroke;
    out.strokeWidth = pen.strokeWidth;
    out.fill = Qt::transparent;
    if (r.kind == PenTools::Recognized::Line) {
        out.type = BoardItem::Type::Connector;
        out.fromPoint = r.from;
        out.toPoint = r.to;
        out.arrowEnd = false;
    } else {
        out.type = BoardItem::Type::Shape;
        out.rect = r.rect;
        out.shapeId = r.kind == PenTools::Recognized::Rectangle ? QStringLiteral("rect")
                    : r.kind == PenTools::Recognized::Ellipse ? QStringLiteral("circle")
                    : r.kind == PenTools::Recognized::Triangle ? QStringLiteral("triangle")
                                                                : QStringLiteral("rhombus");
    }
    m_doc.items[index] = out;
}

// ---------------------------------------------------------------------------
// Libraries

QVariantList BoardCanvas::shapeLibrary() const
{
    QVariantList out;
    for (const ShapeLibrary::Entry &e : ShapeLibrary::shapes())
        out << QVariantMap{ { "id", e.id }, { "name", e.name }, { "category", e.category }, { "keywords", e.keywords } };
    return out;
}

QVariantList BoardCanvas::iconLibrary() const
{
    QVariantList out;
    for (const ShapeLibrary::Entry &e : ShapeLibrary::icons())
        out << QVariantMap{ { "id", e.id }, { "name", e.name }, { "category", e.category }, { "keywords", e.keywords } };
    return out;
}

QString BoardCanvas::emojiFamily() const
{
    return Renderer::emojiFamily();
}

QVariantList BoardCanvas::emojiLibrary() const
{
    QVariantList out;
    for (const ShapeLibrary::EmojiGroup &g : ShapeLibrary::emoji())
        out << QVariantMap{ { "category", g.category }, { "emoji", g.emoji } };
    return out;
}

void BoardCanvas::addStickies(const QStringList &lines)
{
    QStringList texts;
    for (const QString &l : lines)
        if (!l.trimmed().isEmpty())
            texts << l.trimmed();
    if (texts.isEmpty())
        return;
    endTextEdit();
    pushUndo();
    const int columns = qMax(1, int(std::ceil(std::sqrt(double(texts.size())))));
    const int rows = int(std::ceil(texts.size() / double(columns)));
    const QPointF origin = toWorld(boundingRect().center()) - QPointF(columns * 110.0, rows * 110.0);
    QStringList added;
    for (int i = 0; i < texts.size(); ++i) {
        BoardItem it = makeItem(BoardItem::Type::Sticky, QPointF());
        it.rect = QRectF(origin + QPointF((i % columns) * 220.0, (i / columns) * 220.0), QSizeF(200, 200));
        it.text = texts[i];
        added << addItem(it);
    }
    setSelection(added);
    markChanged();
    if (!boundingRect().contains(toScreen(m_doc.boundsOf(added))))
        zoomToFit();
}

// ---------------------------------------------------------------------------
// Layers

QVariantList BoardCanvas::layers() const
{
    QHash<QString, int> counts;
    for (const BoardItem &it : m_doc.items)
        counts[it.layer] += 1;
    QVariantList out;
    out << QVariantMap{ { "id", QString() }, { "name", QStringLiteral("Default layer") }, { "visible", true },
                        { "locked", false }, { "count", counts.value(QString()) } };
    for (const Layer &l : m_doc.layers)
        out << QVariantMap{ { "id", l.id }, { "name", l.name }, { "visible", l.visible }, { "locked", l.locked },
                            { "count", counts.value(l.id) } };
    return out;
}

void BoardCanvas::setActiveLayer(const QString &id)
{
    const QString target = m_doc.layerIndex(id) > 0 ? id : QString();
    if (m_activeLayer == target)
        return;
    m_activeLayer = target;
    emit layersChanged();
}

QString BoardCanvas::addLayer(const QString &name)
{
    Layer l;
    l.id = BoardItem::newId().left(8);
    l.name = name.trimmed().isEmpty() ? QStringLiteral("Layer %1").arg(m_doc.layers.size() + 1) : name.trimmed();
    m_doc.layers.append(l);
    m_activeLayer = l.id;
    m_modified = true;
    emit fileChanged();
    emit layersChanged();
    emit localEdit();
    return l.id;
}

void BoardCanvas::renameLayer(const QString &id, const QString &name)
{
    for (Layer &l : m_doc.layers) {
        if (l.id != id || name.trimmed().isEmpty())
            continue;
        l.name = name.trimmed();
        m_modified = true;
        emit fileChanged();
        emit layersChanged();
        emit localEdit();
    }
}

void BoardCanvas::deleteLayer(const QString &id)
{
    const int i = m_doc.layerIndex(id) - 1;
    if (i < 0)
        return;
    pushUndo();
    for (BoardItem &it : m_doc.items)
        if (it.layer == id)
            it.layer.clear();   // contents move to the default layer, nothing is lost
    m_doc.layers.removeAt(i);
    if (m_activeLayer == id)
        m_activeLayer.clear();
    m_doc.normalizeLayerOrder();
    emit layersChanged();
    markChanged();
}

void BoardCanvas::setLayerVisible(const QString &id, bool visible)
{
    for (Layer &l : m_doc.layers)
        if (l.id == id)
            l.visible = visible;
    if (!visible) {
        QStringList keep;
        for (const BoardItem *it : selectedItems())
            if (it->layer != id)
                keep << it->id;
        setSelection(keep);
    }
    m_modified = true;
    emit fileChanged();
    emit layersChanged();
    emit localEdit();
    update();
}

void BoardCanvas::setLayerLocked(const QString &id, bool locked)
{
    for (Layer &l : m_doc.layers)
        if (l.id == id)
            l.locked = locked;
    m_modified = true;
    emit fileChanged();
    emit layersChanged();
    emit localEdit();
    update();
}

void BoardCanvas::moveLayer(const QString &id, int delta)
{
    const int i = m_doc.layerIndex(id) - 1;
    const int j = i + delta;
    if (i < 0 || j < 0 || j >= m_doc.layers.size())
        return;
    pushUndo();
    m_doc.layers.swapItemsAt(i, j);
    m_doc.normalizeLayerOrder();
    emit layersChanged();
    markChanged();
}

void BoardCanvas::moveSelectionToLayer(const QString &id)
{
    if (m_selection.isEmpty())
        return;
    const QString target = m_doc.layerIndex(id) > 0 ? id : QString();
    pushUndo();
    for (BoardItem *it : selectedItems())
        it->layer = target;
    m_doc.normalizeLayerOrder();
    emit layersChanged();
    markChanged();
}
