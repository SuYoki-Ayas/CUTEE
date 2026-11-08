#include "core/Models.h"

namespace cutee {
namespace {

struct ExamTypeName
{
    ExamType type;
    const char *storage;
    const char *display;
};

constexpr ExamTypeName kExamTypes[] = {
    {ExamType::Oi, "OI", "OI 竞赛"},
    {ExamType::Academic, "ACADEMIC", "文化课"},
};

} // namespace

QString examTypeToStorage(ExamType type)
{
    for (const auto &entry : kExamTypes) {
        if (entry.type == type)
            return QString::fromLatin1(entry.storage);
    }
    return QString::fromLatin1(kExamTypes[0].storage);
}

bool examTypeFromStorage(const QString &value, ExamType *type)
{
    for (const auto &entry : kExamTypes) {
        if (value == QLatin1String(entry.storage)) {
            if (type)
                *type = entry.type;
            return true;
        }
    }
    return false;
}

QString examTypeDisplayName(ExamType type)
{
    for (const auto &entry : kExamTypes) {
        if (entry.type == type)
            return QString::fromUtf8(entry.display);
    }
    return QString::fromUtf8(kExamTypes[0].display);
}

QStringList examTypeFilterLabels()
{
    return {QStringLiteral("全部类型"), examTypeDisplayName(ExamType::Oi),
            examTypeDisplayName(ExamType::Academic)};
}

QString StudentProfile::displayName() const
{
    return name.trimmed();
}

} // namespace cutee
