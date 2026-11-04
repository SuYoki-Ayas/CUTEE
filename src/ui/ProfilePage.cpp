#include "ui/ProfilePage.h"

#include "core/TimeUtils.h"
#include "storage/Database.h"
#include "storage/StudentProfileRepository.h"

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
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("学生档案"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    layout->addWidget(m_banner);

    auto *formWidget = new QWidget(this);
    formWidget->setMaximumWidth(620);
    auto *form = new QFormLayout(formWidget);
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_name = new QLineEdit(formWidget);
    m_name->setPlaceholderText(QStringLiteral("必填，例如：张三"));
    m_studentNo = new QLineEdit(formWidget);
    m_studentNo->setPlaceholderText(QStringLiteral("可选，学号或准考证号"));
    m_schoolClass = new QLineEdit(formWidget);
    m_schoolClass->setPlaceholderText(QStringLiteral("可选，例如：某某中学 高一（3）班"));
    m_note = new QPlainTextEdit(formWidget);
    m_note->setPlaceholderText(QStringLiteral("可选，例如：竞赛方向、目标院校等"));
    m_note->setFixedHeight(100);

    form->addRow(QStringLiteral("姓名 *"), m_name);
    form->addRow(QStringLiteral("学号 / 准考证号"), m_studentNo);
    form->addRow(QStringLiteral("学校 / 班级"), m_schoolClass);
    form->addRow(QStringLiteral("备注"), m_note);
    layout->addWidget(formWidget);

    auto *buttons = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);
    m_saveButton = new QPushButton(QStringLiteral("保存档案"), buttons);
    m_saveButton->setDefault(true);
    m_resetButton = new QPushButton(QStringLiteral("撤销更改"), buttons);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch(1);
    layout->addWidget(buttons);

    m_meta = new QLabel(this);
    m_meta->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(m_meta);
    layout->addStretch(1);

    connect(m_saveButton, &QPushButton::clicked, this, &ProfilePage::save);
    connect(m_resetButton, &QPushButton::clicked, this, &ProfilePage::resetForm);

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
}

void ProfilePage::setBanner(bool firstRun)
{
    m_banner->setText(QString::fromUtf8(firstRun ? kFirstRunBanner : kNormalBanner));
    m_banner->setStyleSheet(
        firstRun ? QStringLiteral("background: #eef4ff; color: #1d4ed8; border: 1px solid #bfd3ff;"
                                  " border-radius: 6px; padding: 10px;")
                 : QStringLiteral("color: #667085;"));
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

    if (profile.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"), QStringLiteral("学生姓名不能为空。"));
        m_name->setFocus();
        return;
    }

    if (m_hasProfile) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("确认保存"),
            QStringLiteral("确定用当前内容覆盖已有档案吗？"), QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    auto saved = m_repository.save(profile);
    if (!saved) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), saved.errorMessage());
        return;
    }

    reload();
    emit profileSaved();
    QMessageBox::information(this, QStringLiteral("已保存"), QStringLiteral("学生档案已保存。"));
}

} // namespace cutee
