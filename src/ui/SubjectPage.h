#pragma once

#include <QHash>
#include <QVector>
#include <QWidget>

#include "core/Models.h"
#include "storage/SubjectRepository.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;

namespace cutee {

class Database;
class SubjectRepository;

class SubjectPage : public QWidget
{
    Q_OBJECT

public:
    explicit SubjectPage(Database &database, QWidget *parent = nullptr);

    void reload();

private:
    void createSubject();
    void editSelected();
    void removeSelected();
    void toggleSelected();
    void moveSelected(int offset);
    int selectedRow() const;
    void updateActions();

    SubjectRepository m_repository;

    QComboBox *m_typeFilter = nullptr;
    QCheckBox *m_showDisabled = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QPushButton *m_upButton = nullptr;
    QPushButton *m_downButton = nullptr;

    QVector<Subject> m_subjects;
    QHash<qint64, int> m_referenceCounts;
};

} // namespace cutee
