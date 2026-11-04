#pragma once

#include <QDialog>
#include <QVector>

#include "core/Models.h"

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace cutee {

class ScoreDialog : public QDialog
{
    Q_OBJECT

public:
    /// `existing` is null when creating a new record. Disabled subjects stay
    /// selectable only when they belong to the record being edited.
    ScoreDialog(const ExamRecord *existing, const QVector<Subject> &subjects,
                QWidget *parent = nullptr);

    ExamRecord record() const;

private:
    void accept() override;
    void refreshSubjects();

    ExamRecord m_record;
    QVector<Subject> m_subjects;
    QComboBox *m_type = nullptr;
    QComboBox *m_subject = nullptr;
    QLineEdit *m_title = nullptr;
    QDateEdit *m_date = nullptr;
    QLineEdit *m_score = nullptr;
    QLineEdit *m_fullScore = nullptr;
    QSpinBox *m_rank = nullptr;
    QSpinBox *m_participants = nullptr;
    QPlainTextEdit *m_note = nullptr;
};

} // namespace cutee
