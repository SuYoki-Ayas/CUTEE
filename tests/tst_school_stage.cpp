#include <QtTest>

#include <QDate>

#include "core/SchoolStage.h"

using namespace cutee;

/// The grade shown in the sidebar is derived, never stored: the entry stage and
/// year are the only inputs, so a school-year rollover promotes the student
/// without editing the profile.
class TestSchoolStage : public QObject
{
    Q_OBJECT

private slots:
    void storageRoundTrip();
    void schoolYearStartsInSeptember();
    void gradeWithinStage();
    void promotionCrossesStages();
    void graduationAfterTwelveYears();
    void unknownEnrolmentYearIsReported();
};

void TestSchoolStage::storageRoundTrip()
{
    for (SchoolStage stage : {SchoolStage::Primary, SchoolStage::Junior, SchoolStage::Senior}) {
        SchoolStage parsed = SchoolStage::Senior;
        QVERIFY(schoolStageFromStorage(schoolStageToStorage(stage), &parsed));
        QCOMPARE(parsed, stage);
    }
    SchoolStage ignored = SchoolStage::Primary;
    QVERIFY(!schoolStageFromStorage(QString(), &ignored));
    QVERIFY(!schoolStageFromStorage(QStringLiteral("UNIVERSITY"), &ignored));
    QCOMPARE(schoolStageDisplayNames(),
             QStringList({QStringLiteral("小学"), QStringLiteral("初中"), QStringLiteral("高中")}));
}

void TestSchoolStage::schoolYearStartsInSeptember()
{
    QCOMPARE(schoolYearStart(QDate(2026, 8, 31)), 2025);
    QCOMPARE(schoolYearStart(QDate(2026, 9, 1)), 2026);
    QCOMPARE(schoolYearStart(QDate(2026, 12, 31)), 2026);
    QCOMPARE(schoolYearStart(QDate(2026, 1, 15)), 2025);
}

void TestSchoolStage::gradeWithinStage()
{
    // Entered 初中 in 2024: 2024-09 → 初中1, 2025-09 → 初中2, 2026-09 → 初中3.
    GradeInfo first = resolveGrade(SchoolStage::Junior, 2024, QDate(2024, 10, 1));
    QVERIFY(first.known);
    QCOMPARE(first.stage, SchoolStage::Junior);
    QCOMPARE(first.grade, 1);
    QCOMPARE(first.label(), QStringLiteral("初中·1年级"));

    GradeInfo second = resolveGrade(SchoolStage::Junior, 2024, QDate(2026, 3, 1));
    QCOMPARE(second.stage, SchoolStage::Junior);
    QCOMPARE(second.grade, 2);
    QCOMPARE(second.label(), QStringLiteral("初中·2年级"));

    GradeInfo third = resolveGrade(SchoolStage::Junior, 2024, QDate(2026, 9, 5));
    QCOMPARE(third.grade, 3);
}

void TestSchoolStage::promotionCrossesStages()
{
    // 初中3 (school year 2026) becomes 高中1 in school year 2027.
    GradeInfo senior = resolveGrade(SchoolStage::Junior, 2024, QDate(2027, 9, 10));
    QVERIFY(senior.known);
    QVERIFY(!senior.graduated);
    QCOMPARE(senior.stage, SchoolStage::Senior);
    QCOMPARE(senior.grade, 1);
    QCOMPARE(senior.label(), QStringLiteral("高中·1年级"));

    // 小学6 becomes 初中1.
    GradeInfo junior = resolveGrade(SchoolStage::Primary, 2020, QDate(2026, 10, 1));
    QCOMPARE(junior.stage, SchoolStage::Junior);
    QCOMPARE(junior.grade, 1);
}

void TestSchoolStage::graduationAfterTwelveYears()
{
    GradeInfo graduated = resolveGrade(SchoolStage::Senior, 2024, QDate(2027, 9, 10));
    QVERIFY(graduated.known);
    QVERIFY(graduated.graduated);
    QCOMPARE(graduated.label(), QStringLiteral("已毕业"));
}

void TestSchoolStage::unknownEnrolmentYearIsReported()
{
    GradeInfo unknown = resolveGrade(SchoolStage::Junior, 0, QDate(2026, 3, 1));
    QVERIFY(!unknown.known);
    QCOMPARE(unknown.label(), QStringLiteral("未填写学段"));

    // A future year is clamped to the first grade instead of going negative.
    GradeInfo future = resolveGrade(SchoolStage::Junior, 2030, QDate(2026, 3, 1));
    QCOMPARE(future.grade, 1);
    QCOMPARE(future.stage, SchoolStage::Junior);
}

QTEST_GUILESS_MAIN(TestSchoolStage)

#include "tst_school_stage.moc"
