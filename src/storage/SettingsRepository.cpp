#include "storage/SettingsRepository.h"

#include "storage/Database.h"
#include "storage/DbUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cutee {

SettingsRepository::SettingsRepository(Database &database)
    : m_database(database)
{
}

Result<QString> SettingsRepository::value(const QString &key, const QString &defaultValue) const
{
    if (!m_database.isOpen())
        return Error(QStringLiteral("数据库未打开"), QStringLiteral("SettingsRepository::value"));

    QSqlQuery query(m_database.connection());
    if (!query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?")))
        return Error(query.lastError().text(), QStringLiteral("SettingsRepository::value"));
    query.addBindValue(key);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("SettingsRepository::value"));
    if (!query.next())
        return defaultValue;
    return query.value(0).toString();
}

Result<void> SettingsRepository::setValue(const QString &key, const QString &value)
{
    return db::execute(m_database.connection(),
                       QStringLiteral("INSERT INTO settings (key, value) VALUES (?, ?) "
                                      "ON CONFLICT(key) DO UPDATE SET value = excluded.value"),
                       {key, value});
}

Result<void> SettingsRepository::remove(const QString &key)
{
    return db::execute(m_database.connection(),
                       QStringLiteral("DELETE FROM settings WHERE key = ?"), {key});
}

} // namespace cutee
