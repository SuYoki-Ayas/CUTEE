#include "core/AppPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace cutee {

QString AppPaths::dataDirectory()
{
    const QString override = qEnvironmentVariable("CUTEE_DATA_DIR");
    if (!override.isEmpty())
        return QDir::cleanPath(override);

    QString location = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (location.isEmpty())
        location = QDir::current().filePath(QStringLiteral("cutee-data"));
    return QDir::cleanPath(location);
}

QString AppPaths::ensureDataDirectory()
{
    const QString directory = dataDirectory();
    QDir().mkpath(directory);
    return directory;
}

QString AppPaths::databaseFile()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("cutee.db"));
}

QString AppPaths::logDirectory()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("logs"));
}

QString AppPaths::logFile()
{
    return QDir(logDirectory()).filePath(QStringLiteral("cutee.log"));
}

QString AppPaths::backupDirectory()
{
    return QDir(dataDirectory()).filePath(QStringLiteral("backups"));
}

} // namespace cutee
