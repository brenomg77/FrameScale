#pragma once
#include <QObject>
#include <QCache>
#include <QImage>
#include <QProcess>
#include <QSize>
#include <QTimer>

// Bounded frame cache and short sequential decode batches. No player or UI
// dependency: a seek returns as soon as its frame arrives on FFmpeg's pipe.
class PreviewFrameReader final : public QObject {
    Q_OBJECT
public:
    explicit PreviewFrameReader(QObject* parent=nullptr);
    ~PreviewFrameReader() override;
    void configure(const QString& ffmpeg, const QString& path, const QSize& bounds,
                   double fps, qint64 frameCount);
    void request(qint64 frame);
    void clear();
    bool busy() const { return process_ || startTimer_.isActive(); }
    int decoderStarts() const { return decoderStarts_; }
signals:
    void frameReady(qint64 frame, const QImage& image);
    void failed(const QString& message);
private:
    void stop();
    void start();
    void consume();
    QCache<qint64,QImage> cache_{128*1024}; // KiB; also holds one native 8K frame.
    QProcess* process_=nullptr;
    QTimer startTimer_;
    QString ffmpeg_,path_;
    QSize bounds_;
    double fps_=25;
    qint64 count_=0,requested_=-1,next_=-1,end_=-1,delivered_=-1;
    QByteArray bytes_;
    int decoderStarts_=0;
};
