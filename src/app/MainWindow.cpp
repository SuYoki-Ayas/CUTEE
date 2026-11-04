#include "app/MainWindow.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "service/LegacyDataService.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"
#include "ui/LogPage.h"
#include "ui/ProfilePage.h"
#include "ui/ScorePage.h"
#include "ui/SettingsPage.h"
#include "ui/SubjectPage.h"
#include "ui/SummaryPage.h"

#include <QCloseEvent>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

namespace cutee {

MainWindow::MainWindow(Database &database, SettingsRepository &settings, QWidget *parent)
    : QMainWindow(parent)
    , m_database(database)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("CUTEE 学生成绩管理 %1").arg(QStringLiteral(CUTEE_VERSION)));
    resize(1180, 760);
    buildUi();
    restoreUiState();

    LegacyDataService legacy(m_database);
    auto legacyStats = legacy.stats();
    if (legacyStats && legacyStats.value().any())
        qInfo().noquote() << QStringLiteral("检测到旧版考试数据，已原样保留：%1")
                                 .arg(legacyStats.value().summaryText());
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(buildSidebar());

    m_stack = new QStackedWidget(central);
    layout->addWidget(m_stack, 1);

    addPage(QStringLiteral("profile"), QStringLiteral("学生档案"),
            new ProfilePage(m_database, this));
    addPage(QStringLiteral("subjects"), QStringLiteral("科目管理"),
            new SubjectPage(m_database, this));
    addPage(QStringLiteral("scores"), QStringLiteral("成绩管理"),
            new ScorePage(m_database, this));
    addPage(QStringLiteral("summary"), QStringLiteral("成绩汇总"),
            new SummaryPage(m_database, this));
    addPage(QStringLiteral("settings"), QStringLiteral("设置"),
            new SettingsPage(m_database, m_settings, this));
    addPage(QStringLiteral("logs"), QStringLiteral("日志"), new LogPage(this));

    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->showMessage(QStringLiteral("就绪"));

    const int version = m_database.schemaVersion();
    m_statusLabel->setText(QStringLiteral("数据库 schema v%1 · %2")
                               .arg(version)
                               .arg(QDir::toNativeSeparators(m_database.filePath())));
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(210);
    sidebar->setStyleSheet(QStringLiteral(
        "#sidebar { background: #1f2530; }"
        "#sidebar QLabel { color: #f2f5f9; }"
        "#sidebar QLabel#appSubtitle { color: #93a1b3; }"
        "#sidebar QListWidget { background: transparent; border: none; color: #cbd5e1;"
        " outline: none; font-size: 14px; }"
        "#sidebar QListWidget::item { padding: 11px 16px; border-radius: 6px; margin: 2px 8px; }"
        "#sidebar QListWidget::item:selected { background: #2f81f7; color: #ffffff; }"
        "#sidebar QListWidget::item:hover:!selected { background: #2a3242; }"));

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 18, 8, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("CUTEE"), sidebar);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setContentsMargins(10, 4, 0, 0);
    layout->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral("单学生成绩管理"), sidebar);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    subtitle->setContentsMargins(10, 0, 0, 6);
    layout->addWidget(subtitle);

    m_nav = new QListWidget(sidebar);
    m_nav->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_nav, 1);

    auto *version = new QLabel(QStringLiteral("版本 %1 · 单机离线").arg(QStringLiteral(CUTEE_VERSION)),
                               sidebar);
    version->setObjectName(QStringLiteral("appSubtitle"));
    version->setContentsMargins(10, 0, 0, 0);
    layout->addWidget(version);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0)
            m_stack->setCurrentIndex(row);
    });

    return sidebar;
}

void MainWindow::addPage(const QString &id, const QString &title, QWidget *page)
{
    const int index = m_stack->addWidget(page);
    m_pageIndex.insert(id, index);

    auto *item = new QListWidgetItem(title, m_nav);
    item->setData(Qt::UserRole, id);
}

void MainWindow::selectPage(const QString &id)
{
    const auto it = m_pageIndex.constFind(id);
    if (it == m_pageIndex.constEnd())
        return;
    m_nav->setCurrentRow(it.value());
}

void MainWindow::restoreUiState()
{
    const auto geometry = m_settings.value(QLatin1String(settings::kWindowGeometry));
    if (geometry.ok() && !geometry.value().isEmpty())
        restoreGeometry(QByteArray::fromBase64(geometry.value().toLatin1()));

    const auto state = m_settings.value(QLatin1String(settings::kWindowState));
    if (state.ok() && !state.value().isEmpty())
        restoreState(QByteArray::fromBase64(state.value().toLatin1()));

    const auto lastPage = m_settings.value(QLatin1String(settings::kLastPage), QStringLiteral("profile"));
    selectPage(lastPage.ok() ? lastPage.value() : QStringLiteral("profile"));
    if (m_nav->currentRow() < 0)
        m_nav->setCurrentRow(0);
}

void MainWindow::saveUiState()
{
    m_settings.setValue(QLatin1String(settings::kWindowGeometry),
                        QString::fromLatin1(saveGeometry().toBase64()));
    m_settings.setValue(QLatin1String(settings::kWindowState),
                        QString::fromLatin1(saveState().toBase64()));
    if (m_nav->currentItem()) {
        m_settings.setValue(QLatin1String(settings::kLastPage),
                            m_nav->currentItem()->data(Qt::UserRole).toString());
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_firstRunHintShown)
        return;
    m_firstRunHintShown = true;
    QTimer::singleShot(0, this, &MainWindow::showFirstRunHint);
}

void MainWindow::showFirstRunHint()
{
    auto *profilePage = qobject_cast<ProfilePage *>(m_stack->widget(m_pageIndex.value(QStringLiteral("profile"))));
    if (!profilePage || profilePage->hasProfile())
        return;

    selectPage(QStringLiteral("profile"));
    QMessageBox::information(
        this, QStringLiteral("首次使用"),
        QStringLiteral("欢迎使用 CUTEE 学生成绩管理。\n\n"
                       "本工具只维护一位学生的档案与成绩，请先在“学生档案”页填写并保存基本信息。"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUiState();
    logging::shutdown();
    QMainWindow::closeEvent(event);
}

} // namespace cutee
