#pragma once

#include "Document.h"

#include <QByteArray>
#include <QString>

// Native `.board` format: a ZIP container holding
//   board.json      - items, title, asset index
//   versions.json   - saved version history
//   assets/<sha256>.<ext> - original image files, byte-for-byte
namespace BoardFile {

// A non-empty password encrypts the whole file (private boards).
QByteArray save(const Document &doc, bool includeHistory = true, const QString &password = {});
bool load(const QByteArray &data, Document &doc, QString *error = nullptr, const QString &password = {});
bool isEncrypted(const QByteArray &data);

bool saveToFile(const Document &doc, const QString &path, QString *error = nullptr, const QString &password = {});
bool loadFromFile(const QString &path, Document &doc, QString *error = nullptr, const QString &password = {});

} // namespace BoardFile
