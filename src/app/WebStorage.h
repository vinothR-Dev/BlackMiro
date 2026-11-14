#pragma once

#include <QtGlobal>   // defines Q_OS_WASM; must come before the #ifdef below

// Web version only: board storage in the browser (window.miruWeb in web/index.html).
// Every board autosaves into IndexedDB; with the File System Access API (Chrome, Edge)
// a board can also be linked to a real .board file that autosave keeps writing.
// All calls are asynchronous; callbacks run on the Qt event loop.
#ifdef Q_OS_WASM

#include <QByteArray>
#include <QString>

#include <functional>

namespace WebStorage {
bool canPickFile();
// status: "file" (written to the linked file too) | "browser" | "permission" |
//         "!reason" (linked file failed; browser copy kept) | "" (error)
void save(const QString &uid, const QString &title, const QByteArray &data, std::function<void(QString status)> done);
// File name written, "" = cancelled, "!reason" = writing failed,
// supported=false = no file access in this browser.
void saveToFile(const QString &uid, const QString &title, const QString &suggestedName, const QByteArray &data,
                bool asNew, std::function<void(bool supported, QString fileName)> done);
// Opens a .board file and remembers it (call linkOpened once the board's id is known).
// supported=false: no file access in this browser; empty data: cancelled.
void openFile(std::function<void(bool supported, QString name, QByteArray data)> done);
void linkOpened(const QString &uid, const QString &title);
void load(const QString &uid, std::function<void(QByteArray data)> done);
void remove(const QString &uid);
}

#endif
