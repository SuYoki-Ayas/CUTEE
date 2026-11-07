#include "ui/ProfilePage.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/StudentProfileRepository.h"
#include "ui/UiKit.h"

#include <QFont>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace cutee {
namespace {

constexpr char kFirstRunBanner[] =
    "欢迎使用 CUTEE！本工具只维护一位学生的成绩档案，请先填写基本信息，保存后即可开始记录成绩。";
constexpr char kNormalBanner[] =
    "本工具只维护一位学生的档案。修改后点击“保存修改”，历史成绩会自动关联到这位学生。";

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
    m_name->setPlaceholderText(QStringLiteral("必填，例如：张三"));
    m_studentNo = new QLineEdit(formWidget);
    m_studentNo->setPlaceholderText(QStringLiteral("可选，学号或准考证号"));
    m_schoolClass = new QLineEdit(formWidget);
    m_schoolClass->setPlaceholderText(QStringLiteral("可选，例如：某某中学 高一（3）班"));
    m_note = new QPlainTextEdit(formWidget);
    m_note->setPlaceholderText(QStringLiteral("可选，例如：竞赛方向、目标院校等"));
    m_note->setFixedHeight(100);

    m_error = ui::makeErrorLabel(formWidget);
    m_error->setVisible(false);

    form->addRow(QStringLiteral("姓名 *"), m_name);
    form->addRow(QStringLiteral("学号 / 准考证号"), m_studentNo);
    form->addRow(QStringLiteral("学校 / 班级"), m_schoolClass);
    form->addRow(QStringLiteral("备注"), m_note);
    form->addRow(QString(), m_error);
    layout->addWidget(formWidget);

    auto *buttons = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);
    m_saveButton = ui::makePrimaryButton(QStringLiteral("保存档案"), buttons);
    m_saveButton->setDefault(true);
    m_resetButton = ui::makeButton(QStringLiteral("撤销更改"), buttons);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addSpacing(12);
    m_feedback = ui::makeStatusLabel(buttons);
    buttonLayout->addWidget(m_feedback);
    buttonLayout->addStretch(1);
    layout->addWidget(buttons);

    m_meta = ui::makeStatusLabel(this);
    layout->addWidget(m_meta);
    layout->addStretch(1);

    connect(m_saveButton, &QPushButton::clicked, this, &ProfilePage::save);
    connect(m_resetButton, &QPushButton::clicked, this, &ProfilePage::resetForm);
    connect(m_name, &QLineEdit::textEdited, this, [this] {
        ui::clearRole(m_name);
        m_error->setVisible(false);
        m_feedback->clear();
    });

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

    resetForm();
    setBanner(!m_hasProfile);
    m_meta->setText(m_hasProfile
                        ? QStringLiteral("档案创建于 %1 · 最近更新 %2")
                              .arg(timeutil::formatLocal(m_loaded.createdAt),
                                   timeutil::formatLocal(m_loaded.updatedAt))
                        : QStringLiteral("尚未创建档案。"));
}

void ProfilePage::resetForm()
{
    m_name->setText(m_loaded.name);
    m_studentNo->setText(m_loaded.studentNo);
    m_schoolClass->setText(m_loaded.schoolClass);
    m_note->setPlainText(m_loaded.note);
    m_resetButton->setEnabled(m_hasProfile);
    ui::clearRole(m_name);
    m_error->setVisible(false);
    m_feedback->clear();
}

void ProfilePage::setBanner(bool firstRun)
{
    m_banner->setText(QString::fromUtf8(firstRun ? kFirstRunBanner : kNormalBanner));
    ui::applyRole(m_banner, firstRun ? "banner" : "hint");
    m_saveButton->setText(firstRun ? QStringLiteral("保存档案并开始使用")
                                   : QStringLiteral("保存修改"));
}

void ProfilePage::save()
{
    StudentProfile profile = m_loaded;
    profile.name = m_name->text().trimmed();
    profile.studentNo = m_studentNo->text().trimmed();
    profile.schoolClass = m_schoolClass->text().trimmed();
    profile.note = m_note->toPlainText().trimmed();

    m_feedback->clear();
    if (profile.name.isEmpty()) {
        ui::applyRole(m_name, "invalid");
        m_error->setText(QStringLiteral("学生姓名不能为空。"));
        m_error->setVisible(true);
        m_name->setFocus();
        return;
    }
    ui::clearRole(m_name);
    m_error->setVisible(false);

    auto saved = m_repository.save(profile);
    if (!saved) {
        m_error->setText(saved.errorMessage());
        m_error->setVisible(true);
        return;
    }

    reload();
    emit profileSaved();
    m_feedback->setText(QStringLiteral("已保存"));
    ui::applyRole(m_feedback, "success");
}

} // namespace cutee
