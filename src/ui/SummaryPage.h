#pragma once

#include <QWidget>

#include "core/ScoreSummary.h"
#include "service/ScoreService.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QTableWidget;

namespace cutee {

class Database;

class SummaryPage : public QWidget
{
    Q_OBJECT

public:
    explicit SummaryPage(Database &database, QWidget *parent = nullptr);

    void reload();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void exportCsv();
    DateRange currentRange() const;

    ScoreService m_service;
    QComboBox *m_rangeKind = nullptr;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLabel *m_summary = nullptr;
    QTableWidget *m_subjectTable = nullptr;
    QTableWidget *m_typeTable = nullptr;

    ScoreSummary m_lastSummary;
};

} // namespace cutee
