#include <QtTest>

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "storage/Database.h"
#include "storage/DbUtils.h"
#include "storage/Migrations.h"
#include "storage/SettingsRepository.h"
#include "storage/StudentProfileRepository.h"

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
    void profileStageColumnsAcceptLegacyRows();
    void legacyVersionUpgradesWithoutOldTables();
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
    QCOMPARE(database.schemaVersion(), 3);

    const QStringList expectedTables = {
        QStringLiteral("student_profile"), QStringLiteral("subjects"),
        QStringLiteral("exam_records"), QStringLiteral("settings"),
        QStringLiteral("schema_version"),
    };

    for (const QString &table : expectedTables) {
        QSqlQuery query(database.connection());
        query.prepare(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"));
        query.addBindValue(table);
        QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
        QVERIFY2(query.next(), qPrintable(QStringLiteral("缺少数据表 %1").arg(table)));
    }

    // The retired multi-student tables must no longer be created.
    for (const QString &table : {QStringLiteral("students"), QStringLiteral("questions"),
                                 QStringLiteral("exams"), QStringLiteral("scores")}) {
        QSqlQuery query(database.connection());
        query.prepare(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"));
        query.addBindValue(table);
        QVERIFY(query.exec());
        QVERIFY2(!query.next(), qPrintable(QStringLiteral("不应再创建旧表 %1").arg(table)));
    }

    // v3 replaced 学号 / 班级 with the entry stage and its enrolment year.
    QSqlQuery columns(database.connection());
    QVERIFY(columns.exec(QStringLiteral("PRAGMA table_info(student_profile)")));
    QStringList names;
    while (columns.next())
        names.append(columns.value(1).toString());
    QVERIFY(names.contains(QStringLiteral("stage")));
    QVERIFY(names.contains(QStringLiteral("enrollment_year")));
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
    profile.stage = SchoolStage::Senior;
    profile.enrollmentYear = 2024;
    profile.targetSchool = QStringLiteral("华中师大一附中");
    QVERIFY(profiles.save(profile).ok());
    QVERIFY(profiles.exists().value());

    StudentProfile edited;
    edited.name = QStringLiteral("张三丰");
    edited.stage = SchoolStage::Senior;
    edited.enrollmentYear = 2024;
    edited.targetSchool = QStringLiteral("华中师大一附中");
    QVERIFY(profiles.save(edited).ok());

    auto loaded = profiles.load();
    QVERIFY(loaded.ok());
    QVERIFY(loaded.value().has_value());
    QCOMPARE(loaded.value()->name, QStringLiteral("张三丰"));
    QCOMPARE(loaded.value()->stage, SchoolStage::Senior);
    QCOMPARE(loaded.value()->enrollmentYear, 2024);
    QCOMPARE(loaded.value()->targetSchool, QStringLiteral("华中师大一附中"));
    QVERIFY(loaded.value()->createdAt.isValid());

    QSqlQuery count(database.connection());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM student_profile")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 1);

    QSqlQuery second(database.connection());
    QVERIFY(!second.exec(QStringLiteral(
        "INSERT INTO student_profile (id, name, updated_at) VALUES (2, '第二个学生', '')")));
}

/// Rows written before v3 have no stage / enrolment year; they must still load
/// and simply report "未填写学段" until the profile is edited.
void TestDatabase::profileStageColumnsAcceptLegacyRows()
{
    Database database(QStringLiteral("test-legacy-profile"));
    QVERIFY(database.open(freshPath(QStringLiteral("legacy-profile"))).ok());
    QVERIFY(database.migrate().ok());

    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("INSERT INTO student_profile "
                                       "(id, name, student_no, school_class, note) "
                                       "VALUES (1, '旧学生', 'S001', '高二（1）班', '')"))
                .ok());

    StudentProfileRepository profiles(database);
    auto loaded = profiles.load();
    QVERIFY(loaded.ok());
    QVERIFY(loaded.value().has_value());
    QCOMPARE(loaded.value()->name, QStringLiteral("旧学生"));
    QCOMPARE(loaded.value()->enrollmentYear, 0);
    QVERIFY(!resolveGrade(loaded.value()->stage, loaded.value()->enrollmentYear, QDate(2026, 3, 1))
                 .known);
}

/// An existing installation that still reports schema v1 (the retired
/// multi-student schema) must upgrade to the current schema without the legacy
/// tables being required and without auto-creating a profile.
void TestDatabase::legacyVersionUpgradesWithoutOldTables()
{
    const QString path = freshPath(QStringLiteral("upgrade-v1"));
    Database database(QStringLiteral("test-upgrade-v1"));
    QVERIFY(database.open(path).ok());

    QVERIFY(db::execute(database.connection(),
                        QStringLiteral("CREATE TABLE schema_version (version INTEGER NOT NULL)"))
                .ok());
    QVERIFY(db::execute(database.connection(), QStringLiteral("INSERT INTO schema_version VALUES (1)"))
                .ok());

    auto migrated = database.migrate();
    QVERIFY2(migrated.ok(), qPrintable(migrated.errorMessage()));
    QCOMPARE(database.schemaVersion(), targetSchemaVersion());

    QSqlQuery profileCount(database.connection());
    QVERIFY(profileCount.exec(QStringLiteral("SELECT COUNT(*) FROM student_profile")));
    QVERIFY(profileCount.next());
    QCOMPARE(profileCount.value(0).toInt(), 0);
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
