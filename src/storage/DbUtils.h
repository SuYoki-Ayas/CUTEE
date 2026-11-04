#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariantList>

#include "core/Result.h"

namespace cutee::db {

/// Runs a single statement and converts driver failures into Error.
Result<void> execute(QSqlDatabase db, const QString &sql, const QVariantList &bindings = {});

/// Last prepared query for reuse patterns; returns an empty string on success.
QString lastErrorText(const QSqlQuery &query);

} // namespace cutee::db
