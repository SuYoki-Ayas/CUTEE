#pragma once

#include <QColor>
#include <QString>

namespace cutee::palette {

/// The five brand pastels. They are the only accent colours of the product and
/// are used as surfaces and state colours in the QSS and in model-painted cells.
/// Keep resources/styles/app.qss in sync when changing them.
inline constexpr char kGreen[] = "#C4F2C8";  // success / enabled / empty state
inline constexpr char kPurple[] = "#E6DAF7"; // primary action / selection
inline constexpr char kLime[] = "#E7EBC3";   // warning / low score rate / academic tag
inline constexpr char kBlue[] = "#D8DDF0";   // table header / informational surface
inline constexpr char kPink[] = "#EDD1D1";   // danger / error / over-full score

// Text colours picked for comfortable contrast on the pastels above.
inline constexpr char kTextPrimary[] = "#2F2C36";
inline constexpr char kTextSecondary[] = "#6A6573";
inline constexpr char kTextMuted[] = "#9A94A3";
inline constexpr char kTextOnGreen[] = "#2F6B3D";
inline constexpr char kTextOnPurple[] = "#4A3A6B";
inline constexpr char kTextOnLime[] = "#6E6420";
inline constexpr char kTextOnBlue[] = "#3D4876";
inline constexpr char kTextOnPink[] = "#8B3E3E";

// Neutral surfaces and lines shared by the palette and the style sheet.
inline constexpr char kCanvas[] = "#F7F6FA";
inline constexpr char kSurface[] = "#FFFFFF";
inline constexpr char kSurfaceAlt[] = "#FBFAFD";
inline constexpr char kBorder[] = "#DCD7E6";
inline constexpr char kBorderStrong[] = "#C6BEDB";
inline constexpr char kFocus[] = "#A38FD0";

// Navigation sidebar: a light lavender surface, one step away from the canvas,
// so the whole window stays in the light pastel scheme.
inline constexpr char kSidebar[] = "#EDE9F7";
inline constexpr char kSidebarHover[] = "#E6E0F3";
inline constexpr char kSidebarActive[] = "#E6DAF7";
inline constexpr char kSidebarBorder[] = "#DFD8EE";

// Chart surfaces shared by the trend chart and the summary rate bars.
inline constexpr char kChartLine[] = "#8B7BC0";
inline constexpr char kChartPoint[] = "#B9A6E3";
inline constexpr char kChartTrack[] = "#EFECF6";
inline constexpr char kChartGrid[] = "#E4E0EC";

// Deep plum used for tooltips and other inverted surfaces that need white text.
inline constexpr char kInk[] = "#2E2A3A";

inline QColor color(const char *hex)
{
    return QColor(QString::fromLatin1(hex));
}

} // namespace cutee::palette
