#pragma once

#include <QHash>
#include <QMainWindow>

class QLabel;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

namespace cutee {

class Database;
class SettingsRepository;

/// Shell window: left navigation plus a stacked page area.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Database &database, SettingsRepository &settings, QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void buildUi();
    QWidget *buildSidebar();
    void installShortcuts();
    void addPage(const QString &id, QWidget *page);
    void selectPage(const QString &id);
    void activateNavItem(QTreeWidgetItem *item);
    QTreeWidgetItem *findNavItem(const QString &id) const;
    void restoreUiState();
    void saveUiState();
    void showFirstRunHint();
    void refreshStudentHeader();
    void reloadAllPages();
    void updateStatusBar();

    Database &m_database;
    SettingsRepository &m_settings;

    QTreeWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_studentName = nullptr;
    QLabel *m_studentGrade = nullptr;
    QLabel *m_studentGoal = nullptr;
    QWidget *m_studentBlock = nullptr;
    QHash<QString, int> m_pageIndex;
    bool m_firstRunHintShown = false;
};

} // namespace cutee
