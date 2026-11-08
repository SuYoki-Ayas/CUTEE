#include "ui/SettingsPage.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "storage/DataMaintenance.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"
#include "ui/DangerConfirmDialog.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
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

} // namespace

SettingsPage::SettingsPage(Database &database, SettingsRepository &settings, QWidget *parent)
    : QWidget(parent)
    , m_database(database)
    , m_settings(settings)
{
    auto *layout = ui::makePageLayout(this);

    layout->addWidget(ui::makePageTitle(QStringLiteral("设置"), this));

    layout->addWidget(buildForm());
    layout->addWidget(buildMaintenanceSection());
    layout->addWidget(buildAboutSection());
    layout->addStretch(1);

    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(saveShortcut, &QShortcut::activated, this, &SettingsPage::save);

    reload();
}

QWidget *SettingsPage::buildForm()
{
    auto *form = new QWidget(this);
    auto *formLayout = new QFormLayout(form);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

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
    m_logLevel->setObjectName(QStringLiteral("settingsLogLevel"));
    m_logLevel->addItem(QStringLiteral("调试（Debug）"), QStringLiteral("debug"));
    m_logLevel->addItem(QStringLiteral("信息（Info）"), QStringLiteral("info"));
    m_logLevel->addItem(QStringLiteral("警告（Warning）"), QStringLiteral("warning"));
    m_logLevel->addItem(QStringLiteral("错误（Critical）"), QStringLiteral("critical"));
    formLayout->addRow(QStringLiteral("日志级别"), m_logLevel);

    auto *hint = ui::makeHintLabel(
        QStringLiteral("所有数据保存在本机 SQLite 文件中，不会上传到网络。"
                       "换电脑或做备份请用下方「数据备份与恢复」。"),
        form);
    formLayout->addRow(QString(), hint);

    auto *buttons = new QWidget(form);
    auto *buttonLayout = new QHBoxLayout(buttons);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(10);

    m_saveButton = ui::makeButton(QStringLiteral("保存设置"), buttons);
    m_saveButton->setObjectName(QStringLiteral("settingsSaveButton"));
    m_saveButton->setDefault(true);
    m_resetButton = ui::makeButton(QStringLiteral("撤销更改"), buttons);
    m_resetButton->setObjectName(QStringLiteral("settingsResetButton"));
    auto *reloadButton = ui::makeButton(QStringLiteral("重新加载"), buttons);

    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addSpacing(6);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addStretch(1);

    m_feedback = ui::makeStatusLabel(buttons);
    m_feedback->setMinimumWidth(240);
    buttonLayout->addWidget(m_feedback);

    formLayout->addRow(QString(), buttons);

    connect(m_saveButton, &QPushButton::clicked, this, &SettingsPage::save);
    connect(m_resetButton, &QPushButton::clicked, this, [this] {
        // 有改动时撤销回数据库中的值；刚保存过时撤销这次保存。
        if (currentLogLevel() == m_loadedLevel && m_canUndoLastSave)
            undoLastChange();
        else
            resetForm();
    });
    connect(reloadButton, &QPushButton::clicked, this, &SettingsPage::reload);
    // currentIndexChanged (not activated) so keyboard and programmatic changes
    // both refresh the action state; m_updatingForm filters the latter.
    connect(m_logLevel, &QComboBox::currentIndexChanged, this, [this] {
        if (m_updatingForm)
            return;
        showFeedback(QString());
        updateActionState();
    });
    connect(openDataButton, &QPushButton::clicked, this,
            [this] { openPath(AppPaths::dataDirectory(), true); });
    connect(openDatabaseButton, &QPushButton::clicked, this,
            [this] { openPath(m_database.filePath(), false); });
    connect(openLogButton, &QPushButton::clicked, this,
            [this] { openPath(AppPaths::logDirectory(), true); });

    return form;
}

QWidget *SettingsPage::buildMaintenanceSection()
{
    auto *group = new QGroupBox(QStringLiteral("数据备份与恢复"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    layout->addWidget(ui::makeHintLabel(
        QStringLiteral("「导出数据库」把当前全部数据存成一个 .db 文件；「导入数据库」会用所选文件"
                       "完全替换当前数据；「重置数据」清空全部档案、科目与成绩。"
                       "导入与重置之前都会自动备份到：%1")
            .arg(QDir::toNativeSeparators(AppPaths::backupDirectory())),
        group));

    auto *buttons = new QWidget(group);
    auto *row = new QHBoxLayout(buttons);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    m_exportButton = ui::makeButton(QStringLiteral("导出数据库…"), buttons);
    m_exportButton->setObjectName(QStringLiteral("settingsExportButton"));
    m_importButton = ui::makeDangerButton(QStringLiteral("导入数据库…"), buttons);
    m_importButton->setObjectName(QStringLiteral("settingsImportButton"));
    m_wipeButton = ui::makeDangerButton(QStringLiteral("重置数据…"), buttons);
    m_wipeButton->setObjectName(QStringLiteral("settingsWipeButton"));

    row->addWidget(m_exportButton);
    row->addWidget(m_importButton);
    row->addWidget(m_wipeButton);
    row->addStretch(1);
    layout->addWidget(buttons);

    connect(m_exportButton, &QPushButton::clicked, this, &SettingsPage::exportDatabase);
    connect(m_importButton, &QPushButton::clicked, this, &SettingsPage::importDatabase);
    connect(m_wipeButton, &QPushButton::clicked, this, &SettingsPage::resetData);

    return group;
}

QWidget *SettingsPage::buildAboutSection()
{
    auto *group = new QGroupBox(QStringLiteral("关于本工具"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    layout->addWidget(ui::makeHintLabel(
        QStringLiteral("CUTEE %1 是 Windows下 采用 Qt6 开发的 单机离线的成绩管理工具")
            .arg(QStringLiteral(CUTEE_VERSION)),
        group));

    layout->addWidget(ui::makeHintLabel(
        QStringLiteral("数据库 schema 版本：%1。日常备份请用上方「导出数据库」；"
                       "换机或还原时用「导入数据库」选择对应的 .db 文件。")
            .arg(m_database.schemaVersion()),
        group));

    return group;
}

void SettingsPage::reload()
{
    const auto level = m_settings.value(QLatin1String(settings::kLogLevel), QStringLiteral("info"));
    m_loadedLevel = level.ok() ? level.value() : QStringLiteral("info");

    m_updatingForm = true;
    const int levelIndex = m_logLevel->findData(m_loadedLevel);
    m_logLevel->setCurrentIndex(levelIndex >= 0 ? levelIndex : 1);
    m_updatingForm = false;

    m_dataDirectory->setText(AppPaths::dataDirectory());
    m_dataDirectory->setCursorPosition(0);
    m_databaseFile->setText(m_database.filePath());
    m_databaseFile->setCursorPosition(0);
    m_logFile->setText(AppPaths::logFile());
    m_logFile->setCursorPosition(0);

    m_canUndoLastSave = false;
    showFeedback(QString());
    updateActionState();
}

QString SettingsPage::currentLogLevel() const
{
    return m_logLevel->currentData().toString();
}

void SettingsPage::resetForm()
{
    m_updatingForm = true;
    const int levelIndex = m_logLevel->findData(m_loadedLevel);
    if (levelIndex >= 0)
        m_logLevel->setCurrentIndex(levelIndex);
    m_updatingForm = false;

    showFeedback(QString());
    updateActionState();
}

void SettingsPage::undoLastChange()
{
    m_updatingForm = true;
    const int levelIndex = m_logLevel->findData(m_previousLevel);
    if (levelIndex >= 0)
        m_logLevel->setCurrentIndex(levelIndex);
    m_updatingForm = false;

    m_canUndoLastSave = false;
    showFeedback(QString());
    updateActionState();
}

void SettingsPage::updateActionState()
{
    if (currentLogLevel() != m_loadedLevel) {
        m_saveButton->setText(QStringLiteral("保存设置"));
        m_saveButton->setEnabled(true);
        m_saveButton->setToolTip(QStringLiteral("把当前设置写入本机数据库"));
        ui::applyRole(m_saveButton, "save");

        m_resetButton->setText(QStringLiteral("撤销更改"));
        m_resetButton->setToolTip(QStringLiteral("恢复为上次保存的设置"));
        m_resetButton->setVisible(true);
        return;
    }

    // Clean state: the save button becomes a passive status chip.
    m_saveButton->setText(QStringLiteral("已保存"));
    m_saveButton->setEnabled(false);
    m_saveButton->setToolTip(QStringLiteral("当前设置与已保存的内容一致"));
    ui::applyRole(m_saveButton, "saved");

    if (m_canUndoLastSave) {
        m_resetButton->setText(QStringLiteral("撤销修改"));
        m_resetButton->setToolTip(
            QStringLiteral("把设置恢复为本次保存前的内容，需再次点击“保存设置”才会写入数据库"));
        m_resetButton->setVisible(true);
    } else {
        m_resetButton->setVisible(false);
    }
}

void SettingsPage::save()
{
    const QString level = currentLogLevel();
    const QString before = m_loadedLevel;

    auto levelResult = m_settings.setValue(QLatin1String(settings::kLogLevel), level);
    if (!levelResult) {
        showFeedback(levelResult.errorMessage(), true);
        updateActionState();
        return;
    }

    logging::setMinimumLevel(logging::levelFromName(level));
    qInfo().noquote() << QStringLiteral("设置已保存：日志级别=%1").arg(level);

    m_loadedLevel = level;
    m_previousLevel = before;
    m_canUndoLastSave = before != level;
    showFeedback(QString());
    updateActionState();
    emit settingsChanged();
}

void SettingsPage::exportDatabase()
{
    QDir().mkpath(AppPaths::backupDirectory());
    const QString suggested =
        QDir(AppPaths::backupDirectory())
            .filePath(QStringLiteral("cutee-%1.db")
                          .arg(QDateTime::currentDateTime().toString(
                              QStringLiteral("yyyyMMdd-HHmmss"))));

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出数据库"), suggested,
        QStringLiteral("SQLite 数据库 (*.db);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    auto exported = maintenance::exportSnapshot(m_database, path);
    if (!exported) {
        showFeedback(exported.errorMessage(), true);
        QMessageBox::critical(this, QStringLiteral("导出失败"), exported.errorMessage());
        return;
    }

    showFeedback(QStringLiteral("已导出 %1").arg(QFileInfo(path).fileName()));
    QMessageBox::information(
        this, QStringLiteral("导出完成"),
        QStringLiteral("全部数据已导出到：\n%1\n\n可在其他电脑上用「导入数据库」恢复这份数据。")
            .arg(QDir::toNativeSeparators(path)));
}

void SettingsPage::importDatabase()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择要导入的数据库文件"), AppPaths::dataDirectory(),
        QStringLiteral("SQLite 数据库 (*.db);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    auto inspected = maintenance::inspectDatabaseFile(path);
    if (!inspected) {
        QMessageBox::critical(this, QStringLiteral("无法导入"), inspected.errorMessage());
        return;
    }

    ui::DangerPrompt prompt;
    prompt.title = QStringLiteral("导入数据库");
    prompt.heading = QStringLiteral("即将用所选文件覆盖当前全部数据");
    prompt.bullets = {
        QStringLiteral("导入文件：\n%1（schema v%2）")
            .arg(QDir::toNativeSeparators(path))
            .arg(inspected.value()),
        QStringLiteral("当前的学生档案、科目、成绩与设置都会被替换，程序内无法撤销。"),
        QStringLiteral("导入前会自动备份当前数据到：\n%1")
            .arg(QDir::toNativeSeparators(AppPaths::backupDirectory())),
    };
    prompt.keyword = QStringLiteral("导入");
    prompt.acceptText = QStringLiteral("覆盖并导入");
    prompt.hint = QStringLiteral("导入完成后所有页面会立即刷新，无需重启程序。");
    if (!ui::confirmTyped(this, prompt))
        return;

    auto imported = maintenance::importDatabaseFile(m_database, path);
    if (!imported) {
        QMessageBox::critical(this, QStringLiteral("导入失败"), imported.errorMessage());
        return;
    }

    reloadAfterDataChange();
    QMessageBox::information(
        this, QStringLiteral("导入完成"),
        QStringLiteral("已导入：\n%1\n\n导入前的数据已备份到：\n%2")
            .arg(QDir::toNativeSeparators(path),
                 QDir::toNativeSeparators(imported.value())));
}

void SettingsPage::resetData()
{
    ui::DangerPrompt prompt;
    prompt.title = QStringLiteral("重置数据");
    prompt.heading = QStringLiteral("即将清空全部档案、科目与成绩");
    prompt.bullets = {
        QStringLiteral("会被删除：学生档案、全部科目、全部成绩记录。"),
        QStringLiteral("会保留：程序设置（日志级别、窗口位置、成绩表列配置）。"),
        QStringLiteral("重置前会自动备份当前数据到：\n%1")
            .arg(QDir::toNativeSeparators(AppPaths::backupDirectory())),
    };
    prompt.keyword = QStringLiteral("重置");
    prompt.acceptText = QStringLiteral("清空数据");
    prompt.hint = QStringLiteral("重置后需要重新填写学生档案才能继续记录成绩。");
    if (!ui::confirmTyped(this, prompt))
        return;

    auto backup = maintenance::createBackup(m_database, QStringLiteral("before-reset"));
    if (!backup) {
        QMessageBox::critical(this, QStringLiteral("重置失败"),
                              QStringLiteral("无法创建备份，已取消重置：\n%1")
                                  .arg(backup.errorMessage()));
        return;
    }

    auto cleared = maintenance::resetAllData(m_database);
    if (!cleared) {
        QMessageBox::critical(this, QStringLiteral("重置失败"), cleared.errorMessage());
        return;
    }

    reloadAfterDataChange();
    QMessageBox::information(
        this, QStringLiteral("已重置"),
        QStringLiteral("档案、科目与成绩已清空。\n\n重置前的数据已备份到：\n%1")
            .arg(QDir::toNativeSeparators(backup.value())));
}

void SettingsPage::reloadAfterDataChange()
{
    // The database identity is unchanged, but its contents (including the
    // stored log level) may come from the imported file.
    reload();
    logging::setMinimumLevel(logging::levelFromName(m_loadedLevel));
    emit dataReplaced();
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
    ui::applyRole(m_feedback, error ? "error" : "success");
}

} // namespace cutee
