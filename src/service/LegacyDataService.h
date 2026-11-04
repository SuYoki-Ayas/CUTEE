#pragma once

#include <QString>

#include "core/Result.h"

namespace cutee {

class Database;

struct LegacyStats
{
    int students = 0;
    int questions = 0;
    int exams = 0;
    int examQuestions = 0;
    int examStudents = 0;
    int answers = 0;
    int submissions = 0;
    int scores = 0;

    bool any() const;
    int totalRows() const;
    QString summaryText() const;
};

/// M1-M3 tables are kept read-only after the pivot to single-student score
/// management. This service reports their size and offers an explicit cleanup.
class LegacyDataService
{
public:
    explicit LegacyDataService(Database &database);

    Result<LegacyStats> stats() const;

    /// Deletes rows from the legacy tables only, inside one transaction.
    /// subjects / exam_records / student_profile are never touched.
    Result<void> clear();

    static QString explanation();

private:
    Database &m_database;
};

} // namespace cutee
