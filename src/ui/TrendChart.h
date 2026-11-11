#pragma once

#include <QDate>
#include <QVector>
#include <QWidget>

#include "core/Models.h"
#include "core/ScoreSummary.h"

class QComboBox;
class QLabel;
class QQmlComponent;
class QQuickWidget;
class QStackedWidget;

namespace cutee {

/// One plotted point of the trend chart: the exam date and the score rate as a
/// percentage (0–100).
struct TrendPoint
{
    QDate date;
    double ratePercent = 0.0;
};

/// Score-rate trend for a single subject, drawn with Qt Graphs (a GraphsView
/// hosted in a QQuickWidget). The page feeds it the records already fetched for
/// the summary, so switching subjects never re-queries the database.
class TrendChart : public QWidget
{
    Q_OBJECT

public:
    explicit TrendChart(QWidget *parent = nullptr);

    void setData(const QVector<ExamRecord> &records, const QVector<SubjectAggregate> &subjects);
    void clear();

    /// Currently plotted points of the selected subject (also used by tests).
    QVector<TrendPoint> currentPoints() const { return m_points; }
    QString currentTitle() const { return m_title; }

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updateSubjectChoices();
    void updateChart();
    void ensureChartLoaded();
    void applyToChart();

    QComboBox *m_subject = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_chartHost = nullptr;
    QWidget *m_chartArea = nullptr;
    QLabel *m_chartTitle = nullptr;
    QQuickWidget *m_view = nullptr;
    QQmlComponent *m_component = nullptr;
    QLabel *m_emptyHint = nullptr;

    QVector<ExamRecord> m_records;
    QVector<SubjectAggregate> m_subjects;
    QVector<TrendPoint> m_points;
    QString m_title;
};

} // namespace cutee
