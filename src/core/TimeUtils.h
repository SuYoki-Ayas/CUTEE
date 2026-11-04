#pragma once

#include <QDateTime>
#include <QString>

namespace cutee::timeutil {

/// All persisted timestamps are UTC ISO-8601 with a trailing Z.
QString toIso(const QDateTime &dateTime);
QDateTime fromIso(const QString &text);

QDateTime nowUtc();

/// Local wall-clock rendering of a stored UTC timestamp.
QString formatLocal(const QDateTime &utc, const QString &format = QStringLiteral("yyyy-MM-dd HH:mm:ss"));

QString formatDuration(qint64 seconds);

} // namespace cutee::timeutil
