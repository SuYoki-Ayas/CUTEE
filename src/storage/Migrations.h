#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace cutee {

struct Migration
{
    int version = 0;
    QString description;
    QStringList statements;
};

/// Ordered schema migrations. Each entry is executed inside its own transaction.
const QVector<Migration> &migrations();

int targetSchemaVersion();

} // namespace cutee
