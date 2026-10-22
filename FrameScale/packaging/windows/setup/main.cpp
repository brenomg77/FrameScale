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
class SetupProgressBar final : public QProgressBar {
public:
    explicit SetupProgressBar(QWidget* parent):QProgressBar(parent) {
        setProperty("appearanceManaged",true);
        setAttribute(Qt::WA_StyledBackground,false);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(7);
        setTextVisible(false);
        animation_.setInterval(30);
        connect(&animation_,&QTimer::timeout,this,[this] {
            if(isVisible() && minimum()==0 && maximum()==0) {
                phase_=(phase_+1)%100;
                update();
            }
        });
        animation_.start();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        // Paint one complete track and fill. Native progress animations and
        // style-sheet chunks can leave stale strips in a translucent window.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track(rect());
        QPainterPath outline;
        outline.addRoundedRect(track,3.5,3.5);
        painter.setClipPath(outline);
        painter.fillRect(track,QColor(60,60,60,30));
        if(minimum()==0 && maximum()==0) {
            const qreal span=width()*.28;
            const qreal x=(width()+span)*phase_/100.-span;
            painter.fillRect(QRectF(x,0,span,height()),QColor("#007aff"));
        } else if(maximum()>minimum() && value()>minimum()) {
            const qreal fraction=qBound(0.,double(value()-minimum())/(maximum()-minimum()),1.);
            painter.fillRect(QRectF(0,0,width()*fraction,height()),QColor("#007aff"));
        }
    }
private:
    QTimer animation_;
    int phase_=0;
};

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
// The installer owns one complete set of control states. Mixing the shared
// translucent application theme with another hover painter produced competing
// borders and colours on layered Windows surfaces.
const char* setupStyle=R"(
QWidget {font-size:13px;color:#292929;}
QWidget#shell, QWidget#header, QWidget#footer, QStackedWidget, QWidget#page {background:transparent;border:0;}
QProgressBar {background:transparent;border:0;padding:0;}
QProgressBar::chunk {background:transparent;border:0;}
QLabel {background:transparent;border:0;}
QLabel[role="muted"] {color:#666973;font-size:12px;}
QLabel[role="title"] {font-size:15px;font-weight:600;}
QLabel[role="message"] {font-size:13px;color:#55555b;}
QPushButton {background:#f8f9fc;color:#292929;border:1px solid #c7c9d0;border-radius:7px;padding:1px 10px;min-height:18px;min-width:0;font-weight:500;}
QPushButton:hover {background:#eceef3;border-color:#a9adb7;}
QPushButton:pressed {background:#dde1e9;border-color:#9299a6;}
QPushButton:focus {border-color:#007aff;}
QPushButton:disabled {background:#e9ebef;color:#858993;border-color:#d4d7de;}
QPushButton[action="true"] {text-align:left;padding:3px 10px;font-size:14px;font-weight:500;}
QPushButton[action="true"]:checked {background:#e5efff;border-color:#90b9ee;}
QPushButton[action="true"]:checked:hover {background:#d8e8ff;border-color:#6fa4e8;}
QPushButton[action="true"]:pressed {background:#cbdcf4;border-color:#6296da;}
QPushButton[variant="primary"] {background:#007aff;border-color:#007aff;color:white;}
QPushButton[variant="primary"]:hover {background:#0064d2;border-color:#0064d2;}
QPushButton[variant="primary"]:pressed {background:#0054b0;border-color:#0054b0;}
QPushButton[variant="primary"]:focus {border-color:#003b80;}
QPushButton[variant="danger"] {background:#d73532;border-color:#d73532;color:white;}
QPushButton[variant="danger"]:hover {background:#bd2926;border-color:#bd2926;}
QPushButton[variant="danger"]:pressed {background:#a3201e;border-color:#a3201e;}
QPushButton[variant="danger"]:focus {border-color:#701512;}
QPushButton[variant="primary"]:disabled, QPushButton[variant="danger"]:disabled {background:#d9dde5;border-color:#d0d4dc;color:#737985;}
QLineEdit {background:#ffffff;color:#292929;border:1px solid #c7c9d0;border-radius:7px;padding:1px 7px;selection-background-color:#007aff;selection-color:white;}
QLineEdit:hover {border-color:#a9adb7;}
QLineEdit:focus {border-color:#007aff;}
QLineEdit:disabled {background:#e9ebef;color:#858993;border-color:#d4d7de;}
QCheckBox {background:transparent;spacing:8px;}
QCheckBox::indicator {width:13px;height:13px;border:1px solid #a9adb7;border-radius:3px;background:#ffffff;}
QCheckBox::indicator:hover {border-color:#007aff;background:#edf4ff;}
QCheckBox::indicator:checked {background:#007aff;border-color:#007aff;image:url(:/setup/check.svg);}
QCheckBox::indicator:checked:hover {background:#0064d2;border-color:#0064d2;}
QCheckBox::indicator:pressed {background:#d8e8ff;border-color:#007aff;}
QCheckBox::indicator:checked:pressed {background:#0054b0;border-color:#0054b0;}
QCheckBox::indicator:disabled {background:#e9ebef;border-color:#d4d7de;}
QCheckBox:focus {color:#0054b0;}
QCheckBox:disabled {color:#858993;}
QLabel#error {color:#b3262d;font-size:12px;}
QLabel#info {font-size:12px;color:#55555b;}
)";
class SetupPanel final : public QFrame {
public:
    explicit SetupPanel(QWidget* parent):QFrame(parent) {
        setAutoFillBackground(false);setAttribute(Qt::WA_StyledBackground,false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#d5d8e0"),1));
        painter.setBrush(QColor("#eef0f5"));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),12,12);
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
        pages_=new QStackedWidget(shell_);pages_->setFrameShape(QFrame::NoFrame);root->addWidget(pages_,1);
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
        // Retain rounded alpha corners, but paint an opaque interior so desktop
        // colours and native blur cannot bleed through the controls.
        setProperty("appearanceManaged",true);
        framescale::window_backdrop::configureGlassWindow(this,false,false);
        setStyleSheet(QString::fromUtf8(setupStyle));
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
        QPainter painter(this);
        QLinearGradient surface(rect().topLeft(),rect().bottomLeft());
        surface.setColorAt(0,QColor("#fafbfe"));
        surface.setColorAt(1,QColor("#edf0f6"));
        painter.fillRect(rect(),surface);
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
            auto* button=new QPushButton(names[i],panel);button->setProperty("action",true);button->setCheckable(true);button->setFixedHeight(28);button->setCursor(Qt::PointingHandCursor);
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
        progress_=new SetupProgressBar(owner);box->addWidget(progress_);
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
        next_->setProperty("variant",index==1 && action_==2?"danger":"primary");
        next_->style()->unpolish(next_);next_->style()->polish(next_);next_->update();
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
