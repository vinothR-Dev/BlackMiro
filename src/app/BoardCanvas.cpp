#include "BoardCanvas.h"

#include "BoardFile.h"
#include "Renderer.h"
#include "WebStorage.h"

#include <QBuffer>
#include <QClipboard>
#include <QJsonArray>
#include <QLocale>
#include <QCursor>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QtMath>

#ifdef Q_OS_WASM
#include <private/qwasmlocalfileaccess_p.h>
#include <memory>
#endif

namespace {

constexpr double kMinZoom = 0.02;
constexpr double kMaxZoom = 40.0;
constexpr double kHandleRadius = 6.0;
constexpr int kMaxVersions = 50;
const char *kClipboardMime = "application/x-miru-items";

bool isResizable(const BoardItem &it)
{
    return it.type != BoardItem::Type::Connector && it.type != BoardItem::Type::Comment;
}

} // namespace

BoardCanvas::BoardCanvas(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setAcceptTouchEvents(true);
    setFlag(ItemAcceptsInputMethod, false);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);
    setAntialiasing(true);
    setOpaquePainting(true);
    setFillColor(QColor(245, 246, 248));

    m_autosave.setSingleShot(true);
    m_autosave.setInterval(1500);
    connect(&m_autosave, &QTimer::timeout, this, &BoardCanvas::autosaveNow);
    connect(this, &BoardCanvas::contentChanged, this, [this]() { ++m_changeSerial; scheduleAutosave(); });
    connect(this, &BoardCanvas::fileChanged, this, &BoardCanvas::scheduleAutosave);

    // Toolbars floating above the selection follow it around.
    connect(this, &BoardCanvas::viewChanged, this, &BoardCanvas::selectionGeometryChanged);
    connect(this, &BoardCanvas::selectionChanged, this, &BoardCanvas::selectionGeometryChanged);
    connect(this, &BoardCanvas::contentChanged, this, &BoardCanvas::selectionGeometryChanged);

    // Touch: press and hold opens the context menu.
    m_viewAnim.setInterval(8);     // ticks as fast as frames arrive; motion is time-based
    m_viewAnim.setTimerType(Qt::PreciseTimer);
    connect(&m_viewAnim, &QTimer::timeout, this, &BoardCanvas::viewAnimTick);
    m_longPress.setSingleShot(true);
    m_longPress.setInterval(550);
    connect(&m_longPress, &QTimer::timeout, this, [this]() {
        if (m_drag != Drag::None && m_dragChanged)
            m_doc.items = m_dragSnapshot;      // undo the bit of movement before the hold
        m_drag = Drag::None;
        m_dragOrigin.clear();
        m_activeId.clear();
        openContextMenuAt(m_longPressPos);
    });
    loadSettings();
}

// ---------------------------------------------------------------------------
// Painting

QRectF BoardCanvas::toScreen(const QRectF &world) const
{
    return QRectF(toScreen(world.topLeft()), world.size() * m_zoom);
}

void BoardCanvas::paint(QPainter *p)
{
    const QRectF view = boundingRect();
    p->fillRect(view, fillColor());

    if (m_showGrid) {
        double step = 24.0;
        while (step * m_zoom < 14.0)
            step *= 4.0;
        const QPointF w0 = toWorld(view.topLeft());
        const QPointF w1 = toWorld(view.bottomRight());
        if (m_gridStyle == QLatin1String("lines")) {
            QColor c = m_gridColor;
            c.setAlpha(110);
            p->setPen(QPen(c, 1.0));
            for (double x = std::floor(w0.x() / step) * step; x <= w1.x(); x += step) {
                const double sx = std::round(toScreen(QPointF(x, 0)).x()) + 0.5;
                p->drawLine(QPointF(sx, view.top()), QPointF(sx, view.bottom()));
            }
            for (double y = std::floor(w0.y() / step) * step; y <= w1.y(); y += step) {
                const double sy = std::round(toScreen(QPointF(0, y)).y()) + 0.5;
                p->drawLine(QPointF(view.left(), sy), QPointF(view.right(), sy));
            }
        } else {
        const double dot = qBound(1.0, 1.6 * qMin(1.0, m_zoom * 1.5), 2.0);
        p->setPen(Qt::NoPen);
        p->setBrush(m_gridColor);
        for (double x = std::floor(w0.x() / step) * step; x <= w1.x(); x += step)
            for (double y = std::floor(w0.y() / step) * step; y <= w1.y(); y += step) {
                const QPointF s = toScreen(QPointF(x, y));
                p->drawEllipse(s, dot / 2, dot / 2);
            }
        }
    }

    p->save();
    p->translate(m_pan);
    p->scale(m_zoom, m_zoom);
    Renderer::Options opt;
    if (!m_editingId.isEmpty())
        opt.hideTextIds << m_editingId;
    ensurePreviews();
    opt.previews = &m_previews;
    opt.deviceScale = m_zoom * (window() ? window()->effectiveDevicePixelRatio() : 1.0);
    const bool darkBackground = fillColor().lightness() < 128;
    if (darkBackground)
        opt.frameTitleColor = QColor(170, 176, 188);
    Renderer::paintDocument(*p, m_doc, QRectF(toWorld(view.topLeft()), toWorld(view.bottomRight())), opt);
    p->restore();

    // --- Overlays (screen space) ---
    p->setRenderHint(QPainter::Antialiasing, true);
    const QColor accent(66, 98, 255);

    if (!m_hoverTarget.isEmpty()) {
        if (const BoardItem *t = m_doc.find(m_hoverTarget)) {
            p->setPen(QPen(accent, 2.0));
            p->setBrush(QColor(66, 98, 255, 25));
            p->drawRect(toScreen(t->rect).adjusted(-4, -4, 4, 4));
        }
    }

    for (const BoardItem *it : selectedItems()) {
        if (it->type == BoardItem::Type::Comment)
            continue;   // the pin draws its own highlight
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(accent, 1.5));
        if (it->type == BoardItem::Type::Connector) {
            const QPolygonF path = m_doc.connectorPath(*it);
            QPolygonF sp;
            for (const QPointF &pt : path)
                sp << toScreen(pt);
            p->setPen(QPen(QColor(66, 98, 255, 90), 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPolyline(sp);
            if (m_selection.size() == 1 && !sp.isEmpty()) {
                p->setPen(QPen(accent, 1.5));
                p->setBrush(Qt::white);
                p->drawEllipse(sp.first(), kHandleRadius, kHandleRadius);
                p->drawEllipse(sp.last(), kHandleRadius, kHandleRadius);
            }
        } else {
            p->drawRect(toScreen(it->rect));
        }
        if (it->locked) {
            // Small padlock badge on the top-right corner.
            const QRectF r = toScreen(it->rect);
            const QRectF badge(r.right() - 10, r.top() - 10, 20, 20);
            p->setPen(Qt::NoPen);
            p->setBrush(accent);
            p->drawEllipse(badge);
            p->setPen(QPen(Qt::white, 1.4));
            p->setBrush(Qt::NoBrush);
            p->drawArc(QRectF(badge.center().x() - 3, badge.top() + 4, 6, 7), 0, 180 * 16);
            p->setBrush(Qt::white);
            p->drawRect(QRectF(badge.center().x() - 4.5, badge.top() + 8.5, 9, 6.5));
        }
    }

    if (const BoardItem *crop = m_doc.find(m_cropId)) {
        // Crop mode: the hidden part of the image is shown faded around the visible part.
        const QImage img = m_doc.assetImage(crop->assetKey);
        const QRectF full = toScreen(m_cropFull);
        const QRectF vis = toScreen(crop->rect);
        QPainterPath outside;
        outside.addRect(full);
        QPainterPath inside;
        inside.addRect(vis);
        p->save();
        p->setClipPath(outside.subtracted(inside));
        p->setOpacity(0.35);
        if (crop->flipH || crop->flipV) {
            p->translate(full.center());
            p->scale(crop->flipH ? -1 : 1, crop->flipV ? -1 : 1);
            p->translate(-full.center());
        }
        p->drawImage(full, img);
        p->restore();
        p->setPen(QPen(accent, 1.0, Qt::DashLine));
        p->setBrush(Qt::NoBrush);
        p->drawRect(full);
    }

    const QRectF sel = anySelectedLocked() ? QRectF() : selectionRect();
    if (!sel.isNull()) {
        const QRectF s = toScreen(sel);
        if (m_selection.size() > 1) {
            p->setPen(QPen(accent, 1.0, Qt::DashLine));
            p->setBrush(Qt::NoBrush);
            p->drawRect(s);
        }
        p->setPen(QPen(accent, 1.5));
        p->setBrush(Qt::white);
        const QPointF pts[8] = { s.topLeft(), QPointF(s.center().x(), s.top()), s.topRight(),
                                 QPointF(s.right(), s.center().y()), s.bottomRight(),
                                 QPointF(s.center().x(), s.bottom()), s.bottomLeft(),
                                 QPointF(s.left(), s.center().y()) };
        for (const QPointF &pt : pts)
            p->drawEllipse(pt, kHandleRadius - 1, kHandleRadius - 1);
    }
    if (m_drag == Drag::None) {
        // Quick-connect dots: blue with a white ring, just outside each side.
        p->setPen(QPen(Qt::white, 2.0));
        p->setBrush(accent);
        for (const QPointF &pt : connectDots())
            p->drawEllipse(pt, 5.5, 5.5);
    }

    // Collaborators: their selections and cursors, in their colour.
    for (const QVariant &v : std::as_const(m_remoteCursors)) {
        const QVariantMap c = v.toMap();
        const QColor col(c.value("color").toString());
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(col, 2.0));
        for (const QVariant &sid : c.value("sel").toList())
            if (const BoardItem *it = m_doc.find(sid.toString()))
                p->drawRect(toScreen(m_doc.itemBounds(*it)).adjusted(-3, -3, 3, 3));
        const QPointF s = toScreen(QPointF(c.value("x").toDouble(), c.value("y").toDouble()));
        if (!c.value("cursor").toBool() || !boundingRect().adjusted(-20, -20, 20, 20).contains(s))
            continue;
        QPolygonF arrow;
        arrow << s << s + QPointF(0, 17) << s + QPointF(4.5, 13) << s + QPointF(8, 20)
              << s + QPointF(10.5, 19) << s + QPointF(7, 12) << s + QPointF(13, 12);
        p->setPen(QPen(Qt::white, 1.2));
        p->setBrush(col);
        p->drawPolygon(arrow);
        const QString chat = c.value("chat").toString();
        const QString name = chat.isEmpty() ? c.value("name").toString()
                                            : c.value("name").toString() + QStringLiteral(": ") + chat;
        QFont f = p->font();
        f.setPixelSize(12);
        f.setBold(true);
        p->setFont(f);
        const QFontMetricsF fm(f);
        const QRectF tag(s.x() + 12, s.y() + 18, fm.horizontalAdvance(name) + 14, fm.height() + 6);
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(tag, 6, 6);
        p->setPen(Qt::white);
        p->drawText(tag, Qt::AlignCenter, name);
    }

    paintMiroOverlays(p);

    if (m_drag == Drag::Lasso && m_lasso.size() > 1) {
        p->setPen(QPen(accent, 1.4, Qt::DashLine));
        p->setBrush(QColor(66, 98, 255, 22));
        p->drawPolygon(m_lasso);
    }
    if (m_tool == QLatin1String("eraser") || m_tool == QLatin1String("precision-eraser")) {
        const double r = m_tool == QLatin1String("eraser") ? 8 : 12;
        p->setPen(QPen(QColor(120, 125, 135), 1.2));
        p->setBrush(QColor(255, 255, 255, 120));
        p->drawEllipse(m_drag == Drag::Erase ? m_lastScreen : m_hoverScreen, r, r);
    }

    if (m_drag == Drag::Marquee && !m_marquee.isNull()) {
        p->setPen(QPen(accent, 1.0));
        p->setBrush(QColor(66, 98, 255, 30));
        p->drawRect(m_marquee.normalized());
    }
}

// ---------------------------------------------------------------------------
// View

void BoardCanvas::setZoomAt(double zoom, const QPointF &anchor)
{
    zoom = qBound(kMinZoom, zoom, kMaxZoom);
    const QPointF world = toWorld(anchor);
    m_zoom = zoom;
    m_pan = anchor - world * m_zoom;
    emit viewChanged();
    if (editing())
        emit editingChanged();
    update();
}

void BoardCanvas::zoomIn() { setZoomAt(m_zoom * 1.25, boundingRect().center()); }
void BoardCanvas::zoomOut() { setZoomAt(m_zoom / 1.25, boundingRect().center()); }
void BoardCanvas::resetZoom() { setZoomAt(1.0, boundingRect().center()); }

void BoardCanvas::zoomToFit()
{
    QRectF content = m_selection.isEmpty() ? m_doc.contentBounds() : m_doc.boundsOf(m_selection);
    if (content.isNull() || width() <= 0 || height() <= 0) {
        m_zoom = 1.0;
        m_pan = QPointF(width() / 2, height() / 2);
    } else {
        content.adjust(-24, -24, 24, 24);
        // Fit into the area not covered by the floating chrome: tool bar (left),
        // title / export bars (top), zoom bar (bottom).
        QRectF view = boundingRect().adjusted(76, 72, -16, -64);
        if (view.width() < 100 || view.height() < 100)
            view = boundingRect();
        m_zoom = qBound(kMinZoom, qMin(view.width() / content.width(), view.height() / content.height()), 2.0);
        m_pan = view.center() - content.center() * m_zoom;
    }
    emit viewChanged();
    if (editing())
        emit editingChanged();
    update();
}

void BoardCanvas::setShowGrid(bool on)
{
    if (m_showGrid == on)
        return;
    m_showGrid = on;
    emit viewChanged();
    update();
}

void BoardCanvas::geometryChange(const QRectF &n, const QRectF &o)
{
    QQuickPaintedItem::geometryChange(n, o);
    if (o.isEmpty() && !n.isEmpty())
        m_pan = QPointF(n.width() / 2, n.height() / 2);
}

void BoardCanvas::setTool(const QString &tool)
{
    if (m_tool == tool)
        return;
    endTextEdit();
    m_tool = tool;
    m_hoverTarget.clear();
    setCursor(tool == QLatin1String("hand") ? Qt::OpenHandCursor
              : tool == QLatin1String("select") ? Qt::ArrowCursor
                                                : Qt::CrossCursor);
    emit toolChanged();
    update();
}

// ---------------------------------------------------------------------------
// Model helpers

void BoardCanvas::pushUndo()
{
    m_undo.push(m_doc.items);
    emit historyChanged();
}

void BoardCanvas::markChanged()
{
    stampChanges();
    const bool wasModified = m_modified;
    m_modified = true;
    if (!wasModified)
        emit fileChanged();
    emit contentChanged();
    emit localEdit();
    update();
}

void BoardCanvas::setSelection(const QStringList &ids)
{
    if (m_selection == ids)
        return;
    m_selection = ids;
    emit selectionChanged();
    update();
}

void BoardCanvas::clearSelection()
{
    endTextEdit();
    setSelection({});
}

QList<BoardItem *> BoardCanvas::selectedItems()
{
    QList<BoardItem *> out;
    for (const QString &id : std::as_const(m_selection))
        if (BoardItem *it = m_doc.find(id))
            out << it;
    return out;
}

QList<const BoardItem *> BoardCanvas::selectedItems() const
{
    QList<const BoardItem *> out;
    for (const QString &id : m_selection)
        if (const BoardItem *it = m_doc.find(id))
            out << it;
    return out;
}


QRectF BoardCanvas::selectionRect() const
{
    QRectF r;
    for (const BoardItem *it : selectedItems()) {
        if (!isResizable(*it))
            continue;
        r = r.isNull() ? it->rect : r.united(it->rect);
    }
    return r;
}

BoardItem BoardCanvas::makeItem(BoardItem::Type type, const QPointF &pos) const
{
    BoardItem it;
    it.id = BoardItem::newId();
    it.type = type;
    it.layer = m_activeLayer;
    switch (type) {
    case BoardItem::Type::Shape:
        it.rect = QRectF(pos - QPointF(80, 55), QSizeF(160, 110));
        it.fill = m_shapeFill;
        break;
    case BoardItem::Type::Icon:
        it.rect = QRectF(pos - QPointF(48, 48), QSizeF(96, 96));
        it.fill = Qt::transparent;
        it.strokeWidth = 4;
        break;
    case BoardItem::Type::Sticky:
        it.rect = QRectF(pos - QPointF(100, 100), QSizeF(200, 200));
        it.fill = m_stickyColor;
        it.strokeWidth = 0;
        it.fontSize = 20;
        break;
    case BoardItem::Type::Text:
        it.rect = QRectF(pos - QPointF(4, 20), QSizeF(260, 40));
        it.fill = Qt::transparent;
        it.strokeWidth = 0;
        it.fontSize = 24;
        break;
    case BoardItem::Type::Frame:
        it.rect = QRectF(pos, m_framePreset);
        it.fill = Qt::white;
        it.strokeWidth = 0;
        it.fontSize = 14;
        break;
    case BoardItem::Type::Pen:
        it.stroke = m_penColor;
        it.strokeWidth = m_penWidth;
        it.fill = Qt::transparent;
        break;
    case BoardItem::Type::Connector:
        it.strokeWidth = 2;
        it.fill = Qt::transparent;
        break;
    case BoardItem::Type::Ellipse:
        it.rect = QRectF(pos - QPointF(70, 70), QSizeF(140, 140));
        break;
    default:
        it.rect = QRectF(pos - QPointF(80, 50), QSizeF(160, 100));
        break;
    }
    return it;
}

QString BoardCanvas::addItem(BoardItem item, bool atBack)
{
    const QString id = item.id;
    if (atBack)
        m_doc.items.prepend(std::move(item));
    else
        m_doc.items.append(std::move(item));
    return id;
}

void BoardCanvas::autoSizeText(BoardItem &item)
{
    if (item.type != BoardItem::Type::Text)
        return;
    BoardItem probe = item;
    if (probe.text.isEmpty())
        probe.text = QStringLiteral("M");
    const TextLayoutResult tl = Renderer::layoutText(probe);
    item.rect.setHeight(qMax(item.fontSize * 1.4, tl.contentHeight + 4));
}

// ---------------------------------------------------------------------------
// Commands

void BoardCanvas::undo()
{
    endTextEdit();
    if (!m_undo.canUndo())
        return;
    m_doc.items = m_undo.undo(m_doc.items);
    QStringList keep;
    for (const QString &id : std::as_const(m_selection))
        if (m_doc.find(id))
            keep << id;
    m_selection = keep;
    emit historyChanged();
    emit selectionChanged();
    markChanged();
}

void BoardCanvas::redo()
{
    endTextEdit();
    if (!m_undo.canRedo())
        return;
    m_doc.items = m_undo.redo(m_doc.items);
    QStringList keep;
    for (const QString &id : std::as_const(m_selection))
        if (m_doc.find(id))
            keep << id;
    m_selection = keep;
    emit historyChanged();
    emit selectionChanged();
    markChanged();
}

void BoardCanvas::deleteSelection()
{
    endTextEdit();
    QSet<QString> ids;
    int lockedCount = 0;
    for (const BoardItem *it : selectedItems()) {
        if (it->locked)
            ++lockedCount;
        else
            ids.insert(it->id);
    }
    if (lockedCount > 0)
        emit message(QStringLiteral("Locked items can't be deleted — unlock them first."));
    if (ids.isEmpty())
        return;
    pushUndo();
    // Connectors attached to deleted items keep their last position as a free end.
    for (BoardItem &it : m_doc.items) {
        if (it.type != BoardItem::Type::Connector || ids.contains(it.id))
            continue;
        if (ids.contains(it.fromId)) { it.fromPoint = m_doc.connectorStart(it); it.fromId.clear(); }
        if (ids.contains(it.toId)) { it.toPoint = m_doc.connectorEnd(it); it.toId.clear(); }
    }
    m_doc.items.removeIf([&](const BoardItem &it) { return ids.contains(it.id); });
    m_selection.clear();
    emit selectionChanged();
    markChanged();
}

void BoardCanvas::copySelection()
{
    m_clipboard.clear();
    QJsonArray arr;
    for (const BoardItem &it : m_doc.items)
        if (m_selection.contains(it.id)) {
            m_clipboard << it;
            arr.append(it.toJson());
        }
    m_pasteCount = 0;
    if (m_clipboard.isEmpty())
        return;
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(kClipboardMime), QJsonDocument(arr).toJson(QJsonDocument::Compact));
    QStringList texts;
    for (const BoardItem &it : std::as_const(m_clipboard))
        if (!it.text.isEmpty())
            texts << it.text;
    mime->setText(texts.join(QLatin1Char('\n')));
    QGuiApplication::clipboard()->setMimeData(mime);
}

void BoardCanvas::paste()
{
    endTextEdit();
    const QMimeData *mime = QGuiApplication::clipboard()->mimeData();
    if (mime && !mime->hasFormat(QString::fromLatin1(kClipboardMime)) && mime->hasText()
        && (mime->text().trimmed().startsWith(QLatin1String("miru://item/"))
            || mime->text().trimmed().startsWith(QLatin1String("blackmiro://item/")))) {
        followLink(mime->text().trimmed());   // "Copy link" pasted back: jump to the object
        return;
    }
    QList<BoardItem> source = m_clipboard;
    if (mime && mime->hasFormat(QString::fromLatin1(kClipboardMime))) {
        source.clear();
        const QJsonArray arr = QJsonDocument::fromJson(mime->data(QString::fromLatin1(kClipboardMime))).array();
        for (const QJsonValue &v : arr)
            source << BoardItem::fromJson(v.toObject());
    } else if (mime && mime->hasImage()) {
        const QImage img = qvariant_cast<QImage>(mime->imageData());
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG"); // clipboard bitmaps have no original file; PNG keeps every pixel
        const QPointF at = toScreen(pasteAnchor());
        addImageData(png, at.x(), at.y());
        return;
    } else if (mime && mime->hasText() && !mime->text().trimmed().isEmpty()) {
        pushUndo();
        BoardItem it = makeItem(BoardItem::Type::Sticky, pasteAnchor());
        it.text = mime->text().trimmed();
        setSelection({ addItem(it) });
        markChanged();
        return;
    }
    if (source.isEmpty())
        return;

    ++m_pasteCount;
    QPointF offset(24.0 * m_pasteCount, 24.0 * m_pasteCount);
    if (m_pasteAtPointer) {
        // Paste centred on the mouse pointer (or the view centre when the pointer is
        // outside the board); pasting again at the same spot steps the copies down.
        QRectF b;
        for (const BoardItem &it : std::as_const(source))
            if (it.type != BoardItem::Type::Connector || (it.fromId.isEmpty() && it.toId.isEmpty())) {
                const QRectF r = it.type == BoardItem::Type::Connector ? QRectF(it.fromPoint, it.toPoint).normalized() : it.rect;
                b = b.isNull() ? r : b.united(r);
            }
        const QPointF anchor = pasteAnchor();
        if (QLineF(anchor, m_lastPasteAnchor).length() > 2.0 / m_zoom)
            m_pasteRepeat = 0;
        m_lastPasteAnchor = anchor;
        offset = anchor - b.center() + QPointF(24.0, 24.0) * m_pasteRepeat++;
    }
    QHash<QString, QString> idMap;
    for (BoardItem &it : source) {
        const QString nid = BoardItem::newId();
        idMap.insert(it.id, nid);
        it.id = nid;
    }
    pushUndo();
    QStringList newSel;
    for (BoardItem it : std::as_const(source)) {
        it.rect.translate(offset);
        if (it.type == BoardItem::Type::Connector) {
            it.fromId = idMap.value(it.fromId);
            it.toId = idMap.value(it.toId);
            it.fromPoint += offset;
            it.toPoint += offset;
        }
        newSel << addItem(it, it.type == BoardItem::Type::Frame);
    }
    setSelection(newSel);
    markChanged();
}

void BoardCanvas::duplicateSelection()
{
    copySelection();
    m_pasteAtPointer = false;   // duplicates sit just below-right of the originals
    paste();
    m_pasteAtPointer = true;
}

QPointF BoardCanvas::pasteAnchor() const
{
    if (m_pointerInside && boundingRect().contains(m_hoverScreen))
        return toWorld(m_hoverScreen);
    return toWorld(boundingRect().center());
}

void BoardCanvas::selectAll()
{
    QStringList ids;
    for (const BoardItem &it : std::as_const(m_doc.items))
        ids << it.id;
    setSelection(ids);
}

void BoardCanvas::bringToFront()
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    QList<BoardItem> moved;
    m_doc.items.removeIf([&](const BoardItem &it) {
        if (m_selection.contains(it.id)) { moved << it; return true; }
        return false;
    });
    m_doc.items.append(moved);
    markChanged();
}

void BoardCanvas::sendToBack()
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    QList<BoardItem> moved;
    m_doc.items.removeIf([&](const BoardItem &it) {
        if (m_selection.contains(it.id)) { moved << it; return true; }
        return false;
    });
    m_doc.items = moved + m_doc.items;
    markChanged();
}

// ---------------------------------------------------------------------------
// Selection style properties

QString BoardCanvas::selType() const
{
    const auto items = selectedItems();
    if (items.isEmpty())
        return {};
    const QString t = BoardItem::typeName(items.first()->type);
    for (const BoardItem *it : items)
        if (BoardItem::typeName(it->type) != t)
            return QStringLiteral("mixed");
    return t;
}

#define SEL_GETTER(name, type, expr, fallback)            \
    type BoardCanvas::name() const                         \
    {                                                      \
        const auto items = selectedItems();                \
        if (items.isEmpty())                               \
            return fallback;                               \
        const BoardItem *it = items.first();               \
        return expr;                                       \
    }

SEL_GETTER(selFill, QColor, it->fill, QColor())
SEL_GETTER(selStroke, QColor, it->stroke, QColor())
SEL_GETTER(selTextColor, QColor, it->textColor, QColor())
SEL_GETTER(selStrokeWidth, double, it->strokeWidth, 0.0)
SEL_GETTER(selFontSize, double, it->fontSize, 0.0)
SEL_GETTER(selBold, bool, it->bold, false)
SEL_GETTER(selTextAlign, int, it->textAlign, 0)
SEL_GETTER(selElbow, bool, it->connectorStyle == BoardItem::ConnectorStyle::Elbow, false)

#undef SEL_GETTER

void BoardCanvas::setSelFill(const QColor &c) { applyToSelection([&](BoardItem &it) { it.fill = c; }); }
void BoardCanvas::setSelStroke(const QColor &c) { applyToSelection([&](BoardItem &it) { it.stroke = c; }); }
void BoardCanvas::setSelTextColor(const QColor &c) { applyToSelection([&](BoardItem &it) { it.textColor = c; }); }
void BoardCanvas::setSelStrokeWidth(double w) { applyToSelection([&](BoardItem &it) { it.strokeWidth = qBound(0.0, w, 64.0); }); }
void BoardCanvas::setSelFontSize(double s)
{
    applyToSelection([&](BoardItem &it) {
        it.fontSize = qBound(4.0, s, 400.0);
        autoSizeText(it);
    });
}
void BoardCanvas::setSelBold(bool b) { applyToSelection([&](BoardItem &it) { it.bold = b; autoSizeText(it); }); }
void BoardCanvas::setSelTextAlign(int a) { applyToSelection([&](BoardItem &it) { it.textAlign = qBound(0, a, 2); }); }
void BoardCanvas::setSelElbow(bool e)
{
    applyToSelection([&](BoardItem &it) {
        it.connectorStyle = e ? BoardItem::ConnectorStyle::Elbow : BoardItem::ConnectorStyle::Straight;
    });
}

// ---------------------------------------------------------------------------
// Text editing

void BoardCanvas::beginTextEdit(const QString &id)
{
    const BoardItem *it = m_doc.find(id);
    if (!it || !it->hasText())
        return;
    endTextEdit();
    m_editingId = id;
    m_editOriginalText = it->text;
    m_editSnapshot = m_doc.items;
    setSelection({ id });
    emit editingChanged();
    update();
}

void BoardCanvas::beginTextEditOnSelection()
{
    if (m_selection.size() == 1)
        beginTextEdit(m_selection.first());
}

void BoardCanvas::updateEditText(const QString &text)
{
    BoardItem *it = m_doc.find(m_editingId);
    if (!it || it->text == text)
        return;
    it->text = text;
    autoSizeText(*it);
    emit editingChanged();
    update();
}

void BoardCanvas::endTextEdit()
{
    if (m_editingId.isEmpty())
        return;
    const QString id = m_editingId;
    m_editingId.clear();
    BoardItem *it = m_doc.find(id);
    const bool changed = it && it->text != m_editOriginalText;
    if (it && it->type == BoardItem::Type::Text && it->text.trimmed().isEmpty()) {
        // Empty text boxes vanish, like in most whiteboards.
        m_doc.items.removeIf([&](const BoardItem &x) { return x.id == id; });
        m_selection.removeAll(id);
        emit selectionChanged();
    }
    const bool exists = m_doc.find(id) != nullptr;
    if (m_editIsNew ? exists : (changed || !exists)) {
        m_undo.push(m_editSnapshot);
        emit historyChanged();
        markChanged();
    }
    m_editIsNew = false;
    m_editSnapshot.clear();
    emit editingChanged();
    update();
}

QRectF BoardCanvas::editRect() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    if (!it)
        return {};
    QRectF r = Renderer::textRect(*it);
    if (it->type == BoardItem::Type::Text)
        r = it->rect;
    return toScreen(r);
}

QString BoardCanvas::editText() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    return it ? it->text : QString();
}

double BoardCanvas::editFontPixelSize() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    return it ? qMax(1.0, it->fontSize * m_zoom) : 16.0;
}

QColor BoardCanvas::editColor() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    if (!it)
        return Qt::black;
    if (it->type == BoardItem::Type::Frame)
        return fillColor().lightness() < 128 ? QColor(170, 176, 188) : QColor(80, 80, 80);
    return it->textColor;
}

int BoardCanvas::editAlign() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    if (!it)
        return 0;
    if (it->type == BoardItem::Type::Frame || (it->type == BoardItem::Type::Text && it->textAlign == 0))
        return 1;
    return it->textAlign;
}

bool BoardCanvas::editBold() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    return it && it->bold;
}

bool BoardCanvas::editCenterV() const
{
    const BoardItem *it = m_doc.find(m_editingId);
    return it && it->type != BoardItem::Type::Text && it->type != BoardItem::Type::Frame;
}

// ---------------------------------------------------------------------------
// Hit testing helpers

int BoardCanvas::handleAt(const QPointF &screen) const
{
    if (anySelectedLocked())
        return -1;
    const QRectF sel = selectionRect();
    if (sel.isNull())
        return -1;
    const QRectF s = toScreen(sel);
    const QPointF pts[8] = { s.topLeft(), QPointF(s.center().x(), s.top()), s.topRight(),
                             QPointF(s.right(), s.center().y()), s.bottomRight(),
                             QPointF(s.center().x(), s.bottom()), s.bottomLeft(),
                             QPointF(s.left(), s.center().y()) };
    for (int i = 0; i < 8; ++i)
        if (QLineF(pts[i], screen).length() <= kHandleRadius + 3)
            return i;
    return -1;
}

int BoardCanvas::connectorEndAt(const QPointF &screen) const
{
    if (m_selection.size() != 1)
        return -1;
    const BoardItem *c = m_doc.find(m_selection.first());
    if (!c || c->type != BoardItem::Type::Connector)
        return -1;
    const QPolygonF path = m_doc.connectorPath(*c);
    if (path.isEmpty())
        return -1;
    if (QLineF(toScreen(path.first()), screen).length() <= kHandleRadius + 3)
        return 0;
    if (QLineF(toScreen(path.last()), screen).length() <= kHandleRadius + 3)
        return 1;
    return -1;
}

QString BoardCanvas::connectableAt(const QPointF &world, const QString &exclude) const
{
    for (qsizetype i = m_doc.items.size() - 1; i >= 0; --i) {
        const BoardItem &it = m_doc.items[i];
        if (it.id == exclude || it.type == BoardItem::Type::Connector || it.type == BoardItem::Type::Pen)
            continue;
        if (it.type == BoardItem::Type::Frame)
            continue;
        if (it.rect.adjusted(-6 / m_zoom, -6 / m_zoom, 6 / m_zoom, 6 / m_zoom).contains(world))
            return it.id;
    }
    return {};
}

void BoardCanvas::updateHoverCursor(const QPointF &screen)
{
    if (m_tool == QLatin1String("hand") || m_spaceDown) {
        setCursor(Qt::OpenHandCursor);
        return;
    }
    if (m_tool != QLatin1String("select")) {
        setCursor(Qt::CrossCursor);
        return;
    }
    const int h = handleAt(screen);
    static const Qt::CursorShape shapes[8] = { Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
                                               Qt::SizeHorCursor, Qt::SizeFDiagCursor, Qt::SizeVerCursor,
                                               Qt::SizeBDiagCursor, Qt::SizeHorCursor };
    if (connectDotAt(screen) >= 0)
        setCursor(Qt::PointingHandCursor);
    else if (h >= 0)
        setCursor(shapes[h]);
    else if (connectorEndAt(screen) >= 0)
        setCursor(Qt::CrossCursor);
    else if (!m_doc.itemAt(toWorld(screen), 4.0 / m_zoom).isEmpty())
        setCursor(Qt::SizeAllCursor);
    else
        setCursor(Qt::ArrowCursor);
}

// ---------------------------------------------------------------------------
// Input

void BoardCanvas::mousePressEvent(QMouseEvent *e)
{
    pointerPress(e->position(), e->button(), e->modifiers(), false);
}

void BoardCanvas::pointerPress(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods, bool touch)
{
    forceActiveFocus(Qt::MouseFocusReason);
    endTextEdit();
    stopViewAnimation();
    m_panSamples.clear();
    m_pressScreen = m_lastScreen = pos;
    m_pressWorld = toWorld(pos);
    m_dragChanged = false;
    m_dragSnapshot = m_doc.items;
    m_dragOrigin.clear();
    m_rightPress = button == Qt::RightButton;

    if (button == Qt::MiddleButton || m_tool == QLatin1String("hand") || m_spaceDown
        || (button == Qt::RightButton)) {
        // Right button: a click opens the context menu (see release), a drag pans.
        m_drag = Drag::Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (cropping() && button == Qt::LeftButton) {
        const int h = handleAt(pos);
        if (h >= 0) {
            m_drag = Drag::Crop;
            m_handle = h;
            return;
        }
        if (const BoardItem *c = m_doc.find(m_cropId); c && c->rect.contains(m_pressWorld))
            return;          // clicks on the image keep crop mode open
        finishCrop();        // clicking elsewhere applies the crop
    }
    if (button != Qt::LeftButton)
        return;

    const bool shift = mods & Qt::ShiftModifier;

    if (m_tool == QLatin1String("select")) {
        // Connector endpoint re-attach
        const int end = connectorEndAt(pos);
        if (end >= 0) {
            m_drag = Drag::ConnectorEnd;
            m_connectorEnd = end;
            m_activeId = m_selection.first();
            return;
        }
        // Quick-connect dot: start a connector from the selected object.
        if (const int dot = connectDotAt(pos); dot >= 0) {
            BoardItem c = makeItem(BoardItem::Type::Connector, m_pressWorld);
            c.arrowEnd = true;
            c.connectorStyle = BoardItem::ConnectorStyle::Curved;
            c.fromId = m_selection.first();
            c.fromPoint = m_pressWorld;
            c.toPoint = m_pressWorld;
            m_quickDir = dot;
            m_activeId = addItem(c);
            m_drag = Drag::Connector;
            update();
            return;
        }
        // Resize handles
        const int h = handleAt(pos);
        if (h >= 0) {
            m_drag = Drag::Resize;
            m_handle = h;
            m_dragOriginBounds = selectionRect();
            m_snapExclude.clear();
            for (const BoardItem *it : selectedItems()) {
                m_dragOrigin.insert(it->id, *it);
                m_snapExclude.insert(it->id);
            }
            return;
        }
        // Link badge on an object: follow the link.
        if (const QString linked = linkBadgeAt(pos); !linked.isEmpty()) {
            if (const BoardItem *it = m_doc.find(linked))
                followLink(it->link);
            return;
        }
        // Comment pins sit above everything.
        if (const QString c = commentAt(pos); !c.isEmpty()) {
            setSelection({ c });
            m_drag = Drag::Move;
            const BoardItem *ci = m_doc.find(c);
            m_dragOrigin.insert(c, *ci);
            m_dragOriginBounds = QRectF();
            m_snapExclude = { c };
            const QRectF r = toScreen(ci->rect);
            emit commentOpened(c, r.right(), r.top());
            return;
        }
        const QString hit = m_doc.itemAt(m_pressWorld, 4.0 / m_zoom);
        if (!hit.isEmpty()) {
            const QStringList hitGroup = expandGroups({ hit });
            if (shift) {
                QStringList s = m_selection;
                if (s.contains(hit)) {
                    for (const QString &g : hitGroup) s.removeAll(g);
                } else {
                    for (const QString &g : hitGroup) if (!s.contains(g)) s << g;
                }
                setSelection(s);
            } else if (!m_selection.contains(hit)) {
                setSelection(hitGroup);
            }
            if (mods & Qt::AltModifier) {
                // Alt+drag drags a copy; the originals stay where they are.
                QHash<QString, QString> idMap;
                for (const BoardItem *it : selectedItems())
                    idMap.insert(it->id, BoardItem::newId());
                QList<BoardItem> added;
                QStringList copies;
                for (const BoardItem *it : selectedItems()) {
                    BoardItem c = *it;
                    c.id = idMap.value(it->id);
                    c.locked = false;
                    if (c.type == BoardItem::Type::Connector) {
                        c.fromId = idMap.value(c.fromId, c.fromId);
                        c.toId = idMap.value(c.toId, c.toId);
                    }
                    added << c;
                    copies << c.id;
                }
                for (const BoardItem &c : std::as_const(added))
                    addItem(c);
                setSelection(copies);
                m_dragChanged = true;   // the copy is a change even without movement
            }
            m_drag = Drag::Move;
            // Frames carry their contents.
            QStringList moving = m_selection;
            for (const BoardItem *it : selectedItems())
                if (it->type == BoardItem::Type::Frame)
                    for (const QString &child : m_doc.itemsInsideFrame(*it))
                        if (!moving.contains(child))
                            moving << child;
            for (const QString &id : std::as_const(moving))
                if (const BoardItem *it = m_doc.find(id); it && !it->locked)
                    m_dragOrigin.insert(id, *it);   // locked items stay put
            m_dragOriginBounds = QRectF();
            m_snapExclude.clear();
            for (const BoardItem &o : std::as_const(m_dragOrigin)) {
                m_snapExclude.insert(o.id);
                if (o.type != BoardItem::Type::Connector)
                    m_dragOriginBounds = m_dragOriginBounds.isNull() ? o.rect : m_dragOriginBounds.united(o.rect);
            }
            return;
        }
        if (!shift)
            setSelection({});
        if (touch) {   // on touch screens, dragging empty space pans the board
            m_drag = Drag::Pan;
            return;
        }
        m_drag = Drag::Marquee;
        m_marquee = QRectF(pos, QSizeF(0, 0));
        return;
    }

    if (m_tool == QLatin1String("eraser") || m_tool == QLatin1String("precision-eraser")) {
        m_drag = Drag::Erase;
        m_eraseChanged = false;
        eraseAt(m_pressWorld);
        return;
    }
    if (m_tool == QLatin1String("lasso")) {
        m_drag = Drag::Lasso;
        m_lasso = QPolygonF() << pos;
        return;
    }
    if (m_tool.startsWith(QLatin1String("emoji:"))) {
        BoardItem it = makeItem(BoardItem::Type::Text, m_pressWorld);
        it.text = m_tool.mid(6);
        it.fontSize = 64;
        it.textAlign = 0;
        it.rect = QRectF(m_pressWorld - QPointF(50, 50), QSizeF(100, 100));
        it.name = QStringLiteral("Emoji");
        const QString id = addItem(it);
        m_undo.push(m_dragSnapshot);
        emit historyChanged();
        setSelection({ id });
        markChanged();
        return;
    }
    if (m_tool == QLatin1String("comment")) {
        addCommentAt(pos.x(), pos.y());
        setTool(QStringLiteral("select"));
        return;
    }
    if (m_tool == QLatin1String("divider")) {
        BoardItem c = makeItem(BoardItem::Type::Connector, m_pressWorld);
        c.fromPoint = m_pressWorld - QPointF(160, 0);
        c.toPoint = m_pressWorld + QPointF(160, 0);
        c.arrowEnd = false;
        c.stroke = QColor(150, 155, 165);
        const QString id = addItem(c);
        m_undo.push(m_dragSnapshot);
        emit historyChanged();
        setTool(QStringLiteral("select"));
        setSelection({ id });
        markChanged();
        return;
    }

    if (m_tool == QLatin1String("pen") || m_tool == QLatin1String("highlighter") || m_tool == QLatin1String("smartpen")) {
        BoardItem it = makeItem(BoardItem::Type::Pen, m_pressWorld);
        it.rect = QRectF(m_pressWorld, QSizeF(0, 0));
        it.points << QPointF(0, 0);
        if (m_tool == QLatin1String("highlighter")) {
            it.stroke = m_highlighterColor;
            it.strokeWidth = 18;
        }
        m_activeId = addItem(it);
        m_drag = Drag::Draw;
        update();
        return;
    }

    if (m_tool == QLatin1String("connector") || m_tool == QLatin1String("line")
        || m_tool == QLatin1String("arrow") || m_tool == QLatin1String("elbow")) {
        BoardItem c = makeItem(BoardItem::Type::Connector, m_pressWorld);
        c.arrowEnd = m_tool != QLatin1String("line");
        if (m_tool == QLatin1String("elbow"))
            c.connectorStyle = BoardItem::ConnectorStyle::Elbow;
        c.fromId = connectableAt(m_pressWorld, {});
        c.fromPoint = m_pressWorld;
        c.toPoint = m_pressWorld;
        m_activeId = addItem(c);
        m_drag = Drag::Connector;
        update();
        return;
    }

    // Shape / sticky / text / frame creation
    static const QHash<QString, BoardItem::Type> creatable = {
        { QStringLiteral("rectangle"), BoardItem::Type::Rectangle },
        { QStringLiteral("rounded"), BoardItem::Type::RoundedRect },
        { QStringLiteral("ellipse"), BoardItem::Type::Ellipse },
        { QStringLiteral("diamond"), BoardItem::Type::Diamond },
        { QStringLiteral("triangle"), BoardItem::Type::Triangle },
        { QStringLiteral("sticky"), BoardItem::Type::Sticky },
        { QStringLiteral("text"), BoardItem::Type::Text },
        { QStringLiteral("frame"), BoardItem::Type::Frame },
    };
    if (m_tool.startsWith(QLatin1String("shape:")) || m_tool.startsWith(QLatin1String("icon:"))) {
        const bool icon = m_tool.startsWith(QLatin1String("icon:"));
        BoardItem it = makeItem(icon ? BoardItem::Type::Icon : BoardItem::Type::Shape, m_pressWorld);
        it.shapeId = m_tool.mid(icon ? 5 : 6);
        m_activeId = addItem(it);
        m_drag = Drag::Create;
        setSelection({ m_activeId });
        return;
    }
    const auto type = creatable.constFind(m_tool);
    if (type == creatable.constEnd())
        return;
    BoardItem it = makeItem(*type, m_pressWorld);
    if (*type == BoardItem::Type::Frame)
        it.text = QStringLiteral("Frame %1").arg(++m_frameCounter);
    m_activeId = addItem(it, *type == BoardItem::Type::Frame);
    m_drag = Drag::Create;
    setSelection({ m_activeId });
}

void BoardCanvas::mouseMoveEvent(QMouseEvent *e)
{
    pointerMove(e->position(), e->modifiers());
    e->accept();
}

void BoardCanvas::pointerMove(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    m_hoverScreen = pos;
    m_pointerInside = true;
    const QPointF world = toWorld(pos);
    const QPointF deltaWorld = world - m_pressWorld;

    switch (m_drag) {
    case Drag::Pan:
        m_pan += pos - m_lastScreen;
        trackPanVelocity(pos);
        emit viewChanged();
        update();
        break;

    case Drag::Move: {
        QPointF d = deltaWorld;
        if (mods & Qt::ShiftModifier) // axis lock
            (std::abs(d.x()) > std::abs(d.y())) ? d.setY(0) : d.setX(0);
        if (!(mods & Qt::ControlModifier) && !m_dragOriginBounds.isNull())   // Ctrl: no snapping
            d = snapMove(m_dragOriginBounds, d);
        else
            m_guides.clear();
        for (auto it = m_dragOrigin.constBegin(); it != m_dragOrigin.constEnd(); ++it) {
            BoardItem *item = m_doc.find(it.key());
            if (!item)
                continue;
            item->rect = it->rect.translated(d);
            if (item->type == BoardItem::Type::Connector) {
                if (item->fromId.isEmpty()) item->fromPoint = it->fromPoint + d;
                if (item->toId.isEmpty()) item->toPoint = it->toPoint + d;
            }
        }
        m_dragChanged = !d.isNull();
        update();
        break;
    }

    case Drag::Resize: {
        QRectF ob = m_dragOriginBounds;
        QRectF nb = ob;
        const int h = m_handle;
        if (h == 0 || h == 6 || h == 7) nb.setLeft(qMin(world.x(), ob.right() - 4));
        if (h == 2 || h == 3 || h == 4) nb.setRight(qMax(world.x(), ob.left() + 4));
        if (h == 0 || h == 1 || h == 2) nb.setTop(qMin(world.y(), ob.bottom() - 4));
        if (h == 4 || h == 5 || h == 6) nb.setBottom(qMax(world.y(), ob.top() + 4));

        bool keepAspect = mods & Qt::ShiftModifier;
        if (m_dragOrigin.size() == 1 && m_dragOrigin.cbegin()->type == BoardItem::Type::Image)
            keepAspect = !keepAspect;
        const bool corner = h == 0 || h == 2 || h == 4 || h == 6;
        if (keepAspect && corner && ob.height() > 0) {
            const double aspect = ob.width() / ob.height();
            const double s = qMax(nb.width() / ob.width(), nb.height() / ob.height());
            const QSizeF size(ob.width() * s, ob.width() * s / aspect);
            if (h == 0) nb = QRectF(ob.bottomRight() - QPointF(size.width(), size.height()), size);
            if (h == 2) nb = QRectF(QPointF(ob.left(), ob.bottom() - size.height()), size);
            if (h == 4) nb = QRectF(ob.topLeft(), size);
            if (h == 6) nb = QRectF(QPointF(ob.right() - size.width(), ob.top()), size);
        }

        if (!(mods & Qt::ControlModifier) && !(keepAspect && corner))
            nb = snapResize(nb, h);
        else
            m_guides.clear();
        const double sx = ob.width() > 0 ? nb.width() / ob.width() : 1.0;
        const double sy = ob.height() > 0 ? nb.height() / ob.height() : 1.0;
        for (auto it = m_dragOrigin.constBegin(); it != m_dragOrigin.constEnd(); ++it) {
            BoardItem *item = m_doc.find(it.key());
            if (!item || !isResizable(*item))
                continue;
            const QRectF r = it->rect;
            item->rect = QRectF(nb.left() + (r.left() - ob.left()) * sx, nb.top() + (r.top() - ob.top()) * sy,
                                r.width() * sx, r.height() * sy);
            if (item->type == BoardItem::Type::Pen) {
                item->points = it->points;
                for (QPointF &p : item->points)
                    p = QPointF(p.x() * sx, p.y() * sy);
            }
        }
        m_dragChanged = true;
        update();
        break;
    }

    case Drag::Marquee:
        m_marquee = QRectF(m_pressScreen, pos);
        update();
        break;

    case Drag::Create: {
        BoardItem *it = m_doc.find(m_activeId);
        if (!it)
            break;
        QRectF r = QRectF(m_pressWorld, world).normalized();
        if (mods & Qt::ShiftModifier || it->type == BoardItem::Type::Sticky) {
            const double s = qMax(r.width(), r.height());
            r = QRectF(m_pressWorld, QSizeF(world.x() < m_pressWorld.x() ? -s : s,
                                            world.y() < m_pressWorld.y() ? -s : s)).normalized();
        }
        if ((pos - m_pressScreen).manhattanLength() > 6) {
            it->rect = r;
            m_dragChanged = true;
        }
        update();
        break;
    }

    case Drag::Draw: {
        BoardItem *it = m_doc.find(m_activeId);
        if (!it)
            break;
        if (QLineF(pos, m_lastScreen).length() < 1.5)
            return; // keep m_lastScreen until movement is meaningful
        it->points << world - it->rect.topLeft();
        update();
        break;
    }

    case Drag::Connector:
    case Drag::ConnectorEnd: {
        BoardItem *c = m_doc.find(m_activeId);
        if (!c)
            break;
        const bool isStart = m_drag == Drag::ConnectorEnd && m_connectorEnd == 0;
        const QString other = isStart ? c->toId : c->fromId;
        m_hoverTarget = connectableAt(world, other);
        if (isStart) {
            c->fromId = m_hoverTarget;
            c->fromPoint = world;
        } else {
            c->toId = m_hoverTarget;
            c->toPoint = world;
        }
        m_dragChanged = true;
        update();
        break;
    }

    case Drag::Crop:
        cropDrag(world);
        break;

    case Drag::Erase:
        eraseAt(world);
        break;

    case Drag::Lasso:
        if (QLineF(pos, m_lasso.last()).length() > 3)
            m_lasso << pos;
        update();
        break;

    case Drag::None:
        break;
    }
    updateInteracting();
    if (m_drag != Drag::None) {
        emit selectionGeometryChanged();
        // Stream live moves / resizes / strokes to collaborators while dragging.
        if (m_drag == Drag::Move || m_drag == Drag::Resize || m_drag == Drag::Draw || m_drag == Drag::Create
            || m_drag == Drag::Connector || m_drag == Drag::ConnectorEnd || m_drag == Drag::Crop)
            emit localEdit();
    }
    emit pointerWorldMoved(world.x(), world.y());
    m_lastScreen = pos;
}

void BoardCanvas::finishCreate()
{
    BoardItem *it = m_doc.find(m_activeId);
    if (!it)
        return;
    if (!m_dragChanged || it->rect.width() < 8 || it->rect.height() < 8) {
        // Click without drag: default size centered at the click.
        const BoardItem def = makeItem(it->type, m_pressWorld);
        it->rect = it->type == BoardItem::Type::Frame
            ? QRectF(m_pressWorld - QPointF(def.rect.width() / 2, def.rect.height() / 2), def.rect.size())
            : def.rect;
    }
    autoSizeText(*it);
    const BoardItem::Type type = it->type;
    const QString id = it->id;
    const QList<BoardItem> before = m_dragSnapshot;
    if (!(type == BoardItem::Type::Sticky && m_stickyStack))
        setTool(QStringLiteral("select"));   // Stack: keep dropping notes
    setSelection({ id });
    if (type == BoardItem::Type::Sticky || type == BoardItem::Type::Text) {
        // Creation + first typing form a single undo step.
        beginTextEdit(id);
        m_editSnapshot = before;
        m_editIsNew = true;
    } else {
        m_undo.push(before);
        emit historyChanged();
    }
    markChanged();
}

void BoardCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    pointerRelease(e->position(), e->modifiers());
    e->accept();
}

void BoardCanvas::pointerRelease(const QPointF &pos, Qt::KeyboardModifiers mods)
{
    const Drag drag = m_drag;
    m_drag = Drag::None;

    switch (drag) {
    case Drag::Pan:
        updateHoverCursor(pos);
        if (m_rightPress && QLineF(pos, m_pressScreen).length() < 5)
            openContextMenuAt(pos);
        else
            startGlide();   // keep the board moving and ease it to a stop
        break;

    case Drag::Crop:
        update();
        break;

    case Drag::Move:
    case Drag::Resize:
    case Drag::ConnectorEnd:
        if (m_dragChanged) {
            m_undo.push(m_dragSnapshot);
            emit historyChanged();
            markChanged();
        }
        break;

    case Drag::Marquee: {
        const QRectF worldRect(toWorld(m_marquee.normalized().topLeft()), toWorld(m_marquee.normalized().bottomRight()));
        QStringList ids = (mods & Qt::ShiftModifier) ? m_selection : QStringList();
        if (worldRect.width() > 2 || worldRect.height() > 2)
            for (const QString &id : m_doc.itemsIn(worldRect))
                if (!ids.contains(id))
                    ids << id;
        m_marquee = QRectF();
        setSelection(ids);
        break;
    }

    case Drag::Create:
        finishCreate();
        break;

    case Drag::Draw: {
        BoardItem *it = m_doc.find(m_activeId);
        if (it) {
            // Normalize: rect = bounds of absolute points; points relative to it.
            QPolygonF abs;
            for (const QPointF &p : std::as_const(it->points))
                abs << it->rect.topLeft() + p;
            const QRectF b = abs.boundingRect();
            it->rect = QRectF(b.topLeft(), b.size().expandedTo(QSizeF(1, 1)));
            for (qsizetype i = 0; i < abs.size(); ++i)
                it->points[i] = abs[i] - b.topLeft();
            if (m_tool == QLatin1String("smartpen"))
                smartReplace(m_activeId);
            m_undo.push(m_dragSnapshot);
            emit historyChanged();
            markChanged();
        }
        break;
    }

    case Drag::Erase:
        if (m_eraseChanged) {
            m_undo.push(m_dragSnapshot);
            emit historyChanged();
            markChanged();
        }
        break;

    case Drag::Lasso: {
        QPolygonF world;
        for (const QPointF &pt : std::as_const(m_lasso))
            world << toWorld(pt);
        QStringList ids;
        if (world.size() >= 3)
            for (const BoardItem &it : std::as_const(m_doc.items))
                if (m_doc.itemPickable(it) && world.containsPoint(m_doc.itemBounds(it).center(), Qt::OddEvenFill))
                    ids << it.id;
        m_lasso.clear();
        setSelection(ids);
        break;
    }

    case Drag::Connector: {
        BoardItem *c = m_doc.find(m_activeId);
        if (c) {
            const bool tiny = QLineF(m_pressScreen, pos).length() < 8;
            if (tiny && m_quickDir >= 0) {
                // Click on a connect dot: add a connected copy in that direction.
                quickConnectCreate(m_activeId);
                m_undo.push(m_dragSnapshot);
                emit historyChanged();
                markChanged();
            } else if (tiny && c->toId.isEmpty()) {
                m_doc.items.removeIf([&](const BoardItem &x) { return x.id == m_activeId; });
            } else {
                m_undo.push(m_dragSnapshot);
                emit historyChanged();
                markChanged();
                setSelection({ m_activeId });
            }
        }
        m_hoverTarget.clear();
        setTool(QStringLiteral("select"));
        break;
    }

    case Drag::None:
        break;
    }
    m_hoverTarget.clear();
    m_quickDir = -1;
    m_activeId.clear();
    m_dragOrigin.clear();
    m_dragSnapshot.clear();
    m_guides.clear();
    m_rightPress = false;
    updateInteracting();
    update();
}

void BoardCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    pointerDoubleClick(e->position());
    e->accept();
}

void BoardCanvas::pointerDoubleClick(const QPointF &pos)
{
    const QPointF world = toWorld(pos);
    if (!commentAt(pos).isEmpty())
        return;
    const QString hit = m_doc.itemAt(world, 4.0 / m_zoom);
    if (!hit.isEmpty()) {
        beginTextEdit(hit);
        return;
    }
    if (m_tool == QLatin1String("select")) {
        // Double-click on empty canvas drops a text box.
        const QList<BoardItem> before = m_doc.items;
        BoardItem it = makeItem(BoardItem::Type::Text, world);
        const QString id = addItem(it);
        beginTextEdit(id);
        m_editSnapshot = before;   // removed again without an undo entry if left empty
        m_editIsNew = true;
        update();
    }
}

void BoardCanvas::hoverMoveEvent(QHoverEvent *e)
{
    updateHoverCursor(e->position());
    m_hoverScreen = e->position();
    m_pointerInside = true;
    if (m_tool.endsWith(QLatin1String("eraser")))
        update();
    const QPointF w = toWorld(e->position());
    emit pointerWorldMoved(w.x(), w.y());
    if (m_tool == QLatin1String("connector")) {
        const QString t = connectableAt(toWorld(e->position()), {});
        if (t != m_hoverTarget) {
            m_hoverTarget = t;
            update();
        }
    }
}

void BoardCanvas::wheelEvent(QWheelEvent *e)
{
    const QPoint pixel = e->pixelDelta();
    const QPoint angle = e->angleDelta();
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    // Navigation mode (right-click menu): mouse = wheel zooms, trackpad = scroll pans,
    // auto = decide from the event (pixel deltas come from touchpads).
    const bool trackpad = m_navMode == QLatin1String("trackpad")
        || (m_navMode == QLatin1String("auto") && !pixel.isNull() && e->source() != Qt::MouseEventNotSynthesized);
    if (!ctrl && trackpad) {
        m_pan += pixel.isNull() ? QPointF(angle) / 2.0 : QPointF(pixel);
        emit viewChanged();
        if (editing()) emit editingChanged();
        update();
    } else if (!ctrl && (e->modifiers() & Qt::ShiftModifier)) {
        m_pan += QPointF(angle.y() != 0 ? angle.y() : angle.x(), 0) / 2.0;
        emit viewChanged();
        update();
    } else {
        const double steps = (angle.y() != 0 ? angle.y() : pixel.y()) / 120.0;
        if (ctrl && !pixel.isNull())
            setZoomAt(m_zoom * std::pow(1.15, steps), e->position());     // trackpad pinch: follow the fingers
        else
            smoothZoomBy(std::pow(1.2, steps), e->position());            // mouse wheel: eased zoom
    }
    e->accept();
}

void BoardCanvas::touchEvent(QTouchEvent *e)
{
    // Qt Quick does not synthesize mouse events for items that accept touch,
    // so one finger is mapped onto the same press/move/release logic here.
    const auto &pts = e->points();
    if (pts.isEmpty()) {
        e->accept();
        return;
    }

    if (pts.size() >= 2) {
        const QPointF a = pts[0].position();
        const QPointF b = pts[1].position();
        const QPointF center = (a + b) / 2.0;
        const double dist = QLineF(a, b).length();
        if (!m_pinching) {
            // A second finger cancels whatever the first one started.
            if (m_drag != Drag::None && m_drag != Drag::Pan && !m_dragSnapshot.isEmpty()) {
                m_doc.items = m_dragSnapshot;
                m_dragSnapshot.clear();
            }
            m_drag = Drag::None;
            m_activeId.clear();
            m_dragOrigin.clear();
            m_marquee = QRectF();
            m_pinching = true;
            m_pinchStartDist = qMax(1.0, dist);
            m_pinchStartZoom = m_zoom;
            m_pinchLastCenter = center;
        }
        m_pan += center - m_pinchLastCenter;
        m_pinchLastCenter = center;
        setZoomAt(m_pinchStartZoom * dist / m_pinchStartDist, center);
        if (e->type() == QEvent::TouchEnd || e->type() == QEvent::TouchCancel)
            m_pinching = false;
        e->accept();
        return;
    }

    const QEventPoint &pt = pts.first();
    const QPointF pos = pt.position();
    if (m_pinching) {
        // Remaining finger after a pinch: ignore until all fingers lift.
        if (e->type() == QEvent::TouchEnd || e->type() == QEvent::TouchCancel)
            m_pinching = false;
        e->accept();
        return;
    }

    switch (pt.state()) {
    case QEventPoint::State::Pressed: {
        const bool isDouble = m_lastTap.isValid() && m_lastTap.elapsed() < 350
                              && QLineF(pos, m_lastTapPos).length() < 24;
        if (isDouble) {
            m_lastTap.invalidate();
            pointerDoubleClick(pos);
            m_drag = Drag::None;
        } else {
            pointerPress(pos, Qt::LeftButton, Qt::NoModifier, true);
            m_longPressPos = pos;
            m_longPress.start();
        }
        break;
    }
    case QEventPoint::State::Updated:
        if (QLineF(pos, m_longPressPos).length() > 10)
            m_longPress.stop();
        if (m_drag != Drag::None)
            pointerMove(pos, Qt::NoModifier);
        break;
    case QEventPoint::State::Released:
        m_longPress.stop();
        if (m_drag != Drag::None) {
            const bool wasTap = QLineF(pos, m_pressScreen).length() < 10;
            pointerRelease(pos, Qt::NoModifier);
            if (wasTap) {
                m_lastTap.start();
                m_lastTapPos = pos;
            }
        }
        break;
    default:
        break;
    }
    if (e->type() == QEvent::TouchCancel && m_drag != Drag::None)
        pointerRelease(pos, Qt::NoModifier);
    e->accept();
}

void BoardCanvas::keyPressEvent(QKeyEvent *e)
{
    const bool ctrl = e->modifiers() & Qt::ControlModifier;
    const bool shift = e->modifiers() & Qt::ShiftModifier;
    const int key = e->key();

    if (ctrl && (e->modifiers() & Qt::AltModifier)) {
        if (key == Qt::Key_C) { copyStyle(); e->accept(); return; }
        if (key == Qt::Key_V) { pasteStyle(); e->accept(); return; }
    }
    const bool alt = e->modifiers() & Qt::AltModifier;
    if (ctrl && alt && key == Qt::Key_K) { emit linkActivated(QStringLiteral("miru:edit-link")); e->accept(); return; }
    if (ctrl && shift) {
        switch (key) {
        case Qt::Key_L: setSelLocked(!selLocked()); break;
        case Qt::Key_C: copyAsPng(); break;                 // Copy as image
        case Qt::Key_G: ungroupSelection(); break;
        case Qt::Key_Z: redo(); break;
        default: e->ignore(); return;
        }
        e->accept();
        return;
    }
    if (alt && !ctrl) {
        switch (key) {
        case Qt::Key_1: zoomToContent(); break;             // Show all
        case Qt::Key_2: zoomToSelection(); break;
        case Qt::Key_Left: duplicateInDirection(-1, 0); break;
        case Qt::Key_Right: duplicateInDirection(1, 0); break;
        case Qt::Key_Up: duplicateInDirection(0, -1); break;
        case Qt::Key_Down: duplicateInDirection(0, 1); break;
        default: e->ignore(); return;
        }
        e->accept();
        return;
    }
    if (ctrl) {
        switch (key) {
        case Qt::Key_Z: shift ? redo() : undo(); break;
        case Qt::Key_Y: redo(); break;
        case Qt::Key_C: copySelection(); break;
        case Qt::Key_X: copySelection(); deleteSelection(); break;
        case Qt::Key_V: paste(); break;
        case Qt::Key_D: duplicateSelection(); break;
        case Qt::Key_A: selectAll(); break;
        case Qt::Key_0: resetZoom(); break;
        case Qt::Key_1: zoomToFit(); break;
        case Qt::Key_Plus:
        case Qt::Key_Equal: zoomIn(); break;
        case Qt::Key_Minus: zoomOut(); break;
        case Qt::Key_G: groupSelection(); break;
        case Qt::Key_B: setSelBold(!selBold()); break;
        case Qt::Key_Backspace: clearContents(); break;
        default: e->ignore(); return;
        }
        e->accept();
        return;
    }

    switch (key) {
    case Qt::Key_Delete:
    case Qt::Key_Backspace: deleteSelection(); break;
    case Qt::Key_Escape:
        if (cropping()) { finishCrop(); break; }
        clearSelection();
        setTool(QStringLiteral("select"));
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (cropping()) { finishCrop(); break; }
        beginTextEditOnSelection();
        break;
    case Qt::Key_Space:
        if (!e->isAutoRepeat()) { m_spaceDown = true; setCursor(Qt::OpenHandCursor); }
        break;
    case Qt::Key_Tab: selectNext(false); break;
    case Qt::Key_Backtab: selectNext(true); break;
    case Qt::Key_PageUp: shift ? bringForward() : bringToFront(); break;
    case Qt::Key_Backslash: arrangeSelection(shift); break;   // \ row, Shift+\ column
    case Qt::Key_Bar: arrangeSelection(true); break;          // Shift+\ on US layouts
    case Qt::Key_PageDown: shift ? sendBackward() : sendToBack(); break;
    case Qt::Key_F1: emit shortcutsRequested(); break;
    case Qt::Key_Slash: emit linkActivated(QStringLiteral("miru:cursor-chat")); break;
    case Qt::Key_G: setShowGrid(!m_showGrid); break;
    case Qt::Key_E: setTool(QStringLiteral("eraser")); break;
    case Qt::Key_C: setTool(QStringLiteral("comment")); break;
    case Qt::Key_S: setTool(QStringLiteral("rectangle")); emit shapesRequested(); break;
    case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4: case Qt::Key_5:
    case Qt::Key_6: case Qt::Key_7: case Qt::Key_8: case Qt::Key_9:
        emit toolSlotRequested(key - Qt::Key_0);
        break;
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down: {
        if (m_selection.isEmpty()) {
            // Nothing selected: arrows move around the board.
            const double s = shift ? 240 : 60;
            m_pan += QPointF(key == Qt::Key_Left ? s : key == Qt::Key_Right ? -s : 0,
                             key == Qt::Key_Up ? s : key == Qt::Key_Down ? -s : 0);
            emit viewChanged();
            update();
            break;
        }
        const double step = shift ? 10.0 : 1.0;
        const QPointF d(key == Qt::Key_Left ? -step : key == Qt::Key_Right ? step : 0,
                        key == Qt::Key_Up ? -step : key == Qt::Key_Down ? step : 0);
        applyToSelection([&](BoardItem &it) {
            if (it.locked)
                return;
            it.rect.translate(d);
            if (it.type == BoardItem::Type::Connector) {
                if (it.fromId.isEmpty()) it.fromPoint += d;
                if (it.toId.isEmpty()) it.toPoint += d;
            }
        });
        break;
    }
    case Qt::Key_V: setTool(QStringLiteral("select")); break;
    case Qt::Key_H: setTool(QStringLiteral("hand")); break;
    case Qt::Key_N: setTool(QStringLiteral("sticky")); break;
    case Qt::Key_R: setTool(QStringLiteral("rectangle")); break;
    case Qt::Key_O: setTool(QStringLiteral("ellipse")); break;
    case Qt::Key_T: setTool(QStringLiteral("text")); break;
    case Qt::Key_P: setTool(QStringLiteral("pen")); break;
    case Qt::Key_L: setTool(QStringLiteral("connector")); break;
    case Qt::Key_F: setTool(QStringLiteral("frame")); break;
    default:
        e->ignore();
        return;
    }
    e->accept();
}

void BoardCanvas::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = false;
        updateHoverCursor(m_lastScreen);
        e->accept();
        return;
    }
    e->ignore();
}

// ---------------------------------------------------------------------------
// Files

void BoardCanvas::newBoard()
{
    endTextEdit();
    if (cropping())
        finishCrop();
    m_doc.clear();
    m_doc.owner = m_ownerEmail;
    m_sigs.clear();
    m_activeLayer.clear();
    emit layersChanged();
    m_password.clear();
    m_undo.clear();
    m_selection.clear();
    m_filePath.clear();
    m_modified = false;
    m_frameCounter = 0;
    m_zoom = 1.0;
    m_pan = QPointF(width() / 2, height() / 2);
    emit selectionChanged();
    emit historyChanged();
    emit fileChanged();
    emit versionsChanged();
    emit contentChanged();
    emit viewChanged();
    update();
}

bool BoardCanvas::openData(const QByteArray &data, const QString &name, const QString &password)
{
    if (BoardFile::isEncrypted(data) && password.isEmpty()) {
        // Private board: keep it until QML collects the password (unlockPending).
        m_pendingData = data;
        m_pendingName = name;
        emit passwordRequired(name);
        return false;
    }
    Document loaded;
    QString error;
    if (!BoardFile::load(data, loaded, &error, password)) {
        emit message(QStringLiteral("Could not open %1: %2").arg(name, error));
        return false;
    }
    endTextEdit();
    if (cropping())
        finishCrop();
    m_password = loaded.isPrivate ? password : QString();
    m_pendingData.clear();
    m_doc = std::move(loaded);
    m_activeLayer.clear();
    emit layersChanged();
    if (m_doc.owner.isEmpty())
        m_doc.owner = m_ownerEmail;
    m_undo.clear();
    m_selection.clear();
    m_modified = false;
    m_frameCounter = 0;
    for (const BoardItem &it : std::as_const(m_doc.items))
        if (it.type == BoardItem::Type::Frame)
            ++m_frameCounter;
    refreshSignatures();
    emit selectionChanged();
    emit historyChanged();
    emit fileChanged();
    emit versionsChanged();
    emit contentChanged();
    if (m_doc.startView.isEmpty())
        zoomToFit();
    else
        goToStartView();
    return true;
}

bool BoardCanvas::openFile(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit message(QStringLiteral("Could not open %1: %2").arg(path, f.errorString()));
        return false;
    }
    m_pendingPath = path;
    if (!openData(f.readAll(), QFileInfo(path).fileName()))
        return false;
    m_pendingPath.clear();
    m_filePath = path;
    emit fileChanged();
    emit boardSaved(path, m_doc.title);
    return true;
}

QByteArray BoardCanvas::saveData(const QString &password)
{
    endTextEdit();
    if (cropping())
        finishCrop();
    if (m_doc.isPrivate && !checkPassword(password)) {
        emit message(QStringLiteral("Wrong password — the private board was not saved."));
        return {};
    }
    if (m_doc.owner.isEmpty())
        m_doc.owner = m_ownerEmail;
    const QDateTime now = QDateTime::currentDateTime();
    if (!m_autosaving || !m_lastVersionAt.isValid() || m_lastVersionAt.secsTo(now) >= 600) {
        m_doc.versions.append({ now, m_doc.itemsToJson() });
        while (m_doc.versions.size() > kMaxVersions)
            m_doc.versions.removeFirst();
        m_lastVersionAt = now;
        emit versionsChanged();
    }
    return BoardFile::save(m_doc, true, m_doc.isPrivate ? password : QString());
}

bool BoardCanvas::saveFile(const QUrl &url, const QString &password)
{
    QString path = url.isEmpty() ? m_filePath : (url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (path.isEmpty())
        return false;
    // Android pickers return content:// URIs whose names we must not touch.
    const bool plainPath = url.isEmpty() ? !path.contains(QLatin1String("://")) : url.isLocalFile();
    if (plainPath && !path.endsWith(QLatin1String(".board"), Qt::CaseInsensitive))
        path += QStringLiteral(".board");
    return writeBoardFile(path, plainPath, password, false);
}

bool BoardCanvas::writeBoardFile(const QString &path, bool plainPath, const QString &password, bool quiet)
{
    m_autosave.stop();
    // A new board takes its name from the file it is first saved as.
    if (plainPath && (m_doc.title.isEmpty() || m_doc.title == Document().title))
        m_doc.title = QFileInfo(path).completeBaseName();
    const QByteArray data = saveData(password);
    if (data.isEmpty())
        return false;
    bool ok = false;
    QString error;
    if (plainPath) {
        // Write-then-rename: a crash or full disk never leaves a half-written board.
        QSaveFile f(path);
        ok = f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit();
        error = f.errorString();
    } else {
        QFile f(path);   // content:// (Android): the provider owns the file, write in place
        ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
        error = f.errorString();
    }
    if (!ok) {
        emit message(QStringLiteral("Could not save %1: %2").arg(plainPath ? QFileInfo(path).fileName() : path, error));
        return false;
    }
    m_filePath = path;
    m_modified = false;
    if (m_doc.isPrivate)
        m_password = password;   // in memory only, so autosave can keep it encrypted
    emit fileChanged();
    noteSavedRecent(path, quiet);
    if (quiet) {
        emit autosaved(path);
    } else {
        const QString what = m_doc.isPrivate ? QStringLiteral("Saved (encrypted) ") : QStringLiteral("Saved ");
        emit message(plainPath ? what + QFileInfo(path).fileName() + QStringLiteral(" — changes now save automatically")
                               : what.trimmed());
    }
    return true;
}

void BoardCanvas::noteSavedRecent(const QString &path, bool quiet)
{
    // Keeps "Recent boards" current without re-rendering its thumbnail on every autosave.
    if (quiet && m_recentClock.isValid() && m_recentClock.elapsed() < 60000)
        return;
    m_recentClock.start();
    emit boardSaved(path, m_doc.title);
}

void BoardCanvas::scheduleAutosave()
{
#ifdef Q_OS_WASM
    const bool hasTarget = true;            // web: always into the browser (and a linked file)
#else
    const bool hasTarget = !m_filePath.isEmpty();
#endif
    if (m_modified && hasTarget && (!m_doc.isPrivate || !m_password.isEmpty()))
        m_autosave.start();   // restarts: saves once editing pauses
}

bool BoardCanvas::flushAutosave()
{
#ifdef Q_OS_WASM
    if (m_modified && (!m_doc.isPrivate || !m_password.isEmpty())) {
        endTextEdit();
        autosaveToBrowser();   // asynchronous: the browser finishes it even while hiding the page
    }
    return !m_modified;
#else
    if (m_modified && !m_filePath.isEmpty() && (!m_doc.isPrivate || !m_password.isEmpty())) {
        endTextEdit();
        m_autosaving = true;
        writeBoardFile(m_filePath, !m_filePath.contains(QLatin1String("://")), m_password, true);
        m_autosaving = false;
    }
    return !m_modified;
#endif
}

void BoardCanvas::autosaveNow()
{
#ifdef Q_OS_WASM
    const bool hasTarget = true;
#else
    const bool hasTarget = !m_filePath.isEmpty();
#endif
    if (!m_modified || !hasTarget || (m_doc.isPrivate && m_password.isEmpty()))
        return;
    if (editing() || m_drag != Drag::None) {   // finish the typing or gesture first
        m_autosave.start();
        return;
    }
#ifdef Q_OS_WASM
    autosaveToBrowser();
#else
    m_autosaving = true;
    writeBoardFile(m_filePath, !m_filePath.contains(QLatin1String("://")), m_password, true);
    m_autosaving = false;
#endif
}

void BoardCanvas::autosaveToBrowser()
{
#ifdef Q_OS_WASM
    m_autosaving = true;
    const QByteArray data = saveData(m_password);
    m_autosaving = false;
    if (data.isEmpty())
        return;
    const int serial = m_changeSerial;
    const QString uid = boardUid();
    QPointer<BoardCanvas> self(this);
    WebStorage::save(uid, m_doc.title, data, [self, serial, uid](const QString &status) {
        if (!self)
            return;
        if (status.isEmpty()) {
            emit self->message(QStringLiteral("Couldn't keep this board in the browser (storage full or blocked)."));
            return;
        }
        if (status == QLatin1String("permission") && !self->m_permissionHinted) {
            self->m_permissionHinted = true;
            emit self->message(QStringLiteral("Changes are kept in this browser. Click Save once to keep writing them to your file."));
        }
        if (self->m_changeSerial == serial && self->m_modified) {
            self->m_modified = false;
            emit self->fileChanged();
        }
        self->noteSavedRecent(QStringLiteral("browser:") + uid, true);
        emit self->autosaved(QStringLiteral("browser:") + uid);
    });
#endif
}

void BoardCanvas::setBoardTitle(const QString &t)
{
    if (m_doc.title == t)
        return;
    m_doc.title = t;
    emit localEdit();
    m_modified = true;
    emit fileChanged();
}

QStringList BoardCanvas::versionLabels() const
{
    QStringList out;
    for (qsizetype i = m_doc.versions.size() - 1; i >= 0; --i) {
        const BoardVersion &v = m_doc.versions[i];
        out << QStringLiteral("%1  ·  %2 items")
                   .arg(QLocale().toString(v.timestamp, QLocale::ShortFormat))
                   .arg(v.items.size());
    }
    return out;
}

void BoardCanvas::restoreVersion(int index)
{
    // index 0 = newest (matches versionLabels order)
    const qsizetype i = m_doc.versions.size() - 1 - index;
    if (i < 0 || i >= m_doc.versions.size())
        return;
    endTextEdit();
    pushUndo();
    m_doc.itemsFromJson(m_doc.versions[i].items);
    m_selection.clear();
    emit selectionChanged();
    markChanged();
    emit message(QStringLiteral("Restored version — press Ctrl+Z to undo."));
}

bool BoardCanvas::addImageFromUrl(const QUrl &url, double sx, double sy)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        emit message(QStringLiteral("Could not read %1").arg(path));
        return false;
    }
    if (path.endsWith(QLatin1String(".board"), Qt::CaseInsensitive))
        return openFile(url);
    return addImageData(f.readAll(), sx, sy);
}

bool BoardCanvas::addImageData(const QByteArray &bytes, double sx, double sy)
{
    const QPointF anchor = (sx < 0 || sy < 0) ? boundingRect().center() : QPointF(sx, sy);
    return placeImagesInRow({ bytes }, toWorld(anchor)) > 0;
}

int BoardCanvas::addImagesFromUrls(const QList<QUrl> &urls, double sx, double sy)
{
    QList<QByteArray> images;
    QStringList names;
    for (const QUrl &url : urls) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (path.endsWith(QLatin1String(".board"), Qt::CaseInsensitive)) {
            openFile(url);              // a dropped board opens instead of being inserted
            return 0;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            emit message(QStringLiteral("Could not read %1").arg(path));
            continue;
        }
        images << f.readAll();
        names << QFileInfo(path).fileName();
    }
    const QPointF anchor = (sx < 0 || sy < 0) ? boundingRect().center() : QPointF(sx, sy);
    return placeImagesInRow(images, toWorld(anchor), names);
}

int BoardCanvas::placeImagesInRow(const QList<QByteArray> &files, const QPointF &center, const QStringList &names)
{
    struct Decoded { QByteArray bytes; QSizeF size; QString name; };
    QList<Decoded> ok;
    int skipped = 0;
    for (qsizetype i = 0; i < files.size(); ++i) {
        const QImage img = QImage::fromData(files[i]);
        if (img.isNull()) { ++skipped; continue; }
        ok.append({ files[i], QSizeF(img.size()), names.value(i) });
    }
    if (ok.isEmpty()) {
        emit message(QStringLiteral("Unsupported image format."));
        return 0;
    }

    // One image: up to 800 px on its longest side. Several: a tidy horizontal row,
    // each at most 400 px tall, vertically centred, 24 px apart, row centred on the drop point.
    constexpr double kGap = 24.0;
    for (Decoded &d : ok) {
        if (ok.size() == 1) {
            if (d.size.width() > 800 || d.size.height() > 800)
                d.size.scale(800, 800, Qt::KeepAspectRatio);
        } else if (d.size.height() > 400) {
            d.size.scale(d.size.width(), 400, Qt::KeepAspectRatio);
        }
    }
    double total = kGap * (ok.size() - 1);
    for (const Decoded &d : std::as_const(ok))
        total += d.size.width();

    endTextEdit();
    pushUndo();
    QStringList added;
    double x = center.x() - total / 2.0;
    for (const Decoded &d : std::as_const(ok)) {
        BoardItem it;
        it.id = BoardItem::newId();
        it.type = BoardItem::Type::Image;
        it.rect = QRectF(QPointF(x, center.y() - d.size.height() / 2.0), d.size);
        it.assetKey = m_doc.addAsset(d.bytes);   // original bytes kept untouched
        it.name = d.name;
        it.strokeWidth = 0;
        it.fill = Qt::transparent;
        added << addItem(it);
        x += d.size.width() + kGap;
    }
    setSelection(added);
    markChanged();
    // A long row may run off screen: zoom out so the whole row is visible.
    if (!boundingRect().contains(toScreen(m_doc.boundsOf(added))))
        zoomToFit();
    if (skipped > 0)
        emit message(QStringLiteral("Added %1 images; skipped %2 unsupported files.").arg(added.size()).arg(skipped));
    else if (added.size() > 1)
        emit message(QStringLiteral("Arranged %1 images in a row.").arg(added.size()));
    return int(added.size());
}

// ---------------------------------------------------------------------------
// Browser (WebAssembly) file access

void BoardCanvas::browserOpenBoard()
{
#ifdef Q_OS_WASM
    QPointer<BoardCanvas> self(this);
    auto opened = [self](const QByteArray &data, const QString &name, bool linked) {
        if (!self || !self->openData(data, name))
            return;
        self->m_filePath.clear();
        emit self->fileChanged();
        if (linked)
            WebStorage::linkOpened(self->boardUid(), self->m_doc.title);   // autosave writes back to it
    };
    if (WebStorage::canPickFile()) {
        WebStorage::openFile([opened](bool, const QString &name, const QByteArray &data) {
            if (!data.isEmpty())
                opened(data, name, true);
        });
        return;
    }
    struct State { QByteArray data; QString name; };
    auto state = std::make_shared<State>();
    QWasmLocalFileAccess::openFile(
        "Board files (*.board)",
        [](bool) {},
        [state](uint64_t size, const std::string &name) -> char * {
            if (size > (1ULL << 31))
                return nullptr;
            state->name = QString::fromStdString(name);
            state->data.resize(qsizetype(size));
            return state->data.data();
        },
        [state, opened]() { opened(state->data, state->name, false); });
#else
    emit message(QStringLiteral("Browser file access is only available in the web version."));
#endif
}

void BoardCanvas::browserInsertImages()
{
#ifdef Q_OS_WASM
    // Collect every chosen file, then lay them out together in one row.
    struct State { QList<QByteArray> files; QStringList names; QByteArray current; QString currentName; int expected = 0; };
    auto state = std::make_shared<State>();
    QPointer<BoardCanvas> self(this);
    auto finishIfDone = [state, self]() {
        if (self && state->expected > 0 && state->files.size() == state->expected) {
            self->placeImagesInRow(state->files, self->toWorld(self->boundingRect().center()), state->names);
            state->expected = 0;
        }
    };
    QWasmLocalFileAccess::openFiles(
        "Images (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.tif *.tiff *.svg)",
        QWasmLocalFileAccess::FileSelectMode::MultipleFiles,
        [state](int fileCount) { state->expected = fileCount; },
        [state](uint64_t size, const std::string &name) -> char * {
            if (size > (1ULL << 30)) {
                --state->expected;     // too large: skipped, never delivered
                return nullptr;
            }
            state->currentName = QString::fromStdString(name);
            state->current.resize(qsizetype(size));
            return state->current.data();
        },
        [state, finishIfDone]() {
            state->files << state->current;
            state->names << state->currentName;
            finishIfDone();
        });
#else
    emit message(QStringLiteral("Browser file access is only available in the web version."));
#endif
}

void BoardCanvas::browserSaveBoard(const QString &password, bool asNew)
{
#ifdef Q_OS_WASM
    QString name = m_doc.title.trimmed();
    name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|]+)")), QStringLiteral("-"));
    if (name.isEmpty())
        name = QStringLiteral("board");
    const QByteArray data = saveData(password);
    if (data.isEmpty())
        return;
    if (m_doc.isPrivate)
        m_password = password;   // in memory only, so autosave stays encrypted
    const int serial = m_changeSerial;
    const QString uid = boardUid();
    QPointer<BoardCanvas> self(this);
    auto markSaved = [self, serial]() {
        if (self && self->m_changeSerial == serial) {
            self->m_modified = false;
            emit self->fileChanged();
        }
    };
    // Chrome / Edge: link the board to a real file; autosave then keeps writing it.
    WebStorage::saveToFile(uid, m_doc.title, name + QStringLiteral(".board"), data, asNew,
                           [self, data, name, uid, markSaved](bool supported, const QString &fileName) {
        if (!self)
            return;
        if (supported) {
            if (fileName.isEmpty())
                return;   // cancelled
            markSaved();
            self->noteSavedRecent(QStringLiteral("browser:") + uid, false);
            emit self->message(QStringLiteral("Saved %1 — changes now save automatically").arg(fileName));
            // A new board takes the file's name (autosave then writes it into the file too).
            if (self->m_doc.title == Document().title)
                self->setBoardTitle(QFileInfo(fileName).completeBaseName());
            return;
        }
        // Other browsers: download a copy; the live board is kept in this browser.
        QWasmLocalFileAccess::saveFile(data, (name + QStringLiteral(".board")).toStdString());
        WebStorage::save(uid, self->m_doc.title, data, [self, markSaved, uid](const QString &status) {
            if (!self || status.isEmpty())
                return;
            markSaved();
            self->noteSavedRecent(QStringLiteral("browser:") + uid, false);
            emit self->message(QStringLiteral("Downloaded a copy. Changes are kept in this browser automatically (Recent boards)."));
        });
    });
#else
    Q_UNUSED(password)
    Q_UNUSED(asNew)
    emit message(QStringLiteral("Browser file access is only available in the web version."));
#endif
}

void BoardCanvas::openBrowserBoard(const QString &uid)
{
#ifdef Q_OS_WASM
    QPointer<BoardCanvas> self(this);
    WebStorage::load(uid, [self](const QByteArray &data) {
        if (!self)
            return;
        if (data.isEmpty()) {
            emit self->message(QStringLiteral("This board is no longer stored in this browser."));
            return;
        }
        if (self->openData(data, QStringLiteral("board"))) {
            self->m_filePath.clear();
            emit self->fileChanged();
        }
    });
#else
    Q_UNUSED(uid)
#endif
}

void BoardCanvas::removeBrowserBoard(const QString &uid)
{
#ifdef Q_OS_WASM
    WebStorage::remove(uid);
#else
    Q_UNUSED(uid)
#endif
}
