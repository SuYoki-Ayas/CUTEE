#include "ui/LogPage.h"

#include "core/AppPaths.h"
#include "core/Logging.h"
#include "ui/UiKit.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace cutee {

LogPage::LogPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = ui::makePageLayout(this);

    layout->addWidget(ui::makePageTitle(QStringLiteral("日志"), this));

    QHBoxLayout *toolbarLayout = nullptr;
    QWidget *toolbar = ui::makeFilterBar(this, &toolbarLayout);

    auto *refreshButton = ui::makeButton(QStringLiteral("刷新"), toolbar);
    refreshButton->setAutoDefault(false);
    auto *openButton = ui::makeButton(QStringLiteral("打开日志目录"), toolbar);
    openButton->setAutoDefault(false);
    m_autoRefresh = new QCheckBox(QStringLiteral("自动刷新（2 秒）"), toolbar);

    auto *pathLabel = new QLabel(QDir::toNativeSeparators(logging::logFilePath()), toolbar);
    ui::applyRole(pathLabel, "hint");

    toolbarLayout->addWidget(refreshButton);
    toolbarLayout->addWidget(openButton);
    toolbarLayout->addWidget(m_autoRefresh);
    toolbarLayout->addSpacing(8);
    toolbarLayout->addWidget(pathLabel, 1);

    layout->addWidget(toolbar);

    m_view = new QPlainTextEdit(this);
    m_view->setObjectName(QStringLiteral("logView"));
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(m_view, 1);

    m_timer = new QTimer(this);
    m_timer->setInterval(2000);
    connect(m_timer, &QTimer::timeout, this, &LogPage::refresh);

    connect(refreshButton, &QPushButton::clicked, this, &LogPage::refresh);
    connect(openButton, &QPushButton::clicked, this, &LogPage::openLogDirectory);
    connect(m_autoRefresh, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked)
            m_timer->start();
        else
            m_timer->stop();
    });

    refresh();
}

void LogPage::refresh()
{
    const QStringList lines = logging::readTail(2000);
    const QTextCursor cursor = m_view->textCursor();
    const bool atBottom = m_view->verticalScrollBar()->value() >= m_view->verticalScrollBar()->maximum();

    m_view->setPlainText(lines.join(QLatin1Char('\n')));
    if (atBottom || cursor.isNull()) {
        m_view->moveCursor(QTextCursor::End);
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
    }
}

void LogPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refresh();
    if (m_autoRefresh->isChecked())
        m_timer->start();
}

void LogPage::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void LogPage::openLogDirectory()
{
    const QString directory = AppPaths::logDirectory();
    if (!QDir(directory).exists()) {
        QMessageBox::information(this, QStringLiteral("日志目录"),
                                 QStringLiteral("日志目录尚未创建：%1")
                                     .arg(QDir::toNativeSeparators(directory)));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

} // namespace cutee
