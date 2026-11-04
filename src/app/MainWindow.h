#pragma once

#include <QHash>
#include <QMainWindow>

class QLabel;
class QListWidget;
class QStackedWidget;

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
    void addPage(const QString &id, const QString &title, QWidget *page);
    void selectPage(const QString &id);
    void restoreUiState();
    void saveUiState();
    void showFirstRunHint();

    Database &m_database;
    SettingsRepository &m_settings;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;
    QLabel *m_statusLabel = nullptr;
    QHash<QString, int> m_pageIndex;
    bool m_firstRunHintShown = false;
};

} // namespace cutee
