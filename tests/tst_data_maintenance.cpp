#include <QtTest>

#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/AppPaths.h"
#include "storage/DataMaintenance.h"
#include "storage/Database.h"
#include "storage/Migrations.h"
#include "storage/SettingsRepository.h"
#include "storage/StudentProfileRepository.h"

using namespace cutee;

/// Export / import / reset must never leave the application without data: an
/// import replaces the file but keeps a snapshot of what was there before.
class TestDataMaintenance : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exportCreatesReadableSnapshot();
    void inspectRejectsNonCuteeFiles();
    void importReplacesDataAndBacksUp();
    void importRefusesLiveDatabase();
    void resetClearsRowsButKeepsSettings();

private:
    QTemporaryDir m_directory;
    Database *m_database = nullptr;
    int m_counter = 0;

    QString dbPath() const
    {
        return m_directory.filePath(QStringLiteral("live%1.db").arg(m_counter));
    }
    void seedProfile(const QString &name)
    {
        StudentProfile profile;
        profile.name = name;
        profile.stage = SchoolStage::Junior;
        profile.enrollmentYear = 2024;
        QVERIFY(StudentProfileRepository(*m_database).save(profile).ok());
    }
    /// Row count helper. Qt's QVERIFY expands to `return;`, so it cannot be used
    /// in a function returning int; -1 signals a query failure instead.
    static int count(const Database &database, const QString &table)
    {
        QSqlQuery query(database.connection());
        if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)) || !query.next())
            return -1;
        return query.value(0).toInt();
    }
};

void TestDataMaintenance::init()
{
    QVERIFY(m_directory.isValid());
    ++m_counter;
    m_database = new Database(QStringLiteral("test-maintenance-%1").arg(m_counter));
    QVERIFY2(m_database->open(dbPath()).ok(), "无法打开测试数据库");
    QVERIFY(m_database->migrate().ok());
}

void TestDataMaintenance::cleanup()
{
    delete m_database;
    m_database = nullptr;
}

void TestDataMaintenance::exportCreatesReadableSnapshot()
{
    seedProfile(QStringLiteral("林小满"));

    const QString target = m_directory.filePath(QStringLiteral("export.db"));
    auto exported = maintenance::exportSnapshot(*m_database, target);
    QVERIFY2(exported.ok(), qPrintable(exported.errorMessage()));
    QVERIFY(QFileInfo::exists(target));

    // The snapshot opens on its own and holds the same profile.
    Database copy(QStringLiteral("test-export-copy-%1").arg(m_counter));
    QVERIFY(copy.open(target).ok());
    QCOMPARE(copy.schemaVersion(), targetSchemaVersion());
    auto sticker = StudentProfileRepository(copy).load();
    QVERIFY(sticker.ok());
    QVERIFY(sticker.value().has_value());
    QCOMPARE(sticker.value()->name, QStringLiteral("林小满"));
    copy.close();

    // Exporting over an existing file replaces it instead of failing.
    QVERIFY(maintenance::exportSnapshot(*m_database, target).ok());
}

void TestDataMaintenance::inspectRejectsNonCuteeFiles()
{
    const QString text = m_directory.filePath(QStringLiteral("not-a-db.db"));
    QFile file(text);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is definitely not sqlite");
    file.close();
    QVERIFY(!maintenance::inspectDatabaseFile(text).ok());
    QVERIFY(!maintenance::inspectDatabaseFile(m_directory.filePath(QStringLiteral("missing.db"))).ok());

    // A plain (non-CUTEE) SQLite database is rejected as well.
    const QString other = m_directory.filePath(QStringLiteral("plain.db"));
    {
        Database plain(QStringLiteral("test-plain-%1").arg(m_counter));
        QVERIFY(plain.open(other).ok());
        QSqlQuery query(plain.connection());
        QVERIFY(query.exec(QStringLiteral("CREATE TABLE unrelated (id INTEGER)")));
        plain.close();
    }
    QVERIFY(!maintenance::inspectDatabaseFile(other).ok());

    QVERIFY(maintenance::inspectDatabaseFile(dbPath()).ok());
}

void TestDataMaintenance::importReplacesDataAndBacksUp()
{
    seedProfile(QStringLiteral("旧学生"));

    // Build an import file holding a different student.
    const QString source = m_directory.filePath(QStringLiteral("incoming.db"));
    {
        Database other(QStringLiteral("test-incoming-%1").arg(m_counter));
        QVERIFY(other.open(source).ok());
        QVERIFY(other.migrate().ok());
        StudentProfile profile;
        profile.name = QStringLiteral("新学生");
        profile.stage = SchoolStage::Senior;
        profile.enrollmentYear = 2023;
        QVERIFY(StudentProfileRepository(other).save(profile).ok());
        other.close();
    }

    auto imported = maintenance::importDatabaseFile(*m_database, source);
    QVERIFY2(imported.ok(), qPrintable(imported.errorMessage()));

    // The live database now holds the imported student...
    QVERIFY(m_database->isOpen());
    auto live = StudentProfileRepository(*m_database).load();
    QVERIFY(live.ok());
    QVERIFY(live.value().has_value());
    QCOMPARE(live.value()->name, QStringLiteral("新学生"));
    QCOMPARE(live.value()->stage, SchoolStage::Senior);

    // ...and the previous data survives in the automatic backup.
    QVERIFY(QFileInfo::exists(imported.value()));
    Database backup(QStringLiteral("test-backup-%1").arg(m_counter));
    QVERIFY(backup.open(imported.value()).ok());
    auto old = StudentProfileRepository(backup).load();
    QVERIFY(old.ok());
    QVERIFY(old.value().has_value());
    QCOMPARE(old.value()->name, QStringLiteral("旧学生"));
    backup.close();

    // No staging leftovers next to the live database.
    const QDir directory(QFileInfo(dbPath()).absolutePath());
    QVERIFY(directory.entryList({QStringLiteral("cutee-import-*")}, QDir::Files).isEmpty());
}

void TestDataMaintenance::importRefusesLiveDatabase()
{
    seedProfile(QStringLiteral("林小满"));
    auto refused = maintenance::importDatabaseFile(*m_database, dbPath());
    QVERIFY(!refused.ok());
    QVERIFY(m_database->isOpen());
}

void TestDataMaintenance::resetClearsRowsButKeepsSettings()
{
    seedProfile(QStringLiteral("林小满"));
    SettingsRepository settings(*m_database);
    QVERIFY(settings.setValue(QLatin1String(cutee::settings::kLogLevel),
                              QStringLiteral("debug"))
                .ok());

    QSqlQuery subject(m_database->connection());
    QVERIFY(subject.exec(QStringLiteral(
        "INSERT INTO subjects (name, type, sort_order, enabled, note, created_at, updated_at) "
        "VALUES ('数学', 'ACADEMIC', 0, 1, '', '', '')")));
    const qint64 subjectId = subject.lastInsertId().toLongLong();
    QSqlQuery record(m_database->connection());
    record.prepare(QStringLiteral(
        "INSERT INTO exam_records (exam_type, subject_id, title, exam_date, "
        "score_hundredths, full_score_hundredths, created_at, updated_at) "
        "VALUES ('ACADEMIC', ?, '期中', '2026-04-01', 9000, 10000, '', '')"));
    record.addBindValue(subjectId);
    QVERIFY(record.exec());

    QCOMPARE(count(*m_database, QStringLiteral("student_profile")), 1);
    QCOMPARE(count(*m_database, QStringLiteral("exam_records")), 1);

    auto cleared = maintenance::resetAllData(*m_database);
    QVERIFY2(cleared.ok(), qPrintable(cleared.errorMessage()));

    QCOMPARE(count(*m_database, QStringLiteral("student_profile")), 0);
    QCOMPARE(count(*m_database, QStringLiteral("subjects")), 0);
    QCOMPARE(count(*m_database, QStringLiteral("exam_records")), 0);

    // Settings and schema survive a reset.
    QCOMPARE(settings.value(QLatin1String(cutee::settings::kLogLevel)).value(),
             QStringLiteral("debug"));
    QCOMPARE(m_database->schemaVersion(), targetSchemaVersion());
}

QTEST_GUILESS_MAIN(TestDataMaintenance)

#include "tst_data_maintenance.moc"
