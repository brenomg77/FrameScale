#pragma once
#include "core/ProcessingOptions.h"
#include "Appearance.h"
#include "LayerTransformControls.h"
#include <QListWidget>
#include <QProxyStyle>
#include <QStyleFactory>
#include "SelectionComboBox.h"
#include <QButtonGroup>
#include <QStyledItemDelegate>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <functional>

// Use the same compact, readable menu rows for every font, regardless of its face.
class LayerFontDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void initStyleOption(QStyleOptionViewItem* option,const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option,index);
        option->font=QApplication::font();option->icon=QIcon();
    }
    QSize sizeHint(const QStyleOptionViewItem& option,const QModelIndex& index) const override {
        QSize size=QStyledItemDelegate::sizeHint(option,index);size.setHeight(28);return size;
    }
};
class LayerCheckBox final : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
    QSize sizeHint() const override {return QSize(fontMetrics().horizontalAdvance(text())+28,std::max(22,fontMetrics().height()+4));}
    QSize minimumSizeHint() const override {return sizeHint();}
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        const bool light=Appearance::light();QRectF box(1,(height()-15)/2.,15,15);
        p.setPen(QPen(isChecked()?QColor("#007aff"):QColor(light?"#aaaaaf":"#727278"),1));
        p.setBrush(isChecked()?QColor(underMouse()?"#0064d2":"#007aff"):QColor(128,128,140,underMouse()?65:25));p.drawRoundedRect(box,4,4);
        if(isChecked()) {p.setPen(QPen(Qt::white,1.8,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));QPainterPath path;path.moveTo(box.left()+3,box.center().y());path.lineTo(box.left()+6,box.bottom()-4);path.lineTo(box.right()-3,box.top()+4);p.drawPath(path);}
        p.setPen(light?QColor("#292929"):QColor("#eeeeee"));p.drawText(rect().adjusted(24,0,0,0),Qt::AlignVCenter|Qt::AlignLeft,text());
        if(hasFocus()) {p.setPen(QPen(QColor("#007aff"),1,Qt::DotLine));p.setBrush(Qt::NoBrush);p.drawRoundedRect(rect().adjusted(0,0,-1,-1),4,4);}
    }
};
class LayerPopupStyle final : public QProxyStyle {
public:
    LayerPopupStyle():QProxyStyle(QStyleFactory::create("Fusion")) {}
    int styleHint(StyleHint hint,const QStyleOption* option=nullptr,const QWidget* widget=nullptr,QStyleHintReturn* data=nullptr) const override {
        if(hint==SH_ComboBox_Popup) return 0;
        return QProxyStyle::styleHint(hint,option,widget,data);
    }
};
class LayerFontCombo final : public QFontComboBox {
public:
    explicit LayerFontCombo(QWidget* parent):QFontComboBox(parent) {
        auto* popupStyle=new LayerPopupStyle;popupStyle->setParent(this);setStyle(popupStyle);
        setEditable(false);setItemDelegate(new LayerFontDelegate(this));setMaxVisibleItems(8);
    }
    void showPopup() override {
        view()->setMaximumHeight(8*28+8);
        auto* popup=view()->window();
        popup->setWindowFlag(Qt::NoDropShadowWindowHint,true);
        popup->setAttribute(Qt::WA_TranslucentBackground);
        popup->setProperty("glassWindowSurface",true);
        popup->setProperty("roundedWindowInstalled",true);
        if(!popup->graphicsEffect()) popup->setGraphicsEffect(new RoundedWindowEffect(popup));
        QFontComboBox::showPopup();
        framescale::window_backdrop::configureGlassWindow(popup,!GlassMaterial::reduced(),!Appearance::light());
    }
};
class LayerPropertyPages final : public QStackedWidget {
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override {return currentWidget()?currentWidget()->sizeHint().boundedTo(currentWidget()->maximumSize()).expandedTo(currentWidget()->minimumSize()):QSize();}
    QSize minimumSizeHint() const override {return sizeHint();}
};
class TextLayerDialog final : public QDialog {
public:
    std::function<void(const framescale::TextLayerOptions&)> changed;
    std::function<void(const framescale::LayerTransform&)> transformChanged;
    explicit TextLayerDialog(QWidget* parent):QDialog(parent,Qt::Tool) {
        setObjectName("textLayerDialog");setWindowTitle(tr("Editar"));setModal(false);
        // A single rounded outline, without a second inset frame.
        setProperty("glassEdgeInset",0);
        setProperty("framelessToolSurface",true);
        setProperty("stableSurface",true);
        setProperty("cachedGlassSurface",true);
        setWindowFlag(Qt::NoDropShadowWindowHint,true);
        setProperty("dialogTitleHeight",42);
        setMinimumWidth(460);setMaximumWidth(480);
        setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
        auto* root=new QVBoxLayout(this);root->setContentsMargins(16,8,16,18);root->setSpacing(10);
        auto* body=new QHBoxLayout;body->setSpacing(10);root->addLayout(body);
        tabs_=new QListWidget(this);tabs_->setObjectName("settingsSections");
        tabs_->addItems({tr("Texto"),tr("Transformação")});tabs_->setFixedWidth(108);
        tabs_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tabs_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tabs_->hide();
        auto* navigation=new QWidget(this);navigation->setObjectName("settingsNavigation");navigation->setFixedWidth(140);
        navigation->setStyleSheet("QWidget#settingsNavigation {background:rgba(128,128,128,16);border-radius:9px;} QWidget#settingsNavigation QPushButton {background:transparent;border:0;text-align:left;padding:3px 4px;} QWidget#settingsNavigation QPushButton:checked {background:rgba(128,128,128,30);font-weight:600;} QWidget#settingsNavigation QPushButton:hover {background:rgba(128,128,128,24);}");
        auto* navLayout=new QVBoxLayout(navigation);navLayout->setContentsMargins(5,5,5,5);navLayout->setSpacing(4);
        for(int i=0;i<2;++i) {
            nav_[i]=new QPushButton(i==0?tr("Texto"):tr("Transformação"),navigation);
            nav_[i]->setCheckable(true);nav_[i]->setAutoExclusive(true);nav_[i]->setAutoDefault(false);nav_[i]->setFixedSize(130,30);
            nav_[i]->setIcon(ControlSymbols::icon(i==0?ControlSymbols::Symbol::Text:ControlSymbols::Symbol::Sliders));nav_[i]->setIconSize(QSize(14,14));
            navLayout->addWidget(nav_[i]);connect(nav_[i],&QPushButton::clicked,this,[this,i]{tabs_->setCurrentRow(i);});
        }
        body->addWidget(navigation,0,Qt::AlignTop);
        auto* right=new QVBoxLayout;right->setSpacing(12);body->addLayout(right,1);
        heading_=new QLabel(tr("Texto"),this);heading_->setStyleSheet("font-size:22px;font-weight:600;background:transparent;");right->addWidget(heading_);
        hint_=new QLabel(tr("Selecione uma camada de texto na timeline."),this);
        hint_->setWordWrap(true);hint_->hide();
        pages_=new LayerPropertyPages(this);right->addWidget(pages_,0,Qt::AlignTop);
        controls_=new QWidget(pages_);controls_->setAutoFillBackground(false);pages_->addWidget(controls_);
        transform_=new LayerTransformControls(pages_);pages_->addWidget(transform_);
        transform_->changed=[this](const framescale::LayerTransform& t){if(transformChanged) transformChanged(t);};
        connect(tabs_,&QListWidget::currentRowChanged,this,[this](int index){nav_[index]->setChecked(true);heading_->setText(index==0?tr("Texto"):tr("Transformação"));pages_->setCurrentIndex(index);pages_->updateGeometry();adjustSize();});
        auto* form=new QFormLayout(controls_);form->setContentsMargins(0,0,0,0);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setHorizontalSpacing(8);form->setVerticalSpacing(9);
        content_=new QPlainTextEdit(controls_);content_->setObjectName("textContent");
        content_->setStyleSheet("QPlainTextEdit#textContent {background:rgba(128,128,140,20);border:1px solid rgba(128,128,140,65);border-radius:9px;padding:6px;}");
        content_->viewport()->setAutoFillBackground(false);
        content_->setPlaceholderText(tr("Escreva o texto…"));content_->setFixedHeight(80);
        content_->hide();
        connect(content_,&QPlainTextEdit::textChanged,this,[this]{commit();});
        family_=new LayerFontCombo(controls_);family_->setObjectName("textFont");family_->setMinimumWidth(0);family_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);form->addRow(tr("Fonte"),family_);
        style_=new SelectionComboBox(controls_);style_->setObjectName("textStyle");style_->setMinimumWidth(0);style_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);form->addRow(tr("Estilo"),style_);
        connect(family_,&QFontComboBox::currentFontChanged,this,[this]{if(!loading_){styles();commit();}});
        connect(style_,&QComboBox::currentTextChanged,this,[this]{commit();});
        size_=number(form,tr("Tamanho"),1,2000,72," px","textSize");
        leading_=number(form,tr("Entrelinha"),0,4000,0," px","textLeading");leading_->setSpecialValueText(tr("Automática"));
        tracking_=number(form,tr("Espaçamento"),-100,500,0," px","textTracking");
        alignment_=new QButtonGroup(this);alignment_->setExclusive(true);
        auto* alignRow=new QWidget(controls_);auto* alignLayout=new QHBoxLayout(alignRow);
        alignLayout->setContentsMargins(0,0,0,0);alignLayout->setSpacing(4);
        const QStringList names{tr("Esquerda"),tr("Centro"),tr("Direita")};
        for(int i=0;i<3;++i) {
            auto* button=new QToolButton(alignRow);button->setObjectName(QString("textAlign%1").arg(i));
            button->setCheckable(true);button->setFixedSize(32,26);button->setToolTip(names[i]);button->setAccessibleName(names[i]);
            QPixmap pix(18,18);pix.fill(Qt::transparent);QPainter painter(&pix);
            painter.setPen(QPen(Appearance::light()?QColor("#303030"):QColor("#eeeeee"),1.5));
            for(int line=0;line<4;++line) {int width=line%2?9:15;int x=i==0?1:i==1?(18-width)/2:17-width;painter.drawLine(x,3+line*4,x+width,3+line*4);}
            painter.end();button->setIcon(QIcon(pix));alignment_->addButton(button,i);alignLayout->addWidget(button);
        }
        alignLayout->addStretch();form->addRow(tr("Alinhamento"),alignRow);
        connect(alignment_,&QButtonGroup::idClicked,this,[this]{commit();});
        underline_=new LayerCheckBox(tr("Sublinhado"),controls_);form->addRow(underline_);
        connect(underline_,&QCheckBox::toggled,this,[this]{commit();});
        fill_=new LayerCheckBox(tr("Preenchimento"),controls_);fillColor_=new QPushButton(controls_);
        form->addRow(fill_,fillColor_);colorButton(fillColor_,true);
        stroke_=new LayerCheckBox(tr("Contorno"),controls_);strokeColor_=new QPushButton(controls_);
        form->addRow(stroke_,strokeColor_);colorButton(strokeColor_,false);
        strokeWidth_=number(form,tr("Largura do contorno"),0,100,2," px","textStrokeWidth");
        connect(fill_,&QCheckBox::toggled,this,[this]{commit();});
        connect(stroke_,&QCheckBox::toggled,this,[this]{commit();});
        Appearance::apply(this);
        for(auto* combo:{static_cast<QComboBox*>(family_),style_}) {
            combo->setView(new QListView(combo));combo->view()->setProperty("appearanceManaged",true);combo->setItemDelegate(new LayerFontDelegate(combo));combo->setMaxVisibleItems(8);
            const bool light=Appearance::light();
            QPalette menuColors=combo->palette();menuColors.setColor(QPalette::Base,light?QColor("#f5f5f7"):QColor("#29292d"));menuColors.setColor(QPalette::Window,menuColors.color(QPalette::Base));menuColors.setColor(QPalette::Text,light?QColor("#292929"):QColor("#eeeeee"));menuColors.setColor(QPalette::Highlight,QColor("#007aff"));menuColors.setColor(QPalette::HighlightedText,Qt::white);
            combo->view()->setPalette(menuColors);combo->view()->viewport()->setPalette(menuColors);combo->view()->viewport()->setAutoFillBackground(false);
            combo->view()->setStyleSheet(QString("QAbstractItemView {background:transparent;color:%1;border:1px solid %2;border-radius:7px;padding:4px;outline:0;selection-background-color:#007aff;selection-color:white;} QAbstractItemView::item {min-height:26px;padding:2px 8px;border-radius:4px;} QAbstractItemView::item:selected {background:#007aff;color:white;}").arg(light?"#292929":"#eeeeee",light?"#c0c0c5":"#55555b"));
        }
        for(auto* button:alignment_->buttons()) button->setStyleSheet("QToolButton {padding:0;margin:0;border:1px solid rgba(128,128,128,55);border-radius:6px;background:rgba(128,128,128,20);} QToolButton:hover {background:rgba(128,128,128,65);} QToolButton:checked {background:rgba(0,122,255,65);border-color:#007aff;}");
        tabs_->setCurrentRow(0);
        setTextOptions({});
    }
    void setTextOptions(const framescale::TextLayerOptions& value) {
        loading_=true;value_=value;
        const QString text=QString::fromStdString(value.content);
        if(content_->toPlainText()!=text) content_->setPlainText(text);
        family_->setCurrentFont(QFont(QString::fromStdString(value.fontFamily)));styles();
        const int style=style_->findText(QString::fromStdString(value.fontStyle));
        if(style>=0) style_->setCurrentIndex(style);
        size_->setValue(value.fontSize);leading_->setValue(value.lineHeight);tracking_->setValue(value.letterSpacing);
        framescale::MediaLayer layer;layer.kind=framescale::LayerKind::Text;layer.text=value;
        transform_->setTransform(framescale::layerTransform(layer),canvas_);
        tabs_->item(0)->setHidden(false);nav_[0]->show();
        if(auto* button=alignment_->button(value.alignment)) button->setChecked(true);underline_->setChecked(value.underline);
        fill_->setChecked(value.fill);stroke_->setChecked(value.stroke);strokeWidth_->setValue(value.strokeWidth);
        updateColors();controls_->setEnabled(true);hint_->setText(tr("Alterações aplicadas à camada de texto selecionada."));loading_=false;
    }
    void setLayer(const framescale::MediaLayer& layer,const QSize& canvas) {
        canvas_=canvas;
        if(layer.kind==framescale::LayerKind::Text) {const bool switching=!textMode_;textMode_=true;setWindowTitle(tr("Editar"));setTextOptions(layer.text);if(switching) tabs_->setCurrentRow(0);}
        else {
            textMode_=false;controls_->setEnabled(false);tabs_->item(0)->setHidden(true);nav_[0]->hide();tabs_->setCurrentRow(1);setWindowTitle(tr("Editar"));
            hint_->setText(tr("Alterações aplicadas à camada selecionada."));
            transform_->setTransform(framescale::layerTransform(layer),canvas_);
        }
        pages_->updateGeometry();layout()->activate();adjustSize();
    }
    void clearSelection() {controls_->setEnabled(false);transform_->setEnabled(false);hint_->setText(tr("Selecione uma camada na timeline."));}
protected:
    bool event(QEvent* event) override {
        const bool result=QDialog::event(event);
        if((event->type()==QEvent::Show || event->type()==QEvent::Move || event->type()==QEvent::Resize) && isVisible() && !backdropPending_) {
            backdropPending_=true;QTimer::singleShot(0,this,[this]{
                backdropPending_=false;if(!parentWidget() || !isVisible()) return;
                auto* owner=parentWidget()->window();
                const QPoint origin=owner->mapFromGlobal(mapToGlobal(QPoint()));
                const QPixmap background=owner->grab(QRect(origin,size()));
                GlassMaterial::updateBackdrop(this,background.toImage(),160);update();
            });
        }
        return result;
    }
private:
    bool backdropPending_=false;
    QDoubleSpinBox* number(QFormLayout* form,const QString& label,double min,double max,double value,const QString& suffix,const char* name) {
        auto* spin=new QDoubleSpinBox(controls_);spin->setObjectName(name);spin->setRange(min,max);spin->setDecimals(1);
        spin->setValue(value);spin->setSuffix(suffix);spin->setKeyboardTracking(false);form->addRow(label,spin);
        connect(spin,&QDoubleSpinBox::valueChanged,this,[this]{commit();});return spin;
    }
    void styles() {
        QSignalBlocker block(style_);const QString previous=style_->currentText();
        style_->clear();style_->addItems(QFontDatabase::styles(family_->currentFont().family()));
        if(style_->count()==0) style_->addItem("Regular");
        const int index=style_->findText(previous);if(index>=0) style_->setCurrentIndex(index);
    }
    void colorButton(QPushButton* button,bool fill) {
        connect(button,&QPushButton::clicked,this,[this,fill] {
            const QColor current(QString::fromStdString(fill?value_.fillColor:value_.strokeColor));
            QColorDialog picker(current,this);picker.setWindowTitle(tr("Cor do texto"));
            picker.setOptions(QColorDialog::ShowAlphaChannel|QColorDialog::DontUseNativeDialog);
            QPalette colors=palette();const bool light=Appearance::light();
            colors.setColor(QPalette::Window,light?QColor("#f1f1f4"):QColor("#29292d"));
            colors.setColor(QPalette::Base,light?QColor("#ffffff"):QColor("#343438"));
            colors.setColor(QPalette::WindowText,light?QColor("#292929"):QColor("#eeeeee"));
            colors.setColor(QPalette::Text,colors.color(QPalette::WindowText));
            picker.setPalette(colors);Appearance::apply(&picker);
            if(picker.exec()!=QDialog::Accepted) return;
            const QColor chosen=picker.selectedColor();
            if(!chosen.isValid()) return;
            (fill?value_.fillColor:value_.strokeColor)=chosen.name(QColor::HexArgb).toStdString();updateColors();commit();
        });
    }
    void updateColors() {
        for(auto pair:{std::make_pair(fillColor_,value_.fillColor),std::make_pair(strokeColor_,value_.strokeColor)}) {
            const QColor color(QString::fromStdString(pair.second));
            QPixmap swatch(24,14);swatch.fill(color);pair.first->setIcon(QIcon(swatch));
            pair.first->setText(color.name());
        }
    }
    void commit() {
        if(loading_ || !controls_->isEnabled()) return;
        value_.content=content_->toPlainText().left(10000).toStdString();
        value_.fontFamily=family_->currentFont().family().toStdString();value_.fontStyle=style_->currentText().toStdString();
        value_.fontSize=size_->value();value_.lineHeight=leading_->value();value_.letterSpacing=tracking_->value();
        value_.alignment=alignment_->checkedId();value_.underline=underline_->isChecked();
        value_.fill=fill_->isChecked();value_.stroke=stroke_->isChecked();value_.strokeWidth=strokeWidth_->value();
        if(changed) changed(value_);
    }
    QPushButton* nav_[2];QLabel* heading_;QListWidget* tabs_;QStackedWidget* pages_;LayerTransformControls* transform_;QSize canvas_{1280,720};
    bool loading_=true,textMode_=true;
    framescale::TextLayerOptions value_;
    QWidget* controls_;QLabel* hint_;QPlainTextEdit* content_;QFontComboBox* family_;QComboBox *style_;QButtonGroup* alignment_;
    QDoubleSpinBox *size_,*leading_,*tracking_,*strokeWidth_;
    QCheckBox *fill_,*stroke_,*underline_;QPushButton *fillColor_,*strokeColor_;
};
