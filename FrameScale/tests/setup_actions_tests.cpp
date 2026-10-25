#define main setupApplicationMain
#include "../packaging/windows/setup/main.cpp"
#undef main
#include <iostream>
#include <QTemporaryDir>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("FrameScale Installer Tests");
    QCoreApplication::setApplicationName("Action routing");
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QApplication::setStyle("Fusion");
    QSettings().setValue("appearance","macos-light");
    QSettings().setValue("reduceMotion",true);
    Appearance::palette();
    Typography::apply();
    SetupWindow window("unused-backend","test",false);
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();app.processEvents();
    int failures=0;
    auto check=[&](bool ok,const char* message) {
        if(!ok) {++failures;std::cerr<<message<<'\n';}
    };
    const QString captureDir=qEnvironmentVariable("FRAMESCALE_SETUP_CAPTURES");
    if(!captureDir.isEmpty()) QDir().mkpath(captureDir);
    auto capture=[&](const QString& name) {
        app.processEvents();
        QImage image=window.grab().toImage();
        if(!captureDir.isEmpty()) image.save(QDir(captureDir).filePath(name+".png"));
        return image;
    };
    auto pixel=[&](const QImage& image,QWidget* widget,const QPoint& local) {
        const QPoint point=widget->mapTo(&window,local);
        return image.pixelColor(qRound(point.x()*image.devicePixelRatio()),qRound(point.y()*image.devicePixelRatio()));
    };
    capture("actions");
    auto click=[&](QWidget* widget) {
        const QPoint point=widget->mapTo(&window,widget->rect().center());
        QWidget* target=window.childAt(point);
        if(target!=widget) {std::cerr<<"wrong hit target\n";return false;}
        const QPointF local=target->mapFrom(&window,point);
        const QPointF global=target->mapToGlobal(local.toPoint());
        QMouseEvent press(QEvent::MouseButtonPress,local,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,local,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(target,&press);QApplication::sendEvent(target,&release);app.processEvents();
        return true;
    };
    const auto buttons=window.findChildren<QPushButton*>();
    QPushButton* next=nullptr;QPushButton* back=nullptr;
    for(auto* button:buttons) {
        if(button->text()=="Continuar") next=button;
        if(button->text()=="Voltar") back=button;
    }
    if(!next||!back) return 1;
    int count=0;
    for(int cycle=0;cycle<2;++cycle) for(auto* button:buttons) if(button->property("action").toBool()) {
        if(!button->isEnabled() || !click(button) || !button->isChecked()) return 2;
        const QString expected=button->text();
        if(!next->isVisible() || next->text()!=expected.section(' ',0,0)) return 3;
        const QString prefix=QString::number(count);
        const QImage normal=capture(prefix+"-normal");
        const QColor background=pixel(normal,&window,QPoint(240,230));
        check(background.alpha()==255,"Installer content must not reveal desktop colours");
        const QRect geometry=next->geometry();
        auto checkFace=[&](const QImage& image) {
            const QColor face=pixel(image,next,QPoint(8,next->height()/2));
            if(count%3==2) check(face.red()>face.blue()+70,"Uninstall button lost its red action colour");
            else check(face.blue()>face.red()+70,"Primary button lost its blue action colour");
            check(next->geometry()==geometry,"Button geometry changed with interaction state");
            const QColor left=pixel(image,next,QPoint(8,4));
            const QColor right=pixel(image,next,QPoint(next->width()-9,4));
            check(qAbs(left.red()-right.red())<=2 && qAbs(left.blue()-right.blue())<=2,
                "Installer button surface is not centred");
        };
        checkFace(normal);
        next->setAttribute(Qt::WA_UnderMouse,true);
        QEnterEvent enter(QPointF(8,8),QPointF(8,8),next->mapToGlobal(QPoint(8,8)));
        QApplication::sendEvent(next,&enter);
        checkFace(capture(prefix+"-hover"));
        next->setDown(true);
        checkFace(capture(prefix+"-pressed"));
        next->setDown(false);
        next->setAttribute(Qt::WA_UnderMouse,false);
        QEvent leave(QEvent::Leave);QApplication::sendEvent(next,&leave);
        next->clearFocus();
        const QImage released=capture(prefix+"-released");
        checkFace(released);
        window.update();
        const QImage repainted=capture(prefix+"-repainted");
        check(released==repainted,"Full repaint differs from partial repaint after button interaction");
        std::cout<<"click and navigation passed: "<<expected.toStdString()<<'\n';
        if(!click(back)) return 4;
        ++count;
    }
    // A full track repaint must erase the previous fill when progress decreases.
    auto* pages=window.findChild<QStackedWidget*>();
    pages->setCurrentIndex(2);
    auto* progress=window.findChild<QProgressBar*>();
    progress->setRange(0,100);progress->setValue(80);
    capture("progress-80");
    progress->setValue(20);
    const QImage partial=capture("progress-20");
    const QColor track=pixel(partial,progress,QPoint(progress->width()*3/4,3));
    check(qAbs(track.red()-track.blue())<30,"Old progress fill remained on the track");
    window.update();
    check(partial==capture("progress-repainted"),"Progress repaint retained stale pixels");
    return count==6 && failures==0?0:5;
}
