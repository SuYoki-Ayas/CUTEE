#include "storage/DataMaintenance.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "storage/Database.h"
#include "storage/DbUtils.h"
#include "storage/Migrations.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace cutee {
namespace maintenance {
namespace {

/// SQLite has no bind parameters for `VACUUM INTO`, so quotes are doubled.
QString quotedLiteral(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(escaped);
}

QString uniqueConnectionName(const QString &prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::Id128));
}

bool samePath(const QString &left, const QString &right)
{
    if (left.isEmpty() || right.isEmpty())
        return false;
    const QString a = QFileInfo(left).canonicalFilePath();
    const QString b = QFileInfo(right).canonicalFilePath();
    if (!a.isEmpty() && !b.isEmpty())
        return QString::compare(a, b, Qt::CaseInsensitive) == 0;
    return QString::compare(QDir::cleanPath(QFileInfo(left).absoluteFilePath()),
                            QDir::cleanPath(QFileInfo(right).absoluteFilePath()),
                            Qt::CaseInsensitive)
        == 0;
}

void removeJournalFiles(const QString &databasePath)
{
    QFile::remove(databasePath + QStringLiteral("-wal"));
    QFile::remove(databasePath + QStringLiteral("-shm"));
}

/// Copies `sourcePath` to `copyPath` and verifies that the copy is a CUTEE
/// database. Returns the schema version found *before* any migration, so the
/// caller can tell the user what it is importing.
///
/// A live database in WAL mode keeps recent pages in `<db>-wal`, so the journal
/// is copied along and folded into the copy before it is used.
Result<int> stageCopy(const QString &sourcePath, const QString &copyPath)
{
    const QFileInfo source(sourcePath);
    if (!source.exists() || !source.isFile())
        return Error(QStringLiteral("文件不存在：%1").arg(QDir::toNativeSeparators(sourcePath)),
                     QStringLiteral("maintenance::stageCopy"));

    QFile::remove(copyPath);
    removeJournalFiles(copyPath);
    if (!QFile::copy(source.absoluteFilePath(), copyPath))
        return Error(QStringLiteral("无法读取所选文件：%1")
                         .arg(QDir::toNativeSeparators(sourcePath)),
                     QStringLiteral("maintenance::stageCopy"));

    const QString sourceWal = source.absoluteFilePath() + QStringLiteral("-wal");
    if (QFileInfo::exists(sourceWal))
        QFile::copy(sourceWal, copyPath + QStringLiteral("-wal"));

    Database staged(uniqueConnectionName(QStringLiteral("cutee_import")));
    auto opened = staged.open(copyPath);
    if (!opened) {
        staged.close();
        removeJournalFiles(copyPath);
        QFile::remove(copyPath);
        return Error(QStringLiteral("不是有效的 SQLite 数据库：%1").arg(opened.errorMessage()),
                     QStringLiteral("maintenance::stageCopy"));
    }

    const int version = staged.schemaVersion();
    if (version < 0) {
        staged.close();
        removeJournalFiles(copyPath);
        QFile::remove(copyPath);
        return Error(QStringLiteral("所选文件不是 CUTEE 数据库（缺少 schema_version 表）"),
                     QStringLiteral("maintenance::stageCopy"));
    }

    const QStringList required = {QStringLiteral("student_profile"), QStringLiteral("subjects"),
                                  QStringLiteral("exam_records")};
    QString missing;
    for (const QString &table : required) {
        QSqlQuery query(staged.connection());
        query.prepare(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"));
        query.addBindValue(table);
        if (!query.exec() || !query.next()) {
            missing = table;
            break;
        }
    }
    if (!missing.isEmpty()) {
        staged.close();
        removeJournalFiles(copyPath);
        QFile::remove(copyPath);
        return Error(QStringLiteral("所选文件不是 CUTEE 数据库（缺少 %1 表）").arg(missing),
                     QStringLiteral("maintenance::stageCopy"));
    }

    if (version > targetSchemaVersion()) {
        staged.close();
        removeJournalFiles(copyPath);
        QFile::remove(copyPath);
        return Error(QStringLiteral("该文件来自更新版本的 CUTEE（schema v%1，本程序支持到 v%2）")
                         .arg(version)
                         .arg(targetSchemaVersion()),
                     QStringLiteral("maintenance::stageCopy"));
    }

    // Upgrade the copy, never the user's original file.
    auto migrated = staged.migrate();
    if (!migrated) {
        staged.close();
        removeJournalFiles(copyPath);
        QFile::remove(copyPath);
        return Error(QStringLiteral("无法升级导入文件的结构：%1").arg(migrated.errorMessage()),
                     QStringLiteral("maintenance::stageCopy"));
    }

    {
        QSqlQuery checkpoint(staged.connection());
        checkpoint.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)"));
    }
    staged.close();
    removeJournalFiles(copyPath);
    return version;
}

} // namespace

Result<void> exportSnapshot(Database &database, const QString &targetPath)
{
    if (!database.isOpen())
        return Error(QStringLiteral("数据库未打开"), QStringLiteral("maintenance::exportSnapshot"));
    if (targetPath.trimmed().isEmpty())
        return Error(QStringLiteral("未指定导出文件"), QStringLiteral("maintenance::exportSnapshot"));

    const QFileInfo target(targetPath);
    QDir().mkpath(target.absolutePath());
    if (target.exists() && !QFile::remove(target.absoluteFilePath()))
        return Error(QStringLiteral("无法覆盖已有文件：%1")
                         .arg(QDir::toNativeSeparators(target.absoluteFilePath())),
                     QStringLiteral("maintenance::exportSnapshot"));

    QSqlQuery query(database.connection());
    if (!query.exec(QStringLiteral("VACUUM INTO %1").arg(quotedLiteral(target.absoluteFilePath()))))
        return Error(query.lastError().text(), QStringLiteral("maintenance::exportSnapshot"));

    if (!QFileInfo::exists(target.absoluteFilePath()))
        return Error(QStringLiteral("导出文件未生成"), QStringLiteral("maintenance::exportSnapshot"));
    return {};
}

Result<QString> createBackup(Database &database, const QString &label)
{
    const QString directory = AppPaths::backupDirectory();
    if (!QDir().mkpath(directory))
        return Error(QStringLiteral("无法创建备份目录：%1").arg(QDir::toNativeSeparators(directory)),
                     QStringLiteral("maintenance::createBackup"));

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString name = QStringLiteral("cutee-%1-%2.db")
                             .arg(label.isEmpty() ? QStringLiteral("backup") : label, stamp);
    const QString path = QDir(directory).filePath(name);

    auto exported = exportSnapshot(database, path);
    if (!exported)
        return exported.error();

    qInfo().noquote() << QStringLiteral("已创建数据库备份：%1")
                             .arg(QDir::toNativeSeparators(path));
    return path;
}

Result<int> inspectDatabaseFile(const QString &path)
{
    const QString directory = AppPaths::ensureDataDirectory();
    const QString copyPath = QDir(directory).filePath(
        QStringLiteral("cutee-inspect-%1.db").arg(QUuid::createUuid().toString(QUuid::Id128)));

    auto version = stageCopy(path, copyPath);
    QFile::remove(copyPath);
    removeJournalFiles(copyPath);
    return version;
}

Result<QString> importDatabaseFile(Database &database, const QString &sourcePath)
{
    const QString livePath = database.filePath();
    if (!database.isOpen() || livePath.isEmpty())
        return Error(QStringLiteral("数据库未打开"), QStringLiteral("maintenance::importDatabaseFile"));
    if (samePath(sourcePath, livePath))
        return Error(QStringLiteral("不能导入当前正在使用的数据库文件"),
                     QStringLiteral("maintenance::importDatabaseFile"));

    const QString directory = QFileInfo(livePath).absolutePath();
    const QString stagedPath = QDir(directory).filePath(
        QStringLiteral("cutee-import-%1.db").arg(QUuid::createUuid().toString(QUuid::Id128)));

    auto staged = stageCopy(sourcePath, stagedPath);
    if (!staged) {
        removeJournalFiles(stagedPath);
        QFile::remove(stagedPath);
        return staged.error();
    }

    // Back up before touching anything; the UI promises this to the user.
    auto backup = createBackup(database, QStringLiteral("before-import"));
    if (!backup) {
        removeJournalFiles(stagedPath);
        QFile::remove(stagedPath);
        return backup.error();
    }
    const QString backupPath = backup.value();

    // Puts the pre-import snapshot back in place and reopens it.
    const auto restoreBackup = [&database, &backupPath, &livePath] {
        removeJournalFiles(livePath);
        QFile::remove(livePath);
        QFile::copy(backupPath, livePath);
        removeJournalFiles(livePath);
        auto reopened = database.open(livePath);
        if (reopened)
            database.migrate();
    };

    database.close();
    removeJournalFiles(livePath);
    if (QFileInfo::exists(livePath) && !QFile::remove(livePath)) {
        auto reopened = database.open(livePath);
        Q_UNUSED(reopened);
        removeJournalFiles(stagedPath);
        QFile::remove(stagedPath);
        return Error(QStringLiteral("无法替换数据库文件（可能正被后台读取），请稍后重试"),
                     QStringLiteral("maintenance::importDatabaseFile"));
    }

    bool replaced = QFile::rename(stagedPath, livePath);
    if (replaced) {
        auto opened = database.open(livePath);
        if (!opened) {
            replaced = false;
        } else {
            auto migrated = database.migrate();
            replaced = migrated.ok();
        }
    }

    if (!replaced) {
        restoreBackup();
        removeJournalFiles(stagedPath);
        QFile::remove(stagedPath);
        return Error(QStringLiteral("导入失败，已还原导入前的数据"),
                     QStringLiteral("maintenance::importDatabaseFile"));
    }

    qInfo().noquote() << QStringLiteral("已导入数据库：%1（备份：%2）")
                             .arg(QDir::toNativeSeparators(sourcePath),
                                  QDir::toNativeSeparators(backupPath));
    return backupPath;
}

Result<void> resetAllData(Database &database)
{
    if (!database.isOpen())
        return Error(QStringLiteral("数据库未打开"), QStringLiteral("maintenance::resetAllData"));

    QSqlDatabase db = database.connection();
    if (!db.transaction())
        return Error(db.lastError().text(), QStringLiteral("maintenance::resetAllData"));

    const QStringList statements = {
        QStringLiteral("DELETE FROM exam_records"),
        QStringLiteral("DELETE FROM subjects"),
        QStringLiteral("DELETE FROM student_profile"),
        // AUTOINCREMENT counters, so a fresh start begins at id 1 again.
        QStringLiteral("DELETE FROM sqlite_sequence "
                       "WHERE name IN ('exam_records', 'subjects')"),
    };

    for (const QString &statement : statements) {
        // sqlite_sequence only exists once an AUTOINCREMENT row was inserted.
        if (statement.contains(QStringLiteral("sqlite_sequence"))) {
            QSqlQuery probe(db);
            if (probe.exec(QStringLiteral("SELECT 1 FROM sqlite_master WHERE type = 'table' "
                                          "AND name = 'sqlite_sequence'"))
                && !probe.next()) {
                continue;
            }
        }
        auto executed = db::execute(db, statement);
        if (!executed) {
            db.rollback();
            return executed;
        }
    }

    if (!db.commit()) {
        const QString message = db.lastError().text();
        db.rollback();
        return Error(message, QStringLiteral("maintenance::resetAllData"));
    }

    qInfo().noquote() << QStringLiteral("数据已重置：档案、科目与成绩已清空");
    return {};
}

} // namespace maintenance
} // namespace cutee
