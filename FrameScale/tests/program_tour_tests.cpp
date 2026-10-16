#include "ui/ProgramTour.h"
#include "ui/ProjectTransition.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QEventLoop>
#include <iostream>

struct ProgramTourTestAccess {
    static QRect spotlight(const ProgramTour& tour) {return tour.targetRect();}
};

// Commands and persistence only; no visible windows, rendering or screenshots.
int main(int argc,char** argv) {
    QApplication application(argc,argv);
    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("FrameScaleTests");
    QCoreApplication::setApplicationName("ProgramTour");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    int failures=0;
    auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    check(shouldOfferProgramTour(),"first launch did not offer tour");
    markProgramTourSeen();
    check(!shouldOfferProgramTour(),"seen flag was not persisted");
    QWidget owner;owner.resize(900,620);
    int prepared=0;
    const QRect firstTarget(20,0,150,22),secondTarget(600,150,250,300);
    const QString paragraphs=QStringLiteral("Open a file or drag it into the app.\n\nCopy a file from your file manager and paste it here.\n\nOpen another file in a new tab to keep the projects separate.");
    auto* tour=new ProgramTour(&owner,{{"One",paragraphs,{}, [=]{return firstTarget;},[&]{prepared=1;}},{"Two","Second",{}, [=]{return secondTarget;},[&]{prepared=2;}}});
    auto press=[&](const QString& text) {
        for(auto* button:tour->findChildren<QPushButton*>()) if(button->text()==text){button->click();return true;}
        check(false,"expected navigation command missing");return false;
    };
    press(QObject::tr("Começar"));check(prepared==1,"start did not prepare first step");
    auto* body=tour->findChild<QLabel*>("tourBody");
    check(body->height()>=body->heightForWidth(body->width()),"wrapped tour paragraphs were clipped");
    check(ProgramTourTestAccess::spotlight(*tour).top()==1,"menu spotlight clipped the top of the labels");
    press(QObject::tr("Próximo"));check(prepared==2,"next did not advance");
    check(ProgramTourTestAccess::spotlight(*tour).center()==secondTarget.center(),"spotlight lost its target center");
    check(!tour->findChild<QFrame*>("tourCard")->geometry().intersects(ProgramTourTestAccess::spotlight(*tour)),"card overlapped target with free space beside it");
    press(QObject::tr("Voltar"));check(prepared==1,"back did not restore previous step");
    tour->findChild<QPushButton*>("tourClose")->click();
    press(QObject::tr("Continuar tour"));check(prepared==1,"exit confirmation lost the step");
    press(QObject::tr("Próximo"));press(QObject::tr("Próximo"));
    bool finished=false;QObject::connect(tour,&QDialog::finished,[&]{finished=true;});
    press(QObject::tr("Concluir"));check(finished,"finish command did not end tour");
    check(!shouldOfferProgramTour(),"manual replay reset the automatic offer");
    {
        ProjectTransition transition(&owner);
        auto wait=[] {QEventLoop loop;QTimer::singleShot(220,&loop,&QEventLoop::quit);loop.exec();};
        transition.present({});check(!transition.isHidden(),"transition did not retain loading view");
        transition.reveal();transition.present({});wait();
        check(!transition.isHidden(),"previous tab fade removed the new loading view");
        transition.reveal();wait();check(transition.isHidden(),"ready view did not finish transition");
    }
    if(!failures) std::cout<<"Tour: first-run persistence, replay, navigation and exit confirmation passed\n";
    return failures?1:0;
}
