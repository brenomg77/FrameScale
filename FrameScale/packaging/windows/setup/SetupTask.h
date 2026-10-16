#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <functional>

namespace setup {
inline QString installedPath() {
    return QSettings("HKEY_CURRENT_USER\\Software\\FrameScale\\Installer",QSettings::NativeFormat)
        .value("InstallDir").toString();
}
inline QString defaultPath() {
    const QString existing=installedPath();
    return existing.isEmpty()?QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath("Programs/FrameScale"):existing;
}
inline QString validatePath(const QString& path) {
    const QFileInfo info(path);
    if(!info.isAbsolute() || path.contains('"') || path.contains('\n') || path.contains('\r'))
        return QString::fromUtf8("Escolha uma pasta local com caminho completo.");
    const auto clean=QDir::cleanPath(path);
    if(QDir(clean).isRoot() || info.isFile()) return QString::fromUtf8("Escolha uma pasta para o FrameScale.");
    const auto windows=QDir::cleanPath(qEnvironmentVariable("WINDIR"));
    if(!windows.isEmpty() && (clean.compare(windows,Qt::CaseInsensitive)==0
        || clean.startsWith(windows+"/",Qt::CaseInsensitive)))
        return QString::fromUtf8("Escolha uma pasta fora do diretório do Windows.");
    return {};
}

// Shared by the visible installer and its silent mode. Progress comes from the
// backend's completed-file byte counter, never from a timer pretending to work.
class Task final : public QObject {
public:
    explicit Task(QObject* parent=nullptr):QObject(parent) {
        timer_.setInterval(100);
        connect(&timer_,&QTimer::timeout,this,[this] {
            QFile status(temp_.filePath("progress.txt"));
            if(status.open(QIODevice::ReadOnly)) {
                bool ok=false;const int value=status.readAll().trimmed().toInt(&ok);
                if(ok && progress) progress(qBound(0,value,100));
            }
        });
        connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status) {
            timer_.stop();running_=false;
            const bool success=status==QProcess::NormalExit && code==0;
            if(success && progress) progress(100);
            if(finished) finished(success,success?QString():message(code));
        });
        connect(&process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error) {
            if(error!=QProcess::FailedToStart) return;
            timer_.stop();running_=false;
            if(finished) finished(false,QString::fromUtf8("Não foi possível iniciar a instalação. Abra novamente o instalador completo."));
        });
    }
    bool running() const {return running_;}
    void start(const QString& backend,const QString& target,bool remove,bool desktop) {
        QString error=validatePath(target);
        const QString program=remove?QDir(target).filePath("Uninstall.exe"):backend;
        if(error.isEmpty() && (!temp_.isValid() || !QFileInfo::exists(program)))
            error=QString::fromUtf8("O pacote de instalação não está disponível. Abra o instalador completo.");
        if(!error.isEmpty()) {if(finished) finished(false,error);return;}
        running_=true;
        process_.setProgram(program);
        process_.setArguments({"/S"});
        // NSIS requires /D= to be the unquoted final argument, including spaces.
        // Quotes/newlines in destinations are rejected by validatePath().
        if(remove) process_.setNativeArguments("_?="+QDir::toNativeSeparators(target));
        else process_.setNativeArguments(QString("/DESKTOP=%1 /STATUS=\"%2\" /D=%3")
            .arg(desktop?1:0).arg(QDir::toNativeSeparators(temp_.filePath("progress.txt")),QDir::toNativeSeparators(target)));
        process_.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags|=0x08000000; // CREATE_NO_WINDOW
        });
        timer_.start();process_.start();
    }
    std::function<void(int)> progress;
    std::function<void(bool,const QString&)> finished;
private:
    static QString message(int code) {
        if(code==2) return QString::fromUtf8("Feche o FrameScale e tente novamente.");
        if(code==4) return QString::fromUtf8("O FrameScale requer Windows 10 ou 11 de 64 bits.");
        return QString::fromUtf8("Não foi possível concluir. Confira o espaço livre e a permissão para gravar na pasta escolhida. Código: %1.").arg(code);
    }
    QProcess process_;
    QTimer timer_;
    QTemporaryDir temp_;
    bool running_=false;
};
}
