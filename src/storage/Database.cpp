#include "storage/Database.h"

#include "core/Logging.h"
#include "storage/DbUtils.h"
#include "storage/Migrations.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cutee {

Database::Database(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

Database::~Database()
{
    close();
}

Result<void> Database::open(const QString &filePath)
{
    close();

    const QString directory = QFileInfo(filePath).absolutePath();
    if (!QDir().mkpath(directory))
        return Error(QStringLiteral("无法创建数据目录：%1").arg(directory), QStringLiteral("Database::open"));

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(filePath);
    if (!db.open()) {
        const QString message = db.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        return Error(message, QStringLiteral("Database::open"));
    }

    m_filePath = filePath;

    // Driver level settings must be applied on every connection.
    const QStringList pragmas = {
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA busy_timeout = 5000"),
    };
    for (const QString &pragma : pragmas) {
        QSqlQuery query(db);
        if (!query.exec(pragma)) {
            const QString message = query.lastError().text();
            close();
            return Error(message, QStringLiteral("Database::open (%1)").arg(pragma));
        }
    }

    if (m_connectionName == QStringLiteral("cutee_main"))
        qInfo().noquote() << QStringLiteral("数据库已打开：%1").arg(QDir::toNativeSeparators(filePath));
    else
        qDebug().noquote() << QStringLiteral("工作线程数据库已打开：%1")
                                  .arg(QDir::toNativeSeparators(filePath));
    return {};
}

void Database::close()
{
    if (!QSqlDatabase::contains(m_connectionName))
        return;

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_filePath.clear();
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
        && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

Result<void> Database::migrate()
{
    if (!isOpen())
        return Error(QStringLiteral("数据库未打开"), QStringLiteral("Database::migrate"));

    QSqlDatabase db = connection();

    auto created = db::execute(db, QStringLiteral(
                                       "CREATE TABLE IF NOT EXISTS schema_version ("
                                       "  version INTEGER NOT NULL"
                                       ")"));
    if (!created)
        return Error(created.error().toString(), QStringLiteral("Database::migrate"));

    int current = readVersion(db);
    if (current < 0)
        return Error(QStringLiteral("无法读取 schema_version"), QStringLiteral("Database::migrate"));

    for (const Migration &migration : migrations()) {
        if (migration.version <= current)
            continue;

        if (!db.transaction())
            return Error(db.lastError().text(), QStringLiteral("开启事务失败"));

        bool failed = false;
        Error failure;
        for (const QString &statement : migration.statements) {
            auto executed = db::execute(db, statement);
            if (!executed) {
                failed = true;
                failure = executed.error();
                break;
            }
        }

        if (!failed) {
            auto written = writeVersion(db, migration.version);
            if (!written) {
                failed = true;
                failure = written.error();
            }
        }

        if (failed) {
            db.rollback();
            return Error(failure.toString(),
                         QStringLiteral("数据库迁移 v%1 失败").arg(migration.version));
        }

        if (!db.commit()) {
            const QString message = db.lastError().text();
            db.rollback();
            return Error(message, QStringLiteral("提交迁移 v%1 失败").arg(migration.version));
        }

        qInfo().noquote() << QStringLiteral("数据库迁移完成 v%1 - %2")
                                 .arg(migration.version)
                                 .arg(migration.description);
        current = migration.version;
    }

    return {};
}

int Database::schemaVersion() const
{
    if (!isOpen())
        return -1;
    QSqlDatabase db = connection();
    return readVersion(db);
}

int Database::readVersion(QSqlDatabase &db) const
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1")))
        return -1;
    if (!query.next())
        return 0;
    return query.value(0).toInt();
}

Result<void> Database::writeVersion(QSqlDatabase &db, int version)
{
    auto cleared = db::execute(db, QStringLiteral("DELETE FROM schema_version"));
    if (!cleared)
        return cleared;
    return db::execute(db, QStringLiteral("INSERT INTO schema_version (version) VALUES (?)"),
                       {version});
}

} // namespace cutee
