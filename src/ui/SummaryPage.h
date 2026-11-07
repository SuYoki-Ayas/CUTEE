#pragma once

#include <QFutureWatcher>
#include <QWidget>

#include "core/ScoreSummary.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace cutee {

class Database;

/// Background summary result. Rows are aggregated on a worker thread with its
/// own SQLite connection so a slow statistic never freezes the window.
struct SummaryOutcome
{
    bool ok = false;
    QString error;
    ScoreSummary summary;
};

class SummaryPage : public QWidget
{
    Q_OBJECT

public:
    explicit SummaryPage(Database &database, QWidget *parent = nullptr);

    void reload();

protected:
    void showEvent(QShowEvent *event) override;

private:
    QWidget *buildFilterBar();
    QWidget *buildContent();
    QWidget *buildEmptyState();

    DateRange currentRange() const;
    void startQuery(const DateRange &range);
    void applyOutcome(const SummaryOutcome &outcome);
    void updateEmptyState();

    void exportCsv();

    Database &m_database;
    QFutureWatcher<SummaryOutcome> *m_watcher = nullptr;
    bool m_running = false;
    bool m_pending = false;

    QComboBox *m_rangeKind = nullptr;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QPushButton *m_exportButton = nullptr;
    QLabel *m_summary = nullptr;

    QStackedWidget *m_content = nullptr;
    QWidget *m_tablesPage = nullptr;
    QWidget *m_emptyPage = nullptr;
    QLabel *m_emptyTitle = nullptr;
    QLabel *m_emptyHint = nullptr;

    QTableWidget *m_subjectTable = nullptr;
    QTableWidget *m_typeTable = nullptr;

    ScoreSummary m_lastSummary;
};

} // namespace cutee
