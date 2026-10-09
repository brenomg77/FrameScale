#pragma once
#include <QApplication>
#include <QIconEngine>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>

// Small, scalable monochrome symbols; no emoji or platform font dependency.
namespace ControlSymbols {
enum class Symbol { None, Expand, Frames, Sparkle, Chip, Layers, Sliders, Noise,
    Contrast, Color, Grid, Gradient, Eye, Queue, Plus, Reset, Folder, Save, Export, Audio, Scale, FrameRate, Sharpen, Denoise, Import, Zoom, RotateLeft, RotateRight, FlipHorizontal, FlipVertical, Link, Unlink, Trash, Info };

inline Symbol forControl(const QString& name)
{
    if (name == "welcomeNewButton") return Symbol::Plus;
    if (name == "welcomeOpenButton") return Symbol::Folder;
    if (name == "welcomeUrlButton") return Symbol::Import;
    if (name == "enableUpscale") return Symbol::Expand;
    if (name == "upscaleFactor") return Symbol::Scale;
    if (name == "enableInterpolation") return Symbol::Frames;
    if (name == "targetFps") return Symbol::FrameRate;
    if (name == "enableEnhancement") return Symbol::Sparkle;
    if (name == "enhanceSharpen") return Symbol::Sharpen;
    if (name == "upscaleEngine" || name == "videoCodec") return Symbol::Chip;
    if (name == "upscaleModel" || name == "rifeModel" || name == "encoderProfile"
        || name == "encoderProresProfile") return Symbol::Layers;
    if (name == "denoiseLevel" || name == "enhanceDenoise") return Symbol::Denoise;
    if (name == "enhanceGrain") return Symbol::Noise;
    if (name == "enhanceDeblock" || name == "encoderPixelFormat") return Symbol::Grid;
    if (name == "enhanceDeband") return Symbol::Gradient;
    if (name == "enhanceContrast") return Symbol::Contrast;
    if (name == "enhanceVibrance") return Symbol::Color;
    if (name == "comparePreviewButton") return Symbol::Eye;
    if (name == "viewQueueButton") return Symbol::Queue;
    if (name == "addToQueueButton") return Symbol::Plus;
    if (name == "resetEnhancement") return Symbol::Reset;
    if (name == "formatOptionsButton" || name == "encoderRateControl" || name == "encoderCrf"
        || name == "encoderSpeed" || name == "encoderLevel" || name == "encoderTargetRate") return Symbol::Sliders;
    if (name == "exportBrowseButton" || name == "exportOpenFolderButton") return Symbol::Folder;
    if (name == "importEncoderPreset") return Symbol::Import;
    if (name == "saveEncoderPreset") return Symbol::Save;
    if (name == "exportStartButton" || name == "encoderSuffix") return Symbol::Export;
    if (name.startsWith("encoderAudio") || name == "encoderKeepAudio" || name == "encoderSampleRate" || name == "encoderChannels") return Symbol::Audio;
    return Symbol::None;
}

inline void paint(QPainter& p, QRectF area, Symbol symbol, const QColor& ink)
{
    p.save(); p.setRenderHint(QPainter::Antialiasing);
    p.translate(area.topLeft()); p.scale(area.width()/20., area.height()/20.);
    p.setPen(QPen(ink, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    auto line = [&](qreal x, qreal y, qreal xx, qreal yy) {p.drawLine(QPointF(x,y),QPointF(xx,yy));};
    auto box = [&](qreal x,qreal y,qreal w,qreal h) {p.drawRoundedRect(QRectF(x,y,w,h),2,2);};
    switch (symbol) {
    case Symbol::Trash:
        line(3,5,17,5); box(5,5,10,13); line(8,2,12,2); line(8,9,8,15); line(12,9,12,15); break;
    case Symbol::Info:
        p.drawEllipse(QRectF(2,2,16,16)); line(10,9,10,15); line(10,5,10,5.5); break;
    case Symbol::RotateLeft:
    case Symbol::RotateRight:
        if (symbol == Symbol::RotateRight) { p.translate(20,0); p.scale(-1,1); }
        p.drawArc(QRectF(3,3,14,14), 135*16, -300*16);
        line(3,3,3,8); line(3,8,8,8); break;
    case Symbol::FlipHorizontal:
    case Symbol::FlipVertical: {
        if (symbol == Symbol::FlipVertical) { p.translate(20,0); p.rotate(90); }
        p.setPen(QPen(ink,1,Qt::DashLine)); line(10,2,10,18);
        p.setPen(QPen(ink,1.45,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        QPolygonF triangle; triangle << QPointF(7,4) << QPointF(2,10) << QPointF(7,16);
        p.drawPolygon(triangle); p.translate(20,0); p.scale(-1,1); p.drawPolygon(triangle); break;
    }
    case Symbol::Link:
    case Symbol::Unlink:
        p.translate(10,10); p.rotate(-35); p.translate(-10,-10);
        p.drawRoundedRect(QRectF(1.5,6,10,8),4,4);
        p.drawRoundedRect(QRectF(8.5,6,10,8),4,4);
        if (symbol == Symbol::Unlink) { p.setPen(QPen(ink,2)); line(10,1,10,4); line(10,16,10,19); }
        break;
    case Symbol::Expand:
        line(3,8,3,3);line(3,3,8,3);line(3,3,8,8);
        line(17,12,17,17);line(17,17,12,17);line(17,17,12,12);break;
    case Symbol::Frames: box(2,5,12,12);box(6,2,12,12);break;
    case Symbol::Layers: {
        QPolygonF diamond;diamond<<QPointF(10,2)<<QPointF(18,7)<<QPointF(10,12)<<QPointF(2,7);
        p.drawPolygon(diamond);line(2,11,10,16);line(10,16,18,11);break; }
    case Symbol::Sparkle: {
        QPainterPath path;path.moveTo(8,2);path.quadTo(8,8,2,9);path.quadTo(8,10,8,17);
        path.quadTo(9,10,15,9);path.quadTo(9,8,8,2);p.drawPath(path);
        line(16,2,16,6);line(14,4,18,4);break; }
    case Symbol::Chip: box(5,5,10,10);box(8,8,4,4);
        for(int n: {7,13}) {line(n,2,n,5);line(n,15,n,18);line(2,n,5,n);line(15,n,18,n);}break;
    case Symbol::Sliders:
        line(3,5,17,5);line(3,10,17,10);line(3,15,17,15);
        line(7,3,7,7);line(13,8,13,12);line(8,13,8,17);break;
    case Symbol::Noise:
        p.setBrush(ink);p.setPen(Qt::NoPen);
        for(int y=4;y<=16;y+=4)for(int x=4;x<=16;x+=4)p.drawEllipse(QPointF(x,y),.8,.8);break;
    case Symbol::Contrast: {
        p.drawEllipse(QRectF(3,3,14,14));QPainterPath half;half.moveTo(10,3);
        half.arcTo(QRectF(3,3,14,14),90,180);half.closeSubpath();p.fillPath(half,ink);break; }
    case Symbol::Color: p.drawEllipse(QRectF(6,2,8,8));p.drawEllipse(QRectF(2,9,8,8));p.drawEllipse(QRectF(10,9,8,8));break;
    case Symbol::Grid: for(int y: {3,11})for(int x: {3,11})box(x,y,6,6);break;
    case Symbol::Gradient: for(int y=4;y<=16;y+=3)line(3,y,17-(y-4)*.45,y);break;
    case Symbol::Eye: {
        QPainterPath eye;eye.moveTo(1,10);eye.quadTo(10,-1,19,10);eye.quadTo(10,21,1,10);p.drawPath(eye);
        p.drawEllipse(QRectF(7,7,6,6));break; }
    case Symbol::Queue: for(int y: {5,10,15}){line(7,y,17,y);p.drawPoint(QPointF(3,y));}break;
    case Symbol::Plus: line(10,3,10,17);line(3,10,17,10);break;
    case Symbol::Reset: p.drawArc(QRectF(3,3,14,14),140*16,-295*16);line(3,3,3,7);line(3,7,7,7);break;
    case Symbol::Folder: {QPainterPath path;path.moveTo(2,6);path.lineTo(2,4);path.lineTo(8,4);path.lineTo(10,6);path.lineTo(18,6);path.lineTo(18,16);path.lineTo(2,16);path.closeSubpath();p.drawPath(path);break;}
    case Symbol::Save: box(3,3,14,14);box(6,3,8,5);box(6,12,8,5);break;
    case Symbol::Export: line(3,11,3,17);line(3,17,17,17);line(17,17,17,11);line(10,13,10,2);line(6,6,10,2);line(14,6,10,2);break;
    case Symbol::Audio: {QPolygonF speaker;speaker<<QPointF(3,8)<<QPointF(6,8)<<QPointF(10,4)<<QPointF(10,16)<<QPointF(6,12)<<QPointF(3,12);p.drawPolygon(speaker);p.drawArc(QRectF(7,3,10,14),-65*16,130*16);break;}
    case Symbol::Scale:
        box(3,3,14,14);line(7,13,13,7);line(9,7,13,7);line(13,7,13,11);break;
    case Symbol::FrameRate:
        p.drawArc(QRectF(3,4,14,14),0,180*16);
        line(3,11,3,15);line(17,11,17,15);line(3,15,6,15);line(14,15,17,15);
        line(10,12,14,7);p.drawEllipse(QRectF(8.5,10.5,3,3));break;
    case Symbol::Sharpen: {
        QPolygonF edge;edge<<QPointF(10,2)<<QPointF(18,17)<<QPointF(2,17);
        p.drawPolygon(edge);QPainterPath half;half.moveTo(10,2);half.lineTo(18,17);half.lineTo(10,17);half.closeSubpath();p.fillPath(half,ink);break; }
    case Symbol::Denoise: {
        QPainterPath wave;wave.moveTo(2,10);wave.cubicTo(5,3,6,17,10,10);wave.cubicTo(13,4,14,13,18,10);p.drawPath(wave);
        p.drawPoint(QPointF(4,4));p.drawPoint(QPointF(9,3));p.drawPoint(QPointF(15,16));break; }
    case Symbol::Import:
        line(3,12,3,17);line(3,17,17,17);line(17,17,17,12);
        line(10,2,10,12);line(6,8,10,12);line(14,8,10,12);break;
    case Symbol::Zoom: p.drawEllipse(QRectF(3,3,10,10));line(12,12,17,17);break;
    case Symbol::None: break;
    }
    p.restore();
}
class Engine final : public QIconEngine {
public:
    Engine(Symbol s,bool primary):symbol(s),white(primary){}
    QIconEngine* clone() const override {return new Engine(symbol,white);}
    void paint(QPainter* p,const QRect& r,QIcon::Mode mode,QIcon::State) override {
        QColor ink = white ? QColor(Qt::white) : qApp->palette().color(QPalette::ButtonText);
        if(mode==QIcon::Disabled) ink=qApp->palette().color(QPalette::Disabled,QPalette::ButtonText);
        // Leave four pixels after the symbol, in addition to Qt's text gap.
        const int side = qMin(r.width(),r.height());
        const QRectF glyph(r.left(),r.top()+(r.height()-side)/2.,side,side);
        ControlSymbols::paint(*p,glyph,symbol,ink);
    }
    QPixmap pixmap(const QSize& size,QIcon::Mode mode,QIcon::State state) override {
        QPixmap result(size);result.fill(Qt::transparent);QPainter p(&result);paint(&p,result.rect(),mode,state);return result;
    }
private: Symbol symbol;bool white;
};
inline QIcon icon(Symbol symbol,bool primary=false) {return QIcon(new Engine(symbol,primary));}
class Label final : public QLabel {
public:
    Label(const QString& text,Symbol symbol,QWidget* parent=nullptr):QLabel(text,parent),symbol_(symbol) {
        // QSS replaces QWidget contents margins when it polishes QLabel.
        // QLabel's own indent survives that polish and reserves the icon column
        // in both text painting and size hints, including after theme changes.
        if(symbol!=Symbol::None)setIndent(27);
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setMinimumHeight(20);
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        QLabel::paintEvent(event);
        if(symbol_==Symbol::None)return;
        QPainter p(this);auto ink=palette().color(isEnabled()?QPalette::Active:QPalette::Disabled,QPalette::WindowText);ink.setAlpha(190);
        paint(p,QRectF(1,(height()-16)/2.,16,16),symbol_,ink);
    }
private:Symbol symbol_;
};
}
