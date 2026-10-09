#pragma once

#include "core/ProcessingOptions.h"
#include <QHash>
#include <QImage>
#include <QTemporaryDir>
#include <QWidget>
#include <memory>
#include <functional>

class PreviewFrameReader;
class ContinuousPreviewAudio;
class AudioTrack;
class TrimTimeline;
class QAudioOutput;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QMediaPlayer;
class QMenu;
class QProcess;
class QShortcut;
class QPushButton;
class QSlider;
class QStackedWidget;
class QVideoWidget;
class QVideoSink;
class QTimer;
class QToolButton;

class MediaPreviewWidget final : public QWidget {
    Q_OBJECT

public:
    explicit MediaPreviewWidget(QWidget* parent = nullptr);
    ~MediaPreviewWidget() override;

    std::function<QString()> projectDetails;
    void setMedia(const QString& path, qint64 initialPosition = 0);
    void setProjectMediaInfo(const QString& path, const QSize& size) { projectPath_=path; projectSize_=size; updateMediaDimensions(); }
    QString mediaPath() const { return mediaPath_; }
    QImage currentFrame() const;
    void setOrientation(const framescale::VideoOrientation& orientation);
    framescale::VideoOrientation orientation() const { return orientation_; }
    void showOrientationMenu(const QPoint& globalPosition);
    void addOrientationMenu(QMenu* parent);

    void clear();
    void showLoading();
    void invalidateLayerPreview(bool empty);
    void failLayerPreview(const QString& message);
    void setLayerVideoVisible(bool visible);
    void enableFileSelection();
    void setEmptyPromptVisible(bool visible);
    void pause();
    void seek(qint64 position) { seekTo(position); }
    void setPairedScrubbing(bool enabled) { pairedScrubbing_ = enabled; }
    void presentPairedFrame(const QImage& frame) { showImage(frame); }
    void play();
    qint64 positionMs() const;
    qint64 durationMs() const;
    double frameRate() const { return fps_; }
    bool isPlaying() const;
    bool isPlayable() const;
    void setCompact(bool compact);
    void useComparisonZoom();
    void setViewZoom(double value);
    void enableFilmstrip(qint64 start=0, qint64 length=0) {
        externalFilmstrip_=true;filmstripStart_=start;filmstripLength_=length;
        if(originalDuration_>0) startFilmstrip();
    }

    void setVolume(int percent);
    void setAudioOffset(int milliseconds);
    void setLayerEditing(bool enabled);
    void showLayerVolume(const QPoint& position);
    void setLayerEditorWidget(QWidget* editor);
    void setPlaybackSpeed(double speed);
    double playbackSpeed() const { return playbackSpeed_; }
    int audioOffset() const { return audioOffsetMs_; }
    void setAudioLinked(bool linked);
    bool audioLinked() const;
    void setVideoTimelineStart(int ms);
    int videoTimelineStart() const { return videoTimelineStartMs_; }
    void undoTimeline();
    void redoTimeline();
    void setComparisonAllowed(bool allowed);
    int volume() const { return volume_; }
    void setTrimRange(double startSeconds, double endSeconds);
    double trimStartSeconds() const;
    double trimEndSeconds() const;

signals:
    void fileSelectionRequested();
    void viewZoomChanged(double value);
    void thumbnailsChanged(const QVector<QImage>& images);
    void mediaReady();
    void videoMetadataReady();
    void scrubFrameReady(const QImage& frame, qint64 position);
    void comparisonRequested();
    void copyRequested();
    void previewError(const QString& message);
    void trimChanged(double startSeconds, double endSeconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void updateVolumeIcon();
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QMenu* createOrientationMenu(QMenu* parent);
    void prepareVideoPreview(bool forceProxy = false);
    void prepareSegment(qint64 position, bool prefetch = false);
    void freezeVideoFrame();
    void updateVideoGeometry();
    void seekTo(qint64 position);
    void stopProxy();
    void requestScrubFrame(qint64 position);
    void startScrubFrame();
    void startFilmstrip();
    void nextFilmstripFrame();
    void stopFrameReaders();
    PreviewFrameReader* filmstripReader_ = nullptr;
    bool filmstripPending_ = false;
    qint64 filmstripFrameIndex_ = -1;
    PreviewFrameReader* frameReader_ = nullptr;
    qint64 scrubFrameIndex_ = -1;
    qint64 filmstripStart_ = 0, filmstripLength_ = 0;
    QTimer* seekTimer_ = nullptr;
    qint64 requestedScrub_ = -1, completedScrub_ = -1, lastRequestedPosition_ = 0;
    QVector<QImage> filmstrip_;
    int filmstripIndex_ = 0, volume_ = 35;
    double fps_ = 25;
    QImage completedScrubImage_;
    bool pairedScrubbing_ = false;
    bool compact_ = false, comparisonZoom_ = false, externalFilmstrip_ = false;
    bool comparisonAllowed_ = true;
    QWidget* emptyPage_ = nullptr;
    QPushButton* comparisonButton_ = nullptr;
    QToolButton* volumeButton_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QWidget* dock_ = nullptr;
    QWidget* zoomBar_ = nullptr;
    QLabel* dimensionsLabel_ = nullptr;
    void updateMediaDimensions();
    void setZoom(double value);
    QComboBox* zoomChoice_ = nullptr;
    double zoom_ = 0, actualZoom_ = 1;
    QPointF pan_, dragPoint_;
    bool panning_ = false;
    bool spaceHeld_ = false;
    QShortcut* spaceShortcut_ = nullptr;
    bool segmented_ = false, pendingResume_ = false, seekAfterLoad_ = false;
    qint64 originalDuration_ = 0, segmentOffset_ = 0, pendingPosition_ = 0;
    quint64 seekSerial_ = 0;
    QProcess* proxy_ = nullptr;
    QProcess* prefetch_ = nullptr;
    std::shared_ptr<QTemporaryDir> activeProxy_;
    void loadVideo(const QString& playbackPath);
    void stopPlayer();
    void stopDecoder();
    void showImage(const QImage& image);
    void updateImageSize();
    void updatePlaybackControls();
    void updateTrimControls();
    void applyTrimEdit(bool startChanged);
    void decodeThumbnail(const QString& path, bool video, const QString& reason = { }, bool staticOnly = true);
    void videoFallback(const QString& reason);
    void thumbnailFailed(const QString& message, bool video, bool staticOnly);
    void showError(const QString& message);

    QStackedWidget* stack_ = nullptr;
    QLabel* imageLabel_ = nullptr;
    QLabel* placeholder_ = nullptr;
    QLabel* notice_ = nullptr;
    QVideoWidget* videoWidget_ = nullptr;
    QWidget* videoSurface_ = nullptr;
    QWidget* toolbar_ = nullptr;
    QWidget* trimBar_ = nullptr;
    TrimTimeline* timeline_ = nullptr;
    QDoubleSpinBox* trimStart_ = nullptr;
    QDoubleSpinBox* trimEnd_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QSlider* seekSlider_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;
    ContinuousPreviewAudio* continuousAudio_ = nullptr;
    bool sparseAudioTimestamps_ = false;
    bool layerEditing_ = false;
    struct TimelineEdit { int audio, video; bool linked; double start, end; };
    QVector<TimelineEdit> undoEdits_, redoEdits_;
    TimelineEdit editBefore_{};
    TimelineEdit timelineState() const;
    void beginTimelineEdit();
    void finishTimelineEdit();
    void restoreTimelineEdit(const TimelineEdit& state);
    void showTimelineMenu(const QPoint& position);
    void selectTimelineTrack(bool audio, bool additive = false);
    int videoTimelineStartMs_ = 0;
    double playbackSpeed_ = 1.;
    int audioOffsetMs_ = 0;
    AudioTrack* audioTrack_ = nullptr;
    QMediaPlayer* shiftedAudioPlayer_ = nullptr;
    QTimer* shiftedAudioTimer_ = nullptr;
    void configureShiftedAudio();
    void syncShiftedAudio(bool seek);
    QVideoSink* videoSink_ = nullptr;
    QProcess* decoder_ = nullptr;
    QHash<QString, std::shared_ptr<QTemporaryDir>> proxies_;
    framescale::VideoOrientation orientation_;
    QImage orientedFrame(const QImage& frame) const;
    QImage image_;
    QString projectPath_;
    QSize projectSize_;
    QLabel* layerBlank_=nullptr;
    bool layerPreviewPending_=false;
    QSize sourceSize_;
    QString mediaPath_;
    quint64 generation_ = 0;
    bool firstVideoFrame_ = false;
    bool fallingBack_ = false;
    bool scrubbing_ = false;
    bool resumeAfterSeek_ = false;
    bool playbackRequested_ = false;
    bool mediaLoaded_ = false;
    double trimStartSeconds_ = 0.0;
    double trimEndSeconds_ = 0.0;
};
