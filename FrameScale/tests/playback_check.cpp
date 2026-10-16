#include "ui/MediaPreviewWidget.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QThread>
#include <QPushButton>
#include <QVideoSink>
#include <QVideoFrame>
#include <iostream>

// Real decoder check without showing, rendering or capturing a window.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (app.arguments().size() != 2) return 64;
    app.setOrganizationName("FrameScalePlaybackCheck");
    MediaPreviewWidget preview;
    preview.setCompact(true);
    QString failure;
    QObject::connect(&preview, &MediaPreviewWidget::previewError, &app,
        [&](const QString& reason) { failure = reason; });
    auto wait = [&](auto condition, int timeout) {
        QElapsedTimer timer; timer.start();
        while (!condition() && failure.isEmpty() && timer.elapsed() < timeout) {
            app.processEvents(); QThread::msleep(5);
        }
        return condition() && failure.isEmpty();
    };
    QElapsedTimer loading; loading.start();
    preview.setMedia(app.arguments()[1]);
    if (!wait([&] { return preview.findChild<QPushButton*>("previewPlayButton")->isEnabled(); }, 20000)) return 1;
    preview.play();
    if (!preview.isPlaying()) { std::cerr << "play request ignored during preparation\n"; return 1; }
    preview.pause();
    if (preview.isPlaying()) return 1;
    preview.play();
    if (!wait([&] { return preview.isPlayable(); }, 150000)) {
        std::cerr << "load failed: " << failure.toStdString() << '\n'; return 1;
    }
    std::cout << "loaded_ms=" << loading.elapsed() << " duration_ms=" << preview.durationMs() << std::endl;
    int frames = 0;
    if (auto* sink = preview.findChild<QVideoSink*>())
        QObject::connect(sink, &QVideoSink::videoFrameChanged, &app,
            [&](const QVideoFrame& frame) { if (frame.isValid()) ++frames; });
    if (!wait([&] { return preview.positionMs() > 4500; }, 150000)) {
        std::cerr << "playback stalled at " << preview.positionMs() << ": " << failure.toStdString() << '\n'; return 1;
    }
    if (frames < 3) { std::cerr << "playback clock advanced without decoded video frames\n"; return 1; }
    preview.pause();
    preview.seek(1000);
    if (!wait([&] { return preview.isPlayable(); }, 150000)) { std::cerr << "backward seek failed\n"; return 1; }
    preview.play();
    if (!wait([&] { return preview.positionMs() > 1600; }, 20000)) { std::cerr << "resume failed\n"; return 1; }
    preview.pause();
    std::cout << "PASS: playback crosses segment boundary and resumes after backward seek\n";
}
