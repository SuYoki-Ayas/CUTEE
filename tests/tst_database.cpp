#include <QtTest>

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "storage/Database.h"
#include "storage/DbUtils.h"
#include "storage/Migrations.h"
#include "storage/SettingsRepository.h"
#include "storage/StudentProfileRepository.h"
#include "service/LegacyDataService.h"

using namespace cutee;

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void migrationCreatesSchema();
    void migrationIsIdempotent();
    void settingsRoundTrip();
    void singleProfileIsUpserted();
    void legacyStudentsArePreserved();
    void legacyCleanupKeepsNewData();
    void foreignKeysAreEnforced();

private:
    QString freshPath(const QString &name) const;
    QTemporaryDir m_directory;
};

QString TestDatabase::freshPath(const QString &name) const
{
    return m_directory.filePath(name + QStringLiteral(".db"));
}

void TestDatabase::initTestCase()
{
    QVERIFY2(m_directory.isValid(), "临时目录创建失败");
}

void TestDatabase::migrationCreatesSchema()
{
    Database database(QStringLiteral("test-migration"));
    auto opened = database.open(freshPath(QStringLiteral("migration")));
    QVERIFY2(opened.ok(), qPrintable(opened.errorMessage()));

    auto migrated = database.migrate();
    QVERIFY2(migrated.ok(), qPrintable(migrated.errorMessage()));
    QCOMPARE(database.schemaVersion(), targetSchemaVersion());
    QCOMPARE(database.schemaVersion(), 2);

    const QStringList expectedTables = {
        // New single-student schema.
        QStringLiteral("student_profile"), QStringLiteral("subjects"),
        QStringLiteral("exam_records"),
        // Legacy tables are kept and must not be dropped.
        QStringLiteral("students"),      QStringLiteral("questions"),
        QStringLiteral("exams"),         QStringLiteral("exam_questions"),
        QStringLiteral("exam_students"), QStringLiteral("answers"),
        QStringLiteral("submissions"),   QStringLiteral("scores"),
        // Shared infrastructure.
        QStringLiteral("settings"), QStringLiteral("schema_version"),
    };

    for (const QString &table : expectedTables) {
        QSqlQuery query(database.connection());
        query.prepare(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"));
        query.addBindValue(table);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QVERIFY2(query.next(), qPrintable(QStringLiteral("缺少数据表 %1").arg(table)));
    }
}

void TestDatabase::migrationIsIdempotent()
{
    Database database(QStringLiteral("test-idempotent"));
    QVERIFY(database.open(freshPath(QStringLiteral("idempotent"))).ok());

    auto first = database.migrate();
    QVERIFY2(first.ok(), qPrintable(first.errorMessage()));
    const int version = database.schemaVersion();

    auto second = database.migrate();
    QVERIFY2(second.ok(), qPrintable(second.errorMessage()));
    QCOMPARE(database.schemaVersion(), version);
}

void TestDatabase::settingsRoundTrip()
{
    Database database(QStringLiteral("test-settings"));
    QVERIFY(database.open(freshPath(QStringLiteral("settings"))).ok());
    QVERIFY(database.migrate().ok());

    SettingsRepository settings(database);

    auto missing = settings.value(QStringLiteral("does.not.exist"), QStringLiteral("fallback"));
    QVERIFY(missing.ok());
    QCOMPARE(missing.value(), QStringLiteral("fallback"));

    QVERIFY(settings.setValue(QStringLiteral("test.key"), QStringLiteral("debug")).ok());
    QCOMPARE(settings.value(QStringLiteral("test.key")).value(), QStringLiteral("debug"));

    QVERIFY(settings.remove(QStringLiteral("test.key")).ok());
    QCOMPARE(settings.value(QStringLiteral("test.key"), QStringLiteral("gone")).value(),
             QStringLiteral("gone"));
}

void TestDatabase::singleProfileIsUpserted()
{
    Database database(QStringLiteral("test-profile"));
    QVERIFY(database.open(freshPath(QStringLiteral("profile"))).ok());
    QVERIFY(database.migrate().ok());

    StudentProfileRepository profiles(database);
    QVERIFY(!profiles.exists().value());

    StudentProfile profile;
    profile.name = QStringLiteral("张三");
    profile.studentNo = QStringLiteral("S001");
    profile.schoolClass = QStringLiteral("高一（1）班");
    QVERIFY(profiles.save(profile).ok());
    QVERIFY(profiles.exists().value());

    StudentProfile edited;
    edited.name = QStringLiteral("张三丰");
    QVERIFY(profiles.save(edited).ok());

    auto loaded = profiles.load();
    QVERIFY(loaded.ok());
    QVERIFY(loaded.value().has_value());
    QCOMPARE(loaded.value()->name, QStringLiteral("张三丰"));
    QVERIFY(loaded.value()->createdAt.isValid());

    QSqlQuery count(database.connection());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM student_profile")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);

    QSqlQuery second(database.connection());
    QVERIFY(!second.exec(QStringLiteral(
        "INSERT INTO student_profile (id, name, updated_at) VALUES (2, '第二个学生', '')")));
}

void TestDatabase::legacyStudentsArePreserved()
{
    const QString path = freshPath(QStringLiteral("legacy"));
    Database database(QStringLiteral("test-legacy"));
    QVERIFY(database.open(path).ok());

    // Build a v1 database by hand, then seed two legacy students.
    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("CREATE TABLE schema_version (version INTEGER NOT NULL)"))
                .ok());
    for (const Migration &migration : migrations()) {
        if (migration.version > 1)
            break;
        for (const QString &statement : migration.statements)
            QVERIFY2(db::execute(database.connection(), statement).ok(), qPrintable(statement));
    }
    QVERIFY(db::execute(database.connection(), QStringLiteral("INSERT INTO schema_version VALUES (1)"))
                .ok());
    QVERIFY(db::execute(
                database.connection(),
                QStringLiteral("INSERT INTO students (id, name, student_no, class_name, note, created_at, updated_at) "
                               "VALUES (?, ?, ?, ?, ?, ?, ?)"),
                {QStringLiteral("s1"), QStringLiteral("第一位"), QStringLiteral("A001"),
                 QStringLiteral("高一（1）班"), QStringLiteral("旧备注"),
                 QStringLiteral("2024-09-01T00:00:00Z"), QStringLiteral("2024-09-01T00:00:00Z")})
                .ok());
    QVERIFY(db::execute(
                database.connection(),
                QStringLiteral("INSERT INTO students (id, name, student_no, class_name, note, created_at, updated_at) "
                               "VALUES (?, ?, ?, ?, ?, ?, ?)"),
                {QStringLiteral("s2"), QStringLiteral("第二位"), QStringLiteral("A002"),
                 QStringLiteral("高一（2）班"), QString(),
                 QStringLiteral("2024-10-01T00:00:00Z"), QStringLiteral("2024-10-01T00:00:00Z")})
                .ok());

    auto migrated = database.migrate();
    QVERIFY2(migrated.ok(), qPrintable(migrated.errorMessage()));
    QCOMPARE(database.schemaVersion(), 2);

    // Old rows are still there, nothing was deleted.
    QSqlQuery countQuery(database.connection());
    QVERIFY(countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM students")));
    QVERIFY(countQuery.next());
    QCOMPARE(countQuery.value(0).toInt(), 2);

    // The earliest legacy student became the single current profile.
    QSqlQuery profile(database.connection());
    QVERIFY(profile.exec(QStringLiteral(
        "SELECT name, student_no, school_class, note FROM student_profile WHERE id = 1")));
    QVERIFY(profile.next());
    QCOMPARE(profile.value(0).toString(), QStringLiteral("第一位"));
    QCOMPARE(profile.value(1).toString(), QStringLiteral("A001"));
    QCOMPARE(profile.value(2).toString(), QStringLiteral("高一（1）班"));
    QCOMPARE(profile.value(3).toString(), QStringLiteral("旧备注"));
}

void TestDatabase::legacyCleanupKeepsNewData()
{
    Database database(QStringLiteral("test-legacy-cleanup"));
    QVERIFY(database.open(freshPath(QStringLiteral("legacy-cleanup"))).ok());
    QVERIFY(database.migrate().ok());

    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("INSERT INTO students (id, name, student_no, class_name, note, "
                                       "created_at, updated_at) VALUES ('s1', '旧学生', '', '', '', '', '')"))
                .ok());
    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("INSERT INTO subjects (name, type, sort_order, enabled, note, "
                                       "created_at, updated_at) VALUES ('数学', 'ACADEMIC', 1, 1, '', '', '')"))
                .ok());
    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("INSERT INTO student_profile (id, name, updated_at) "
                                       "VALUES (1, '当前学生', '')"))
                .ok());

    LegacyDataService legacy(database);
    auto stats = legacy.stats();
    QVERIFY(stats.ok());
    QCOMPARE(stats.value().students, 1);
    QCOMPARE(stats.value().totalRows(), 1);

    QVERIFY(legacy.clear().ok());

    auto cleared = legacy.stats();
    QVERIFY(cleared.ok());
    QCOMPARE(cleared.value().totalRows(), 0);

    QSqlQuery newData(database.connection());
    QVERIFY(newData.exec(QStringLiteral(
        "SELECT (SELECT COUNT(*) FROM subjects), (SELECT COUNT(*) FROM student_profile)")));
    QVERIFY(newData.next());
    QCOMPARE(newData.value(0).toInt(), 1);
    QCOMPARE(newData.value(1).toInt(), 1);
}

void TestDatabase::foreignKeysAreEnforced()
{
    Database database(QStringLiteral("test-foreign-keys"));
    QVERIFY(database.open(freshPath(QStringLiteral("foreign-keys"))).ok());
    QVERIFY(database.migrate().ok());

    QSqlQuery query(database.connection());
    const bool inserted = query.exec(QStringLiteral(
        "INSERT INTO exam_records (exam_type, subject_id, title, exam_date, "
        "score_hundredths, full_score_hundredths, created_at, updated_at) "
        "VALUES ('OI', 9999, 'missing subject', '2025-01-01', 100, 100, '', '')"));
    QVERIFY2(!inserted, "外键约束未生效：应拒绝引用不存在的科目");
}

QTEST_GUILESS_MAIN(TestDatabase)

#include "tst_database.moc"
