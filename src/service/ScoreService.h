#pragma once

#include <QStringList>
#include <QVector>

#include <optional>

#include "core/Models.h"
#include "core/Result.h"
#include "core/ScoreSummary.h"
#include "storage/ExamRecordRepository.h"
#include "storage/SubjectRepository.h"

namespace cutee {

class Database;

struct RecordValidation
{
    Error error;
    QStringList warnings;

    bool ok() const { return error.isEmpty(); }
};

/// Validation, aggregation and CSV rendering for exam records. All UI pages go
/// through this service so no SQL leaks into widgets.
class ScoreService
{
public:
    explicit ScoreService(Database &database);

    /// Checks field ranges and that the subject exists, is enabled for new
    /// records, and matches the record's exam type.
    RecordValidation validate(const ExamRecord &record, bool isNew) const;

    Result<void> create(ExamRecord &record);
    Result<void> update(const ExamRecord &record);
    Result<void> remove(qint64 id);

    Result<QVector<ExamRecord>> list(const ExamRecordFilter &filter) const;
    Result<ExamRecord> find(qint64 id) const;

    Result<QVector<Subject>> subjectChoices(std::optional<ExamType> type,
                                            bool includeDisabled = false) const;
    Result<ScoreSummary> summarize(const DateRange &range) const;

    static QString recordsToCsv(const QVector<ExamRecord> &records);
    static QString summaryToCsv(const ScoreSummary &summary);

private:
    ExamRecordRepository m_records;
    SubjectRepository m_subjects;
};

} // namespace cutee
