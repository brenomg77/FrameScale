#pragma once
#include "Appearance.h"
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

class OutputConflictDialog final : public QDialog {
public:
    enum Choice { Cancelled=QDialog::Rejected, Replace=QDialog::Accepted, KeepBoth=2 };
    explicit OutputConflictDialog(const QString& path,QWidget* parent=nullptr):QDialog(parent) {
        setObjectName("outputConflictDialog");
        setWindowTitle(tr("Substituir arquivo?"));
        setProperty("dialogTitleHeight",36);
        auto* layout=new QVBoxLayout(this);
        layout->setContentsMargins(16,8,16,16);layout->setSpacing(14);
        auto* message=new QLabel(tr("Já existe um arquivo com este nome:\n%1").arg(path),this);
        message->setObjectName("outputConflictMessage");
        message->setTextFormat(Qt::PlainText);message->setWordWrap(true);
        message->setTextInteractionFlags(Qt::TextSelectableByMouse);
        message->setMaximumWidth(440);layout->addWidget(message);
        auto* buttons=new QDialogButtonBox(this);
        auto* replace=buttons->addButton(tr("Substituir"),QDialogButtonBox::DestructiveRole);
        replace->setObjectName("replaceOutputButton");
        auto* keep=buttons->addButton(tr("Manter ambos"),QDialogButtonBox::AcceptRole);
        keep->setObjectName("keepBothOutputButton");keep->setProperty("variant","primary");keep->setDefault(true);
        auto* cancel=buttons->addButton(QDialogButtonBox::Cancel);
        cancel->setObjectName("cancelOutputButton");
        connect(replace,&QPushButton::clicked,this,[this]{done(Replace);});
        connect(keep,&QPushButton::clicked,this,[this]{done(KeepBoth);});
        connect(cancel,&QPushButton::clicked,this,&QDialog::reject);
        layout->addWidget(buttons);
        Appearance::apply(this);
        setFixedWidth(460);adjustSize();
    }
};
