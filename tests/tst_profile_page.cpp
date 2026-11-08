#include <QtTest>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>

#include "storage/Database.h"
#include "storage/StudentProfileRepository.h"
#include "ui/ProfilePage.h"

using namespace cutee;

namespace {

QString roleOf(const QPushButton *widget)
{
    return widget ? widget->property("uiRole").toString() : QString();
}

} // namespace

/// The profile page switches between three action states:
/// clean (grey 已保存, no undo), dirty (green 保存修改 + 撤销更改) and
/// just-saved (grey 已保存 + 撤销修改).
class TestProfilePage : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void firstRunOffersCreateAction();
    void cleanStateHidesUndo();
    void editingSwitchesToGreenSave();
    void savingSwitchesToUndoState();
    void undoRestoresPreSaveValues();
    void resetDiscardsEditsWithoutTouchingDatabase();
    void gradeHintFollowsEnrolment();
    void goalFieldMarksDirtyAndRoundTrips();

private:
    QTemporaryDir m_directory;
    Database *m_database = nullptr;
    QScopedPointer<ProfilePage> m_page;
    int m_counter = 0;

    QLineEdit *nameEdit() const
    {
        return m_page->findChild<QLineEdit *>(QStringLiteral("profileName"));
    }
    QComboBox *stageCombo() const
    {
        return m_page->findChild<QComboBox *>(QStringLiteral("profileStage"));
    }
    QSpinBox *yearSpin() const
    {
        return m_page->findChild<QSpinBox *>(QStringLiteral("profileEnrollmentYear"));
    }
    QPushButton *saveButton() const
    {
        return m_page->findChild<QPushButton *>(QStringLiteral("profileSaveButton"));
    }
    QPushButton *resetButton() const
    {
        return m_page->findChild<QPushButton *>(QStringLiteral("profileResetButton"));
    }

    /// Junior 2024 keeps the assertions independent from the current date: the
    /// stage index changes are what mark the form dirty.
    void seedProfile(SchoolStage stage = SchoolStage::Junior, int enrollmentYear = 2024)
    {
        StudentProfile profile;
        profile.name = QStringLiteral("林小满");
        profile.stage = stage;
        profile.enrollmentYear = enrollmentYear;
        QVERIFY(StudentProfileRepository(*m_database).save(profile).ok());
        m_page->reload();
    }
};

void TestProfilePage::init()
{
    QVERIFY(m_directory.isValid());
    m_database = new Database(QStringLiteral("test-profile-page-%1").arg(++m_counter));
    QVERIFY2(m_database->open(m_directory.filePath(QStringLiteral("page%1.db").arg(m_counter))).ok(),
             "无法打开测试数据库");
    QVERIFY(m_database->migrate().ok());
    m_page.reset(new ProfilePage(*m_database));
}

void TestProfilePage::cleanup()
{
    m_page.reset();
    delete m_database;
    m_database = nullptr;
}

void TestProfilePage::firstRunOffersCreateAction()
{
    QVERIFY(!m_page->hasProfile());
    QVERIFY(saveButton());
    QCOMPARE(saveButton()->text(), QStringLiteral("保存档案并开始使用"));
    QVERIFY(saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("save"));
    QVERIFY(resetButton()->isHidden());
}

void TestProfilePage::cleanStateHidesUndo()
{
    seedProfile();

    QVERIFY(m_page->hasProfile());
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(!saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("saved"));
    QVERIFY(resetButton()->isHidden());
}

void TestProfilePage::editingSwitchesToGreenSave()
{
    seedProfile();

    stageCombo()->setCurrentIndex(stageCombo()->findData(schoolStageToStorage(SchoolStage::Senior)));

    QCOMPARE(saveButton()->text(), QStringLiteral("保存修改"));
    QVERIFY(saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("save"));
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));
}

void TestProfilePage::savingSwitchesToUndoState()
{
    seedProfile();

    stageCombo()->setCurrentIndex(stageCombo()->findData(schoolStageToStorage(SchoolStage::Senior)));
    saveButton()->click();

    QCOMPARE(roleOf(saveButton()), QStringLiteral("saved"));
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(!saveButton()->isEnabled());
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销修改"));

    auto stored = StudentProfileRepository(*m_database).load();
    QVERIFY(stored.ok());
    QVERIFY(stored.value().has_value());
    QCOMPARE(stored.value()->stage, SchoolStage::Senior);
    QCOMPARE(stored.value()->enrollmentYear, 2024);
}

void TestProfilePage::undoRestoresPreSaveValues()
{
    seedProfile();

    stageCombo()->setCurrentIndex(stageCombo()->findData(schoolStageToStorage(SchoolStage::Senior)));
    saveButton()->click();
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销修改"));

    resetButton()->click();

    QCOMPARE(stageCombo()->currentData().toString(),
             schoolStageToStorage(SchoolStage::Junior));
    QCOMPARE(saveButton()->text(), QStringLiteral("保存修改"));
    QVERIFY(saveButton()->isEnabled());
    QCOMPARE(roleOf(saveButton()), QStringLiteral("save"));
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));

    // The database still holds the saved value until the user saves again.
    auto stored = StudentProfileRepository(*m_database).load();
    QVERIFY(stored.ok());
    QCOMPARE(stored.value()->stage, SchoolStage::Senior);
}

void TestProfilePage::resetDiscardsEditsWithoutTouchingDatabase()
{
    seedProfile();

    yearSpin()->setValue(2021);
    QVERIFY(!resetButton()->isHidden());
    QCOMPARE(resetButton()->text(), QStringLiteral("撤销更改"));

    resetButton()->click();

    QCOMPARE(yearSpin()->value(), 2024);
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));
    QVERIFY(resetButton()->isHidden());

    auto stored = StudentProfileRepository(*m_database).load();
    QVERIFY(stored.ok());
    QCOMPARE(stored.value()->enrollmentYear, 2024);
}

void TestProfilePage::gradeHintFollowsEnrolment()
{
    seedProfile(SchoolStage::Junior, 2024);

    auto hint = m_page->findChild<QLabel *>(QStringLiteral("profileGradeHint"));
    QVERIFY(hint);
    const QString expected =
        resolveGrade(SchoolStage::Junior, 2024, QDate::currentDate()).label();
    QVERIFY2(hint->text().contains(expected),
             qPrintable(QStringLiteral("提示未包含推算年级 %1：%2").arg(expected, hint->text())));

    // Changing the entry stage re-derives the grade immediately.
    stageCombo()->setCurrentIndex(stageCombo()->findData(schoolStageToStorage(SchoolStage::Primary)));
    const QString primary = resolveGrade(SchoolStage::Primary, 2024, QDate::currentDate()).label();
    QVERIFY2(hint->text().contains(primary), qPrintable(hint->text()));
}

void TestProfilePage::goalFieldMarksDirtyAndRoundTrips()
{
    seedProfile();

    QLineEdit *goalEdit = m_page->findChild<QLineEdit *>(QStringLiteral("profileGoal"));
    QVERIFY(goalEdit);
    // keyClicks is ASCII-only; setText still fires textChanged, the signal the
    // dirty detection relies on (IME commits arrive through it too).
    goalEdit->setText(QStringLiteral("华中师大一附中"));
    QCOMPARE(saveButton()->text(), QStringLiteral("保存修改"));

    saveButton()->click();
    QCOMPARE(saveButton()->text(), QStringLiteral("已保存"));

    auto stored = StudentProfileRepository(*m_database).load();
    QVERIFY(stored.ok());
    QCOMPARE(stored.value()->targetSchool, QStringLiteral("华中师大一附中"));
}

QTEST_MAIN(TestProfilePage)

#include "tst_profile_page.moc"
