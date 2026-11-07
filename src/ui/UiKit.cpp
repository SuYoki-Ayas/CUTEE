#include "ui/UiKit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

namespace cutee::ui {
namespace {

QLabel *makeLabel(const QString &text, const char *objectName, const char *role, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    if (objectName)
        label->setObjectName(QString::fromLatin1(objectName));
    if (role)
        label->setProperty("uiRole", QString::fromLatin1(role));
    return label;
}

QPushButton *makeRoleButton(const QString &text, const char *role, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setAutoDefault(false);
    if (role)
        applyRole(button, role);
    return button;
}

} // namespace

QVBoxLayout *makePageLayout(QWidget *page)
{
    page->setObjectName(QStringLiteral("pageRoot"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(kPageMargin, kPageMargin, kPageMargin, kPageMargin);
    layout->setSpacing(kPageSpacing);
    return layout;
}

QLabel *makePageTitle(const QString &text, QWidget *parent)
{
    return makeLabel(text, "pageTitle", nullptr, parent);
}

QLabel *makePageDescription(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, "pageDescription", nullptr, parent);
    label->setWordWrap(true);
    return label;
}

QLabel *makeSectionTitle(const QString &text, QWidget *parent)
{
    return makeLabel(text, nullptr, "section", parent);
}

QLabel *makeHintLabel(const QString &text, QWidget *parent)
{
    auto *label = makeLabel(text, nullptr, "hint", parent);
    label->setWordWrap(true);
    return label;
}

QLabel *makeStatusLabel(QWidget *parent)
{
    return makeLabel(QString(), nullptr, "status", parent);
}

QLabel *makeErrorLabel(QWidget *parent)
{
    auto *label = makeLabel(QString(), nullptr, "error", parent);
    label->setWordWrap(true);
    return label;
}

QWidget *makeFilterBar(QWidget *parent, QHBoxLayout **layout)
{
    auto *bar = new QWidget(parent);
    bar->setObjectName(QStringLiteral("filterBar"));
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(8);
    if (layout)
        *layout = barLayout;
    return bar;
}

QPushButton *makeButton(const QString &text, QWidget *parent)
{
    return makeRoleButton(text, nullptr, parent);
}

QPushButton *makePrimaryButton(const QString &text, QWidget *parent)
{
    return makeRoleButton(text, "primary", parent);
}

QPushButton *makeDangerButton(const QString &text, QWidget *parent)
{
    return makeRoleButton(text, "danger", parent);
}

void applyRole(QWidget *widget, const char *role)
{
    if (!widget)
        return;
    widget->setProperty("uiRole", QString::fromLatin1(role));
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void clearRole(QWidget *widget)
{
    if (!widget)
        return;
    widget->setProperty("uiRole", QVariant());
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

bool confirmDanger(QWidget *parent, const QString &title, const QString &text,
                   const QString &acceptText)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, parent);
    QPushButton *accept = box.addButton(acceptText, QMessageBox::DestructiveRole);
    QPushButton *cancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();
    return box.clickedButton() == accept;
}

} // namespace cutee::ui
