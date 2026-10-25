#pragma once
#include "core/ProcessingOptions.h"
#include "ControlSymbols.h"
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QToolButton>
#include <QPushButton>
#include <functional>
#include <QMouseEvent>
#include <QLineEdit>
#include <QPainter>

// Paint the icon and hover surface in the same coordinates. Native style
// padding and application-wide button rules must not offset either one.
class LayerIconButton final : public QToolButton {
public:
    explicit LayerIconButton(QWidget* parent):QToolButton(parent) {
        setFixedSize(26,26);setIconSize(QSize(16,16));
        setProperty("appearanceManaged",true);setCursor(Qt::PointingHandCursor);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        if(underMouse() || isDown()) {
            painter.setPen(Qt::NoPen);painter.setBrush(QColor(128,128,128,isDown()?85:45));
            painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),5,5);
        }
        const QRect iconRect((width()-16)/2,(height()-16)/2,16,16);
        icon().paint(&painter,iconRect,Qt::AlignCenter,isEnabled()?QIcon::Normal:QIcon::Disabled);
    }
};

class TransformValue final : public QDoubleSpinBox {
public:
 explicit TransformValue(QWidget* p):QDoubleSpinBox(p) {setButtonSymbols(QAbstractSpinBox::NoButtons);lineEdit()->setReadOnly(true);lineEdit()->setCursor(Qt::SizeHorCursor);lineEdit()->setMouseTracking(true);lineEdit()->installEventFilter(this);setStyleSheet("QDoubleSpinBox {background:transparent;border:0;color:#168bff;padding:2px;} QLineEdit {background:transparent;border:0;color:#168bff;}");}
protected:
 bool eventFilter(QObject* o,QEvent* e) override {
 if(o==lineEdit()) {
 if(e->type()==QEvent::MouseMove && !drag_ && lineEdit()->isReadOnly()) {auto* m=static_cast<QMouseEvent*>(e);lineEdit()->setCursor(m->position().x()<=lineEdit()->fontMetrics().horizontalAdvance(lineEdit()->text())+6?Qt::SizeHorCursor:Qt::ArrowCursor);}
 if(e->type()==QEvent::MouseButtonDblClick && static_cast<QMouseEvent*>(e)->position().x()<=lineEdit()->fontMetrics().horizontalAdvance(lineEdit()->text())+6) {lineEdit()->setReadOnly(false);lineEdit()->selectAll();return true;}
 if(e->type()==QEvent::MouseButtonPress && lineEdit()->isReadOnly() && static_cast<QMouseEvent*>(e)->position().x()<=lineEdit()->fontMetrics().horizontalAdvance(lineEdit()->text())+6) {auto* m=static_cast<QMouseEvent*>(e);if(m->button()==Qt::LeftButton){origin_=m->globalPosition().x();initial_=value();drag_=true;return true;}}
 if(e->type()==QEvent::MouseMove && drag_) {auto* m=static_cast<QMouseEvent*>(e);setValue(initial_+(m->globalPosition().x()-origin_)*(m->modifiers().testFlag(Qt::ShiftModifier)?.1:1.));return true;}
 if(e->type()==QEvent::MouseButtonRelease && drag_) {drag_=false;return true;}
 if(e->type()==QEvent::FocusOut) lineEdit()->setReadOnly(true);
 } return QDoubleSpinBox::eventFilter(o,e);}
private: bool drag_=false;double origin_=0,initial_=0;
};

class LayerTransformControls final : public QWidget {
public:
    std::function<void(const framescale::LayerTransform&)> changed;
    explicit LayerTransformControls(QWidget* parent):QWidget(parent) {
        setObjectName("layerTransformPanel");
        auto* form=new QFormLayout(this);form->setContentsMargins(0,0,0,0);
        const QStringList labels={tr("Posição"),tr("Escala"),tr("Rotação"),tr("Opacidade")};
        const char* names[]={"layerPositionX","layerPositionY","layerScaleX","layerScaleY","layerRotation","layerOpacity"};
        for(int row=0;row<4;++row) {
            auto* pair=new QWidget(this);auto* layout=new QHBoxLayout(pair);layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
            const int first=row<2?row*2:row+2,last=row<2?first+1:first;
            for(int i=first;i<=last;++i) {
                auto* spin=new TransformValue(pair);fields_[i]=spin;spin->setObjectName(names[i]);spin->setDecimals(1);spin->setKeyboardTracking(false);
                spin->setRange(i<2?-100000:i==4?-36000:i==5?0:1,i<2?100000:i==4?36000:i==5?100:500);
                spin->setSuffix(i<2?QStringLiteral(" px"):i==4?QString::fromUtf8("°"):QStringLiteral(" %"));
                spin->setAccessibleName(i==0?tr("Posição X"):i==1?tr("Posição Y"):i==2?tr("Escala horizontal"):i==3?tr("Escala vertical"):labels[row]);
                spin->setFixedWidth(68);layout->addWidget(spin,0,Qt::AlignLeft);
                if(i==0) {auto* spacer=new QWidget(pair);spacer->setFixedSize(26,26);layout->addWidget(spacer);}
                if(i==2) {link_=new LayerIconButton(pair);link_->setCheckable(true);link_->setChecked(true);link_->setFixedSize(26,26);link_->setProperty("appearanceManaged",true);link_->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Link));link_->setToolTip(tr("Unir/desunir escala"));layout->addWidget(link_,0,Qt::AlignVCenter);connect(link_,&QToolButton::toggled,this,[this](bool linked){link_->setIcon(ControlSymbols::icon(linked?ControlSymbols::Symbol::Link:ControlSymbols::Symbol::Unlink));});}
                connect(spin,&QDoubleSpinBox::valueChanged,this,[this,i]{if(!loading_ && link_ && link_->isChecked() && (i==2 || i==3)) {const QSignalBlocker block(fields_[5-i]);fields_[5-i]->setValue(fields_[i]->value());}commit();});
            }
            layout->addStretch();form->addRow(labels[row],pair);
        }
        auto* reset=new QPushButton(tr("Restaurar transformação"),this);reset->setObjectName("resetLayerTransform");
        reset->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Reset));reset->setIconSize(QSize(14,14));reset->setAutoDefault(false);
        reset->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);form->addRow(reset);
        connect(reset,&QPushButton::clicked,this,[this]{setTransform({},canvas_);if(changed) changed({});});
        setTransform({},QSize(1280,720));
    }
    void setTransform(const framescale::LayerTransform& t,const QSize& canvas) {
        loading_=true;canvas_=canvas.isEmpty()?QSize(1280,720):canvas;
        fields_[0]->setRange(-canvas_.width()*10.,canvas_.width()*10.);fields_[1]->setRange(-canvas_.height()*10.,canvas_.height()*10.);
        const double values[]={t.x*canvas_.width()/100.,t.y*canvas_.height()/100.,t.scaleX,t.scaleY,t.rotation,t.opacity};
        for(int i=0;i<6;++i) fields_[i]->setValue(values[i]);loading_=false;setEnabled(true);
    }
private:
    void commit() {if(!loading_ && changed) changed({fields_[0]->value()*100./canvas_.width(),fields_[1]->value()*100./canvas_.height(),fields_[2]->value(),fields_[3]->value(),fields_[4]->value(),fields_[5]->value()});}
    QToolButton* link_=nullptr;QDoubleSpinBox* fields_[6]{};QSize canvas_{1280,720};bool loading_=false;
};
