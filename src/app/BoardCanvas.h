#pragma once

#include "Document.h"
#include "UndoStack.h"

#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <QVariantList>
#include <QQuickPaintedItem>
#include <QSet>
#include <QLineF>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <array>

// Renders a fitted preview of a board (home screen thumbnails, template cards).
QImage renderPreview(const Document &doc, const QSize &size, const QColor &background);

class BoardCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(double zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY viewChanged)
    Q_PROPERTY(QString gridStyle READ gridStyle WRITE setGridStyle NOTIFY settingsChanged)   // "dots" | "lines"
    // True while an object is being dragged, resized or connected (toolbars hide).
    Q_PROPERTY(bool interacting READ interacting NOTIFY interactingChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY contentChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY fileChanged)
    Q_PROPERTY(QString boardTitle READ boardTitle WRITE setBoardTitle NOTIFY fileChanged)
    Q_PROPERTY(QStringList versionLabels READ versionLabels NOTIFY versionsChanged)

    // Private boards: saved encrypted, PDF exports password-protected.
    Q_PROPERTY(bool boardPrivate READ boardPrivate NOTIFY fileChanged)
    Q_PROPERTY(QString boardOwner READ boardOwner NOTIFY fileChanged)
    Q_PROPERTY(QString ownerEmail MEMBER m_ownerEmail NOTIFY defaultsChanged)   // signed-in profile
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY viewChanged)

    // Screen rectangle of the selection, for toolbars that float above it.
    Q_PROPERTY(QRectF selectionScreenRect READ selectionScreenRect NOTIFY selectionGeometryChanged)
    Q_PROPERTY(QString selName READ selName NOTIFY selectionChanged)
    Q_PROPERTY(bool selLocked READ selLocked WRITE setSelLocked NOTIFY selectionChanged)
    Q_PROPERTY(QString selAltText READ selAltText WRITE setSelAltText NOTIFY selectionChanged)
    Q_PROPERTY(bool selCropped READ selCropped NOTIFY selectionChanged)
    Q_PROPERTY(bool cropping READ cropping NOTIFY croppingChanged)
    Q_PROPERTY(bool canPasteStyle READ canPasteStyle NOTIFY styleClipboardChanged)

    // Live collaboration: other people's cursors / selections ([{name,color,x,y,sel}]).
    Q_PROPERTY(QVariantList remoteCursors READ remoteCursors WRITE setRemoteCursors NOTIFY remoteCursorsChanged)

    // Style of the current selection (first selected item); setters apply to all selected.
    Q_PROPERTY(QString selType READ selType NOTIFY selectionChanged)
    Q_PROPERTY(QColor selFill READ selFill WRITE setSelFill NOTIFY selectionChanged)
    Q_PROPERTY(QColor selStroke READ selStroke WRITE setSelStroke NOTIFY selectionChanged)
    Q_PROPERTY(QColor selTextColor READ selTextColor WRITE setSelTextColor NOTIFY selectionChanged)
    Q_PROPERTY(double selStrokeWidth READ selStrokeWidth WRITE setSelStrokeWidth NOTIFY selectionChanged)
    Q_PROPERTY(double selFontSize READ selFontSize WRITE setSelFontSize NOTIFY selectionChanged)
    Q_PROPERTY(bool selBold READ selBold WRITE setSelBold NOTIFY selectionChanged)
    Q_PROPERTY(int selTextAlign READ selTextAlign WRITE setSelTextAlign NOTIFY selectionChanged)
    Q_PROPERTY(bool selElbow READ selElbow WRITE setSelElbow NOTIFY selectionChanged)
    Q_PROPERTY(int selLineType READ selLineType WRITE setSelLineType NOTIFY selectionChanged)   // 0 straight, 1 elbow, 2 curved
    Q_PROPERTY(int selDash READ selDash WRITE setSelDash NOTIFY selectionChanged)               // 0 solid, 1 dashed, 2 dotted
    Q_PROPERTY(bool selArrowStart READ selArrowStart WRITE setSelArrowStart NOTIFY selectionChanged)
    Q_PROPERTY(bool selArrowEnd READ selArrowEnd WRITE setSelArrowEnd NOTIFY selectionChanged)

    // Defaults for newly drawn items
    Q_PROPERTY(QColor stickyColor MEMBER m_stickyColor NOTIFY defaultsChanged)
    Q_PROPERTY(QColor penColor MEMBER m_penColor NOTIFY defaultsChanged)
    Q_PROPERTY(double penWidth MEMBER m_penWidth NOTIFY defaultsChanged)
    Q_PROPERTY(QColor highlighterColor MEMBER m_highlighterColor NOTIFY defaultsChanged)
    Q_PROPERTY(QColor shapeFill MEMBER m_shapeFill NOTIFY defaultsChanged)        // "Apply colors" in the shape library
    Q_PROPERTY(bool stickyStack MEMBER m_stickyStack NOTIFY defaultsChanged)     // keep placing notes
    Q_PROPERTY(QSizeF framePreset MEMBER m_framePreset NOTIFY defaultsChanged)   // size for a click with Frame

    // Layers ([{id,name,visible,locked,count}], default layer first)
    Q_PROPERTY(QVariantList layers READ layers NOTIFY layersChanged)
    Q_PROPERTY(QString activeLayer READ activeLayer WRITE setActiveLayer NOTIFY layersChanged)

    // Board settings from the right-click menu (kept between sessions)
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY settingsChanged)
    Q_PROPERTY(bool snapObjects READ snapObjects WRITE setSnapObjects NOTIFY settingsChanged)
    Q_PROPERTY(bool showObjectSize READ showObjectSize WRITE setShowObjectSize NOTIFY settingsChanged)
    Q_PROPERTY(bool suggestSize READ suggestSize WRITE setSuggestSize NOTIFY settingsChanged)
    Q_PROPERTY(bool showComments READ showComments WRITE setShowComments NOTIFY settingsChanged)
    Q_PROPERTY(QString navMode READ navMode WRITE setNavMode NOTIFY settingsChanged)   // auto / mouse / trackpad
    Q_PROPERTY(bool hasStartView READ hasStartView NOTIFY fileChanged)
    Q_PROPERTY(QString cursorChat READ cursorChat WRITE setCursorChat NOTIFY cursorChatChanged)
    Q_PROPERTY(QString selLink READ selLink WRITE setSelLink NOTIFY selectionChanged)
    Q_PROPERTY(bool selGrouped READ selGrouped NOTIFY selectionChanged)
    Q_PROPERTY(QString selLayer READ selLayer NOTIFY selectionChanged)

    // In-place text editor overlay (positioned by QML)
    Q_PROPERTY(bool editing READ editing NOTIFY editingChanged)
    Q_PROPERTY(QRectF editRect READ editRect NOTIFY editingChanged)
    Q_PROPERTY(QString editText READ editText NOTIFY editingChanged)
    Q_PROPERTY(double editFontPixelSize READ editFontPixelSize NOTIFY editingChanged)
    Q_PROPERTY(QColor editColor READ editColor NOTIFY editingChanged)
    Q_PROPERTY(int editAlign READ editAlign NOTIFY editingChanged)
    Q_PROPERTY(bool editBold READ editBold NOTIFY editingChanged)
    Q_PROPERTY(bool editCenterV READ editCenterV NOTIFY editingChanged)

public:
    explicit BoardCanvas(QQuickItem *parent = nullptr);

    void paint(QPainter *painter) override;

    const Document &document() const { return m_doc; }
    QStringList selectedIds() const { return m_selection; }

    QString tool() const { return m_tool; }
    void setTool(const QString &tool);
    double zoom() const { return m_zoom; }
    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool on);
    int selectionCount() const { return int(m_selection.size()); }
    int itemCount() const { return int(m_doc.items.size()); }
    bool canUndo() const { return m_undo.canUndo(); }
    bool canRedo() const { return m_undo.canRedo(); }
    QString filePath() const { return m_filePath; }
    bool modified() const { return m_modified; }
    QString boardTitle() const { return m_doc.title; }
    void setBoardTitle(const QString &t);
    QStringList versionLabels() const;

    bool boardPrivate() const { return m_doc.isPrivate; }
    QString boardOwner() const { return m_doc.owner; }
    QColor gridColor() const { return m_gridColor; }
    void setGridColor(const QColor &c);
    QRectF selectionScreenRect() const;
    QString selName() const;
    bool selLocked() const;
    void setSelLocked(bool locked);
    QString selAltText() const;
    void setSelAltText(const QString &alt);
    bool selCropped() const;
    bool cropping() const { return !m_cropId.isEmpty(); }
    bool canPasteStyle() const { return m_hasStyleClipboard; }

    // Live collaboration
    Document &mutableDocument() { return m_doc; }
    void remoteApplied();                       // after SyncEngine merged remote ops into the document
    QVariantList remoteCursors() const { return m_remoteCursors; }

    QVariantList layers() const;
    QString activeLayer() const { return m_activeLayer; }
    void setActiveLayer(const QString &id);
    void setRemoteCursors(const QVariantList &cursors);

    QString selType() const;
    QColor selFill() const;
    void setSelFill(const QColor &c);
    QColor selStroke() const;
    void setSelStroke(const QColor &c);
    QColor selTextColor() const;
    void setSelTextColor(const QColor &c);
    double selStrokeWidth() const;
    void setSelStrokeWidth(double w);
    double selFontSize() const;
    void setSelFontSize(double s);
    bool selBold() const;
    void setSelBold(bool b);
    int selTextAlign() const;
    void setSelTextAlign(int a);
    bool selElbow() const;
    void setSelElbow(bool e);
    int selLineType() const;
    void setSelLineType(int t);
    int selDash() const;
    void setSelDash(int d);
    bool selArrowStart() const;
    void setSelArrowStart(bool on);
    bool selArrowEnd() const;
    void setSelArrowEnd(bool on);
    Q_INVOKABLE void swapConnectorEnds();
    Q_INVOKABLE void editSelectionText();
    QString gridStyle() const { return m_gridStyle; }
    void setGridStyle(const QString &s);
    bool interacting() const { return m_interacting; }

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool on);
    bool snapObjects() const { return m_snapObjects; }
    void setSnapObjects(bool on);
    bool showObjectSize() const { return m_showObjectSize; }
    void setShowObjectSize(bool on);
    bool suggestSize() const { return m_suggestSize; }
    void setSuggestSize(bool on);
    bool showComments() const { return m_showComments; }
    void setShowComments(bool on);
    QString navMode() const { return m_navMode; }
    void setNavMode(const QString &mode);
    bool hasStartView() const { return !m_doc.startView.isEmpty(); }
    QString cursorChat() const { return m_cursorChat; }
    void setCursorChat(const QString &text);
    QString selLink() const;
    void setSelLink(const QString &link);
    bool selGrouped() const;
    QString selLayer() const;

    bool editing() const { return !m_editingId.isEmpty(); }
    QRectF editRect() const;
    QString editText() const;
    double editFontPixelSize() const;
    QColor editColor() const;
    int editAlign() const;
    bool editBold() const;
    bool editCenterV() const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void duplicateSelection();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void paste();
    // Line the selected objects up evenly: a row (\) or a column (Shift+\).
    Q_INVOKABLE void arrangeSelection(bool vertical);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void bringToFront();
    Q_INVOKABLE void sendToBack();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();
    Q_INVOKABLE void resetZoom();

    Q_INVOKABLE void newBoard();
    Q_INVOKABLE bool openFile(const QUrl &url);
    Q_INVOKABLE bool saveFile(const QUrl &url = QUrl(), const QString &password = QString());
    Q_INVOKABLE bool openData(const QByteArray &data, const QString &name, const QString &password = QString());
    Q_INVOKABLE QByteArray saveData(const QString &password = QString());
    // Writes pending changes to the open file now (closing, app sent to background).
    // True when nothing is left unsaved.
    Q_INVOKABLE bool flushAutosave();
    // Stable id of this board (created on first use; saved in the file).
    Q_INVOKABLE QString boardUid() { if (m_doc.uid.isEmpty()) m_doc.uid = BoardItem::newId(); return m_doc.uid; }

    // Private boards
    Q_INVOKABLE bool makePrivate(const QString &password);
    Q_INVOKABLE bool makePublic(const QString &password);
    Q_INVOKABLE bool checkPassword(const QString &password) const;
    Q_INVOKABLE bool unlockPending(const QString &password);   // after passwordRequired()
    Q_INVOKABLE void cancelPending();
    Q_INVOKABLE bool addImageFromUrl(const QUrl &url, double screenX = -1, double screenY = -1);
    Q_INVOKABLE bool addImageData(const QByteArray &bytes, double screenX = -1, double screenY = -1);
    // Several images are auto-arranged in a horizontal row centred on the point.
    Q_INVOKABLE int addImagesFromUrls(const QList<QUrl> &urls, double screenX = -1, double screenY = -1);
    Q_INVOKABLE void restoreVersion(int index);

    // Web build: the browser's own open / download dialogs (no file system access).
    Q_INVOKABLE void browserOpenBoard();
    Q_INVOKABLE void browserInsertImages();
    Q_INVOKABLE void browserSaveBoard(const QString &password = QString());

    // Right-click menu
    Q_INVOKABLE void copyAsPng();
    Q_INVOKABLE void copyAsSvg();
    Q_INVOKABLE void copyStyle();
    Q_INVOKABLE void pasteStyle();
    Q_INVOKABLE void bringForward();
    Q_INVOKABLE void sendBackward();
    Q_INVOKABLE void renameSelection(const QString &name);
    Q_INVOKABLE QString selectionInfo() const;

    // Images
    Q_INVOKABLE void flipSelection(bool horizontal);
    Q_INVOKABLE void startCrop();
    Q_INVOKABLE void finishCrop();
    Q_INVOKABLE void resetCrop();
    Q_INVOKABLE void resetImageSize();
    Q_INVOKABLE QString originalImageName() const;
    Q_INVOKABLE bool saveOriginalImage(const QUrl &url);
    Q_INVOKABLE void browserDownloadOriginal();

    // Templates and home screen
    Q_INVOKABLE QVariantList templates() const;
    Q_INVOKABLE void newFromTemplate(const QString &id);
    Q_INVOKABLE QString saveThumbnail() const;   // PNG preview of the board for "Recent boards"

    // Libraries (shape / icon / emoji panels)
    Q_INVOKABLE QVariantList shapeLibrary() const;   // [{id,name,category,keywords}]
    Q_INVOKABLE QVariantList iconLibrary() const;
    Q_INVOKABLE QVariantList emojiLibrary() const;   // [{category, emoji:[…]}]
    Q_INVOKABLE QString emojiFamily() const;         // system colour emoji, else bundled; "" = none
    Q_INVOKABLE void addStickies(const QStringList &lines);   // bulk mode: one note per line

    // Layers
    Q_INVOKABLE QString addLayer(const QString &name);
    Q_INVOKABLE void renameLayer(const QString &id, const QString &name);
    Q_INVOKABLE void deleteLayer(const QString &id);
    Q_INVOKABLE void setLayerVisible(const QString &id, bool visible);
    Q_INVOKABLE void setLayerLocked(const QString &id, bool locked);
    Q_INVOKABLE void moveLayer(const QString &id, int delta);
    Q_INVOKABLE void moveSelectionToLayer(const QString &id);

    // Miro-style menu actions (BoardCanvasMenus.cpp)
    Q_INVOKABLE void unlockAll();
    Q_INVOKABLE void addTextAt(double screenX, double screenY);
    Q_INVOKABLE void addStickyAt(double screenX, double screenY);
    // Test hook for screenshot runs: press at `from` and drag to `to` (no release).
    Q_INVOKABLE void simulateDrag(const QPointF &from, const QPointF &to)
    {
        pointerPress(from, Qt::LeftButton, {}, false);
        pointerMove(to, {});
    }
    Q_INVOKABLE void simulateRelease(const QPointF &at) { pointerRelease(at, {}); }
    Q_INVOKABLE QPointF connectDotScreen(int side) const
    {
        const QList<QPointF> d = connectDots();
        return side >= 0 && side < d.size() ? d[side] : QPointF();
    }
    Q_INVOKABLE void selectConnectorDemo()
    {
        for (const BoardItem &it : std::as_const(m_doc.items))
            if (it.isConnector()) { setSelection({ it.id }); return; }
    }
    Q_INVOKABLE void addCommentAt(double screenX, double screenY);     // -1,-1 = next to the selection
    Q_INVOKABLE void setCommentText(const QString &id, const QString &text);
    Q_INVOKABLE QVariantMap commentInfo(const QString &id) const;
    Q_INVOKABLE void setStartViewToCurrent();
    Q_INVOKABLE void setStartViewToFrame(const QString &frameId);
    Q_INVOKABLE void clearStartView();
    Q_INVOKABLE void goToStartView();
    Q_INVOKABLE QVariantList frames() const;                  // [{id, name}]
    Q_INVOKABLE void zoomToContent();                         // Alt+1 "Show all"
    Q_INVOKABLE void zoomToSelection();                       // Alt+2
    Q_INVOKABLE void zoomToItem(const QString &id);
    Q_INVOKABLE void createFrameAroundSelection();
    Q_INVOKABLE void copyLink();
    Q_INVOKABLE void followLink(const QString &link);
    Q_INVOKABLE void groupSelection();
    Q_INVOKABLE void ungroupSelection();
    Q_INVOKABLE void clearContents();                         // Ctrl+Backspace
    Q_INVOKABLE void selectNext(bool backwards);              // Tab / Shift+Tab
    Q_INVOKABLE void duplicateInDirection(double dx, double dy);
    Q_INVOKABLE QVariantMap selectionMeta() const;            // Info card
    Q_INVOKABLE bool saveSelectionAsTemplate(const QString &name);
    Q_INVOKABLE bool deleteCustomTemplate(const QString &id);
    Q_INVOKABLE int addSelectionToSaved();                    // "Add to Saved files"
    Q_INVOKABLE QVariantList savedFiles() const;              // [{url, name}]
    Q_INVOKABLE bool insertSavedFile(const QUrl &url);
    Q_INVOKABLE bool removeSavedFile(const QUrl &url);
    Q_INVOKABLE QPointF menuScreenPos() const { return m_menuScreen; }
    static Document loadCustomTemplate(const QString &id);    // "custom:<file>"

    Q_INVOKABLE void beginTextEditOnSelection();
    Q_INVOKABLE void updateEditText(const QString &text);
    Q_INVOKABLE void endTextEdit();

signals:
    void toolChanged();
    void viewChanged();
    void selectionChanged();
    void contentChanged();
    void historyChanged();
    void fileChanged();
    void versionsChanged();
    void defaultsChanged();
    void editingChanged();
    void message(const QString &text);
    void selectionGeometryChanged();
    void interactingChanged();
    void croppingChanged();
    void styleClipboardChanged();
    void contextMenuRequested(double x, double y);
    void passwordRequired(const QString &name);   // an encrypted board is waiting for unlockPending()
    void localEdit();                             // the user changed the board (not a remote merge)
    void pointerWorldMoved(double x, double y);   // cursor position in board coordinates
    void remoteCursorsChanged();
    void boardSaved(const QString &path, const QString &title);
    void autosaved(const QString &path);   // quiet save to the open file after an edit
    void layersChanged();
    void settingsChanged();
    void cursorChatChanged();
    void linkActivated(const QString &url);                // external link clicked on an object
    void commentOpened(const QString &id, double x, double y);
    void toolSlotRequested(int index);                     // 1..9: Nth tool of the toolbar
    void shortcutsRequested();                             // F1
    void shapesRequested();                                // S

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void hoverMoveEvent(QHoverEvent *e) override;
    void hoverEnterEvent(QHoverEvent *e) override { m_pointerInside = true; m_hoverScreen = e->position(); }
    void hoverLeaveEvent(QHoverEvent *) override { m_pointerInside = false; }
    // Where Paste puts things: the mouse pointer, else the middle of the view.
    QPointF pasteAnchor() const;
    bool m_pointerInside = false;
    bool m_pasteAtPointer = true;
    QPointF m_lastPasteAnchor;
    int m_pasteRepeat = 0;
    void wheelEvent(QWheelEvent *e) override;
    void touchEvent(QTouchEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    enum class Drag { None, Pan, Move, Resize, Marquee, Create, Draw, Connector, ConnectorEnd, Crop, Erase, Lasso };

    QPointF toWorld(const QPointF &screen) const { return (screen - m_pan) / m_zoom; }
    QPointF toScreen(const QPointF &world) const { return world * m_zoom + m_pan; }
    QRectF toScreen(const QRectF &world) const;
    void setZoomAt(double zoom, const QPointF &screenAnchor);

    void pushUndo();                               // snapshot current items before a change
    void markChanged();
    void setSelection(const QStringList &ids);
    QList<BoardItem *> selectedItems();
    QList<const BoardItem *> selectedItems() const;
    template <typename F> void applyToSelection(F &&fn);

    BoardItem makeItem(BoardItem::Type type, const QPointF &worldPos) const;
    QString addItem(BoardItem item, bool atBack = false);
    int placeImagesInRow(const QList<QByteArray> &files, const QPointF &worldCenter, const QStringList &names = {});
    void beginTextEdit(const QString &id);
    void autoSizeText(BoardItem &item);

    QRectF selectionRect() const;                  // world rect of resizable selection
    int handleAt(const QPointF &screen) const;     // -1 if none; 0..7 resize handles
    int connectorEndAt(const QPointF &screen) const; // 0 start, 1 end, -1 none
    QString connectableAt(const QPointF &world, const QString &exclude) const;
    void updateHoverCursor(const QPointF &screen);

    // Shared by mouse and single-finger touch input.
    void pointerPress(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers mods, bool touch);
    void pointerMove(const QPointF &pos, Qt::KeyboardModifiers mods);
    void pointerRelease(const QPointF &pos, Qt::KeyboardModifiers mods);
    void pointerDoubleClick(const QPointF &pos);
    void finishCreate();

    Document m_doc;
    UndoStack m_undo;
    QStringList m_selection;
    QList<BoardItem> m_clipboard;
    int m_pasteCount = 0;

    QString m_tool = QStringLiteral("select");
    double m_zoom = 1.0;
    QPointF m_pan = QPointF(0, 0);
    bool m_showGrid = true;
    QString m_gridStyle = QStringLiteral("dots");
    bool m_interacting = false;
    void updateInteracting();
    // Blue "connect" dots around a single selected object (Miro quick-connect):
    // 0 top, 1 right, 2 bottom, 3 left; -1 none.
    int connectDotAt(const QPointF &screen) const;
    QList<QPointF> connectDots() const;
    int m_quickDir = -1;
    void quickConnectCreate(const QString &connectorId);
    bool m_spaceDown = false;

    QString m_filePath;
    bool m_modified = false;

    // Drag state
    Drag m_drag = Drag::None;
    QPointF m_pressScreen;
    QPointF m_pressWorld;
    QPointF m_lastScreen;
    QList<BoardItem> m_dragSnapshot;
    QHash<QString, BoardItem> m_dragOrigin;       // original state of moving/resizing items
    QRectF m_dragOriginBounds;
    int m_handle = -1;
    int m_connectorEnd = -1;
    QString m_activeId;                           // item being created / drawn
    QRectF m_marquee;
    QString m_hoverTarget;                        // connector attach highlight
    bool m_dragChanged = false;

    // Pinch
    bool m_pinching = false;
    double m_pinchStartDist = 0;
    double m_pinchStartZoom = 1;
    QPointF m_pinchLastCenter;
    QElapsedTimer m_lastTap;
    QPointF m_lastTapPos;

    // Text editing
    QString m_editingId;
    QString m_editOriginalText;
    QList<BoardItem> m_editSnapshot;
    bool m_editIsNew = false;

    QColor m_stickyColor = QColor(255, 249, 177);
    QColor m_penColor = QColor(26, 26, 26);
    double m_penWidth = 3.0;
    int m_frameCounter = 0;

    // Private boards
    QString m_password;                 // in memory only, for the open private board
    QString m_ownerEmail;
    QByteArray m_pendingData;           // encrypted board waiting for its password
    QString m_pendingName;
    QString m_pendingPath;

    QColor m_gridColor = QColor(200, 204, 212);
    QVariantList m_remoteCursors;

    // Tools
    QColor m_highlighterColor = QColor(250, 199, 16, 110);
    QColor m_shapeFill = QColor(255, 255, 255);
    bool m_stickyStack = false;
    QSizeF m_framePreset = QSizeF(960, 540);
    QString m_activeLayer;
    QPolygonF m_lasso;                  // screen coordinates while lassoing
    QPointF m_hoverScreen;
    bool m_eraseChanged = false;
    void eraseAt(const QPointF &world);
    void smartReplace(const QString &penId);

    // Context menu (right click without drag / long press)
    bool m_rightPress = false;
    QTimer m_longPress;

    // Screen-sized image previews, decoded off the UI thread (see ensurePreviews).
    QHash<QString, QImage> m_previews;
    QSet<QString> m_previewPending;
    void ensurePreviews();

    // Smooth view motion: eased wheel zoom and a gliding pan after the hand drag.
    QTimer m_viewAnim;
    // Autosave: once the board has a file, every edit is written back to it shortly after.
    QTimer m_autosave;
    QDateTime m_lastVersionAt;          // autosaves add a version at most every 10 minutes
    bool m_autosaving = false;
    void scheduleAutosave();
    void autosaveNow();
    bool writeBoardFile(const QString &path, bool plainPath, const QString &password, bool quiet);
    double m_zoomGoal = 0;            // 0 = no zoom animation
    QPointF m_zoomAnchor;
    QPointF m_glide;                  // pan velocity, px per ms
    QElapsedTimer m_animClock;
    QElapsedTimer m_panClock;
    QList<QPair<qint64, QPointF>> m_panSamples;
    void smoothZoomBy(double factor, const QPointF &anchor);
    void trackPanVelocity(const QPointF &pos);
    void startGlide();
    void stopViewAnimation();
    void viewAnimTick();
    QPointF m_longPressPos;

    // Copy / paste style
    BoardItem m_styleClipboard;
    bool m_hasStyleClipboard = false;

    // Crop mode
    QString m_cropId;
    QRectF m_cropFull;                  // world rect of the whole (uncropped) image
    QList<BoardItem> m_cropSnapshot;

    void openContextMenuAt(const QPointF &pos);
    QPointF m_menuScreen;               // where the context menu was opened

    // Settings
    bool m_snapToGrid = false;
    bool m_snapObjects = true;
    bool m_showObjectSize = false;
    bool m_suggestSize = true;
    bool m_showComments = true;
    QString m_navMode = QStringLiteral("auto");
    QString m_cursorChat;
    void loadSettings();
    void saveSetting(const char *key, const QVariant &value);

    // Snapping guides (world coordinates) while moving / resizing
    QList<QLineF> m_guides;
    QPointF snapMove(const QRectF &moving, const QPointF &delta);
    QRectF snapResize(const QRectF &r, int handle);
    QSet<QString> m_snapExclude;
    // Nearest object to the left, top, right and bottom of r that overlaps it on
    // the other axis (null rect when none) — used for gap measurements.
    std::array<QRectF, 4> gapNeighbours(const QRectF &r) const;
    void paintGapMeasurements(QPainter *p, const QRectF &r);

    // Info: created / modified stamps
    QHash<QString, size_t> m_sigs;
    void stampChanges();
    void refreshSignatures();

    // Comments, links, groups
    QString commentAt(const QPointF &screen) const;
    QString linkBadgeAt(const QPointF &screen) const;
    QRectF linkBadgeRect(const BoardItem &it) const;
    QStringList expandGroups(const QStringList &ids) const;
    void paintMiroOverlays(QPainter *p);
    void fitWorldRect(const QRectF &r, double maxZoom);
    void cropDrag(const QPointF &world);
    bool anySelectedLocked() const;
};

// Used from several source files.
template <typename F>
void BoardCanvas::applyToSelection(F &&fn)
{
    if (m_selection.isEmpty())
        return;
    pushUndo();
    for (BoardItem *it : selectedItems())
        fn(*it);
    markChanged();
    emit selectionChanged();
}
