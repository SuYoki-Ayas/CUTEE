#pragma once

#include <QVector>
#include <QWidget>

#include "core/Models.h"
#include "service/ScoreService.h"

class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace cutee {

class Database;

class ScorePage : public QWidget
{
    Q_OBJECT

public:
    explicit ScorePage(Database &database, QWidget *parent = nullptr);

    void reload();

protected:
    void showEvent(QShowEvent *event) override;

private:
    void reloadSubjects();
    void createRecord();
    void editSelected();
    void removeRecord();
    void exportCsv();
    int selectedRow() const;
    void updateActions();
    void updateSummary();

    ScoreService m_service;
    QComboBox *m_typeFilter = nullptr;
    QComboBox *m_subjectFilter = nullptr;
    QCheckBox *m_dateFilter = nullptr;
    QDateEdit *m_from = nullptr;
    QDateEdit *m_to = nullptr;
    QLineEdit *m_keyword = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_exportButton = nullptr;

    QVector<ExamRecord> m_records;
};

} // namespace cutee
