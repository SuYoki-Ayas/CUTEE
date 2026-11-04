#include "storage/ExamRecordRepository.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/DbUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace cutee {
namespace {

constexpr char kSelectSql[] =
    "SELECT r.id, r.exam_type, r.subject_id, s.name AS subject_name, r.title, r.exam_date, "
    "r.score_hundredths, r.full_score_hundredths, r.rank, r.participants, r.note, r.detail_json, "
    "r.created_at, r.updated_at "
    "FROM exam_records r JOIN subjects s ON s.id = r.subject_id";

ExamRecord recordFromQuery(const QSqlQuery &query)
{
    ExamRecord record;
    record.id = query.value(QStringLiteral("id")).toLongLong();
    examTypeFromStorage(query.value(QStringLiteral("exam_type")).toString(), &record.examType);
    record.subjectId = query.value(QStringLiteral("subject_id")).toLongLong();
    record.subjectName = query.value(QStringLiteral("subject_name")).toString();
    record.title = query.value(QStringLiteral("title")).toString();

    const QString dateText = query.value(QStringLiteral("exam_date")).toString();
    record.examDate = QDate::fromString(dateText, Qt::ISODate);

    record.scoreHundredths = query.value(QStringLiteral("score_hundredths")).toLongLong();
    record.fullScoreHundredths = query.value(QStringLiteral("full_score_hundredths")).toLongLong();

    const QVariant rank = query.value(QStringLiteral("rank"));
    if (!rank.isNull())
        record.rank = rank.toInt();
    const QVariant participants = query.value(QStringLiteral("participants"));
    if (!participants.isNull())
        record.participants = participants.toInt();

    record.note = query.value(QStringLiteral("note")).toString();
    record.detailJson = query.value(QStringLiteral("detail_json")).toString();
    record.createdAt = timeutil::fromIso(query.value(QStringLiteral("created_at")).toString());
    record.updatedAt = timeutil::fromIso(query.value(QStringLiteral("updated_at")).toString());
    return record;
}

QString dateToStorage(const QDate &date)
{
    return date.isValid() ? date.toString(Qt::ISODate) : QString();
}

} // namespace

ExamRecordRepository::ExamRecordRepository(Database &database)
    : m_database(database)
{
}

Result<QVector<ExamRecord>> ExamRecordRepository::list(const ExamRecordFilter &filter) const
{
    QString sql = QString::fromLatin1(kSelectSql);
    QStringList conditions;
    QVariantList bindings;

    if (filter.type.has_value()) {
        conditions.append(QStringLiteral("r.exam_type = ?"));
        bindings.append(examTypeToStorage(*filter.type));
    }
    if (filter.subjectId > 0) {
        conditions.append(QStringLiteral("r.subject_id = ?"));
        bindings.append(filter.subjectId);
    }
    if (filter.from.isValid()) {
        conditions.append(QStringLiteral("r.exam_date >= ?"));
        bindings.append(dateToStorage(filter.from));
    }
    if (filter.to.isValid()) {
        conditions.append(QStringLiteral("r.exam_date <= ?"));
        bindings.append(dateToStorage(filter.to));
    }
    if (!filter.keyword.trimmed().isEmpty()) {
        conditions.append(QStringLiteral(
            "(r.title LIKE ? OR s.name LIKE ? OR r.note LIKE ?)"));
        const QString pattern = QStringLiteral("%%%1%%").arg(filter.keyword.trimmed());
        bindings.append(pattern);
        bindings.append(pattern);
        bindings.append(pattern);
    }

    if (!conditions.isEmpty())
        sql += QStringLiteral(" WHERE ") + conditions.join(QStringLiteral(" AND "));
    sql += filter.newestFirst ? QStringLiteral(" ORDER BY r.exam_date DESC, r.id DESC")
                              : QStringLiteral(" ORDER BY r.exam_date ASC, r.id ASC");

    QSqlQuery query(m_database.connection());
    if (!query.prepare(sql))
        return Error(query.lastError().text(), QStringLiteral("ExamRecordRepository::list"));
    for (const QVariant &value : bindings)
        query.addBindValue(value);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("ExamRecordRepository::list"));

    QVector<ExamRecord> records;
    while (query.next())
        records.append(recordFromQuery(query));
    return records;
}

Result<ExamRecord> ExamRecordRepository::find(qint64 id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QString::fromLatin1(kSelectSql) + QStringLiteral(" WHERE r.id = ?"));
    query.addBindValue(id);
    if (!query.exec())
        return Error(query.lastError().text(), QStringLiteral("ExamRecordRepository::find"));
    if (!query.next())
        return Error(QStringLiteral("成绩记录不存在或已被删除"),
                     QStringLiteral("ExamRecordRepository::find"));
    return recordFromQuery(query);
}

Result<void> ExamRecordRepository::create(ExamRecord &record)
{
    const QDateTime now = timeutil::nowUtc();
    const QVariant rank = record.rank ? QVariant(*record.rank) : QVariant();
    const QVariant participants = record.participants ? QVariant(*record.participants) : QVariant();

    auto result = db::execute(
        m_database.connection(),
        QStringLiteral(
            "INSERT INTO exam_records (exam_type, subject_id, title, exam_date, score_hundredths, "
            "full_score_hundredths, rank, participants, note, detail_json, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
        {examTypeToStorage(record.examType), record.subjectId, record.title.trimmed(),
         dateToStorage(record.examDate), record.scoreHundredths, record.fullScoreHundredths, rank,
         participants, record.note, record.detailJson, timeutil::toIso(now), timeutil::toIso(now)});
    if (!result)
        return Error(result.error().message(), QStringLiteral("ExamRecordRepository::create"));

    QSqlQuery idQuery(m_database.connection());
    if (!idQuery.exec(QStringLiteral("SELECT last_insert_rowid()")) || !idQuery.next())
        return Error(idQuery.lastError().text(), QStringLiteral("ExamRecordRepository::create"));
    record.id = idQuery.value(0).toLongLong();
    record.createdAt = now;
    record.updatedAt = now;
    return {};
}

Result<void> ExamRecordRepository::update(const ExamRecord &record)
{
    if (record.id <= 0)
        return Error(QStringLiteral("缺少成绩记录 ID"), QStringLiteral("ExamRecordRepository::update"));

    const QVariant rank = record.rank ? QVariant(*record.rank) : QVariant();
    const QVariant participants = record.participants ? QVariant(*record.participants) : QVariant();

    auto result = db::execute(
        m_database.connection(),
        QStringLiteral(
            "UPDATE exam_records SET exam_type = ?, subject_id = ?, title = ?, exam_date = ?, "
            "score_hundredths = ?, full_score_hundredths = ?, rank = ?, participants = ?, note = ?, "
            "detail_json = ?, updated_at = ? WHERE id = ?"),
        {examTypeToStorage(record.examType), record.subjectId, record.title.trimmed(),
         dateToStorage(record.examDate), record.scoreHundredths, record.fullScoreHundredths, rank,
         participants, record.note, record.detailJson, timeutil::toIso(timeutil::nowUtc()),
         record.id});
    if (!result)
        return Error(result.error().message(), QStringLiteral("ExamRecordRepository::update"));
    return {};
}

Result<void> ExamRecordRepository::remove(qint64 id)
{
    auto result = db::execute(m_database.connection(),
                              QStringLiteral("DELETE FROM exam_records WHERE id = ?"), {id});
    if (!result)
        return Error(result.error().message(), QStringLiteral("ExamRecordRepository::remove"));
    return {};
}

Result<QVector<ExamRecord>> ExamRecordRepository::listInRange(const QDate &from, const QDate &to) const
{
    ExamRecordFilter filter;
    filter.from = from;
    filter.to = to;
    return list(filter);
}

} // namespace cutee
