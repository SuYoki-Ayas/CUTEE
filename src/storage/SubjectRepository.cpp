#include "storage/SubjectRepository.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/DbUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cutee {
namespace {

Subject subjectFromQuery(const QSqlQuery &query)
{
    Subject subject;
    subject.id = query.value(QStringLiteral("id")).toLongLong();
    subject.name = query.value(QStringLiteral("name")).toString();
    examTypeFromStorage(query.value(QStringLiteral("type")).toString(), &subject.type);
    subject.sortOrder = query.value(QStringLiteral("sort_order")).toInt();
    subject.enabled = query.value(QStringLiteral("enabled")).toInt() != 0;
    subject.note = query.value(QStringLiteral("note")).toString();
    subject.createdAt = timeutil::fromIso(query.value(QStringLiteral("created_at")).toString());
    subject.updatedAt = timeutil::fromIso(query.value(QStringLiteral("updated_at")).toString());
    return subject;
}

constexpr char kSelectColumns[] =
    "id, name, type, sort_order, enabled, note, created_at, updated_at";

Error friendlyError(const Error &error, const QString &context)
{
    const QString message = error.message();
    if (message.contains(QLatin1String("subjects.type"), Qt::CaseInsensitive)
        || message.contains(QLatin1String("UNIQUE"), Qt::CaseInsensitive))
        return Error(QStringLiteral("同类型下已存在同名科目"), context);
    if (message.contains(QLatin1String("FOREIGN KEY"), Qt::CaseInsensitive))
        return Error(QStringLiteral("该科目已被成绩引用，无法删除"), context);
    return Error(message, context);
}

} // namespace

SubjectRepository::SubjectRepository(Database &database)
    : m_database(database)
{
}

Result<QVector<Subject>> SubjectRepository::list(std::optional<ExamType> type,
                                                 bool includeDisabled) const
{
    QString sql = QStringLiteral("SELECT %1 FROM subjects").arg(QString::fromLatin1(kSelectColumns));
    QStringList conditions;
    QVariantList bindings;

    if (type.has_value()) {
        conditions.append(QStringLiteral("type = ?"));
        bindings.append(examTypeToStorage(*type));
    }
    if (!includeDisabled)
        conditions.append(QStringLiteral("enabled = 1"));
    if (!conditions.isEmpty())
        sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    sql += QStringLiteral(
        " ORDER BY CASE type WHEN 'OI' THEN 0 ELSE 1 END, sort_order, name COLLATE NOCASE");

    QSqlQuery query(m_database.connection());
    if (!query.prepare(sql))
        return Error(query.lastError().text(), QStringLiteral("SubjectRepository::list"));
    for (const QVariant &value : bindings)
        query.addBindValue(value);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("SubjectRepository::list"));

    QVector<Subject> subjects;
    while (query.next())
        subjects.append(subjectFromQuery(query));
    return subjects;
}

Result<Subject> SubjectRepository::find(qint64 id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT %1 FROM subjects WHERE id = ?")
                      .arg(QString::fromLatin1(kSelectColumns)));
    query.addBindValue(id);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("SubjectRepository::find"));
    if (!query.next())
        return Error(QStringLiteral("科目不存在或已被删除"), QStringLiteral("SubjectRepository::find"));
    return subjectFromQuery(query);
}

Result<void> SubjectRepository::create(Subject &subject)
{
    const QString name = subject.name.trimmed();
    if (name.isEmpty())
        return Error(QStringLiteral("科目名称不能为空"), QStringLiteral("SubjectRepository::create"));

    if (subject.sortOrder <= 0) {
        auto next = nextSortOrder(subject.type);
        if (!next)
            return next.error();
        subject.sortOrder = next.value();
    }

    const QDateTime now = timeutil::nowUtc();
    subject.name = name;
    subject.createdAt = now;
    subject.updatedAt = now;

    auto result = db::execute(
        m_database.connection(),
        QStringLiteral("INSERT INTO subjects (name, type, sort_order, enabled, note, created_at, updated_at) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?)"),
        {subject.name, examTypeToStorage(subject.type), subject.sortOrder, subject.enabled ? 1 : 0,
         subject.note.trimmed(), timeutil::toIso(now), timeutil::toIso(now)});
    if (!result)
        return friendlyError(result.error(), QStringLiteral("SubjectRepository::create"));

    QSqlQuery idQuery(m_database.connection());
    if (!idQuery.exec(QStringLiteral("SELECT last_insert_rowid()")) || !idQuery.next())
        return Error(idQuery.lastError().text(), QStringLiteral("SubjectRepository::create"));
    subject.id = idQuery.value(0).toLongLong();
    return {};
}

Result<void> SubjectRepository::update(const Subject &subject)
{
    if (subject.id <= 0)
        return Error(QStringLiteral("缺少科目 ID"), QStringLiteral("SubjectRepository::update"));
    if (subject.name.trimmed().isEmpty())
        return Error(QStringLiteral("科目名称不能为空"), QStringLiteral("SubjectRepository::update"));

    auto existing = find(subject.id);
    if (!existing)
        return existing.error();
    if (existing.value().type != subject.type)
        return Error(QStringLiteral("科目类型不可修改，如需更换类型请新建科目"),
                     QStringLiteral("SubjectRepository::update"));

    auto result = db::execute(
        m_database.connection(),
        QStringLiteral("UPDATE subjects SET name = ?, sort_order = ?, enabled = ?, note = ?, "
                       "updated_at = ? WHERE id = ?"),
        {subject.name.trimmed(), subject.sortOrder, subject.enabled ? 1 : 0,
         subject.note.trimmed(), timeutil::toIso(timeutil::nowUtc()), subject.id});
    if (!result)
        return friendlyError(result.error(), QStringLiteral("SubjectRepository::update"));
    return {};
}

Result<void> SubjectRepository::remove(qint64 id)
{
    auto references = referenceCount(id);
    if (!references)
        return references.error();
    if (references.value() > 0) {
        return Error(QStringLiteral("该科目已被 %1 条成绩引用，不能删除。请改用“停用”，"
                                    "历史成绩仍可正常显示。")
                         .arg(references.value()),
                     QStringLiteral("SubjectRepository::remove"));
    }

    auto result = db::execute(m_database.connection(),
                              QStringLiteral("DELETE FROM subjects WHERE id = ?"), {id});
    if (!result)
        return friendlyError(result.error(), QStringLiteral("SubjectRepository::remove"));
    return {};
}

Result<void> SubjectRepository::setEnabled(qint64 id, bool enabled)
{
    auto result = db::execute(
        m_database.connection(),
        QStringLiteral("UPDATE subjects SET enabled = ?, updated_at = ? WHERE id = ?"),
        {enabled ? 1 : 0, timeutil::toIso(timeutil::nowUtc()), id});
    if (!result)
        return Error(result.error().message(), QStringLiteral("SubjectRepository::setEnabled"));
    return {};
}

Result<void> SubjectRepository::move(qint64 id, int offset)
{
    auto subject = find(id);
    if (!subject)
        return subject.error();

    auto siblings = listOfType(subject.value().type);
    if (!siblings)
        return siblings.error();

    QVector<Subject> ordered = siblings.value();
    int index = -1;
    for (int i = 0; i < ordered.size(); ++i) {
        if (ordered.at(i).id == id) {
            index = i;
            break;
        }
    }
    const int target = index + offset;
    if (index < 0 || target < 0 || target >= ordered.size())
        return {};

    ordered.move(index, target);

    QSqlDatabase db = m_database.connection();
    if (!db.transaction())
        return Error(db.lastError().text(), QStringLiteral("SubjectRepository::move"));

    const QString now = timeutil::toIso(timeutil::nowUtc());
    for (int i = 0; i < ordered.size(); ++i) {
        if (ordered.at(i).sortOrder == i + 1)
            continue;
        auto updated = db::execute(db,
                                   QStringLiteral("UPDATE subjects SET sort_order = ? WHERE id = ?"),
                                   {i + 1, ordered.at(i).id});
        if (!updated) {
            db.rollback();
            return Error(updated.error().message(), QStringLiteral("SubjectRepository::move"));
        }
    }

    if (!db.commit()) {
        const QString message = db.lastError().text();
        db.rollback();
        return Error(message, QStringLiteral("SubjectRepository::move"));
    }
    return {};
}

Result<int> SubjectRepository::referenceCount(qint64 id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM exam_records WHERE subject_id = ?"));
    query.addBindValue(id);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("SubjectRepository::referenceCount"));
    if (!query.next())
        return 0;
    return query.value(0).toInt();
}

Result<int> SubjectRepository::nextSortOrder(ExamType type) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT COALESCE(MAX(sort_order), 0) + 1 FROM subjects WHERE type = ?"));
    query.addBindValue(examTypeToStorage(type));
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("SubjectRepository::nextSortOrder"));
    if (!query.next())
        return 1;
    return query.value(0).toInt();
}

Result<QVector<Subject>> SubjectRepository::listOfType(ExamType type) const
{
    return list(type, true);
}

} // namespace cutee
