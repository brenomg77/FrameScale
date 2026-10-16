#pragma once
#include "Appearance.h"
#include <QWidget>
#include <QPainter>
#include <QVariantAnimation>

// Keep the destination project's last view on screen while its decoders reload.
// This is a bounded display cache, never an input to processing or export.
class ProjectTransition final : public QWidget {
public:
    explicit ProjectTransition(QWidget* parent):QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        fade_.setDuration(160);fade_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&fade_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){opacity_=value.toReal();update();});
        connect(&fade_,&QVariantAnimation::finished,this,[this]{hide();frame_={};});
        hide();
    }
    void present(const QPixmap& frame) {
        fade_.stop();opacity_=1;frame_=frame;setGeometry(parentWidget()->rect());show();raise();update();
    }
    void reveal() {
        if(isHidden() || fade_.state()==QAbstractAnimation::Running) return;
        if(Appearance::reduceMotion()) {hide();frame_={};return;}
        fade_.setStartValue(1.);fade_.setEndValue(0.);fade_.start();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setOpacity(opacity_);
        if(frame_.isNull()) painter.fillRect(rect(),QColor("#181818"));
        else painter.drawPixmap(rect(),frame_);
    }
private:
    QPixmap frame_;QVariantAnimation fade_;qreal opacity_=1;
};
