#pragma once

#include <QSqlDatabase>
#include <QString>

#include "core/Result.h"

namespace cutee {

/// Owns one named QSqlDatabase connection and applies schema migrations.
class Database
{
public:
    explicit Database(QString connectionName = QStringLiteral("cutee_main"));
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    Result<void> open(const QString &filePath);
    void close();

    bool isOpen() const;
    QSqlDatabase connection() const;
    QString filePath() const { return m_filePath; }

    /// Brings the schema up to the latest version; safe to call repeatedly.
    Result<void> migrate();
    int schemaVersion() const;

private:
    int readVersion(QSqlDatabase &db) const;
    Result<void> writeVersion(QSqlDatabase &db, int version);

    QString m_connectionName;
    QString m_filePath;
};

} // namespace cutee
