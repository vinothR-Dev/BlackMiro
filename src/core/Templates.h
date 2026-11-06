#pragma once

#include "Document.h"

#include <QList>
#include <QString>

// Built-in board templates (all free). Each builds a ready-to-edit Document.
namespace Templates {

struct Info
{
    QString id;
    QString name;
    QString description;
    QString category;
};

QList<Info> list();
Document create(const QString &id);   // empty Document for unknown ids

} // namespace Templates
