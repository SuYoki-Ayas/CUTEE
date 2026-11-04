#pragma once

#include <QString>

#include <optional>

namespace cutee::scoreutil {

constexpr qint64 kHundredthsPerPoint = 100;

/// Canonical, locale independent rendering: 152.5, 90, 0.25.
QString formatPoints(qint64 hundredths);

/// Parses a decimal string with at most two fractional digits.
/// Accepts a leading '+', rejects thousands separators and exponents.
std::optional<qint64> parsePoints(const QString &text);

/// 0.381 -> "38.1%". Returns "-" when the denominator is not positive.
QString formatRate(double rate);
QString formatRate(qint64 scoreHundredths, qint64 fullScoreHundredths);

double rateOf(qint64 scoreHundredths, qint64 fullScoreHundredths);

} // namespace cutee::scoreutil
