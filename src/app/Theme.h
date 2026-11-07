#pragma once

class QApplication;

namespace cutee::theme {

/// Must be called before the QApplication is constructed. Qt 6 enables high DPI
/// scaling by default; PassThrough avoids rounding artefacts on 125%/150% screens.
void applyHighDpiPolicy();

/// Locks the application to a light Fusion look: style, palette and the global
/// style sheet from resources/cutee.qrc. Follows the OS colour scheme otherwise.
void apply(QApplication &app);

} // namespace cutee::theme
