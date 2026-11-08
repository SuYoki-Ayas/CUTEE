#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "core/SchoolStage.h"

namespace cutee {

/// The two score families. OI contests and academic subjects never mix in
/// aggregates because their scales are not comparable.
enum class ExamType {
    Oi,
    Academic,
};

QString examTypeToStorage(ExamType type);
bool examTypeFromStorage(const QString &value, ExamType *type);
QString examTypeDisplayName(ExamType type);
QStringList examTypeFilterLabels();

/// The single student this application manages (student_profile row id = 1).
struct StudentProfile
{
    QString name;
    /// Stage the student entered in `enrollmentYear`; the current grade is
    /// derived from it (see core/SchoolStage.h) and never stored.
    SchoolStage stage = SchoolStage::Junior;
    /// Calendar year the student started `stage`; 0 means "not filled in yet".
    int enrollmentYear = 0;
    /// 目标院校 (kept in the legacy `note` column, no schema change).
    QString targetSchool;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isEmpty() const { return name.trimmed().isEmpty(); }
    QString displayName() const;
};

struct Subject
{
    qint64 id = 0;
    QString name;
    ExamType type = ExamType::Academic;
    int sortOrder = 0;
    bool enabled = true;
    QString note;
    QDateTime createdAt;
    QDateTime updatedAt;
};

/// One exam result. Scores are stored as integer hundredths of a point so that
/// summing and averaging stay exact (no binary floating point drift).
struct ExamRecord
{
    qint64 id = 0;
    ExamType examType = ExamType::Academic;
    qint64 subjectId = 0;
    QString subjectName; // populated by repository joins, not persisted
    QString title;
    QDate examDate;
    qint64 scoreHundredths = 0;
    qint64 fullScoreHundredths = 0;
    std::optional<int> rank;
    std::optional<int> participants;
    QString note;
    QString detailJson;
    QDateTime createdAt;
    QDateTime updatedAt;
};

} // namespace cutee
