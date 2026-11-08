#include <QtTest>
#include <QColor>

#include "core/Models.h"
#include "ui/ScoreTableModel.h"

namespace {

cutee::ExamRecord makeRecord(qint64 id, cutee::ExamType type, qint64 subjectId,
                             const QString &subjectName, const QString &title, const QDate &date,
                             qint64 score, qint64 fullScore, const QString &note = {})
{
    cutee::ExamRecord record;
    record.id = id;
    record.examType = type;
    record.subjectId = subjectId;
    record.subjectName = subjectName;
    record.title = title;
    record.examDate = date;
    record.scoreHundredths = score;
    record.fullScoreHundredths = fullScore;
    record.note = note;
    return record;
}

} // namespace

class TestScoreTableModel : public QObject
{
    Q_OBJECT

private slots:
    void exposesRowsColumnsAndHeaders()
    {
        cutee::ScoreTableModel model;
        model.setRecords({
            makeRecord(1, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("第一轮"),
                       QDate(2025, 9, 20), 8000, 10000),
        });

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.columnCount(), static_cast<int>(cutee::ScoreTableModel::ColumnCount));
        QCOMPARE(model.headerData(cutee::ScoreTableModel::DateColumn, Qt::Horizontal).toString(),
                 QStringLiteral("日期"));
        QCOMPARE(model.headerData(cutee::ScoreTableModel::RateColumn, Qt::Horizontal).toString(),
                 QStringLiteral("得分率"));
        QCOMPARE(model.data(model.index(0, cutee::ScoreTableModel::ScoreColumn)).toString(),
                 QStringLiteral("80"));
        QCOMPARE(model.data(model.index(0, cutee::ScoreTableModel::RateColumn)).toString(),
                 QStringLiteral("80.0%"));
    }

    void highlightsLowRateButNotHighRate()
    {
        cutee::ScoreTableModel model;
        model.setRecords({
            makeRecord(1, cutee::ExamType::Academic, 1, QStringLiteral("数学"), QStringLiteral("期中"),
                       QDate(2025, 11, 1), 5500, 10000),
            makeRecord(2, cutee::ExamType::Academic, 1, QStringLiteral("数学"), QStringLiteral("期末"),
                       QDate(2025, 12, 1), 6000, 10000),
            makeRecord(3, cutee::ExamType::Academic, 1, QStringLiteral("数学"), QStringLiteral("月考"),
                       QDate(2025, 10, 1), 12000, 10000),
        });

        const QModelIndex low = model.index(0, cutee::ScoreTableModel::RateColumn);
        const QModelIndex exact = model.index(1, cutee::ScoreTableModel::RateColumn);
        const QModelIndex over = model.index(2, cutee::ScoreTableModel::RateColumn);

        QCOMPARE(low.data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#6E6420")));
        QCOMPARE(exact.data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#2F2C36")));
        QCOMPARE(over.data(Qt::ForegroundRole).value<QColor>(), QColor(QStringLiteral("#8B3E3E")));
    }

    void exposesNumericSortValues()
    {
        cutee::ScoreTableModel model;
        model.setRecords({
            makeRecord(1, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("第一轮"),
                       QDate(2025, 9, 20), 15250, 40000),
        });

        const QModelIndex row = model.index(0, 0);
        QCOMPARE(model.data(row, cutee::ScoreTableModel::SortValueRole).toDate(),
                 QDate(2025, 9, 20));
        QCOMPARE(model.index(0, cutee::ScoreTableModel::ScoreColumn)
                     .data(cutee::ScoreTableModel::SortValueRole)
                     .toLongLong(),
                 qint64(15250));
        QCOMPARE(model.data(row, cutee::ScoreTableModel::RecordIdRole).toLongLong(), qint64(1));
    }

    void proxyFiltersByTypeSubjectKeywordAndDate()
    {
        cutee::ScoreTableModel model;
        model.setRecords({
            makeRecord(1, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("CSP-J 第一轮"),
                       QDate(2025, 9, 20), 8000, 10000),
            makeRecord(2, cutee::ExamType::Oi, 11, QStringLiteral("NOIP"), QStringLiteral("NOIP 模拟"),
                       QDate(2025, 10, 1), 20000, 40000, QStringLiteral("难度偏高")),
            makeRecord(3, cutee::ExamType::Academic, 1, QStringLiteral("数学"), QStringLiteral("期中考试"),
                       QDate(2025, 11, 5), 9000, 10000),
        });

        cutee::ScoreFilterProxyModel proxy;
        proxy.setSourceModel(&model);
        QCOMPARE(proxy.rowCount(), 3);

        proxy.setExamType(cutee::ExamType::Oi);
        QCOMPARE(proxy.rowCount(), 2);

        proxy.setExamType(std::nullopt);
        proxy.setSubjectId(11);
        QCOMPARE(proxy.rowCount(), 1);

        proxy.setSubjectId(0);
        proxy.setKeyword(QStringLiteral("期中"));
        QCOMPARE(proxy.rowCount(), 1);

        proxy.setKeyword(QStringLiteral("难度"));
        QCOMPARE(proxy.rowCount(), 1);

        proxy.setKeyword(QString());
        proxy.setDateRange(QDate(2025, 10, 1), QDate(2025, 10, 31));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.data(proxy.index(0, 0), cutee::ScoreTableModel::RecordIdRole).toLongLong(),
                 qint64(2));

        proxy.setDateRange(QDate(2025, 9, 20), QDate(2025, 9, 20));
        QCOMPARE(proxy.rowCount(), 1);
        QVERIFY(proxy.hasActiveFilters());
    }

    void proxySortsByDateAndRate()
    {
        cutee::ScoreTableModel model;
        model.setRecords({
            makeRecord(1, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("A"),
                       QDate(2025, 9, 20), 5000, 10000),
            makeRecord(2, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("B"),
                       QDate(2025, 11, 20), 9000, 10000),
            makeRecord(3, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("C"),
                       QDate(2025, 10, 20), 7000, 10000),
        });

        cutee::ScoreFilterProxyModel proxy;
        proxy.setSourceModel(&model);

        proxy.sort(cutee::ScoreTableModel::DateColumn, Qt::DescendingOrder);
        QCOMPARE(proxy.data(proxy.index(0, 0), cutee::ScoreTableModel::RecordIdRole).toLongLong(),
                 qint64(2));
        QCOMPARE(proxy.data(proxy.index(2, 0), cutee::ScoreTableModel::RecordIdRole).toLongLong(),
                 qint64(1));

        proxy.sort(cutee::ScoreTableModel::RateColumn, Qt::AscendingOrder);
        QCOMPARE(proxy.data(proxy.index(0, 0), cutee::ScoreTableModel::RecordIdRole).toLongLong(),
                 qint64(1));
    }

    void resettingRecordsUpdatesRowCount()
    {
        cutee::ScoreTableModel model;
        model.setRecords({});
        QCOMPARE(model.rowCount(), 0);

        model.setRecords({
            makeRecord(1, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("A"),
                       QDate(2025, 9, 20), 5000, 10000),
            makeRecord(2, cutee::ExamType::Oi, 10, QStringLiteral("CSP-J"), QStringLiteral("B"),
                       QDate(2025, 9, 21), 6000, 10000),
        });
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.indexOfRecord(2), 1);
        QVERIFY(model.recordAt(1).title == QStringLiteral("B"));
    }
};

QTEST_GUILESS_MAIN(TestScoreTableModel)

#include "tst_score_model.moc"
