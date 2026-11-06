#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "BoardCanvas.h"

// Runs exports off the UI thread (where threads exist) so large high-DPI
// renders never freeze the board.
class ExportController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(BoardCanvas *canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList formats READ formats CONSTANT)
    // Web and Android can't write into arbitrary folders: export becomes one
    // download / one "save as" file (a ZIP when there are several outputs).
    Q_PROPERTY(bool singleFileExport READ singleFileExport CONSTANT)

public:
    explicit ExportController(QObject *parent = nullptr);

    BoardCanvas *canvas() const { return m_canvas; }
    void setCanvas(BoardCanvas *c);
    bool busy() const { return m_busy; }
    QVariantList formats() const;
    bool singleFileExport() const;

    // options: formats (list of names), compression ("low"|"medium"|"high"|"custom"),
    // scope ("board"|"selection"|"frames"), folder (url), baseName, transparent,
    // zip, customDpi, customQuality, custom16Bit
    Q_INVOKABLE void start(const QVariantMap &options);
    Q_INVOKABLE void startToUrl(const QVariantMap &options, const QUrl &target);
    Q_INVOKABLE QString packageName(const QVariantMap &options) const;
    Q_INVOKABLE QString toLocalPath(const QUrl &url) const { return url.isLocalFile() ? url.toLocalFile() : url.toString(); }
    Q_INVOKABLE QString describe(const QString &compression, int customDpi, int customQuality, bool custom16Bit) const;

signals:
    void canvasChanged();
    void busyChanged();
    void finished(bool ok, const QString &message, const QStringList &files, const QUrl &folder);

private:
    enum class Mode { Folder, Download, Url };
    void run(const QVariantMap &options, Mode mode, const QUrl &target);

    QPointer<BoardCanvas> m_canvas;
    bool m_busy = false;
};
