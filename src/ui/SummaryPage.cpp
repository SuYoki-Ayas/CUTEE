#include "ui/SummaryPage.h"

#include "core/CsvExport.h"
#include "core/Logging.h"
#include "core/ScoreValue.h"
#include "service/ScoreService.h"
#include "storage/Database.h"
#include "ui/Palette.h"
#include "ui/TrendChart.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace cutee {
namespace {

constexpr int kRateColumn = 4;

/// Paints the average score rate as a slim bar behind the percentage text.
class RateBarDelegate : public QStyledItemDelegate
{
public:
    explicit RateBarDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QString text = opt.text;
        const QVariant rateValue = index.data(Qt::UserRole);
        opt.text.clear();
        QStyledItemDelegate::paint(painter, opt, index);

        if (!rateValue.isValid())
            return;

        const double rate = qBound(0.0, rateValue.toDouble(), 1.0);
        const QRect bar = opt.rect.adjusted(6, 8, -6, -8);
        const int fillWidth = static_cast<int>(bar.width() * rate);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(palette::color(palette::kChartTrack));
        painter->drawRoundedRect(bar, 3, 3);
        QColor fill(palette::color(palette::kChartPoint));
        fill.setAlpha(150);
        painter->setBrush(fill);
        painter->drawRoundedRect(QRect(bar.left(), bar.top(), fillWidth, bar.height()), 3, 3);

        painter->setPen(opt.palette.color(QPalette::Text));
        painter->drawText(opt.rect.adjusted(0, 0, -6, 0), Qt::AlignRight | Qt::AlignVCenter,
                          text);
        painter->restore();
    }
};

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
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->horizontalHeader()->setHighlightSections(false);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(headers.size() - 2, QHeaderView::Stretch);
    table->setItemDelegateForColumn(kRateColumn, new RateBarDelegate(table));
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
        readOnlyItem(stats.latestDate.toString(QStringLiteral("yyyy-MM-dd")), Qt::AlignCenter),
        readOnlyItem(stats.latestTitle),
        readOnlyItem(scoreutil::formatPoints(stats.latestHundredths), Qt::AlignRight),
    };
    for (int column = 0; column < items.size(); ++column) {
        table->setItem(row, column + 2, items.at(column));
        if (column + 2 == kRateColumn)
            items.at(column)->setData(Qt::UserRole, stats.averageRate);
    }
}

SummaryOutcome runSummary(const QString &databasePath, const DateRange &range)
{
    SummaryOutcome outcome;

    Database database(QStringLiteral("cutee_summary_%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    auto opened = database.open(databasePath);
    if (!opened) {
        outcome.error = opened.errorMessage();
        return outcome;
    }

    ScoreService service(database);
    auto summarized = service.summarize(range);
    if (!summarized) {
        outcome.error = summarized.errorMessage();
        return outcome;
    }

    ExamRecordFilter filter;
    if (!range.isAll()) {
        filter.from = range.from;
        filter.to = range.to;
    }
    filter.newestFirst = false;
    auto records = service.list(filter);
    if (!records) {
        outcome.error = records.errorMessage();
        return outcome;
    }

    outcome.summary = summarized.value();
    outcome.records = records.value();
    outcome.ok = true;
    return outcome;
}

} // namespace

SummaryPage::SummaryPage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_database(database)
{
    m_watcher = new QFutureWatcher<SummaryOutcome>(this);
    connect(m_watcher, &QFutureWatcher<SummaryOutcome>::finished, this, [this] {
        m_running = false;
        applyOutcome(m_watcher->result());
        if (m_pending)
            reload();
    });

    auto *layout = ui::makePageLayout(this);

    layout->addWidget(ui::makePageTitle(QStringLiteral("成绩汇总"), this));
    layout->addWidget(ui::makePageDescription(
        QStringLiteral("按科目和考试类型分别统计。OI 竞赛与文化课分数始终分开计算。"
                       "“本学期”指 2 月 1 日至 8 月 31 日，或 9 月 1 日至次年 1 月 31 日。"),
        this));
    layout->addWidget(buildFilterBar());
    layout->addWidget(buildContent(), 1);

    m_summary = ui::makeStatusLabel(this);
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

    auto *exportShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+E")), this);
    exportShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exportShortcut, &QShortcut::activated, this, &SummaryPage::exportCsv);

    reload();
}

QWidget *SummaryPage::buildFilterBar()
{
    QHBoxLayout *row = nullptr;
    QWidget *bar = ui::makeFilterBar(this, &row);

    m_rangeKind = new QComboBox(bar);
    m_rangeKind->addItem(QStringLiteral("全部时间"), static_cast<int>(TimeRangeKind::All));
    m_rangeKind->addItem(QStringLiteral("本学期"), static_cast<int>(TimeRangeKind::Semester));
    m_rangeKind->addItem(QStringLiteral("自定义"), static_cast<int>(TimeRangeKind::Custom));
    m_rangeKind->setMinimumWidth(110);

    m_from = new QDateEdit(bar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setMinimumWidth(112);
    m_to = new QDateEdit(bar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setMinimumWidth(112);

    const DateRange semester = resolveTimeRange(TimeRangeKind::Semester, {}, {});
    m_from->setDate(semester.from);
    m_to->setDate(semester.to);
    m_from->setEnabled(false);
    m_to->setEnabled(false);

    auto *refreshButton = ui::makeButton(QStringLiteral("刷新"), bar);
    m_exportButton = ui::makeButton(QStringLiteral("导出汇总 CSV"), bar);
    m_exportButton->setToolTip(QStringLiteral("导出汇总 CSV（Ctrl+E）"));

    row->addWidget(new QLabel(QStringLiteral("时间范围"), bar));
    row->addWidget(m_rangeKind);
    row->addWidget(m_from);
    row->addWidget(new QLabel(QStringLiteral("至"), bar));
    row->addWidget(m_to);
    row->addStretch(1);
    row->addWidget(refreshButton);
    row->addWidget(m_exportButton);

    connect(refreshButton, &QPushButton::clicked, this, &SummaryPage::reload);
    connect(m_exportButton, &QPushButton::clicked, this, &SummaryPage::exportCsv);

    return bar;
}

QWidget *SummaryPage::buildContent()
{
    m_content = new QStackedWidget(this);
    m_content->addWidget(buildEmptyState());

    auto *tabs = new QTabWidget(m_content);
    tabs->setDocumentMode(true);
    m_tablesPage = tabs;

    auto *summaryTab = new QWidget(tabs);
    auto *tablesLayout = new QVBoxLayout(summaryTab);
    tablesLayout->setContentsMargins(0, 8, 0, 0);
    tablesLayout->setSpacing(10);

    tablesLayout->addWidget(ui::makeSectionTitle(QStringLiteral("按科目汇总"), summaryTab));
    m_subjectTable = makeSummaryTable(
        {QStringLiteral("科目类型"), QStringLiteral("科目"), QStringLiteral("考试次数"),
         QStringLiteral("平均得分"), QStringLiteral("平均得分率"), QStringLiteral("最高分"),
         QStringLiteral("最低分"), QStringLiteral("最近考试日期"),
         QStringLiteral("最近考试名称"), QStringLiteral("最近一次成绩")},
        summaryTab);
    tablesLayout->addWidget(m_subjectTable, 3);

    tablesLayout->addWidget(ui::makeSectionTitle(QStringLiteral("按考试类型汇总"), summaryTab));
    m_typeTable = makeSummaryTable(
        {QStringLiteral("考试类型"), QStringLiteral("备注"), QStringLiteral("考试次数"),
         QStringLiteral("平均得分"), QStringLiteral("平均得分率"), QStringLiteral("最高分"),
         QStringLiteral("最低分"), QStringLiteral("最近考试日期"),
         QStringLiteral("最近考试名称"), QStringLiteral("最近一次成绩")},
        summaryTab);
    tablesLayout->addWidget(m_typeTable, 1);

    tabs->addTab(summaryTab, QStringLiteral("汇总表"));

    m_trend = new TrendChart(tabs);
    tabs->addTab(m_trend, QStringLiteral("成绩趋势"));

    m_content->addWidget(tabs);
    return m_content;
}

QWidget *SummaryPage::buildEmptyState()
{
    m_emptyPage = new QWidget(m_content);
    m_emptyPage->setObjectName(QStringLiteral("emptyState"));

    auto *layout = new QVBoxLayout(m_emptyPage);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(10);
    layout->addStretch(1);

    m_emptyTitle = new QLabel(m_emptyPage);
    m_emptyTitle->setObjectName(QStringLiteral("emptyStateTitle"));
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyTitle);

    m_emptyHint = ui::makePageDescription(QString(), m_emptyPage);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyHint);

    layout->addStretch(1);
    return m_emptyPage;
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
    if (m_running) {
        m_pending = true;
        return;
    }
    startQuery(range);
}

void SummaryPage::startQuery(const DateRange &range)
{
    m_running = true;
    m_pending = false;
    m_exportButton->setEnabled(false);
    m_summary->setText(QStringLiteral("正在统计…"));

    const QString path = m_database.filePath();
    m_watcher->setFuture(QtConcurrent::run([path, range] { return runSummary(path, range); }));
}

void SummaryPage::applyOutcome(const SummaryOutcome &outcome)
{
    if (!outcome.ok) {
        m_summary->setText(QStringLiteral("统计失败：%1").arg(outcome.error));
        if (m_trend)
            m_trend->clear();
        return;
    }

    m_lastSummary = outcome.summary;
    if (m_trend)
        m_trend->setData(outcome.records, m_lastSummary.bySubject);

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

    updateEmptyState();
    m_exportButton->setEnabled(m_lastSummary.recordCount > 0);
}

void SummaryPage::updateEmptyState()
{
    const bool empty = m_lastSummary.recordCount == 0;
    m_content->setCurrentIndex(empty ? 0 : 1);

    if (!empty) {
        m_summary->setText(QStringLiteral("%1 · 共 %2 条成绩 · %3 个科目")
                               .arg(m_lastSummary.range.displayText())
                               .arg(m_lastSummary.recordCount)
                               .arg(m_lastSummary.bySubject.size()));
        return;
    }

    if (m_lastSummary.range.isAll()) {
        m_emptyTitle->setText(QStringLiteral("还没有成绩记录"));
        m_emptyHint->setText(
            QStringLiteral("统计范围是全部时间，暂时没有任何成绩。请先到“成绩管理”录入成绩。"));
        m_summary->setText(QStringLiteral("全部时间 · 暂无成绩记录。"));
    } else {
        m_emptyTitle->setText(QStringLiteral("所选时间范围内没有成绩"));
        m_emptyHint->setText(
            QStringLiteral("尝试把“时间范围”改为全部时间，或选择包含考试日期的时间段。"));
        m_summary->setText(QStringLiteral("%1 · 没有符合条件的成绩。")
                               .arg(m_lastSummary.range.displayText()));
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
                                  .arg(directory,
                                       QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
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
                             QStringLiteral("汇总已导出到：\n%1")
                                 .arg(QDir::toNativeSeparators(path)));
}

} // namespace cutee
