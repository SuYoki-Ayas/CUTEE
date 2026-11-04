#include "app/MainWindow.h"
#include "core/AppPaths.h"
#include "core/Logging.h"
#include "storage/Database.h"
#include "storage/SettingsRepository.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CUTEE"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("cutee.local"));
    QCoreApplication::setApplicationName(QStringLiteral("CUTEE"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CUTEE_VERSION));

    cutee::logging::init();
    qInfo().noquote() << QStringLiteral("CUTEE %1 启动，数据目录：%2")
                             .arg(QStringLiteral(CUTEE_VERSION),
                                  cutee::AppPaths::dataDirectory());

    cutee::AppPaths::ensureDataDirectory();

    cutee::Database database;
    auto opened = database.open(cutee::AppPaths::databaseFile());
    if (!opened) {
        qCritical().noquote() << QStringLiteral("打开数据库失败：%1").arg(opened.errorMessage());
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"),
                              QStringLiteral("无法打开数据库：\n%1\n\n数据目录：\n%2")
                                  .arg(opened.errorMessage(), cutee::AppPaths::dataDirectory()));
        return 1;
    }

    auto migrated = database.migrate();
    if (!migrated) {
        qCritical().noquote() << QStringLiteral("数据库迁移失败：%1").arg(migrated.errorMessage());
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"),
                              QStringLiteral("数据库结构初始化失败：\n%1")
                                  .arg(migrated.errorMessage()));
        return 2;
    }

    cutee::SettingsRepository settings(database);
    const auto level = settings.value(QLatin1String(cutee::settings::kLogLevel),
                                      QStringLiteral("info"));
    cutee::logging::setMinimumLevel(
        cutee::logging::levelFromName(level.ok() ? level.value() : QStringLiteral("info")));

    cutee::MainWindow window(database, settings);
    window.show();

    const int exitCode = app.exec();
    qInfo().noquote() << QStringLiteral("CUTEE 退出，代码 %1").arg(exitCode);
    cutee::logging::shutdown();
    return exitCode;
}
