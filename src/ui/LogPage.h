#pragma once

#include <QWidget>

class QCheckBox;
class QPlainTextEdit;
class QTimer;

namespace cutee {

/// Shows the tail of the rotating application log.
class LogPage : public QWidget
{
    Q_OBJECT

public:
    explicit LogPage(QWidget *parent = nullptr);

    void refresh();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void openLogDirectory();

    QPlainTextEdit *m_view = nullptr;
    QCheckBox *m_autoRefresh = nullptr;
    QTimer *m_timer = nullptr;
};

} // namespace cutee
