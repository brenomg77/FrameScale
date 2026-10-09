#include "processing/LayerComposition.h"
#include "processing/ProcessingJob.h"

#include "core/ModelCatalog.h"
#include "processing/RuntimePaths.h"
#include "platform/ProcessingResources.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QThreadPool>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#endif

namespace framescale {
namespace {
QString shiftedAudioFilter(int milliseconds, double start, double duration)
{
    QString filter = "asetpts=PTS-STARTPTS";
    if (milliseconds > 0) filter += QString(",adelay=%1:all=1").arg(milliseconds);
    else if (milliseconds < 0) filter += QString(",atrim=start=%1,asetpts=PTS-STARTPTS").arg(-milliseconds/1000., 0, 'f', 6);
    return filter + QString(",apad,atrim=start=%1:duration=%2,asetpts=PTS-STARTPTS")
        .arg(start, 0, 'f', 9).arg(duration, 0, 'f', 9);
}


    QString frameRateArgument(const double fps)
    {
        // Nine-decimal rates can make FFmpeg choose a time base with a
        // near-INT_MAX numerator; H.264/MP4 then fails when muxing VFR sources.
        // A reduced rational with micro-FPS precision keeps that time base
        // bounded while exceeding the output contract's rate precision.
        const qint64 denominator = std::max<qint64>(1, std::min<qint64>(1000000,
            static_cast<qint64>((std::numeric_limits<int>::max() - 1) / std::max(1.0, fps))));
        const qint64 numerator = qRound64(fps * denominator);
        const qint64 divisor = std::gcd(numerator, denominator);
        return QStringLiteral("%1/%2").arg(numerator / divisor).arg(denominator / divisor);
    }

    std::shared_ptr<QTemporaryDir> createTemporaryWorkspace()
    {
        // A video can leave tens of thousands of PNGs. Releasing a job must never
        // remove them on the GUI thread. Verification/scans retain the workspace
        // until their reads are finished; only its last owner schedules removal.
        return { new QTemporaryDir(QDir::tempPath() + QStringLiteral("/FrameScale-XXXXXX")),
            [](QTemporaryDir* directory) {
                const QString path = directory->isValid() ? directory->path() : QString();
                directory->setAutoRemove(false);
                delete directory;
                if (!path.isEmpty()) {
                    QThreadPool::globalInstance()->start([path] { QDir(path).removeRecursively(); });
                }
            } };
    }

    QString fromPath(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return QString::fromStdWString(path.wstring());
#else
        return QString::fromUtf8(path.string().c_str());
#endif
    }

    double parseRate(const QString& rate)
    {
        const QStringList parts = rate.split(QLatin1Char('/'));
        if (parts.size() == 2) {
            bool numeratorOk = false;
            bool denominatorOk = false;
            const double numerator = parts[0].toDouble(&numeratorOk);
            const double denominator = parts[1].toDouble(&denominatorOk);
            if (numeratorOk && denominatorOk && denominator != 0.0) {
                return numerator / denominator;
            }
        }

        bool ok = false;
        const double value = rate.toDouble(&ok);
        return ok ? value : 0.0;
    }

    double jsonNumber(const QJsonObject& object, const QString& name)
    {
        const QJsonValue value = object.value(name);
        if (value.isDouble()) {
            return value.toDouble();
        }
        if (value.isString()) {
            bool ok = false;
            const double number = value.toString().toDouble(&ok);
            return ok ? number : 0.0;
        }
        return 0.0;
    }

    qint64 jsonInteger(const QJsonObject& object, const QString& name)
    {
        const QJsonValue value = object.value(name);
        if (value.isDouble()) {
            return static_cast<qint64>(value.toDouble());
        }
        if (value.isString()) {
            bool ok = false;
            const qint64 number = value.toString().toLongLong(&ok);
            return ok ? number : 0;
        }
        return 0;
    }

    int displayRotation(const QJsonObject& stream)
    {
        for (const QJsonValue& value : stream.value(QStringLiteral("side_data_list")).toArray()) {
            const QJsonObject sideData = value.toObject();
            if (sideData.contains(QStringLiteral("rotation"))) {
                return static_cast<int>(jsonInteger(
                    sideData, QStringLiteral("rotation")));
            }
        }
        return static_cast<int>(jsonInteger(
            stream.value(QStringLiteral("tags")).toObject(),
            QStringLiteral("rotate")));
    }

    bool rotationSwapsDimensions(const int rotation)
    {
        const int normalized = ((rotation % 360) + 360) % 360;
        return normalized == 90 || normalized == 270;
    }

    bool approximatelyEqual(const double left, const double right,
        const double tolerance)
    {
        return std::isfinite(left) && std::isfinite(right)
            && std::abs(left - right) <= tolerance;
    }

    QString denoiseArgument(const DenoiseLevel denoise)
    {
        return QString::number(static_cast<int>(denoise));
    }

    bool verifyFrames(const QString& directory, const qint64 expectedCount,
        const int expectedWidth, const int expectedHeight,
        const QString& engine, QString& message)
    {
        const QFileInfoList files = QDir(directory).entryInfoList(
            { QStringLiteral("*.png") }, QDir::Files, QDir::Name);
        if (files.size() != expectedCount) {
            message = QObject::tr("%1 produziu %2 fotogramas; eram esperados exatamente %3.")
                          .arg(engine)
                          .arg(files.size())
                          .arg(expectedCount);
            return false;
        }
        for (const QFileInfo& file : files) {
            if (file.size() <= 0) {
                message = QObject::tr("%1 produziu um fotograma vazio: %2")
                              .arg(engine, file.fileName());
                return false;
            }
            QImageReader reader(file.absoluteFilePath());
            const QSize dimensions = reader.size();
            const bool large = qint64(dimensions.width()) * dimensions.height() > 16000000;
            QFile png(reader.fileName());
            const bool complete = png.open(QIODevice::ReadOnly) && png.size() >= 12 && png.seek(png.size() - 12)
                && png.read(12) == QByteArray::fromHex("0000000049454e44ae426082");
            const QImage image = large ? QImage() : reader.read();
            if (large ? !complete : image.isNull()) {
                message = QObject::tr("%1 produziu um PNG inválido: %2")
                              .arg(engine, file.fileName());
                return false;
            }
            if (expectedWidth > 0 && expectedHeight > 0
                && dimensions != QSize(expectedWidth, expectedHeight)) {
                message = QObject::tr(
                    "%1 produziu %2 com %3×%4; eram esperados %5×%6.")
                              .arg(engine, file.fileName())
                              .arg(dimensions.width())
                              .arg(dimensions.height())
                              .arg(expectedWidth)
                              .arg(expectedHeight);
                return false;
            }
        }
        return true;
    }

    bool verifyImage(const QString& path, const int expectedWidth,
        const int expectedHeight, const QString& engine,
        QString& message)
    {
        const QFileInfo file(path);
        if (!file.exists() || !file.isFile() || file.size() <= 0) {
            message = QObject::tr("%1 não produziu a imagem esperada.").arg(engine);
            return false;
        }

        QImageReader reader(path);
        const QSize dimensions = reader.size();
        const bool large = qint64(dimensions.width()) * dimensions.height() > 16000000;
        QFile png(reader.fileName());
        const bool complete = png.open(QIODevice::ReadOnly) && png.size() >= 12 && png.seek(png.size() - 12)
            && png.read(12) == QByteArray::fromHex("0000000049454e44ae426082");
        const QImage image = large ? QImage() : reader.read();
        if (large ? !complete : image.isNull()) {
            message = QObject::tr("%1 produziu uma imagem inválida.").arg(engine);
            return false;
        }
        if (expectedWidth > 0 && expectedHeight > 0
            && dimensions != QSize(expectedWidth, expectedHeight)) {
            message = QObject::tr("%1 produziu uma imagem com %2×%3; eram esperados %4×%5.")
                          .arg(engine)
                          .arg(dimensions.width())
                          .arg(dimensions.height())
                          .arg(expectedWidth)
                          .arg(expectedHeight);
            return false;
        }
        return true;
    }

    QString imageCodecForPath(const QString& output)
    {
        const QString extension = QFileInfo(output).suffix().toLower();
        if (extension == QStringLiteral("jpg") || extension == QStringLiteral("jpeg")) {
            return QStringLiteral("mjpeg");
        }
        if (extension == QStringLiteral("webp")) {
            return QStringLiteral("libwebp");
        }
        if (extension == QStringLiteral("bmp")) {
            return QStringLiteral("bmp");
        }
        if (extension == QStringLiteral("png")) {
            return QStringLiteral("png");
        }
        return { };
    }

    QString modelBaseName(const UpscaleModel& model, const int scale)
    {
        if (model.id == "realesr-animevideov3") {
            return QStringLiteral("realesr-animevideov3-x%1").arg(scale);
        }
        if (model.id == "realesrgan-plus-anime-x4") {
            return QStringLiteral("realesrgan-x4plus-anime");
        }
        if (model.id == "realesrgan-plus-x4") {
            return QStringLiteral("realesrgan-x4plus");
        }
        return QFileInfo(fromPath(model.relativePath)).fileName();
    }

    bool reportsMp4AudioCodecIncompatibility(const QByteArray& output)
    {
        const QString text = QString::fromLocal8Bit(output).toLower();
        const bool mentionsMp4Muxer = text.contains(QStringLiteral("[mp4"))
            || text.contains(QStringLiteral("mp4 @"))
            || text.contains(QStringLiteral("output format mp4"));
        const bool reportsUnsupportedCodec = text.contains(QStringLiteral("codec not currently supported in container"))
            || text.contains(QStringLiteral("codec is not supported in container"))
            || text.contains(QStringLiteral("could not find tag for codec"))
            || (text.contains(QStringLiteral("tag"))
                && text.contains(QStringLiteral("incompatible with output codec")));
        return mentionsMp4Muxer && reportsUnsupportedCodec;
    }

} // namespace

ProcessingJob::ProcessingJob(ProcessingOptions options, QObject* parent)
    : ProcessingJob(std::move(options), RuntimePaths(), parent)
{
}

ProcessingJob::ProcessingJob(
    ProcessingOptions options,
    RuntimePaths runtimePaths,
    QObject* parent)
    : QObject(parent)
    , options_(std::move(options))
    , requestedOptions_(options_)
    , runtimePaths_(std::move(runtimePaths))
{
    cancellationTimer_.setSingleShot(true);
    cancellationTimer_.setInterval(2000);
    frameProgressTimer_.setInterval(80);
    verificationTimer_.setInterval(15);
    connect(&verificationTimer_, &QTimer::timeout, this, [this] {
        if (!verification_ || !verification_->complete.load(std::memory_order_acquire)) {
            return;
        }
        verificationTimer_.stop();
        const auto result = std::move(verification_);
        if (running_ && !cancelling_) {
            completeVerifiedStep(result->valid, result->error);
        }
    });
    connect(&frameProgressTimer_, &QTimer::timeout,
        this, &ProcessingJob::pollCompletedFrames);
    connect(&cancellationTimer_, &QTimer::timeout, this, [this] {
        if (cancelling_ && process_.state() != QProcess::NotRunning) {
            process_.kill();
        }
    });
    process_.setProcessChannelMode(QProcess::MergedChannels);
    connect(&process_, &QProcess::readyReadStandardOutput,
        this, &ProcessingJob::handleOutput);
    connect(&process_, &QProcess::readyReadStandardError,
        this, &ProcessingJob::handleOutput);
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this, &ProcessingJob::handleProcessFinished);
    connect(&process_, &QProcess::errorOccurred, this,
        [this](const QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && running_) {
                fail(tr("Não foi possível iniciar %1.\n%2")
                        .arg(process_.program(), process_.errorString()));
            }
        });
}

ProcessingJob::~ProcessingJob()
{
    verificationTimer_.stop();
    cancellationTimer_.stop();
    frameProgressTimer_.stop();
    running_ = false;
    cancelling_ = false;
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished(1500);
    }
    cleanupOutputArtifacts();
}

void ProcessingJob::start()
{
    if (running_) {
        return;
    }

    options_ = requestedOptions_;
    composingLayers_ = false;
    compositionDirectory_.reset();
    cancellationTimer_.stop();
    frameProgressTimer_.stop();
    verificationTimer_.stop();
    verification_.reset();
    frameScan_.reset();
    completedFrameFiles_.clear();
    currentFrameProgress_ = 0;
    cleanupOutputArtifacts();
    temporaryDirectory_.reset();
    running_ = false;
    cancelling_ = false;
    cancellationSignalled_ = false;
    aacRetryAttempted_ = false;
    steps_.clear();
    stepIndex_ = -1;
    completedWeight_ = 0;
    totalWeight_ = 1;
    outputBuffer_.clear();
    progressBuffer_.clear();
    outputTemporaryPath_.clear();
    commitToken_.clear();
    recoveryAttempted_ = false;
    savedPartialPath_.clear(); finalFramesDirectory_.clear(); recoveryEncodeArguments_.clear();
    directVideo_ = false; videoEncodeStep_ = -1;
    sourceFps_ = 0.0;
    durationSeconds_ = 0.0;
    sourceFrameCount_ = 0;
    trimStartFrame_ = 0;
    trimEndFrame_ = 0;
    effectiveTrimStartSeconds_ = 0.0;
    probingVideoTimeline_ = videoTimelineProbed_ = invalidVideoTimeline_ = false;
    timelineFrameCount_ = 0;
    timelineFirstTimestamp_ = timelineLastTimestamp_ = timelineLastDuration_ = 0.0;
    timelinePreviousInterval_ = timelineDurationSeconds_ = 0.0;
    sourceProbeMetadata_.clear();
    timelineBuffer_.clear();
    sourceWidth_ = 0;
    sourceHeight_ = 0;
    sourceAudioStreamCount_ = 0;
    sourceAudioCodecs_.clear();
    videoOutputContract_ = { };
    lastProgress_ = 0;

    const auto errors = validate(options_);
    if (!errors.empty()) {
        fail(QString::fromUtf8(errors.front().c_str()));
        return;
    }

    QString preflightError;
    if (!preflight(preflightError)) {
        fail(preflightError);
        return;
    }

    running_ = true;
    commitToken_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    outputTemporaryPath_ = outputTemporaryPath();
    QFile::remove(outputTemporaryPath_);
    emit progressChanged(0, tr("Verificando multimédia"));
    if (!running_ || cancelling_) {
        return;
    }
    emit frameProgressChanged(0, 0);
    if (!running_ || cancelling_) {
        return;
    }
    if (options_.layered) {
        compositionDirectory_ = createTemporaryWorkspace();
        if (!compositionDirectory_->isValid()) { fail(tr("Não foi possível preparar as camadas.")); return; }
        compositionPath_ = compositionDirectory_->filePath("composition.mkv");
        const auto plan = composeLayers(options_.layers, compositionPath_);
        if (!plan.error.isEmpty()) { fail(plan.error); return; }
        composingLayers_ = true;
        emit progressChanged(0, tr("Compondo camadas"));
        if (!running_ || cancelling_) return;
        process_.setProcessChannelMode(QProcess::MergedChannels);
        process_.start(toolPath("ffmpeg"), plan.arguments);
        return;
    }
    probeMedia();
}

bool ProcessingJob::setPaused(bool paused)
{
    if(paused_==paused) return true;
    if(!running_ || cancelling_ || process_.state()!=QProcess::Running) return false;
#ifdef _WIN32
    using Control = LONG (NTAPI*)(HANDLE);
    const auto module=GetModuleHandleW(L"ntdll.dll");
    const auto control=reinterpret_cast<Control>(GetProcAddress(module,paused ? "NtSuspendProcess" : "NtResumeProcess"));
    if(!control) return false;
    HANDLE process=OpenProcess(0x0800,FALSE,DWORD(process_.processId()));
    if(!process) return false;
    const bool success=control(process)>=0;
    CloseHandle(process);
    if(!success) return false;
#else
    if(::kill(pid_t(process_.processId()),paused ? SIGSTOP : SIGCONT)!=0) return false;
#endif
    paused_=paused;
    return true;
}

void ProcessingJob::cancel()
{
    if (!running_ || cancelling_) {
        return;
    }

    if(paused_) setPaused(false);
    cancelling_ = true;
    frameProgressTimer_.stop();
    if (process_.state() != QProcess::NotRunning) {
        process_.terminate();
        cancellationTimer_.start();
        return;
    }

    finishCancellation();
}

bool ProcessingJob::isRunning() const
{
    return running_;
}

QString ProcessingJob::toolPath(const QString& name) const
{
    return runtimePaths_.tool(name);
}

QString ProcessingJob::modelPath(const std::filesystem::path& relative) const
{
    return runtimePaths_.model(relative);
}

QString ProcessingJob::realEsrganModelPath(const UpscaleModel& model, double scaleFactor) const
{
    std::filesystem::path relative = model.relativePath;
    if (model.id == "realesr-animevideov3") {
        relative += "-x" + std::to_string(nativeScale(model, scaleFactor));
    }
    return runtimePaths_.ncnnModelBase(relative);
}

bool ProcessingJob::validateModelFiles(QString& error) const
{
    auto requirePair = [&error](const QString& base, const QString& name) {
        const QString param = base + QStringLiteral(".param");
        const QString bin = base + QStringLiteral(".bin");
        if (!QFileInfo::exists(param) || !QFileInfo::exists(bin)) {
            error = QObject::tr("Os ficheiros do modelo %1 estão incompletos.\nEsperado: %2 e %3")
                        .arg(name, param, bin);
            return false;
        }
        return true;
    };

    for (const auto& pass : upscalePasses(options_)) {
        const UpscaleModel* model = findUpscaleModel(pass.modelId);
        if (model == nullptr) {
            error = tr("Modelo de upscale não encontrado.");
            return false;
        }
        if (model->engine == UpscaleEngine::RealESRGAN) {
            const QString base = realEsrganModelPath(*model, pass.scaleFactor);
            if (!requirePair(base, QString::fromStdString(model->displayName))) {
                return false;
            }
        } else if (model->engine == UpscaleEngine::RealCUGAN) {
            const QString noise = denoiseArgument(pass.denoise);
            QString suffix;
            if (noise == QStringLiteral("0")) {
                suffix = QStringLiteral("no-denoise");
            } else if (noise == QStringLiteral("-1")) {
                suffix = QStringLiteral("conservative");
            } else {
                suffix = QStringLiteral("denoise%1x").arg(noise);
            }
            const QString base = QDir(modelPath(model->relativePath))
                                     .filePath(QStringLiteral("up%1x-%2")
                                             .arg(nativeScale(*model, pass.scaleFactor))
                                             .arg(suffix));
            if (!requirePair(base, QString::fromStdString(model->displayName))) {
                return false;
            }
        } else if (!QFileInfo::exists(modelPath(model->relativePath))) {
            error = tr("Shader Anime4K não encontrado: %1")
                        .arg(modelPath(model->relativePath));
            return false;
        }
    }

    if (!audioOnly(options_) && usesInterpolation(options_.operation)) {
        const RifeModel* model = findRifeModel(options_.rifeModelId);
        if (model == nullptr) {
            error = tr("Modelo RIFE não encontrado.");
            return false;
        }
        if (!requirePair(QDir(modelPath(model->relativePath)).filePath(QStringLiteral("flownet")),
                QString::fromStdString(model->displayName))) {
            return false;
        }
    }
    return true;
}

bool ProcessingJob::preflight(QString& error) const
{
    QStringList tools { QStringLiteral("ffprobe") };
    if (options_.mediaType == MediaType::Video || options_.mediaType == MediaType::Image) {
        tools.append(QStringLiteral("ffmpeg"));
    }
    for (const auto& pass : upscalePasses(options_)) {
        if (pass.engine == UpscaleEngine::RealESRGAN) {
            tools.append(QStringLiteral("realesrgan-ncnn-vulkan"));
        } else if (pass.engine == UpscaleEngine::RealCUGAN) {
            tools.append(QStringLiteral("realcugan-ncnn-vulkan"));
        } else {
            tools.append(QStringLiteral("mpv"));
        }
    }
    if (!audioOnly(options_) && usesInterpolation(options_.operation)) {
        tools.append(QStringLiteral("rife-ncnn-vulkan"));
    }

    tools.removeDuplicates();
    for (const QString& tool : tools) {
        const QString path = toolPath(tool);
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile()) {
            error = tr("Ferramenta necessária não encontrada: %1\nEsperado em runtime/bin.")
                        .arg(path);
            return false;
        }
    }
    if (imageSequence(options_) && !validateSequenceDestination(fromPath(options_.outputPath), error))
        return false;
    return validateModelFiles(error);
}

void ProcessingJob::probeMedia()
{
    process_.setProcessChannelMode(QProcess::MergedChannels);
    sourceAudioStreamCount_ = 0;
    sourceAudioCodecs_.clear();
    sourceFrameCount_ = 0;
    sourceWidth_ = sourceHeight_ = 0;
    sourceFps_ = durationSeconds_ = 0;
    process_.setProgram(toolPath(QStringLiteral("ffprobe")));
    QStringList arguments {
        QStringLiteral("-v"), QStringLiteral("error"),

        QStringLiteral("-show_streams"), QStringLiteral("-show_format"),
        QStringLiteral("-of"), QStringLiteral("json"),
        fromPath(options_.inputPath)
    };
    if (QFileInfo(fromPath(options_.inputPath)).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
        arguments.prepend(QStringLiteral("0"));
        arguments.prepend(QStringLiteral("-min_delay"));
    }
    process_.setArguments(arguments);
    outputBuffer_.clear();
    progressBuffer_.clear();
    process_.start();
}

void ProcessingJob::probeVideoTimeline()
{
    // Matroska often has no video duration/count in its stream header. The
    // container duration can instead describe a longer audio/subtitle stream.
    // Decode only video and consume timing records incrementally, retaining no
    // frame list in memory. This also handles VFR without assuming nominal FPS.
    sourceProbeMetadata_ = outputBuffer_;
    outputBuffer_.clear();
    timelineBuffer_.clear();
    probingVideoTimeline_ = true;
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    QStringList arguments { "-v", "error", "-select_streams", "v:0", "-show_frames",
        "-show_entries", "frame=best_effort_timestamp_time,duration_time,pkt_duration_time:frame_side_data=",
        "-of", "compact=p=0:nk=0", fromPath(options_.inputPath) };
    if (QFileInfo(fromPath(options_.inputPath)).suffix().compare("gif", Qt::CaseInsensitive) == 0)
        arguments = QStringList { "-min_delay", "0" } + arguments;
    process_.setArguments(arguments);
    process_.start();
}

void ProcessingJob::consumeVideoTimeline(const QByteArray& bytes)
{
    timelineBuffer_ += bytes;
    qsizetype consumed = 0;
    qsizetype newline;
    while ((newline = timelineBuffer_.indexOf('\n', consumed)) >= 0) {
        const QByteArray line = timelineBuffer_.mid(consumed, newline - consumed).trimmed();
        consumed = newline + 1;
        if (line.isEmpty())
            continue;
        bool timestampValid = false;
        double timestamp = 0.0, duration = 0.0;
        for (const QByteArray& field : line.split('|')) {
            const auto separator = field.indexOf('=');
            const QByteArray key = field.left(separator);
            const QByteArray value = field.mid(separator + 1);
            if (key == "best_effort_timestamp_time") {
                timestamp = value.toDouble(&timestampValid);
                timestampValid = timestampValid && std::isfinite(timestamp);
            } else if (key == "duration_time" || key == "pkt_duration_time") {
                bool valid = false;
                const double candidate = value.toDouble(&valid);
                if (valid && std::isfinite(candidate) && candidate > 0.0)
                    duration = candidate;
            }
        }
        if (!timestampValid) {
            invalidVideoTimeline_ = true;
            continue;
        }
        if (timelineFrameCount_ == 0) {
            timelineFirstTimestamp_ = timelineLastTimestamp_ = timestamp;
            timelineLastDuration_ = duration;
        } else {
            timelineFirstTimestamp_ = std::min(timelineFirstTimestamp_, timestamp);
            if (timestamp >= timelineLastTimestamp_) {
                if (timestamp > timelineLastTimestamp_)
                    timelinePreviousInterval_ = timestamp - timelineLastTimestamp_;
                timelineLastTimestamp_ = timestamp;
                timelineLastDuration_ = duration;
            }
        }
        ++timelineFrameCount_;
    }
    timelineBuffer_.remove(0, consumed);
    // A malformed tool response must not grow an unterminated record forever.
    if (timelineBuffer_.size() > 65536) {
        invalidVideoTimeline_ = true;
        timelineBuffer_.clear();
    }
}

bool ProcessingJob::prepareSteps(QString& error)
{
    temporaryDirectory_ = createTemporaryWorkspace();
    if (!temporaryDirectory_->isValid()) {
        error = tr("Não foi possível criar a pasta temporária.");
        return false;
    }

    steps_.clear();
    stepIndex_ = -1;
    completedWeight_ = 0;
    if (audioOnly(options_))
        return prepareAudioSteps(error);
    return options_.mediaType == MediaType::Video
        ? prepareVideoSteps(error)
        : prepareImageSteps(error);
}

bool ProcessingJob::prepareAudioSteps(QString& error)
{
    if (sourceAudioStreamCount_ == 0) {
        error = tr("O ficheiro selecionado não contém áudio.");
        return false;
    }
    const bool mp3 = options_.outputSuffix == ".mp3";
    const QString codec = mp3 ? "libmp3lame" : "pcm_s16le";
    const double audioOutputDuration=durationSeconds_/options_.playbackSpeed;
    QStringList args { "-y", "-i", fromPath(options_.inputPath), "-ss", QString::number(effectiveTrimStartSeconds_, 'f', 9), "-t", QString::number(audioOutputDuration, 'f', 9), "-map", "0:a:0", "-vn", "-map_chapters", "-1", "-map_metadata", options_.keepMetadata ? "0" : "-1", "-c:a", codec };
    if (options_.audioOffsetMs || options_.playbackSpeed != 1.) {
        const int ss = args.indexOf("-ss"); args.removeAt(ss); args.removeAt(ss);
        args << "-af" << shiftedAudioFilter(options_.audioOffsetMs, effectiveTrimStartSeconds_, durationSeconds_) + (options_.playbackSpeed==1. ? QString() : QString(",aresample=48000,asetrate=%1,aresample=48000").arg(qRound(48000*options_.playbackSpeed)));
    }
    if (mp3)
        args << "-b:a" << QString::number(options_.audioBitRate) << "-id3v2_version" << "3";
    if (options_.audioSampleRate)
        args << "-ar" << QString::number(options_.audioSampleRate);
    if (options_.audioChannels)
        args << "-ac" << QString::number(options_.audioChannels);
    args << "-metadata" << "crf=" << "-metadata" << "encoding_crf=" << "-metadata" << "encoding_codec=" + codec << "-f" << (mp3 ? "mp3" : "wav") << outputTemporaryPath_;
    steps_.push_back({ tr("Exportando áudio"), toolPath("ffmpeg"), args, 90, { } });
    steps_.push_back({ tr("Validando áudio"), toolPath("ffprobe"), { "-v", "error", "-show_streams", "-show_format", "-of", "json", outputTemporaryPath_ }, 5,
        [this, mp3](QString& message) {
            const auto object = QJsonDocument::fromJson(outputBuffer_).object();
            const auto streams = object.value("streams").toArray();
            const double duration = jsonNumber(object.value("format").toObject(), "duration");
            if (streams.size() != 1 || streams[0].toObject().value("codec_type").toString() != "audio" || streams[0].toObject().value("codec_name").toString() != (mp3 ? "mp3" : "pcm_s16le") || !std::isfinite(duration) || qAbs(duration - durationSeconds_/options_.playbackSpeed) > .15) {
                message = QObject::tr("O áudio exportado não corresponde ao formato ou duração esperados.");
                return false;
            }
            return true;
        } });
    steps_.push_back({ tr("Verificando decodificação completa"), toolPath("ffmpeg"), { "-v", "error", "-xerror", "-i", outputTemporaryPath_, "-map", "0:a:0", "-f", "null", "-" }, 5, { } });
    totalWeight_ = 100;
    return true;
}

bool ProcessingJob::appendUpscaleSteps(const QString& input, bool sequence,
    qint64 frameCount, QString& result, QString& error)
{
    result = input;
    int index = 0;
    for (const auto& pass : upscalePasses(options_)) {
        const UpscaleModel* model = findUpscaleModel(pass.modelId);
        if (!model) {
            error = tr("Modelo de upscale não encontrado.");
            return false;
        }
        const int scale = nativeScale(*model, pass.scaleFactor);
        const int width = scaledDimension(sourceWidth_, scale);
        const int height = scaledDimension(sourceHeight_, scale);
        const QString name = QStringLiteral("ai-%1").arg(index++);
        const QString output = sequence ? createFramesDirectory(name)
                                        : temporaryDirectory_->filePath(name + ".png");
        QString program;
        QStringList arguments;
        if (pass.engine == UpscaleEngine::RealESRGAN || pass.engine == UpscaleEngine::RealCUGAN) {
            const bool esrgan = pass.engine == UpscaleEngine::RealESRGAN;
            program = toolPath(esrgan ? "realesrgan-ncnn-vulkan" : "realcugan-ncnn-vulkan");
            arguments = { "-i", result, "-o", output,
                "-t", processingTileSize(), "-j", sequence ? "2:1:2" : "1:1:1",
                "-s", QString::number(scale),
                "-n", esrgan ? modelBaseName(*model, scale) : denoiseArgument(pass.denoise),
                "-m", esrgan ? QFileInfo(realEsrganModelPath(*model, pass.scaleFactor)).absolutePath()
                              : modelPath(model->relativePath),
                "-f", "png" };
        } else {
            program = toolPath("mpv");
            arguments = { "--no-config", sequence ? "mf://" + QDir(result).filePath("*.png") : result,
                "--gpu-api=vulkan", QString("--vf=gpu=w=%1:h=%2").arg(width).arg(height),
                "--glsl-shader=" + modelPath(model->relativePath),
                "--o=" + (sequence ? QDir(output).filePath("%08d.png") : output),
                "--of=image2", "--ovc=png", "--no-audio" };
            if (sequence)
                arguments << QString("--mf-fps=%1").arg(options_.reductionFps ? sourceFrameCount_ / durationSeconds_ : videoOutputContract_.fps, 0, 'f', 9);
            else
                arguments << "--frames=1";
        }
        const QString engine = QString::fromStdString(model->displayName);
        const QString title = pass.restoreOriginalSize
            ? tr("Restaurando com %1").arg(engine) : tr("Aplicando %1").arg(engine);
        steps_.push_back({ title, program, arguments, sequence ? 45 : 90,
            [output, sequence, frameCount, width, height, engine](QString& message) {
                return sequence ? verifyFrames(output, frameCount, width, height, engine, message)
                                : verifyImage(output, width, height, engine, message);
            }, false, frameCount, sequence ? output : QString(), true });
        result = output;
        if (pass.restoreOriginalSize) {
            // Restoration networks have a native 2x/4x output. Bring this pass
            // back to source dimensions before the independently selected
            // enlargement; its scale must never multiply the user's scale.
            const QString normalized = sequence ? createFramesDirectory(name + "-restored")
                : temporaryDirectory_->filePath(name + "-restored.png");
            QStringList resizeArguments { "-y", "-filter_threads", processingFilterThreads() };
            if (sequence)
                resizeArguments << "-framerate" << frameRateArgument(options_.reductionFps ? sourceFrameCount_ / durationSeconds_ : videoOutputContract_.fps);
            resizeArguments << "-i" << (sequence ? QDir(result).filePath("%08d.png") : result)
                << "-vf" << QString("scale=%1:%2:flags=lanczos,format=rgb24").arg(sourceWidth_).arg(sourceHeight_)
                << "-frames:v" << QString::number(frameCount) << "-compression_level" << "1"
                << (sequence ? QDir(normalized).filePath("%08d.png") : normalized);
            steps_.push_back({ tr("Preparando imagem restaurada"), toolPath("ffmpeg"), resizeArguments, 5,
                [normalized, sequence, frameCount, width = sourceWidth_, height = sourceHeight_](QString& message) {
                    return sequence ? verifyFrames(normalized, frameCount, width, height, QObject::tr("FFmpeg"), message)
                                    : verifyImage(normalized, width, height, QObject::tr("FFmpeg"), message);
                }, false, frameCount, sequence ? normalized : QString(), true });
            result = normalized;
        }
    }
    return true;
}

bool ProcessingJob::prepareImageSteps(QString& error)
{
    if (!audioOnly(options_) && usesInterpolation(options_.operation)) {
        error = tr("A interpolação não pode ser aplicada a uma imagem isolada.");
        return false;
    }

    QString input = fromPath(options_.inputPath);
    const auto cleanup = QString::fromStdString(enhancementCleanupFilter(options_.enhancement, false));
    if (!cleanup.isEmpty()) {
        const QString cleaned = temporaryDirectory_->filePath("cleaned.png");
        steps_.push_back({tr("Aplicando melhorias"), toolPath("ffmpeg"),
            {"-y","-filter_threads",processingFilterThreads(),"-i",input,"-vf",cleanup,"-frames:v","1",cleaned}, 10,
            [cleaned,width=sourceWidth_,height=sourceHeight_](QString& message){return verifyImage(cleaned,width,height,QObject::tr("FFmpeg"),message);}});
        input=cleaned;
    }
    QString result;
    if (!appendUpscaleSteps(input, false, 1, result, error))
        return false;
    const int expectedWidth = scaledDimension(sourceWidth_, usesUpscale(options_.operation) ? options_.scaleFactor : 1);
    const int expectedHeight = scaledDimension(sourceHeight_, usesUpscale(options_.operation) ? options_.scaleFactor : 1);

    const QString partialOutput = outputTemporaryPath_;
    const QString extension = QFileInfo(fromPath(options_.outputPath)).suffix().toLower();
    const bool preserveAlpha = extension == "png" || extension == "webp" || extension == "tif" || extension == "tiff" || extension == "bmp";
    QStringList imageEncodeArguments { QStringLiteral("-y"), QStringLiteral("-i"), result };
    if (options_.keepMetadata || preserveAlpha)
        imageEncodeArguments.append({ QStringLiteral("-i"), fromPath(options_.inputPath) });
    if (options_.keepMetadata) {
        imageEncodeArguments.append({ QStringLiteral("-map_metadata"), QStringLiteral("1"),
            QStringLiteral("-map_metadata:s:v:0"), QStringLiteral("1:s:v:0") });
    } else {
        imageEncodeArguments.append({ QStringLiteral("-map_metadata"), QStringLiteral("-1") });
    }
    const QString scale = QStringLiteral("scale=%1:%2:flags=lanczos").arg(expectedWidth).arg(expectedHeight);
    const QString finish = QString::fromStdString(enhancementFinishFilter(options_.enhancement));
    const QString colorFilter = scale + (finish.isEmpty() ? QString() : "," + finish);
    if (preserveAlpha) {
        // AI engines may discard alpha. Restore the original coverage, resized
        // to the final dimensions, independently of processed RGB and metadata.
        const QString filter = "[0:v]" + colorFilter + ",format=rgb24[color];[1:v]format=rgba,alphaextract,"
            + scale + "[alpha];[color][alpha]alphamerge[out]";
        imageEncodeArguments.append({ "-filter_complex_threads", processingFilterThreads(), "-filter_complex", filter,
            "-map", "[out]" });
    } else {
        imageEncodeArguments.append({ "-map", "0:v:0", "-vf", colorFilter });
    }
    imageEncodeArguments.append({
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-c:v"), imageCodecForPath(fromPath(options_.outputPath)),
        QStringLiteral("-f"), QStringLiteral("image2"), partialOutput });
    steps_.push_back({ tr("Gravando resultado"),
        toolPath(QStringLiteral("ffmpeg")), imageEncodeArguments,
        5, [partialOutput](QString& message) {
            const QFileInfo file(partialOutput);
            if (!file.exists() || !file.isFile() || file.size() <= 0) {
                message = QObject::tr("O FFmpeg não produziu a imagem final.");
                return false;
            }
            return true;
        } });

    const QString validatedImage = temporaryDirectory_->filePath(
        QStringLiteral("validated.png"));
    steps_.push_back({ tr("Validando imagem codificada"),
        toolPath(QStringLiteral("ffmpeg")),
        { QStringLiteral("-y"), QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-xerror"), QStringLiteral("-err_detect"),
            QStringLiteral("explode"), QStringLiteral("-i"), partialOutput,
            QStringLiteral("-map"), QStringLiteral("0:v:0"),
            QStringLiteral("-frames:v"), QStringLiteral("1"),
            QStringLiteral("-c:v"), QStringLiteral("png"),
            QStringLiteral("-f"), QStringLiteral("image2"), validatedImage },
        5, [validatedImage, expectedWidth, expectedHeight](QString& message) {
            if (!verifyImage(validatedImage, expectedWidth, expectedHeight,
                    QObject::tr("FFmpeg"), message)) {
                return false;
            }
            return true;
        } });
    // Inspect process diagnostics on the owning thread before dispatching the
    // independent file check; workers can outlive cancellation/job destruction.
    steps_.back().rejectDiagnostics = true;

    for (Step& step : steps_) {
        if (options_.enhancement.enabled && step.program == toolPath("ffmpeg"))
            step.arguments = QStringList{"-filter_threads",processingFilterThreads(),"-filter_complex_threads",processingFilterThreads()} + step.arguments;
        step.progressFrameCount = 1;
    }
    for (Step& step : steps_)
        step.verifyInBackground = bool(step.verify);
    totalWeight_ = 0;
    for (const auto& step : steps_)
        totalWeight_ += step.weight;
    return true;
}

QString ProcessingJob::createFramesDirectory(const QString& name)
{
    const QString path = temporaryDirectory_->filePath(name);
    QDir().mkpath(path);
    return path;
}

bool ProcessingJob::prepareVideoSteps(QString& error)
{
    const bool gifOutput = QFileInfo(fromPath(options_.outputPath)).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0;
    const bool trimmed = trimStartFrame_ > 0 || options_.trimEndSeconds > 0.0;
    const double sourcePlaybackFps = sourceFrameCount_ / durationSeconds_;
    const bool reduce = options_.reductionFps > 0;
    const int outputFps = reduce ? options_.reductionFps : options_.targetFps;
    const QString cadenceFilter = reduce ? QString(",fps=%1:round=up").arg(outputFps) : QString();
    const qint64 finalFrameCount = (usesInterpolation(options_.operation) || reduce)
        ? std::max<qint64>(reduce ? 1 : 2, static_cast<qint64>(std::ceil(durationSeconds_ * outputFps - 1e-9)))
        : sourceFrameCount_;
    const double finalFps = (usesInterpolation(options_.operation) || reduce)
        ? outputFps
        : sourcePlaybackFps;
    videoOutputContract_.frameCount = std::max<qint64>(1, static_cast<qint64>(std::ceil(finalFrameCount / options_.playbackSpeed - 1e-9)));
    videoOutputContract_.width = usesUpscale(options_.operation)
        ? scaledDimension(sourceWidth_, options_.scaleFactor, !imageSequence(options_) && options_.pixelFormat != "yuv444p")
        : sourceWidth_;
    videoOutputContract_.height = usesUpscale(options_.operation)
        ? scaledDimension(sourceHeight_, options_.scaleFactor, !imageSequence(options_) && options_.pixelFormat != "yuv444p")
        : sourceHeight_;
    const int resizeWidth = videoOutputContract_.width;
    const int resizeHeight = videoOutputContract_.height;
    if (options_.orientation.quarterTurns % 2 != 0)
        std::swap(videoOutputContract_.width, videoOutputContract_.height);
    videoOutputContract_.fps = finalFps;
    videoOutputContract_.durationSeconds = videoOutputContract_.frameCount / finalFps;
    videoOutputContract_.audioStreamCount = options_.keepAudio && !gifOutput && !imageSequence(options_) ? sourceAudioStreamCount_ : 0;
    videoOutputContract_.audioCodecs = videoOutputContract_.audioStreamCount > 0 ? sourceAudioCodecs_ : QStringList();
    QString audioEncoder = QString::fromStdString(options_.audioCodec);
    const QString container = QFileInfo(fromPath(options_.outputPath)).suffix().toLower();
    const bool avi = container == "avi";
    // Keep the original packets only for audio formats in our delivery profile.
    // PCM/Vorbis from MKV must not silently produce an unsuitable MP4/MOV.
    if (audioEncoder == "copy" && (container == "mp4" || container == "mov")) {
        const QStringList compatible { "aac", "mp3", "ac3", "eac3", "alac" };
        for (const auto& codec : sourceAudioCodecs_) {
            if (!compatible.contains(codec) && !(container == "mov" && codec.startsWith("pcm_"))) {
                audioEncoder = "aac";
                break;
            }
        }
    }
    if (avi && audioEncoder == "copy")
        audioEncoder = "pcm_s16le";
    if (audioEncoder == "copy" && (trimmed || options_.audioSampleRate || options_.audioChannels || options_.audioOffsetMs || options_.playbackSpeed != 1.))
        audioEncoder = "aac";
    if (audioEncoder != "copy") {
        // Re-encoding audio avoids packet-boundary offsets at the cut.
        for (QString& codec : videoOutputContract_.audioCodecs) {
            codec = audioEncoder == "libmp3lame" ? "mp3" : audioEncoder;
        }
    }

    const QString inputFrames = createFramesDirectory(QStringLiteral("input"));
    const QString interpolatedFrames = createFramesDirectory(QStringLiteral("interpolated"));
    const QString inputPattern = QDir(inputFrames).filePath(QStringLiteral("%08d.png"));
    const QString sourceRate = frameRateArgument(sourcePlaybackFps);
    QString extractFilter = QStringLiteral("setpts=PTS-STARTPTS,fps=%1").arg(sourceRate);
    if (trimmed) {
        extractFilter += QStringLiteral(",trim=start_frame=%1:end_frame=%2,setpts=PTS-STARTPTS")
                             .arg(trimStartFrame_)
                             .arg(trimEndFrame_);
    }

    const auto deinterlace = QString::fromStdString(enhancementDeinterlaceFilter(options_.enhancement));
    if (!deinterlace.isEmpty()) extractFilter.prepend(deinterlace+",");
    const auto cleanup = QString::fromStdString(enhancementCleanupFilter(options_.enhancement,true));
    if (!cleanup.isEmpty()) extractFilter += ","+cleanup;
    QString finish = QString::fromStdString(enhancementFinishFilter(options_.enhancement));
    const auto orientation = QString::fromStdString(orientationFilter(options_.orientation));
    if (!orientation.isEmpty()) finish += (finish.isEmpty() ? QString() : ",") + orientation;
    // RIFE's Windows image loader wraps decoded bytes as three-channel RGB.
    // GIF/alpha sources otherwise produce RGBA PNGs and corrupt row/channel
    // alignment. Normalize at the engine boundary, regardless of container.
    if (!audioOnly(options_) && usesInterpolation(options_.operation))
        extractFilter += QStringLiteral(",format=rgb24");

    steps_.push_back({ tr("Preparando vídeo"),
        toolPath(QStringLiteral("ffmpeg")),
        { QStringLiteral("-y"),
            QStringLiteral("-i"), fromPath(options_.inputPath),
            QStringLiteral("-map"), QStringLiteral("0:v:0"),
            QStringLiteral("-vf"),
            extractFilter,
            QStringLiteral("-frames:v"), QString::number(sourceFrameCount_),
            QStringLiteral("-compression_level"), QStringLiteral("1"), inputPattern },
        15,
        [inputFrames, expected = sourceFrameCount_, width = sourceWidth_,
            height = sourceHeight_](QString& message) {
            return verifyFrames(inputFrames, expected, width, height,
                QObject::tr("O FFmpeg"), message);
        },
        false, sourceFrameCount_, inputFrames });

    QString currentFrames = inputFrames;
    if (!audioOnly(options_) && usesInterpolation(options_.operation)) {
        const RifeModel* rife = findRifeModel(options_.rifeModelId);
        if (rife == nullptr) {
            error = tr("Modelo RIFE não encontrado.");
            return false;
        }
        const qint64 desiredFrames = finalFrameCount;
        steps_.push_back({ tr("Interpolando com RIFE"),
            toolPath(QStringLiteral("rife-ncnn-vulkan")),
            { QStringLiteral("-i"), inputFrames, QStringLiteral("-o"), interpolatedFrames,
                QStringLiteral("-j"), processingRifeThreads(sourceWidth_, sourceHeight_),
                QStringLiteral("-n"), QString::number(desiredFrames),
                QStringLiteral("-m"), modelPath(rife->relativePath),
                QStringLiteral("-f"), QStringLiteral("%08d.png") },
            35,
            [interpolatedFrames, desiredFrames, width = sourceWidth_,
                height = sourceHeight_](QString& message) {
                return verifyFrames(interpolatedFrames, desiredFrames,
                    width, height, QObject::tr("O RIFE"), message);
            },
            false, desiredFrames, interpolatedFrames });
        currentFrames = interpolatedFrames;
    }

    QString processedFrames;
    if (!appendUpscaleSteps(currentFrames, true, reduce ? sourceFrameCount_ : finalFrameCount, processedFrames, error))
        return false;
    currentFrames = processedFrames;

    const bool direct = !needsUpscale(options_) && !usesInterpolation(options_.operation);
    if (direct)
        steps_.clear();
    const QString partialOutput = outputTemporaryPath_;
    const QString pattern = QDir(currentFrames).filePath(QStringLiteral("%08d.png"));
    if (imageSequence(options_)) {
        if (!QDir().mkdir(partialOutput)) {
            error = tr("Não foi possível criar a pasta da sequência.");
            return false;
        }
        const bool png = options_.outputSuffix == ".png";
        const QString framePattern = QDir(partialOutput).filePath(png ? "frame_%08d.png" : "frame_%08d.jpg");
        QStringList args { "-y" };
        if (!direct)
            args << "-framerate" << frameRateArgument(reduce ? sourcePlaybackFps : finalFps);
        args << "-i" << (direct ? fromPath(options_.inputPath) : pattern);
        if (options_.keepMetadata && !direct)
            args << "-i" << fromPath(options_.inputPath) << "-map_metadata" << "1";
        else
            args << "-map_metadata" << (options_.keepMetadata ? "0" : "-1");
        args << "-map" << "0:v:0";
        const QString resize = QString("scale=%1:%2:flags=lanczos").arg(resizeWidth).arg(resizeHeight);
        args << "-vf" << ((direct ? extractFilter + "," + resize : resize) + (finish.isEmpty() ? QString() : ","+finish) + cadenceFilter)
             << "-frames:v" << QString::number(finalFrameCount) << "-an" << "-c:v" << (png ? "png" : "mjpeg");
        if (png)
            args << "-compression_level" << "3" << "-pix_fmt" << "rgb24";
        else
            args << "-q:v" << "2" << "-pix_fmt" << "yuvj444p";
        args << "-start_number" << "1" << "-f" << "image2" << framePattern;
        const int width = videoOutputContract_.width, height = videoOutputContract_.height;
        steps_.push_back({ tr("Gravando sequência de imagens"), toolPath("ffmpeg"), args, 20,
            [partialOutput, finalFrameCount, width, height, png](QString& message) {
                const auto files = QDir(partialOutput).entryList({ png ? "*.png" : "*.jpg" }, QDir::Files, QDir::Name);
                if (files.size() != finalFrameCount) {
                    message = QObject::tr("A sequência não contém todos os fotogramas esperados.");
                    return false;
                }
                for (int i = 0; i < files.size(); ++i) {
                    const QString expected = QString("frame_%1.").arg(i + 1, 8, 10, QChar('0')) + (png ? "png" : "jpg");
                    QImageReader reader(QDir(partialOutput).filePath(files[i]));
                    if (files[i] != expected || reader.size() != QSize(width, height) || reader.read().isNull()) {
                        message = QObject::tr("Foi encontrado um fotograma inválido na sequência.");
                        return false;
                    }
                }
                return true;
            },
            false, finalFrameCount, { }, true });
        totalWeight_ = 0;
        for (auto& step : steps_) {
            if (options_.enhancement.enabled && step.program == toolPath("ffmpeg"))
                step.arguments = QStringList{"-filter_threads",processingFilterThreads(),"-filter_complex_threads",processingFilterThreads()} + step.arguments;
            totalWeight_ += step.weight;
            if (!step.progressFramesDirectory.isEmpty())
                step.verifyInBackground = true;
        }
        return true;
    }
    QStringList encodeArguments {
        QStringLiteral("-y"), QStringLiteral("-framerate"),
        frameRateArgument(reduce ? sourcePlaybackFps : videoOutputContract_.fps),
        QStringLiteral("-i"), pattern
    };
    if (effectiveTrimStartSeconds_ > 0.0 && !options_.audioOffsetMs) {
        encodeArguments.append({ QStringLiteral("-ss"),
            QString::number(effectiveTrimStartSeconds_, 'f', 9) });
    }
    encodeArguments.append({ QStringLiteral("-i"), fromPath(options_.inputPath),
        QStringLiteral("-map"), gifOutput ? QStringLiteral("[gif]") : QStringLiteral("0:v:0") });
    QString scaleFilter;
    if (needsUpscale(options_) || resizeWidth != sourceWidth_ || resizeHeight != sourceHeight_) {
        scaleFilter = QStringLiteral("scale=%1:%2:flags=lanczos")
                          .arg(resizeWidth)
                          .arg(resizeHeight);
    }
    if (!finish.isEmpty()) scaleFilter += (scaleFilter.isEmpty() ? QString() : ",")+finish;
    if (reduce) scaleFilter += (scaleFilter.isEmpty() ? cadenceFilter.mid(1) : cadenceFilter);
    if (options_.playbackSpeed != 1.) {
        const QString speedFilter=QString("setpts=(PTS-STARTPTS)/%1,fps=%2:round=up,tpad=stop_mode=clone:stop=-1,trim=end_frame=%3,setpts=N/(%2*TB)")
            .arg(options_.playbackSpeed,0,'f',12)
            .arg(frameRateArgument(finalFps)).arg(videoOutputContract_.frameCount);
        scaleFilter += (scaleFilter.isEmpty() ? QString() : ",")+speedFilter;
    }
    if (gifOutput) {
        // Per-frame palettes keep memory bounded for long animations.
        QString gifFilter = QStringLiteral("[0:v]") + (scaleFilter.isEmpty() ? QString() : scaleFilter + QLatin1Char(','));
        gifFilter += QStringLiteral("split[a][b];[a]palettegen=stats_mode=single[p];[b][p]paletteuse=new=1[gif]");
        encodeArguments.append({ QStringLiteral("-filter_complex"), gifFilter });
    } else if (!scaleFilter.isEmpty()) {
        encodeArguments.append({ QStringLiteral("-vf"), scaleFilter });
    }
    if (videoOutputContract_.audioStreamCount > 0) {
        encodeArguments.append({ QStringLiteral("-map"), QStringLiteral("1:a") });
    }
    if (options_.keepMetadata) {
        encodeArguments.append({ QStringLiteral("-map_metadata"), QStringLiteral("1"),
            QStringLiteral("-map_metadata:s:v:0"), QStringLiteral("1:s:v:0") });
        for (int audioIndex = 0; audioIndex < videoOutputContract_.audioStreamCount;
            ++audioIndex) {
            encodeArguments.append({ QStringLiteral("-map_metadata:s:a:%1").arg(audioIndex),
                QStringLiteral("1:s:a:%1").arg(audioIndex) });
        }
    } else {
        encodeArguments.append({ "-map_metadata", "-1" });
    }
    // Chapters need an explicit trim/remapping contract before they can be
    // retained. FFmpeg otherwise copies them implicitly and creates an extra
    // MOV/MP4 data stream which is not part of our video/audio output contract.
    encodeArguments.append({ "-map_chapters", "-1" });
    encodeArguments.append({ QStringLiteral("-frames:v"), QString::number(videoOutputContract_.frameCount),
        QStringLiteral("-fps_mode"), QStringLiteral("cfr") });
    if (gifOutput) {
        encodeArguments.append({ QStringLiteral("-c:v"), QStringLiteral("gif"),
            QStringLiteral("-loop"), QStringLiteral("0"), QStringLiteral("-an") });
    } else {
        encodeArguments.append({ "-c:v", QString::fromStdString(options_.videoCodec),
            "-pix_fmt", QString::fromStdString(options_.pixelFormat == "auto" ? "yuv420p" : options_.pixelFormat),
            "-movflags", "+faststart+use_metadata_tags" });
        if (options_.videoCodec == "prores_ks") {
            encodeArguments.append({ "-profile:v", QString::number(options_.proresProfile), "-metadata", "encoding_profile=" + QString::fromStdString(proresProfileName(options_.proresProfile)),
                "-metadata", "crf=", "-metadata", "encoding_crf=", "-metadata", "encoding_preset=" });
        } else if (options_.videoCodec == "mjpeg") {
            encodeArguments.append({ "-q:v", "2", "-metadata", "crf=", "-metadata", "encoding_crf=", "-metadata", "encoding_preset=" });
        } else {
            encodeArguments.append({ "-preset", QString::fromStdString(options_.encoderPreset),
                "-crf", QString::number(options_.crf),
                "-metadata", "crf=" + QString::number(options_.crf),
                "-metadata", "encoding_crf=" + QString::number(options_.crf),
                "-metadata", "encoding_preset=" + QString::fromStdString(options_.encoderPreset),
                "-metadata", "encoding_profile=" });
        }
        // Source tags are copied above; these describe this new encode.
        encodeArguments.append({ "-metadata", "encoding_application=FrameScale",
            "-metadata", "encoding_codec=" + QString::fromStdString(options_.videoCodec),
            "-metadata", "major_brand=", "-metadata", "minor_version=", "-metadata", "compatible_brands=",
            "-metadata:s:v:0", "crf=" + (options_.videoCodec == "prores_ks" || options_.videoCodec == "mjpeg" || options_.rateControl != "crf" ? QString() : QString::number(options_.crf)),
            "-metadata:s:v:0", "encoding_crf=" + (options_.videoCodec == "prores_ks" || options_.videoCodec == "mjpeg" || options_.rateControl != "crf" ? QString() : QString::number(options_.crf)) });
        if (options_.rateControl != "crf" && options_.videoCodec != "prores_ks" && options_.videoCodec != "mjpeg") {
            const int crf = encodeArguments.indexOf("-crf");
            if (crf >= 0) {
                encodeArguments.removeAt(crf);
                encodeArguments.removeAt(crf);
            }
            for (auto& arg : encodeArguments)
                if (arg.startsWith("crf=") || arg.startsWith("encoding_crf="))
                    arg = arg.section('=', 0, 0) + "=";
            encodeArguments << "-b:v" << QString::number(options_.bitRate);
            if (options_.rateControl == "cbr")
                encodeArguments << "-minrate" << QString::number(options_.bitRate) << "-maxrate" << QString::number(options_.bitRate) << "-bufsize" << QString::number(qint64(options_.bitRate) * 2);
            else
                encodeArguments << "-maxrate" << QString::number(qint64(options_.bitRate) * 2) << "-bufsize" << QString::number(qint64(options_.bitRate) * 4);
        }
        if (options_.videoCodec == "libx264" || options_.videoCodec == "libx265") {
            if (options_.videoProfile != "auto")
                encodeArguments << "-profile:v" << QString::fromStdString(options_.videoProfile);
            if (options_.videoLevel != "auto")
                encodeArguments << "-level:v" << QString::fromStdString(options_.videoLevel);
        }
        if (options_.videoCodec == "libx265") {
            encodeArguments.append({ QStringLiteral("-tag:v"), QStringLiteral("hvc1") });
        }
    }
    if (videoOutputContract_.audioStreamCount > 0) {
        encodeArguments.append({ "-c:a", audioEncoder });
        if (audioEncoder != "copy") {
            if (audioEncoder != "pcm_s16le")
                encodeArguments << "-b:a" << QString::number(options_.audioBitRate);
            if (options_.audioSampleRate)
                encodeArguments << "-ar" << QString::number(options_.audioSampleRate);
            if (options_.audioChannels)
                encodeArguments << "-ac" << QString::number(options_.audioChannels);
        }
    }
    encodeArguments.append({ QStringLiteral("-t"),
        QString::number(videoOutputContract_.durationSeconds, 'f', 9),
        QStringLiteral("-f"), gifOutput ? QStringLiteral("gif") : QStringLiteral("mp4"), partialOutput });
    if (!gifOutput) {
        const std::pair<const char*, int> values[] = { { "-b:v", options_.bitRate }, { "-bufsize", options_.bufferSize },
            { "-minrate", options_.minRate }, { "-maxrate", options_.maxRate } };
        for (const auto& value : values) {
            if (value.second <= 0 || options_.rateControl != "crf")
                continue;
            encodeArguments.insert(encodeArguments.size() - 1, QString::fromLatin1(value.first));
            encodeArguments.insert(encodeArguments.size() - 1, QString::number(value.second));
        }
        if (options_.qmin >= 0) {
            encodeArguments.insert(encodeArguments.size() - 1, "-qmin");
            encodeArguments.insert(encodeArguments.size() - 1, QString::number(options_.qmin));
        }
        if (options_.qmax >= 0) {
            encodeArguments.insert(encodeArguments.size() - 1, "-qmax");
            encodeArguments.insert(encodeArguments.size() - 1, QString::number(options_.qmax));
        }
        for (const auto& item : options_.customEncoderOptions) {
            encodeArguments.insert(encodeArguments.size() - 1, QString::fromStdString("-" + item.first));
            encodeArguments.insert(encodeArguments.size() - 1, QString::fromStdString(item.second));
        }
    }
    const QString suffix = QFileInfo(fromPath(options_.outputPath)).suffix().toLower();
    if (suffix == "mkv" || suffix == "avi") {
        const int format = encodeArguments.lastIndexOf("-f");
        encodeArguments[format + 1] = suffix == "avi" ? "avi" : "matroska";
        const int flags = encodeArguments.indexOf("-movflags");
        if (flags >= 0) {
            encodeArguments.removeAt(flags);
            encodeArguments.removeAt(flags);
        }
    } else if (suffix == "mov") {
        const int format = encodeArguments.lastIndexOf("-f");
        encodeArguments[format + 1] = "mov";
    }
    if (direct) {
        // Replace the image-sequence input with the source. Keep audio/video maps on that one input.
        const int map = encodeArguments.indexOf("-map");
        encodeArguments = encodeArguments.mid(map);
        for (int i = 0; i < encodeArguments.size(); ++i) {
            QString& value = encodeArguments[i];
            if (value == "1:a")
                value = "0:a";
            else if (value == "1" && i > 0 && encodeArguments[i - 1] == "-map_metadata")
                value = "0";
            else if (value.startsWith("1:s:"))
                value.replace(0, 1, "0");
        }
        if (!gifOutput) {
            int filter = encodeArguments.indexOf("-vf");
            const QString trimFilter = extractFilter + (scaleFilter.isEmpty() ? QString() : "," + scaleFilter);
            if (filter >= 0)
                encodeArguments[filter + 1] = trimFilter;
            else {
                encodeArguments.prepend(trimFilter);
                encodeArguments.prepend("-vf");
            }
        }
        encodeArguments.prepend(fromPath(options_.inputPath));
        encodeArguments.prepend("-i");
        encodeArguments.prepend("-y");
        if (trimmed && videoOutputContract_.audioStreamCount > 0) {
            const QString af = QString("atrim=start=%1:duration=%2,asetpts=PTS-STARTPTS").arg(effectiveTrimStartSeconds_, 0, 'f', 9).arg(durationSeconds_, 0, 'f', 9);
            encodeArguments.insert(encodeArguments.size() - 1, "-af");
            encodeArguments.insert(encodeArguments.size() - 1, af);
        }
        if (gifOutput) {
            const int filter = encodeArguments.indexOf("-filter_complex");
            encodeArguments[filter + 1].replace("[0:v]", "[0:v]" + extractFilter + ",");
        }
    }
    if (options_.audioOffsetMs && videoOutputContract_.audioStreamCount > 0) {
        const int af = encodeArguments.indexOf("-af");
        const QString filter = shiftedAudioFilter(options_.audioOffsetMs, effectiveTrimStartSeconds_, durationSeconds_);
        if (af >= 0) encodeArguments[af + 1] = filter;
        else { encodeArguments.insert(encodeArguments.size()-1, "-af"); encodeArguments.insert(encodeArguments.size()-1, filter); }
    }
    if(options_.playbackSpeed != 1. && videoOutputContract_.audioStreamCount>0) {
        const int af=encodeArguments.indexOf("-af");
        const QString speedFilter=QString("aresample=48000,asetrate=%1,aresample=48000,apad,atrim=duration=%2")
            .arg(qRound(48000*options_.playbackSpeed)).arg(videoOutputContract_.durationSeconds,0,'f',9);
        if(af>=0) encodeArguments[af+1] += ","+speedFilter;
        else { encodeArguments.insert(encodeArguments.size()-1,"-af"); encodeArguments.insert(encodeArguments.size()-1,speedFilter); }
    }
    finalFramesDirectory_ = currentFrames;
    recoveryEncodeArguments_ = encodeArguments;
    directVideo_ = direct;
    videoEncodeStep_ = int(steps_.size());
    steps_.push_back({ gifOutput ? tr("Codificando GIF") : options_.videoCodec == "mjpeg" ? tr("Codificando Motion JPEG")
            : options_.videoCodec == "prores_ks"                                          ? tr("Codificando vídeo %1").arg(QString::fromStdString(proresProfileName(options_.proresProfile)))
            : options_.videoCodec == "libx265"                                            ? tr("Codificando vídeo H.265")
                                                                                          : tr("Codificando vídeo H.264"),
        toolPath(QStringLiteral("ffmpeg")),
        std::move(encodeArguments), 14, [partialOutput](QString& message) {
            const QFileInfo file(partialOutput);
            if (!file.exists() || !file.isFile() || file.size() == 0) {
                message = QObject::tr("O FFmpeg não produziu o vídeo final.");
                return false;
            }
            return true;
        },
        videoOutputContract_.audioStreamCount > 0 && audioEncoder == "copy", videoOutputContract_.frameCount });

    steps_.push_back({ tr("Validando metadados do vídeo"),
        toolPath(QStringLiteral("ffprobe")),
        { QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-show_streams"),
            QStringLiteral("-show_format"), QStringLiteral("-of"),
            QStringLiteral("json"), partialOutput },
        3, [this](QString& message) { return verifyFinalVideoProbe(message); } });

    steps_.push_back({ tr("Verificando decodificação completa"),
        toolPath(QStringLiteral("ffmpeg")),
        { QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-progress"), QStringLiteral("pipe:1"),
            QStringLiteral("-nostats"), QStringLiteral("-stats_period"), QStringLiteral("0.5"),
            QStringLiteral("-xerror"), QStringLiteral("-err_detect"),
            QStringLiteral("explode"), QStringLiteral("-i"), partialOutput,
            QStringLiteral("-map"), QStringLiteral("0:v:0"),
            QStringLiteral("-map"), QStringLiteral("0:a?"),
            QStringLiteral("-fps_mode"), QStringLiteral("passthrough"),
            QStringLiteral("-f"), QStringLiteral("null"), QStringLiteral("-") },
        3, [this](QString& message) {
            if (!outputBuffer_.trimmed().isEmpty()) {
                message = QObject::tr(
                    "A decodificação final reportou erros.\n%1")
                              .arg(QString::fromLocal8Bit(outputBuffer_).right(1400));
                return false;
            }
            // Count during the integrity decode, instead of decoding the whole
            // export once in ffprobe and a second time here.
            if (decodedFrameCount_ != videoOutputContract_.frameCount) {
                message = QObject::tr("O vídeo final contém %1 fotogramas; eram esperados %2.")
                    .arg(decodedFrameCount_).arg(videoOutputContract_.frameCount);
                return false;
            }
            return true;
        },
        false, videoOutputContract_.frameCount });

    totalWeight_ = 0;
    for (Step& step : steps_) {
        if (options_.enhancement.enabled && step.program == toolPath("ffmpeg"))
            step.arguments = QStringList{"-filter_threads",processingFilterThreads(),"-filter_complex_threads",processingFilterThreads()} + step.arguments;
        totalWeight_ += step.weight;
        step.verifyInBackground = !step.progressFramesDirectory.isEmpty();
        if (step.program == toolPath(QStringLiteral("ffmpeg"))
            && step.progressFrameCount > 0 && !step.arguments.contains(QStringLiteral("-xerror"))) {
            step.arguments = QStringList { QStringLiteral("-nostats"), QStringLiteral("-stats_period"),
                QStringLiteral("0.05"), QStringLiteral("-progress"), QStringLiteral("pipe:1") }
                + step.arguments;
        }
        if (step.program == toolPath(QStringLiteral("ffmpeg"))) {
            for (int index = step.arguments.size() - 2; index >= 0; --index) {
                if (step.arguments[index] == QStringLiteral("-i")
                    && QFileInfo(step.arguments[index + 1]).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0) {
                    step.arguments.insert(index, QStringLiteral("0"));
                    step.arguments.insert(index, QStringLiteral("-min_delay"));
                }
            }
        } else if (gifOutput && step.program == toolPath(QStringLiteral("ffprobe"))) {
            step.arguments = QStringList { QStringLiteral("-min_delay"), QStringLiteral("0") } + step.arguments;
        }
    }
    return true;
}

bool ProcessingJob::verifyFinalVideoProbe(QString& error) const
{
    const bool gifOutput = QFileInfo(fromPath(options_.outputPath)).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0;
    const QString expectedCodec = gifOutput  ? QStringLiteral("gif")
        : options_.videoCodec == "prores_ks" ? QStringLiteral("prores")
        : options_.videoCodec == "libx265"   ? QStringLiteral("hevc")
        : options_.videoCodec == "mjpeg"     ? QStringLiteral("mjpeg")
                                             : QStringLiteral("h264");
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        outputBuffer_, &parseError);
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject()) {
        error = tr("O ffprobe devolveu metadados inválidos para o vídeo final.");
        return false;
    }

    const QStringList formats = document.object()
                                    .value(QStringLiteral("format"))
                                    .toObject()
                                    .value(QStringLiteral("format_name"))
                                    .toString()
                                    .split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QString suffix = QFileInfo(fromPath(options_.outputPath)).suffix().toLower();
    const QString expectedFormat = gifOutput ? "gif" : suffix == "mkv" ? "matroska"
        : suffix == "avi"                                              ? "avi"
        : suffix == "mov"                                              ? "mov"
                                                                       : "mp4";
    if (!formats.contains(expectedFormat)) {
        error = tr("O ficheiro final não usa o formato de saída selecionado.");
        return false;
    }

    int videoStreams = 0;
    int audioStreams = 0;
    int unexpectedStreams = 0;
    QStringList audioCodecs;
    const QJsonArray streams = document.object()
                                   .value(QStringLiteral("streams"))
                                   .toArray();
    for (const QJsonValue& value : streams) {
        const QJsonObject stream = value.toObject();
        const QString type = stream.value(QStringLiteral("codec_type")).toString();
        if (type == QStringLiteral("video")) {
            ++videoStreams;
            if (stream.value(QStringLiteral("codec_name")).toString()
                != expectedCodec) {
                error = tr("O vídeo final não usa o codec selecionado (%1).").arg(expectedCodec);
                return false;
            }
            // FFmpeg feeds prores_ks with 10-bit 4:4:4 frames; its 4444 decoder
            // reports the profile's 12-bit output representation.
            const QString expectedPixels = options_.videoCodec == "prores_ks" && options_.proresProfile >= 4
                ? QStringLiteral("yuv444p12le")
                : QString::fromStdString(options_.pixelFormat == "auto" ? "yuv420p" : options_.pixelFormat);
            if (!gifOutput && stream.value(QStringLiteral("pix_fmt")).toString() != expectedPixels) {
                error = tr("O vídeo final não usa o formato de píxel selecionado.");
                return false;
            }
            if (stream.value(QStringLiteral("width")).toInt()
                    != videoOutputContract_.width
                || stream.value(QStringLiteral("height")).toInt()
                    != videoOutputContract_.height) {
                error = tr("O vídeo final tem uma resolução diferente da esperada.");
                return false;
            }
            const qint64 frames = jsonInteger(stream, QStringLiteral("nb_frames"));
            // Some containers (notably Matroska) do not store a frame count.
            // The following full decode checks the actual count in all cases.
            if (frames > 0 && frames != videoOutputContract_.frameCount) {
                error = tr("O vídeo final contém %1 fotogramas; eram esperados %2.")
                            .arg(frames)
                            .arg(videoOutputContract_.frameCount);
                return false;
            }
            const double fps = parseRate(stream.value(
                                                   QStringLiteral("avg_frame_rate"))
                    .toString());
            const double fpsTolerance = std::max(
                0.0001, videoOutputContract_.fps * 0.000001);
            if (!gifOutput && !approximatelyEqual(fps, videoOutputContract_.fps, fpsTolerance)) {
                error = tr("O vídeo final tem FPS %1; eram esperados %2.")
                            .arg(fps, 0, 'f', 6)
                            .arg(videoOutputContract_.fps, 0, 'f', 6);
                return false;
            }
            double duration = jsonNumber(
                stream, QStringLiteral("duration"));
            if (duration <= 0.0 && (gifOutput || suffix == "mkv" || suffix == "avi")) {
                duration = jsonNumber(document.object().value(QStringLiteral("format")).toObject(),
                    QStringLiteral("duration"));
            }
            const double durationTolerance = std::max(
                gifOutput ? 0.02 * videoOutputContract_.frameCount : 0.05, 1.0 / videoOutputContract_.fps);
            if (!std::isfinite(duration) || duration <= 0.0) {
                error = tr("Não foi possível validar a duração do vídeo final.");
                return false;
            }
            if (!approximatelyEqual(duration,
                    videoOutputContract_.durationSeconds,
                    durationTolerance)) {
                error = tr("A duração do vídeo final (%1 s) difere da esperada (%2 s).")
                            .arg(duration, 0, 'f', 6)
                            .arg(videoOutputContract_.durationSeconds, 0, 'f', 6);
                return false;
            }
        } else if (type == QStringLiteral("audio")) {
            ++audioStreams;
            audioCodecs.append(stream.value(
                                         QStringLiteral("codec_name"))
                    .toString());
        } else {
            ++unexpectedStreams;
        }
    }

    if (unexpectedStreams != 0) {
        error = tr("O ficheiro final contém %1 streams inesperadas.")
                    .arg(unexpectedStreams);
        return false;
    }
    if (videoStreams != 1) {
        error = tr("O ficheiro final contém %1 streams de vídeo; era esperada exatamente uma.")
                    .arg(videoStreams);
        return false;
    }
    if (audioStreams != videoOutputContract_.audioStreamCount) {
        error = tr("O ficheiro final contém %1 streams de áudio; eram esperadas %2.")
                    .arg(audioStreams)
                    .arg(videoOutputContract_.audioStreamCount);
        return false;
    }
    if (aacRetryAttempted_) {
        for (const QString& codec : audioCodecs) {
            if (codec != QStringLiteral("aac")) {
                error = tr("O fallback de áudio não produziu AAC em todas as streams.");
                return false;
            }
        }
    } else if (audioCodecs != videoOutputContract_.audioCodecs) {
        error = tr("Os codecs de áudio copiados não correspondem à origem.");
        return false;
    }
    return true;
}

void ProcessingJob::runNextStep()
{
    frameProgressTimer_.stop();
    frameScan_.reset();
    if (cancelling_) {
        finishCancellation();
        return;
    }

    ++stepIndex_;
    if (stepIndex_ >= static_cast<int>(steps_.size())) {
        finishSuccessfully();
        return;
    }

    const Step& step = steps_[stepIndex_];
    outputBuffer_.clear();
    progressBuffer_.clear();
    completedFrameFiles_.clear();
    currentFrameProgress_ = 0;
    decodedFrameCount_ = 0;
    lastProgress_ = std::max(lastProgress_,
        std::clamp(completedWeight_ * 100 / std::max(1, totalWeight_), 0, 99));
    emit progressChanged(lastProgress_, step.title);
    if (!running_ || cancelling_) {
        return;
    }
    emit frameProgressChanged(0, step.progressFrameCount);
    if (!running_ || cancelling_) {
        return;
    }
    process_.setProgram(step.program);
    process_.setArguments(step.arguments);
    const bool integrityDecode = step.arguments.contains(QStringLiteral("-xerror"))
        && step.arguments.contains(QStringLiteral("-progress"));
    process_.setProcessChannelMode(integrityDecode ? QProcess::SeparateChannels : QProcess::MergedChannels);
    process_.start();
    if (!step.progressFramesDirectory.isEmpty()) {
        frameProgressTimer_.start();
    }
}

void ProcessingJob::handleOutput()
{
    const QByteArray chunk = process_.readAllStandardOutput();
    if (probingVideoTimeline_) {
        consumeVideoTimeline(chunk);
        outputBuffer_.append(process_.readAllStandardError());
        outputBuffer_ = outputBuffer_.right(4 * 1024 * 1024);
        return;
    }
    const bool integrityDecode = process_.processChannelMode() == QProcess::SeparateChannels;
    outputBuffer_.append(integrityDecode ? process_.readAllStandardError() : chunk);
    constexpr qsizetype maximumDiagnosticOutput = 4 * 1024 * 1024;
    if (stepIndex_ >= 0 && outputBuffer_.size() > maximumDiagnosticOutput) {
        outputBuffer_ = outputBuffer_.right(maximumDiagnosticOutput);
    }

    if (!running_ || cancelling_
        || stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())
        || steps_[stepIndex_].progressFrameCount <= 0) {
        return;
    }

    progressBuffer_.append(chunk);
    constexpr qsizetype maximumCarry = 4096;
    if (progressBuffer_.size() > maximumCarry) {
        progressBuffer_ = progressBuffer_.right(maximumCarry);
    }
    const QString text = QString::fromLocal8Bit(progressBuffer_);

    static const QRegularExpression percentExpression(
        QStringLiteral(R"((\d{1,3})(?:\.\d+)?%)"));
    QRegularExpressionMatchIterator percentMatches = percentExpression.globalMatch(text);
    double greatestPercent = -1.0;
    while (percentMatches.hasNext()) {
        greatestPercent = std::max(
            greatestPercent,
            percentMatches.next().captured(1).toDouble());
    }
    // NCNN percentages describe individual tiles/frames and reset repeatedly.
    // Only single-image stages can use these as a stage percentage.
    if (steps_[stepIndex_].progressFrameCount == 1
        && greatestPercent >= 0.0 && greatestPercent <= 100.0) {
        updateCurrentProgress(greatestPercent / 100.0);
        if (!running_ || cancelling_) {
            return;
        }
    }

    static const QRegularExpression frameExpression(
        QStringLiteral(R"(\bframe\s*=\s*(\d+)(?=\s|$))"));
    QRegularExpressionMatchIterator frameMatches = frameExpression.globalMatch(text);
    qint64 greatestFrame = 0;
    while (frameMatches.hasNext()) {
        const QRegularExpressionMatch match = frameMatches.next();
        bool ok = false;
        const qint64 frame = match.captured(1).toLongLong(&ok);
        if (ok) {
            greatestFrame = std::max(greatestFrame, frame);
        }
    }
    if (greatestFrame > 0 && steps_[stepIndex_].progressFramesDirectory.isEmpty()) {
        if (integrityDecode)
            decodedFrameCount_ = std::max(decodedFrameCount_, greatestFrame);
        updateCurrentFrameProgress(greatestFrame);
    }
}

void ProcessingJob::handleProcessFinished(
    const int exitCode,
    const QProcess::ExitStatus status)
{
    frameProgressTimer_.stop();
    handleOutput();
    if (!running_) {
        return;
    }
    if (cancelling_) {
        finishCancellation();
        return;
    }

    if (composingLayers_) {
        composingLayers_ = false;
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(tr("Não foi possível compor as camadas.\n%1").arg(QString::fromUtf8(outputBuffer_).right(2000)));
            return;
        }
#ifdef _WIN32
        options_.inputPath = std::filesystem::path(compositionPath_.toStdWString());
#else
        options_.inputPath = std::filesystem::path(compositionPath_.toStdString());
#endif
        options_.playbackSpeed = 1;
        options_.audioOffsetMs = 0;
        options_.mediaType = MediaType::Video;
        outputBuffer_.clear();
        probeMedia();
        return;
    }
    if (stepIndex_ == -1) {
        if (status != QProcess::NormalExit || exitCode != 0) {
            fail(tr("O ffprobe não conseguiu analisar o ficheiro.\n%1")
                    .arg(QString::fromLocal8Bit(outputBuffer_).right(1000)));
            return;
        }

        if (probingVideoTimeline_) {
            consumeVideoTimeline("\n");
            probingVideoTimeline_ = false;
            videoTimelineProbed_ = true;
            // Missing final-frame duration is common in older demuxers. Prefer
            // the last observed presentation interval to the nominal rate.
            double finalDuration = timelineLastDuration_;
            if (finalDuration <= 0.0)
                finalDuration = timelinePreviousInterval_ > 0.0
                    ? timelinePreviousInterval_ : (sourceFps_ > 0.0 ? 1.0 / sourceFps_ : 0.0);
            timelineDurationSeconds_ = timelineLastTimestamp_ + finalDuration - timelineFirstTimestamp_;
            if (invalidVideoTimeline_ || timelineFrameCount_ <= 0
                || !std::isfinite(timelineDurationSeconds_) || timelineDurationSeconds_ <= 0.0
                || !outputBuffer_.trimmed().isEmpty()) {
                fail(tr("Não foi possível determinar a duração dos fotogramas do vídeo.\n%1")
                    .arg(QString::fromLocal8Bit(outputBuffer_).right(1000)));
                return;
            }
            outputBuffer_ = std::move(sourceProbeMetadata_);
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            outputBuffer_, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            fail(tr("O ffprobe devolveu metadados inválidos para o ficheiro de entrada."));
            return;
        }
        // Only the selected video stream defines the visual timeline. Audio,
        // subtitles and chapter tracks may extend the container duration.
        durationSeconds_ = 0.0;
        sourceAudioStreamCount_ = 0;
        sourceAudioCodecs_.clear();
        const QJsonArray streams = document.object()
                                       .value(QStringLiteral("streams"))
                                       .toArray();
        bool selectedVideo = false;
        for (const QJsonValue& value : streams) {
            const QJsonObject stream = value.toObject();
            const QString type = stream.value(QStringLiteral("codec_type")).toString();
            if (type == QStringLiteral("video") && !selectedVideo) {
                selectedVideo = true;
                sourceFps_ = parseRate(stream.value(
                                                 QStringLiteral("avg_frame_rate"))
                        .toString());
                if (sourceFps_ <= 0.0) {
                    sourceFps_ = parseRate(stream.value(
                                                     QStringLiteral("r_frame_rate"))
                            .toString());
                }
                sourceWidth_ = stream.value(QStringLiteral("width")).toInt();
                sourceHeight_ = stream.value(QStringLiteral("height")).toInt();
                if (rotationSwapsDimensions(displayRotation(stream))) {
                    std::swap(sourceWidth_, sourceHeight_);
                }
                sourceFrameCount_ = jsonInteger(
                    stream, QStringLiteral("nb_read_frames"));
                if (sourceFrameCount_ <= 0) {
                    sourceFrameCount_ = jsonInteger(
                        stream, QStringLiteral("nb_frames"));
                }
                const double streamDuration = jsonNumber(
                    stream, QStringLiteral("duration"));
                if (streamDuration > 0.0) {
                    durationSeconds_ = streamDuration;
                }
                const qint64 durationTicks = jsonInteger(
                    stream, QStringLiteral("duration_ts"));
                const double timeBase = parseRate(stream.value(
                                                            QStringLiteral("time_base"))
                        .toString());
                if (durationTicks > 0 && timeBase > 0.0) {
                    durationSeconds_ = durationTicks * timeBase;
                }
            } else if (type == QStringLiteral("audio")) {
                ++sourceAudioStreamCount_;
                sourceAudioCodecs_.append(stream.value(
                                                    QStringLiteral("codec_name"))
                        .toString());
            }
        }
        if (options_.mediaType == MediaType::Video && selectedVideo && !videoTimelineProbed_
            && (sourceFrameCount_ <= 0 || !std::isfinite(durationSeconds_) || durationSeconds_ <= 0.0
                || !std::isfinite(sourceFps_) || sourceFps_ <= 0.0)) {
            probeVideoTimeline();
            return;
        }
        if (videoTimelineProbed_) {
            sourceFrameCount_ = timelineFrameCount_;
            durationSeconds_ = timelineDurationSeconds_;
            if (!std::isfinite(sourceFps_) || sourceFps_ <= 0.0)
                sourceFps_ = sourceFrameCount_ / durationSeconds_;
        }
        if (sourceFrameCount_ <= 0 && durationSeconds_ > 0.0
            && sourceFps_ > 0.0) {
            sourceFrameCount_ = qRound64(durationSeconds_ * sourceFps_);
        }
        if (sourceWidth_ <= 0 || sourceHeight_ <= 0) {
            fail(tr("O ficheiro não contém uma stream de vídeo/imagem válida."));
            return;
        }
        if (options_.mediaType == MediaType::Video
            && (!std::isfinite(durationSeconds_) || durationSeconds_ <= 0.0
                || !std::isfinite(sourceFps_) || sourceFps_ <= 0.0
                || sourceFrameCount_ <= 0)) {
            fail(tr("Não foi possível determinar FPS, duração e número de fotogramas do vídeo."));
            return;
        }
        if (options_.mediaType == MediaType::Video) {
            const double playbackRate = sourceFrameCount_ / durationSeconds_;
            const double requestedEnd = options_.trimEndSeconds > 0.0
                ? std::min(options_.trimEndSeconds, durationSeconds_)
                : durationSeconds_;
            trimStartFrame_ = static_cast<qint64>(std::ceil(options_.trimStartSeconds * playbackRate - 1e-9));
            trimEndFrame_ = std::min(sourceFrameCount_, static_cast<qint64>(std::ceil(requestedEnd * playbackRate - 1e-9)));
            if (trimStartFrame_ >= trimEndFrame_) {
                fail(tr("O intervalo de corte não contém fotogramas. Escolha um início anterior ao fim do vídeo."));
                return;
            }
            effectiveTrimStartSeconds_ = trimStartFrame_ / playbackRate;
            sourceFrameCount_ = trimEndFrame_ - trimStartFrame_;
            durationSeconds_ = sourceFrameCount_ / playbackRate;
        }
        if (!audioOnly(options_) && usesInterpolation(options_.operation)
            && sourceFrameCount_ < 2) {
            fail(tr("A interpolação requer pelo menos dois fotogramas de origem."));
            return;
        }
        if (!audioOnly(options_) && options_.mediaType == MediaType::Video) {
            const int finalWidth = usesUpscale(options_.operation)
                ? scaledDimension(sourceWidth_, options_.scaleFactor, !imageSequence(options_) && options_.pixelFormat != "yuv444p")
                : sourceWidth_;
            const int finalHeight = usesUpscale(options_.operation)
                ? scaledDimension(sourceHeight_, options_.scaleFactor, !imageSequence(options_) && options_.pixelFormat != "yuv444p")
                : sourceHeight_;
            const bool gifOutput = QFileInfo(fromPath(options_.outputPath)).suffix().compare(QStringLiteral("gif"), Qt::CaseInsensitive) == 0;
            if (!audioOnly(options_) && !imageSequence(options_) && !gifOutput && (options_.pixelFormat == "auto" || options_.pixelFormat == "yuv420p" || options_.pixelFormat == "yuv422p10le")
                && ((finalWidth % 2) != 0 || (finalHeight % 2) != 0)) {
                fail(tr("A resolução final %1×%2 não pode ser codificada em yuv420p; a largura e a altura têm de ser pares.")
                        .arg(finalWidth)
                        .arg(finalHeight));
                return;
            }
        }
        const double sourcePlaybackFps = sourceFrameCount_ / durationSeconds_;
        if (!audioOnly(options_) && options_.reductionFps > 0 && options_.reductionFps >= sourcePlaybackFps - .0001) {
            fail(tr("A reducao de FPS exige um valor inferior ao original."));
            return;
        }
        if (!audioOnly(options_) && usesInterpolation(options_.operation)
            && options_.targetFps <= sourcePlaybackFps + 0.0001) {
            fail(tr("O FPS pretendido (%1) tem de ser maior que o FPS médio de origem (%2).")
                    .arg(options_.targetFps)
                    .arg(sourcePlaybackFps, 0, 'f', 3));
            return;
        }

        QString error;
        if (!prepareSteps(error)) {
            fail(error);
            return;
        }
        if (!running_ || cancelling_) {
            return;
        }
        runNextStep();
        return;
    }

    if (stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())) {
        fail(tr("O pipeline entrou num estado interno inválido."));
        return;
    }

    if (status != QProcess::NormalExit || exitCode != 0) {
        if (status == QProcess::NormalExit && retryCurrentVideoEncodeWithAac()) {
            return;
        }
        fail(tr("A etapa “%1” falhou (código %2).\n%3")
                .arg(steps_[stepIndex_].title)
                .arg(exitCode)
                .arg(QString::fromLocal8Bit(outputBuffer_).right(1400)));
        return;
    }

    const Step& step = steps_[stepIndex_];
    if (step.rejectDiagnostics && !outputBuffer_.trimmed().isEmpty()) {
        fail(tr("A decodificação da imagem final reportou erros.\n%1")
            .arg(QString::fromLocal8Bit(outputBuffer_).right(1400)));
        return;
    }
    if (step.verify && step.verifyInBackground) {
        verification_ = std::make_shared<VerificationResult>();
        const auto result = verification_;
        // These callbacks contain only immutable paths/dimensions, never this.
        QThreadPool::globalInstance()->start([result, verify = step.verify,
                                                 workspace = temporaryDirectory_] {
            result->valid = verify(result->error);
            result->complete.store(true, std::memory_order_release);
        });
        verificationTimer_.start();
        return;
    }
    QString verificationError;
    const bool valid = !step.verify || step.verify(verificationError);
    completeVerifiedStep(valid, verificationError);
}

void ProcessingJob::completeVerifiedStep(const bool valid, const QString& error)
{
    if (!running_ || cancelling_) {
        return;
    }
    if (!valid) {
        fail(error);
        return;
    }

    updateCurrentFrameProgress(steps_[stepIndex_].progressFrameCount);
    if (!running_ || cancelling_) {
        return;
    }
    completedWeight_ += steps_[stepIndex_].weight;
    runNextStep();
}

bool ProcessingJob::retryCurrentVideoEncodeWithAac()
{
    if (stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())
        || !steps_[stepIndex_].retryWithAac || aacRetryAttempted_ || cancelling_
        || !reportsMp4AudioCodecIncompatibility(outputBuffer_)) {
        return false;
    }

    Step& step = steps_[stepIndex_];
    int codecIndex = -1;
    for (int index = 0; index + 1 < step.arguments.size(); ++index) {
        if (step.arguments[index] == QStringLiteral("-c:a")
            && step.arguments[index + 1] == QStringLiteral("copy")) {
            codecIndex = index + 1;
            break;
        }
    }
    if (codecIndex < 0) {
        return false;
    }

    aacRetryAttempted_ = true;
    QFile::remove(outputTemporaryPath_);
    step.arguments[codecIndex] = QStringLiteral("aac");
    step.arguments.insert(codecIndex + 1, QStringLiteral("-b:a"));
    step.arguments.insert(codecIndex + 2, QStringLiteral("192k"));
    outputBuffer_.clear();
    progressBuffer_.clear();
    currentFrameProgress_ = 0;
    emit progressChanged(lastProgress_,
        tr("Áudio incompatível; tentando novamente em AAC"));
    if (!running_ || cancelling_) {
        return true;
    }
    emit frameProgressChanged(0, step.progressFrameCount);
    if (!running_ || cancelling_) {
        return true;
    }
    process_.setProgram(step.program);
    process_.setArguments(step.arguments);
    process_.start();
    return true;
}

void ProcessingJob::updateCurrentProgress(const double fraction)
{
    if (!running_ || cancelling_
        || stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())) {
        return;
    }
    const int value = static_cast<int>(
        (completedWeight_ + steps_[stepIndex_].weight * std::clamp(fraction, 0.0, 1.0))
        * 100.0 / std::max(1, totalWeight_));
    lastProgress_ = std::max(lastProgress_, std::clamp(value, 0, 99));
    emit progressChanged(lastProgress_, steps_[stepIndex_].title);
}

void ProcessingJob::updateCurrentFrameProgress(const qint64 frameCount)
{
    if (!running_ || cancelling_
        || stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())) {
        return;
    }
    const qint64 total = steps_[stepIndex_].progressFrameCount;
    const qint64 completed = std::clamp(frameCount, qint64(0), total);
    if (total <= 0 || completed <= currentFrameProgress_) {
        return;
    }
    // Every emitted increment refers to a frame already reported by FFmpeg
    // or confirmed complete on disk. Several can arrive in one event-loop turn.
    while (currentFrameProgress_ < completed) {
        emit frameProgressChanged(++currentFrameProgress_, total);
        if (!running_ || cancelling_) {
            return;
        }
    }
    updateCurrentProgress(static_cast<double>(completed) / total);
}

void ProcessingJob::pollCompletedFrames()
{
    if (!running_ || cancelling_
        || stepIndex_ < 0 || stepIndex_ >= static_cast<int>(steps_.size())) {
        return;
    }
    const QString directory = steps_[stepIndex_].progressFramesDirectory;
    if (directory.isEmpty()) {
        return;
    }
    if (frameScan_) {
        if (!frameScan_->complete.load(std::memory_order_acquire)) {
            return;
        }
        completedFrameFiles_ = std::move(frameScan_->files);
        frameScan_.reset();
        updateCurrentFrameProgress(completedFrameFiles_.size());
        if (!running_ || cancelling_) {
            return;
        }
    }
    frameScan_ = std::make_shared<FrameScanResult>();
    frameScan_->files = completedFrameFiles_;
    QThreadPool::globalInstance()->start([result = frameScan_, directory,
                                             workspace = temporaryDirectory_] {
        const QStringList files = QDir(directory).entryList(
            { QStringLiteral("*.png") }, QDir::Files, QDir::NoSort);
        const QByteArray pngEnd = QByteArray::fromHex("0000000049454e44ae426082");
        for (const QString& name : files) {
            if (result->files.contains(name)) {
                continue;
            }
            QFile file(QDir(directory).filePath(name));
            if (file.open(QIODevice::ReadOnly) && file.size() >= pngEnd.size()
                && file.seek(file.size() - pngEnd.size())
                && file.read(pngEnd.size()) == pngEnd) {
                result->files.insert(name);
            }
        }
        result->complete.store(true, std::memory_order_release);
    });
}

void ProcessingJob::finishSuccessfully()
{
    cancellationTimer_.stop();
    frameProgressTimer_.stop();
    frameScan_.reset();
    QString error;
    if (!copyOrCommitOutput(outputTemporaryPath_, error)) {
        fail(error);
        return;
    }
    running_ = false;
    temporaryDirectory_.reset();
    compositionDirectory_.reset();
    emit progressChanged(100, tr("Concluído"));
    emit finished(fromPath(options_.outputPath));
}

void ProcessingJob::fail(const QString& message)
{
    if (!running_ && stepIndex_ >= 0) {
        return;
    }
    cancellationTimer_.stop();
    frameProgressTimer_.stop();
    verificationTimer_.stop();
    verification_.reset();
    frameScan_.reset();
    running_ = false;
    process_.kill();
    cleanupOutputArtifacts();
    temporaryDirectory_.reset();
    emit failed(message);
}

void ProcessingJob::finishCancellation()
{
    if (cancellationSignalled_) {
        return;
    }
    cancellationTimer_.stop();
    if (preservePartialOnCancel && !recoveryAttempted_ && recoverPartialVideo()) return;
    cancellationSignalled_ = true;
    frameProgressTimer_.stop();
    verificationTimer_.stop();
    verification_.reset();
    frameScan_.reset();
    running_ = false;
    cleanupOutputArtifacts();
    temporaryDirectory_.reset();
    compositionDirectory_.reset();
    emit cancelled();
}

bool ProcessingJob::recoverPartialVideo()
{
    recoveryAttempted_ = true;
    if (options_.mediaType != MediaType::Video || recoveryEncodeArguments_.isEmpty()) return false;
    frameProgressTimer_.stop();
    verificationTimer_.stop();
    verification_.reset();
    emit progressChanged(lastProgress_, tr("Salvando o trecho já processado…"));
    struct Scan { std::atomic<bool> done{false}; qint64 count = 0; };
    auto scan = std::make_shared<Scan>();
    QString folder = finalFramesDirectory_;
    // A later restoration/resize pass may not have started yet. Recover the
    // contiguous completed frames of the active pass instead of discarding them.
    if(!directVideo_ && stepIndex_>=0 && stepIndex_<int(steps_.size())
        && steps_[stepIndex_].progressFrameCount==videoOutputContract_.frameCount
        && !steps_[stepIndex_].progressFramesDirectory.isEmpty()
        && !QFileInfo::exists(QDir(folder).filePath("00000001.png")))
        folder=steps_[stepIndex_].progressFramesDirectory;
    const QString recoveryFolder=folder;
    const bool direct = directVideo_;
    const qint64 encoded = stepIndex_ == videoEncodeStep_ ? currentFrameProgress_
        : stepIndex_ > videoEncodeStep_ ? videoOutputContract_.frameCount : 0;
    const qint64 maximum = videoOutputContract_.frameCount;
    QThreadPool::globalInstance()->start([scan, folder, direct, encoded, maximum] {
        if (direct) scan->count = encoded;
        else for (qint64 i = 1; i <= maximum; ++i) {
            QImage image(QDir(folder).filePath(QString("%1.png").arg(i, 8, 10, QChar('0'))));
            if (image.isNull()) break;
            ++scan->count;
        }
        scan->done.store(true);
    });
    auto* poll = new QTimer(this);
    poll->setInterval(50);
    connect(poll, &QTimer::timeout, this, [this, poll, scan, recoveryFolder] {
        if (!scan->done.load()) return;
        poll->stop(); poll->deleteLater();
        if (scan->count < 1) { finishCancellation(); return; }
        QStringList args = recoveryEncodeArguments_;
        if(!directVideo_ && recoveryFolder!=finalFramesDirectory_) {
            const int input=args.indexOf("-i");
            if(input<0) { finishCancellation(); return; }
            args[input+1]=QDir(recoveryFolder).filePath("%08d.png");
        }
        const int frames = args.indexOf("-frames:v"), duration = args.lastIndexOf("-t");
        if (frames < 0 || duration < 0) { finishCancellation(); return; }
        args[frames + 1] = QString::number(scan->count);
        args[duration + 1] = QString::number(scan->count / videoOutputContract_.fps, 'f', 9);
        auto* recovery = new QProcess(this);
        recovery->setStandardOutputFile(QProcess::nullDevice());
        recovery->setStandardErrorFile(QProcess::nullDevice());
        auto complete = [this, recovery](int code, QProcess::ExitStatus exit) {
            recovery->deleteLater();
            if (code != 0 || exit != QProcess::NormalExit) { finishCancellation(); return; }
            auto* check = new QProcess(this);
            check->setStandardOutputFile(QProcess::nullDevice());
            check->setStandardErrorFile(QProcess::nullDevice());
            connect(check, &QProcess::finished, this, [this, check](int result, QProcess::ExitStatus state) {
                check->deleteLater();
                if (result == 0 && state == QProcess::NormalExit) {
                    const QFileInfo destination(fromPath(options_.outputPath));
                    for (int number = 1; number < 10000; ++number) {
                        const QString suffix = number == 1 ? QString() : "_" + QString::number(number);
                        const QString path = destination.dir().filePath(destination.completeBaseName() + "_parcial" + suffix + "." + destination.suffix());
                        if (!QFileInfo::exists(path) && QFile::rename(outputTemporaryPath_, path)) { savedPartialPath_ = path; break; }
                    }
                }
                finishCancellation();
            });
            connect(check, &QProcess::errorOccurred, this, [this,check](QProcess::ProcessError e) {
                if (e == QProcess::FailedToStart) { check->deleteLater(); finishCancellation(); }
            });
            check->start(toolPath("ffmpeg"), {"-v", "error", "-xerror", "-i", outputTemporaryPath_, "-map", "0:v:0", "-map", "0:a?", "-f", "null", "-"});
        };
        connect(recovery, &QProcess::finished, this, complete);
        connect(recovery, &QProcess::errorOccurred, this, [this,recovery](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) { recovery->deleteLater(); finishCancellation(); }
        });
        recovery->start(toolPath("ffmpeg"), args);
    });
    poll->start();
    return true;
}

void ProcessingJob::cleanupOutputArtifacts()
{
    if (!outputTemporaryPath_.isEmpty()) {
        if (imageSequence(options_) && QFileInfo(outputTemporaryPath_).isDir()) {
            const QString path = outputTemporaryPath_;
            QThreadPool::globalInstance()->start([path] { QDir(path).removeRecursively(); });
        } else
            QFile::remove(outputTemporaryPath_);
    }
    if (!commitToken_.isEmpty()) {
        QFile::remove(stagedOutputPath());
    }
}

QString ProcessingJob::outputTemporaryPath() const
{
    const QFileInfo output(fromPath(options_.outputPath));
    const QString extension = output.suffix();
    const QString base = extension.isEmpty()
        ? output.fileName()
        : output.completeBaseName();
    const QString token = commitToken_.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : commitToken_;
    const QString name = extension.isEmpty()
        ? QStringLiteral("%1.framescale-%2.part").arg(base, token)
        : QStringLiteral("%1.framescale-%2.part.%3").arg(base, token, extension);
    return QDir(output.absolutePath()).filePath(name);
}

QString ProcessingJob::stagedOutputPath() const
{
    return QStringLiteral("%1.framescale-%2.staged")
        .arg(fromPath(options_.outputPath), commitToken_);
}

QString ProcessingJob::backupOutputPath() const
{
    return QStringLiteral("%1.framescale-%2.backup")
        .arg(fromPath(options_.outputPath), commitToken_);
}

bool ProcessingJob::validateSequenceDestination(const QString& destination, QString& error) const
{
    const QFileInfo info(destination);
    if (!info.exists() && !info.isSymLink())
        return true;
    if (!info.isDir() || info.isSymLink()) {
        error = tr("Escolha uma pasta própria para a sequência.");
        return false;
    }
    const auto entries = QDir(info.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    const QRegularExpression frame("^frame_[0-9]{8}\\.(png|jpg)$");
    for (const auto& file : entries) {
        if (!file.isFile() || file.isSymLink() || !frame.match(file.fileName()).hasMatch()) {
            error = tr("Esta pasta contém outros arquivos. Escolha uma pasta vazia ou uma sequência criada pelo FrameScale.");
            return false;
        }
    }
    return true;
}

bool ProcessingJob::copyOrCommitOutput(const QString& source, QString& error)
{
    QString destination = fromPath(options_.outputPath);
    if (QFileInfo::exists(destination) && resolveOutputConflict) {
        const QString chosen = resolveOutputConflict(destination);
        if (chosen.isEmpty()) {
            error = tr("Exportação cancelada. O arquivo existente foi preservado.");
            return false;
        }
        destination = chosen;
#ifdef _WIN32
        options_.outputPath = std::filesystem::path(chosen.toStdWString());
#else
        options_.outputPath = std::filesystem::u8path(chosen.toUtf8().constData());
#endif
    }
    if (imageSequence(options_)) {
        if (!validateSequenceDestination(destination, error))
            return false;
        if (QFileInfo(destination).isSymLink() || (QFileInfo::exists(destination) && !QFileInfo(destination).isDir())) {
            error = tr("O destino da sequência deve ser uma pasta normal.");
            return false;
        }
        const bool exists = QFileInfo::exists(destination);
        if (exists && !resolveOutputConflict && (!confirmOverwrite || !confirmOverwrite(destination))) {
            error = tr("Substituição recusada. A pasta existente foi preservada.");
            return false;
        }
        const QString backup = backupOutputPath();
        if (exists && !QDir().rename(destination, backup)) {
            error = tr("Não foi possível preparar a substituição da pasta.");
            return false;
        }
        if (!QDir().rename(source, destination)) {
            if (exists && !QDir().rename(backup, destination))
                error = tr("Não foi possível restaurar a pasta anterior. A cópia permanece em %1.").arg(backup);
            else
                error = tr("Não foi possível guardar a sequência em %1.").arg(destination);
            return false;
        }
        if (exists)
            QThreadPool::globalInstance()->start([backup] { QDir(backup).removeRecursively(); });
        return true;
    }
    if (!QFileInfo::exists(destination)) {
        if (QFile::rename(source, destination)) {
            return true;
        }
        const QString staged = stagedOutputPath();
        if (!QFile::copy(source, staged) || !QFile::rename(staged, destination)) {
            QFile::remove(staged);
            error = tr("Não foi possível promover o resultado para %1.").arg(destination);
            return false;
        }
        QFile::remove(source);
        return true;
    }

    const QString staged = stagedOutputPath();
    if (!resolveOutputConflict && confirmOverwrite && !confirmOverwrite(destination)) {
        error = tr("Substituição recusada. O arquivo existente foi preservado.");
        return false;
    }
    const QString backup = backupOutputPath();
    if (!QFile::rename(source, staged)) {
        error = tr("Não foi possível preparar o resultado final em %1.").arg(staged);
        return false;
    }
    if (!QFile::rename(destination, backup)) {
        QFile::rename(staged, source);
        error = tr("Não foi possível substituir o ficheiro existente: %1.").arg(destination);
        return false;
    }
    if (!QFile::rename(staged, destination)) {
        if (!QFile::rename(backup, destination)) {
            error = tr("Não foi possível promover o resultado nem restaurar o ficheiro anterior. A cópia anterior permanece em %1.")
                        .arg(backup);
            return false;
        }
        QFile::rename(staged, source);
        error = tr("Não foi possível promover o resultado para %1.").arg(destination);
        return false;
    }

    QFile::remove(backup);
    return true;
}

} // namespace framescale
