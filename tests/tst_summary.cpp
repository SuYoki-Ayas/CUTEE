#include <QtTest>

#include <QTemporaryDir>

#include "core/CsvExport.h"
#include "core/ScoreSummary.h"
#include "core/ScoreValue.h"
#include "service/ScoreService.h"
#include "storage/Database.h"
#include "storage/SubjectRepository.h"

using namespace cutee;

class TestSummary : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void semesterRangeResolution();
    void customRangeIsNormalized();
    void aggregationSeparatesTypesAndSubjects();
    void serviceSummaryRespectsRange();
    void recordCsvFields();
    void summaryCsvSections();

private:
    static ExamRecord makeRecord(ExamType type, qint64 subjectId, const QString &title,
                                 const QDate &date, qint64 score, qint64 fullScore);
    static Subject makeSubject(qint64 id, const QString &name, ExamType type);

    QTemporaryDir m_directory;
};

ExamRecord TestSummary::makeRecord(ExamType type, qint64 subjectId, const QString &title,
                                   const QDate &date, qint64 score, qint64 fullScore)
{
    ExamRecord record;
    record.examType = type;
    record.subjectId = subjectId;
    record.title = title;
    record.examDate = date;
    record.scoreHundredths = score;
    record.fullScoreHundredths = fullScore;
    return record;
}

Subject TestSummary::makeSubject(qint64 id, const QString &name, ExamType type)
{
    Subject subject;
    subject.id = id;
    subject.name = name;
    subject.type = type;
    subject.enabled = true;
    return subject;
}

void TestSummary::initTestCase()
{
    QVERIFY(m_directory.isValid());
}

void TestSummary::semesterRangeResolution()
{
    const DateRange fall = resolveTimeRange(TimeRangeKind::Semester, {}, {}, QDate(2025, 10, 15));
    QCOMPARE(fall.from, QDate(2025, 9, 1));
    QCOMPARE(fall.to, QDate(2026, 1, 31));

    const DateRange january = resolveTimeRange(TimeRangeKind::Semester, {}, {}, QDate(2026, 1, 20));
    QCOMPARE(january.from, QDate(2025, 9, 1));
    QCOMPARE(january.to, QDate(2026, 1, 31));

    const DateRange spring = resolveTimeRange(TimeRangeKind::Semester, {}, {}, QDate(2026, 3, 1));
    QCOMPARE(spring.from, QDate(2026, 2, 1));
    QCOMPARE(spring.to, QDate(2026, 8, 31));

    const DateRange august = resolveTimeRange(TimeRangeKind::Semester, {}, {}, QDate(2026, 8, 31));
    QCOMPARE(august.from, QDate(2026, 2, 1));
    QCOMPARE(august.to, QDate(2026, 8, 31));

    const DateRange all = resolveTimeRange(TimeRangeKind::All, {}, {}, QDate(2026, 3, 1));
    QVERIFY(all.isAll());
    QVERIFY(all.contains(QDate(1999, 1, 1)));
}

void TestSummary::customRangeIsNormalized()
{
    const DateRange swapped = resolveTimeRange(TimeRangeKind::Custom, QDate(2025, 6, 30),
                                               QDate(2025, 1, 1));
    QCOMPARE(swapped.from, QDate(2025, 1, 1));
    QCOMPARE(swapped.to, QDate(2025, 6, 30));
    QVERIFY(swapped.contains(QDate(2025, 3, 1)));
    QVERIFY(!swapped.contains(QDate(2025, 9, 1)));

    const DateRange missing = resolveTimeRange(TimeRangeKind::Custom, {}, {});
    QVERIFY(missing.isAll());
}

void TestSummary::aggregationSeparatesTypesAndSubjects()
{
    const Subject math = makeSubject(1, QStringLiteral("数学"), ExamType::Academic);
    const Subject noip = makeSubject(2, QStringLiteral("NOIP"), ExamType::Oi);
    const QVector<Subject> subjects = {noip, math};

    QVector<ExamRecord> records = {
        makeRecord(ExamType::Academic, 1, QStringLiteral("数学期中"), QDate(2025, 3, 10), 9000, 10000),
        makeRecord(ExamType::Academic, 1, QStringLiteral("数学期末"), QDate(2025, 6, 20), 8000, 10000),
        makeRecord(ExamType::Oi, 2, QStringLiteral("NOIP 初赛"), QDate(2025, 9, 20), 15250, 40000),
        makeRecord(ExamType::Oi, 2, QStringLiteral("NOIP 复赛"), QDate(2025, 11, 30), 20000, 40000),
    };
    records[3].id = 3;

    const ScoreSummary summary = buildScoreSummary(records, subjects);
    QCOMPARE(summary.recordCount, 4);
    QCOMPARE(summary.bySubject.size(), 2);
    QCOMPARE(summary.byType.size(), 2);

    // OI subjects are listed first so the two scales are never visually mixed.
    const SubjectAggregate &oi = summary.bySubject.at(0);
    QCOMPARE(oi.subjectName, QStringLiteral("NOIP"));
    QCOMPARE(oi.type, ExamType::Oi);
    QCOMPARE(oi.stats.examCount, 2);
    QCOMPARE(oi.stats.averageHundredths, qint64(17625)); // (152.5 + 200) / 2
    QCOMPARE(qRound(oi.stats.averageRate * 1000000), 440625);
    QCOMPARE(oi.stats.bestHundredths, qint64(20000));
    QCOMPARE(oi.stats.worstHundredths, qint64(15250));
    QCOMPARE(oi.stats.latestDate, QDate(2025, 11, 30));
    QCOMPARE(oi.stats.latestHundredths, qint64(20000));
    QCOMPARE(oi.stats.latestTitle, QStringLiteral("NOIP 复赛"));

    const SubjectAggregate &academic = summary.bySubject.at(1);
    QCOMPARE(academic.subjectName, QStringLiteral("数学"));
    QCOMPARE(academic.stats.examCount, 2);
    QCOMPARE(academic.stats.averageHundredths, qint64(8500));
    QCOMPARE(qRound(academic.stats.averageRate * 1000000), 850000);
    QCOMPARE(academic.stats.bestHundredths, qint64(9000));
    QCOMPARE(academic.stats.worstHundredths, qint64(8000));
    QCOMPARE(academic.stats.latestDate, QDate(2025, 6, 20));
    QCOMPARE(academic.stats.latestHundredths, qint64(8000));

    QCOMPARE(summary.byType.at(0).type, ExamType::Oi);
    QCOMPARE(summary.byType.at(0).stats.examCount, 2);
    QCOMPARE(summary.byType.at(0).stats.averageHundredths, qint64(17625));
    QCOMPARE(summary.byType.at(1).type, ExamType::Academic);
    QCOMPARE(summary.byType.at(1).stats.examCount, 2);
    QCOMPARE(summary.byType.at(1).stats.averageHundredths, qint64(8500));
}

void TestSummary::serviceSummaryRespectsRange()
{
    Database database(QStringLiteral("summary-service"));
    QVERIFY(database.open(m_directory.filePath(QStringLiteral("summary.db"))).ok());
    QVERIFY(database.migrate().ok());

    SubjectRepository subjects(database);
    ScoreService service(database);

    Subject math;
    math.name = QStringLiteral("数学");
    math.type = ExamType::Academic;
    QVERIFY(subjects.create(math).ok());

    Subject oi;
    oi.name = QStringLiteral("CSP-J");
    oi.type = ExamType::Oi;
    QVERIFY(subjects.create(oi).ok());

    ExamRecord spring =
        makeRecord(ExamType::Academic, math.id, QStringLiteral("春季月考"), QDate(2025, 3, 1), 9000, 10000);
    QVERIFY(service.create(spring).ok());
    ExamRecord autumn =
        makeRecord(ExamType::Oi, oi.id, QStringLiteral("秋季初赛"), QDate(2025, 10, 1), 8000, 10000);
    QVERIFY(service.create(autumn).ok());

    const DateRange firstHalf = resolveTimeRange(TimeRangeKind::Custom, QDate(2025, 1, 1),
                                                 QDate(2025, 6, 30));
    auto filtered = service.summarize(firstHalf);
    QVERIFY(filtered.ok());
    QCOMPARE(filtered.value().recordCount, 1);
    QCOMPARE(filtered.value().bySubject.size(), 1);
    QCOMPARE(filtered.value().bySubject.first().subjectName, QStringLiteral("数学"));
    QCOMPARE(filtered.value().byType.size(), 1);
    QCOMPARE(filtered.value().byType.first().type, ExamType::Academic);

    auto everything = service.summarize(DateRange{});
    QVERIFY(everything.ok());
    QCOMPARE(everything.value().recordCount, 2);
    QCOMPARE(everything.value().byType.size(), 2);
}

void TestSummary::recordCsvFields()
{
    ExamRecord record =
        makeRecord(ExamType::Oi, 7, QStringLiteral("初赛, \"A\" 组"), QDate(2025, 9, 20), 15250, 40000);
    record.subjectName = QStringLiteral("NOIP");
    record.rank = 12;
    record.participants = 300;
    record.note = QStringLiteral("备注");

    const QString csv = ScoreService::recordsToCsv({record});
    const QStringList lines = csv.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    QCOMPARE(lines.size(), 2);
    QCOMPARE(lines.at(0),
             QStringLiteral("考试类型,科目,考试名称,考试日期,得分,满分,得分率,排名,参考人数,备注"));
    QVERIFY2(lines.at(1).contains(QStringLiteral("OI 竞赛,NOIP,")), qPrintable(lines.at(1)));
    QVERIFY2(lines.at(1).contains(QStringLiteral("\"初赛, \"\"A\"\" 组\"")), qPrintable(lines.at(1)));
    QVERIFY2(lines.at(1).contains(QStringLiteral("2025-09-20,152.5,400,38.1%,12,300,备注")),
             qPrintable(lines.at(1)));
}

void TestSummary::summaryCsvSections()
{
    const Subject noip = makeSubject(2, QStringLiteral("NOIP"), ExamType::Oi);
    const Subject math = makeSubject(1, QStringLiteral("数学"), ExamType::Academic);
    QVector<ExamRecord> records = {
        makeRecord(ExamType::Academic, 1, QStringLiteral("期中"), QDate(2025, 3, 10), 9000, 10000),
        makeRecord(ExamType::Oi, 2, QStringLiteral("初赛"), QDate(2025, 9, 20), 15250, 40000),
    };

    const ScoreSummary summary =
        buildScoreSummary(records, {noip, math}, resolveTimeRange(TimeRangeKind::All, {}, {}));
    const QString csv = ScoreService::summaryToCsv(summary);

    QVERIFY(csv.contains(QStringLiteral("汇总范围,全部时间")));
    QVERIFY(csv.contains(QStringLiteral("记录数,2")));
    QVERIFY(csv.contains(QStringLiteral("按科目汇总")));
    QVERIFY(csv.contains(QStringLiteral("按考试类型汇总")));
    QVERIFY(csv.contains(QStringLiteral("OI 竞赛,NOIP,1,152.5,38.1%,152.5,152.5,2025-09-20,初赛,152.5")));
    QVERIFY(csv.contains(QStringLiteral("文化课,数学,1,90,90.0%,90,90,2025-03-10,期中,90")));
    QVERIFY(csv.endsWith(QStringLiteral("\r\n")));
}

QTEST_GUILESS_MAIN(TestSummary)

#include "tst_summary.moc"
