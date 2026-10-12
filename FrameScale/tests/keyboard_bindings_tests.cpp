#include "ui/KeyboardShortcutsPage.h"
#include "ui/TrimTimeline.h"
#include <QTemporaryDir>
#include <iostream>

class TestTimeline : public TrimTimeline {
public:
    using TrimTimeline::keyPressEvent;
};

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("FrameScaleTests");
    QCoreApplication::setApplicationName("KeyboardBindings");
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    auto check=[](bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}};
    QWidget owner;
    auto* shortcut=KeyboardBindings::bind("undo",&owner);
    auto* page=keyboardShortcutsPage(&owner);
    auto find=[&](const QString& id){for(auto* edit:page->findChildren<QKeySequenceEdit*>())
        if(edit->property("keyboardCommand")==id) return edit;return static_cast<QKeySequenceEdit*>(nullptr);};
    auto* playInput=find("play")->findChild<QLineEdit*>();
    check(playInput && playInput->text()==QString::fromUtf8("Espaço"),"Key label was not localized");
    QFocusEvent focusIn(QEvent::FocusIn,Qt::MouseFocusReason);
    QApplication::sendEvent(find("play"),&focusIn);
    check(playInput->text().isEmpty()&&!playInput->placeholderText().isEmpty(),"Capture prompt missing");
    check(find("play")->keySequence()==QKeySequence("Space"),"Capture prompt erased the binding");
    QFocusEvent focusOut(QEvent::FocusOut,Qt::MouseFocusReason);
    QApplication::sendEvent(find("play"),&focusOut);
    check(playInput->text()==QString::fromUtf8("Espaço"),"Leaving capture did not restore key label");
    QApplication::sendEvent(find("play"),&focusIn);
    QKeyEvent capture(QEvent::KeyPress,Qt::Key_K,Qt::ControlModifier);
    QApplication::sendEvent(find("play"),&capture);
    QApplication::sendEvent(find("play"),&focusOut);
    check(find("play")->keySequence()==QKeySequence("Ctrl+K"),"Prompt prevented capturing a new binding");
    find("play")->setKeySequence(KeyboardBindings::defaults("play"));
    find("undo")->setKeySequence(QKeySequence("Ctrl+U"));
    check(KeyboardBindings::sequence("undo")==QKeySequence("Ctrl+Z"),"Unsaved edits changed active keys");
    check(saveKeyboardShortcuts(page),"Valid changes rejected");
    check(shortcut->key()==QKeySequence("Ctrl+U"),"Existing shortcut did not refresh");
    QKeyEvent updated(QEvent::KeyPress,Qt::Key_U,Qt::ControlModifier);
    QKeyEvent original(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier);
    check(KeyboardBindings::matches(&updated,"undo"),"Custom event binding failed");
    check(!KeyboardBindings::matches(&original,"undo"),"Old binding remained active");
    auto* reopened=keyboardShortcutsPage(&owner);
    bool persisted=false;
    for(auto* edit:reopened->findChildren<QKeySequenceEdit*>())
        if(edit->property("keyboardCommand")=="undo") persisted=edit->keySequence()==QKeySequence("Ctrl+U");
    check(persisted,"Saved binding did not survive reopening");
    find("open")->setKeySequence(QKeySequence("Ctrl+U"));
    check(!saveKeyboardShortcuts(page),"Conflicting binding accepted");
    check(KeyboardBindings::sequence("open")==QKeySequence("Ctrl+O"),"Conflict partially saved settings");
    for(auto* edit:page->findChildren<QKeySequenceEdit*>()) edit->setKeySequence(KeyboardBindings::defaults(edit->property("keyboardCommand").toString()));
    check(saveKeyboardShortcuts(page),"Restoring defaults failed");
    find("undo")->setKeySequence({});
    check(saveKeyboardShortcuts(page),"Clearing binding failed");
    check(shortcut->key().isEmpty()&&!KeyboardBindings::matches(&original,"undo"),"Cleared binding remained active");
    find("play")->setKeySequence(QKeySequence("P"));
    find("next")->setKeySequence(QKeySequence("Ctrl+Right"));
    check(saveKeyboardShortcuts(page),"Timeline bindings rejected");
    TestTimeline timeline;
    timeline.setRange(10,0,10,0); timeline.setFrameRate(60);
    int plays=0; double seek=-1;
    timeline.playbackRequested=[&]{++plays;};
    timeline.seekRequested=[&](double value){seek=value;};
    QKeyEvent play(QEvent::KeyPress,Qt::Key_P,Qt::NoModifier);
    timeline.keyPressEvent(&play);
    check(plays==1,"Timeline did not execute custom playback key");
    QKeyEvent oldPlay(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier);
    timeline.keyPressEvent(&oldPlay);
    check(plays==1,"Timeline still executed old playback key");
    QKeyEvent next(QEvent::KeyPress,Qt::Key_Right,Qt::ControlModifier);
    timeline.keyPressEvent(&next);
    check(std::abs(seek-1./60)<1.e-6,"Timeline did not execute custom next-frame key");
    std::cout<<"Keyboard bindings tests passed\n";
}
