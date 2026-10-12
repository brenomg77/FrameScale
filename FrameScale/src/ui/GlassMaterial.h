#pragma once
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QScreen>
#include <QVariantMap>
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
inline QImage blur(const QImage& input, int maximum = 160, int radius = 3)
{
    if (input.isNull()) return {};
    QImage image = input.scaled(maximum, maximum, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    image.setDevicePixelRatio(1);
    radius=std::clamp(radius,1,12);
    const int count = radius*2+1;
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
    return image;
}
inline bool updateBackdrop(QWidget* window, const QImage& source, int maximum=96)
{
    if (!window) return false;
    if (reduced() || source.isNull()) {
        const bool changed=window->property("glassSourceKey").isValid()
            || !window->property("glassBackdrop").value<QImage>().isNull();
        window->setProperty("glassSourceKey", QVariant());
        window->setProperty("glassBackdrop", QVariant());
        return changed;
    }
    // A theme change can clear the image while the decoded frame stays the
    // same. The frame key alone must not suppress rebuilding the material.
    if (window->property("glassSourceKey").toLongLong()==source.cacheKey()
        && !window->property("glassBackdrop").value<QImage>().isNull()) return false;
    window->setProperty("glassSourceKey",source.cacheKey());
    window->setProperty("glassBackdrop",blur(source,maximum));
    return true;
}
inline void paintTint(QPainter& painter,const QRectF& area,bool light,int alpha,qreal radius=0)
{
    QLinearGradient tint(area.topLeft(),area.bottomRight());
    tint.setColorAt(0,light?QColor(255,255,255,qMin(255,alpha+10)):QColor(47,48,52,qMin(255,alpha+8)));
    tint.setColorAt(.45,light?QColor(250,250,252,alpha):QColor(34,35,39,alpha));
    tint.setColorAt(1,light?QColor(238,239,244,qMax(0,alpha-6)):QColor(27,28,32,qMax(0,alpha-4)));
    painter.fillRect(area,tint);
    if(radius>0) {
        // A narrow specular rim gives the rounded material depth without
        // bright lines across the content or a heavy second outline.
        QLinearGradient rim(area.topLeft(),area.bottomLeft());
        rim.setColorAt(0,QColor(255,255,255,light?130:62));
        rim.setColorAt(.35,QColor(255,255,255,light?55:25));
        rim.setColorAt(1,QColor(255,255,255,12));
        painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QBrush(rim),1));
        painter.drawRoundedRect(area.adjusted(.5,.5,-.5,-.5),radius,radius);
    }
}
// Keep the top-level surface genuinely translucent. Never place an opaque
// desktop snapshot or a stretched media frame below this tint.
inline void paintWindowSurface(QPainter& painter,const QRectF& area,bool light)
{
    if(reduced()) painter.fillRect(area,light?QColor("#fafafa"):QColor("#292929"));
    else paintTint(painter,area,light,light?165:112);
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
    // Alpha composition works without Windows' native transparency effects.
    // An absent cached image must not turn a translucent surface opaque.
    if (!translucent || solid)
        painter.fillRect(area,light ? QColor("#fafafa") : QColor("#292929"));
    if (!solid && !backdrop.isNull()) {
        painter.setOpacity(translucent ? 0.84 : 0.12);
        painter.drawImage(area,backdrop);
        painter.setOpacity(1.0);
    }
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
        // A high white luminance floor washed out the light material, unlike
        // the dark variant. Keep enough contrast for ink without hiding colour.
        if (light && low<165) alpha=qMax(alpha,qRound(255.0*(165-low)/(255-low)));
        if (!light && high>70) alpha=qMax(alpha,qRound(255.0*(high-70)/(high-32)));
    }
    if(solid) painter.fillRect(area,light?QColor("#fafafa"):QColor("#292929"));
    else paintTint(painter,area,light,alpha,radius);
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
