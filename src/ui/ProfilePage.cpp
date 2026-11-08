#include "ui/ProfilePage.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/StudentProfileRepository.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cutee {
namespace {

constexpr char kFirstRunBanner[] =
    "欢迎使用 CUTEE！本工具负责维护您的成绩档案，请先填写基本信息，保存后即可开始记录成绩。";
constexpr char kNormalBanner[] =
    "CUTEE负责维护您的档案和成绩。修改后点击“保存修改”，历史成绩会自动关联。";

/// Compares only the user editable fields, so timestamps do not mark the form
/// as changed.
bool isSameProfile(const StudentProfile &left, const StudentProfile &right)
{
    return left.name == right.name && left.stage == right.stage
        && left.enrollmentYear == right.enrollmentYear
        && left.targetSchool == right.targetSchool;
}

} // namespace

ProfilePage::ProfilePage(Database &database, QWidget *parent)
    : QWidget(parent)
    , m_repository(database)
{
    auto *layout = ui::makePageLayout(this);

    layout->addWidget(ui::makePageTitle(QStringLiteral("学生档案"), this));

    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    layout->addWidget(m_banner);

    auto *formWidget = new QWidget(this);
    formWidget->setMaximumWidth(620);
    auto *form = new QFormLayout(formWidget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    formWidget->setMinimumWidth(420);

    m_name = new QLineEdit(formWidget);
    m_name->setObjectName(QStringLiteral("profileName"));
    m_name->setPlaceholderText(QStringLiteral("君之名氏"));
    m_stage = new QComboBox(formWidget);
    m_stage->setObjectName(QStringLiteral("profileStage"));
    for (SchoolStage stage : {SchoolStage::Primary, SchoolStage::Junior, SchoolStage::Senior})
        m_stage->addItem(schoolStageDisplayName(stage), schoolStageToStorage(stage));
    m_enrollmentYear = new QSpinBox(formWidget);
    m_enrollmentYear->setObjectName(QStringLiteral("profileEnrollmentYear"));
    m_enrollmentYear->setRange(0, schoolYearStart(QDate::currentDate()) + 1);
    m_enrollmentYear->setSpecialValueText(QStringLiteral("未填写"));
    m_enrollmentYear->setSuffix(QStringLiteral(" 年"));
    m_enrollmentYear->setValue(schoolYearStart(QDate::currentDate()));
    m_enrollmentYear->setToolTip(
        QStringLiteral("填写进入所选学段的年份，例如 2024 年进入初中。年级会随系统日期自动推算。"));
    m_goal = new QLineEdit(formWidget);
    m_goal->setObjectName(QStringLiteral("profileGoal"));
    m_goal->setPlaceholderText(QStringLiteral("可选，例如：华中师大一附中"));

    m_gradeHint = ui::makeHintLabel(QString(), formWidget);
    m_gradeHint->setObjectName(QStringLiteral("profileGradeHint"));

    m_error = ui::makeErrorLabel(formWidget);
    m_error->setVisible(false);

    form->addRow(QStringLiteral("姓名 *"), m_name);
    form->addRow(QStringLiteral("所在学段"), m_stage);
    form->addRow(QStringLiteral("入学年份"), m_enrollmentYear);
    form->addRow(QString(), m_gradeHint);
    form->addRow(QStringLiteral("目标院校"), m_goal);
    form->addRow(QString(), m_error);
    layout->addWidget(formWidget);

    auto *buttons = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);
    m_saveButton = ui::makeButton(QStringLiteral("保存档案"), buttons);
    m_saveButton->setObjectName(QStringLiteral("profileSaveButton"));
    m_saveButton->setDefault(true);
    m_resetButton = ui::makeButton(QStringLiteral("撤销更改"), buttons);
    m_resetButton->setObjectName(QStringLiteral("profileResetButton"));
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch(1);
    layout->addWidget(buttons);

    m_meta = ui::makeStatusLabel(this);
    layout->addWidget(m_meta);
    layout->addStretch(1);

    connect(m_saveButton, &QPushButton::clicked, this, &ProfilePage::save);
    connect(m_resetButton, &QPushButton::clicked, this, [this] {
        // 有改动时撤销回数据库中的值；刚保存过时撤销这次保存。
        const StudentProfile current = currentFormValues();
        if (isSameProfile(current, m_loaded) && m_canUndoLastSave)
            undoLastChange();
        else
            resetForm();
    });

    // Use textChanged (not textEdited) so IME commit also marks the form dirty;
    // programmatic fills are ignored through m_updatingForm.
    for (QLineEdit *edit : {m_name, m_goal}) {
        connect(edit, &QLineEdit::textChanged, this, [this] {
            if (m_updatingForm)
                return;
            ui::clearRole(m_name);
            m_error->setVisible(false);
            updateActionState();
        });
    }
    connect(m_stage, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (m_updatingForm)
            return;
        m_error->setVisible(false);
        updateActionState();
    });
    connect(m_enrollmentYear, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
        if (m_updatingForm)
            return;
        m_error->setVisible(false);
        updateActionState();
    });

    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveShortcut, &QShortcut::activated, this, &ProfilePage::save);

    reload();
}

void ProfilePage::reload()
{
    auto loaded = m_repository.load();
    if (!loaded) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), loaded.errorMessage());
        return;
    }

    m_hasProfile = loaded.value().has_value() && !loaded.value()->isEmpty();
    if (m_hasProfile)
        m_loaded = *loaded.value();
    else
        m_loaded = StudentProfile{};

    // A fresh load invalidates any snapshot that could be restored.
    m_previous = m_loaded;
    m_canUndoLastSave = false;

    resetForm();
    setBanner(!m_hasProfile);
    m_meta->setText(m_hasProfile
                        ? QStringLiteral("档案创建于 %1 · 最近更新 %2")
                              .arg(timeutil::formatLocal(m_loaded.createdAt),
                                   timeutil::formatLocal(m_loaded.updatedAt))
                        : QStringLiteral("尚未创建档案。"));
}

StudentProfile ProfilePage::currentFormValues() const
{
    StudentProfile profile = m_loaded;
    profile.name = m_name->text().trimmed();
    SchoolStage stage;
    if (schoolStageFromStorage(m_stage->currentData().toString(), &stage))
        profile.stage = stage;
    profile.enrollmentYear = m_enrollmentYear->value();
    profile.targetSchool = m_goal->text().trimmed();
    return profile;
}

void ProfilePage::resetForm()
{
    m_updatingForm = true;
    m_name->setText(m_loaded.name);
    const int stageIndex = m_stage->findData(schoolStageToStorage(m_loaded.stage));
    m_stage->setCurrentIndex(stageIndex >= 0 ? stageIndex : 0);
    m_enrollmentYear->setValue(m_loaded.enrollmentYear);
    m_goal->setText(m_loaded.targetSchool);
    m_updatingForm = false;

    ui::clearRole(m_name);
    m_error->setVisible(false);
    updateActionState();
}

void ProfilePage::updateActionState()
{
    updateGradeHint();

    const bool dirty = !isSameProfile(currentFormValues(), m_loaded);

    if (!m_hasProfile) {
        // First run: nothing stored yet, so saving is the primary action.
        m_saveButton->setText(QStringLiteral("保存档案并开始使用"));
        m_saveButton->setEnabled(true);
        ui::applyRole(m_saveButton, "save");
        m_resetButton->setVisible(false);
        return;
    }

    if (dirty) {
        m_saveButton->setText(QStringLiteral("保存修改"));
        m_saveButton->setEnabled(true);
        ui::applyRole(m_saveButton, "save");
        m_resetButton->setText(QStringLiteral("撤销更改"));
        m_resetButton->setToolTip(QStringLiteral("恢复为上次保存的档案内容"));
        m_resetButton->setVisible(true);
        return;
    }

    // Clean state: the save button becomes a passive status chip.
    m_saveButton->setText(QStringLiteral("已保存"));
    m_saveButton->setEnabled(false);
    m_saveButton->setToolTip(QStringLiteral("当前内容与已保存的档案一致"));
    ui::applyRole(m_saveButton, "saved");

    if (m_canUndoLastSave) {
        m_resetButton->setText(QStringLiteral("撤销修改"));
        m_resetButton->setToolTip(
            QStringLiteral("把档案恢复为本次保存前的内容，需再次点击“保存修改”才会写入数据库"));
        m_resetButton->setVisible(true);
    } else {
        m_resetButton->setVisible(false);
    }
}

void ProfilePage::undoLastChange()
{
    m_updatingForm = true;
    m_name->setText(m_previous.name);
    const int stageIndex = m_stage->findData(schoolStageToStorage(m_previous.stage));
    m_stage->setCurrentIndex(stageIndex >= 0 ? stageIndex : 0);
    m_enrollmentYear->setValue(m_previous.enrollmentYear);
    m_goal->setText(m_previous.targetSchool);
    m_updatingForm = false;

    m_canUndoLastSave = false;
    ui::clearRole(m_name);
    m_error->setVisible(false);
    updateActionState();
}

void ProfilePage::updateGradeHint()
{
    const GradeInfo info = resolveGrade(currentFormValues().stage, m_enrollmentYear->value(),
                                        QDate::currentDate());
    m_gradeHint->setText(
        QStringLiteral("按当前系统日期推算：%1（升入新学年后自动更新）").arg(info.label()));
}

void ProfilePage::setBanner(bool firstRun)
{
    m_banner->setText(QString::fromUtf8(firstRun ? kFirstRunBanner : kNormalBanner));
    ui::applyRole(m_banner, firstRun ? "banner" : "hint");
}

void ProfilePage::save()
{
    const bool hadProfile = m_hasProfile;
    const StudentProfile before = m_loaded;
    StudentProfile profile = currentFormValues();

    m_error->setVisible(false);
    if (profile.name.isEmpty()) {
        ui::applyRole(m_name, "invalid");
        m_error->setText(QStringLiteral("学生姓名不能为空。"));
        m_error->setVisible(true);
        m_name->setFocus();
        return;
    }
    ui::clearRole(m_name);

    auto saved = m_repository.save(profile);
    if (!saved) {
        m_error->setText(saved.errorMessage());
        m_error->setVisible(true);
        return;
    }

    reload();

    // Keep the pre-save snapshot so 撤销修改 can bring those values back. Only an
    // edit of an existing profile is undoable; creating it is not.
    m_canUndoLastSave = hadProfile && m_hasProfile && !isSameProfile(before, m_loaded);
    m_previous = before;
    updateActionState();

    emit profileSaved();
}

} // namespace cutee
