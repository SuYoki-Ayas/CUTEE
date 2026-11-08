#include "core/SchoolStage.h"

#include <array>

namespace cutee {
namespace {

struct StageDescriptor
{
    SchoolStage stage;
    const char *storage;
    const char *display;
    int years;
};

constexpr std::array<StageDescriptor, 3> kStages = {{
    {SchoolStage::Primary, "PRIMARY", "小学", 6},
    {SchoolStage::Junior, "JUNIOR", "初中", 3},
    {SchoolStage::Senior, "SENIOR", "高中", 3},
}};

constexpr int kTotalYears = 12;

const StageDescriptor &descriptor(SchoolStage stage)
{
    for (const StageDescriptor &entry : kStages) {
        if (entry.stage == stage)
            return entry;
    }
    return kStages[0];
}

/// Years of schooling that precede `stage`.
int stageOffset(SchoolStage stage)
{
    int offset = 0;
    for (const StageDescriptor &entry : kStages) {
        if (entry.stage == stage)
            break;
        offset += entry.years;
    }
    return offset;
}

} // namespace

QString schoolStageToStorage(SchoolStage stage)
{
    return QString::fromLatin1(descriptor(stage).storage);
}

bool schoolStageFromStorage(const QString &value, SchoolStage *stage)
{
    for (const StageDescriptor &entry : kStages) {
        if (value.compare(QString::fromLatin1(entry.storage), Qt::CaseInsensitive) == 0) {
            if (stage)
                *stage = entry.stage;
            return true;
        }
    }
    return false;
}

QString schoolStageDisplayName(SchoolStage stage)
{
    return QString::fromUtf8(descriptor(stage).display);
}

QStringList schoolStageDisplayNames()
{
    QStringList names;
    for (const StageDescriptor &entry : kStages)
        names.append(QString::fromUtf8(entry.display));
    return names;
}

int schoolStageYearCount(SchoolStage stage)
{
    return descriptor(stage).years;
}

int schoolYearStart(const QDate &today)
{
    if (!today.isValid())
        return 0;
    return today.month() >= kSchoolYearStartMonth ? today.year() : today.year() - 1;
}

QString GradeInfo::label() const
{
    if (!known)
        return QStringLiteral("未填写学段");
    if (graduated)
        return QStringLiteral("已毕业");
    return QStringLiteral("%1·%2年级").arg(schoolStageDisplayName(stage)).arg(grade);
}

GradeInfo resolveGrade(SchoolStage entryStage, int enrollmentYear, const QDate &today)
{
    GradeInfo info;
    info.stage = entryStage;
    if (enrollmentYear <= 0 || !today.isValid())
        return info;

    info.known = true;
    const int elapsed = qMax(0, schoolYearStart(today) - enrollmentYear);
    const int index = stageOffset(entryStage) + elapsed;
    if (index >= kTotalYears) {
        info.graduated = true;
        return info;
    }

    if (index < 6)
        info.stage = SchoolStage::Primary;
    else if (index < 9)
        info.stage = SchoolStage::Junior;
    else
        info.stage = SchoolStage::Senior;

    info.grade = index - stageOffset(info.stage) + 1;
    return info;
}

} // namespace cutee
