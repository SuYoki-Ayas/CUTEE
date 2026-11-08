#pragma once

#include <QWidget>

#include "core/Models.h"
#include "storage/StudentProfileRepository.h"

class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace cutee {

class Database;
class StudentProfileRepository;

/// The one and only student profile. Editing replaces the existing row; there
/// is intentionally no list and no "add another student" action.
class ProfilePage : public QWidget
{
    Q_OBJECT

public:
    explicit ProfilePage(Database &database, QWidget *parent = nullptr);

    bool hasProfile() const { return m_hasProfile; }
    void reload();

signals:
    void profileSaved();

private:
    void save();
    void resetForm();
    void setBanner(bool firstRun);
    void updateActionState();
    void updateGradeHint();
    void undoLastChange();
    StudentProfile currentFormValues() const;

    StudentProfileRepository m_repository;

    QLabel *m_banner = nullptr;
    QLabel *m_meta = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_gradeHint = nullptr;
    QLineEdit *m_name = nullptr;
    QComboBox *m_stage = nullptr;
    QSpinBox *m_enrollmentYear = nullptr;
    QLineEdit *m_goal = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_resetButton = nullptr;

    bool m_hasProfile = false;
    /// Guards the "dirty" detection while the form is filled programmatically.
    bool m_updatingForm = false;
    /// Last value stored in the database; the form is "clean" when it matches.
    StudentProfile m_loaded;
    /// Values captured right before the last successful save, used by 撤销修改.
    StudentProfile m_previous;
    bool m_canUndoLastSave = false;
};

} // namespace cutee
