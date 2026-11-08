#include <QtTest>

#include <QComboBox>
#include <QPushButton>
#include <QTemporaryDir>

#include "storage/Database.h"
#include "storage/SettingsRepository.h"
#include "ui/SettingsPage.h"

using namespace cutee;

namespace {

QString roleOf(const QPushButton *widget)
{
    return widget ? widget->property("uiRole").toString() : QString();
}

} // namespace

/// The settings page reuses the profile page action contract:
/// clean (grey 已保存, no undo), dirty (green 保存设置 + 撤销更改) and
/// just-saved (grey 已保存 + 撤销修改).
class TestSettingsPage : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void cleanStateShowsSavedChip();
    void changingLevelSwitchesToGreenSave();
    void savingSwitchesToUndoState();
    void undoRestoresPreviousLevel();
    void resetDiscardsEditsWithoutTouchingDatabase();

private:
    QTemporaryDir m_directory;
    Database *m_database = nullptr;
    QScopedPointer<SettingsRepository> m_settings;
    QScopedPointer<SettingsPage> m_page;
    int m_counter = 0;

    QComboBox *levelCombo() const
    {
        return m_page->findChild<QComboBox *>(QStringLiteral("settingsLogLevel"));
    }
    QPushButton *saveButton() const
    {
        return m_page->findChild<QPushButton *>(QStringLiteral("settingsSaveButton"));
    }
    QPushButton *resetButton() const
    {
        return m_page->findChild<QPushButton *>(QStringLiteral("settingsResetButton"));
    }
    QString storedLevel() const
    {
        const auto level = m_settings->value(QLatin1String(settings::kLogLevel));
        return level.ok() ? level.value() : QString();
    }
};

void TestSettingsPage::init()
{
    QVERIFY(m_directory.isValid());
    m_database = new Database(QStringLiteral("test-settings-page-%1").arg(++m_counter));
    QVERIFY2(m_database->open(m_directory.filePath(QStringLiteral("page%1.db").arg(m_counter))).ok(),
             "无法打开测试数据库");
    QVERIFY(m_database->migrate().ok());
    m_settings.reset(new SettingsRepository(*m_database));
    m_page.reset(new SettingsPage(*m_database, *m_settings));
}

void TestSettingsPage::cleanup()
{
    m_page.reset();
    m_settings.reset();
    delete m_database;
    m_database = nullptr;
}

void TestSettingsPage::cleanStateShowsSavedChip()
{
    QVERIFY(saveButton());
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(!saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("saved"));
    QVERIFY(resetButton()->isHidden());
}

void TestSettingsPage::changingLevelSwitchesToGreenSave()
{
    levelCombo()->setCurrentIndex(0);

    QCOMPARE(saveButton()->text(), QStringLiteral("保存设置"));
    QVERIFY(saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("save"));
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));
}

void TestSettingsPage::savingSwitchesToUndoState()
{
    levelCombo()->setCurrentIndex(0);
    saveButton()->click();

    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(!saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("saved"));
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销修改"));
    QCOMPARE(storedLevel(), QStringLiteral("debug"));
}

void TestSettingsPage::undoRestoresPreviousLevel()
{
    levelCombo()->setCurrentIndex(0);
    saveButton()->click();
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销修改"));

    resetButton()->click();

    QCOMPARE(levelCombo()->currentData().toString(), QStringLiteral("info"));
    QCOMPARE(saveButton()->text(), QStringLiteral("保存设置"));
    QVERIFY(saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("save"));
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));

    // The database still holds the saved value until the user saves again.
    QCOMPARE(storedLevel(), QStringLiteral("debug"));
}

void TestSettingsPage::resetDiscardsEditsWithoutTouchingDatabase()
{
    levelCombo()->setCurrentIndex(3);
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));

    resetButton()->click();

    QCOMPARE(levelCombo()->currentData().toString(), QStringLiteral("info"));
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(resetButton()->isHidden());
    QVERIFY(storedLevel().isEmpty());
}

QTEST_MAIN(TestSettingsPage)

#include "tst_settings_page.moc"
