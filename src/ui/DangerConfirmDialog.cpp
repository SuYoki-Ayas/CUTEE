#include "ui/DangerConfirmDialog.h"

#include "ui/UiKit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace cutee::ui {
namespace {

QLabel *bulletLabel(const QStringList &bullets, QWidget *parent)
{
    QStringList lines;
    for (const QString &bullet : bullets)
        lines.append(QStringLiteral("· %1").arg(bullet));

    auto *label = new QLabel(lines.join(QLatin1Char('\n')), parent);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setObjectName(QStringLiteral("dangerBullets"));
    return label;
}

} // namespace

DangerConfirmDialog::DangerConfirmDialog(const DangerPrompt &prompt, QWidget *parent)
    : QDialog(parent)
    , m_prompt(prompt)
{
    setObjectName(QStringLiteral("dangerConfirmDialog"));
    setWindowTitle(prompt.title);
    setModal(true);
    setMinimumWidth(520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(12);

    auto *heading = new QLabel(prompt.heading, this);
    heading->setObjectName(QStringLiteral("dangerHeading"));
    heading->setWordWrap(true);
    layout->addWidget(heading);

    if (!prompt.bullets.isEmpty())
        layout->addWidget(bulletLabel(prompt.bullets, this));

    if (!prompt.hint.isEmpty()) {
        auto *hint = ui::makeHintLabel(prompt.hint, this);
        hint->setObjectName(QStringLiteral("dangerHint"));
        layout->addWidget(hint);
    }

    auto *instruction = new QLabel(
        QStringLiteral("为确认操作，请在下框中输入「%1」：").arg(prompt.keyword), this);
    instruction->setWordWrap(true);
    layout->addWidget(instruction);

    m_edit = new QLineEdit(this);
    m_edit->setObjectName(QStringLiteral("dangerKeyword"));
    m_edit->setPlaceholderText(prompt.keyword);
    m_edit->setClearButtonEnabled(true);
    layout->addWidget(m_edit);

    m_error = ui::makeErrorLabel(this);
    m_error->setVisible(false);
    layout->addWidget(m_error);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    m_accept = ui::makeDangerButton(prompt.acceptText, this);
    m_accept->setObjectName(QStringLiteral("dangerAcceptButton"));
    m_accept->setEnabled(false);
    m_cancel = ui::makeButton(QStringLiteral("取消"), this);
    m_cancel->setObjectName(QStringLiteral("dangerCancelButton"));
    m_cancel->setDefault(true);
    buttons->addStretch(1);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_accept);
    layout->addLayout(buttons);

    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_accept, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_edit, &QLineEdit::textChanged, this, [this] { updateAcceptState(); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] {
        if (m_accept->isEnabled())
            accept();
    });

    m_edit->setFocus();
}

void DangerConfirmDialog::updateAcceptState()
{
    // Exact match on the trimmed text: typing "导入 " should not confirm.
    const bool matches = m_edit->text().trimmed() == m_prompt.keyword;
    m_accept->setEnabled(matches);
    m_error->setVisible(false);

    // Enter follows the enabled button, so it either cancels or confirms.
    m_accept->setDefault(matches);
    m_cancel->setDefault(!matches);
}

bool confirmTyped(QWidget *parent, const DangerPrompt &prompt)
{
    DangerConfirmDialog dialog(prompt, parent);
    return dialog.exec() == QDialog::Accepted;
}

} // namespace cutee::ui
