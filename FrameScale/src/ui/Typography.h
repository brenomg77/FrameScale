#pragma once
#include <QApplication>
#include <QFontDatabase>
namespace Typography {
inline QString textFamily, displayFamily;
inline void apply()
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    // Use fonts already provided by the operating system. No font binaries
    // are embedded or redistributed with FrameScale.
#ifdef Q_OS_WIN
    if (QFontDatabase::families().contains(QStringLiteral("Segoe UI")))
        font.setFamily(QStringLiteral("Segoe UI"));
#endif
    textFamily = displayFamily = font.family();
    font.setPixelSize(14);
    font.setWeight(QFont::Medium);
    font.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(font);
}
inline QFont display(int pixels=22)
{
    QFont font(displayFamily.isEmpty()?textFamily:displayFamily);
    font.setPixelSize(pixels);
    font.setWeight(QFont::DemiBold);
    return font;
}
}
