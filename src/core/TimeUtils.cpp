#include "core/TimeUtils.h"

namespace cutee::timeutil {

QString toIso(const QDateTime &dateTime)
{
    if (!dateTime.isValid())
        return {};
    return dateTime.toUTC().toString(Qt::ISODate);
}

QDateTime fromIso(const QString &text)
{
    if (text.isEmpty())
        return {};
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODate);
    if (!parsed.isValid())
        parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid())
        return {};
    return parsed.toUTC();
}

QDateTime nowUtc()
{
    return QDateTime::currentDateTimeUtc();
}

QString formatLocal(const QDateTime &utc, const QString &format)
{
    if (!utc.isValid())
        return QStringLiteral("-");
    return utc.toLocalTime().toString(format);
}

QString formatDuration(qint64 seconds)
{
    if (seconds < 0)
        seconds = 0;
    const qint64 hours = seconds / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

} // namespace cutee::timeutil
