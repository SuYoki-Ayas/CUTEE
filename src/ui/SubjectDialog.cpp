#include "ui/SubjectDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cutee {

SubjectDialog::SubjectDialog(const Subject &subject, bool isNew, QWidget *parent)
    : QDialog(parent)
    , m_subject(subject)
{
    setWindowTitle(isNew ? QStringLiteral("新增科目") : QStringLiteral("编辑科目"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setSpacing(10);

    m_name = new QLineEdit(m_subject.name, this);
    m_name->setPlaceholderText(QStringLiteral("例如：NOIP、模拟赛、数学、英语"));

    m_type = new QComboBox(this);
    m_type->addItem(examTypeDisplayName(ExamType::Oi), static_cast<int>(ExamType::Oi));
    m_type->addItem(examTypeDisplayName(ExamType::Academic), static_cast<int>(ExamType::Academic));
    m_type->setCurrentIndex(m_subject.type == ExamType::Oi ? 0 : 1);
    m_type->setEnabled(isNew);

    m_sortOrder = new QSpinBox(this);
    m_sortOrder->setRange(1, 9999);
    m_sortOrder->setValue(m_subject.sortOrder > 0 ? m_subject.sortOrder : 1);

    m_enabled = new QCheckBox(QStringLiteral("启用（停用后不会出现在新成绩录入中）"), this);
    m_enabled->setChecked(m_subject.enabled);

    m_note = new QPlainTextEdit(m_subject.note, this);
    m_note->setFixedHeight(80);
    m_note->setPlaceholderText(QStringLiteral("可选，例如适用范围、考试说明"));

    form->addRow(QStringLiteral("名称 *"), m_name);
    form->addRow(QStringLiteral("类型 *"), m_type);
    form->addRow(QStringLiteral("排序"), m_sortOrder);
    form->addRow(QStringLiteral("状态"), m_enabled);
    form->addRow(QStringLiteral("备注"), m_note);

    if (!isNew) {
        auto *hint = new QLabel(QStringLiteral("科目类型创建后不可修改，如需更换类型请新建科目。"), this);
        hint->setWordWrap(true);
        hint->setStyleSheet(QStringLiteral("color: #667085;"));
        form->addRow(QString(), hint);
    }

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SubjectDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SubjectDialog::reject);

    m_name->setFocus();
}

Subject SubjectDialog::subject() const
{
    return m_subject;
}

void SubjectDialog::accept()
{
    const QString name = m_name->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"), QStringLiteral("科目名称不能为空。"));
        m_name->setFocus();
        return;
    }

    m_subject.name = name;
    m_subject.type = static_cast<ExamType>(m_type->currentData().toInt());
    m_subject.sortOrder = m_sortOrder->value();
    m_subject.enabled = m_enabled->isChecked();
    m_subject.note = m_note->toPlainText().trimmed();
    QDialog::accept();
}

} // namespace cutee
