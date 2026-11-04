#pragma once

#include <QString>
#include <QStringList>

namespace cutee::logging {

enum class Level {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Critical = 3,
};

/// Installs the Qt message handler and opens the rotating log file.
void init();
void shutdown();

void setMinimumLevel(Level level);
Level minimumLevel();

Level levelFromName(const QString &name);
QString levelName(Level level);

QString logFilePath();

/// Last `maxLines` entries of the log file (oldest first).
QStringList readTail(int maxLines = 2000);

} // namespace cutee::logging
