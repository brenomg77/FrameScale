#pragma once
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QToolButton>
#include <QDialog>
#include <QLineEdit>
#include <QFocusEvent>
#include <QRegularExpression>
#include <QSignalBlocker>
#include "ui/Typography.h"
#include "ui/ControlSymbols.h"
#include "ui/KeyboardBindings.h"

class ShortcutSequenceEdit final : public QKeySequenceEdit {
public:
    ShortcutSequenceEdit(const QKeySequence& sequence,QWidget* parent):QKeySequenceEdit(sequence,parent) {
        connect(this,&QKeySequenceEdit::keySequenceChanged,this,[this]{displaySequence();});
        connect(this,&QKeySequenceEdit::editingFinished,this,[this]{displaySequence();});
        displaySequence();
    }
protected:
    void focusInEvent(QFocusEvent* event) override {
        const auto previous=keySequence();
        QKeySequenceEdit::focusInEvent(event);
        setKeySequence(previous);
        if(auto* input=findChild<QLineEdit*>()) {
            input->setPlaceholderText(QObject::tr("Pressione as teclas…"));
            const QSignalBlocker blocker(input);
            input->clear();
        }
    }
    void focusOutEvent(QFocusEvent* event) override {
        QKeySequenceEdit::focusOutEvent(event);
        displaySequence();
    }
private:
    void displaySequence() {
        auto* input=findChild<QLineEdit*>();if(!input)return;
        QString text=keySequence().toString(QKeySequence::PortableText);
        const char* keys[]={"Space","Left","Right","Up","Down","Del","Backspace","Esc"};
        const char* labels[]={"Espaço","Esquerda","Direita","Cima","Baixo","Excluir","Apagar","Esc"};
        for(int i=0;i<8;++i)
            text.replace(QRegularExpression(QString("\\b%1\\b").arg(keys[i])),QObject::tr(labels[i]));
        input->setPlaceholderText(QObject::tr("Pressione as teclas…"));
        input->setText(text);
    }
};

inline QWidget* keyboardShortcutsPage(QWidget* parent) {
    auto* page = new QWidget(parent);
    page->setObjectName("keyboardShortcutsPage");
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12,4,12,2); layout->setSpacing(6);
    auto* header=new QHBoxLayout;
    auto* title = new QLabel(QObject::tr("Atalhos de teclado"), page);
    title->setStyleSheet(QString("font-family:\"%1\";font-size:22px;font-weight:600;background:transparent;").arg(Typography::displayFamily));
    header->addWidget(title,1);layout->addLayout(header);
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName("keyboardShortcutsScroll");
    scroll->viewport()->setObjectName("keyboardShortcutsViewport");
    auto* list = new QWidget(scroll); list->setObjectName("keyboardShortcutsList");
    auto* groups = new QVBoxLayout(list); groups->setContentsMargins(0,0,8,0); groups->setSpacing(8);
    scroll->setStyleSheet("QScrollArea#keyboardShortcutsScroll, QWidget#keyboardShortcutsViewport, QWidget#keyboardShortcutsList {background:transparent;border:0;} QWidget#shortcutGroup {background:rgba(128,128,128,16);border-radius:9px;} QPushButton#shortcutHeading {background:transparent;border:0;text-align:left;padding:5px 6px;font-weight:600;} QKeySequenceEdit {background:rgba(128,128,128,18);border:1px solid rgba(128,128,128,65);border-radius:6px;padding:2px;} QKeySequenceEdit QLineEdit {background:transparent;border:0;padding:0;font-size:12px;}");
    const char* names[] = {"Ficheiros e aplicação", "Edição e camadas", "Reprodução e timeline", "Prévia", "Fila de exportação", "Teclado e rato"};
    using S = ControlSymbols::Symbol;
    const S icons[] = {S::Folder,S::Layers,S::Frames,S::Eye,S::Queue,S::Sliders};
    for (int groupIndex=0; groupIndex<6; ++groupIndex) {
        auto* group = new QWidget(list); group->setObjectName("shortcutGroup");
        auto* box = new QVBoxLayout(group); box->setContentsMargins(5,3,5,5); box->setSpacing(4);
        auto* heading = new QPushButton(QObject::tr(names[groupIndex]), group);
        heading->setObjectName("shortcutHeading"); heading->setAutoDefault(false);
        heading->setIcon(ControlSymbols::icon(icons[groupIndex])); heading->setIconSize(QSize(15,15));
        heading->setCheckable(true); heading->setChecked(false);
        auto* headingRow=new QHBoxLayout;headingRow->setSpacing(2);headingRow->addWidget(heading,1);
        auto* expand=new QToolButton(group);expand->setObjectName("shortcutExpandButton");
        expand->setAutoRaise(true);expand->setFixedSize(22,22);expand->setIconSize(QSize(13,13));
        auto updateExpansion=[expand](bool open){expand->setIcon(ControlSymbols::icon(open?S::Minus:S::Plus));expand->setToolTip(open?QObject::tr("Contrair"):QObject::tr("Expandir"));};
        updateExpansion(false);
        QObject::connect(expand,&QToolButton::clicked,heading,&QPushButton::toggle);
        QObject::connect(heading,&QPushButton::toggled,expand,updateExpansion);
        headingRow->addWidget(expand);box->addLayout(headingRow);
        auto* body = new QWidget(group); auto* rows = new QVBoxLayout(body);
        rows->setContentsMargins(6,2,6,4); rows->setSpacing(7);
        for (const auto& command : KeyboardBindings::commands) {
            if (command.group != groupIndex) continue;
            auto* row = new QHBoxLayout; row->setSpacing(8);
            auto* label = new QLabel(QObject::tr(command.label), body); label->setWordWrap(true);
            auto* edit = new ShortcutSequenceEdit(KeyboardBindings::sequence(command.id), body);
            edit->setMaximumSequenceLength(1); edit->setClearButtonEnabled(true);
            edit->setFixedWidth(118); edit->setFixedHeight(28);
            edit->setProperty("keyboardCommand", command.id);
            edit->setAccessibleName(label->text()); edit->setToolTip(QObject::tr("Clique e pressione a nova combinação de teclas."));
            row->addWidget(label,1); row->addWidget(edit); rows->addLayout(row);
        }
        auto info = [&](const QString& text) {auto* label=new QLabel(text,body);label->setWordWrap(true);rows->addWidget(label);};
        if(groupIndex==5) info(QObject::tr("Ctrl + roda do rato: zoom\nCtrl + clique direito: menu das camadas\nCtrl + clique: selecionar pistas originais\nShift + arrastar: alinhar pistas originais\nEspaço + arrastar: mover a prévia"));
        body->hide();
        QObject::connect(heading,&QPushButton::toggled,body,&QWidget::setVisible);
        box->addWidget(body); groups->addWidget(group);
    }
    groups->addStretch(); scroll->setWidget(list); layout->addWidget(scroll,1);
    auto* error = new QLabel(page); error->setObjectName("shortcutError"); error->setWordWrap(true);
    error->setStyleSheet("color:#e87878;background:transparent;"); error->hide(); layout->addWidget(error);
    auto* reset = new QPushButton(QObject::tr("Restaurar atalhos originais"),page);
    reset->setAutoDefault(false); reset->setIcon(ControlSymbols::icon(S::Reset));
    reset->setFocusPolicy(Qt::NoFocus);
    reset->setIconSize(QSize(13,13));reset->setFixedHeight(22);
    reset->setStyleSheet("font-size:12px;padding:0px 6px;min-height:18px;");
    layout->addWidget(reset,0,Qt::AlignLeft);
    QObject::connect(reset,&QPushButton::clicked,page,[page,error]{
        for(auto* edit:page->findChildren<QKeySequenceEdit*>()) edit->setKeySequence(KeyboardBindings::defaults(edit->property("keyboardCommand").toString()));
        error->hide();
    });
    return page;
}
inline bool saveKeyboardShortcuts(QWidget* page) {
    const auto edits=page->findChildren<QKeySequenceEdit*>();
    for(int i=0;i<edits.size();++i) for(int j=i+1;j<edits.size();++j) {
        const auto a=edits[i]->keySequence(), b=edits[j]->keySequence();
        const auto first=edits[i]->property("keyboardCommand").toString(), second=edits[j]->property("keyboardCommand").toString();
        if((first=="delete" && second=="queueDelete") || (first=="queueDelete" && second=="delete")) continue;
        if(!a.isEmpty() && a==b) {
            auto* error=page->findChild<QLabel*>("shortcutError");
            error->setText(QObject::tr("O atalho %1 está repetido em %2 e %3.").arg(a.toString(QKeySequence::NativeText),edits[i]->accessibleName(),edits[j]->accessibleName()));
            error->show(); return false;
        }
    }
    QSettings settings;
    for(auto* edit:edits) settings.setValue("keyboardShortcuts/"+edit->property("keyboardCommand").toString(), edit->keySequence().toString(QKeySequence::PortableText));
    KeyboardBindings::refresh(); return true;
}
