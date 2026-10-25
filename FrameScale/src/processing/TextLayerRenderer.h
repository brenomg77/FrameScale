#pragma once
#include "core/ProcessingOptions.h"
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QImage>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QTransform>
#include <algorithm>
#include <cmath>

namespace framescale {
// Include every text property in comparison cache keys; strings never enter
// FFmpeg filter expressions. Qt renders the same Unicode glyphs for both paths.
inline QJsonObject textLayerKey(const TextLayerOptions& t) {
    return {{"content",QString::fromStdString(t.content)},
        {"font",QString::fromStdString(t.fontFamily)}, {"style",QString::fromStdString(t.fontStyle)},
        {"size",t.fontSize},{"leading",t.lineHeight},{"tracking",t.letterSpacing},
        {"x",t.x},{"y",t.y},{"scaleX",t.horizontalScale},{"scaleY",t.verticalScale},
        {"rotation",t.rotation},{"opacity",t.opacity},{"alignment",t.alignment},{"fill",t.fill},{"stroke",t.stroke},
        {"fillColor",QString::fromStdString(t.fillColor)}, {"strokeColor",QString::fromStdString(t.strokeColor)},
        {"strokeWidth",t.strokeWidth},{"underline",t.underline}};
}
inline QFont textLayerFont(const TextLayerOptions& t) {
    QFont font=QFontDatabase::font(QString::fromStdString(t.fontFamily),QString::fromStdString(t.fontStyle),12);
    font.setPixelSize(qRound(std::clamp(t.fontSize,1.,2000.)));
    font.setLetterSpacing(QFont::AbsoluteSpacing,std::clamp(t.letterSpacing,-100.,500.));
    return font;
}
struct TextLayerGeometry {
    QFont font;
    QStringList lines;
    QVector<double> widths;
    double width=0,height=0,leading=0,lineHeight=0,ascent=0;
    int alignment=1;
    double lineX(int row) const {
        return alignment==0?0:alignment==2?width-widths[row]:(width-widths[row])/2;
    }
    QPointF cursorPoint(int position) const {
        int row=0;
        while(row+1<lines.size() && position>lines[row].size()) {position-=lines[row].size()+1;++row;}
        QTextLayout layout(lines[row],font);layout.beginLayout();auto line=layout.createLine();if(line.isValid()) line.setLineWidth(1.e9);layout.endLayout();
        const double x=line.isValid()?line.cursorToX(std::clamp(position,0,int(lines[row].size()))):0;
        return QPointF(lineX(row)+x,row*leading);
    }
    int cursorAt(const QPointF& point) const {
        const int row=std::clamp(int(std::floor(point.y()/std::max(1.,leading))),0,int(lines.size())-1);
        int position=0;for(int i=0;i<row;++i) position+=lines[i].size()+1;
        QTextLayout layout(lines[row],font);layout.beginLayout();auto line=layout.createLine();if(line.isValid()) line.setLineWidth(1.e9);layout.endLayout();
        return position+(line.isValid()?line.xToCursor(point.x()-lineX(row),QTextLine::CursorBetweenCharacters):0);
    }
};
inline TextLayerGeometry textLayerGeometry(const TextLayerOptions& t) {
    TextLayerGeometry g;g.font=textLayerFont(t);g.lines=QString::fromStdString(t.content).left(10000).split('\n');
    const QFontMetricsF metrics(g.font);
    g.leading=t.lineHeight>0?std::clamp(t.lineHeight,1.,4000.):metrics.lineSpacing();
    g.lineHeight=metrics.height();g.ascent=metrics.ascent();g.alignment=t.alignment;
    for(const auto& line:g.lines) {const double w=metrics.horizontalAdvance(line);g.widths.append(w);g.width=std::max(g.width,w);}
    g.height=g.lineHeight+g.leading*(g.lines.size()-1);return g;
}
inline QRectF textLayerBounds(const TextLayerOptions& t,int width,int height) {
    const auto geometry=textLayerGeometry(t);
    const QFontMetricsF metrics(geometry.font);
    const double w=std::max(geometry.width,metrics.horizontalAdvance(QStringLiteral(" ")));
    const QSizeF size(w*std::clamp(t.horizontalScale,1.,500.)/100.,
        geometry.height*std::clamp(t.verticalScale,1.,500.)/100.);
    QTransform matrix;matrix.translate(width*t.x/100.,height*t.y/100.);matrix.rotate(t.rotation);
    return matrix.mapRect(QRectF(QPointF(-size.width()/2,-size.height()/2),size));
}
inline void paintTextLayer(QPainter& painter,const TextLayerOptions& t,int width,int height) {
    painter.save();
    const auto geometry=textLayerGeometry(t);
    const QFont& font=geometry.font;
    const QFontMetricsF metrics(font);
    const auto& lines=geometry.lines;
    const double leading=geometry.leading;
    const double blockWidth=geometry.width,blockHeight=geometry.height;
    QPainterPath glyphs;
    double baseline=metrics.ascent();
    for(const auto& line:lines) {
        const double lineWidth=metrics.horizontalAdvance(line);
        const double offset=t.alignment==0?0:t.alignment==2?blockWidth-lineWidth:(blockWidth-lineWidth)/2;
        glyphs.addText(QPointF(offset,baseline),font,line);
        if(t.underline && !line.isEmpty()) glyphs.addRect(QRectF(offset,baseline+metrics.underlinePos(),lineWidth,std::max(1.,metrics.lineWidth())));
        baseline+=leading;
    }
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(width*t.x/100.,height*t.y/100.);
    painter.rotate(t.rotation);painter.setOpacity(painter.opacity()*std::clamp(t.opacity,0.,100.)/100.);
    painter.scale(std::clamp(t.horizontalScale,1.,500.)/100.,std::clamp(t.verticalScale,1.,500.)/100.);
    painter.translate(-blockWidth/2,-blockHeight/2);
    if(t.stroke && t.strokeWidth>0) painter.strokePath(glyphs,QPen(QColor(QString::fromStdString(t.strokeColor)),std::clamp(t.strokeWidth,0.,100.),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    if(t.fill) painter.fillPath(glyphs,QColor(QString::fromStdString(t.fillColor)));
    painter.restore();
}
inline QImage renderTextLayer(const TextLayerOptions& t,int width,int height) {
    if(!qobject_cast<QGuiApplication*>(QCoreApplication::instance()) || width<2 || height<2
        || qint64(width)*height>134217728) return {};
    QImage image(width,height,QImage::Format_ARGB32_Premultiplied);
    if(image.isNull()) return {};
    image.fill(Qt::transparent);
    QPainter painter(&image);paintTextLayer(painter,t,width,height);
    return image;
}
}
