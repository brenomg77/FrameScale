#include <QJsonArray>
#include "ComparisonDialog.h"
#include "Appearance.h"
#include "EnhancementWidget.h"
#include "MediaPreviewWidget.h"
#include "TrimTimeline.h"
#include "processing/ProcessingJob.h"
#include "processing/RuntimePaths.h"
#include "platform/TaskbarProgress.h"
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
struct CachedComparison {
    QByteArray key;
    QString output;
    std::shared_ptr<QTemporaryDir> directory;
};
QList<CachedComparison>& comparisonCache()
{
    static QList<CachedComparison> cache;
    static const auto cleanup = QObject::connect(qApp, &QObject::destroyed, [] { cache.clear(); });
    Q_UNUSED(cleanup);
    return cache;
}
std::shared_ptr<QTemporaryDir> comparisonDirectory()
{
    return std::shared_ptr<QTemporaryDir>(new QTemporaryDir, [](QTemporaryDir* directory) {
        const QString path = directory->path();
        directory->setAutoRemove(false);
        delete directory;
        if (!path.isEmpty()) {
            if (auto* pool = QThreadPool::globalInstance())
                pool->start([path] { QDir(path).removeRecursively(); });
            else
                QDir(path).removeRecursively(); // Qt may already be shut down at process exit.
        }
    });
}
std::filesystem::path path(const QString& value)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::u8path(value.toUtf8().constData());
#endif
}
QString text(const std::filesystem::path& value)
{
#ifdef Q_OS_WIN
    return QString::fromStdWString(value.wstring());
#else
    return QString::fromUtf8(value.u8string().c_str());
#endif
}
}

ComparisonDialog::ComparisonDialog(framescale::ProcessingOptions options, double position,
    double duration, double fps, QWidget* parent)
    : QDialog(parent)
    , options_(std::move(options))
    , temporary_(comparisonDirectory())
{
    Q_UNUSED(position);
    setObjectName("comparisonDialog");
    setWindowTitle(tr("Preview"));
    resize(1240, 760);
    setMinimumSize(760, 460);
    // Keep the native window surface transparent so RoundedWindowEffect can
    // provide the antialiased outer corners. Media panes remain opaque.
    setStyleSheet("QDialog#comparisonDialog { background:transparent; } QLabel#comparisonStatus { color:#d2d2d2; padding:4px 0; }");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    auto* splitter = new QSplitter(this);
    splitter->setHandleWidth(8);
    splitter->setStyleSheet("QSplitter::handle {background:transparent;}");
    splitter->setChildrenCollapsible(false);
    auto pane = [&](const QString& title, const char* name) {
        auto* widget = new QWidget(splitter);
        auto* column = new QVBoxLayout(widget);
        column->setContentsMargins(0, 0, 0, 0);
        auto* header = new QWidget(widget);
        header->setObjectName("comparisonHeader");
        header->setGraphicsEffect(new RoundedWindowEffect(header));
        header->setStyleSheet("QWidget#comparisonHeader {background:#242424;border-bottom:2px solid #1473e6;} QLabel {background:transparent;border:none;color:#ededed;}");
        auto* headerLayout = new QVBoxLayout(header);
        headerLayout->setContentsMargins(12, 8, 12, 8);
        headerLayout->setSpacing(3);
        auto* label = new QLabel(title, header);
        label->setStyleSheet("font-weight:600;font-size:13px;");
        headerLayout->addWidget(label);
        auto* filename = new QLabel(QFileInfo(text(options_.inputPath)).fileName(), header);
        filename->setTextFormat(Qt::PlainText);
        filename->setMinimumWidth(0);
        filename->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        filename->setToolTip(text(options_.inputPath));
        filename->setStyleSheet("color:#bcbcbc;font-size:11px;");
        headerLayout->addWidget(filename);
        column->addWidget(header);
        auto* preview = new MediaPreviewWidget(widget);
        preview->setObjectName(name);
        preview->setCompact(true);
        preview->useComparisonZoom();
        column->addWidget(preview, 1);
        splitter->addWidget(widget);
        return preview;
    };
    original_ = pane(tr("Original"), "comparisonOriginal");
    processed_ = pane(tr("Resultado"), "comparisonProcessed");
    processed_->setVolume(0);
    original_->setPairedScrubbing(true);
    processed_->setPairedScrubbing(true);
    connect(original_, &MediaPreviewWidget::scrubFrameReady, this, [this](const QImage& frame, qint64 position) {
        if (!playing_ && position == expectedOriginal_) { originalFrame_ = frame; presentPair(); }
    });
    connect(processed_, &MediaPreviewWidget::scrubFrameReady, this, [this](const QImage& frame, qint64 position) {
        if (!playing_ && position == expectedProcessed_) { processedFrame_ = frame; presentPair(); }
    });
    processed_->showLoading();
    splitter->setSizes({ 600, 600 });
    layout->addWidget(splitter, 1);
    status_ = new QLabel(tr("Carregando…"), this);
    status_->setWordWrap(true);
    status_->setObjectName("comparisonStatus");
    layout->addWidget(status_);
    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setMinimumHeight(26);
    progress_->setStyleSheet("QProgressBar {background:#292d34;color:white;border:1px solid #777;border-radius:6px;text-align:center;font-size:13px;font-weight:600;} QProgressBar::chunk {background:#0968cf;border-radius:5px;}");
    layout->addWidget(progress_);
    timeline_ = new TrimTimeline(this);
    timeline_->setObjectName("comparisonTimeline");
    timeline_->setSeekingOnly(true);
    timeline_->setEnabled(false);
    timeline_->setVisible(options_.mediaType == framescale::MediaType::Video);
    timeline_->seekRequested = [this](double seconds) { if(ready_) {stopPlayback(); seekBoth(qRound64(seconds*1000));} };
    timeline_->playbackRequested = [this] { togglePlayback(); };
    layout->addWidget(timeline_);
    connect(original_, &MediaPreviewWidget::thumbnailsChanged, this, [this](const QVector<QImage>& images) { timeline_->setThumbnails(images); });
    for (auto* player : {original_,processed_})
        connect(player,&MediaPreviewWidget::videoMetadataReady,this,[this] {
            if (ready_ && original_->durationMs()>0 && processed_->durationMs()>0) seekBoth(seekPosition_);
        });
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    space->setAutoRepeat(false);
    connect(space, &QShortcut::activated, this, &ComparisonDialog::togglePlayback);
    clock_ = new QTimer(this);
    clock_->setInterval(50);
    connect(clock_, &QTimer::timeout, this, [this] {
        if (!ready_)
            return;
        const bool available = original_->isPlayable() && processed_->isPlayable();
        timeline_->setEnabled(original_->durationMs()>0 && processed_->durationMs()>0);
        timeline_->setFrameRate(processed_->frameRate());
        const double rate = processed_->frameRate();
        const double cursor = playing_ ? processed_->positionMs() / 1000.
            : std::round(seekPosition_ * rate / 1000.) / rate;
        timeline_->setRange(comparisonLength() / 1000., 0, 0, cursor);
        if (!playing_)
            return;
        const auto position = std::max(qint64(0), qRound64((original_->positionMs() - sourceOffset_)/options_.playbackSpeed));
        const auto length = comparisonLength();
        // Large results use short display copies. Buffering the next copy is
        // not the end of the video: pause its peer and resume both together.
        if (!available) {
            if (!waitingForPlayers_) {
                // Resume at the new segment boundary, not just before it:
                // seeking backwards here would reload the previous segment.
                resumePosition_ = std::max(position, processed_->positionMs());
                waitingForPlayers_ = true;
                original_->pause();
                processed_->pause();
                seekBoth(resumePosition_);
            }
            return;
        }
        if (waitingForPlayers_) {
            seekBoth(resumePosition_);
            if (!original_->isPlayable() || !processed_->isPlayable()) return;
            waitingForPlayers_ = false;
            original_->play();
            processed_->play();
            return;
        }
        if (!original_->isPlaying() || position >= length - 45) {
            stopPlayback();
            seekBoth(0);
            return;
        }
        // Position notifications from the two players are asynchronous. Seeking
        // every tick for sub-frame drift repeatedly flushes the decoder.
        if (qAbs(position - processed_->positionMs()) > 250
            && (!syncCorrection_.isValid() || syncCorrection_.elapsed() >= 1000)) {
            processed_->seek(position);
            syncCorrection_.restart();
        }
    });
    clock_->start();
    connect(original_, &MediaPreviewWidget::previewError, this, &ComparisonDialog::fail);
    connect(processed_, &MediaPreviewWidget::previewError, this, &ComparisonDialog::fail);
    if (options_.mediaType == framescale::MediaType::Video) {
        fps = std::isfinite(fps) && fps > 0 ? fps : 25;
        sourceFps_ = fps;
        sourceDuration_ = duration;
        const double end = options_.trimEndSeconds > 0 ? std::min(duration, options_.trimEndSeconds) : duration;
        // Count first, then convert to time: subtraction of fractional frame
        // timestamps can make an exact one-frame selection look too short.
        const qint64 firstFrame = static_cast<qint64>(std::ceil(options_.trimStartSeconds * fps - 1e-9));
        const qint64 lastFrame = options_.trimEndSeconds > 0
            ? static_cast<qint64>(std::floor(end * fps + 1e-9)) : qRound64(end * fps);
        const qint64 frameCount = lastFrame - firstFrame;
        if (frameCount < 1) {
            QTimer::singleShot(0, this, [this] { fail(tr("O trecho selecionado é curto demais para a preview.")); });
            return;
        }
        // Render the complete selected range. Without a trim this is the full
        // source; the current playhead no longer silently limits it to 2 s.
        options_.trimStartSeconds = firstFrame / fps;
        options_.trimEndSeconds = options_.trimEndSeconds > 0 ? lastFrame / fps : duration;
    }
    sourceOffset_ = qRound64(options_.trimStartSeconds * 1000);
    clipLength_ = qRound64((options_.trimEndSeconds - options_.trimStartSeconds) * 1000);
    timeline_->setFrameRate(framescale::usesInterpolation(options_.operation) ? options_.targetFps : options_.reductionFps > 0 ? options_.reductionFps : sourceFps_);
    timeline_->setRange(std::max(.001, (options_.trimEndSeconds - options_.trimStartSeconds)), 0, 0, 0);
    // The reference is always decoded from the selected source, never a job output.
    original_->setMedia(text(options_.inputPath), sourceOffset_);
    original_->setTrimRange(options_.trimStartSeconds, options_.trimEndSeconds);
    original_->setAudioOffset(options_.audioOffsetMs);
    original_->setPlaybackSpeed(options_.playbackSpeed);
    // The strip represents time in the selected source. Upscaled 8K frames
    // add no navigation information and are much more expensive to decode.
    original_->enableFilmstrip(sourceOffset_,clipLength_);
    setProperty("comparisonSource", text(options_.inputPath));
    Appearance::apply(this);
    QTimer::singleShot(0, this, [this] { render(); });
}
ComparisonDialog::~ComparisonDialog()
{
    framescale::taskbar::clear(this);
    detachPreparation();
    detachJob();
    original_->clear();
    processed_->clear();
}
void ComparisonDialog::detachJob()
{
    if (!job_)
        return;
    auto* job = job_.release();
    job->disconnect(this);
    job->setParent(qApp);
    // Retain the directory until cancellation and all file handles are finished.
    const auto directory = temporary_;
    connect(job, &QObject::destroyed, qApp, [directory] { });
    if (job->isRunning()) {
        connect(job, &framescale::ProcessingJob::cancelled, job, &QObject::deleteLater);
        connect(job, &framescale::ProcessingJob::finished, job, &QObject::deleteLater);
        connect(job, &framescale::ProcessingJob::failed, job, &QObject::deleteLater);
        job->cancel();
    } else
        job->deleteLater();
}
void ComparisonDialog::render()
{
    if (closing_ || property("comparisonFailed").toBool())
        return;
    if (!temporary_->isValid()) {
        fail(tr("Não foi possível criar a prévia temporária."));
        return;
    }
    auto options = options_;
    if (options.mediaType == framescale::MediaType::Image) {
        options.outputSuffix = ".png";
    } else {
        // Comparison previews show processing even when the delivery is audio
        // or an image sequence. Use lossless H.264 for this temporary viewer.
        options.outputSuffix = ".mp4";
        options.videoCodec = "libx264";
        options.crf = 0;
        options.rateControl = "crf";
        options.encoderPreset = "ultrafast";
        options.pixelFormat = "yuv444p";
        options.videoProfile = options.videoLevel = "auto";
        options.keepAudio = false;
        options.bitRate = options.bufferSize = options.minRate = options.maxRate = 0;
        options.qmin = options.qmax = -1;
        options.customEncoderOptions.clear();
    }
    const QFileInfo source(text(options.inputPath));
    QJsonObject key { { "source", source.canonicalFilePath() }, { "size", QString::number(source.size()) }, { "modified", QString::number(source.lastModified().toMSecsSinceEpoch()) },
        { "operation", int(options.operation) }, { "engine", int(options.upscaleEngine) }, { "model", QString::fromStdString(options.upscaleModelId) },
        { "rife", QString::fromStdString(options.rifeModelId) }, { "denoise", int(options.denoise) }, { "scale", options.scaleFactor }, { "fps", options.targetFps }, { "reductionFps", options.reductionFps }, { "audioOffsetMs", options.audioOffsetMs }, { "playbackSpeed", options.playbackSpeed },
        { "sourceFps", sourceFps_ }, { "start", options.trimStartSeconds }, { "end", options.trimEndSeconds },
        { "rotation", options.orientation.quarterTurns }, { "horizontalFlip", options.orientation.horizontalFlip },
        { "verticalFlip", options.orientation.verticalFlip },
        { "enhancement", QJsonObject::fromVariantMap(enhancementValues(options.enhancement)) } };
    QJsonArray layerKeys;
    for(const auto& layer : options.layers) {
        const QFileInfo file(text(layer.path));
        layerKeys.append(QJsonObject{{"path",file.absoluteFilePath()}, {"modified",QString::number(file.lastModified().toMSecsSinceEpoch())},
            {"size",QString::number(file.size())},{"kind",int(layer.kind)},{"start",layer.start},{"in",layer.in},{"out",layer.out},{"speed",layer.speed},{"enabled",layer.enabled}});
    }
    key.insert("layers",layerKeys);
    key.insert("layered",options.layered);
    cacheKey_ = QCryptographicHash::hash(QJsonDocument(key).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
    for (const auto& cached : comparisonCache()) {
        if (cached.key == cacheKey_ && QFileInfo::exists(cached.output)) {
            temporary_ = cached.directory;
            setProperty("comparisonCacheHit", true);
            showResult(cached.output);
            return;
        }
    }
    const QString output = temporary_->filePath("processed" + QString::fromStdString(options.outputSuffix));
    options.outputPath = path(output);
    if (!options.layered && options.mediaType == framescale::MediaType::Video
        && (options.trimStartSeconds > 0 || options.trimEndSeconds < sourceDuration_ - 1. / sourceFps_)) {
        prepareClip(std::move(options));
        return;
    }
    if (options.mediaType == framescale::MediaType::Video)
        options.trimStartSeconds = options.trimEndSeconds = 0;
    startJob(std::move(options));
}
void ComparisonDialog::prepareClip(framescale::ProcessingOptions options)
{
    // A preview must not decode/count a long source from frame zero just to
    // process a trimmed range. Seek before decoding and stage only this
    // bounded range. FFV1 RGB keeps the same native-resolution pixels supplied
    // to the image-based AI pipeline, with no proxy scaling or lossy encoding.
    // AVI records the exact rational frame rate and count, avoiding a second
    // full-count probe and millisecond timestamp rounding for 23.976/29.97 FPS.
    const QString input = temporary_->filePath("source.avi");
    const qint64 frames = std::max(qint64(1), qRound64((options.trimEndSeconds - options.trimStartSeconds) * sourceFps_));
    auto* process = new QProcess(this);
    preparation_ = process;
    process->setProgram(framescale::RuntimePaths().tool("ffmpeg"));
    process->setArguments({ "-v", "error", "-y", "-threads", "2", "-ss", QString::number(options.trimStartSeconds, 'f', 9),
        "-i", text(options.inputPath), "-map", "0:v:0", "-an", "-sn", "-dn",
        "-filter_threads", "2", "-vf", "fps=" + QString::number(sourceFps_, 'f', 9) + ",setpts=PTS-STARTPTS",
        "-frames:v", QString::number(frames), "-c:v", "ffv1", "-level", "3", "-g", "1", "-pix_fmt", "bgr0", "-threads", "2", input });
    progress_->setRange(0, 0);
    framescale::taskbar::setBusy(this);
    status_->setText(tr("Carregando…"));
    status_->show();
    options.inputPath = path(input);
    options.trimStartSeconds = options.trimEndSeconds = 0;
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (preparation_ == process && error == QProcess::FailedToStart) {
            preparation_ = nullptr;
            process->deleteLater();
            fail(tr("Não foi possível preparar o trecho da preview."));
        }
    });
    connect(process, &QProcess::finished, this, [this, process, options = std::move(options)](int exitCode, QProcess::ExitStatus status) mutable {
        if (preparation_ != process)
            return;
        preparation_ = nullptr;
        const QString error = QString::fromLocal8Bit(process->readAllStandardError()).right(1600);
        process->deleteLater();
        if (closing_)
            return;
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(tr("Não foi possível preparar o trecho da preview.") + "\n" + error);
            return;
        }
        setProperty("comparisonPreparedSource", text(options.inputPath));
        startJob(std::move(options));
    });
    process->start();
}
void ComparisonDialog::detachPreparation()
{
    if (!preparation_)
        return;
    auto* process = preparation_;
    preparation_ = nullptr;
    process->disconnect(this);
    process->setParent(qApp);
    const auto directory = temporary_;
    connect(process, &QObject::destroyed, qApp, [directory] { });
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
        return;
    }
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    process->kill();
}
void ComparisonDialog::startJob(framescale::ProcessingOptions options)
{
    if (closing_)
        return;
    progress_->setRange(0, 100);
    framescale::taskbar::setProgress(this, 0);
    job_ = std::make_unique<framescale::ProcessingJob>(options, this);
    connect(job_.get(), &framescale::ProcessingJob::progressChanged, this, [this](int percent, const QString& stage) {
        progress_->setValue(percent);
        framescale::taskbar::setProgress(this, percent);
        status_->setText(stage);
    });
    connect(job_.get(), &framescale::ProcessingJob::failed, this, [this](const QString& message) {
        if (closing_)
            QDialog::reject();
        else
            fail(message);
    });
    connect(job_.get(), &framescale::ProcessingJob::cancelled, this, [this] { QDialog::reject(); });
    connect(job_.get(), &framescale::ProcessingJob::finished, this, [this](const QString& output) {
        if (closing_) {
            QDialog::reject();
            return;
        }
        auto& cache = comparisonCache();
        cache.append({ cacheKey_, output, temporary_ });
        while (cache.size() > 2)
            cache.removeFirst();
        showResult(output);
    });
    job_->start();
}
void ComparisonDialog::showResult(const QString& output)
{
    framescale::taskbar::clear(this);
    processedPath_ = output;
    ready_ = true;
    processed_->setMedia(output);
    seekBoth(0);
    progress_->hide();
    setProperty("comparisonReady", true);
    status_->clear();
    status_->hide();
}
void ComparisonDialog::togglePlayback()
{
    if (playing_) {
        stopPlayback();
        seekBoth(seekPosition_);
        return;
    }
    if (!ready_ || !original_->isPlayable() || !processed_->isPlayable())
        return;
    if ((original_->positionMs() - sourceOffset_)/options_.playbackSpeed >= comparisonLength() - 100)
        seekBoth(0);
    playing_ = true;
    original_->play();
    processed_->play();
}
qint64 ComparisonDialog::comparisonLength() const
{
    return std::max(qint64(0), std::min(qRound64(clipLength_/options_.playbackSpeed), processed_->durationMs()));
}
void ComparisonDialog::seekBoth(qint64 position)
{
    const auto relative = std::clamp(position, qint64(0), std::max(qint64(0), comparisonLength() - 1));
    seekPosition_ = relative;
    originalFrame_ = {}; processedFrame_ = {};
    auto clampFrame = [](MediaPreviewWidget* player, qint64 time) {
        return std::clamp(time, qint64(0), std::max(qint64(0), player->durationMs() - qRound64(1000. / player->frameRate())));
    };
    expectedOriginal_ = clampFrame(original_, sourceOffset_ + qRound64(relative*options_.playbackSpeed));
    expectedProcessed_ = clampFrame(processed_, relative);
    original_->seek(expectedOriginal_);
    processed_->seek(expectedProcessed_);
}
void ComparisonDialog::presentPair()
{
    if (originalFrame_.isNull() || processedFrame_.isNull()) return;
    original_->presentPairedFrame(originalFrame_);
    processed_->presentPairedFrame(processedFrame_);
}
void ComparisonDialog::stopPlayback()
{
    if (playing_) seekPosition_ = processed_->positionMs();
    playing_ = false;
    waitingForPlayers_ = false;
    original_->pause();
    processed_->pause();
}
void ComparisonDialog::fail(const QString& message)
{
    framescale::taskbar::clear(this);
    detachPreparation();
    detachJob();
    stopPlayback();
    ready_ = false;
    status_->setText(message);
    status_->show();
    progress_->hide();
    timeline_->setEnabled(false);
    setProperty("comparisonReady", false);
    setProperty("comparisonFailed", true);
}
void ComparisonDialog::reject()
{
    framescale::taskbar::clear(this);
    closing_ = true;
    stopPlayback();
    detachPreparation();
    detachJob();
    QDialog::reject();
}
void ComparisonDialog::closeEvent(QCloseEvent* event)
{
    reject();
    event->accept();
}
