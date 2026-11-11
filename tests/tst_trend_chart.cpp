#include <QtTest>

#include <QComboBox>
#include <QStackedWidget>

#include "core/ScoreSummary.h"
#include "ui/TrendChart.h"

using namespace cutee;

class TrendChartTest : public QObject
{
    Q_OBJECT

private slots:
    void plotsOnePointPerRecord();
    void sortsPointsByDate();
    void showsEmptyStateWithoutRecords();
    void switchesSubjectWithoutRequerying();
};

namespace {

ExamRecord makeRecord(qint64 id, qint64 subjectId, const QDate &date, qint64 score,
                      qint64 fullScore)
{
    ExamRecord record;
    record.id = id;
    record.subjectId = subjectId;
    record.examDate = date;
    record.scoreHundredths = score;
    record.fullScoreHundredths = fullScore;
    return record;
}

SubjectAggregate makeSubject(qint64 id, const QString &name)
{
    SubjectAggregate subject;
    subject.subjectId = id;
    subject.subjectName = name;
    subject.type = ExamType::Academic;
    return subject;
}

} // namespace

void TrendChartTest::plotsOnePointPerRecord()
{
    TrendChart chart;
    const QVector<ExamRecord> records = {
        makeRecord(1, 7, QDate(2026, 1, 10), 8000, 10000),
        makeRecord(2, 7, QDate(2026, 3, 10), 9000, 10000),
    };
    chart.setData(records, {makeSubject(7, QStringLiteral("数学"))});

    const QVector<TrendPoint> points = chart.currentPoints();
    QCOMPARE(points.size(), 2);
    QCOMPARE(points.at(0).ratePercent, 80.0);
    QCOMPARE(points.at(1).ratePercent, 90.0);
    QVERIFY(chart.currentTitle().contains(QStringLiteral("数学")));

    auto *stack = chart.findChild<QStackedWidget *>(QStringLiteral("trendStack"));
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 0);
}

void TrendChartTest::sortsPointsByDate()
{
    TrendChart chart;
    const QVector<ExamRecord> records = {
        makeRecord(2, 7, QDate(2026, 5, 1), 6000, 10000),
        makeRecord(1, 7, QDate(2026, 2, 1), 7000, 10000),
    };
    chart.setData(records, {makeSubject(7, QStringLiteral("数学"))});

    const QVector<TrendPoint> points = chart.currentPoints();
    QCOMPARE(points.size(), 2);
    QCOMPARE(points.at(0).date, QDate(2026, 2, 1));
    QCOMPARE(points.at(1).date, QDate(2026, 5, 1));
}

void TrendChartTest::showsEmptyStateWithoutRecords()
{
    TrendChart chart;
    chart.setData({}, {});

    auto *stack = chart.findChild<QStackedWidget *>(QStringLiteral("trendStack"));
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 1);
    QVERIFY(chart.currentPoints().isEmpty());
}

void TrendChartTest::switchesSubjectWithoutRequerying()
{
    TrendChart chart;
    const QVector<ExamRecord> records = {
        makeRecord(1, 7, QDate(2026, 1, 10), 8000, 10000),
        makeRecord(2, 9, QDate(2026, 2, 10), 5000, 10000),
        makeRecord(3, 9, QDate(2026, 3, 10), 10000, 10000),
    };
    chart.setData(records,
                  {makeSubject(7, QStringLiteral("数学")), makeSubject(9, QStringLiteral("英语"))});

    QCOMPARE(chart.currentPoints().size(), 1);
    QVERIFY(chart.currentTitle().contains(QStringLiteral("数学")));

    auto *subject = chart.findChild<QComboBox *>(QStringLiteral("trendSubject"));
    QVERIFY(subject);
    subject->setCurrentIndex(1);

    QCOMPARE(chart.currentPoints().size(), 2);
    QVERIFY(chart.currentTitle().contains(QStringLiteral("英语")));
}

QTEST_MAIN(TrendChartTest)
#include "tst_trend_chart.moc"
