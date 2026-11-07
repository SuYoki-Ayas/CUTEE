#include "app/Theme.h"

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
    return QColor(QString::fromLatin1(hex));
}

/// Explicit light palette so a Windows dark mode setting cannot produce white
/// text on white inputs.
QPalette lightPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, color("#f5f6f8"));
    palette.setColor(QPalette::WindowText, color("#1f2933"));
    palette.setColor(QPalette::Base, color("#ffffff"));
    palette.setColor(QPalette::AlternateBase, color("#fafbfc"));
    palette.setColor(QPalette::Text, color("#1f2933"));
    palette.setColor(QPalette::Button, color("#ffffff"));
    palette.setColor(QPalette::ButtonText, color("#344054"));
    palette.setColor(QPalette::BrightText, color("#ffffff"));
    palette.setColor(QPalette::Highlight, color("#2f6fed"));
    palette.setColor(QPalette::HighlightedText, color("#ffffff"));
    palette.setColor(QPalette::ToolTipBase, color("#1f2930"));
    palette.setColor(QPalette::ToolTipText, color("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, color("#98a2b3"));
    palette.setColor(QPalette::Link, color("#2f6fed"));
    palette.setColor(QPalette::Mid, color("#d0d5dd"));
    palette.setColor(QPalette::Midlight, color("#e4e7ec"));
    palette.setColor(QPalette::Dark, color("#98a2b3"));
    palette.setColor(QPalette::Light, color("#ffffff"));
    palette.setColor(QPalette::Shadow, color("#667085"));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, color("#98a2b3"));
    palette.setColor(QPalette::Disabled, QPalette::Text, color("#98a2b3"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, color("#98a2b3"));
    palette.setColor(QPalette::Disabled, QPalette::Base, color("#f2f4f7"));
    palette.setColor(QPalette::Disabled, QPalette::Button, color("#f2f4f7"));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, color("#d7e2f8"));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, color("#98a2b3"));
    return palette;
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
