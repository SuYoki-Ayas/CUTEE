#pragma once

#include <QVector>
#include <QWidget>

#include "core/Models.h"
#include "core/ScoreSummary.h"

class QChart;
class QChartView;
class QComboBox;
class QLabel;
class QStackedWidget;

namespace cutee {

/// Score-rate trend for a single subject, drawn with Qt Charts (QChartView, so
/// no QML is involved). The page feeds it the records already fetched for the
/// summary, so switching subjects never re-queries the database.
class TrendChart : public QWidget
{
    Q_OBJECT

public:
    explicit TrendChart(QWidget *parent = nullptr);

    void setData(const QVector<ExamRecord> &records, const QVector<SubjectAggregate> &subjects);
    void clear();

private:
    void updateSubjectChoices();
    void updateChart();

    QComboBox *m_subject = nullptr;
    QChartView *m_view = nullptr;
    QChart *m_chart = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_emptyHint = nullptr;

    QVector<ExamRecord> m_records;
    QVector<SubjectAggregate> m_subjects;
};

} // namespace cutee
