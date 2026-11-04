#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/Result.h"

namespace cutee::csvutil {

/// RFC 4180 quoting: fields containing separators, quotes or newlines are quoted.
QString escapeField(const QString &value);

/// Rows joined with CRLF; every row is terminated.
QString toCsv(const QVector<QStringList> &rows);

/// Writes UTF-8 with a BOM so Excel opens Chinese columns correctly.
Result<void> writeFile(const QString &path, const QString &content);

} // namespace cutee::csvutil
