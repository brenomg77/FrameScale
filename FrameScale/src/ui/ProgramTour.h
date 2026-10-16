#pragma once
#include "Appearance.h"
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <vector>
#include <cmath>

inline bool shouldOfferProgramTour() {return !QSettings().value("onboarding/tourSeen",false).toBool();}
inline void markProgramTourSeen() {QSettings().setValue("onboarding/tourSeen",true);QSettings().sync();}

class TourGlassCard final : public QFrame {
public:
    explicit TourGlassCard(QWidget* parent):QFrame(parent) {setAutoFillBackground(false);}
    void setBackdrop(const QImage& image) {backdrop_=GlassMaterial::blur(image,180,3);update();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        GlassMaterial::paint(painter,rect(),backdrop_,Appearance::light(),Appearance::light()?105:130,14,true);
    }
private:
    QImage backdrop_;
};

class TourTimelineExample final : public QWidget {
public:
    explicit TourTimelineExample(QWidget* parent):QWidget(parent) {setFixedHeight(78);setAccessibleName(tr("Exemplo da timeline"));}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Appearance::light()?QColor("#666670"):QColor("#bcbcc4"));
        p.drawText(QRect(0,0,width(),18),tr("Exemplo da timeline"));
        p.setPen(Qt::NoPen);p.setBrush(QColor(128,128,140,35));p.drawRoundedRect(QRectF(0,23,width(),53),6,6);
        for(int i=0;i<8;++i) {
            p.setBrush(QColor(70+i*7,100+i*6,140+i*8));
            p.drawRoundedRect(QRectF(8+i*(width()-16)/8.,30,(width()-24)/8.,19),3,3);
        }
        p.setPen(QPen(QColor("#007aff"),1));
        for(int x=8;x<width()-8;++x) {
            const double h=2+4*std::abs(std::sin(x*.24)*std::sin(x*.07));
            p.drawLine(QPointF(x,62-h),QPointF(x,62+h));
        }
        p.setPen(QPen(QColor("#007aff"),2));p.drawLine(QPointF(width()*.38,24),QPointF(width()*.38,75));
    }
};

// Read-only coach marks: no sample files, processing jobs or changed options.
class ProgramTour final : public QDialog {
    friend struct ProgramTourTestAccess;
public:
    struct Step {
        QString title,body,shortcut;
        std::function<QRect()> target;
        std::function<void()> prepare;
        bool timelineExample=false;
    };
    ProgramTour(QWidget* owner,std::vector<Step> steps):QDialog(owner),owner_(owner),steps_(std::move(steps)) {
        setObjectName("programTour");setProperty("appearanceManaged",true);
        setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
        setWindowModality(Qt::WindowModal);setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);setWindowTitle(tr("Tour do FrameScale"));
        owner_->installEventFilter(this);
        card_=new TourGlassCard(this);card_->setObjectName("tourCard");
        auto* layout=new QVBoxLayout(card_);layout->setContentsMargins(22,20,22,20);layout->setSpacing(14);
        auto* header=new QHBoxLayout;
        counter_=new QLabel(card_);counter_->setObjectName("tourCounter");header->addWidget(counter_);header->addStretch();
        auto* close=new QPushButton(QString::fromUtf8("×"),card_);close->setObjectName("tourClose");close->setFixedSize(26,26);
        close->setAccessibleName(tr("Sair do tour"));header->addWidget(close);layout->addLayout(header);
        identity_=new QWidget(card_);auto* identityLayout=new QHBoxLayout(identity_);
        identityLayout->setContentsMargins(0,4,0,4);identityLayout->setSpacing(18);
        auto* logo=new QLabel(identity_);logo->setPixmap(QPixmap(":/brand/framescale-logo.png").scaled(64,64,Qt::KeepAspectRatio,Qt::SmoothTransformation));identityLayout->addWidget(logo);
        QImage wordmark(":/brand/framescale-wordmark.png");QRect ink;
        for(int y=0;y<wordmark.height();++y) for(int x=0;x<wordmark.width();++x)
            if(qAlpha(wordmark.pixel(x,y))>12) ink=ink.united(QRect(x,y,1,1));
        if(!ink.isEmpty()) wordmark=wordmark.copy(ink);
        auto* brand=new QLabel(identity_);brand->setPixmap(QPixmap::fromImage(wordmark).scaled(220,52,Qt::KeepAspectRatio,Qt::SmoothTransformation));identityLayout->addWidget(brand);identityLayout->addStretch();layout->addWidget(identity_);
        title_=new QLabel(card_);title_->setObjectName("tourTitle");title_->setWordWrap(true);layout->addWidget(title_);
        body_=new QLabel(card_);body_->setObjectName("tourBody");body_->setWordWrap(true);body_->setTextFormat(Qt::PlainText);layout->addWidget(body_);
        example_=new TourTimelineExample(card_);layout->addWidget(example_);example_->hide();
        shortcut_=new QLabel(card_);shortcut_->setObjectName("tourShortcut");shortcut_->setWordWrap(true);layout->addWidget(shortcut_);
        progress_=new QProgressBar(card_);progress_->setTextVisible(false);progress_->setFixedHeight(4);progress_->setRange(0,int(steps_.size()));layout->addWidget(progress_);
        auto* buttons=new QHBoxLayout;
        back_=new QPushButton(card_);buttons->addWidget(back_);buttons->addStretch();
        next_=new QPushButton(card_);next_->setObjectName("tourNext");next_->setDefault(true);buttons->addWidget(next_);layout->addLayout(buttons);
        const bool light=Appearance::light();
        setStyleSheet(QString(
            "QDialog#programTour {background:transparent;selection-background-color:%1;} QFrame#tourCard {background:transparent;border:0;}"
            "QLabel {background:transparent;border:0;color:%3;font-size:13px;} QLabel#tourTitle {font-size:20px;font-weight:600;}"
            "QLabel#tourCounter {color:%4;font-size:12px;} QLabel#tourShortcut {color:%5;font-size:12px;}"
            "QPushButton {min-height:28px;min-width:65px;padding:2px 10px;border-radius:7px;border:1px solid %2;background:transparent;color:%3;}"
            "QPushButton:hover {background:%6;} QPushButton#tourNext {background:#007aff;border-color:#007aff;color:white;}"
            "QPushButton#tourNext:hover {background:#1686ff;} QPushButton#tourClose {min-width:0;min-height:0;padding:0;font-size:20px;border:0;}"
            "QProgressBar {border:0;background:%2;border-radius:2px;} QProgressBar::chunk {background:#007aff;border-radius:2px;}")
            .arg(light?"#f4f4f6":"#242428",light?"#d0d1d7":"#44444c",light?"#242428":"#f4f4f6",
                 light?"#6d6e76":"#a8a9b2",light?"#0066d6":"#79b7ff",light?"#e4e6eb":"#34343c"));
        connect(close,&QPushButton::clicked,this,[this]{requestExit();});
        connect(back_,&QPushButton::clicked,this,[this]{
            if(confirming_) {confirming_=false;render();}
            else if(index_==0) finishTour();
            else {--index_;prepare();}
        });
        connect(next_,&QPushButton::clicked,this,[this]{
            if(confirming_ || index_==int(steps_.size())+1) finishTour();
            else {++index_;prepare();}
        });
        syncGeometry();render();
        auto* tracking=new QTimer(this);tracking->setInterval(100);
        connect(tracking,&QTimer::timeout,this,[this]{
            const auto target=targetRect();
            if(target!=lastTarget_) {lastTarget_=target;placeCard();update();}
        });tracking->start();
    }
protected:
    bool eventFilter(QObject* object,QEvent* event) override {
        if(object==owner_ && (event->type()==QEvent::Resize || event->type()==QEvent::Move))
            QTimer::singleShot(0,this,[this]{syncGeometry();placeCard();update();});
        return QDialog::eventFilter(object,event);
    }
    void resizeEvent(QResizeEvent* event) override {QDialog::resizeEvent(event);placeCard();}
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape) {requestExit();return;}
        QDialog::keyPressEvent(event);
    }
    void reject() override {requestExit();}
    void closeEvent(QCloseEvent* event) override {
        if(closing_) event->accept();else {event->ignore();requestExit();}
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath shade;shade.setFillRule(Qt::OddEvenFill);shade.addRect(rect());
        const QRect target=targetRect();
        if(!target.isEmpty()) shade.addRoundedRect(QRectF(target),12,12);
        painter.fillPath(shade,QColor(0,0,0,145));
        if(!target.isEmpty()) {
            painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor("#3896ff"),2));
            painter.drawRoundedRect(QRectF(target).adjusted(1,1,-1,-1),11,11);
        }
    }
private:
    void syncGeometry() {if(owner_) setGeometry(QRect(owner_->mapToGlobal(QPoint()),owner_->size()));}
    QRect targetRect() const {
        if(confirming_ || index_<1 || index_>int(steps_.size())) return {};
        const auto& target=steps_[index_-1].target;
        const QRect area=target?target():QRect();
        return area.isEmpty()?QRect():area.adjusted(-3,-3,3,3).intersected(rect().adjusted(1,1,-1,-1));
    }
    void prepare() {
        if(index_>=1 && index_<=int(steps_.size()) && steps_[index_-1].prepare) steps_[index_-1].prepare();
        backdropArea_={};render();QTimer::singleShot(0,this,[this]{backdropArea_={};placeCard();update();});
    }
    void render() {
        back_->setVisible(true);shortcut_->hide();progress_->setVisible(!confirming_);
        identity_->setVisible(!confirming_ && index_==0);
        title_->setVisible(confirming_ || index_!=0);
        example_->setVisible(!confirming_ && index_>=1 && index_<=int(steps_.size()) && steps_[index_-1].timelineExample && targetRect().isEmpty());
        if(confirming_) {
            counter_->setText(tr("Tour do FrameScale"));title_->setText(tr("Sair do tour?"));
            body_->setText(tr("Pode voltar a fazer o tour em Definições → Geral → Tour."));
            back_->setText(tr("Continuar tour"));next_->setText(tr("Sair do tour"));
        } else if(index_==0) {
            counter_->setText(tr("Conheça o FrameScale"));setWindowTitle(tr("Conheça o FrameScale"));
            body_->setText(tr("Um passeio rápido: abra um arquivo, escolha o processamento e exporte o resultado."));
            back_->setText(tr("Explorar sozinho"));next_->setText(tr("Começar"));progress_->setValue(0);
        } else if(index_>int(steps_.size())) {
            counter_->setText(tr("Tour concluído"));title_->setText(tr("Pronto para começar"));
            body_->setText(tr("Comece com um trecho curto para comparar o resultado.\n\nPara rever estas dicas, abra Definições → Geral → Tour."));
            back_->setText(tr("Voltar"));next_->setText(tr("Concluir"));progress_->setValue(int(steps_.size()));
        } else {
            const auto& step=steps_[index_-1];counter_->setText(tr("%1 de %2").arg(index_).arg(steps_.size()));
            title_->setText(step.title);body_->setText(step.body);shortcut_->setText(step.shortcut);shortcut_->setVisible(!step.shortcut.isEmpty());
            back_->setText(tr("Voltar"));next_->setText(tr("Próximo"));progress_->setValue(index_);
        }
        placeCard();update();
        const auto controls=findChildren<QPushButton*>();for(auto* button:controls) button->setAutoDefault(false);
        next_->setFocus(Qt::OtherFocusReason);
    }
    void placeCard() {
        if(!card_) return;
        card_->setFixedWidth(qMin(370,qMax(260,width()-32)));
        card_->layout()->invalidate();card_->layout()->activate();
        // Recompute wrapped text after every step instead of retaining the
        // previous card's height and clipping the final sentence/punctuation.
        const int wrappedHeight=card_->layout()->totalHeightForWidth(card_->width());
        card_->resize(card_->width(),qMax(card_->layout()->totalMinimumSize().height(),wrappedHeight)+4);
        card_->layout()->setGeometry(card_->rect());
        const QRect target=targetRect();QPoint position((width()-card_->width())/2,(height()-card_->height())/2);
        if(!target.isEmpty()) {
            if(target.left()>card_->width()+36) position={target.left()-card_->width()-18,target.center().y()-card_->height()/2};
            else if(width()-target.right()>card_->width()+36) position={target.right()+18,target.center().y()-card_->height()/2};
            else if(target.top()>card_->height()+36) position={target.center().x()-card_->width()/2,target.top()-card_->height()-18};
            else position={target.center().x()-card_->width()/2,target.bottom()+18};
        }
        position.setX(qBound(16,position.x(),qMax(16,width()-card_->width()-16)));
        position.setY(qBound(16,position.y(),qMax(16,height()-card_->height()-16)));card_->move(position);card_->raise();
        const QRect sample(card_->pos(),card_->size());
        if(owner_ && owner_->isVisible() && sample!=backdropArea_) {
            backdropArea_=sample;card_->setBackdrop(owner_->grab(sample).toImage());
        }
    }
    void requestExit() {if(index_==0 || index_>int(steps_.size())) finishTour();else {confirming_=true;render();}}
    void finishTour() {closing_=true;accept();}
    QPointer<QWidget> owner_;
    std::vector<Step> steps_;int index_=0;bool confirming_=false,closing_=false;
    TourGlassCard* card_=nullptr;QLabel *title_=nullptr,*body_=nullptr,*counter_=nullptr,*shortcut_=nullptr;
    QWidget* identity_=nullptr;QRect lastTarget_,backdropArea_;
    TourTimelineExample* example_=nullptr;
    QProgressBar* progress_=nullptr;QPushButton *back_=nullptr,*next_=nullptr;
};
