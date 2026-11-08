#include <QtTest>

#include <QStackedWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QXYSeries>

#include "core/ScoreSummary.h"
#include "ui/TrendChart.h"

using namespace cutee;

class TrendChartTest : public QObject
{
    Q_OBJECT

private slots:
    void drawsOnePointPerRecord();
    void showsEmptyStateWithoutRecords();
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

void TrendChartTest::drawsOnePointPerRecord()
{
    TrendChart chart;
    const QVector<ExamRecord> records = {
        makeRecord(1, 7, QDate(2026, 1, 10), 8000, 10000),
        makeRecord(2, 7, QDate(2026, 3, 10), 9000, 10000),
    };
    chart.setData(records, {makeSubject(7, QStringLiteral("数学"))});

    auto *view = chart.findChild<QChartView *>();
    QVERIFY(view);
    QVERIFY(view->chart());
    QCOMPARE(view->chart()->series().size(), 2);

    auto *line = qobject_cast<QXYSeries *>(view->chart()->series().first());
    QVERIFY(line);
    QCOMPARE(line->count(), 2);

    auto *stack = chart.findChild<QStackedWidget *>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 0);
}

void TrendChartTest::showsEmptyStateWithoutRecords()
{
    TrendChart chart;
    chart.setData({}, {});

    auto *stack = chart.findChild<QStackedWidget *>();
    QVERIFY(stack);
    QCOMPARE(stack->currentIndex(), 1);

    auto *view = chart.findChild<QChartView *>();
    QVERIFY(view);
    QVERIFY(view->chart());
    QVERIFY(view->chart()->series().isEmpty());
}

QTEST_MAIN(TrendChartTest)
#include "tst_trend_chart.moc"
