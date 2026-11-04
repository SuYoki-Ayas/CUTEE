#pragma once

#include <QDate>
#include <QVector>

#include <optional>

#include "core/Models.h"
#include "core/Result.h"

namespace cutee {

class Database;

struct ExamRecordFilter
{
    std::optional<ExamType> type;
    qint64 subjectId = 0;
    QDate from;
    QDate to;
    QString keyword;
    bool newestFirst = true;

    bool hasDateRange() const { return from.isValid() || to.isValid(); }
};

class ExamRecordRepository
{
public:
    explicit ExamRecordRepository(Database &database);

    Result<QVector<ExamRecord>> list(const ExamRecordFilter &filter = {}) const;
    Result<ExamRecord> find(qint64 id) const;
    Result<void> create(ExamRecord &record);
    Result<void> update(const ExamRecord &record);
    Result<void> remove(qint64 id);

    /// Records inside `range`; used by the summary so filtering only happens once.
    Result<QVector<ExamRecord>> listInRange(const QDate &from, const QDate &to) const;

private:
    Database &m_database;
};

} // namespace cutee
