#include <QtTest>

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "core/ScoreValue.h"
#include "service/ScoreService.h"
#include "storage/Database.h"
#include "storage/DbUtils.h"
#include "storage/ExamRecordRepository.h"
#include "storage/SubjectRepository.h"

using namespace cutee;

class TestScores : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void subjectCrud();
    void duplicateSubjectNameIsScopedToType();
    void referencedSubjectCannotBeDeleted();
    void subjectOrderingAndEnableFlag();
    void subjectTypeIsImmutable();
    void scoreValueParsing();
    void recordCrudAndFiltering();
    void recordValidation();

private:
    Database *openDatabase(const QString &name);

    QTemporaryDir m_directory;
};

Database *TestScores::openDatabase(const QString &name)
{
    auto *database = new Database(QStringLiteral("scores-") + name);
    if (!database->open(m_directory.filePath(name + QStringLiteral(".db"))).ok()) {
        delete database;
        return nullptr;
    }
    if (!database->migrate().ok()) {
        delete database;
        return nullptr;
    }
    return database;
}

void TestScores::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void TestScores::subjectCrud()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("subject-crud")));
    QVERIFY(database);
    SubjectRepository subjects(*database);

    Subject oi;
    oi.name = QStringLiteral("  NOIP ");
    oi.type = ExamType::Oi;
    oi.note = QStringLiteral("提高组");
    auto created = subjects.create(oi);
    QVERIFY2(created.ok(), qPrintable(created.errorMessage()));
    QVERIFY(oi.id > 0);
    QCOMPARE(oi.name, QStringLiteral("NOIP"));
    QVERIFY(oi.sortOrder > 0);

    auto found = subjects.find(oi.id);
    QVERIFY(found.ok());
    QCOMPARE(found.value().type, ExamType::Oi);
    QCOMPARE(found.value().note, QStringLiteral("提高组"));

    Subject academic;
    academic.name = QStringLiteral("数学");
    academic.type = ExamType::Academic;
    QVERIFY(subjects.create(academic).ok());

    auto all = subjects.list();
    QVERIFY(all.ok());
    QCOMPARE(all.value().size(), 2);

    auto oiOnly = subjects.list(ExamType::Oi);
    QVERIFY(oiOnly.ok());
    QCOMPARE(oiOnly.value().size(), 1);
    QCOMPARE(oiOnly.value().first().name, QStringLiteral("NOIP"));

    Subject edited = found.value();
    edited.name = QStringLiteral("NOIP 提高组");
    QVERIFY2(subjects.update(edited).ok(), "更新科目失败");
    QCOMPARE(subjects.find(oi.id).value().name, QStringLiteral("NOIP 提高组"));

    QVERIFY(subjects.remove(oi.id).ok());
    QVERIFY(!subjects.find(oi.id).ok());
}

void TestScores::duplicateSubjectNameIsScopedToType()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("subject-duplicate")));
    QVERIFY(database);
    SubjectRepository subjects(*database);

    Subject first;
    first.name = QStringLiteral("模拟赛");
    first.type = ExamType::Oi;
    QVERIFY(subjects.create(first).ok());

    Subject clash;
    clash.name = QStringLiteral("模拟赛");
    clash.type = ExamType::Oi;
    auto rejected = subjects.create(clash);
    QVERIFY2(!rejected.ok(), "同类型同名科目应被拒绝");
    QVERIFY(rejected.errorMessage().contains(QStringLiteral("同名")));

    Subject sameNameOtherType;
    sameNameOtherType.name = QStringLiteral("模拟赛");
    sameNameOtherType.type = ExamType::Academic;
    QVERIFY2(subjects.create(sameNameOtherType).ok(), "不同类型允许同名");
}

void TestScores::referencedSubjectCannotBeDeleted()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("subject-protection")));
    QVERIFY(database);
    SubjectRepository subjects(*database);

    Subject subject;
    subject.name = QStringLiteral("CSP-J");
    subject.type = ExamType::Oi;
    QVERIFY(subjects.create(subject).ok());

    QVERIFY(db::execute(
                database->connection(),
                QStringLiteral("INSERT INTO exam_records (exam_type, subject_id, title, exam_date, "
                               "score_hundredths, full_score_hundredths, created_at, updated_at) "
                               "VALUES ('OI', ?, '初赛', '2025-09-20', 9000, 10000, '', '')"),
                {subject.id})
                .ok());

    QCOMPARE(subjects.referenceCount(subject.id).value(), 1);
    auto removed = subjects.remove(subject.id);
    QVERIFY2(!removed.ok(), "被成绩引用的科目不应被删除");
    QVERIFY(removed.errorMessage().contains(QStringLiteral("停用")));

    QVERIFY(db::execute(database->connection(),
                        QStringLiteral("DELETE FROM exam_records WHERE subject_id = ?"), {subject.id})
                .ok());
    QVERIFY2(subjects.remove(subject.id).ok(), "引用解除后应允许删除");
}

void TestScores::subjectOrderingAndEnableFlag()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("subject-order")));
    QVERIFY(database);
    SubjectRepository subjects(*database);

    QStringList names = {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")};
    QVector<Subject> created;
    for (const QString &name : names) {
        Subject subject;
        subject.name = name;
        subject.type = ExamType::Academic;
        QVERIFY(subjects.create(subject).ok());
        created.append(subject);
    }

    auto ordered = subjects.list(ExamType::Academic);
    QVERIFY(ordered.ok());
    QCOMPARE(ordered.value().size(), 3);
    QCOMPARE(ordered.value().at(0).name, QStringLiteral("A"));

    QVERIFY(subjects.move(created.at(2).id, -1).ok());
    ordered = subjects.list(ExamType::Academic);
    QCOMPARE(ordered.value().at(1).name, QStringLiteral("C"));
    QCOMPARE(ordered.value().at(2).name, QStringLiteral("B"));

    QVERIFY(subjects.setEnabled(created.at(0).id, false).ok());
    auto enabledOnly = subjects.list(ExamType::Academic, false);
    QVERIFY(enabledOnly.ok());
    QCOMPARE(enabledOnly.value().size(), 2);
    auto withDisabled = subjects.list(ExamType::Academic, true);
    QCOMPARE(withDisabled.value().size(), 3);
    QVERIFY(!withDisabled.value().at(0).enabled || !withDisabled.value().at(1).enabled
            || !withDisabled.value().at(2).enabled);
}

void TestScores::subjectTypeIsImmutable()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("subject-type")));
    QVERIFY(database);
    SubjectRepository subjects(*database);

    Subject subject;
    subject.name = QStringLiteral("物理");
    subject.type = ExamType::Academic;
    QVERIFY(subjects.create(subject).ok());

    Subject changed = subject;
    changed.id = subject.id;
    changed.type = ExamType::Oi;
    auto rejected = subjects.update(changed);
    QVERIFY2(!rejected.ok(), "科目类型不应被修改");
    QVERIFY(rejected.errorMessage().contains(QStringLiteral("类型")));
}

void TestScores::scoreValueParsing()
{
    QCOMPARE(scoreutil::parsePoints(QStringLiteral("152.5")).value(), qint64(15250));
    QCOMPARE(scoreutil::parsePoints(QStringLiteral("152.50")).value(), qint64(15250));
    QCOMPARE(scoreutil::parsePoints(QStringLiteral("0.25")).value(), qint64(25));
    QCOMPARE(scoreutil::parsePoints(QStringLiteral("90")).value(), qint64(9000));
    QVERIFY(!scoreutil::parsePoints(QStringLiteral("12.345")).has_value());
    QVERIFY(!scoreutil::parsePoints(QStringLiteral("-1")).has_value());
    QVERIFY(!scoreutil::parsePoints(QStringLiteral("abc")).has_value());

    QCOMPARE(scoreutil::formatPoints(15250), QStringLiteral("152.5"));
    QCOMPARE(scoreutil::formatPoints(9000), QStringLiteral("90"));
    QCOMPARE(scoreutil::formatPoints(25), QStringLiteral("0.25"));
    QCOMPARE(scoreutil::formatRate(15250, 40000), QStringLiteral("38.1%"));
}

void TestScores::recordCrudAndFiltering()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("record-crud")));
    QVERIFY(database);
    SubjectRepository subjects(*database);
    ScoreService service(*database);

    Subject csp;
    csp.name = QStringLiteral("CSP-J");
    csp.type = ExamType::Oi;
    QVERIFY(subjects.create(csp).ok());

    Subject math;
    math.name = QStringLiteral("数学");
    math.type = ExamType::Academic;
    QVERIFY(subjects.create(math).ok());

    auto makeRecord = [](ExamType type, qint64 subjectId, const QString &title, const QDate &date,
                         qint64 score, qint64 fullScore) {
        ExamRecord record;
        record.examType = type;
        record.subjectId = subjectId;
        record.title = title;
        record.examDate = date;
        record.scoreHundredths = score;
        record.fullScoreHundredths = fullScore;
        return record;
    };

    ExamRecord mathMidterm =
        makeRecord(ExamType::Academic, math.id, QStringLiteral("数学期中"), QDate(2025, 3, 10), 9200, 10000);
    QVERIFY2(service.create(mathMidterm).ok(), "新增数学期中失败");
    ExamRecord mathFinal =
        makeRecord(ExamType::Academic, math.id, QStringLiteral("数学期末"), QDate(2025, 6, 20), 8800, 10000);
    QVERIFY(service.create(mathFinal).ok());
    ExamRecord contest =
        makeRecord(ExamType::Oi, csp.id, QStringLiteral("CSP-J 第一轮"), QDate(2025, 9, 20), 9000, 10000);
    contest.rank = 12;
    contest.participants = 300;
    QVERIFY(service.create(contest).ok());
    QVERIFY(contest.id > 0);

    ExamRecordFilter all;
    auto listed = service.list(all);
    QVERIFY(listed.ok());
    QCOMPARE(listed.value().size(), 3);
    QCOMPARE(listed.value().first().examDate, QDate(2025, 9, 20)); // newest first

    ExamRecordFilter oiOnly;
    oiOnly.type = ExamType::Oi;
    auto oiRecords = service.list(oiOnly);
    QVERIFY(oiRecords.ok());
    QCOMPARE(oiRecords.value().size(), 1);
    QCOMPARE(oiRecords.value().first().subjectName, QStringLiteral("CSP-J"));
    QCOMPARE(oiRecords.value().first().rank.value(), 12);
    QCOMPARE(oiRecords.value().first().participants.value(), 300);

    ExamRecordFilter mathOnly;
    mathOnly.subjectId = math.id;
    QCOMPARE(service.list(mathOnly).value().size(), 2);

    ExamRecordFilter spring;
    spring.from = QDate(2025, 1, 1);
    spring.to = QDate(2025, 6, 30);
    auto springRecords = service.list(spring);
    QVERIFY(springRecords.ok());
    QCOMPARE(springRecords.value().size(), 2);

    ExamRecordFilter keyword;
    keyword.keyword = QStringLiteral("期中");
    auto keywordRecords = service.list(keyword);
    QVERIFY(keywordRecords.ok());
    QCOMPARE(keywordRecords.value().size(), 1);
    QCOMPARE(keywordRecords.value().first().id, mathMidterm.id);

    ExamRecord edited = mathMidterm;
    edited.title = QStringLiteral("数学期中（补考）");
    edited.scoreHundredths = 9500;
    QVERIFY2(service.update(edited).ok(), "编辑成绩失败");
    auto reloaded = service.find(mathMidterm.id);
    QVERIFY(reloaded.ok());
    QCOMPARE(reloaded.value().scoreHundredths, qint64(9500));
    QCOMPARE(reloaded.value().title, QStringLiteral("数学期中（补考）"));
    QVERIFY(!reloaded.value().rank.has_value());
    QVERIFY(!reloaded.value().participants.has_value());

    QVERIFY(service.remove(mathFinal.id).ok());
    QCOMPARE(service.list(all).value().size(), 2);
    QVERIFY(!service.find(mathFinal.id).ok());
}

void TestScores::recordValidation()
{
    QScopedPointer<Database> database(openDatabase(QStringLiteral("record-validation")));
    QVERIFY(database);
    SubjectRepository subjects(*database);
    ScoreService service(*database);

    Subject math;
    math.name = QStringLiteral("英语");
    math.type = ExamType::Academic;
    QVERIFY(subjects.create(math).ok());

    Subject oi;
    oi.name = QStringLiteral("模拟赛");
    oi.type = ExamType::Oi;
    QVERIFY(subjects.create(oi).ok());

    ExamRecord valid;
    valid.examType = ExamType::Academic;
    valid.subjectId = math.id;
    valid.title = QStringLiteral("单元测试");
    valid.examDate = QDate(2025, 4, 1);
    valid.scoreHundredths = 8000;
    valid.fullScoreHundredths = 10000;
    QVERIFY(service.validate(valid, true).ok());

    ExamRecord noTitle = valid;
    noTitle.title = QStringLiteral("  ");
    QVERIFY(!service.validate(noTitle, true).ok());
    QVERIFY(service.validate(noTitle, true).error.message().contains(QStringLiteral("考试名称")));

    ExamRecord zeroFull = valid;
    zeroFull.fullScoreHundredths = 0;
    QVERIFY(!service.validate(zeroFull, true).ok());
    QVERIFY(service.validate(zeroFull, true).error.message().contains(QStringLiteral("满分")));

    ExamRecord negative = valid;
    negative.scoreHundredths = -1;
    QVERIFY(!service.validate(negative, true).ok());

    ExamRecord mismatched = valid;
    mismatched.examType = ExamType::Oi;
    QVERIFY(!service.validate(mismatched, true).ok());
    QVERIFY(service.validate(mismatched, true).error.message().contains(QStringLiteral("类型")));

    ExamRecord overFull = valid;
    overFull.scoreHundredths = 10500;
    const RecordValidation warning = service.validate(overFull, true);
    QVERIFY2(warning.ok(), "得分高于满分只应警告");
    QCOMPARE(warning.warnings.size(), 1);
    QVERIFY(warning.warnings.first().contains(QStringLiteral("高于满分")));

    ExamRecord badRank = valid;
    badRank.rank = 30;
    badRank.participants = 20;
    QVERIFY(!service.validate(badRank, true).ok());

    ExamRecord missingSubject = valid;
    missingSubject.subjectId = 99999;
    QVERIFY(!service.validate(missingSubject, true).ok());

    // A disabled subject may not be used for new records but stays editable.
    QVERIFY(subjects.setEnabled(math.id, false).ok());
    QVERIFY(!service.validate(valid, true).ok());
    QVERIFY(service.validate(valid, false).ok());

    ExamRecord stored = valid;
    auto created = service.create(stored);
    QVERIFY(!created.ok()); // still disabled for insert

    QVERIFY(subjects.setEnabled(math.id, true).ok());
    QVERIFY(service.create(stored).ok());
    QVERIFY(subjects.setEnabled(math.id, false).ok());

    stored.note = QStringLiteral("停用后仍可编辑");
    QVERIFY2(service.update(stored).ok(), "已停用科目的历史成绩仍应允许编辑");
}

QTEST_GUILESS_MAIN(TestScores)

#include "tst_scores.moc"
