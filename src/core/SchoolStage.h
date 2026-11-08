#pragma once

#include <QDate>
#include <QString>
#include <QStringList>

namespace cutee {

/// Chinese school ladder. The three stages are consecutive:
/// 小学 1–6 → 初中 1–3 → 高中 1–3 (12 years in total).
enum class SchoolStage {
    Primary,
    Junior,
    Senior,
};

QString schoolStageToStorage(SchoolStage stage);
bool schoolStageFromStorage(const QString &value, SchoolStage *stage);
QString schoolStageDisplayName(SchoolStage stage);
QStringList schoolStageDisplayNames();

/// Number of grades in one stage.
int schoolStageYearCount(SchoolStage stage);

/// A new Chinese school year starts in September.
inline constexpr int kSchoolYearStartMonth = 9;

/// Year the school year containing `today` started in: September and later
/// belong to this calendar year, earlier months still belong to the previous.
int schoolYearStart(const QDate &today);

/// Grade derived from "entered `entryStage` in `enrollmentYear`" plus today's
/// date. Progression continues across stages, so a 初中 3 年级 student becomes
/// 高中 1 年级 in the next school year without editing the stored profile.
struct GradeInfo
{
    /// False when no enrolment year is stored (legacy or freshly created row).
    bool known = false;
    /// True once all 12 years have elapsed.
    bool graduated = false;
    SchoolStage stage = SchoolStage::Junior;
    int grade = 0; // 1-based within `stage`

    /// "初中·2年级" / "已毕业" / "未填写学段"
    QString label() const;
};

GradeInfo resolveGrade(SchoolStage entryStage, int enrollmentYear, const QDate &today);

} // namespace cutee
