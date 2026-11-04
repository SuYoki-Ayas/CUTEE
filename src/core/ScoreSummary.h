#pragma once

#include <QDate>
#include <QString>
#include <QVector>

#include "core/Models.h"

namespace cutee {

enum class TimeRangeKind {
    All,
    Semester,
    Custom,
};

/// Inclusive calendar range used by the summary page.
struct DateRange
{
    QDate from;
    QDate to;

    bool isAll() const { return !from.isValid() && !to.isValid(); }
    bool contains(const QDate &date) const;
    QString displayText() const;
};

/// "本学期" = 2/1-8/31 or 9/1-1/31; the fall semester crosses the new year.
DateRange resolveTimeRange(TimeRangeKind kind, const QDate &customFrom, const QDate &customTo,
                           const QDate &today = QDate::currentDate());

struct AggregateBucket
{
    int examCount = 0;
    qint64 averageHundredths = 0;
    double averageRate = 0.0;
    qint64 bestHundredths = 0;
    qint64 worstHundredths = 0;
    QDate latestDate;
    qint64 latestHundredths = 0;
    QString latestTitle;
};

struct SubjectAggregate
{
    qint64 subjectId = 0;
    QString subjectName;
    ExamType type = ExamType::Academic;
    AggregateBucket stats;
};

struct TypeAggregate
{
    ExamType type = ExamType::Academic;
    AggregateBucket stats;
};

struct ScoreSummary
{
    DateRange range;
    int recordCount = 0;
    QVector<SubjectAggregate> bySubject;
    QVector<TypeAggregate> byType;
};

/// Pure aggregation: the UI and the tests call the same code path. OI and
/// academic records are bucketed separately and never averaged together.
ScoreSummary buildScoreSummary(const QVector<ExamRecord> &records,
                               const QVector<Subject> &subjects, const DateRange &range = {});

} // namespace cutee
