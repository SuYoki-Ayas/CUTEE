#include "storage/StudentProfileRepository.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/DbUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cutee {
namespace {

StudentProfile profileFromQuery(const QSqlQuery &query)
{
    StudentProfile profile;
    profile.name = query.value(QStringLiteral("name")).toString();
    SchoolStage stage;
    if (schoolStageFromStorage(query.value(QStringLiteral("stage")).toString(), &stage))
        profile.stage = stage;
    profile.enrollmentYear = query.value(QStringLiteral("enrollment_year")).toInt();
    profile.targetSchool = query.value(QStringLiteral("note")).toString();
    profile.createdAt = timeutil::fromIso(query.value(QStringLiteral("created_at")).toString());
    profile.updatedAt = timeutil::fromIso(query.value(QStringLiteral("updated_at")).toString());
    return profile;
}

} // namespace

StudentProfileRepository::StudentProfileRepository(Database &database)
    : m_database(database)
{
}

Result<std::optional<StudentProfile>> StudentProfileRepository::load() const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "SELECT name, stage, enrollment_year, note, created_at, updated_at "
        "FROM student_profile WHERE id = 1"));
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("StudentProfileRepository::load"));
    if (!query.next())
        return std::optional<StudentProfile>{};
    return std::optional<StudentProfile>{profileFromQuery(query)};
}

Result<bool> StudentProfileRepository::exists() const
{
    auto loaded = load();
    if (!loaded)
        return loaded.error();
    return loaded.value().has_value() && !loaded.value()->isEmpty();
}

Result<void> StudentProfileRepository::save(const StudentProfile &profile)
{
    const QString name = profile.name.trimmed();
    if (name.isEmpty())
        return Error(QStringLiteral("学生姓名不能为空"), QStringLiteral("StudentProfileRepository::save"));

    const QDateTime now = timeutil::nowUtc();
    const QString nowIso = timeutil::toIso(now);

    auto result = db::execute(
        m_database.connection(),
        QStringLiteral(
            "INSERT INTO student_profile (id, name, stage, enrollment_year, note, created_at, updated_at) "
            "VALUES (1, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(id) DO UPDATE SET "
            "  name = excluded.name,"
            "  stage = excluded.stage,"
            "  enrollment_year = excluded.enrollment_year,"
            "  note = excluded.note,"
            "  updated_at = excluded.updated_at"),
        {name, schoolStageToStorage(profile.stage), qMax(0, profile.enrollmentYear),
         profile.targetSchool.trimmed(), nowIso, nowIso});
    if (!result) {
        return Error(result.error().message(),
                     QStringLiteral("StudentProfileRepository::save"));
    }
    return {};
}

} // namespace cutee
