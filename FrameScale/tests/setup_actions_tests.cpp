#define main setupApplicationMain
#include "../packaging/windows/setup/main.cpp"
#undef main
#include <iostream>

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("FrameScale Installer Tests");
    QCoreApplication::setApplicationName("Action routing");
    QApplication::setStyle("Fusion");
    QSettings().setValue("appearance","macos-light");
    Appearance::palette();
    SetupWindow window("unused-backend","test",false);
    window.show();app.processEvents();
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
    for(auto* button:buttons) if(button->property("action").toBool()) {
        if(!button->isEnabled() || !click(button) || !button->isChecked()) return 2;
        const QString expected=button->text();
        if(!next->isVisible() || next->text()!=expected.section(' ',0,0)) return 3;
        std::cout<<"click and navigation passed: "<<expected.toStdString()<<'\n';
        if(!click(back)) return 4;
        ++count;
    }
    return count==3?0:5;
}
