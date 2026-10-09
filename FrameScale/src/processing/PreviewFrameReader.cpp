#include "PreviewFrameReader.h"
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

PreviewFrameReader::PreviewFrameReader(QObject* parent) : QObject(parent) {
    startTimer_.setSingleShot(true);
    startTimer_.setInterval(20);
    connect(&startTimer_,&QTimer::timeout,this,&PreviewFrameReader::start);
}
PreviewFrameReader::~PreviewFrameReader() { stop(); }
void PreviewFrameReader::stop() {
    startTimer_.stop();
    if (process_) {
        auto* old=process_;process_=nullptr;
        disconnect(old,nullptr,this,nullptr);
        old->setParent(nullptr);
        if(old->state()==QProcess::NotRunning)old->deleteLater();else old->kill();
    }
    bytes_.clear();
}
void PreviewFrameReader::clear() { stop();cache_.clear();requested_=delivered_=-1;path_.clear(); }
void PreviewFrameReader::configure(const QString& ffmpeg,const QString& path,const QSize& bounds,double fps,qint64 count) {
    if(ffmpeg_==ffmpeg && path_==path && bounds_.width()>=bounds.width() && bounds_.height()>=bounds.height()
       && std::abs(fps_-fps)<1e-8 && count_==count)return;
    clear();ffmpeg_=ffmpeg;path_=path;bounds_=bounds;fps_=fps;count_=count;
}
void PreviewFrameReader::request(qint64 frame) {
    if(path_.isEmpty() || count_<1 || fps_<=0)return;
    requested_=std::clamp(frame,qint64(0),count_-1);
    delivered_=-1;
    if (auto* cached=cache_.object(requested_)) {
        const QImage image=*cached;
        delivered_=requested_;emit frameReady(requested_,image);
        return;
    }
    if(process_ && requested_>=next_ && requested_<end_)return;
    stop();startTimer_.start();
}
void PreviewFrameReader::start() {
    if(requested_<0 || path_.isEmpty())return;
    if(auto* cached=cache_.object(requested_)){const QImage image=*cached;delivered_=requested_;emit frameReady(requested_,image);return;}
    const bool large = qint64(bounds_.width()) * bounds_.height() > 16000000;
    next_=large ? requested_ : std::max(qint64(0),requested_-2);
    end_=std::min(count_,next_+(large ? 1 : 8));
    auto* process=new QProcess(this);process_=process;
    ++decoderStarts_;
    connect(process,&QProcess::readyReadStandardOutput,this,&PreviewFrameReader::consume);
    connect(process,&QProcess::finished,this,[this,process](int code,QProcess::ExitStatus status){
        if(process_!=process)return;
        consume();
        if(process_!=process)return;
        process_=nullptr;bytes_.clear();
        if(delivered_!=requested_) {
            const QString detail=QString::fromLocal8Bit(process->readAllStandardError()).right(600);
            emit failed(code!=0 || status!=QProcess::NormalExit ? detail : tr("O descodificador não devolveu o fotograma solicitado."));
        }
    });
    connect(process,&QProcess::finished,process,&QObject::deleteLater);
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart && process_==process){
            process_=nullptr;process->deleteLater();emit failed(process->errorString());
        }
    });
    QTimer::singleShot(30000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
    // PPM carries dimensions for each frame, avoids PNG compression, and allows
    // presentation before the remainder of the short batch has finished.
    const QString filter=QString("scale=w='min(iw,%1)':h='min(ih,%2)':force_original_aspect_ratio=decrease").arg(bounds_.width()).arg(bounds_.height());
    process->start(ffmpeg_,{"-v","error","-nostdin","-threads","2","-ss",QString::number(std::max(0.,next_/fps_-0.000001),'f',9),
        "-i",path_,"-map","0:v:0","-an","-sn","-filter_threads","1","-vf",filter,
        "-frames:v",QString::number(end_-next_),"-fps_mode","passthrough","-threads","1","-f","image2pipe","-c:v","ppm","pipe:1"});
}
void PreviewFrameReader::consume() {
    if(!process_)return;
    auto* process=process_;
    bytes_+=process->readAllStandardOutput();
    static const QRegularExpression header("\\AP6\\s+(\\d+)\\s+(\\d+)\\s+255[\\r]?\\n");
    while(process_==process && !bytes_.isEmpty()) {
        const auto match=header.match(QString::fromLatin1(bytes_.left(96)));
        if(!match.hasMatch()) {
            if(bytes_.size()>96){stop();emit failed(tr("O descodificador devolveu um fotograma inválido."));}
            return;
        }
        const int width=match.captured(1).toInt(),height=match.captured(2).toInt();
        const qint64 length=qint64(width)*height*3,offset=match.capturedLength();
        if(width<1 || height<1 || width>bounds_.width() || height>bounds_.height() || length>128*1024*1024){
            stop();emit failed(tr("O fotograma excede o limite de memória da preview."));return;
        }
        if(bytes_.size()<offset+length)return;
        QImage image(reinterpret_cast<const uchar*>(bytes_.constData()+offset),width,height,width*3,QImage::Format_RGB888);
        image=image.copy();
        bytes_.remove(0,offset+length);
        const qint64 frame=next_++;
        cache_.insert(frame,new QImage(image),qMax(1,int(image.sizeInBytes()/1024)));
        if(frame==requested_){delivered_=frame;emit frameReady(frame,image);}
    }
}
