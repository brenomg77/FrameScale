#include "processing/PreviewFrameReader.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QProcess>
#include <QTimer>
#include <iostream>
#include <cmath>

int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    const auto args=app.arguments();
    if(args.size()!=5){std::cerr<<"ffmpeg input fps frameCount\n";return 64;}
    const QString ffmpeg=args[1],path=args[2];
    const double fps=args[3].toDouble();const qint64 count=args[4].toLongLong();
    PreviewFrameReader reader;reader.configure(ffmpeg,path,QSize(640,360),fps,count);
    QString failure;
    QObject::connect(&reader,&PreviewFrameReader::failed,&app,[&](const QString& message){failure=message;});
    auto read=[&](qint64 frame,bool rapid=false) {
        QEventLoop loop;QImage image;QElapsedTimer elapsed;elapsed.start();
        auto connection=QObject::connect(&reader,&PreviewFrameReader::frameReady,&loop,[&](qint64 got,const QImage& value){
            if(got!=frame){failure="stale frame delivered";loop.quit();return;}
            image=value;loop.quit();
        });
        QTimer timer;timer.setSingleShot(true);QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);timer.start(30000);
        if(rapid){reader.request(4);reader.request(2);}
        reader.request(frame);
        if(image.isNull())loop.exec();
        QObject::disconnect(connection);
        std::cout<<"frame="<<frame<<" ms="<<elapsed.elapsed()<<" decoders="<<reader.decoderStarts()<<'\n';
        if(image.isNull())failure="timeout or empty frame: "+failure;
        return image;
    };
    auto oracle=[&](qint64 frame, QSize bounds = QSize(640,360)){
        QProcess process;
        process.start(ffmpeg,{"-v","error","-threads","2","-i",path,
            "-filter_threads","1","-vf",QString("select=eq(n\\,%1),scale=w='min(iw,%2)':h='min(ih,%3)':force_original_aspect_ratio=decrease").arg(frame).arg(bounds.width()).arg(bounds.height()),
            "-frames:v","1","-threads","1","-f","image2pipe","-c:v","ppm","pipe:1"});
        if(!process.waitForFinished(30000) || process.exitCode()!=0){failure="oracle failed";return QImage();}
        return QImage::fromData(process.readAllStandardOutput(),"PPM").convertToFormat(QImage::Format_RGB888);
    };
    for(qint64 frame : {qint64(12),qint64(13),qint64(3),count-1}) {
        const auto image=read(frame);
        const auto expected=oracle(frame);
        if(image!=expected)failure="frame pixels differ from independent decoder at "+QString::number(frame);
        if(!failure.isEmpty()){std::cerr<<failure.toStdString()<<'\n';return 1;}
        if(frame==12){
            const int starts=reader.decoderStarts();
            const auto next=read(13);
            if(next.isNull() || reader.decoderStarts()!=starts){std::cerr<<"adjacent frame restarted decoding\n";return 1;}
            const auto previous=read(11);
            if(previous!=oracle(11) || reader.decoderStarts()!=starts){std::cerr<<"backward cached frame failed\n";return 1;}
        }
    }
    reader.clear();reader.configure(ffmpeg,path,QSize(640,360),fps,count);
    const auto latest=read(18,true);
    if(latest!=oracle(18) || !failure.isEmpty()){std::cerr<<"rapid seek failed\n";return 1;}
    reader.configure(ffmpeg,path,QSize(160,90),fps,count);
    QElapsedTimer strip;strip.start();
    const int beforeStrip=reader.decoderStarts();
    for(int i=0;i<16;++i) {
        const qint64 frame=std::min(count-1,qint64(std::floor(std::min(double(count),fps)*i/16.)));
        if(read(frame).isNull() || !failure.isEmpty()){std::cerr<<"filmstrip failed\n";return 1;}
    }
    std::cout<<"filmstrip: 16/16 thumbnails, "<<strip.elapsed()<<" ms, "<<reader.decoderStarts()-beforeStrip<<" decoders\n";
    reader.configure(ffmpeg,path,QSize(8192,8192),fps,count);
    const auto detailed=read(12);
    if(detailed!=oracle(12,QSize(8192,8192)) || !failure.isEmpty()) {
        std::cerr<<"native zoom decode failed: "<<failure.toStdString()<<'\n';return 1;
    }
    const int zoomStarts=reader.decoderStarts();
    reader.configure(ffmpeg,path,QSize(640,360),fps,count);
    if(read(12)!=detailed || reader.decoderStarts()!=zoomStarts) {
        std::cerr<<"zoom-out discarded detailed frame\n";return 1;
    }
    std::cout<<"PASS: zoom retains native "<<detailed.width()<<'x'<<detailed.height()<<" pixels\n";
    std::cout<<"PASS: exact frames, adjacent cache, latest seek and filmstrip\n";
    return 0;
}
