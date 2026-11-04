#include "service/LegacyDataService.h"

#include "core/Logging.h"
#include "storage/Database.h"

#include <QSqlError>
#include <QSqlQuery>

namespace cutee {
namespace {

Result<int> countRows(Database &database, const QString &table)
{
    QSqlQuery query(database.connection());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(table)))
        return Error(query.lastError().text(), QStringLiteral("LegacyDataService::stats"));
    if (!query.next())
        return 0;
    return query.value(0).toInt();
}

} // namespace

bool LegacyStats::any() const
{
    return totalRows() > 0;
}

int LegacyStats::totalRows() const
{
    return students + questions + exams + examQuestions + examStudents + answers + submissions
        + scores;
}

QString LegacyStats::summaryText() const
{
    if (!any())
        return QStringLiteral("未检测到旧版考试数据。");
    return QStringLiteral("旧表仍保留：学生 %1 · 题目 %2 · 考试 %3 · 试卷题目 %4 · 考生名单 %5 · "
                          "作答 %6 · 交卷 %7 · 旧成绩 %8（共 %9 行）")
        .arg(students)
        .arg(questions)
        .arg(exams)
        .arg(examQuestions)
        .arg(examStudents)
        .arg(answers)
        .arg(submissions)
        .arg(scores)
        .arg(totalRows());
}

LegacyDataService::LegacyDataService(Database &database)
    : m_database(database)
{
}

Result<LegacyStats> LegacyDataService::stats() const
{
    LegacyStats stats;
    const struct
    {
        const char *table;
        int *target;
    } tables[] = {
        {"students", &stats.students},
        {"questions", &stats.questions},
        {"exams", &stats.exams},
        {"exam_questions", &stats.examQuestions},
        {"exam_students", &stats.examStudents},
        {"answers", &stats.answers},
        {"submissions", &stats.submissions},
        {"scores", &stats.scores},
    };

    for (const auto &entry : tables) {
        auto count = countRows(m_database, QString::fromLatin1(entry.table));
        if (!count)
            return count.error();
        *entry.target = count.value();
    }
    return stats;
}

Result<void> LegacyDataService::clear()
{
    auto before = stats();
    if (!before)
        return before.error();
    if (!before.value().any())
        return {};

    QSqlDatabase db = m_database.connection();
    if (!db.transaction())
        return Error(db.lastError().text(), QStringLiteral("LegacyDataService::clear"));

    // Children first, then parents, so foreign keys stay satisfied.
    const QStringList order = {
        QStringLiteral("answers"),     QStringLiteral("submissions"),  QStringLiteral("scores"),
        QStringLiteral("exam_students"), QStringLiteral("exam_questions"), QStringLiteral("exams"),
        QStringLiteral("questions"),   QStringLiteral("students"),
    };
    for (const QString &table : order) {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("DELETE FROM %1").arg(table))) {
            const QString message = query.lastError().text();
            db.rollback();
            return Error(message, QStringLiteral("LegacyDataService::clear"));
        }
    }

    if (!db.commit()) {
        const QString message = db.lastError().text();
        db.rollback();
        return Error(message, QStringLiteral("LegacyDataService::clear"));
    }

    qInfo().noquote() << QStringLiteral("已清理旧版考试数据：%1").arg(before.value().summaryText());
    return {};
}

QString LegacyDataService::explanation()
{
    return QStringLiteral(
        "CUTEE 已改为“单学生成绩管理”。旧版本的 students / questions / exams / exam_questions / "
        "exam_students / answers / submissions / scores 表及其数据会原样保留，本版本不再读写它们，"
        "也不会自动删除或覆盖。新的成绩保存在 subjects / exam_records 中，与旧表没有关联。\n\n"
        "如确认不再需要旧数据，可在下方执行“清理旧考试数据”。该操作会删除上述旧表中的全部行，"
        "但不会修改学生档案、科目和新成绩记录，也无法撤销。");
}

} // namespace cutee
