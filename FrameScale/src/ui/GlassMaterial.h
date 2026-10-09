#pragma once
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QStyleHints>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <QAccessibilityHints>
#endif
#include <QWidget>
#include <algorithm>

namespace GlassMaterial {
inline bool reduced()
{
    // Studio is always opaque, including custom-painted panels and popups.
    // It must not inherit the Liquid Glass transparency preference.
    if (QSettings().value("appearance", "macos-light").toString() == "studio") return true;
    #if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    if (qApp && qApp->styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast) return true;
    #endif
    // macOS appearance is an explicit in-app material choice. Keep the OS
    // high-contrast override, and let users disable translucency here without
    // modifying the desktop's transparency preference.
    return !QSettings().value("glassTransparency", true).toBool();
}
// Cache a small, genuinely blurred raster rather than attaching a live blur
// effect to video playback. Three separable box passes approximate a Gaussian.
inline QImage blur(const QImage& input, int maximum = 160)
{
    if (input.isNull()) return {};
    QImage image = input.scaled(maximum, maximum, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    image.setDevicePixelRatio(1);
    const int radius = 4, count = radius*2+1;
    for (int pass=0; pass<3; ++pass) {
        QImage work(image.size(),QImage::Format_RGB32);
        for (int y=0;y<image.height();++y) {
            int r=0,g=0,b=0;
            auto sample=[&](int x) {return image.pixel(std::clamp(x,0,image.width()-1),y);};
            for(int k=-radius;k<=radius;++k) {auto p=sample(k);r+=qRed(p);g+=qGreen(p);b+=qBlue(p);}
            auto* line=reinterpret_cast<QRgb*>(work.scanLine(y));
            for(int x=0;x<image.width();++x) {line[x]=qRgb(r/count,g/count,b/count);auto a=sample(x-radius),z=sample(x+radius+1);r+=qRed(z)-qRed(a);g+=qGreen(z)-qGreen(a);b+=qBlue(z)-qBlue(a);}
        }
        for(int x=0;x<image.width();++x) {
            int r=0,g=0,b=0;
            auto sample=[&](int y) {return work.pixel(x,std::clamp(y,0,work.height()-1));};
            for(int k=-radius;k<=radius;++k) {auto p=sample(k);r+=qRed(p);g+=qGreen(p);b+=qBlue(p);}
            for(int y=0;y<image.height();++y) {reinterpret_cast<QRgb*>(image.scanLine(y))[x]=qRgb(r/count,g/count,b/count);auto a=sample(y-radius),z=sample(y+radius+1);r+=qRed(z)-qRed(a);g+=qGreen(z)-qGreen(a);b+=qBlue(z)-qBlue(a);}
        }
    }
    // Neutral material: source content supplies depth, never a colored UI tint.
    for (int y=0; y<image.height(); ++y) {
        auto* line=reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x=0; x<image.width(); ++x) {
            const int gray=qGray(line[x]); line[x]=qRgb(gray,gray,gray);
        }
    }
    return image;
}
inline void paint(QPainter& painter, const QRectF& area, const QImage& backdrop, bool light, int tint, qreal radius=0, bool translucent=false)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;clip.addRoundedRect(area,radius,radius);painter.setClipPath(clip);
    if (QSettings().value("appearance", "macos-light").toString() == "studio") {
        painter.fillRect(area, QColor("#292929"));
        painter.restore();
        return;
    }
    const bool solid = reduced();
    if (!translucent || solid || backdrop.isNull())
        painter.fillRect(area,light ? QColor("#fafafa") : QColor("#292929"));
    if (!solid && !backdrop.isNull()) {
        painter.setOpacity(translucent ? 0.94 : 0.10);
        painter.drawImage(area,backdrop);
        painter.setOpacity(1.0);
    }
    QLinearGradient fill(area.topLeft(),translucent ? area.bottomRight() : area.bottomLeft());
    int alpha=solid?255:tint;
    // Keep text legible when a floating menu overlaps a dark video or a bright
    // canvas. Adapt the tint instead of making text and icons translucent.
    if (translucent && !solid && !backdrop.isNull()) {
        int low=255, high=0;
        for (int y=0;y<backdrop.height();y+=qMax(1,backdrop.height()/8))
            for (int x=0;x<backdrop.width();x+=qMax(1,backdrop.width()/8)) {
                const int luminance=qGray(backdrop.pixel(x,y));
                low=qMin(low,luminance);high=qMax(high,luminance);
            }
        if (light && low<205) alpha=qMax(alpha,qRound(255.0*(205-low)/(255-low)));
        if (!light && high>70) alpha=qMax(alpha,qRound(255.0*(high-70)/(high-32)));
    }
    const QColor neutral = light ? QColor(255,255,255,alpha) : QColor(41,41,41,alpha);
    fill.setColorAt(0,neutral);
    fill.setColorAt(1,neutral);
    painter.fillRect(area,fill);
    painter.restore();
}
}
class GlassBackdrop final : public QWidget {
public:
    explicit GlassBackdrop(QWidget* parent=nullptr):QWidget(parent){}
protected:
    void paintEvent(QPaintEvent*) override {
        if (window()->property("glassWindowSurface").toBool()) return;
        QPainter painter(this);
        const QString theme=QSettings().value("appearance","macos-light").toString();
        if(theme=="studio") {painter.fillRect(rect(),QColor("#1e1e1e"));return;}
        GlassMaterial::paint(painter,rect(),window()->property("glassBackdrop").value<QImage>(),theme!="macos-dark",theme=="macos-dark"?155:120);
    }
};
