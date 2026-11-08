#pragma once

#include <QString>

#include "core/Result.h"

namespace cutee {

class Database;

/// Key/value application settings. Values are always stored as text.
class SettingsRepository
{
public:
    explicit SettingsRepository(Database &database);

    Result<QString> value(const QString &key, const QString &defaultValue = {}) const;
    Result<void> setValue(const QString &key, const QString &value);
    Result<void> remove(const QString &key);

private:
    Database &m_database;
};

namespace settings {

inline constexpr char kLogLevel[] = "logging.level";
inline constexpr char kLastPage[] = "ui.last_page";
inline constexpr char kWindowGeometry[] = "ui.window_geometry";
inline constexpr char kWindowState[] = "ui.window_state";
inline constexpr char kWindowMaximized[] = "ui.window_maximized";
inline constexpr char kScoreHiddenColumns[] = "ui.score_hidden_columns";

} // namespace settings

} // namespace cutee
