#include "storage/DbUtils.h"

#include <QSqlError>

namespace cutee::db {

Result<void> execute(QSqlDatabase db, const QString &sql, const QVariantList &bindings)
{
    QSqlQuery query(db);
    if (!query.prepare(sql))
        return Error(query.lastError().text(), QStringLiteral("SQL prepare failed"));

    for (QVariant value : bindings) {
        // A default-constructed QString binds as SQL NULL; every text column in the
        // schema is NOT NULL DEFAULT '', so normalize it to an empty string.
        if (value.userType() == QMetaType::QString && value.toString().isNull())
            value = QString::fromLatin1("");
        query.addBindValue(value);
    }

    if (!query.exec())
        return Error(query.lastError().text(), sql.trimmed().left(120));

    return {};
}

QString lastErrorText(const QSqlQuery &query)
{
    return query.lastError().text();
}

} // namespace cutee::db
