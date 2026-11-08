#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QLabel;
class QLineEdit;
class QPushButton;

namespace cutee::ui {

/// Content of a destructive-operation confirmation. The user has to type
/// `keyword` before the accept button enables, so a stray Enter or double click
/// cannot wipe data.
struct DangerPrompt
{
    QString title;
    QString heading;       // one line summary shown in bold
    QStringList bullets;   // consequences, one per line
    QString keyword;       // text the user must type verbatim
    QString acceptText;    // destructive button label
    QString hint;          // optional closing note
};

/// Second-level verification window used by import and reset.
class DangerConfirmDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DangerConfirmDialog(const DangerPrompt &prompt, QWidget *parent = nullptr);

    QLineEdit *keywordEdit() const { return m_edit; }
    QPushButton *acceptButton() const { return m_accept; }
    QPushButton *cancelButton() const { return m_cancel; }

private:
    void updateAcceptState();

    DangerPrompt m_prompt;
    QLineEdit *m_edit = nullptr;
    QLabel *m_error = nullptr;
    QPushButton *m_accept = nullptr;
    QPushButton *m_cancel = nullptr;
};

/// Shows the dialog modally. Returns true only when the user typed the keyword
/// and pressed the destructive button.
bool confirmTyped(QWidget *parent, const DangerPrompt &prompt);

} // namespace cutee::ui
