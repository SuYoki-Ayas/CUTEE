#include "ui/ScoreDialog.h"

#include "core/ScoreValue.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cutee {
namespace {

QLineEdit *makePointsEdit(const QString &text, QWidget *parent)
{
    auto *edit = new QLineEdit(text, parent);
    auto *validator = new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^\\d{0,6}(\\.\\d{0,2})?$")), edit);
    edit->setValidator(validator);
    edit->setPlaceholderText(QStringLiteral("例如：85 或 152.5"));
    return edit;
}

} // namespace

ScoreDialog::ScoreDialog(const ExamRecord *existing, const QVector<Subject> &subjects,
                         QWidget *parent)
    : QDialog(parent)
    , m_subjects(subjects)
{
    setWindowTitle(existing ? QStringLiteral("编辑成绩") : QStringLiteral("新增成绩"));
    setMinimumWidth(480);

    if (existing)
        m_record = *existing;

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    auto *form = new QFormLayout();
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_type = new QComboBox(this);
    m_type->addItem(examTypeDisplayName(ExamType::Oi), static_cast<int>(ExamType::Oi));
    m_type->addItem(examTypeDisplayName(ExamType::Academic), static_cast<int>(ExamType::Academic));
    m_type->setCurrentIndex(m_record.examType == ExamType::Oi ? 0 : 1);

    m_subject = new QComboBox(this);

    m_title = new QLineEdit(m_record.title, this);
    m_title->setPlaceholderText(QStringLiteral("例如：2025 CSP-J 第一轮、期中考试"));

    m_date = new QDateEdit(this);
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_date->setDate(m_record.examDate.isValid() ? m_record.examDate : QDate::currentDate());

    m_score = makePointsEdit(scoreutil::formatPoints(m_record.scoreHundredths), this);
    m_fullScore = makePointsEdit(scoreutil::formatPoints(m_record.fullScoreHundredths), this);
    if (!existing) {
        m_score->setText(QString());
        m_fullScore->setText(QStringLiteral("100"));
    }

    m_rank = new QSpinBox(this);
    m_rank->setRange(0, 999999);
    m_rank->setSpecialValueText(QStringLiteral("未填写"));
    m_rank->setValue(m_record.rank.value_or(0));

    m_participants = new QSpinBox(this);
    m_participants->setRange(0, 999999);
    m_participants->setSpecialValueText(QStringLiteral("未填写"));
    m_participants->setValue(m_record.participants.value_or(0));

    m_note = new QPlainTextEdit(m_record.note, this);
    m_note->setFixedHeight(80);
    m_note->setPlaceholderText(QStringLiteral("可选，例如：失分原因、题目难度"));

    form->addRow(QStringLiteral("考试类型 *"), m_type);
    form->addRow(QStringLiteral("科目 *"), m_subject);
    form->addRow(QStringLiteral("考试名称 *"), m_title);
    form->addRow(QStringLiteral("考试日期 *"), m_date);
    form->addRow(QStringLiteral("得分 *"), m_score);
    form->addRow(QStringLiteral("满分 *"), m_fullScore);
    form->addRow(QStringLiteral("排名"), m_rank);
    form->addRow(QStringLiteral("参考人数"), m_participants);
    form->addRow(QStringLiteral("备注"), m_note);

    auto *hint = ui::makeHintLabel(
        QStringLiteral("得分与满分按两位小数精确保存。"), this);
    form->addRow(QString(), hint);

    layout->addLayout(form);

    m_error = ui::makeErrorLabel(this);
    m_error->setVisible(false);
    layout->addWidget(m_error);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    ui::applyRole(buttons->button(QDialogButtonBox::Save), "primary");
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ScoreDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ScoreDialog::reject);
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ScoreDialog::refreshSubjects);

    connect(m_title, &QLineEdit::textEdited, this, [this] { refreshValidation(true); });
    connect(m_score, &QLineEdit::textEdited, this, [this] { refreshValidation(true); });
    connect(m_fullScore, &QLineEdit::textEdited, this, [this] { refreshValidation(true); });
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { refreshValidation(true); });
    connect(m_subject, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { refreshValidation(true); });

    refreshSubjects();
    refreshValidation(false);
    m_title->setFocus();
}

void ScoreDialog::refreshSubjects()
{
    const auto type = static_cast<ExamType>(m_type->currentData().toInt());
    const qint64 currentId = m_subject->currentData().isValid()
        ? m_subject->currentData().toLongLong()
        : m_record.subjectId;

    m_subject->clear();
    for (const Subject &subject : m_subjects) {
        if (subject.type != type)
            continue;
        // Disabled subjects are only offered when editing a record that already
        // refers to them, so history stays editable.
        if (!subject.enabled && subject.id != m_record.subjectId)
            continue;
        const QString label = subject.enabled ? subject.name
                                              : QStringLiteral("%1（已停用）").arg(subject.name);
        m_subject->addItem(label, subject.id);
    }

    if (m_subject->count() == 0)
        m_subject->addItem(QStringLiteral("暂无可用科目，请先在“科目管理”中添加"), QVariant());

    const int index = m_subject->findData(currentId);
    if (index >= 0)
        m_subject->setCurrentIndex(index);
}

void ScoreDialog::refreshValidation(bool showErrors)
{
    if (!showErrors) {
        markInvalid(m_subject, false);
        markInvalid(m_title, false);
        markInvalid(m_score, false);
        markInvalid(m_fullScore, false);
        m_error->clear();
        m_error->setVisible(false);
        return;
    }

    const QString message = validationMessage();
    markInvalid(m_subject,
                !m_subject->currentData().isValid() || m_subject->currentData().toLongLong() <= 0);
    markInvalid(m_title, m_title->text().trimmed().isEmpty());
    markInvalid(m_score, !scoreutil::parsePoints(m_score->text()));
    const auto fullScore = scoreutil::parsePoints(m_fullScore->text());
    markInvalid(m_fullScore, !fullScore || *fullScore <= 0);

    if (!message.isEmpty()) {
        ui::applyRole(m_error, "error");
        m_error->setText(message);
        m_error->setVisible(true);
        return;
    }

    const auto score = scoreutil::parsePoints(m_score->text());
    if (score && fullScore && *fullScore > 0 && *score > *fullScore) {
        ui::applyRole(m_error, "warning");
        m_error->setText(QStringLiteral("得分高于满分，保存时需再次确认。"));
        m_error->setVisible(true);
        return;
    }

    m_error->clear();
    m_error->setVisible(false);
}

QString ScoreDialog::validationMessage() const
{
    if (!m_subject->currentData().isValid() || m_subject->currentData().toLongLong() <= 0)
        return QStringLiteral("请选择一个可用的科目。");
    if (m_title->text().trimmed().isEmpty())
        return QStringLiteral("考试名称不能为空。");
    if (!m_date->date().isValid())
        return QStringLiteral("请选择有效的考试日期。");

    const auto fullScore = scoreutil::parsePoints(m_fullScore->text());
    if (!fullScore || *fullScore <= 0)
        return QStringLiteral("满分必须是大于 0 的数字，最多两位小数。");
    if (!scoreutil::parsePoints(m_score->text()))
        return QStringLiteral("得分必须是不小于 0 的数字，最多两位小数。");
    return {};
}

void ScoreDialog::markInvalid(QWidget *field, bool invalid)
{
    if (invalid)
        ui::applyRole(field, "invalid");
    else
        ui::clearRole(field);
}

void ScoreDialog::accept()
{
    refreshValidation(true);
    if (!validationMessage().isEmpty()) {
        if (!m_subject->currentData().isValid() || m_subject->currentData().toLongLong() <= 0)
            m_subject->setFocus();
        else if (m_title->text().trimmed().isEmpty())
            m_title->setFocus();
        else if (!scoreutil::parsePoints(m_fullScore->text()))
            m_fullScore->setFocus();
        else
            m_score->setFocus();
        return;
    }

    const qint64 subjectId = m_subject->currentData().toLongLong();
    const qint64 score = *scoreutil::parsePoints(m_score->text());
    const qint64 fullScore = *scoreutil::parsePoints(m_fullScore->text());

    if (score > fullScore) {
        const auto answer = QMessageBox::warning(
            this, QStringLiteral("得分高于满分"),
            QStringLiteral("得分 %1 高于满分 %2。\n\n确定要按此内容保存吗？")
                .arg(scoreutil::formatPoints(score), scoreutil::formatPoints(fullScore)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    m_record.examType = static_cast<ExamType>(m_type->currentData().toInt());
    m_record.subjectId = subjectId;
    m_record.subjectName.clear();
    for (const Subject &subject : m_subjects) {
        if (subject.id == subjectId) {
            m_record.subjectName = subject.name;
            break;
        }
    }
    m_record.title = m_title->text().trimmed();
    m_record.examDate = m_date->date();
    m_record.scoreHundredths = score;
    m_record.fullScoreHundredths = fullScore;
    m_record.rank = m_rank->value() > 0 ? std::optional<int>(m_rank->value()) : std::nullopt;
    m_record.participants = m_participants->value() > 0
        ? std::optional<int>(m_participants->value())
        : std::nullopt;
    m_record.note = m_note->toPlainText().trimmed();

    QDialog::accept();
}

ExamRecord ScoreDialog::record() const
{
    return m_record;
}

} // namespace cutee
