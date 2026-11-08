#pragma once

#include <QString>

namespace cutee {

/// All mutable application data lives under a single per-user directory so that
/// backups, resets and packaging stay simple. CUTEE_DATA_DIR overrides the
/// location (used by tests and portable deployments).
class AppPaths
{
public:
    static QString dataDirectory();
    static QString ensureDataDirectory();
    static QString databaseFile();
    static QString logDirectory();
    static QString logFile();
    /// Snapshots created by import/reset and by manual export live here.
    static QString backupDirectory();
};

} // namespace cutee
