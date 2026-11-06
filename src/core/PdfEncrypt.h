#pragma once

#include <QByteArray>
#include <QString>

// Adds password protection to a PDF produced by QPdfWriter (classic xref table).
// Uses the PDF Standard Security Handler, revision 4 with AES-128 (/AESV2), which every
// mainstream viewer (Acrobat, Chrome, Edge, Firefox, macOS Preview, Android) can open.
// The password is required to open the document; once opened, printing/copying is allowed.
namespace PdfEncrypt {

// Returns the encrypted PDF, or an empty array if the input could not be parsed.
QByteArray encrypt(const QByteArray &pdf, const QString &password, QString *error = nullptr);

} // namespace PdfEncrypt
