#include "service/ScoreService.h"

#include "core/CsvExport.h"
#include "core/Logging.h"
#include "core/ScoreValue.h"
#include "storage/Database.h"
#include "storage/SubjectRepository.h"

#include <QDate>

namespace cutee {
namespace {

constexpr qint64 kMaxPointsHundredths = 99999 * scoreutil::kHundredthsPerPoint + 99;

QString rankText(const std::optional<int> &rank)
{
    return rank ? QString::number(*rank) : QString();
}

} // namespace

ScoreService::ScoreService(Database &database)
    : m_records(database)
    , m_subjects(database)
{
}

RecordValidation ScoreService::validate(const ExamRecord &record, bool isNew) const
{
    RecordValidation validation;

    if (record.title.trimmed().isEmpty())
        validation.error = Error(QStringLiteral("考试名称不能为空"), QStringLiteral("ScoreService::validate"));
    else if (record.subjectId <= 0)
        validation.error = Error(QStringLiteral("请选择科目"), QStringLiteral("ScoreService::validate"));
    else if (!record.examDate.isValid())
        validation.error = Error(QStringLiteral("考试日期无效"), QStringLiteral("ScoreService::validate"));
    else if (record.fullScoreHundredths <= 0)
        validation.error =
            Error(QStringLiteral("满分必须大于 0"), QStringLiteral("ScoreService::validate"));
    else if (record.scoreHundredths < 0)
        validation.error =
            Error(QStringLiteral("得分不能为负数"), QStringLiteral("ScoreService::validate"));
    else if (record.fullScoreHundredths > kMaxPointsHundredths
             || record.scoreHundredths > kMaxPointsHundredths)
        validation.error = Error(QStringLiteral("分数超出支持范围（最大 99999.99）"),
                                 QStringLiteral("ScoreService::validate"));
    else if (record.participants && *record.participants <= 0)
        validation.error = Error(QStringLiteral("参考人数必须大于 0"),
                                 QStringLiteral("ScoreService::validate"));
    else if (record.rank && *record.rank <= 0)
        validation.error =
            Error(QStringLiteral("排名必须大于 0"), QStringLiteral("ScoreService::validate"));
    else if (record.rank && record.participants && *record.rank > *record.participants)
        validation.error = Error(QStringLiteral("排名不能大于参考人数"),
                                 QStringLiteral("ScoreService::validate"));

    if (!validation.ok())
        return validation;

    auto subject = m_subjects.find(record.subjectId);
    if (!subject) {
        validation.error = Error(subject.error().message(), QStringLiteral("ScoreService::validate"));
        return validation;
    }
    if (subject.value().type != record.examType) {
        validation.error = Error(QStringLiteral("科目「%1」属于%2，与所选考试类型不一致")
                                     .arg(subject.value().name,
                                          examTypeDisplayName(subject.value().type)),
                                 QStringLiteral("ScoreService::validate"));
        return validation;
    }
    if (isNew && !subject.value().enabled) {
        validation.error = Error(QStringLiteral("科目「%1」已停用，不能用于新成绩")
                                     .arg(subject.value().name),
                                 QStringLiteral("ScoreService::validate"));
        return validation;
    }

    if (record.scoreHundredths > record.fullScoreHundredths) {
        validation.warnings.append(
            QStringLiteral("得分 %1 高于满分 %2，请确认是否填写正确。")
                .arg(scoreutil::formatPoints(record.scoreHundredths),
                     scoreutil::formatPoints(record.fullScoreHundredths)));
    }
    return validation;
}

Result<void> ScoreService::create(ExamRecord &record)
{
    const RecordValidation validation = validate(record, true);
    if (!validation.ok())
        return validation.error;

    auto created = m_records.create(record);
    if (!created)
        return created.error();
    qInfo().noquote() << QStringLiteral("新增成绩记录 #%1：%2 / %3 / %4")
                             .arg(record.id)
                             .arg(examTypeDisplayName(record.examType), record.subjectName,
                                  record.title);
    return {};
}

Result<void> ScoreService::update(const ExamRecord &record)
{
    const RecordValidation validation = validate(record, false);
    if (!validation.ok())
        return validation.error;

    auto updated = m_records.update(record);
    if (!updated)
        return updated.error();
    qInfo().noquote() << QStringLiteral("更新成绩记录 #%1").arg(record.id);
    return {};
}

Result<void> ScoreService::remove(qint64 id)
{
    auto removed = m_records.remove(id);
    if (!removed)
        return removed.error();
    qInfo().noquote() << QStringLiteral("删除成绩记录 #%1").arg(id);
    return {};
}

Result<QVector<Subject>> ScoreService::subjectChoices(std::optional<ExamType> type,
                                                      bool includeDisabled) const
{
    return m_subjects.list(type, includeDisabled);
}

Result<QVector<ExamRecord>> ScoreService::list(const ExamRecordFilter &filter) const
{
    return m_records.list(filter);
}

Result<ExamRecord> ScoreService::find(qint64 id) const
{
    return m_records.find(id);
}

Result<ScoreSummary> ScoreService::summarize(const DateRange &range) const
{
    const QDate from = range.isAll() ? QDate() : range.from;
    const QDate to = range.isAll() ? QDate() : range.to;

    auto records = m_records.listInRange(from, to);
    if (!records)
        return records.error();
    auto subjects = m_subjects.list(std::nullopt, true);
    if (!subjects)
        return subjects.error();

    return buildScoreSummary(records.value(), subjects.value(), range);
}

QString ScoreService::recordsToCsv(const QVector<ExamRecord> &records)
{
    QVector<QStringList> rows;
    rows.append({QStringLiteral("考试类型"), QStringLiteral("科目"), QStringLiteral("考试名称"),
                 QStringLiteral("考试日期"), QStringLiteral("得分"), QStringLiteral("满分"),
                 QStringLiteral("得分率"), QStringLiteral("排名"), QStringLiteral("参考人数"),
                 QStringLiteral("备注")});

    for (const ExamRecord &record : records) {
        rows.append({examTypeDisplayName(record.examType), record.subjectName, record.title,
                     record.examDate.toString(Qt::ISODate),
                     scoreutil::formatPoints(record.scoreHundredths),
                     scoreutil::formatPoints(record.fullScoreHundredths),
                     scoreutil::formatRate(record.scoreHundredths, record.fullScoreHundredths),
                     rankText(record.rank),
                     record.participants ? QString::number(*record.participants) : QString(),
                     record.note});
    }
    return csvutil::toCsv(rows);
}

QString ScoreService::summaryToCsv(const ScoreSummary &summary)
{
    QVector<QStringList> rows;
    rows.append({QStringLiteral("汇总范围"), summary.range.displayText()});
    rows.append({QStringLiteral("记录数"), QString::number(summary.recordCount)});
    rows.append(QStringList());

    rows.append({QStringLiteral("按科目汇总")});
    rows.append({QStringLiteral("科目类型"), QStringLiteral("科目"), QStringLiteral("考试次数"),
                 QStringLiteral("平均得分"), QStringLiteral("平均得分率"), QStringLiteral("最高分"),
                 QStringLiteral("最低分"), QStringLiteral("最近考试日期"),
                 QStringLiteral("最近考试名称"), QStringLiteral("最近一次成绩")});
    for (const SubjectAggregate &item : summary.bySubject) {
        rows.append({examTypeDisplayName(item.type), item.subjectName,
                     QString::number(item.stats.examCount),
                     scoreutil::formatPoints(item.stats.averageHundredths),
                     scoreutil::formatRate(item.stats.averageRate),
                     scoreutil::formatPoints(item.stats.bestHundredths),
                     scoreutil::formatPoints(item.stats.worstHundredths),
                     item.stats.latestDate.toString(Qt::ISODate), item.stats.latestTitle,
                     scoreutil::formatPoints(item.stats.latestHundredths)});
    }

    rows.append(QStringList());
    rows.append({QStringLiteral("按考试类型汇总")});
    rows.append({QStringLiteral("考试类型"), QStringLiteral("考试次数"), QStringLiteral("平均得分"),
                 QStringLiteral("平均得分率"), QStringLiteral("最高分"), QStringLiteral("最低分"),
                 QStringLiteral("最近考试日期"), QStringLiteral("最近考试名称"),
                 QStringLiteral("最近一次成绩")});
    for (const TypeAggregate &item : summary.byType) {
        rows.append({examTypeDisplayName(item.type), QString::number(item.stats.examCount),
                     scoreutil::formatPoints(item.stats.averageHundredths),
                     scoreutil::formatRate(item.stats.averageRate),
                     scoreutil::formatPoints(item.stats.bestHundredths),
                     scoreutil::formatPoints(item.stats.worstHundredths),
                     item.stats.latestDate.toString(Qt::ISODate), item.stats.latestTitle,
                     scoreutil::formatPoints(item.stats.latestHundredths)});
    }

    return csvutil::toCsv(rows);
}

} // namespace cutee
