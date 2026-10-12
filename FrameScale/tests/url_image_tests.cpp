#include "ui/VideoDownloadDialog.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>

// Functional network/file checks only. No windows shown or screenshots taken.
int main(int argc,char** argv) {
    QApplication app(argc,argv);
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
