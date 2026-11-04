#include "ui/ScorePage.h"

#include "core/CsvExport.h"
#include "core/Logging.h"
#include "core/ScoreValue.h"
#include "service/ScoreService.h"
#include "storage/Database.h"
#include "ui/ScoreDialog.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

namespace cutee {
namespace {

constexpr int kIdRole = Qt::UserRole + 1;

QTableWidgetItem *readOnlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setToolTip(text);
    return item;
}

} // namespace

ScorePage::ScorePage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_service(database)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("成绩管理"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("记录这位学生的每次 OI 竞赛与文化课考试成绩。得分率 = 得分 ÷ 满分，"
                       "列表默认按考试日期倒序。"),
        this);
    description->setWordWrap(true);
    description->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(description);

    auto *toolbar = new QWidget(this);
    auto *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(8);

    m_typeFilter = new QComboBox(toolbar);
    m_typeFilter->addItem(QStringLiteral("全部类型"), -1);
    m_typeFilter->addItem(examTypeDisplayName(ExamType::Oi), static_cast<int>(ExamType::Oi));
    m_typeFilter->addItem(examTypeDisplayName(ExamType::Academic),
                          static_cast<int>(ExamType::Academic));

    m_subjectFilter = new QComboBox(toolbar);
    m_subjectFilter->setMinimumWidth(150);

    m_dateFilter = new QCheckBox(QStringLiteral("按日期筛选"), toolbar);
    m_from = new QDateEdit(toolbar);
    m_from->setCalendarPopup(true);
    m_from->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_from->setDate(QDate::currentDate().addYears(-1));
    m_to = new QDateEdit(toolbar);
    m_to->setCalendarPopup(true);
    m_to->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_to->setDate(QDate::currentDate());
    m_from->setEnabled(false);
    m_to->setEnabled(false);

    m_keyword = new QLineEdit(toolbar);
    m_keyword->setPlaceholderText(QStringLiteral("搜索考试名称 / 科目 / 备注"));
    m_keyword->setClearButtonEnabled(true);
    m_keyword->setMinimumWidth(200);

    auto *addButton = new QPushButton(QStringLiteral("新增成绩"), toolbar);
    m_editButton = new QPushButton(QStringLiteral("编辑"), toolbar);
    m_deleteButton = new QPushButton(QStringLiteral("删除"), toolbar);
    m_exportButton = new QPushButton(QStringLiteral("导出明细 CSV"), toolbar);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), toolbar);

    for (QPushButton *button : {m_editButton, m_deleteButton, m_exportButton, refreshButton})
        button->setAutoDefault(false);

    toolbarLayout->addWidget(m_typeFilter);
    toolbarLayout->addWidget(m_subjectFilter);
    toolbarLayout->addWidget(m_dateFilter);
    toolbarLayout->addWidget(m_from);
    toolbarLayout->addWidget(m_to);
    toolbarLayout->addWidget(m_keyword);
    toolbarLayout->addWidget(addButton);
    toolbarLayout->addWidget(m_editButton);
    toolbarLayout->addWidget(m_deleteButton);
    toolbarLayout->addWidget(m_exportButton);
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addStretch(1);
    layout->addWidget(toolbar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(10);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("日期"), QStringLiteral("类型"), QStringLiteral("科目"),
         QStringLiteral("考试名称"), QStringLiteral("得分"), QStringLiteral("满分"),
         QStringLiteral("得分率"), QStringLiteral("排名"), QStringLiteral("参考人数"),
         QStringLiteral("备注")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    for (int column = 4; column <= 8; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    layout->addWidget(m_table, 1);

    m_summary = new QLabel(this);
    m_summary->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(m_summary);

    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ScorePage::reload);
    connect(m_subjectFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ScorePage::reload);
    connect(m_dateFilter, &QCheckBox::toggled, this, [this](bool checked) {
        m_from->setEnabled(checked);
        m_to->setEnabled(checked);
        reload();
    });
    connect(m_from, &QDateEdit::dateChanged, this, [this] {
        if (m_dateFilter->isChecked())
            reload();
    });
    connect(m_to, &QDateEdit::dateChanged, this, [this] {
        if (m_dateFilter->isChecked())
            reload();
    });
    connect(m_keyword, &QLineEdit::textChanged, this, &ScorePage::reload);
    connect(addButton, &QPushButton::clicked, this, &ScorePage::createRecord);
    connect(m_editButton, &QPushButton::clicked, this, &ScorePage::editSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &ScorePage::removeRecord);
    connect(m_exportButton, &QPushButton::clicked, this, &ScorePage::exportCsv);
    connect(refreshButton, &QPushButton::clicked, this, [this] {
        reloadSubjects();
        reload();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ScorePage::updateActions);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->row() >= 0)
            editSelected();
    });

    reloadSubjects();
    reload();
    updateActions();
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
    ExamRecordFilter filter;
    const int typeValue = m_typeFilter->currentData().toInt();
    if (typeValue >= 0)
        filter.type = static_cast<ExamType>(typeValue);
    filter.subjectId = m_subjectFilter->currentData().isValid()
        ? m_subjectFilter->currentData().toLongLong()
        : 0;
    if (m_dateFilter->isChecked()) {
        filter.from = qMin(m_from->date(), m_to->date());
        filter.to = qMax(m_from->date(), m_to->date());
    }
    filter.keyword = m_keyword->text();

    auto result = m_service.list(filter);
    if (!result) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), result.errorMessage());
        return;
    }

    m_records = result.value();
    m_table->setRowCount(m_records.size());
    for (int row = 0; row < m_records.size(); ++row) {
        const ExamRecord &record = m_records.at(row);
        auto *scoreItem = readOnlyItem(scoreutil::formatPoints(record.scoreHundredths));
        scoreItem->setData(kIdRole, record.id);
        scoreItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        const QStringList values = {
            record.examDate.toString(QStringLiteral("yyyy-MM-dd")),
            examTypeDisplayName(record.examType),
            record.subjectName,
            record.title,
            scoreutil::formatPoints(record.scoreHundredths),
            scoreutil::formatPoints(record.fullScoreHundredths),
            scoreutil::formatRate(record.scoreHundredths, record.fullScoreHundredths),
            record.rank ? QString::number(*record.rank) : QString(),
            record.participants ? QString::number(*record.participants) : QString(),
            record.note,
        };
        for (int column = 0; column < values.size(); ++column) {
            if (column == 4) {
                m_table->setItem(row, column, scoreItem);
                continue;
            }
            auto *item = readOnlyItem(values.at(column));
            if (column >= 4 && column <= 8)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(row, column, item);
        }
        if (record.scoreHundredths > record.fullScoreHundredths) {
            m_table->item(row, 6)->setForeground(QColor(QStringLiteral("#b54708")));
            m_table->item(row, 6)->setToolTip(QStringLiteral("得分高于满分"));
        }
    }

    updateSummary();
    updateActions();
}

void ScorePage::updateSummary()
{
    if (m_records.isEmpty()) {
        auto choices = m_service.subjectChoices(std::nullopt, true);
        const bool hasSubjects = choices.ok() && !choices.value().isEmpty();
        m_summary->setText(hasSubjects
                               ? QStringLiteral("没有符合条件的成绩记录。")
                               : QStringLiteral("暂无成绩记录。请先在“科目管理”中添加科目，再新增成绩。"));
        return;
    }

    int oiCount = 0;
    int academicCount = 0;
    for (const ExamRecord &record : m_records) {
        if (record.examType == ExamType::Oi)
            ++oiCount;
        else
            ++academicCount;
    }
    m_summary->setText(QStringLiteral("共 %1 条成绩 · OI 竞赛 %2 条 · 文化课 %3 条")
                           .arg(m_records.size())
                           .arg(oiCount)
                           .arg(academicCount));
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
}

void ScorePage::editSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;

    auto subjects = m_service.subjectChoices(std::nullopt, true);
    if (!subjects) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), subjects.errorMessage());
        return;
    }

    ScoreDialog dialog(&m_records.at(row), subjects.value(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const ExamRecord record = dialog.record();
    auto updated = m_service.update(record);
    if (!updated) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), updated.errorMessage());
        return;
    }
    reload();
}

void ScorePage::removeRecord()
{
    const int row = selectedRow();
    if (row < 0)
        return;

    const ExamRecord record = m_records.at(row);
    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认删除成绩"),
        QStringLiteral("确定删除这条成绩记录吗？\n\n%1 · %2 · %3\n得分 %4 / %5\n\n该操作不可撤销。")
            .arg(record.examDate.toString(QStringLiteral("yyyy-MM-dd")), record.subjectName,
                 record.title, scoreutil::formatPoints(record.scoreHundredths),
                 scoreutil::formatPoints(record.fullScoreHundredths)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
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
    if (m_records.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("无法导出"),
                                 QStringLiteral("当前筛选条件下没有成绩记录。"));
        return;
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (directory.isEmpty())
        directory = QDir::homePath();
    const QString suggested = QStringLiteral("%1/成绩明细_%2.csv")
                                  .arg(directory, QDate::currentDate().toString(QStringLiteral("yyyyMMdd")));    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出成绩明细"), suggested, QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;

    auto written = csvutil::writeFile(path, ScoreService::recordsToCsv(m_records));
    if (!written) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), written.errorMessage());
        return;
    }

    qInfo().noquote() << QStringLiteral("导出成绩明细 %1 条：%2").arg(m_records.size()).arg(path);
    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("已导出 %1 条成绩到：\n%2")
                                 .arg(m_records.size())
                                 .arg(QDir::toNativeSeparators(path)));
}

int ScorePage::selectedRow() const
{
    const auto selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return -1;
    const int row = selection.first().row();
    return row < m_records.size() ? row : -1;
}

void ScorePage::updateActions()
{
    const bool hasSelection = selectedRow() >= 0;
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
    m_exportButton->setEnabled(!m_records.isEmpty());
}

} // namespace cutee
