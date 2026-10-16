#include "SetupTask.h"
#include "../../../src/ui/Appearance.h"
#include "../../../src/ui/Typography.h"
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCommandLineParser>
#include <QEnterEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <memory>
#include <QUrl>
#include <QWindow>

namespace {
class CaptionButton final : public QAbstractButton {
public:
    CaptionButton(bool close,QWidget* parent):QAbstractButton(parent),close_(close) {
        setFixedSize(32,26);setCursor(Qt::PointingHandCursor);
        setAccessibleName(close?QStringLiteral("Fechar instalador"):QStringLiteral("Minimizar"));
    }
protected:
    void enterEvent(QEnterEvent* event) override {update();QAbstractButton::enterEvent(event);}
    void leaveEvent(QEvent* event) override {update();QAbstractButton::leaveEvent(event);}
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        const QRectF face=QRectF(rect()).adjusted(1,1,-1,-1);
        if(underMouse() || isDown()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(close_?QColor(isDown()?"#e5332a":"#ff453a"):QColor(0,0,0,isDown()?36:22));
            painter.drawRoundedRect(face,6,6);
        }
        painter.setPen(QPen(close_ && (underMouse()||isDown())?Qt::white:QColor("#343434"),1.3,Qt::SolidLine,Qt::RoundCap));
        const QPointF center=face.center();
        if(close_) {
            painter.drawLine(center+QPointF(-3,-3),center+QPointF(3,3));
            painter.drawLine(center+QPointF(-3,3),center+QPointF(3,-3));
        } else painter.drawLine(center+QPointF(-3.5,0),center+QPointF(3.5,0));
    }
private:
    bool close_;
};
// Only installer layout rules live here. The actual control faces, palette,
// window material, blur and rounded alpha outline come from the application.
const char* setupStyle=R"(
QWidget#shell, QWidget#header, QWidget#footer, QStackedWidget, QWidget#page {background:transparent;border:0;}
QLabel {background:transparent;border:0;color:#292929;}
QLabel[role="muted"] {color:#72747b;font-size:12px;}
QLabel[role="title"] {font-size:15px;font-weight:600;}
QLabel[role="message"] {font-size:13px;color:#55555b;}
QLabel#brand {font-size:13px;font-weight:500;}
QPushButton#close,QPushButton#minimize {border:0;background:transparent;padding:0;border-radius:5px;font-size:16px;}
QPushButton#close:hover {background:#ff453a;color:white;}
QPushButton#minimize:hover {background:rgba(0,0,0,24);}
QPushButton:hover {background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(255,255,255,110),stop:0.45 rgba(255,255,255,100),stop:1 rgba(248,249,252,92));border-color:rgba(60,60,60,45);}
QPushButton[action="true"] {text-align:left;padding:3px 10px;border-radius:8px;font-size:14px;font-weight:500;}
QPushButton[action="true"]:checked {background:rgba(255,255,255,150);border:1px solid rgba(60,60,60,55);}
QPushButton[action="true"]:hover {background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 rgba(255,255,255,96),stop:0.45 rgba(255,255,255,86),stop:1 rgba(248,249,252,80));border-color:rgba(60,60,60,38);}
QPushButton[action="true"]:checked:hover {background:rgba(255,255,255,150);border-color:rgba(60,60,60,55);}
QPushButton[action="true"]:focus {border-color:rgba(60,60,60,70);}
QPushButton[danger="true"] {background:#ff453a;color:white;border-color:#ff453a;}
QPushButton[danger="true"]:hover {background:#e5332a;border-color:#e5332a;}
QPushButton:disabled {color:#898989;}
QLineEdit {font-size:13px;}
QCheckBox {background:transparent;spacing:8px;font-size:13px;}
QCheckBox::indicator {width:13px;height:13px;border:1px solid rgba(60,60,60,65);border-radius:3px;background:rgba(255,255,255,88);}
QCheckBox::indicator:checked {background:#007aff;border-color:#1686ff;image:url(:/setup/check.svg);}
QProgressBar {height:7px;max-height:7px;border:0;border-radius:3px;background:rgba(60,60,60,30);}
QProgressBar::chunk {background:#007aff;border-radius:3px;}
QLabel#error {color:#b3262d;font-size:12px;}
QLabel#info {font-size:12px;color:#55555b;}
)";
class ActionButton final : public QPushButton {
public:
    ActionButton(const QString& text,QWidget* parent):QPushButton(text,parent) {
        hover_.setDuration(140);hover_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&hover_,&QVariantAnimation::valueChanged,this,[this](const QVariant& value){shade_=value.toReal();update();});
    }
protected:
    void enterEvent(QEnterEvent* event) override {fade(1);QPushButton::enterEvent(event);}
    void leaveEvent(QEvent* event) override {fade(0);QPushButton::leaveEvent(event);}
    void paintEvent(QPaintEvent* event) override {
        QPushButton::paintEvent(event);
        if(!isEnabled() || shade_<=0) return;
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        // A gentle wash over the existing material, without a drop shadow or
        // an abrupt change in the selected row's background.
        painter.setPen(Qt::NoPen);painter.setBrush(QColor(0,0,0,qRound(8*shade_)));
        painter.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),7,7);
    }
private:
    void fade(qreal target) {
        hover_.stop();
        if(Appearance::reduceMotion()) {shade_=target;update();return;}
        hover_.setStartValue(shade_);hover_.setEndValue(target);hover_.start();
    }
    QVariantAnimation hover_;qreal shade_=0;
};
class SetupPanel final : public QFrame {
public:
    explicit SetupPanel(QWidget* parent):QFrame(parent) {
        setAutoFillBackground(false);setAttribute(Qt::WA_StyledBackground,false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        // Same inner panel material used by the app's light dialogs.
        GlassMaterial::paint(painter,rect(),{},true,88,12,true);
    }
};

class SetupWindow final : public QWidget {
public:
    SetupWindow(QString backend,QString version,bool uninstall):backend_(std::move(backend)),version_(std::move(version)) {
        setWindowFlags(Qt::Window|Qt::FramelessWindowHint|Qt::WindowMinimizeButtonHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowTitle("Instalador FrameScale");setWindowIcon(QIcon(":/setup/symbol.png"));
        setFixedSize(480,310);
        auto* outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
        shell_=new QWidget(this);shell_->setObjectName("shell");outer->addWidget(shell_);
        auto* root=new QVBoxLayout(shell_);root->setContentsMargins(16,8,16,12);root->setSpacing(8);
        auto* header=new QWidget(shell_);header->setObjectName("header");header->setFixedHeight(30);header->installEventFilter(this);
        auto* title=new QHBoxLayout(header);title->setContentsMargins(0,0,0,0);title->setSpacing(8);
        title->addStretch();
        auto* minimize=new CaptionButton(false,header);title->addWidget(minimize);
        auto* closeButton=new CaptionButton(true,header);title->addWidget(closeButton);
        connect(minimize,&QAbstractButton::clicked,this,&QWidget::showMinimized);connect(closeButton,&QAbstractButton::clicked,this,&QWidget::close);
        root->addWidget(header);
        pages_=new QStackedWidget(shell_);root->addWidget(pages_,1);
        buildActions();buildDestination();buildProgress();buildFinish();
        error_=label({},shell_);error_->setObjectName("error");error_->setWordWrap(true);error_->hide();root->addWidget(error_);
        auto* footer=new QWidget(shell_);footer->setObjectName("footer");auto* actions=new QHBoxLayout(footer);actions->setContentsMargins(0,0,0,0);actions->setSpacing(8);
        actions->addStretch();
        back_=new QPushButton("Voltar",footer);back_->setFixedHeight(26);actions->addWidget(back_);
        next_=new QPushButton("Continuar",footer);next_->setProperty("variant","primary");next_->setMinimumWidth(92);next_->setFixedHeight(26);actions->addWidget(next_);root->addWidget(footer);
        connect(back_,&QPushButton::clicked,this,[this]{showPage(0);});
        connect(next_,&QPushButton::clicked,this,[this]{advance();});
        task_=new setup::Task(this);
        task_->progress=[this](int value){progress_->setRange(0,100);progress_->setValue(value);percent_->setText(QString::number(value)+"%");};
        task_->finished=[this](bool success,const QString& message) {
            busy_=false;
            if(success) {showPage(3);return;}
            showPage(1);info_->hide();error_->setText(message);error_->show();
        };
        setProperty("glassEdgeInset",2);
        installRoundedWindow(this);
        Appearance::apply(this);
        setStyleSheet(styleSheet()+QString::fromUtf8(setupStyle));
        if(uninstall) {action_=2;showPage(1);} else showPage(0);
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(watched->objectName()=="header" && event->type()==QEvent::MouseButtonPress) {
            auto* mouse=static_cast<QMouseEvent*>(event);
            if(mouse->button()==Qt::LeftButton && windowHandle()) {windowHandle()->startSystemMove();return true;}
        }
        return QWidget::eventFilter(watched,event);
    }
    void closeEvent(QCloseEvent* event) override {
        if(busy_) {error_->setText(QString::fromUtf8("Aguarde a operação terminar antes de fechar."));error_->show();event->ignore();}
        else event->accept();
    }
    void paintEvent(QPaintEvent*) override {
        // The shared rounded effect paints the translucent window material.
        // Supply its opaque light fallback when accessibility disables glass.
        if(!property("glassWindowSurface").toBool()) {
            QPainter painter(this);painter.fillRect(rect(),QColor("#fafafa"));
        }
    }
private:
    static QLabel* label(const QString& text,QWidget* parent,const char* role=nullptr) {
        auto* result=new QLabel(text,parent);if(role) result->setProperty("role",role);return result;
    }
    QVBoxLayout* page() {
        auto* widget=new QWidget(pages_);widget->setObjectName("page");pages_->addWidget(widget);
        auto* layout=new QVBoxLayout(widget);layout->setContentsMargins(1,4,1,0);layout->setSpacing(9);return layout;
    }
    void buildActions() {
        auto* box=page();auto* owner=box->parentWidget();
        box->setSpacing(6);
        auto* identity=new QHBoxLayout;identity->setContentsMargins(12,0,12,0);identity->setSpacing(24);
        auto* symbol=label({},owner);symbol->setFixedSize(76,76);
        symbol->setAlignment(Qt::AlignCenter);
        symbol->setPixmap(QPixmap(":/setup/logo.png").scaled(76,76,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        identity->addWidget(symbol);
        QImage wordmark(":/setup/wordmark.png");
        QRect ink;
        for(int y=0;y<wordmark.height();++y) for(int x=0;x<wordmark.width();++x)
            if(qAlpha(wordmark.pixel(x,y))>12) ink=ink.united(QRect(x,y,1,1));
        if(!ink.isEmpty()) wordmark=wordmark.copy(ink);
        auto* brand=label({},owner);brand->setAccessibleName("FrameScale");
        brand->setPixmap(QPixmap::fromImage(wordmark).scaled(230,58,Qt::KeepAspectRatio,Qt::SmoothTransformation));
        identity->addWidget(brand);identity->addStretch();
        box->addLayout(identity);box->addSpacing(26);
        auto* panel=new SetupPanel(owner);
        auto* rows=new QVBoxLayout(panel);rows->setContentsMargins(12,8,12,8);rows->setSpacing(8);
        auto* group=new QButtonGroup(owner);group->setExclusive(true);
        const QStringList names={QString::fromUtf8("Instalar FrameScale"),QString::fromUtf8("Reparar instalação"),QString::fromUtf8("Desinstalar FrameScale")};
        for(int i=0;i<3;++i) {
            auto* button=new ActionButton(names[i],panel);button->setProperty("action",true);button->setCheckable(true);button->setFixedHeight(28);button->setCursor(Qt::PointingHandCursor);
            const ControlSymbols::Symbol symbols[]={ControlSymbols::Symbol::Import,ControlSymbols::Symbol::Reset,ControlSymbols::Symbol::Trash};
            button->setIcon(ControlSymbols::icon(symbols[i]));button->setIconSize(QSize(19,15));
            group->addButton(button,i);rows->addWidget(button);
            if(i==0) button->setChecked(true);
            // Always allow choosing an action. Installation discovery may be
            // empty for portable or moved copies; validate the chosen folder
            // on the next page instead of silently disabling the entire row.
            connect(button,&QPushButton::clicked,this,[this,i]{action_=i;showPage(1);});
        }
        box->addWidget(panel);box->addStretch();
    }
    void buildDestination() {
        auto* box=page();auto* owner=box->parentWidget();
        destinationTitle_=label({},owner,"title");box->addWidget(destinationTitle_);
        destinationText_=label({},owner,"message");destinationText_->setWordWrap(true);box->addWidget(destinationText_);
        box->addSpacing(3);box->addWidget(label(QString::fromUtf8("Pasta do programa"),owner,"muted"));
        auto* pathRow=new QHBoxLayout;pathRow->setSpacing(8);
        path_=new QLineEdit(QDir::toNativeSeparators(setup::defaultPath()),owner);path_->setFixedHeight(26);path_->setAccessibleName(QString::fromUtf8("Pasta de instalação"));pathRow->addWidget(path_,1);
        browse_=new QPushButton("Escolher",owner);browse_->setFixedHeight(26);pathRow->addWidget(browse_);box->addLayout(pathRow);
        connect(browse_,&QPushButton::clicked,this,[this] {
            const QString directory=QFileDialog::getExistingDirectory(this,QString::fromUtf8("Pasta de instalação"),path_->text());
            if(!directory.isEmpty()) path_->setText(QDir::toNativeSeparators(directory));
        });
        desktop_=new QCheckBox(QString::fromUtf8("Criar atalho na área de trabalho"),owner);box->addWidget(desktop_);
        info_=label({},owner);info_->setObjectName("info");info_->setWordWrap(true);box->addWidget(info_);box->addStretch();
    }
    void buildProgress() {
        auto* box=page();auto* owner=box->parentWidget();box->addStretch();
        progressTitle_=label({},owner,"title");box->addWidget(progressTitle_);
        stage_=label({},owner,"message");stage_->setWordWrap(true);box->addWidget(stage_);box->addSpacing(18);
        progress_=new QProgressBar(owner);progress_->setTextVisible(false);box->addWidget(progress_);
        auto* row=new QHBoxLayout;row->addStretch();percent_=label("0%",owner,"muted");row->addWidget(percent_);box->addLayout(row);box->addStretch();
    }
    void buildFinish() {
        auto* box=page();auto* owner=box->parentWidget();box->addStretch();
        auto* logo=label({},owner);logo->setPixmap(QPixmap(":/setup/symbol.png").scaled(56,56,Qt::KeepAspectRatio,Qt::SmoothTransformation));box->addWidget(logo);
        finishTitle_=label({},owner,"title");box->addWidget(finishTitle_);
        finishText_=label({},owner,"message");finishText_->setWordWrap(true);box->addWidget(finishText_);
        launch_=new QCheckBox("Abrir o FrameScale",owner);box->addWidget(launch_);box->addStretch();
    }
    void showPage(int index) {
        error_->hide();info_->show();pages_->setCurrentIndex(index);back_->setVisible(index==1 && !backend_.isEmpty());
        next_->setEnabled(index!=2);next_->setVisible(index!=2 && index!=0);
        next_->setProperty("danger",index==1 && action_==2);next_->setProperty("variant",index==1 && action_==2?"danger":"primary");
        next_->style()->unpolish(next_);next_->style()->polish(next_);
        if(index==0) next_->setText("Continuar");
        if(index==1) {
            const bool remove=action_==2,repair=action_==1;
            destinationTitle_->setText(remove?"Desinstalar FrameScale":repair?QString::fromUtf8("Reparar instalação"):"Instalar FrameScale");
            destinationText_->setText(remove?QString::fromUtf8("Remova o FrameScale desta pasta."):repair?QString::fromUtf8("Reinstale os arquivos do FrameScale."):QString::fromUtf8("Escolha onde instalar o FrameScale."));
            if(remove || repair) path_->setText(QDir::toNativeSeparators(setup::defaultPath()));
            path_->setEnabled(true);browse_->setEnabled(true);desktop_->setVisible(!remove);
            info_->setVisible(remove);
            info_->setText(remove?QString::fromUtf8("Suas preferências e arquivos pessoais serão preservados."):QString::fromUtf8(""));
            next_->setText(remove?"Desinstalar":repair?"Reparar":"Instalar");
        }
        if(index==3) {
            const bool remove=action_==2;next_->setText("Fechar");
            finishTitle_->setText(remove?"FrameScale removido":QString::fromUtf8("Instalação concluída"));
            finishText_->setText(remove?QString::fromUtf8("O programa foi removido. Seus arquivos pessoais foram preservados."):QString::fromUtf8("O FrameScale foi instalado."));
            launch_->setVisible(!remove);
        }
    }
    void advance() {
        if(pages_->currentIndex()==0) {showPage(1);return;}
        if(pages_->currentIndex()==3) {
            if(action_!=2 && launch_->isChecked()) QProcess::startDetached(QDir(path_->text()).filePath("bin/FrameScale.exe"),{});
            close();return;
        }
        const QString error=setup::validatePath(path_->text());
        if(!error.isEmpty()) {info_->hide();error_->setText(error);error_->show();return;}
        if(action_!=0 && !QFileInfo(QDir(path_->text()).filePath("bin/FrameScale.exe")).isFile()) {
            info_->hide();error_->setText(QString::fromUtf8("Escolha a pasta onde o FrameScale está instalado."));error_->show();return;
        }
        busy_=true;showPage(2);progress_->setRange(0,action_==2?0:100);progress_->setValue(0);percent_->setText(action_==2?QString():"0%");
        progressTitle_->setText(action_==2?"Removendo FrameScale":action_==1?"Reparando FrameScale":"Instalando FrameScale");
        stage_->setText(action_==2?QString::fromUtf8("Removendo os arquivos do programa e os atalhos…"):QString::fromUtf8("Copiando os arquivos…"));
        task_->start(backend_,path_->text(),action_==2,desktop_->isChecked());
    }
    QString backend_,version_;
    bool busy_=false;int action_=0;
    QWidget* shell_=nullptr;QStackedWidget* pages_=nullptr;
    QLabel *error_=nullptr,*destinationTitle_=nullptr,*destinationText_=nullptr,*info_=nullptr,*progressTitle_=nullptr,*stage_=nullptr,*percent_=nullptr,*finishTitle_=nullptr,*finishText_=nullptr;
    QPushButton *next_=nullptr,*back_=nullptr,*browse_=nullptr;
    QLineEdit* path_=nullptr;QCheckBox *desktop_=nullptr,*launch_=nullptr;
    QProgressBar* progress_=nullptr;setup::Task* task_=nullptr;
};
}

int main(int argc,char** argv) {
    bool silent=false;for(int i=1;i<argc;++i) if(QByteArray(argv[i])=="--silent-install") silent=true;
    std::unique_ptr<QCoreApplication> application;
    if(silent) application=std::make_unique<QCoreApplication>(argc,argv);
    else application=std::make_unique<QApplication>(argc,argv);
    QCoreApplication::setApplicationName("FrameScale Setup");
    QCoreApplication::setOrganizationName("FrameScale Installer");
    QCommandLineParser parser;
    parser.addOption({"backend","Installation backend","file"});parser.addOption({"version","Application version","version",FRAMESCALE_VERSION});
    parser.addOption({"target","Silent installation target","path"});parser.addOption({"desktop","Create desktop shortcut","enabled","0"});
    parser.addOption({"silent-install","Install without a window"});parser.addOption({"uninstall","Open removal confirmation"});
    parser.process(*application);
    if(silent) {
        setup::Task task;task.finished=[&](bool success,const QString&){application->exit(success?0:1);};
        QTimer::singleShot(0,&task,[&]{task.start(parser.value("backend"),parser.value("target"),false,parser.value("desktop")=="1");});
        return application->exec();
    }
    QApplication::setStyle("Fusion");
    // Installer-specific settings: never change the user's FrameScale theme.
    QSettings settings;settings.setValue("appearance","macos-light");settings.setValue("glassTransparency",true);
    Typography::apply();Appearance::palette();
    SetupWindow window(parser.value("backend"),parser.value("version"),parser.isSet("uninstall"));window.show();
    return application->exec();
}
