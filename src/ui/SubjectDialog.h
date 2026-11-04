#pragma once

#include <QDialog>

#include "core/Models.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

namespace cutee {

/// Create/edit dialog for a subject. The type of an existing subject is frozen
/// because exam records already depend on it.
class SubjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SubjectDialog(const Subject &subject, bool isNew, QWidget *parent = nullptr);

    Subject subject() const;

private:
    void accept() override;

    Subject m_subject;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QSpinBox *m_sortOrder = nullptr;
    QCheckBox *m_enabled = nullptr;
    QPlainTextEdit *m_note = nullptr;
};

} // namespace cutee
