#include "ui/ScoreDialog.h"

#include "core/ScoreValue.h"

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
    auto *form = new QFormLayout();
    form->setSpacing(10);

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

    auto *hint = new QLabel(
        QStringLiteral("得分与满分按两位小数精确保存；得分可以高于满分，但保存前会再次确认。"), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #667085;"));
    form->addRow(QString(), hint);

    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ScoreDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ScoreDialog::reject);
    connect(m_type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ScoreDialog::refreshSubjects);

    refreshSubjects();
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

void ScoreDialog::accept()
{
    const qint64 subjectId = m_subject->currentData().isValid()
        ? m_subject->currentData().toLongLong()
        : 0;
    if (subjectId <= 0) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"),
                             QStringLiteral("请选择一个可用的科目。"));
        return;
    }
    if (m_title->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"),
                             QStringLiteral("考试名称不能为空。"));
        m_title->setFocus();
        return;
    }

    const auto score = scoreutil::parsePoints(m_score->text());
    const auto fullScore = scoreutil::parsePoints(m_fullScore->text());
    if (!score) {
        QMessageBox::warning(this, QStringLiteral("分数无效"),
                             QStringLiteral("得分必须是不小于 0 的数字，最多两位小数。"));
        m_score->setFocus();
        return;
    }
    if (!fullScore || *fullScore <= 0) {
        QMessageBox::warning(this, QStringLiteral("分数无效"),
                             QStringLiteral("满分必须是大于 0 的数字，最多两位小数。"));
        m_fullScore->setFocus();
        return;
    }
    if (*score > *fullScore) {
        const auto answer = QMessageBox::warning(
            this, QStringLiteral("得分高于满分"),
            QStringLiteral("得分 %1 高于满分 %2。\n\n确定要按此内容保存吗？")
                .arg(scoreutil::formatPoints(*score), scoreutil::formatPoints(*fullScore)),
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
    m_record.scoreHundredths = *score;
    m_record.fullScoreHundredths = *fullScore;
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
