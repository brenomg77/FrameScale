#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include "ui/Typography.h"

inline QWidget* keyboardShortcutsPage(QWidget* parent) {
    auto* page=new QWidget(parent);
    page->setObjectName("keyboardShortcutsPage");
    auto* layout=new QVBoxLayout(page);
    layout->setContentsMargins(12,4,12,12);layout->setSpacing(12);
    auto* title=new QLabel(QObject::tr("Keyboard Shortcuts"),page);
    title->setFont(Typography::display());
    layout->addWidget(title);
    auto* intro=new QLabel(QObject::tr("Os atalhos dependem da área ativa. Clique na timeline para usar os seus comandos."),page);
    intro->setWordWrap(true);layout->addWidget(intro);
    auto* scroll=new QScrollArea(page);scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* list=new QWidget(scroll);auto* groups=new QVBoxLayout(list);
    groups->setContentsMargins(0,0,10,0);groups->setSpacing(18);
    QGridLayout* rows=nullptr;int row=0;
    auto section=[&](const QString& name){
        auto* group=new QWidget(list);auto* box=new QVBoxLayout(group);
        box->setContentsMargins(0,0,0,0);box->setSpacing(8);
        auto* heading=new QLabel(name,group);auto font=heading->font();font.setBold(true);heading->setFont(font);
        box->addWidget(heading);rows=new QGridLayout;rows->setColumnStretch(0,1);
        rows->setHorizontalSpacing(16);rows->setVerticalSpacing(8);box->addLayout(rows);groups->addWidget(group);row=0;
    };
    auto add=[&](const QString& action,const QString& keys){
        auto* label=new QLabel(action,list);label->setWordWrap(true);
        auto* key=new QLabel(keys,list);key->setAlignment(Qt::AlignCenter);
        key->setMinimumHeight(26);key->setContentsMargins(8,3,8,3);
        key->setStyleSheet("QLabel {border:1px solid rgba(128,128,128,90);border-radius:6px;background:rgba(128,128,128,18);}");
        rows->addWidget(label,row,0);rows->addWidget(key,row++,1,Qt::AlignRight);
    };
    section(QObject::tr("Ficheiros e aplicação"));
    add(QObject::tr("Novo projeto"),"Ctrl + N");add(QObject::tr("Abrir ficheiro"),"Ctrl + O");
    add(QObject::tr("Colar imagem ou ficheiro da área de transferência"),"Ctrl + V");
    add(QObject::tr("Sair do programa"),"Ctrl + Q / Alt + F4");
    section(QObject::tr("Edição e camadas"));
    add(QObject::tr("Desfazer edição da timeline"),"Ctrl + Z");
    add(QObject::tr("Refazer edição da timeline"),"Ctrl + Shift + Z");
    add(QObject::tr("Excluir camada selecionada (inclui o grupo unido)"),"Delete");
    section(QObject::tr("Reprodução e timeline"));
    add(QObject::tr("Reproduzir / pausar (timeline ou comparação)"),QObject::tr("Espaço"));
    add(QObject::tr("Fotograma anterior / seguinte (timeline)"),"← / →");
    add(QObject::tr("Aumentar / diminuir zoom da timeline"),"+ ou = / −");
    add(QObject::tr("Repor a vista completa da timeline"),"0");
    section(QObject::tr("Prévia"));
    add(QObject::tr("Ajustar imagem ou vídeo à área de prévia"),"Ctrl + 0");
    add(QObject::tr("Mover a vista com o ponteiro sobre a prévia"),QObject::tr("Segurar Espaço + arrastar"));
    section(QObject::tr("Teclado e rato na timeline"));
    add(QObject::tr("Zoom centrado no ponteiro"),QObject::tr("Ctrl + roda do rato"));
    add(QObject::tr("Abrir menu das camadas, incluindo juntar / separar"),QObject::tr("Ctrl + clique direito"));
    add(QObject::tr("Selecionar áudio / vídeo sem iniciar arrasto (pistas originais)"),QObject::tr("Ctrl + clique"));
    add(QObject::tr("Ajustar o arrasto de vídeo aos fotogramas / alinhar áudio próximo de zero (pistas originais)"),QObject::tr("Shift + arrastar"));
    section(QObject::tr("Fila de exportação"));
    add(QObject::tr("Remover item selecionado na tabela da fila"),"Delete");
    section(QObject::tr("Campos de texto e caixas de diálogo"));
    add(QObject::tr("Selecionar tudo / copiar / cortar / colar texto"),"Ctrl + A / C / X / V");
    add(QObject::tr("Mover foco para o próximo / anterior controlo"),"Tab / Shift + Tab");
    add(QObject::tr("Acionar o botão predefinido / cancelar diálogo"),"Enter / Esc");
    groups->addStretch();scroll->setWidget(list);layout->addWidget(scroll,1);
    return page;
}
