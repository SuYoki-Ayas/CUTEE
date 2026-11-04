#include "core/ScoreValue.h"

#include <QRegularExpression>

#include <cmath>

namespace cutee::scoreutil {
namespace {

QString trimZeros(QString text)
{
    if (!text.contains(QLatin1Char('.')))
        return text;
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text;
}

} // namespace

QString formatPoints(qint64 hundredths)
{
    const bool negative = hundredths < 0;
    const qint64 magnitude = negative ? -hundredths : hundredths;
    const qint64 whole = magnitude / kHundredthsPerPoint;
    const qint64 fraction = magnitude % kHundredthsPerPoint;

    QString text = QString::number(whole);
    if (fraction != 0) {
        text += QLatin1Char('.');
        text += QStringLiteral("%1").arg(fraction, 2, 10, QLatin1Char('0'));
        text = trimZeros(text);
    }
    if (negative && (whole != 0 || fraction != 0))
        text.prepend(QLatin1Char('-'));
    return text;
}

std::optional<qint64> parsePoints(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("^\\+?(\\d{1,9})(?:\\.(\\d{1,2})?)?$"));
    const QString trimmed = text.trimmed();
    const QRegularExpressionMatch match = pattern.match(trimmed);
    if (!match.hasMatch())
        return std::nullopt;

    const qint64 whole = match.captured(1).toLongLong();
    QString fraction = match.captured(2);
    if (fraction.isEmpty())
        fraction = QStringLiteral("0");
    else if (fraction.size() == 1)
        fraction += QLatin1Char('0');

    return whole * kHundredthsPerPoint + fraction.toLongLong();
}

QString formatRate(double rate)
{
    if (!std::isfinite(rate))
        return QStringLiteral("-");
    return QStringLiteral("%1%").arg(rate * 100.0, 0, 'f', 1);
}

QString formatRate(qint64 scoreHundredths, qint64 fullScoreHundredths)
{
    if (fullScoreHundredths <= 0)
        return QStringLiteral("-");
    return formatRate(rateOf(scoreHundredths, fullScoreHundredths));
}

double rateOf(qint64 scoreHundredths, qint64 fullScoreHundredths)
{
    if (fullScoreHundredths <= 0)
        return 0.0;
    return static_cast<double>(scoreHundredths) / static_cast<double>(fullScoreHundredths);
}

} // namespace cutee::scoreutil
