#pragma once

#include <QString>

#include "core/Result.h"

namespace cutee {

class Database;

/// Database-level maintenance: export, import, auto-backup and reset. The UI
/// only calls these, so no page ever touches the SQLite files directly.
namespace maintenance {

/// Writes a consistent snapshot of the live database to `targetPath` using
/// `VACUUM INTO`, which is safe while the connection is open (WAL included).
/// An existing file at `targetPath` is replaced.
Result<void> exportSnapshot(Database &database, const QString &targetPath);

/// Stores a snapshot under `<data>/backups/` and returns the created path.
Result<QString> createBackup(Database &database, const QString &label);

/// Checks that `path` is a readable CUTEE database and returns its schema
/// version. The file is opened read-only and left untouched.
Result<int> inspectDatabaseFile(const QString &path);

/// Replaces the live data with `sourcePath`:
/// auto-backup → close → swap the file → reopen → migrate. On any failure the
/// previous database is restored, so the app never ends up without data.
/// Returns the path of the backup created beforehand.
Result<QString> importDatabaseFile(Database &database, const QString &sourcePath);

/// Deletes every profile, subject and score row. Schema and `settings` (log
/// level, window geometry, column configuration) are kept.
Result<void> resetAllData(Database &database);

} // namespace maintenance
} // namespace cutee
