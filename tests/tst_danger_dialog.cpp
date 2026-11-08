#include <QtTest>

#include <QLineEdit>
#include <QPushButton>

#include "ui/DangerConfirmDialog.h"

using namespace cutee::ui;

namespace {

DangerPrompt samplePrompt()
{
    DangerPrompt prompt;
    prompt.title = QStringLiteral("重置数据");
    prompt.heading = QStringLiteral("即将清空全部数据");
    prompt.bullets = {QStringLiteral("会删除全部成绩。")};
    prompt.keyword = QStringLiteral("重置");
    prompt.acceptText = QStringLiteral("清空数据");
    prompt.hint = QStringLiteral("重置后需重新填写档案。");
    return prompt;
}

} // namespace

/// The destructive button stays disabled until the exact keyword is typed, so a
/// stray click or Enter can never wipe the database.
class TestDangerDialog : public QObject
{
    Q_OBJECT

private slots:
    void acceptStartsDisabled();
    void keywordEnablesAccept();
    void wrongKeywordKeepsAcceptDisabled();
    void cancelButtonIsNotTheDestructiveOne();
};

void TestDangerDialog::acceptStartsDisabled()
{
    DangerConfirmDialog dialog(samplePrompt());
    QVERIFY(dialog.acceptButton());
    QVERIFY(!dialog.acceptButton()->isEnabled());
    QCOMPARE(dialog.acceptButton()->text(), QStringLiteral("清空数据"));
}

void TestDangerDialog::keywordEnablesAccept()
{
    DangerConfirmDialog dialog(samplePrompt());
    dialog.keywordEdit()->setText(QStringLiteral("重置"));
    QVERIFY(dialog.acceptButton()->isEnabled());

    // Padding is trimmed, so a trailing space does not block a careful user.
    dialog.keywordEdit()->setText(QStringLiteral("  重置 "));
    QVERIFY(dialog.acceptButton()->isEnabled());
}

void TestDangerDialog::wrongKeywordKeepsAcceptDisabled()
{
    DangerConfirmDialog dialog(samplePrompt());
    dialog.keywordEdit()->setText(QStringLiteral("重置数据"));
    QVERIFY(!dialog.acceptButton()->isEnabled());

    dialog.keywordEdit()->clear();
    QVERIFY(!dialog.acceptButton()->isEnabled());
}

void TestDangerDialog::cancelButtonIsNotTheDestructiveOne()
{
    DangerConfirmDialog dialog(samplePrompt());
    QVERIFY(dialog.cancelButton());
    QCOMPARE(dialog.cancelButton()->text(), QStringLiteral("取消"));
    // Enter must not confirm before the keyword is typed.
    QVERIFY(dialog.cancelButton()->isDefault());
    dialog.keywordEdit()->setText(QStringLiteral("重置"));
    QVERIFY(dialog.acceptButton()->isDefault());
}

QTEST_MAIN(TestDangerDialog)

#include "tst_danger_dialog.moc"
