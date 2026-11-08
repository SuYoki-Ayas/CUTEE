#pragma once

#include <QFutureWatcher>
#include <QWidget>

#include "core/Models.h"
#include "service/ScoreService.h"
#include "storage/SettingsRepository.h"

class QAction;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QStackedWidget;
class QTableView;
class QTimer;

namespace cutee {

class Database;
class ScoreFilterProxyModel;
class ScoreTableModel;

/// Background result for the full record list, fetched on its own SQLite
/// connection so a large table never blocks the window.
struct ScoreLoadOutcome
{
    bool ok = false;
    QString error;
    QVector<ExamRecord> records;
};

/// Score list backed by a table model plus a filtering/sorting proxy, so filter
/// changes never rebuild the page or re-query the database.
class ScorePage : public QWidget
{
    Q_OBJECT

public:
    explicit ScorePage(Database &database, QWidget *parent = nullptr);

    void reload();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QWidget *buildFilterBar();
    QWidget *buildActionBar();
    QWidget *buildContent();
    QWidget *buildEmptyState();

    void reloadSubjects();
    void applyFilters();
    void resetFilters();
    void updateSummary();
    void updateActions();
    void updateEmptyState();
    void selectRecord(qint64 id);

    void createRecord();
    void editSelected();
    void removeRecord();
    void exportCsv();

    void buildColumnsMenu();
    void applyColumnVisibility();
    void saveColumnVisibility();

    bool hasSelection() const;
    ExamRecord selectedRecord() const;
    QVector<ExamRecord> visibleRecords() const;

    ScoreService m_service;
    Database &m_database;
    SettingsRepository m_settings;
    QFutureWatcher<ScoreLoadOutcome> *m_watcher = nullptr;
    bool m_loading = false;
    bool m_pending = false;
    ScoreTableModel *m_model = nullptr;
    ScoreFilterProxyModel *m_proxy = nullptr;

    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_subjectFilter = nullptr;
    QCheckBox *m_dateFilter = nullptr;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLineEdit *m_keyword = nullptr;
    QTimer *m_keywordTimer = nullptr;

    QStackedWidget *m_content = nullptr;
    QTableView *m_table = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptyHint = nullptr;
    QPushButton *m_emptyAction = nullptr;

    QLabel *m_summary = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QPushButton *m_columnsButton = nullptr;
    QMenu *m_columnsMenu = nullptr;
    QVector<QAction *> m_columnActions;
};

} // namespace cutee
