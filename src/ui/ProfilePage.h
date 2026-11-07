#pragma once

#include <QWidget>

#include "core/Models.h"
#include "storage/StudentProfileRepository.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

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

    StudentProfileRepository m_repository;

    QLabel *m_banner = nullptr;
    QLabel *m_meta = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_feedback = nullptr;
    QLineEdit *m_name = nullptr;
    QLineEdit *m_studentNo = nullptr;
    QLineEdit *m_schoolClass = nullptr;
    QPlainTextEdit *m_note = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_resetButton = nullptr;

    bool m_hasProfile = false;
    StudentProfile m_loaded;
};

} // namespace cutee
