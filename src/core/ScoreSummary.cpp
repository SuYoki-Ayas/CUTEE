#include "core/ScoreSummary.h"

#include "core/ScoreValue.h"

#include <QHash>
#include <QMap>

#include <algorithm>
#include <climits>
#include <cmath>

namespace cutee {
namespace {

AggregateBucket bucketFor(const QVector<ExamRecord> &records)
{
    AggregateBucket bucket;
    if (records.isEmpty())
        return bucket;

    bucket.examCount = records.size();
    qint64 sum = 0;
    double rateSum = 0.0;
    qint64 latestId = -1;
    bucket.bestHundredths = records.first().scoreHundredths;
    bucket.worstHundredths = records.first().scoreHundredths;

    for (const ExamRecord &record : records) {
        sum += record.scoreHundredths;
        rateSum += scoreutil::rateOf(record.scoreHundredths, record.fullScoreHundredths);

        bucket.bestHundredths = qMax(bucket.bestHundredths, record.scoreHundredths);
        bucket.worstHundredths = qMin(bucket.worstHundredths, record.scoreHundredths);

        const bool newer = !bucket.latestDate.isValid() || record.examDate > bucket.latestDate
            || (record.examDate == bucket.latestDate && record.id > latestId);
        if (newer) {
            bucket.latestDate = record.examDate;
            bucket.latestHundredths = record.scoreHundredths;
            bucket.latestTitle = record.title;
            latestId = record.id;
        }
    }

    bucket.averageHundredths = (sum * 2 + bucket.examCount) / (2 * bucket.examCount);
    bucket.averageRate = rateSum / bucket.examCount;
    return bucket;
}

} // namespace

bool DateRange::contains(const QDate &date) const
{
    if (isAll())
        return true;
    if (!date.isValid())
        return false;
    if (from.isValid() && date < from)
        return false;
    if (to.isValid() && date > to)
        return false;
    return true;
}

QString DateRange::displayText() const
{
    if (isAll())
        return QStringLiteral("全部时间");
    return QStringLiteral("%1 至 %2").arg(from.toString(QStringLiteral("yyyy-MM-dd")),
                                         to.toString(QStringLiteral("yyyy-MM-dd")));
}

DateRange resolveTimeRange(TimeRangeKind kind, const QDate &customFrom, const QDate &customTo,
                           const QDate &today)
{
    switch (kind) {
    case TimeRangeKind::All:
        return {};
    case TimeRangeKind::Semester: {
        const int year = today.year();
        if (today.month() >= 9)
            return {QDate(year, 9, 1), QDate(year + 1, 1, 31)};
        if (today.month() == 1)
            return {QDate(year - 1, 9, 1), QDate(year, 1, 31)};
        return {QDate(year, 2, 1), QDate(year, 8, 31)};
    }
    case TimeRangeKind::Custom: {
        if (!customFrom.isValid() || !customTo.isValid())
            return {};
        if (customFrom <= customTo)
            return {customFrom, customTo};
        return {customTo, customFrom};
    }
    }
    return {};
}

ScoreSummary buildScoreSummary(const QVector<ExamRecord> &records,
                               const QVector<Subject> &subjects, const DateRange &range)
{
    ScoreSummary summary;
    summary.range = range;

    QHash<qint64, Subject> subjectById;
    QHash<qint64, int> subjectOrder;
    for (int i = 0; i < subjects.size(); ++i) {
        subjectById.insert(subjects.at(i).id, subjects.at(i));
        subjectOrder.insert(subjects.at(i).id, i);
    }

    QMap<qint64, QVector<ExamRecord>> bySubjectRecords;
    QMap<ExamType, QVector<ExamRecord>> byTypeRecords;

    for (const ExamRecord &record : records) {
        if (!range.contains(record.examDate))
            continue;
        bySubjectRecords[record.subjectId].append(record);
        byTypeRecords[record.examType].append(record);
        ++summary.recordCount;
    }

    for (auto it = bySubjectRecords.constBegin(); it != bySubjectRecords.constEnd(); ++it) {
        SubjectAggregate aggregate;
        aggregate.subjectId = it.key();
        aggregate.stats = bucketFor(it.value());

        if (subjectById.contains(it.key())) {
            const Subject subject = subjectById.value(it.key());
            aggregate.subjectName = subject.name;
            aggregate.type = subject.type;
        } else {
            aggregate.subjectName = it.value().first().subjectName;
            aggregate.type = it.value().first().examType;
        }
        summary.bySubject.append(aggregate);
    }

    std::sort(summary.bySubject.begin(), summary.bySubject.end(),
              [&subjectOrder](const SubjectAggregate &lhs, const SubjectAggregate &rhs) {
                  const int lhsType = lhs.type == ExamType::Oi ? 0 : 1;
                  const int rhsType = rhs.type == ExamType::Oi ? 0 : 1;
                  if (lhsType != rhsType)
                      return lhsType < rhsType;
                  const int lhsOrder = subjectOrder.value(lhs.subjectId, INT_MAX);
                  const int rhsOrder = subjectOrder.value(rhs.subjectId, INT_MAX);
                  if (lhsOrder != rhsOrder)
                      return lhsOrder < rhsOrder;
                  return lhs.subjectName < rhs.subjectName;
              });

    for (ExamType type : {ExamType::Oi, ExamType::Academic}) {
        const auto it = byTypeRecords.constFind(type);
        if (it == byTypeRecords.constEnd() || it.value().isEmpty())
            continue;
        TypeAggregate aggregate;
        aggregate.type = type;
        aggregate.stats = bucketFor(it.value());
        summary.byType.append(aggregate);
    }

    return summary;
}

} // namespace cutee
