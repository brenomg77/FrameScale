#include "ui/Appearance.h"
#include <QEventLoop>
#include <QTemporaryDir>
#include <QDir>
#include <cmath>
#include <iostream>

// Render only our own hidden widgets, including when exercising the Windows
// platform plugin. Never capture the desktop or move the system pointer.
class BackdropFixture final : public QWidget {
public:
    QColor left{210,40,30,128}, right{25,70,225,128};
    bool plainGlass=false;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        if(plainGlass) {GlassMaterial::paintWindowSurface(p,rect(),Appearance::light());return;}
        p.fillRect(QRect(0,0,width()/2,height()),left);
        p.fillRect(QRect(width()/2,0,width()/2,height()),right);
    }
};

int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setStyle("Fusion");
    QCoreApplication::setOrganizationName("FrameScaleTests");
    QCoreApplication::setApplicationName("RoundedWindows");
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings().setValue("reduceMotion",true);
    QSettings().setValue("glassTransparency",true);
    int failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    auto settle=[&]{app.processEvents();app.processEvents();};
    auto tick=[&]{QEventLoop loop;QTimer::singleShot(180,&loop,&QEventLoop::quit);loop.exec();settle();};
    const QString captures=argc>1?QString::fromLocal8Bit(argv[1]):QString();
    if(!captures.isEmpty()) QDir().mkpath(captures);
    BackdropFixture owner;
    owner.setAttribute(Qt::WA_DontShowOnScreen);
    owner.setWindowFlag(Qt::FramelessWindowHint);
    owner.setAttribute(Qt::WA_TranslucentBackground);
    owner.resize(800,500);owner.move(100,100);owner.show();settle();
    auto inspect=[&](QWidget& window,const QString& name) {
        const QImage image=window.grab().toImage();
        check(!image.isNull(),"Window capture is empty");
        bool outside=false;
        const qreal ratio=image.devicePixelRatio();
        for(int y=0;y<image.height();++y) for(int x=0;x<image.width();++x) {
            const qreal dx=std::min((x+.5)/ratio,window.width()-(x+.5)/ratio);
            const qreal dy=std::min((y+.5)/ratio,window.height()-(y+.5)/ratio);
            if(dx<14 && dy<14 && std::hypot(14-dx,14-dy)>14.8
                && image.pixelColor(x,y).alpha()!=0) outside=true;
        }
        check(!outside,"Window paints a square tip outside its rounded outline");
        check(!window.property("nativeGlassActive").toBool(),"Owned window still has rectangular native blur");
#ifdef Q_OS_WIN
        if(QApplication::platformName()=="windows") {
            const auto hwnd=reinterpret_cast<HWND>(window.winId());
            check(!(GetWindowLongPtr(hwnd,GWL_STYLE)&(WS_BORDER|WS_DLGFRAME|WS_THICKFRAME)),"Native frame remains around rounded dialog");
            check(!(GetWindowLongPtr(hwnd,GWL_EXSTYLE)&(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE|WS_EX_DLGMODALFRAME)),"Native extended frame remains around rounded dialog");
            HRGN region=CreateRectRgn(0,0,0,0);
            check(GetWindowRgn(hwnd,region)!=ERROR && PtInRegion(region,image.width()/2,image.height()/2)
                && !PtInRegion(region,0,0) && !PtInRegion(region,image.width()-1,0)
                && !PtInRegion(region,0,image.height()-1) && !PtInRegion(region,image.width()-1,image.height()-1),
                "Native window region exposes a square corner");
            DeleteObject(region);
        }
#endif
        if(!captures.isEmpty()) check(image.save(captures+'/'+name+".png"),"Could not save window fixture");
    };
    for(const QString theme:{QString("macos-light"),QString("macos-dark")}) {
        QSettings().setValue("appearance",theme);Appearance::palette();
        for(const auto flags:{Qt::WindowFlags(Qt::Dialog),Qt::WindowFlags(Qt::Tool)}) {
            QDialog dialog(&owner,flags);dialog.setAttribute(Qt::WA_DontShowOnScreen);
            dialog.setWindowTitle("Rounded window");
            auto* layout=new QVBoxLayout(&dialog);layout->addStretch();
            Appearance::apply(&dialog);dialog.resize(240,160);
            dialog.show();settle();dialog.move(owner.mapToGlobal(QPoint(30,40)));settle();
            const QString name=theme+(flags==Qt::Dialog?"-dialog":"-tool");
            inspect(dialog,name);
            auto backdrop=[&]{return dialog.property("glassBackdrop").value<QImage>();};
            check(!backdrop().isNull(),"Owned dialog did not retain its blurred backdrop");
            if(!backdrop().isNull()) {
                const auto colour=backdrop().pixelColor(backdrop().rect().center());
                // The hidden Windows backend may place a dialog itself; sample
                // against its actual mapped position rather than the request.
                const QPoint centre=owner.mapFromGlobal(dialog.mapToGlobal(dialog.rect().center()));
                const QColor expected=centre.x()<owner.width()/2?owner.left:owner.right;
                check(qAbs(colour.red()-expected.red())<=3 && qAbs(colour.blue()-expected.blue())<=3
                    && colour.alpha()>=125 && colour.alpha()<=130,
                    "Backdrop loses the owner's colour or transparency");
                const auto first=backdrop().cacheKey();tick();
                check(first==backdrop().cacheKey(),"Unchanged background is repeatedly rebuilt or captures itself");
            }
            dialog.move(owner.mapToGlobal(QPoint(460,100)));dialog.resize(260,180);settle();
            inspect(dialog,name+"-moved");
            check(!backdrop().isNull() && backdrop().pixelColor(backdrop().rect().center()).blue()>150,
                "Moving the dialog did not refresh the background");
            owner.right=QColor(30,210,70,128);owner.update();tick();
            check(!backdrop().isNull() && backdrop().pixelColor(backdrop().rect().center()).green()>150,
                "Visible dialog does not follow changes behind it");
            owner.right=QColor(25,70,225,128);owner.update();
            QSettings().setValue("glassTransparency",false);Appearance::apply(&dialog);settle();
            check(backdrop().isNull(),"Opaque mode retains a stale blurred image");inspect(dialog,name+"-opaque");
            QSettings().setValue("glassTransparency",true);Appearance::apply(&dialog);settle();
            check(!backdrop().isNull(),"Re-enabling glass does not restore the backdrop");inspect(dialog,name+"-restored");
            QSettings().setValue("glassTransparency",false);Appearance::apply(&dialog);
            QSettings().setValue("glassTransparency",true);Appearance::apply(&dialog);settle();
            check(!backdrop().isNull(),"A quick theme toggle leaves the unchanged backdrop empty");
            owner.plainGlass=true;owner.setProperty("glassWindowSurface",true);owner.update();tick();
            const QImage empty=dialog.grab().toImage();
            const int faceAlpha=empty.pixelColor(empty.rect().center()).alpha();
            check(faceAlpha<(theme=="macos-light"?185:135),"Empty glass window duplicates the owner's tint and becomes opaque");
            owner.plainGlass=false;owner.setProperty("glassWindowSurface",false);owner.update();
            dialog.hide();settle();
        }
        // Font chooser containers are QWidget popups, not QDialog subclasses.
        QWidget popup(&owner,Qt::Popup);popup.setAttribute(Qt::WA_DontShowOnScreen);
        popup.setProperty("glassWindowSurface",true);installRoundedWindow(&popup);
        framescale::window_backdrop::configureGlassWindow(&popup,true,theme=="macos-dark");
        popup.resize(220,170);popup.show();settle();popup.move(owner.mapToGlobal(QPoint(50,50)));settle();
        inspect(popup,theme+"-popup");
        check(!popup.property("glassBackdrop").value<QImage>().isNull(),"Popup lacks its clipped glass backdrop");
        popup.hide();
        // Export queue opts out of cached blur but must share frame cleanup.
        QDialog stable(&owner);stable.setAttribute(Qt::WA_DontShowOnScreen);
        stable.setProperty("stableSurface",true);
        Appearance::apply(&stable);stable.resize(240,160);stable.show();settle();
        inspect(stable,theme+"-stable");stable.hide();
    }
    if(!failures) std::cout<<"Rounded windows: alpha, native regions, movement, live backdrop and theme toggles OK\n";
    return failures?1:0;
}
