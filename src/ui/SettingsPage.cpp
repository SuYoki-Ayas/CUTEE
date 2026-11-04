#include "ui/SettingsPage.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "service/LegacyDataService.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace cutee {
namespace {

QWidget *readOnlyPathRow(QLineEdit **field, const QString &text, QPushButton **button,
                         const QString &buttonText, QWidget *parent)
{
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    *field = new QLineEdit(text, container);
    (*field)->setReadOnly(true);
    (*field)->setCursorPosition(0);
    layout->addWidget(*field, 1);

    *button = new QPushButton(buttonText, container);
    (*button)->setAutoDefault(false);
    layout->addWidget(*button);
    return container;
}

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

SettingsPage::SettingsPage(Database &database, SettingsRepository &settings, QWidget *parent)
    : QWidget(parent)
    , m_database(database)
    , m_settings(settings)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("设置"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    layout->addWidget(buildForm());
    layout->addWidget(buildLegacySection());
    layout->addStretch(1);

    reload();
}

QWidget *SettingsPage::buildForm()
{
    auto *form = new QWidget(this);
    auto *formLayout = new QFormLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QPushButton *openDataButton = nullptr;
    QPushButton *openDatabaseButton = nullptr;
    QPushButton *openLogButton = nullptr;

    QWidget *dataRow = readOnlyPathRow(&m_dataDirectory, AppPaths::dataDirectory(), &openDataButton,
                                       QStringLiteral("打开"), form);
    QWidget *databaseRow = readOnlyPathRow(&m_databaseFile, m_database.filePath(),
                                           &openDatabaseButton, QStringLiteral("打开所在目录"), form);
    QWidget *logRow = readOnlyPathRow(&m_logFile, AppPaths::logFile(), &openLogButton,
                                      QStringLiteral("打开"), form);

    formLayout->addRow(QStringLiteral("数据目录"), dataRow);
    formLayout->addRow(QStringLiteral("数据库文件"), databaseRow);
    formLayout->addRow(QStringLiteral("日志文件"), logRow);

    m_logLevel = new QComboBox(form);
    m_logLevel->addItem(QStringLiteral("调试（Debug）"), QStringLiteral("debug"));
    m_logLevel->addItem(QStringLiteral("信息（Info）"), QStringLiteral("info"));
    m_logLevel->addItem(QStringLiteral("警告（Warning）"), QStringLiteral("warning"));
    m_logLevel->addItem(QStringLiteral("错误（Critical）"), QStringLiteral("critical"));
    formLayout->addRow(QStringLiteral("日志级别"), m_logLevel);

    auto *hint = new QLabel(
        QStringLiteral("所有数据保存在本机 SQLite 文件中，不会上传到网络。备份时复制数据目录即可。"
                       "当前数据库 schema 版本：%1。")
            .arg(m_database.schemaVersion()),
        form);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #667085;"));
    formLayout->addRow(QString(), hint);

    auto *buttons = new QWidget(form);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);

    auto *saveButton = new QPushButton(QStringLiteral("保存设置"), buttons);
    saveButton->setDefault(true);
    auto *reloadButton = new QPushButton(QStringLiteral("重新加载"), buttons);
    reloadButton->setAutoDefault(false);

    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addStretch(1);

    m_feedback = new QLabel(buttons);
    m_feedback->setMinimumWidth(240);
    buttonLayout->addWidget(m_feedback);

    formLayout->addRow(QString(), buttons);

    connect(saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    connect(reloadButton, &QPushButton::clicked, this, &SettingsPage::reload);
    connect(openDataButton, &QPushButton::clicked, this,
            [this] { openPath(AppPaths::dataDirectory(), true); });
    connect(openDatabaseButton, &QPushButton::clicked, this,
            [this] { openPath(m_database.filePath(), false); });
    connect(openLogButton, &QPushButton::clicked, this,
            [this] { openPath(AppPaths::logDirectory(), true); });

    return form;
}

QWidget *SettingsPage::buildLegacySection()
{
    auto *group = new QGroupBox(QStringLiteral("旧版考试数据（只读保留）"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setSpacing(10);

    auto *explanation = new QLabel(LegacyDataService::explanation(), group);
    explanation->setWordWrap(true);
    explanation->setStyleSheet(QStringLiteral("color: #667085;"));
    layout->addWidget(explanation);

    m_legacyStatus = new QLabel(group);
    m_legacyStatus->setWordWrap(true);
    layout->addWidget(m_legacyStatus);

    auto *buttons = new QWidget(group);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);

    auto *refreshButton = new QPushButton(QStringLiteral("刷新统计"), buttons);
    refreshButton->setAutoDefault(false);
    m_clearLegacyButton = new QPushButton(QStringLiteral("清理旧考试数据…"), buttons);
    m_clearLegacyButton->setAutoDefault(false);
    m_clearLegacyButton->setStyleSheet(QStringLiteral("color: #b42318;"));

    buttonLayout->addWidget(refreshButton);
    buttonLayout->addWidget(m_clearLegacyButton);
    buttonLayout->addStretch(1);
    layout->addWidget(buttons);

    connect(refreshButton, &QPushButton::clicked, this, &SettingsPage::reloadLegacy);
    connect(m_clearLegacyButton, &QPushButton::clicked, this, &SettingsPage::clearLegacy);

    return group;
}

void SettingsPage::reload()
{
    const auto level = m_settings.value(QLatin1String(settings::kLogLevel), QStringLiteral("info"));
    const QString levelName = level.ok() ? level.value() : QStringLiteral("info");
    const int levelIndex = m_logLevel->findData(levelName);
    m_logLevel->setCurrentIndex(levelIndex >= 0 ? levelIndex : 1);

    m_dataDirectory->setText(AppPaths::dataDirectory());
    m_dataDirectory->setCursorPosition(0);
    m_databaseFile->setText(m_database.filePath());
    m_databaseFile->setCursorPosition(0);
    m_logFile->setText(AppPaths::logFile());
    m_logFile->setCursorPosition(0);

    reloadLegacy();
    showFeedback(QString());
}

void SettingsPage::reloadLegacy()
{
    LegacyDataService service(m_database);
    auto stats = service.stats();
    if (!stats) {
        m_legacyStatus->setText(QStringLiteral("读取旧数据统计失败：%1").arg(stats.errorMessage()));
        m_legacyStatus->setStyleSheet(QStringLiteral("color: #b42318;"));
        m_clearLegacyButton->setEnabled(false);
        return;
    }

    m_legacyStatus->setText(stats.value().summaryText());
    m_legacyStatus->setStyleSheet(QStringLiteral("color: #667085;"));
    m_clearLegacyButton->setEnabled(stats.value().any());
}

void SettingsPage::clearLegacy()
{
    LegacyDataService service(m_database);
    auto stats = service.stats();
    if (!stats) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), stats.errorMessage());
        return;
    }
    if (!stats.value().any()) {
        QMessageBox::information(this, QStringLiteral("无需清理"),
                                 QStringLiteral("旧版考试数据已经为空。"));
        return;
    }

    const auto first = QMessageBox::warning(
        this, QStringLiteral("确认清理旧数据"),
        QStringLiteral("即将删除旧版考试数据：\n\n%1\n\n"
                       "学生档案、科目与新成绩记录不会受到影响。\n该操作不可撤销，确定继续吗？")
            .arg(stats.value().summaryText()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (first != QMessageBox::Yes)
        return;

    bool accepted = false;
    const QString typed = QInputDialog::getText(
        this, QStringLiteral("再次确认"),
        QStringLiteral("请输入“清理”两个字以确认删除旧版考试数据："), QLineEdit::Normal, QString(),
        &accepted);
    if (!accepted)
        return;
    if (typed.trimmed() != QStringLiteral("清理")) {
        QMessageBox::information(this, QStringLiteral("已取消"),
                                 QStringLiteral("输入不匹配，未删除任何数据。"));
        return;
    }

    auto cleared = service.clear();
    if (!cleared) {
        QMessageBox::warning(this, QStringLiteral("清理失败"), cleared.errorMessage());
        return;
    }

    reloadLegacy();
    QMessageBox::information(this, QStringLiteral("清理完成"),
                             QStringLiteral("旧版考试数据已删除，新成绩数据保持不变。"));
}

void SettingsPage::save()
{
    const QString level = m_logLevel->currentData().toString();

    auto levelResult = m_settings.setValue(QLatin1String(settings::kLogLevel), level);
    if (!levelResult) {
        showFeedback(levelResult.errorMessage(), true);
        return;
    }

    logging::setMinimumLevel(logging::levelFromName(level));
    qInfo().noquote() << QStringLiteral("设置已保存：日志级别=%1").arg(level);
    showFeedback(QStringLiteral("已保存"));
    emit settingsChanged();
}

void SettingsPage::openPath(const QString &path, bool isDirectory)
{
    const QFileInfo info(path);
    const QString target = isDirectory ? path : info.absolutePath();
    if (!QDir(target).exists()) {
        QMessageBox::warning(this, QStringLiteral("路径不存在"),
                             QStringLiteral("目录不存在：%1").arg(QDir::toNativeSeparators(target)));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(target))) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
                             QStringLiteral("无法打开：%1").arg(QDir::toNativeSeparators(target)));
    }
}

void SettingsPage::showFeedback(const QString &message, bool error)
{
    m_feedback->setText(message);
    m_feedback->setStyleSheet(error ? QStringLiteral("color: #c0392b;")
                                    : QStringLiteral("color: #1f7a3f;"));
}

} // namespace cutee
