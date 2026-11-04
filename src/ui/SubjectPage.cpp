#include "ui/SubjectPage.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/SubjectRepository.h"
#include "ui/SubjectDialog.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
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

SubjectPage::SubjectPage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_repository(database)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("科目管理"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *description = new QLabel(
        QStringLiteral("OI 竞赛科目（如 CSP-J、NOIP、模拟赛）与文化课科目（如语文、数学）分开管理。"
                       "已被成绩引用的科目不能删除，只能停用。"),
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

    m_showDisabled = new QCheckBox(QStringLiteral("显示已停用"), toolbar);
    m_showDisabled->setChecked(true);

    auto *addButton = new QPushButton(QStringLiteral("新增科目"), toolbar);
    m_editButton = new QPushButton(QStringLiteral("编辑"), toolbar);
    m_deleteButton = new QPushButton(QStringLiteral("删除"), toolbar);
    m_toggleButton = new QPushButton(QStringLiteral("停用"), toolbar);
    m_upButton = new QPushButton(QStringLiteral("上移"), toolbar);
    m_downButton = new QPushButton(QStringLiteral("下移"), toolbar);
    auto *refreshButton = new QPushButton(QStringLiteral("刷新"), toolbar);

    for (QPushButton *button :
         {m_editButton, m_deleteButton, m_toggleButton, m_upButton, m_downButton, refreshButton})
        button->setAutoDefault(false);

    toolbarLayout->addWidget(m_typeFilter);
    toolbarLayout->addWidget(m_showDisabled);
    toolbarLayout->addWidget(addButton);
    toolbarLayout->addWidget(m_editButton);
    toolbarLayout->addWidget(m_deleteButton);
    toolbarLayout->addWidget(m_toggleButton);
    toolbarLayout->addWidget(m_upButton);
    toolbarLayout->addWidget(m_downButton);
    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addStretch(1);
    layout->addWidget(toolbar);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("名称"), QStringLiteral("类型"), QStringLiteral("状态"),
         QStringLiteral("成绩引用"), QStringLiteral("排序"), QStringLiteral("备注"),
         QStringLiteral("更新时间")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    layout->addWidget(m_table, 1);

    m_summary = new QLabel(this);
    m_summary->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(m_summary);

    connect(addButton, &QPushButton::clicked, this, &SubjectPage::createSubject);
    connect(m_editButton, &QPushButton::clicked, this, &SubjectPage::editSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &SubjectPage::removeSelected);
    connect(m_toggleButton, &QPushButton::clicked, this, &SubjectPage::toggleSelected);
    connect(m_upButton, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(refreshButton, &QPushButton::clicked, this, &SubjectPage::reload);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SubjectPage::reload);
    connect(m_showDisabled, &QCheckBox::toggled, this, &SubjectPage::reload);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &SubjectPage::updateActions);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
        if (item && item->row() >= 0)
            editSelected();
    });

    reload();
}

void SubjectPage::reload()
{
    std::optional<ExamType> type;
    const int typeValue = m_typeFilter->currentData().toInt();
    if (typeValue >= 0)
        type = static_cast<ExamType>(typeValue);

    auto result = m_repository.list(type, m_showDisabled->isChecked());
    if (!result) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), result.errorMessage());
        return;
    }

    m_subjects = result.value();
    m_table->setRowCount(m_subjects.size());
    for (int row = 0; row < m_subjects.size(); ++row) {
        const Subject &subject = m_subjects.at(row);
        auto *nameItem = readOnlyItem(subject.name);
        nameItem->setData(kIdRole, subject.id);
        if (!subject.enabled)
            nameItem->setForeground(QColor(QStringLiteral("#98a2b3")));
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, readOnlyItem(examTypeDisplayName(subject.type)));
        m_table->setItem(row, 2, readOnlyItem(subject.enabled ? QStringLiteral("启用")
                                                              : QStringLiteral("已停用")));
        m_table->setItem(row, 3, readOnlyItem(QStringLiteral("-")));
        m_table->setItem(row, 4, readOnlyItem(QString::number(subject.sortOrder)));
        m_table->setItem(row, 5, readOnlyItem(subject.note));
        m_table->setItem(row, 6, readOnlyItem(timeutil::formatLocal(subject.updatedAt)));
    }

    // Reference counts are per row; query them after the bulk load.
    for (int row = 0; row < m_subjects.size(); ++row) {
        auto references = m_repository.referenceCount(m_subjects.at(row).id);
        m_table->item(row, 3)->setText(references.ok() ? QString::number(references.value())
                                                       : QStringLiteral("?"));
    }

    m_summary->setText(m_subjects.isEmpty()
                           ? QStringLiteral("暂无科目，点击“新增科目”开始。")
                           : QStringLiteral("共 %1 个科目").arg(m_subjects.size()));
    updateActions();
}

void SubjectPage::createSubject()
{
    Subject subject;
    const int typeValue = m_typeFilter->currentData().toInt();
    subject.type = typeValue >= 0 ? static_cast<ExamType>(typeValue) : ExamType::Academic;
    subject.sortOrder = 0;
    subject.enabled = true;

    SubjectDialog dialog(subject, true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Subject created = dialog.subject();
    auto result = m_repository.create(created);
    if (!result) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), result.errorMessage());
        return;
    }
    reload();
}

void SubjectPage::editSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;

    const Subject &current = m_subjects.at(row);
    SubjectDialog dialog(current, false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const Subject edited = dialog.subject();
    auto result = m_repository.update(edited);
    if (!result) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), result.errorMessage());
        return;
    }
    reload();
}

void SubjectPage::removeSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;

    const Subject subject = m_subjects.at(row);
    auto references = m_repository.referenceCount(subject.id);
    if (!references) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), references.errorMessage());
        return;
    }

    if (references.value() > 0) {
        QMessageBox::information(
            this, QStringLiteral("无法删除"),
            QStringLiteral("科目「%1」已被 %2 条成绩引用，删除会破坏历史数据。\n\n"
                           "请改用“停用”：停用后该科目不再出现在新成绩录入中，历史成绩仍可查看。")
                .arg(subject.name)
                .arg(references.value()));
        return;
    }

    const auto answer = QMessageBox::question(
        this, QStringLiteral("确认删除科目"),
        QStringLiteral("确定删除科目「%1」吗？\n\n该操作不可撤销。").arg(subject.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    auto result = m_repository.remove(subject.id);
    if (!result) {
        QMessageBox::warning(this, QStringLiteral("删除失败"), result.errorMessage());
        return;
    }
    reload();
}

void SubjectPage::toggleSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;

    const Subject subject = m_subjects.at(row);
    const bool enable = !subject.enabled;
    if (!enable) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("停用科目"),
            QStringLiteral("停用科目「%1」后，它不会再出现在新成绩录入中，历史成绩不受影响。\n"
                           "确定停用吗？")
                .arg(subject.name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    auto result = m_repository.setEnabled(subject.id, enable);
    if (!result) {
        QMessageBox::warning(this, QStringLiteral("操作失败"), result.errorMessage());
        return;
    }
    reload();
}

void SubjectPage::moveSelected(int offset)
{
    const int row = selectedRow();
    if (row < 0)
        return;

    auto result = m_repository.move(m_subjects.at(row).id, offset);
    if (!result) {
        QMessageBox::warning(this, QStringLiteral("排序失败"), result.errorMessage());
        return;
    }
    reload();
}

int SubjectPage::selectedRow() const
{
    const auto selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return -1;
    const int row = selection.first().row();
    return row < m_subjects.size() ? row : -1;
}

void SubjectPage::updateActions()
{
    const int row = selectedRow();
    const bool hasSelection = row >= 0;
    m_editButton->setEnabled(hasSelection);
    m_deleteButton->setEnabled(hasSelection);
    m_toggleButton->setEnabled(hasSelection);
    m_upButton->setEnabled(hasSelection && row > 0);
    m_downButton->setEnabled(hasSelection && row >= 0 && row + 1 < m_subjects.size());

    if (hasSelection) {
        m_toggleButton->setText(m_subjects.at(row).enabled ? QStringLiteral("停用")
                                                           : QStringLiteral("启用"));
    } else {
        m_toggleButton->setText(QStringLiteral("停用"));
    }
}

} // namespace cutee
