#include "ui/SummaryPage.h"

#include "core/CsvExport.h"
#include "core/Logging.h"
#include "core/ScoreValue.h"
#include "service/ScoreService.h"
#include "storage/Database.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

namespace cutee {
namespace {

QTableWidgetItem *readOnlyItem(const QString &text, Qt::Alignment alignment = Qt::AlignLeft)
{
    auto *item = new QTableWidgetItem(text);
    item->setToolTip(text);
    item->setTextAlignment(alignment | Qt::AlignVCenter);
    return item;
}

QTableWidget *makeSummaryTable(const QStringList &headers, QWidget *parent)
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(headers.size() - 2, QHeaderView::Stretch);
    return table;
}

void fillBucketRow(QTableWidget *table, int row, const AggregateBucket &stats)
{
    const QList<QTableWidgetItem *> items = {
        readOnlyItem(QString::number(stats.examCount), Qt::AlignRight),
        readOnlyItem(scoreutil::formatPoints(stats.averageHundredths), Qt::AlignRight),
        readOnlyItem(scoreutil::formatRate(stats.averageRate), Qt::AlignRight),
        readOnlyItem(scoreutil::formatPoints(stats.bestHundredths), Qt::AlignRight),
        readOnlyItem(scoreutil::formatPoints(stats.worstHundredths), Qt::AlignRight),
        readOnlyItem(stats.latestDate.toString(QStringLiteral("yyyy-MM-dd"))),
        readOnlyItem(stats.latestTitle),
        readOnlyItem(scoreutil::formatPoints(stats.latestHundredths), Qt::AlignRight),
    };
    for (int column = 0; column < items.size(); ++column)
        table->setItem(row, column + 2, items.at(column));
}

} // namespace

SummaryPage::SummaryPage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_service(database)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("成绩汇总"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("按科目和考试类型分别统计。OI 竞赛与文化课分数不可比，因此始终分开计算。"
                       "“本学期”指 2 月 1 日至 8 月 31 日，或 9 月 1 日至次年 1 月 31 日。"),
        this);
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(description);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    m_rangeKind = new QComboBox(toolbar);
    m_rangeKind->addItem(QStringLiteral("全部时间"), static_cast<int>(TimeRangeKind::All));
    m_rangeKind->addItem(QStringLiteral("本学期"), static_cast<int>(TimeRangeKind::Semester));
    m_rangeKind->addItem(QStringLiteral("自定义"), static_cast<int>(TimeRangeKind::Custom));

    m_from = new QDateEdit(toolbar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to = new QDateEdit(toolbar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));

    const DateRange semester = resolveTimeRange(TimeRangeKind::Semester, {}, {});
    m_from->setDate(semester.from);
    m_to->setDate(semester.to);
    m_from->setEnabled(false);
    m_to->setEnabled(false);

    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), toolbar);
    auto *exportButton = new QPushButton(QStringLiteral("导出汇总 CSV"), toolbar);
    refreshButton->setAutoDefault(false);
    exportButton->setAutoDefault(false);

    toolbarLayout->addWidget(new QLabel(QStringLiteral("时间范围"), toolbar));
    toolbarLayout->addWidget(m_rangeKind);
    toolbarLayout->addWidget(m_from);
    toolbarLayout->addWidget(new QLabel(QStringLiteral("至"), toolbar));
    toolbarLayout->addWidget(m_to);
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addWidget(exportButton);
    toolbarLayout->addStretch(1);
    layout->addWidget(toolbar);

    auto *subjectLabel = new QLabel(QStringLiteral("按科目汇总"), this);
    QFont sectionFont = subjectLabel->font();
    sectionFont.setBold(true);
    subjectLabel->setFont(sectionFont);
    layout->addWidget(subjectLabel);

    m_subjectTable = makeSummaryTable(
        {QStringLiteral("科目类型"), QStringLiteral("科目"), QStringLiteral("考试次数"),
         QStringLiteral("平均得分"), QStringLiteral("平均得分率"), QStringLiteral("最高分"),
         QStringLiteral("最低分"), QStringLiteral("最近考试日期"),
         QStringLiteral("最近考试名称"), QStringLiteral("最近一次成绩")},
        this);
    layout->addWidget(m_subjectTable, 3);

    auto *typeLabel = new QLabel(QStringLiteral("按考试类型汇总"), this);
    typeLabel->setFont(sectionFont);
    layout->addWidget(typeLabel);

    m_typeTable = makeSummaryTable(
        {QStringLiteral("考试类型"), QStringLiteral("备注"), QStringLiteral("考试次数"),
         QStringLiteral("平均得分"), QStringLiteral("平均得分率"), QStringLiteral("最高分"),
         QStringLiteral("最低分"), QStringLiteral("最近考试日期"),
         QStringLiteral("最近考试名称"), QStringLiteral("最近一次成绩")},
        this);
    m_typeTable->setMaximumHeight(150);
    layout->addWidget(m_typeTable, 1);

    m_summary = new QLabel(this);
    m_summary->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(m_summary);

    connect(m_rangeKind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        const bool custom = static_cast<TimeRangeKind>(m_rangeKind->currentData().toInt())
            == TimeRangeKind::Custom;
        m_from->setEnabled(custom);
        m_to->setEnabled(custom);
        reload();
    });
    connect(m_from, &QDateEdit::dateChanged, this, [this] {
        if (m_from->isEnabled())
            reload();
    });
    connect(m_to, &QDateEdit::dateChanged, this, [this] {
        if (m_to->isEnabled())
            reload();
    });
    connect(refreshButton, &QPushButton::clicked, this, &SummaryPage::reload);
    connect(exportButton, &QPushButton::clicked, this, &SummaryPage::exportCsv);

    reload();
}

void SummaryPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();
}

DateRange SummaryPage::currentRange() const
{
    const auto kind = static_cast<TimeRangeKind>(m_rangeKind->currentData().toInt());
    return resolveTimeRange(kind, m_from->date(), m_to->date());
}

void SummaryPage::reload()
{
    const DateRange range = currentRange();
    auto summarized = m_service.summarize(range);
    if (!summarized) {
        QMessageBox::critical(this, QStringLiteral("统计失败"), summarized.errorMessage());
        return;
    }

    m_lastSummary = summarized.value();

    m_subjectTable->setRowCount(m_lastSummary.bySubject.size());
    for (int row = 0; row < m_lastSummary.bySubject.size(); ++row) {
        const SubjectAggregate &item = m_lastSummary.bySubject.at(row);
        m_subjectTable->setItem(row, 0, readOnlyItem(examTypeDisplayName(item.type)));
        m_subjectTable->setItem(row, 1, readOnlyItem(item.subjectName));
        fillBucketRow(m_subjectTable, row, item.stats);
    }

    m_typeTable->setRowCount(m_lastSummary.byType.size());
    for (int row = 0; row < m_lastSummary.byType.size(); ++row) {
        const TypeAggregate &item = m_lastSummary.byType.at(row);
        m_typeTable->setItem(row, 0, readOnlyItem(examTypeDisplayName(item.type)));
        m_typeTable->setItem(
            row, 1,
            readOnlyItem(item.type == ExamType::Oi ? QStringLiteral("算法竞赛成绩，满分因赛事而异")
                                                   : QStringLiteral("校内文化课成绩")));
        fillBucketRow(m_typeTable, row, item.stats);
    }

    if (m_lastSummary.recordCount == 0) {
        m_summary->setText(QStringLiteral("%1 · 暂无成绩记录。可先在“成绩管理”中添加成绩。")
                               .arg(range.displayText()));
    } else {
        m_summary->setText(QStringLiteral("%1 · 共 %2 条成绩 · %3 个科目")
                               .arg(range.displayText())
                               .arg(m_lastSummary.recordCount)
                               .arg(m_lastSummary.bySubject.size()));
    }
}

void SummaryPage::exportCsv()
{
    if (m_lastSummary.recordCount == 0) {
        QMessageBox::information(this, QStringLiteral("无法导出"),
                                 QStringLiteral("当前时间范围内没有成绩记录。"));
        return;
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty())
        directory = QDir::homePath();
    const QString suggested = QStringLiteral("%1/成绩汇总_%2.csv")
                                  .arg(directory, QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出成绩汇总"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    auto written = csvutil::writeFile(path, ScoreService::summaryToCsv(m_lastSummary));
    if (!written) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), written.errorMessage());
        return;
    }

    qInfo().noquote() << QStringLiteral("导出成绩汇总：%1").arg(path);
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("汇总已导出到：\n%1").arg(QDir::toNativeSeparators(path)));
}

} // namespace cutee
