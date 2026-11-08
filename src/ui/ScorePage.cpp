#include "ui/ScorePage.h"

#include "core/CsvExport.h"
#include "core/Logging.h"
#include "ui/ScoreTableModel.h"
#include "core/ScoreValue.h"
#include "storage/Database.h"
#include "ui/ScoreDialog.h"
#include "ui/UiKit.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <utility>

namespace cutee {
namespace {

constexpr int kKeywordDelayMs = 180;
constexpr int kRowHeight = 28;

ScoreLoadOutcome runScoreLoad(const QString &databasePath)
{
    ScoreLoadOutcome outcome;

    Database database(QStringLiteral("cutee_scores_%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    auto opened = database.open(databasePath);
    if (!opened) {
        outcome.error = opened.errorMessage();
        return outcome;
    }

    ScoreService service(database);
    auto result = service.list(ExamRecordFilter{});
    if (!result) {
        outcome.error = result.errorMessage();
        return outcome;
    }

    outcome.records = result.value();
    outcome.ok = true;
    return outcome;
}

} // namespace

ScorePage::ScorePage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_service(database)
    , m_database(database)
    , m_settings(database)
{
    m_model = new ScoreTableModel(this);
    m_proxy = new ScoreFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);

    m_watcher = new QFutureWatcher<ScoreLoadOutcome>(this);
    connect(m_watcher, &QFutureWatcher<ScoreLoadOutcome>::finished, this, [this] {
        m_loading = false;
        const ScoreLoadOutcome outcome = m_watcher->result();
        if (!outcome.ok) {
            QMessageBox::critical(this, QStringLiteral("读取失败"), outcome.error);
        } else {
            const ExamRecord current = selectedRecord();
            m_model->setRecords(outcome.records);
            if (current.id > 0)
                selectRecord(current.id);
            applyFilters();
            qDebug().noquote()
                << QStringLiteral("成绩列表刷新：%1 条").arg(outcome.records.size());
        }
        if (m_pending)
            reload();
    });

    m_keywordTimer = new QTimer(this);
    m_keywordTimer->setSingleShot(true);
    m_keywordTimer->setInterval(kKeywordDelayMs);

    auto *layout = ui::makePageLayout(this);

    layout->addWidget(ui::makePageTitle(QStringLiteral("成绩管理"), this));
    layout->addWidget(ui::makePageDescription(
        QStringLiteral("记录您的每次 OI 竞赛与文化课考试成绩，"
                       "列表默认按考试日期倒序；点击表头可切换排序。"),
        this));
    layout->addWidget(buildFilterBar());
    layout->addWidget(buildActionBar());
    layout->addWidget(buildContent(), 1);

    m_summary = ui::makeStatusLabel(this);
    layout->addWidget(m_summary);

    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { applyFilters(); });
    connect(m_subjectFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { applyFilters(); });
    connect(m_dateFilter, &QCheckBox::toggled, this, [this](bool checked) {
        m_from->setEnabled(checked);
        m_to->setEnabled(checked);
        applyFilters();
    });
    connect(m_from, &QDateEdit::dateChanged, this, [this] {
        if (m_dateFilter->isChecked())
            applyFilters();
    });
    connect(m_to, &QDateEdit::dateChanged, this, [this] {
        if (m_dateFilter->isChecked())
            applyFilters();
    });
    connect(m_keyword, &QLineEdit::textChanged, this, [this] { m_keywordTimer->start(); });
    connect(m_keywordTimer, &QTimer::timeout, this, &ScorePage::applyFilters);

    connect(m_addButton, &QPushButton::clicked, this, &ScorePage::createRecord);
    connect(m_editButton, &QPushButton::clicked, this, &ScorePage::editSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &ScorePage::removeRecord);
    connect(m_exportButton, &QPushButton::clicked, this, &ScorePage::exportCsv);
    connect(m_emptyAction, &QPushButton::clicked, this, [this] {
        if (m_model->rowCount() == 0)
            createRecord();
        else
            resetFilters();
    });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid())
            editSelected();
    });

    buildColumnsMenu();
    applyColumnVisibility();

    auto *newShortcut = new QShortcut(QKeySequence::New, this);
    newShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(newShortcut, &QShortcut::activated, this, &ScorePage::createRecord);
    auto *exportShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+E")), this);
    exportShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(exportShortcut, &QShortcut::activated, this, &ScorePage::exportCsv);
    auto *findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        m_keyword->setFocus();
        m_keyword->selectAll();
    });
    auto *editShortcut = new QShortcut(QKeySequence(Qt::Key_F2), m_table);
    editShortcut->setContext(Qt::WidgetShortcut);
    connect(editShortcut, &QShortcut::activated, this, &ScorePage::editSelected);
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, m_table);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &ScorePage::removeRecord);

    reloadSubjects();
    reload();
}

QWidget *ScorePage::buildFilterBar()
{
    QHBoxLayout *row = nullptr;
    QWidget *bar = ui::makeFilterBar(this, &row);

    m_typeFilter = new QComboBox(bar);
    m_typeFilter->addItem(QStringLiteral("全部类型"), -1);
    m_typeFilter->addItem(examTypeDisplayName(ExamType::Oi), static_cast<int>(ExamType::Oi));
    m_typeFilter->addItem(examTypeDisplayName(ExamType::Academic),
                          static_cast<int>(ExamType::Academic));
    m_typeFilter->setMinimumWidth(110);

    m_subjectFilter = new QComboBox(bar);
    m_subjectFilter->setMinimumWidth(150);

    m_dateFilter = new QCheckBox(QStringLiteral("按日期筛选"), bar);

    m_from = new QDateEdit(bar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addYears(-1));
    m_from->setMinimumWidth(112);

    m_to = new QDateEdit(bar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_to->setMinimumWidth(112);

    m_from->setEnabled(false);
    m_to->setEnabled(false);

    row->addWidget(m_typeFilter);
    row->addWidget(m_subjectFilter);
    row->addWidget(m_dateFilter);
    row->addWidget(m_from);
    row->addWidget(new QLabel(QStringLiteral("至"), bar));
    row->addWidget(m_to);
    row->addStretch(1);

    return bar;
}

QWidget *ScorePage::buildActionBar()
{
    QHBoxLayout *row = nullptr;
    QWidget *bar = ui::makeFilterBar(this, &row);

    m_keyword = new QLineEdit(bar);
    m_keyword->setPlaceholderText(QStringLiteral("搜索考试名称 / 科目 / 备注"));
    m_keyword->setClearButtonEnabled(true);
    m_keyword->setMinimumWidth(160);

    m_addButton = ui::makePrimaryButton(QStringLiteral("新增成绩"), bar);
    m_editButton = ui::makeButton(QStringLiteral("编辑"), bar);
    m_deleteButton = ui::makeDangerButton(QStringLiteral("删除"), bar);
    m_exportButton = ui::makeButton(QStringLiteral("导出明细 CSV"), bar);
    m_exportButton->setToolTip(QStringLiteral("导出明细 CSV（Ctrl+E）"));
    m_columnsButton = ui::makeButton(QStringLiteral("列"), bar);
    m_columnsButton->setToolTip(QStringLiteral("显示或隐藏表格列"));
    auto *refreshButton = ui::makeButton(QStringLiteral("刷新"), bar);

    row->addWidget(m_keyword, 1);
    row->addWidget(m_addButton);
    row->addWidget(m_editButton);
    row->addWidget(m_deleteButton);
    row->addWidget(m_columnsButton);
    row->addWidget(m_exportButton);
    row->addWidget(refreshButton);

    connect(refreshButton, &QPushButton::clicked, this, [this] {
        reloadSubjects();
        reload();
    });

    return bar;
}

QWidget *ScorePage::buildContent()
{
    m_content = new QStackedWidget(this);

    m_table = new QTableView(m_content);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setWordWrap(false);
    m_table->setTextElideMode(Qt::ElideRight);
    m_table->setSortingEnabled(true);
    m_table->setCornerButtonEnabled(false);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    auto *header = m_table->horizontalHeader();
    header->setHighlightSections(false);
    header->setSortIndicatorShown(true);
    header->setStretchLastSection(false);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(ScoreTableModel::TitleColumn, QHeaderView::Stretch);
    header->setMinimumSectionSize(56);

    const std::pair<int, int> widths[] = {
        {ScoreTableModel::DateColumn, 90},          {ScoreTableModel::TypeColumn, 70},
        {ScoreTableModel::SubjectColumn, 120},      {ScoreTableModel::ScoreColumn, 70},
        {ScoreTableModel::FullScoreColumn, 70},     {ScoreTableModel::RateColumn, 80},
        {ScoreTableModel::RankColumn, 54},          {ScoreTableModel::ParticipantsColumn, 74},
        {ScoreTableModel::NoteColumn, 150},
    };
    for (const auto &[column, width] : widths)
        header->resizeSection(column, width);

    m_table->sortByColumn(ScoreTableModel::DateColumn, Qt::DescendingOrder);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &ScorePage::updateActions);

    m_content->addWidget(m_table);
    m_emptyState = buildEmptyState();
    m_content->addWidget(m_emptyState);

    return m_content;
}

QWidget *ScorePage::buildEmptyState()
{
    auto *page = new QWidget(m_content);
    page->setObjectName(QStringLiteral("emptyState"));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(10);
    layout->addStretch(1);

    m_emptyTitle = new QLabel(page);
    m_emptyTitle->setObjectName(QStringLiteral("emptyStateTitle"));
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyTitle);

    m_emptyHint = ui::makePageDescription(QString(), page);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyHint);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    m_emptyAction = ui::makePrimaryButton(QStringLiteral("新增成绩"), page);
    buttonRow->addWidget(m_emptyAction);
    buttonRow->addStretch(1);
    layout->addSpacing(6);
    layout->addLayout(buttonRow);

    layout->addStretch(1);
    return page;
}

void ScorePage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reloadSubjects();
    reload();
}

void ScorePage::reloadSubjects()
{
    auto subjects = m_service.subjectChoices(std::nullopt, true);
    if (!subjects) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), subjects.errorMessage());
        return;
    }

    const qint64 currentId = m_subjectFilter->currentData().isValid()
        ? m_subjectFilter->currentData().toLongLong()
        : 0;

    m_subjectFilter->blockSignals(true);
    m_subjectFilter->clear();
    m_subjectFilter->addItem(QStringLiteral("全部科目"), QVariant(0));
    for (const Subject &subject : subjects.value()) {
        const QString label = QStringLiteral("[%1] %2%3")
                                  .arg(subject.type == ExamType::Oi ? QStringLiteral("OI")
                                                                    : QStringLiteral("文化课"),
                                       subject.name,
                                       subject.enabled ? QString() : QStringLiteral("（已停用）"));
        m_subjectFilter->addItem(label, subject.id);
    }
    const int index = m_subjectFilter->findData(currentId);
    m_subjectFilter->setCurrentIndex(index >= 0 ? index : 0);
    m_subjectFilter->blockSignals(false);
}

void ScorePage::reload()
{
    if (m_loading) {
        m_pending = true;
        return;
    }

    m_loading = true;
    m_pending = false;
    const QString path = m_database.filePath();
    m_watcher->setFuture(QtConcurrent::run([path] { return runScoreLoad(path); }));
}

void ScorePage::applyFilters()
{
    const int typeValue = m_typeFilter->currentData().toInt();    m_proxy->setExamType(typeValue >= 0
                             ? std::optional<ExamType>(static_cast<ExamType>(typeValue))
                             : std::nullopt);

    const QVariant subjectData = m_subjectFilter->currentData();
    m_proxy->setSubjectId(subjectData.isValid() ? subjectData.toLongLong() : 0);

    if (m_dateFilter->isChecked()) {
        m_proxy->setDateRange(qMin(m_from->date(), m_to->date()),
                              qMax(m_from->date(), m_to->date()));
    } else {
        m_proxy->setDateRange(QDate(), QDate());
    }

    m_proxy->setKeyword(m_keyword->text());

    updateSummary();
    updateActions();
    updateEmptyState();
}

void ScorePage::resetFilters()
{
    const QSignalBlocker typeBlocker(m_typeFilter);
    const QSignalBlocker subjectBlocker(m_subjectFilter);
    const QSignalBlocker dateBlocker(m_dateFilter);
    const QSignalBlocker keywordBlocker(m_keyword);

    m_typeFilter->setCurrentIndex(0);
    m_subjectFilter->setCurrentIndex(0);
    m_dateFilter->setChecked(false);
    m_keyword->clear();
    m_from->setEnabled(false);
    m_to->setEnabled(false);

    applyFilters();
}

void ScorePage::updateSummary()
{
    const int total = m_model->rowCount();
    const int shown = m_proxy->rowCount();

    if (total == 0) {
        m_summary->setText(QStringLiteral("暂无成绩记录。"));
        return;
    }

    int oiCount = 0;
    int academicCount = 0;
    for (int row = 0; row < shown; ++row) {
        const int type = m_proxy->index(row, 0).data(ScoreTableModel::ExamTypeRole).toInt();
        if (static_cast<ExamType>(type) == ExamType::Oi)
            ++oiCount;
        else
            ++academicCount;
    }

    QString text = shown == total ? QStringLiteral("共 %1 条成绩").arg(total)
                                  : QStringLiteral("显示 %1 / %2 条成绩").arg(shown).arg(total);
    if (shown > 0) {
        text += QStringLiteral(" · OI 竞赛 %1 条 · 文化课 %2 条")
                    .arg(oiCount)
                    .arg(academicCount);
    }
    m_summary->setText(text);
}

void ScorePage::updateActions()
{
    const bool selected = hasSelection();
    m_editButton->setEnabled(selected);
    m_deleteButton->setEnabled(selected);
    m_exportButton->setEnabled(m_proxy->rowCount() > 0);
}

void ScorePage::updateEmptyState()
{
    const int total = m_model->rowCount();
    const bool filteredEmpty = total > 0 && m_proxy->rowCount() == 0;

    if (total == 0) {
        m_emptyTitle->setText(QStringLiteral("还没有成绩记录"));
        m_emptyHint->setText(QStringLiteral(
            "先到“科目管理”添加科目，再点击“新增成绩”录入第一条成绩。"));
        m_emptyAction->setText(QStringLiteral("新增成绩"));
    } else if (filteredEmpty) {
        m_emptyTitle->setText(QStringLiteral("没有符合筛选条件的成绩"));
        m_emptyHint->setText(QStringLiteral("调整考试类型、科目、日期范围或关键字后再试。"));
        m_emptyAction->setText(QStringLiteral("清除筛选条件"));
    }

    m_content->setCurrentIndex(total == 0 || filteredEmpty ? 1 : 0);
}

void ScorePage::selectRecord(qint64 id)
{
    const int sourceRow = m_model->indexOfRecord(id);
    if (sourceRow < 0)
        return;

    const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(sourceRow, 0));
    if (!proxyIndex.isValid())
        return;

    m_table->selectionModel()->setCurrentIndex(
        proxyIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_table->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
}

void ScorePage::createRecord()
{
    auto subjects = m_service.subjectChoices(std::nullopt, true);
    if (!subjects) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), subjects.errorMessage());
        return;
    }
    if (subjects.value().isEmpty()) {
        QMessageBox::information(this, QStringLiteral("暂无科目"),
                                 QStringLiteral("请先在“科目管理”中添加至少一个科目，再录入成绩。"));
        return;
    }

    ScoreDialog dialog(nullptr, subjects.value(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    ExamRecord record = dialog.record();
    auto created = m_service.create(record);
    if (!created) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), created.errorMessage());
        return;
    }
    reload();
    selectRecord(record.id);
}

void ScorePage::editSelected()
{
    if (!hasSelection())
        return;

    auto subjects = m_service.subjectChoices(std::nullopt, true);
    if (!subjects) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), subjects.errorMessage());
        return;
    }

    const ExamRecord current = selectedRecord();
    ScoreDialog dialog(&current, subjects.value(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExamRecord record = dialog.record();
    auto updated = m_service.update(record);
    if (!updated) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), updated.errorMessage());
        return;
    }
    reload();
    selectRecord(record.id);
}

void ScorePage::removeRecord()
{
    if (!hasSelection())
        return;

    const ExamRecord record = selectedRecord();
    if (!ui::confirmDanger(
            this, QStringLiteral("确认删除成绩"),
            QStringLiteral("确定删除这条成绩记录吗？\n\n%1 · %2 · %3\n得分 %4 / %5\n\n该操作不可撤销。")
                .arg(record.examDate.toString(QStringLiteral("yyyy-MM-dd")), record.subjectName,
                     record.title, scoreutil::formatPoints(record.scoreHundredths),
                     scoreutil::formatPoints(record.fullScoreHundredths)),
            QStringLiteral("删除")))
        return;

    auto removed = m_service.remove(record.id);
    if (!removed) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), removed.errorMessage());
        return;
    }
    reload();
}

void ScorePage::exportCsv()
{
    const QVector<ExamRecord> records = visibleRecords();
    if (records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("无法导出"),
                                 QStringLiteral("当前筛选条件下没有成绩记录。"));
        return;
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty())
        directory = QDir::homePath();
    const QString suggested = QStringLiteral("%1/成绩明细_%2.csv")
                                  .arg(directory,
                                       QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出成绩明细"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    auto written = csvutil::writeFile(path, ScoreService::recordsToCsv(records));
    if (!written) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), written.errorMessage());
        return;
    }

    qInfo().noquote() << QStringLiteral("导出成绩明细 %1 条：%2").arg(records.size()).arg(path);
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 条成绩到：\n%2")
                                 .arg(records.size())
                                 .arg(QDir::toNativeSeparators(path)));
}

bool ScorePage::hasSelection() const
{
    return m_table && m_table->selectionModel() && m_table->selectionModel()->hasSelection();
}

ExamRecord ScorePage::selectedRecord() const
{
    if (!hasSelection())
        return {};

    const QModelIndex proxyIndex = m_table->selectionModel()->selectedRows().value(0);
    if (!proxyIndex.isValid())
        return {};

    const QModelIndex sourceIndex = m_proxy->mapToSource(proxyIndex);
    return m_model->recordAt(sourceIndex.row());
}

QVector<ExamRecord> ScorePage::visibleRecords() const
{
    QVector<ExamRecord> records;
    records.reserve(m_proxy->rowCount());
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const QModelIndex sourceIndex = m_proxy->mapToSource(m_proxy->index(row, 0));
        records.append(m_model->recordAt(sourceIndex.row()));
    }
    return records;
}

void ScorePage::buildColumnsMenu()
{
    if (!m_columnsButton)
        return;

    m_columnsMenu = new QMenu(m_columnsButton);
    for (int column = 0; column < ScoreTableModel::ColumnCount; ++column) {
        const QString label = m_model->headerData(column, Qt::Horizontal).toString();
        auto *action = m_columnsMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(true);
        connect(action, &QAction::toggled, this, [this, column](bool checked) {
            if (!m_table)
                return;
            m_table->setColumnHidden(column, !checked);
            saveColumnVisibility();
        });
        m_columnActions.append(action);
    }

    m_columnsMenu->addSeparator();
    m_columnsMenu->addAction(QStringLiteral("显示全部列"), this, [this] {
        for (int column = 0; column < m_columnActions.size(); ++column) {
            QAction *action = m_columnActions.at(column);
            if (!action->isChecked()) {
                const QSignalBlocker blocker(action);
                action->setChecked(true);
                m_table->setColumnHidden(column, false);
            }
        }
        saveColumnVisibility();
    });
    m_columnsButton->setMenu(m_columnsMenu);
}

void ScorePage::applyColumnVisibility()
{
    if (!m_table || m_columnActions.isEmpty())
        return;

    QSet<int> hidden;
    const auto stored = m_settings.value(QLatin1String(settings::kScoreHiddenColumns));
    if (stored.ok()) {
        const QStringList parts = stored.value().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            bool ok = false;
            const int column = part.trimmed().toInt(&ok);
            if (ok && column >= 0 && column < ScoreTableModel::ColumnCount)
                hidden.insert(column);
        }
    }

    for (int column = 0; column < m_columnActions.size(); ++column) {
        const bool visible = !hidden.contains(column);
        const QSignalBlocker blocker(m_columnActions.at(column));
        m_columnActions.at(column)->setChecked(visible);
        m_table->setColumnHidden(column, !visible);
    }
}

void ScorePage::saveColumnVisibility()
{
    if (!m_table)
        return;

    QStringList hidden;
    for (int column = 0; column < ScoreTableModel::ColumnCount; ++column) {
        if (m_table->isColumnHidden(column))
            hidden.append(QString::number(column));
    }
    m_settings.setValue(QLatin1String(settings::kScoreHiddenColumns), hidden.join(QLatin1Char(',')));
}

} // namespace cutee
