#pragma once
#include "processing/RuntimePaths.h"
#include "ui/Appearance.h"
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QProcess>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCloseEvent>
#include <memory>
#include <QTimer>

class VideoDownloadDialog final : public QDialog {
public:
    explicit VideoDownloadDialog(QWidget* parent=nullptr):QDialog(parent) {
        setObjectName("urlDownloadDialog");setWindowTitle(tr("Abrir por URL"));setMinimumWidth(480);resize(520,230);setFont(QApplication::font());
        auto* layout=new QVBoxLayout(this);layout->setContentsMargins(16,12,16,16);layout->setSpacing(8);
        layout->addWidget(new QLabel(tr("Link do vídeo"),this));
        url_=new QLineEdit(this);url_->setPlaceholderText(tr("Cole o link do YouTube, TikTok ou Instagram"));layout->addWidget(url_);
        layout->addWidget(new QLabel(tr("Salvar em"),this));
        auto* row=new QHBoxLayout;row->setSpacing(8);folder_=new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),this);
        row->addWidget(folder_);browse_=new QPushButton(tr("Escolher..."),this);row->addWidget(browse_);layout->addLayout(row);
        status_=new QLabel(this);status_->setWordWrap(true);status_->setTextFormat(Qt::PlainText);layout->addWidget(status_);status_->hide();
        progress_=new QProgressBar(this);progress_->setRange(0,100);progress_->setValue(0);progress_->setFixedHeight(6);progress_->setTextVisible(false);progress_->hide();layout->addWidget(progress_);
        auto* actions=new QHBoxLayout;actions->addStretch();cancel_=new QPushButton(tr("Fechar"),this);start_=new QPushButton(tr("Baixar"),this);start_->setProperty("variant","primary");actions->addWidget(cancel_);actions->addWidget(start_);layout->addLayout(actions);
        connect(browse_,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getExistingDirectory(this,tr("Salvar vídeo em"),folder_->text());if(!path.isEmpty())folder_->setText(path);});
        connect(start_,&QPushButton::clicked,this,[this]{if(!saved_.isEmpty()){edit_=true;accept();}else begin();});
        connect(cancel_,&QPushButton::clicked,this,[this]{reject();});
        lookupTimer_.setSingleShot(true);
        lookupTimer_.setInterval(45000);
        connect(&lookupTimer_,&QTimer::timeout,this,[this]{
            stop();controls(true);progress_->hide();
            showError(tr("O site demorou demais para responder. Verifique o link e tente novamente."));
        });
        process_.setProcessChannelMode(QProcess::MergedChannels);
        connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{consume();});
        connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart){lookupTimer_.stop();busy_=false;controls(true);showError(tr("Não foi possível iniciar o download. Verifique a instalação."));}});
        connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus state){
            consume();lookupTimer_.stop();busy_=false;if(cancelled_){temporary_.reset();return;}controls(true);
            if(code==0 && downloaded_.isEmpty()){showError(tr("Este link não disponibilizou um vídeo gravado. Transmissões ao vivo ou agendadas não são suportadas."));temporary_.reset();return;}
            if(code!=0 || state!=QProcess::NormalExit || !QFileInfo(downloaded_).isFile()) {showError(tr("Não foi possível baixar. O link pode estar indisponível ou exigir login.\n%1").arg(error_.right(700)));temporary_.reset();return;}
            const auto canonical=QFileInfo(downloaded_).canonicalFilePath();
            if(!canonical.startsWith(QFileInfo(temporary_->path()).canonicalFilePath()+"/",Qt::CaseInsensitive)){showError(tr("O download retornou um arquivo inválido."));return;}
            QFileInfo info(downloaded_);QString target=QDir(destination_).filePath(info.fileName());
            for(int n=1;QFileInfo::exists(target);++n)target=QDir(destination_).filePath(info.completeBaseName()+QString(" (%1).").arg(n)+info.suffix());
            if(!QFile::rename(downloaded_,target)){temporary_->setAutoRemove(false);showError(tr("Não foi possível mover o vídeo. Ele está salvo em:\n%1").arg(downloaded_));return;}
            saved_=target;temporary_.reset();progress_->setRange(0,100);progress_->setValue(100);status_->clear();status_->hide();progress_->hide();url_->setEnabled(false);folder_->setEnabled(false);browse_->setEnabled(false);start_->setText(tr("Editar agora"));start_->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Sliders,true));cancel_->setText(tr("Concluir"));
        });
        for(auto* button:{browse_,cancel_,start_})button->setFixedHeight(24);
        url_->setMinimumHeight(24);folder_->setMinimumHeight(24);
        browse_->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Folder));
        start_->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Import,true));
        for(auto* button:{browse_,start_})button->setIconSize(QSize(19,15));
        Appearance::apply(this);
    }
    QString fileToEdit() const {return edit_?saved_:QString();}
    void reject() override {stop();QDialog::reject();}
    ~VideoDownloadDialog() override {stop();}
protected:
    void closeEvent(QCloseEvent* event) override {stop();QDialog::closeEvent(event);}
private:
    void showError(const QString& text){progress_->hide();status_->setText(text);status_->show();}
    void controls(bool enabled){url_->setEnabled(enabled);folder_->setEnabled(enabled);browse_->setEnabled(enabled);start_->setEnabled(enabled);cancel_->setText(enabled?tr("Fechar"):tr("Cancelar"));if(enabled){progress_->setRange(0,100);progress_->setValue(0);}}
    void stop(){lookupTimer_.stop();if(!busy_)return;cancelled_=true;
#ifdef Q_OS_WIN
        QProcess killer;if(process_.processId()>0)killer.start("taskkill",{"/PID",QString::number(process_.processId()),"/T","/F"});killer.waitForFinished(3000);
#endif
        process_.kill();process_.waitForFinished(3000);busy_=false;temporary_.reset();}
    void consume(){buffer_+=process_.readAllStandardOutput();int end;while((end=buffer_.indexOf('\n'))>=0){QString line=QString::fromUtf8(buffer_.left(end)).trimmed();buffer_.remove(0,end+1);
        if(line.startsWith("FS_READY:"))lookupTimer_.stop();
        else if(line.startsWith("FS_FILE:"))downloaded_=line.mid(8);
        else if(line.startsWith("FS_PROGRESS:")){QString value=line.mid(12).trimmed();value.remove('%');bool ok=false;double percent=value.toDouble(&ok);if(ok){progress_->setRange(0,100);progress_->setValue(qBound(0,int(percent),100));}}
        else if(line.startsWith("ERROR:"))error_=(error_+"\n"+line).right(1500);
    }if(buffer_.size()>65536)buffer_=buffer_.right(65536);}
    void begin(){QUrl url(url_->text().trimmed());if(!url.isValid() || (url.scheme()!="https" && url.scheme()!="http") || url.host().isEmpty() || !url.userInfo().isEmpty()){showError(tr("Informe um link HTTP ou HTTPS válido."));return;}
        const QString host=url.host().toLower();
        const QStringList parts=url.path().split('/',Qt::SkipEmptyParts);
        if ((host=="twitch.tv" || host.endsWith(".twitch.tv")) && host!="clips.twitch.tv"
            && !parts.isEmpty() && parts.first()!="videos" && !parts.contains("clip")) {
            showError(tr("Transmissões ao vivo não são suportadas. Use o link de um vídeo gravado ou clipe."));return;
        }
        framescale::RuntimePaths paths;auto tool=paths.tool("yt-dlp");if(!QFileInfo::exists(tool)){showError(tr("O componente de download não está instalado."));return;}
        destination_=QDir(folder_->text()).absolutePath();if(!QDir(destination_).exists()){showError(tr("Escolha uma pasta existente para salvar."));return;}
        temporary_=std::make_unique<QTemporaryDir>(QDir(destination_).filePath(".framescale-download-XXXXXX"));if(!temporary_->isValid()){showError(tr("Não foi possível gravar nessa pasta."));return;}
        downloaded_.clear();error_.clear();buffer_.clear();cancelled_=false;busy_=true;controls(false);progress_->show();progress_->setRange(0,0);status_->clear();status_->hide();
        QStringList args={"--ignore-config","--no-plugin-dirs","--no-playlist","--playlist-items","1","--no-simulate","--match-filters","!is_live & !is_upcoming","--print","before_dl:FS_READY:1","--newline","--no-colors","--encoding","utf-8","--socket-timeout","30","--retries","3","--no-overwrites","--no-remote-components","--ffmpeg-location",QFileInfo(paths.tool("ffmpeg")).absolutePath(),"--js-runtimes","deno:"+paths.tool("deno"),"-f","bv*+ba/b","--merge-output-format","mkv","--windows-filenames","--trim-filenames","150","-P",temporary_->path(),"-o","%(title).120B [%(id)s].%(ext)s","--progress","--progress-template","download:FS_PROGRESS:%(progress._percent_str)s","--print","after_move:FS_FILE:%(filepath)s","--",url.toString()};
        lookupTimer_.start();
        process_.start(tool,args);
    }
    QLineEdit *url_,*folder_;QPushButton *browse_,*cancel_,*start_;QLabel* status_;QProgressBar* progress_;QProcess process_;QTimer lookupTimer_;QByteArray buffer_;QString downloaded_,destination_,saved_,error_;std::unique_ptr<QTemporaryDir> temporary_;bool busy_=false,cancelled_=false,edit_=false;
};
