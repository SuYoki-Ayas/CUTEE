#include "app/MainWindow.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "core/SchoolStage.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"
#include "storage/StudentProfileRepository.h"
#include "ui/LogPage.h"
#include "ui/ProfilePage.h"
#include "ui/ScorePage.h"
#include "ui/SettingsPage.h"
#include "ui/SubjectPage.h"
#include "ui/SummaryPage.h"

#include "ui/Palette.h"

#include <QAbstractItemView>
#include <QAction>
#include <QBrush>
#include <QCloseEvent>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVector>

namespace cutee {

MainWindow::MainWindow(Database &database, SettingsRepository &settings, QWidget *parent)
    : QMainWindow(parent)
    , m_database(database)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("CUTEE~ %1").arg(QStringLiteral(CUTEE_VERSION)));
    setMinimumSize(1024, 680);
    resize(1240, 800);
    buildUi();
    restoreUiState();
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

    auto *profilePage = new ProfilePage(m_database, this);
    connect(profilePage, &ProfilePage::profileSaved, this, &MainWindow::refreshStudentHeader);
    addPage(QStringLiteral("profile"), profilePage);
    addPage(QStringLiteral("subjects"), new SubjectPage(m_database, this));
    addPage(QStringLiteral("scores"), new ScorePage(m_database, this));
    addPage(QStringLiteral("summary"), new SummaryPage(m_database, this));
    auto *settingsPage = new SettingsPage(m_database, m_settings, this);
    connect(settingsPage, &SettingsPage::dataReplaced, this, &MainWindow::reloadAllPages);
    addPage(QStringLiteral("settings"), settingsPage);
    addPage(QStringLiteral("logs"), new LogPage(this));

    installShortcuts();

    setCentralWidget(central);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusBar()->addPermanentWidget(m_statusLabel);
    statusBar()->showMessage(QStringLiteral("就绪"));

    updateStatusBar();
    refreshStudentHeader();
}

void MainWindow::updateStatusBar()
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(QStringLiteral("数据库 schema v%1 · %2")
                               .arg(m_database.schemaVersion())
                               .arg(QDir::toNativeSeparators(m_database.filePath())));
}

void MainWindow::reloadAllPages()
{
    const auto page = [this](const char *id) {
        return m_stack->widget(m_pageIndex.value(QString::fromLatin1(id)));
    };

    if (auto *profile = qobject_cast<ProfilePage *>(page("profile")))
        profile->reload();
    if (auto *subjects = qobject_cast<SubjectPage *>(page("subjects")))
        subjects->reload();
    if (auto *scores = qobject_cast<ScorePage *>(page("scores")))
        scores->reload();
    if (auto *summary = qobject_cast<SummaryPage *>(page("summary")))
        summary->reload();

    updateStatusBar();
    refreshStudentHeader();
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QWidget(this);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(212);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 18, 8, 12);
    layout->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("CUTEE"), sidebar);
    title->setObjectName(QStringLiteral("appTitle"));
    title->setContentsMargins(10, 4, 0, 0);
    layout->addWidget(title);

    auto *subtitle = new QLabel(QStringLiteral("成绩管理和实用工具"), sidebar);
    subtitle->setObjectName(QStringLiteral("appSubtitle"));
    subtitle->setContentsMargins(10, 0, 0, 6);
    layout->addWidget(subtitle);

    // Student identity block: the name as a bold second-level heading with the
    // date-derived grade and the target school underneath. Hidden until a
    // profile exists.
    m_studentBlock = new QWidget(sidebar);
    auto *studentLayout = new QVBoxLayout(m_studentBlock);
    studentLayout->setContentsMargins(10, 0, 8, 4);
    studentLayout->setSpacing(2);

    m_studentName = new QLabel(m_studentBlock);
    m_studentName->setObjectName(QStringLiteral("studentName"));
    m_studentName->setWordWrap(true);
    m_studentGrade = new QLabel(m_studentBlock);
    m_studentGrade->setObjectName(QStringLiteral("studentGrade"));
    m_studentGrade->setWordWrap(true);
    m_studentGoal = new QLabel(m_studentBlock);
    m_studentGoal->setObjectName(QStringLiteral("studentGoal"));
    m_studentGoal->setWordWrap(true);

    studentLayout->addWidget(m_studentName);
    studentLayout->addWidget(m_studentGrade);
    studentLayout->addWidget(m_studentGoal);
    m_studentBlock->setVisible(false);
    layout->addWidget(m_studentBlock);

    m_nav = new QTreeWidget(sidebar);
    m_nav->setObjectName(QStringLiteral("navTree"));
    m_nav->setHeaderHidden(true);
    m_nav->setRootIsDecorated(true);
    m_nav->setIndentation(16);
    m_nav->setFocusPolicy(Qt::NoFocus);
    m_nav->setSelectionMode(QAbstractItemView::SingleSelection);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setAnimated(false);
    layout->addWidget(m_nav, 1);

    auto *version = new QLabel(QStringLiteral("版本 %1 · 单机离线").arg(QStringLiteral(CUTEE_VERSION)),
                               sidebar);
    version->setObjectName(QStringLiteral("sidebarMeta"));
    version->setContentsMargins(10, 0, 0, 0);
    layout->addWidget(version);

    const auto makeLeaf = [this](QTreeWidgetItem *group, const QString &id, const QString &text,
                                 const QString &shortcut) {
        auto *item = group ? new QTreeWidgetItem(group) : new QTreeWidgetItem(m_nav);
        item->setText(0, text);
        item->setData(0, Qt::UserRole, id);
        item->setToolTip(0, QStringLiteral("%1（%2）").arg(text, shortcut));
        return item;
    };
    const auto makeGroup = [this](const QString &text) {
        auto *item = new QTreeWidgetItem(m_nav);
        item->setText(0, text);
        // Categories are containers, not pages: keeping them out of selection
        // means the stack only ever follows real leaves.
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        item->setForeground(0, QBrush(palette::color(palette::kTextPrimary)));
        item->setSizeHint(0, QSize(0, 34));
        item->setExpanded(true);
        return item;
    };

    makeLeaf(nullptr, QStringLiteral("profile"), QStringLiteral("档案"), QStringLiteral("Ctrl+1"));

    QTreeWidgetItem *exam = makeGroup(QStringLiteral("考试"));
    makeLeaf(exam, QStringLiteral("subjects"), QStringLiteral("科目管理"), QStringLiteral("Ctrl+2"));
    makeLeaf(exam, QStringLiteral("scores"), QStringLiteral("成绩管理"), QStringLiteral("Ctrl+3"));
    makeLeaf(exam, QStringLiteral("summary"), QStringLiteral("成绩汇总"), QStringLiteral("Ctrl+4"));

    QTreeWidgetItem *settingsGroup = makeGroup(QStringLiteral("设置"));
    makeLeaf(settingsGroup, QStringLiteral("settings"), QStringLiteral("设置"),
             QStringLiteral("Ctrl+5"));
    makeLeaf(settingsGroup, QStringLiteral("logs"), QStringLiteral("日志"), QStringLiteral("Ctrl+6"));

    connect(m_nav, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        if (!item)
            return;
        if (item->childCount() > 0) {
            item->setExpanded(!item->isExpanded());
            return;
        }
        activateNavItem(item);
    });
    connect(m_nav, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                if (current && current->childCount() == 0)
                    activateNavItem(current);
            });

    return sidebar;
}

void MainWindow::installShortcuts()
{
    struct PageShortcut
    {
        const char *id;
        const char *keys;
    };
    static const PageShortcut kShortcuts[] = {
        {"profile", "Ctrl+1"},  {"scores", "Ctrl+2"},   {"summary", "Ctrl+3"},
        {"subjects", "Ctrl+4"}, {"settings", "Ctrl+5"}, {"logs", "Ctrl+6"},
    };

    for (const PageShortcut &entry : kShortcuts) {
        auto *action = new QAction(this);
        action->setShortcut(QKeySequence(QString::fromLatin1(entry.keys)));
        action->setShortcutContext(Qt::WindowShortcut);
        const QString id = QString::fromLatin1(entry.id);
        connect(action, &QAction::triggered, this, [this, id] { selectPage(id); });
        addAction(action);
    }
}

void MainWindow::addPage(const QString &id, QWidget *page)
{
    const int index = m_stack->addWidget(page);
    m_pageIndex.insert(id, index);
}

void MainWindow::activateNavItem(QTreeWidgetItem *item)
{
    if (!item)
        return;
    const QString id = item->data(0, Qt::UserRole).toString();
    const auto it = m_pageIndex.constFind(id);
    if (it == m_pageIndex.constEnd())
        return;
    m_stack->setCurrentIndex(it.value());
}

QTreeWidgetItem *MainWindow::findNavItem(const QString &id) const
{
    for (int i = 0; i < m_nav->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = m_nav->topLevelItem(i);
        if (top->data(0, Qt::UserRole).toString() == id)
            return top;
        for (int j = 0; j < top->childCount(); ++j) {
            QTreeWidgetItem *child = top->child(j);
            if (child->data(0, Qt::UserRole).toString() == id)
                return child;
        }
    }
    return nullptr;
}

void MainWindow::selectPage(const QString &id)
{
    QTreeWidgetItem *item = findNavItem(id);
    if (!item)
        return;
    if (QTreeWidgetItem *parent = item->parent())
        parent->setExpanded(true);
    if (m_nav->currentItem() != item)
        m_nav->setCurrentItem(item);
    const auto it = m_pageIndex.constFind(id);
    if (it != m_pageIndex.constEnd())
        m_stack->setCurrentIndex(it.value());
}

void MainWindow::restoreUiState()
{
    const auto geometry = m_settings.value(QLatin1String(settings::kWindowGeometry));
    if (geometry.ok() && !geometry.value().isEmpty())
        restoreGeometry(QByteArray::fromBase64(geometry.value().toLatin1()));

    // A geometry saved on a 100% screen can come back smaller than the minimum
    // size once Windows reports a different device pixel ratio.
    if (!isMaximized() && !isFullScreen()) {
        const QSize minimum = minimumSize();
        if (width() < minimum.width() || height() < minimum.height())
            resize(qMax(width(), minimum.width()), qMax(height(), minimum.height()));
    }

    const auto state = m_settings.value(QLatin1String(settings::kWindowState));
    if (state.ok() && !state.value().isEmpty())
        restoreState(QByteArray::fromBase64(state.value().toLatin1()));

    const auto lastPage = m_settings.value(QLatin1String(settings::kLastPage), QStringLiteral("profile"));
    selectPage(lastPage.ok() ? lastPage.value() : QStringLiteral("profile"));
    if (!m_nav->currentItem())
        selectPage(QStringLiteral("profile"));
}

void MainWindow::saveUiState()
{
    m_settings.setValue(QLatin1String(settings::kWindowGeometry),
                        QString::fromLatin1(saveGeometry().toBase64()));
    m_settings.setValue(QLatin1String(settings::kWindowState),
                        QString::fromLatin1(saveState().toBase64()));
    if (QTreeWidgetItem *current = m_nav->currentItem()) {
        const QString id = current->data(0, Qt::UserRole).toString();
        if (!id.isEmpty())
            m_settings.setValue(QLatin1String(settings::kLastPage), id);
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // Re-derive the grade so a profile edited elsewhere (or a school-year
    // rollover while the app stayed open) shows the current label.
    refreshStudentHeader();
    if (m_firstRunHintShown)
        return;
    m_firstRunHintShown = true;
    QTimer::singleShot(0, this, &MainWindow::showFirstRunHint);
}

void MainWindow::refreshStudentHeader()
{
    if (!m_studentBlock || !m_studentName || !m_studentGrade || !m_studentGoal)
        return;

    auto loaded = StudentProfileRepository(m_database).load();
    if (!loaded || !loaded.value() || loaded.value()->isEmpty()) {
        m_studentName->clear();
        m_studentGrade->clear();
        m_studentGoal->clear();
        m_studentBlock->setVisible(false);
        return;
    }

    const StudentProfile &profile = *loaded.value();
    const GradeInfo grade =
        resolveGrade(profile.stage, profile.enrollmentYear, QDate::currentDate());
    const QString goal = profile.targetSchool.trimmed();

    m_studentName->setText(profile.displayName());
    m_studentGrade->setText(grade.label());
    m_studentGoal->setText(QStringLiteral("目标：%1").arg(goal));
    m_studentGoal->setVisible(!goal.isEmpty());
    m_studentBlock->setVisible(true);
}

void MainWindow::showFirstRunHint()
{
    auto *profilePage = qobject_cast<ProfilePage *>(m_stack->widget(m_pageIndex.value(QStringLiteral("profile"))));
    if (!profilePage || profilePage->hasProfile())
        return;

    selectPage(QStringLiteral("profile"));
    QMessageBox::information(
        this, QStringLiteral("首次使用"),
        QStringLiteral("欢迎使用 CUTEE~ 。\n\n"
                       "CUTEE负责维护您的档案与成绩，请先在“学生档案”页填写并保存基本信息。"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveUiState();
    logging::shutdown();
    QMainWindow::closeEvent(event);
}

} // namespace cutee
