#include "app/Theme.h"

#include "ui/Palette.h"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QTranslator>

namespace cutee::theme {
namespace {

QColor color(const char *hex)
{
    return palette::color(hex);
}

/// Explicit light palette so a Windows dark mode setting cannot produce white
/// text on white inputs. Accent surfaces come from the five brand pastels.
QPalette lightPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, color(palette::kCanvas));
    p.setColor(QPalette::WindowText, color(palette::kTextPrimary));
    p.setColor(QPalette::Base, color(palette::kSurface));
    p.setColor(QPalette::AlternateBase, color(palette::kSurfaceAlt));
    p.setColor(QPalette::Text, color(palette::kTextPrimary));
    p.setColor(QPalette::Button, color(palette::kSurface));
    p.setColor(QPalette::ButtonText, color("#4A4655"));
    p.setColor(QPalette::BrightText, color("#FFFFFF"));
    p.setColor(QPalette::Highlight, color(palette::kPurple));
    p.setColor(QPalette::HighlightedText, color("#3A2E5C"));
    p.setColor(QPalette::ToolTipBase, color(palette::kInk));
    p.setColor(QPalette::ToolTipText, color("#FFFFFF"));
    p.setColor(QPalette::PlaceholderText, color(palette::kTextMuted));
    p.setColor(QPalette::Link, color("#7A63B8"));
    p.setColor(QPalette::LinkVisited, color("#5C4A8C"));
    p.setColor(QPalette::Mid, color(palette::kBorder));
    p.setColor(QPalette::Midlight, color("#E4E0EC"));
    p.setColor(QPalette::Dark, color("#B3AAC4"));
    p.setColor(QPalette::Light, color("#FFFFFF"));
    p.setColor(QPalette::Shadow, color(palette::kTextSecondary));

    p.setColor(QPalette::Disabled, QPalette::WindowText, color("#A9A2B6"));
    p.setColor(QPalette::Disabled, QPalette::Text, color("#A9A2B6"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, color("#A9A2B6"));
    p.setColor(QPalette::Disabled, QPalette::Base, color("#F2F0F6"));
    p.setColor(QPalette::Disabled, QPalette::Button, color("#F2F0F6"));
    p.setColor(QPalette::Disabled, QPalette::Highlight, color("#EFEAF8"));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, color("#A9A2B6"));
    return p;
}

} // namespace

void applyHighDpiPolicy()
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

void apply(QApplication &app)
{
    if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
        app.setStyle(fusion);

    app.setPalette(lightPalette());

    QFont font = app.font();
    font.setFamilies({QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Segoe UI"),
                      QStringLiteral("Microsoft YaHei")});
    font.setPointSizeF(9.5);
    app.setFont(font);

    // Standard dialogs (file chooser, input dialogs) should follow the Chinese UI.
    auto *translator = new QTranslator(&app);
    if (translator->load(QLocale(QLocale::Chinese, QLocale::China), QStringLiteral("qtbase"),
                         QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(translator);
    else
        delete translator;

    QFile styleSheet(QStringLiteral(":/styles/app.qss"));
    if (!styleSheet.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("无法加载样式表 :/styles/app.qss，界面将回退到 Fusion 默认外观");
        return;
    }
    const QString sheet = QString::fromUtf8(styleSheet.readAll());
    app.setStyleSheet(sheet);
    qInfo().noquote() << QStringLiteral("已应用浅色主题样式表（%1 字符）").arg(sheet.size());
}

} // namespace cutee::theme
