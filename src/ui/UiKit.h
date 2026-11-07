#pragma once

#include <QString>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace cutee::ui {

/// Shared spacing so every page has identical outer margins and rhythm.
inline constexpr int kPageMargin = 24;
inline constexpr int kPageSpacing = 14;

/// Vertical layout for a page root: uniform margins plus `pageRoot` object name.
QVBoxLayout *makePageLayout(QWidget *page);

/// Page heading, secondary description, section heading, muted hint.
QLabel *makePageTitle(const QString &text, QWidget *parent);
QLabel *makePageDescription(const QString &text, QWidget *parent);
QLabel *makeSectionTitle(const QString &text, QWidget *parent);
QLabel *makeHintLabel(const QString &text, QWidget *parent);
QLabel *makeStatusLabel(QWidget *parent);
QLabel *makeErrorLabel(QWidget *parent);

/// Compact filter bar: zero margins, 8px spacing. Returns the bar widget and
/// writes its layout to `layout`.
QWidget *makeFilterBar(QWidget *parent, QHBoxLayout **layout);

QPushButton *makeButton(const QString &text, QWidget *parent);
QPushButton *makePrimaryButton(const QString &text, QWidget *parent);
QPushButton *makeDangerButton(const QString &text, QWidget *parent);

/// Sets a dynamic `uiRole` property (see resources/styles/app.qss) and repolishes.
void applyRole(QWidget *widget, const char *role);

/// Removes the `uiRole` property and repolishes, restoring the default look.
void clearRole(QWidget *widget);

/// Warning-style confirmation with an explicit Chinese action label. Returns
/// true only when the user presses `acceptText`; "取消" is the default button.
bool confirmDanger(QWidget *parent, const QString &title, const QString &text,
                   const QString &acceptText);

} // namespace cutee::ui
