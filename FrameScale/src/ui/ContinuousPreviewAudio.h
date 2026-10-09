#pragma once
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QAudioSink>
#include <QMediaDevices>
#include <QTimer>
#include <QUrl>
#include <functional>
#include <algorithm>

// Read decoded samples at their native rate. Some HE-AAC files carry sparse
// packet timestamps; using those as the audio clock repeatedly drains the sink.
// Keep only one decoded buffer plus the device buffer, never a media-file copy.
class ContinuousPreviewAudio final : public QObject {
public:
    explicit ContinuousPreviewAudio(QObject* parent) : QObject(parent) {
        setObjectName("continuousPreviewAudio");
        format_ = QMediaDevices::defaultAudioOutput().preferredFormat();
        decoder_ = new QAudioDecoder(this);
        decoder_->setAudioFormat(format_);
        sink_ = new QAudioSink(format_, this);
        sink_->setBufferSize(format_.bytesForDuration(200000));
        timer_.setInterval(10);
        connect(&timer_, &QTimer::timeout, this, [this] { pump(); });
        connect(decoder_, &QAudioDecoder::bufferReady, this, [this] { pump(); });
        connect(decoder_, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), this,
            [this](QAudioDecoder::Error) { pause(); if (failed) failed(decoder_->errorString()); });
    }
    ~ContinuousPreviewAudio() override { timer_.stop(); decoder_->stop(); sink_->stop(); }
    std::function<void(const QString&)> failed;
    void setSource(const QString& source) { source_ = source; }
    void setPlaybackSpeed(double speed) {
        pause(); const float volume=sink_->volume(); sink_->stop(); delete sink_; output_=nullptr;
        QAudioFormat outputFormat=format_; outputFormat.setSampleRate(qRound(format_.sampleRate()*speed));
        sink_=new QAudioSink(outputFormat,this); sink_->setVolume(volume);
        sink_->setBufferSize(outputFormat.bytesForDuration(200000)); initialized_=false;
    }
    void setVolume(float volume) { sink_->setVolume(volume); }
    void seek(qint64 ms) {
        const bool resume = running_;
        pause(); decoder_->stop(); sink_->reset(); output_ = nullptr;
        pending_.clear(); skipped_ = 0;
        silenceFrames_ = std::max<qint64>(0, -ms) * format_.sampleRate() / 1000;
        skipFrames_ = std::max<qint64>(0, ms) * format_.sampleRate() / 1000;
        initialized_ = false;
        if (resume) play();
    }
    void play() {
        if (running_) return;
        running_ = true;
        if (!initialized_) {
            initialized_ = true;
            output_ = sink_->start();
            decoder_->setSource(QUrl::fromLocalFile(source_));
            decoder_->start();
        } else sink_->resume();
        timer_.start(); pump();
    }
    void pause() { running_ = false; timer_.stop(); sink_->suspend(); }
private:
    void pump() {
        if (!running_ || !output_) return;
        if (!pending_.isEmpty()) {
            const qint64 bytes = std::min<qint64>(pending_.size(), sink_->bytesFree());
            if (bytes > 0) {
                const qint64 written = output_->write(pending_.constData(), bytes);
                if (written > 0) pending_.remove(0, written);
            }
            if (!pending_.isEmpty()) return;
        }
        if (silenceFrames_ > 0) {
            const qint64 frames = std::min<qint64>(silenceFrames_, format_.sampleRate()/100);
            pending_.fill(0, frames * format_.bytesPerFrame()); silenceFrames_ -= frames; return;
        }
        if (!decoder_->bufferAvailable()) return;
        const QAudioBuffer buffer = decoder_->read();
        if (!buffer.isValid()) return;
        const qint64 skip = std::min<qint64>(buffer.frameCount(), std::max<qint64>(0, skipFrames_ - skipped_));
        skipped_ += skip;
        const qsizetype offset = skip * format_.bytesPerFrame();
        pending_ = QByteArray(buffer.constData<char>() + offset, buffer.byteCount() - offset);
    }
    QAudioDecoder* decoder_ = nullptr;
    QAudioSink* sink_ = nullptr;
    QIODevice* output_ = nullptr;
    QAudioFormat format_;
    QTimer timer_;
    QByteArray pending_;
    QString source_;
    qint64 skipFrames_ = 0, skipped_ = 0, silenceFrames_ = 0;
    bool running_ = false, initialized_ = false;
};
