#include "ui/VideoDownloadDialog.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <functional>

static bool waitFor(QApplication& app,const std::function<bool()>& ready,int milliseconds) {
    QElapsedTimer clock;clock.start();
    while(!ready() && clock.elapsed()<milliseconds) {
        app.processEvents();QThread::msleep(5);
    }
    return ready();
}

// Real bundled yt-dlp, a one-hour synthetic MP4, and a local HTTP transfer
// lasting longer than the dialog's 45-second discovery timeout. No external
// services or user media; only two tiny encoded frames are needed.
static int longVideoDownload(QApplication& app) {
    framescale::RuntimePaths paths;
    if(!QFileInfo::exists(paths.tool("yt-dlp")) || !QFileInfo::exists(paths.tool("ffmpeg"))) {
        std::cerr<<"Bundled download tools are unavailable\n";return 77;
    }
    QTemporaryDir destination;
    if(!destination.isValid())return 10;
    const QString fixture=QDir(destination.path()).filePath("one-hour.mp4");
    QProcess encoder;
    encoder.start(paths.tool("ffmpeg"),{"-v","error","-f","lavfi","-i",
        "color=c=blue:s=32x32:r=1/1800","-frames:v","2","-c:v","libx264",
        "-pix_fmt","yuv420p","-movflags","+faststart",fixture});
    if(!encoder.waitForFinished(15000) || encoder.exitCode()!=0) {
        std::cerr<<encoder.readAllStandardError().constData();return 11;
    }
    QFile file(fixture);
    if(!file.open(QIODevice::ReadOnly))return 12;
    QByteArray video=file.readAll();file.close();
    // A valid unused MP4 atom keeps the response open for several chunks.
    video+=QByteArray::fromHex("0002000866726565")+QByteArray(128*1024,'\0');
    QTcpServer server;
    if(!server.listen(QHostAddress::LocalHost))return 13;
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{
        while(server.hasPendingConnections()) {
            auto* socket=server.nextPendingConnection();
            auto request=std::make_shared<QByteArray>();
            auto sent=std::make_shared<bool>(false);
            QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket,request,sent,video]{
                *request+=socket->readAll();
                if(*sent || !request->contains("\r\n\r\n"))return;
                *sent=true;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: video/mp4\r\nContent-Length: "+
                    QByteArray::number(video.size())+"\r\nConnection: close\r\n\r\n");
                if(request->startsWith("HEAD ")){socket->disconnectFromHost();return;}
                socket->write(video.left(4096));
                // Keep sending so the socket inactivity timeout is never hit;
                // only the erroneous overall discovery limit would cancel it.
                auto* timer=new QTimer(socket);timer->setInterval(12000);
                auto offset=std::make_shared<qsizetype>(4096);
                QObject::connect(timer,&QTimer::timeout,socket,[socket,timer,offset,video]{
                    const auto chunk=video.mid(*offset,32768);
                    socket->write(chunk);*offset+=chunk.size();
                    if(*offset>=video.size()){timer->stop();socket->disconnectFromHost();}
                });
                timer->start();
            });
            QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }
    });
    VideoDownloadDialog dialog;
    const auto edits=dialog.findChildren<QLineEdit*>();
    edits[0]->setText(QString("http://127.0.0.1:%1/long.mp4").arg(server.serverPort()));
    edits[1]->setText(destination.path());
    QPushButton* download=nullptr;
    for(auto* button:dialog.findChildren<QPushButton*>())
        if(button->property("variant")=="primary")download=button;
    if(!download)return 14;
    QElapsedTimer elapsed;elapsed.start();download->click();
    if(!waitFor(app,[&]{return download->isEnabled();},75000) ||
       download->text()!=QString::fromUtf8("Editar agora")) {
        for(auto* label:dialog.findChildren<QLabel*>())std::cerr<<label->text().toStdString()<<'\n';
        return 15;
    }
    if(elapsed.elapsed()<45000)return 16;
    download->click();
    QFile result(dialog.fileToEdit());
    if(!result.open(QIODevice::ReadOnly) || result.readAll()!=video)return 17;
    std::cout<<"One-hour video download survived 45 seconds and preserved all bytes: OK ("
        <<elapsed.elapsed()<<" ms)\n";
    return 0;
}

// Functional network/file checks only. No windows shown or screenshots taken.
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    if(app.arguments().contains("--video"))return longVideoDownload(app);
    QTemporaryDir destination;
    QTcpServer server;
    if(!destination.isValid() || !server.listen(QHostAddress::LocalHost)) return 1;
    QByteArray png;QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);
    QImage image(4,4,QImage::Format_RGB32);image.fill(Qt::red);image.save(&buffer,"PNG");
    QObject::connect(&server,&QTcpServer::newConnection,&app,[&]{
        auto* socket=server.nextPendingConnection();
        QObject::connect(socket,&QTcpSocket::readyRead,socket,[socket,png]{
            const auto request=socket->readAll();
            if(request.startsWith("GET /redirect "))
                socket->write("HTTP/1.1 302 Found\r\nLocation: /image\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            else socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "+QByteArray::number(png.size())+"\r\nConnection: close\r\n\r\n"+png);
            socket->disconnectFromHost();
        });
        QObject::connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    for(const QString path:{QString("/image"),QString("/redirect")}) {
        VideoDownloadDialog dialog;
        const auto edits=dialog.findChildren<QLineEdit*>();
        edits[0]->setText(QString("http://127.0.0.1:%1%2").arg(server.serverPort()).arg(path));
        edits[1]->setText(destination.path());
        QPushButton* download=nullptr;
        for(auto* button:dialog.findChildren<QPushButton*>())
            if(button->property("variant")=="primary") download=button;
        if(!download) return 2;
        download->click();
        QElapsedTimer clock;clock.start();
        while(download->text()!=QString::fromUtf8("Editar agora") && clock.elapsed()<5000) {
            app.processEvents();QThread::msleep(5);
        }
        if(clock.elapsed()>=5000) {std::cerr<<"image download timed out\n";return 3;}
        download->click();
        const auto result=dialog.fileToEdit();
        if(QImage(result).size()!=QSize(4,4)) return 4;
    }
    if(QDir(destination.path()).entryList({"*.png"},QDir::Files).size()!=2) return 5;
    std::cout<<"Image URLs without extension, redirects and duplicate filename preservation: OK\n";
}
