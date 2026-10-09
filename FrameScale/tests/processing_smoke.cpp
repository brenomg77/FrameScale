#include "core/ProcessingOptions.h"
#include "processing/ProcessingJob.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QThread>
#include <QVector>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace framescale {

// Observe a queued verification without introducing timing assumptions or a
// public testing hook in the processing API.
struct ProcessingJobTestAccess {
    static bool partialRegression() {
        QTemporaryDir directory;
        ProcessingOptions options;
        options.mediaType=MediaType::Video;
        options.outputPath=std::filesystem::u8path((directory.path()+"/result.mp4").toUtf8().constData());
        ProcessingJob job(options);
        job.preservePartialOnCancel=true;
        job.running_=true; job.cancelling_=true;
        job.finalFramesDirectory_=directory.path()+"/future";
        job.outputTemporaryPath_=directory.path()+"/partial.mp4";
        job.videoOutputContract_.frameCount=10; job.videoOutputContract_.fps=25;
        QDir().mkpath(directory.path()+"/frames");
        QImage frame(32,32,QImage::Format_RGB32); frame.fill(Qt::red);
        for(int i=1;i<=3;++i) frame.save(directory.path()+QString("/frames/%1.png").arg(i,8,10,QChar('0')));
        job.steps_.resize(1); job.stepIndex_=0;
        job.steps_[0].progressFrameCount=10;
        job.steps_[0].progressFramesDirectory=directory.path()+"/frames";
        job.recoveryEncodeArguments_={"-v","error","-y","-framerate","25","-i",job.finalFramesDirectory_+"/%08d.png","-frames:v","10","-c:v","libx264","-pix_fmt","yuv420p","-t","0.4",job.outputTemporaryPath_};
        QEventLoop loop;
        QObject::connect(&job,&ProcessingJob::cancelled,&loop,&QEventLoop::quit);
        QTimer::singleShot(15000,&loop,&QEventLoop::quit);
        job.finishCancellation(); loop.exec();
        if(job.savedPartialPath_.isEmpty()) return false;
        QProcess probe;
        probe.start(job.toolPath("ffprobe"),{"-v","error","-count_frames","-select_streams","v:0","-show_entries","stream=nb_read_frames","-of","csv=p=0",job.savedPartialPath_});
        return probe.waitForFinished(10000) && probe.readAllStandardOutput().trimmed()=="3";
    }
    static bool pauseRegression() {
        ProcessingJob job(ProcessingOptions{});
        job.process_.disconnect(&job);
        job.process_.start(QCoreApplication::applicationFilePath(),{"--pause-heartbeat"});
        if(!job.process_.waitForStarted(5000) || !job.process_.waitForReadyRead(5000)) return false;
        job.running_=true;
        if(!job.setPaused(true)) return false;
        job.process_.readAllStandardOutput();
        const bool stopped=!job.process_.waitForReadyRead(250);
        if(!job.setPaused(false)) return false;
        const bool resumed=job.process_.waitForReadyRead(5000);
        const bool pausedAgain=job.setPaused(true);
        job.cancel();
        job.process_.kill(); job.process_.waitForFinished(5000);
        return stopped && resumed && pausedAgain && !job.isPaused();
    }
    static std::function<bool()> pendingFinalVerification(const ProcessingJob& job)
    {
        const auto result = job.verification_;
        if (job.stepIndex_ != static_cast<int>(job.steps_.size()) - 1
            || !result || result->complete.load(std::memory_order_acquire))
            return {};
        return [result] {
            return result->complete.load(std::memory_order_acquire) && result->valid;
        };
    }
};

} // namespace framescale

namespace {

QString argument(const int index, char* argv[])
{
#ifdef _WIN32
    return QCoreApplication::arguments().value(index);
#else
    return QString::fromLocal8Bit(argv[index]);
#endif
}

std::filesystem::path toPath(const QString& path)
{
#ifdef _WIN32
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toStdString());
#endif
}

QString executableName(const QString& name)
{
#ifdef _WIN32
    return name + QStringLiteral(".exe");
#else
    return name;
#endif
}

bool writeFile(const QString& path, const QByteArray& contents = QByteArrayLiteral("model"))
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

bool copyFakeTool(const QString& source, const QString& root, const QString& name)
{
    const QString destination = QDir(root).filePath(
        QStringLiteral("runtime/bin/") + executableName(name));
    QDir().mkpath(QFileInfo(destination).absolutePath());
    return QFile::copy(source, destination);
}

bool createFakeRoot(const QString& root, const QString& fakeTool)
{
    const QStringList tools {
        QStringLiteral("ffmpeg"),
        QStringLiteral("ffprobe"),
        QStringLiteral("realesrgan-ncnn-vulkan"),
        QStringLiteral("realcugan-ncnn-vulkan"),
        QStringLiteral("rife-ncnn-vulkan"),
        QStringLiteral("mpv")
    };
    for (const QString& tool : tools) {
        if (!copyFakeTool(fakeTool, root, tool)) {
            return false;
        }
    }

    const QString models = QDir(root).filePath(QStringLiteral("models"));
    const QStringList pairs {
        QStringLiteral("realesrgan/realesr-animevideov3-x2"),
        QStringLiteral("realcugan/models-se/up2x-denoise2x"),
        QStringLiteral("rife/rife-v4.6/flownet")
    };
    for (const QString& pair : pairs) {
        if (!writeFile(QDir(models).filePath(pair + QStringLiteral(".param")))
            || !writeFile(QDir(models).filePath(pair + QStringLiteral(".bin")))) {
            return false;
        }
    }
    return writeFile(QDir(models).filePath(
        QStringLiteral("libplacebo/anime4k-v4-a.glsl")),
        QByteArrayLiteral("//!HOOK MAIN\n"));
}

bool writePlan(const QString& root, const QJsonObject& plan)
{
    return writeFile(QDir(root).filePath(QStringLiteral("fake-plan.json")),
                     QJsonDocument(plan).toJson(QJsonDocument::Compact));
}

struct FrameProgress {
    QString stage;
    qint64 current;
    qint64 total;
};

struct ScenarioResult {
    int finished = 0;
    int failed = 0;
    int cancelled = 0;
    QString error;
    std::vector<int> progress;
    std::vector<FrameProgress> frameProgress;
    bool startReturned = false;
    bool terminalBeforeStartReturned = false;
};

ScenarioResult runExistingJob(
    framescale::ProcessingJob& job,
    const bool cancelDuringRun = false,
    const std::function<void(
        framescale::ProcessingJob&, const QString&)>& progressHook = {},
    const std::function<void(
        framescale::ProcessingJob&, const FrameProgress&)>& frameHook = {})
{
    ScenarioResult result;
    QString currentStage;
    QEventLoop loop;
    QObject::connect(&job, &framescale::ProcessingJob::progressChanged,
        &loop, [&result, &job, &progressHook, &currentStage](
            const int percent, const QString& stage) {
            result.progress.push_back(percent);
            currentStage = stage;
            if (progressHook) {
                progressHook(job, stage);
            }
        });
    QObject::connect(&job, &framescale::ProcessingJob::frameProgressChanged,
        &loop, [&result, &job, &currentStage, &frameHook](
            const qint64 current, const qint64 total) {
            const FrameProgress frame { currentStage, current, total };
            result.frameProgress.push_back(frame);
            if (frameHook) {
                frameHook(job, frame);
            }
        });
    QObject::connect(&job, &framescale::ProcessingJob::finished,
        &loop, [&result, &loop] {
            ++result.finished;
            result.terminalBeforeStartReturned = !result.startReturned;
            loop.quit();
        });
    QObject::connect(&job, &framescale::ProcessingJob::failed,
        &loop, [&result, &loop](const QString& message) {
            ++result.failed;
            result.error = message;
            result.terminalBeforeStartReturned = !result.startReturned;
            loop.quit();
        });
    QObject::connect(&job, &framescale::ProcessingJob::cancelled,
        &loop, [&result, &loop] {
            ++result.cancelled;
            result.terminalBeforeStartReturned = !result.startReturned;
            loop.quit();
        });
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout,
        &loop, [&result, &job, &loop] {
            result.error = QStringLiteral("watchdog timeout");
            job.cancel();
            loop.quit();
        });
    watchdog.start(20000);
    QTimer::singleShot(0, &loop, [&job, &result, cancelDuringRun] {
        job.start();
        result.startReturned = true;
        if (cancelDuringRun && job.isRunning()) {
            QTimer::singleShot(100, &job, &framescale::ProcessingJob::cancel);
        }
    });
    loop.exec();
    return result;
}

ScenarioResult runScenario(
    const framescale::ProcessingOptions& options,
    const QString& root,
    const bool cancelDuringRun = false)
{
    framescale::ProcessingJob job(options, framescale::RuntimePaths(root));
    return runExistingJob(job, cancelDuringRun);
}

framescale::ProcessingOptions imageOptions(const QString& input, const QString& output)
{
    framescale::ProcessingOptions options;
    options.inputPath = toPath(input);
    options.outputPath = toPath(output);
    options.mediaType = framescale::MediaType::Image;
    options.operation = framescale::Operation::Upscale;
    options.upscaleEngine = framescale::UpscaleEngine::RealESRGAN;
    options.upscaleModelId = "realesr-animevideov3";
    options.scaleFactor = 2;
    return options;
}

framescale::ProcessingOptions videoOptions(const QString& input, const QString& output)
{
    framescale::ProcessingOptions options;
    options.inputPath = toPath(input);
    options.outputPath = toPath(output);
    options.mediaType = framescale::MediaType::Video;
    options.operation = framescale::Operation::UpscaleAndInterpolation;
    options.upscaleEngine = framescale::UpscaleEngine::RealESRGAN;
    options.upscaleModelId = "realesr-animevideov3";
    options.rifeModelId = "rife-v4.6";
    options.scaleFactor = 2;
    options.targetFps = 8;
    return options;
}

bool monotonicProgress(const ScenarioResult& result)
{
    return !result.progress.empty()
        && std::is_sorted(result.progress.begin(), result.progress.end());
}

bool completeFrameProgress(const ScenarioResult& result)
{
    qint64 previous = 0;
    qint64 total = 0;
    QString stage;
    bool countedFrames = false;
    for (const FrameProgress& frame : result.frameProgress) {
        if (frame.stage != stage) {
            if (previous != total || frame.current != 0) {
                return false;
            }
            stage = frame.stage;
            previous = 0;
            total = frame.total;
        }
        if (frame.total != total || frame.current < previous || frame.current > previous + 1
            || frame.current > total || frame.current < 0) {
            return false;
        }
        previous = frame.current;
        countedFrames |= frame.current > 0;
    }
    return countedFrames && previous == total;
}

QVector<QJsonObject> readInvocations(const QString& root)
{
    QVector<QJsonObject> invocations;
    QFile file(QDir(root).filePath(QStringLiteral("invocations.jsonl")));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return invocations;
    }
    while (!file.atEnd()) {
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(
            file.readLine().trimmed(), &error);
        if (error.error == QJsonParseError::NoError && document.isObject()) {
            invocations.append(document.object());
        }
    }
    return invocations;
}

QString invocationTool(const QJsonObject& invocation)
{
    return invocation.value(QStringLiteral("tool")).toString();
}

QStringList invocationArguments(const QJsonObject& invocation)
{
    QStringList arguments;
    for (const QJsonValue& value
         : invocation.value(QStringLiteral("arguments")).toArray()) {
        arguments.append(value.toString());
    }
    return arguments;
}

int firstInvocation(const QVector<QJsonObject>& invocations,
                    const QString& tool)
{
    for (int index = 0; index < invocations.size(); ++index) {
        if (invocationTool(invocations[index]) == tool) {
            return index;
        }
    }
    return -1;
}

int runFakeSuite(const QString& fakeTool)
{
    int failures = 0;
    auto check = [&failures](const bool condition, const QString& message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << message.toStdString() << '\n';
        }
    };
    auto rootFor = [&fakeTool, &check](QTemporaryDir& temporary) {
        check(temporary.isValid(), QStringLiteral("temporary root is invalid"));
        check(createFakeRoot(temporary.path(), fakeTool),
              QStringLiteral("could not create fake runtime"));
    };

    {
        QTemporaryDir temporary(
            QDir::tempPath() + QStringLiteral("/FrameScale-model-resolution-XXXXXX"));
        check(temporary.isValid(), QStringLiteral("model-resolution root is invalid"));
        const QString working = QDir(temporary.path()).filePath(QStringLiteral("working"));
        check(QDir().mkpath(working), QStringLiteral("could not create fallback working directory"));
        const QString uniqueDirectory = QStringLiteral("resolution-")
            + QFileInfo(temporary.path()).fileName();
        const QString modelPrefix = uniqueDirectory
            + QStringLiteral("/realesr-animevideov3");
        const QString localBase = QDir(working).filePath(
            QStringLiteral("models/") + modelPrefix + QStringLiteral("-x2"));
        const QString parentBase = QDir(temporary.path()).filePath(
            QStringLiteral("models/") + modelPrefix + QStringLiteral("-x3"));
        const QString incompleteBase = QDir(working).filePath(
            QStringLiteral("models/") + modelPrefix + QStringLiteral("-x4"));
        const QString completeFallback = QDir(temporary.path()).filePath(
            QStringLiteral("models/") + modelPrefix + QStringLiteral("-x4"));
        for (const QString& base : { localBase, parentBase, completeFallback }) {
            check(writeFile(base + QStringLiteral(".param"))
                      && writeFile(base + QStringLiteral(".bin")),
                  QStringLiteral("could not create model-resolution pair"));
        }
        check(writeFile(incompleteBase + QStringLiteral(".param")),
              QStringLiteral("could not create incomplete nearer model"));
        const QString originalCurrent = QDir::currentPath();
        check(QDir::setCurrent(working), QStringLiteral("could not select fallback working directory"));
        const framescale::RuntimePaths paths;
        check(paths.ncnnModelBase(toPath(modelPrefix + QStringLiteral("-x2"))) == localBase,
              QStringLiteral("NCNN model did not resolve its concrete parameter file from cwd"));
        check(paths.ncnnModelBase(toPath(modelPrefix + QStringLiteral("-x3"))) == parentBase,
              QStringLiteral("NCNN model did not resolve the requested scale from cwd parent"));
        const QString selectedIncomplete = paths.ncnnModelBase(
            toPath(modelPrefix + QStringLiteral("-x4")));
        check(selectedIncomplete == incompleteBase
                  && !QFileInfo::exists(selectedIncomplete + QStringLiteral(".bin")),
              QStringLiteral("NCNN resolution mixed model files from separate roots"));
        check(QDir::setCurrent(originalCurrent),
              QStringLiteral("could not restore working directory after model resolution"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-image-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada ç漢字.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("saída ç漢字.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::red);
        check(image.save(input), QStringLiteral("could not create image input"));
        check(writePlan(temporary.path(), {}), QStringLiteral("could not write image plan"));
        auto options = imageOptions(input, output);
        options.keepMetadata = false;
        const ScenarioResult result = runScenario(options, temporary.path());
        bool restoredAlpha = false;
        for (const auto& invocation : readInvocations(temporary.path())) {
            const auto args = invocationArguments(invocation);
            const QString filter = args.value(args.indexOf("-filter_complex") + 1);
            if (filter.contains("alphaextract") && filter.contains("alphamerge")) {
                restoredAlpha = args.count("-i") == 2 && args.contains("[out]");
            }
        }
        check(restoredAlpha, QStringLiteral("image export must restore source alpha even without metadata"));
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("image scenario did not finish: ") + result.error);
        check(!result.terminalBeforeStartReturned,
              QStringLiteral("processing completed synchronously inside start()"));
        check(QImage(output).size() == QSize(32, 24),
              QStringLiteral("image result has wrong dimensions"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("successful image left commit artifacts"));
        check(monotonicProgress(result) && result.progress.back() == 100,
              QStringLiteral("image progress contract failed"));
        check(completeFrameProgress(result),
              QStringLiteral("image frame progress did not reset and finish each stage"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-webp-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.webp"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::red);
        check(image.save(input), QStringLiteral("could not create opaque-format input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("opaqueImageEncoding"), true } }),
              QStringLiteral("could not write opaque-format plan"));
        const ScenarioResult result = runScenario(imageOptions(input, output), temporary.path());
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("image validation depended on an optional Qt decoder: ") + result.error);
        check(QImage(output).isNull() && QFileInfo(output).size() > 0,
              QStringLiteral("opaque-format fixture did not exercise missing Qt decoding"));
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        check(invocations.size() == 4,
              QStringLiteral("image pipeline did not run encode and independent decode"));
        if (invocations.size() == 4) {
            const QStringList encoded = invocationArguments(invocations[2]);
            const QStringList decoded = invocationArguments(invocations[3]);
            check(encoded.value(encoded.indexOf(QStringLiteral("-c:v")) + 1)
                      == QStringLiteral("libwebp"),
                  QStringLiteral("WebP output did not select its encoder"));
            check(decoded.value(decoded.indexOf(QStringLiteral("-i")) + 1) == encoded.last()
                      && decoded.contains(QStringLiteral("-xerror")),
                  QStringLiteral("image validation did not decode the encoded partial"));
            // Workspace removal is deliberately asynchronous to keep the GUI responsive.
            check(QThreadPool::globalInstance()->waitForDone(5000),
                  QStringLiteral("image workspace cleanup did not finish"));
            check(!QFileInfo::exists(QFileInfo(decoded.last()).absolutePath()),
                  QStringLiteral("successful image validation left its temporary PNG workspace"));
        }
        check(monotonicProgress(result) && result.progress.back() == 100,
              QStringLiteral("opaque-format image progress contract failed"));
    }

    for (const auto& fault : {
             qMakePair(QStringLiteral("emptyImageEncodeOutput"), QJsonValue(true)),
             qMakePair(QStringLiteral("imageDecodeFailure"), QJsonValue(true)),
             qMakePair(QStringLiteral("imageDecodeDiagnostics"), QJsonValue(true)),
             qMakePair(QStringLiteral("omitImageDecodeOutput"), QJsonValue(true)),
             qMakePair(QStringLiteral("imageDecodeWidth"), QJsonValue(31)) }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-image-validation-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::green);
        check(image.save(input), QStringLiteral("could not create image-validation input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed image-validation destination"));
        check(writePlan(temporary.path(), QJsonObject { { fault.first, fault.second } }),
              QStringLiteral("could not write image-validation failure plan"));
        const ScenarioResult result = runScenario(imageOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0 && result.cancelled == 0,
              QStringLiteral("image validation accepted ") + fault.first);
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        check(invocations.size()
                  == (fault.first == QStringLiteral("emptyImageEncodeOutput") ? 3 : 4),
              QStringLiteral("image failure occurred at the wrong pipeline stage: ") + fault.first);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly)
                  && destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("image validation replaced destination after ") + fault.first);
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("image validation failure left commit artifacts: ") + fault.first);
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-image-validation-cancel-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        check(image.save(input), QStringLiteral("could not create validation-cancel input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed validation-cancel destination"));
        check(writePlan(temporary.path(), {}),
              QStringLiteral("could not write validation-cancel plan"));
        framescale::ProcessingJob job(
            imageOptions(input, output), framescale::RuntimePaths(temporary.path()));
        const ScenarioResult result = runExistingJob(
            job, false, [](framescale::ProcessingJob& currentJob, const QString& stage) {
                if (stage == QStringLiteral("Validando imagem codificada")) {
                    currentJob.cancel();
                }
            });
        check(result.cancelled == 1 && result.finished == 0 && result.failed == 0,
              QStringLiteral("image-validation cancellation failed: ") + result.error);
        check(readInvocations(temporary.path()).size() == 3,
              QStringLiteral("image decoder started after synchronous cancellation"));
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly)
                  && destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("image-validation cancellation replaced destination"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("image-validation cancellation left commit artifacts"));
    }

    for (const bool cancelFirst : { false, true }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-queued-image-check-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage source(16, 12, QImage::Format_RGBA8888);
        source.fill(Qt::green);
        check(source.save(input) && writeFile(output, "previous") && writePlan(temporary.path(), {}),
              QStringLiteral("could not prepare queued verification test"));
        QThreadPool* pool = QThreadPool::globalInstance();
        check(pool->waitForDone(5000), QStringLiteral("previous workers did not finish"));
        const int originalThreads = pool->maxThreadCount();
        pool->setMaxThreadCount(1);
        auto gate = std::make_shared<QSemaphore>();
        auto entered = std::make_shared<QSemaphore>();
        bool blocked = false;
        bool observedPending = false;
        int finished = 0, failed = 0, cancelled = 0;
        std::function<bool()> verificationSucceeded;
        QEventLoop loop;
        auto job = std::make_unique<framescale::ProcessingJob>(
            imageOptions(input, output), framescale::RuntimePaths(temporary.path()));
        QObject::connect(job.get(), &framescale::ProcessingJob::finished, &loop, [&] {
            ++finished;
            loop.quit();
        });
        QObject::connect(job.get(), &framescale::ProcessingJob::failed, &loop, [&] {
            ++failed;
            loop.quit();
        });
        QObject::connect(job.get(), &framescale::ProcessingJob::cancelled, &loop, [&] { ++cancelled; });
        QObject::connect(job.get(), &framescale::ProcessingJob::progressChanged,
            &loop, [&](int, const QString& stage) {
                if (blocked || stage != QStringLiteral("Validando imagem codificada"))
                    return;
                blocked = true;
                // Previous image checks have finished. Hold the sole worker so
                // the final decoded-image check is guaranteed to remain queued.
                pool->start([gate, entered] { entered->release(); gate->acquire(); });
                check(entered->tryAcquire(1, 5000), QStringLiteral("worker gate did not start"));
            });
        QTimer poll;
        poll.setInterval(10);
        QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
            if (!blocked || !job)
                return;
            verificationSucceeded = framescale::ProcessingJobTestAccess::pendingFinalVerification(*job);
            if (!verificationSucceeded)
                return;
            observedPending = true;
            if (cancelFirst)
                job->cancel();
            job.reset();
            loop.quit();
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        watchdog.start(10000);
        poll.start();
        QTimer::singleShot(0, &loop, [&] { job->start(); });
        loop.exec();
        poll.stop();
        job.reset();
        gate->release();
        check(pool->waitForDone(5000), QStringLiteral("queued image verification did not drain"));
        pool->setMaxThreadCount(originalThreads);
        QCoreApplication::processEvents();
        check(observedPending, QStringLiteral("test did not reach a pending final image verification"));
        check(verificationSucceeded && verificationSucceeded(),
              QStringLiteral("image verification lost its immutable data after job destruction"));
        check(finished == 0 && failed == 0 && cancelled == (cancelFirst ? 1 : 0),
              QStringLiteral("queued image verification emitted a late terminal signal"));
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly) && destination.readAll() == "previous",
              QStringLiteral("queued image verification replaced an existing destination"));
        check(QDir(temporary.path()).entryList({ "*.part.*", "*.backup", "*.staged" }, QDir::Files).isEmpty(),
              QStringLiteral("queued image verification left output artifacts"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-image-replace-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage inputImage(16, 12, QImage::Format_RGBA8888);
        inputImage.fill(Qt::red);
        QImage previous(4, 4, QImage::Format_RGBA8888);
        previous.fill(Qt::black);
        check(inputImage.save(input), QStringLiteral("could not create replacement input"));
        check(previous.save(output), QStringLiteral("could not seed replacement output"));
        check(writePlan(temporary.path(), {}), QStringLiteral("could not write replacement plan"));
        const ScenarioResult result = runScenario(imageOptions(input, output), temporary.path());
        check(result.finished == 1 && QImage(output).size() == QSize(32, 24),
              QStringLiteral("successful image did not replace destination"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("image replacement left commit artifacts"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-cugan-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::yellow);
        check(image.save(input), QStringLiteral("could not create Real-CUGAN input"));
        check(writePlan(temporary.path(), {}), QStringLiteral("could not write Real-CUGAN plan"));
        auto options = imageOptions(input, output);
        options.upscaleEngine = framescale::UpscaleEngine::RealCUGAN;
        options.upscaleModelId = "realcugan-se";
        options.denoise = framescale::DenoiseLevel::Level2;
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("Real-CUGAN scenario did not finish: ") + result.error);
        check(QImage(output).size() == QSize(32, 24),
              QStringLiteral("Real-CUGAN result has wrong dimensions"));
        check(monotonicProgress(result) && result.progress.back() == 100,
              QStringLiteral("Real-CUGAN progress contract failed"));
        QFile logFile(QDir(temporary.path()).filePath(QStringLiteral("invocations.jsonl")));
        check(logFile.open(QIODevice::ReadOnly),
              QStringLiteral("could not read Real-CUGAN invocation log"));
        const QByteArray log = logFile.readAll();
        check(log.contains("realcugan-ncnn-vulkan") && log.contains("\"-n\",\"2\""),
              QStringLiteral("Real-CUGAN command branch was not observed"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-anime4k-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::cyan);
        check(image.save(input), QStringLiteral("could not create Anime4K input"));
        check(writePlan(temporary.path(), {}), QStringLiteral("could not write Anime4K plan"));
        auto options = imageOptions(input, output);
        options.upscaleEngine = framescale::UpscaleEngine::Anime4K;
        options.upscaleModelId = "anime4k-v4-a";
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("Anime4K scenario did not finish: ") + result.error);
        check(QImage(output).size() == QSize(32, 24),
              QStringLiteral("Anime4K result has wrong dimensions"));
        check(monotonicProgress(result) && result.progress.back() == 100,
              QStringLiteral("Anime4K progress contract failed"));
        QFile logFile(QDir(temporary.path()).filePath(QStringLiteral("invocations.jsonl")));
        check(logFile.open(QIODevice::ReadOnly),
              QStringLiteral("could not read Anime4K invocation log"));
        const QByteArray log = logFile.readAll();
        check(log.contains("mpv") && log.contains("--glsl-shader="),
              QStringLiteral("Anime4K command branch was not observed"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-video-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input, QByteArrayLiteral("source")), QStringLiteral("could not create video input"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
            { QStringLiteral("audioCodecs"), QJsonArray { QStringLiteral("aac"), QStringLiteral("mp3") } }
        };
        check(writePlan(temporary.path(), plan), QStringLiteral("could not write video plan"));
        const ScenarioResult result = runScenario(videoOptions(input, output), temporary.path());
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("video scenario did not finish: ") + result.error);
        check(QFileInfo(output).size() > 0, QStringLiteral("video output was not committed"));
        check(monotonicProgress(result) && result.progress.back() == 100,
              QStringLiteral("video progress contract failed"));
        check(completeFrameProgress(result),
              QStringLiteral("video frame progress did not reset and finish each stage"));
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        const int rife = firstInvocation(invocations, QStringLiteral("rife-ncnn-vulkan"));
        const int upscale = firstInvocation(invocations, QStringLiteral("realesrgan-ncnn-vulkan"));
        check(rife >= 0 && upscale > rife,
              QStringLiteral("combined pipeline did not run RIFE before upscale"));
        int encode = -1;
        int finalProbe = -1;
        int finalDecode = -1;
        for (int index = 0; index < invocations.size(); ++index) {
            const QStringList arguments = invocationArguments(invocations[index]);
            if (invocationTool(invocations[index]) == QStringLiteral("ffmpeg")
                && arguments.last().endsWith(QStringLiteral("%08d.png"))
                && arguments.contains(QStringLiteral("-vf"))) {
                const QString filter = arguments.value(arguments.indexOf(QStringLiteral("-vf")) + 1);
                check(filter.endsWith(QStringLiteral("format=rgb24")),
                    QStringLiteral("RIFE input must be RGB24 to avoid RGBA channel corruption"));
            }
            if (invocationTool(invocations[index]) == QStringLiteral("ffmpeg")
                && arguments.contains(QStringLiteral("libx264"))) {
                encode = index;
                check(arguments.contains(QStringLiteral("-crf"))
                          && arguments.contains(QStringLiteral("18"))
                          && arguments.contains(QStringLiteral("-pix_fmt"))
                          && arguments.contains(QStringLiteral("yuv420p"))
                          && arguments.contains(QStringLiteral("-f"))
                          && arguments.contains(QStringLiteral("mp4")),
                      QStringLiteral("video encode contract arguments are incomplete"));
            } else if (invocationTool(invocations[index]) == QStringLiteral("ffprobe")
                       && arguments.last().contains(QStringLiteral(".part."))) {
                finalProbe = index;
                check(!arguments.contains(QStringLiteral("-count_frames")),
                      QStringLiteral("final metadata probe must not decode the export twice"));
            } else if (invocationTool(invocations[index]) == QStringLiteral("ffmpeg")
                       && arguments.last() == QStringLiteral("-")) {
                finalDecode = index;
                check(arguments.contains(QStringLiteral("-xerror"))
                          && arguments.contains(QStringLiteral("explode"))
                          && arguments.contains(QStringLiteral("0:v:0"))
                          && arguments.contains(QStringLiteral("0:a?")),
                      QStringLiteral("full-decode validation arguments are incomplete"));
            }
        }
        check(encode > upscale && finalProbe > encode && finalDecode > finalProbe,
              QStringLiteral("encode/probe/decode order contract failed"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("successful video left commit artifacts"));
    }

    for (const QString& engine : {
             QStringLiteral("esrgan"), QStringLiteral("cugan"),
             QStringLiteral("anime4k"), QStringLiteral("rife") }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-frame-progress-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create frame-progress input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("frames"), 2 }, { QStringLiteral("fps"), 4.0 },
                  { QStringLiteral("slowFrameWrites"), true } }),
              QStringLiteral("could not write frame-progress plan"));
        auto options = videoOptions(input, output);
        options.operation = engine == QStringLiteral("rife")
            ? framescale::Operation::Interpolation : framescale::Operation::Upscale;
        if (engine == QStringLiteral("cugan")) {
            options.upscaleEngine = framescale::UpscaleEngine::RealCUGAN;
            options.upscaleModelId = "realcugan-se";
            options.denoise = framescale::DenoiseLevel::Level2;
        } else if (engine == QStringLiteral("anime4k")) {
            options.upscaleEngine = framescale::UpscaleEngine::Anime4K;
            options.upscaleModelId = "anime4k-v4-a";
        }
        framescale::ProcessingJob job(options, framescale::RuntimePaths(temporary.path()));
        bool sawIntermediateFrames = false;
        const ScenarioResult result = runExistingJob(job, false, {},
            [&](framescale::ProcessingJob&, const FrameProgress& frame) {
                if ((!frame.stage.startsWith(QStringLiteral("Aplicando"))
                     && !frame.stage.startsWith(QStringLiteral("Interpolando")))
                    || frame.current == 0) {
                    return;
                }
                sawIntermediateFrames |= frame.current < frame.total;
                const auto invocations = readInvocations(temporary.path());
                if (invocations.isEmpty()) {
                    check(false, QStringLiteral("missing engine invocation during frame progress"));
                    return;
                }
                const QStringList arguments = invocationArguments(invocations.back());
                QString directory = arguments.value(arguments.indexOf(QStringLiteral("-o")) + 1);
                for (const QString& argument : arguments) {
                    if (argument.startsWith(QStringLiteral("--o="))) {
                        directory = QFileInfo(argument.mid(4)).absolutePath();
                    }
                }
                qint64 completePngs = 0;
                for (const QString& name : QDir(directory).entryList(
                         { QStringLiteral("*.png") }, QDir::Files)) {
                    QFile file(QDir(directory).filePath(name));
                    if (file.open(QIODevice::ReadOnly) && file.size() >= 12
                        && file.seek(file.size() - 12)
                        && file.read(12) == QByteArray::fromHex("0000000049454e44ae426082")) {
                        ++completePngs;
                    }
                }
                check(frame.current <= completePngs,
                      engine + QStringLiteral(" counted a partially written PNG as complete"));
            });
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              engine + QStringLiteral(" frame-progress scenario failed: ") + result.error);
        check(sawIntermediateFrames && completeFrameProgress(result),
              engine + QStringLiteral(" did not report monotonic intermediate completed frames"));
    }

    for (int cancelPoint = 0; cancelPoint < 3; ++cancelPoint) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-frame-cancel-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input) && writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed frame-cancel files"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("frames"), 2 }, { QStringLiteral("fps"), 4.0 },
                  { QStringLiteral("slowFrameWrites"), cancelPoint == 1 } }),
              QStringLiteral("could not write frame-cancel plan"));
        auto options = videoOptions(input, output);
        options.operation = framescale::Operation::Upscale;
        framescale::ProcessingJob job(options, framescale::RuntimePaths(temporary.path()));
        bool requestedCancellation = false;
        const ScenarioResult result = runExistingJob(job, false, {},
            [&](framescale::ProcessingJob& currentJob, const FrameProgress& frame) {
                const bool engine = frame.stage.startsWith(QStringLiteral("Aplicando"));
                const bool finalDecode = frame.stage.startsWith(QStringLiteral("Verificando decodificação"));
                if (!requestedCancellation
                    && ((cancelPoint == 0 && engine && frame.current == 0)
                        || (cancelPoint == 1 && engine && frame.current == 1)
                        || (cancelPoint == 2 && finalDecode
                            && frame.current == frame.total && frame.total > 0))) {
                    requestedCancellation = true;
                    currentJob.cancel();
                }
            });
        check(requestedCancellation && result.cancelled == 1
                  && result.finished == 0 && result.failed == 0,
              QStringLiteral("synchronous frame cancellation failed: ") + result.error);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly)
                  && destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("frame callback cancellation replaced destination"));
        if (cancelPoint == 0) {
            check(firstInvocation(readInvocations(temporary.path()),
                                  QStringLiteral("realesrgan-ncnn-vulkan")) < 0,
                  QStringLiteral("engine started after cancellation at frame reset"));
        }
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("frame cancellation left commit artifacts"));
    }

    for (const auto engine : {
             framescale::UpscaleEngine::RealCUGAN,
             framescale::UpscaleEngine::Anime4K }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-video-engine-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create video-engine input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 } }),
              QStringLiteral("could not write video-engine plan"));
        auto options = videoOptions(input, output);
        options.upscaleEngine = engine;
        if (engine == framescale::UpscaleEngine::RealCUGAN) {
            options.upscaleModelId = "realcugan-se";
            options.denoise = framescale::DenoiseLevel::Level2;
        } else {
            options.upscaleModelId = "anime4k-v4-a";
        }
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.finished == 1 && result.failed == 0,
              QStringLiteral("video-engine scenario failed: ") + result.error);
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        const int rife = firstInvocation(invocations, QStringLiteral("rife-ncnn-vulkan"));
        const QString tool = engine == framescale::UpscaleEngine::RealCUGAN
            ? QStringLiteral("realcugan-ncnn-vulkan") : QStringLiteral("mpv");
        check(rife >= 0 && firstInvocation(invocations, tool) > rife,
              QStringLiteral("video engine did not run after RIFE: ") + tool);
    }

    for (const auto operation : {
             framescale::Operation::Upscale,
             framescale::Operation::Interpolation }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-video-operation-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create video-operation input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 } }),
              QStringLiteral("could not write video-operation plan"));
        auto options = videoOptions(input, output);
        options.operation = operation;
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.finished == 1 && result.failed == 0,
              QStringLiteral("standalone video operation failed: ") + result.error);
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        const bool sawRife = firstInvocation(
            invocations, QStringLiteral("rife-ncnn-vulkan")) >= 0;
        const bool sawUpscale = firstInvocation(
            invocations, QStringLiteral("realesrgan-ncnn-vulkan")) >= 0;
        check(sawRife == (operation == framescale::Operation::Interpolation)
                  && sawUpscale == (operation == framescale::Operation::Upscale),
              QStringLiteral("standalone operation ran the wrong engine stages"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-rotation-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create rotated video input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
                  { QStringLiteral("rotation"), 90 } }),
              QStringLiteral("could not write rotated video plan"));
        auto options = videoOptions(input, output);
        options.operation = framescale::Operation::Upscale;
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.finished == 1 && result.failed == 0,
              QStringLiteral("rotated video scenario failed: ") + result.error);
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        int extraction = -1;
        int encode = -1;
        for (int index = 0; index < invocations.size(); ++index) {
            const QStringList arguments = invocationArguments(invocations[index]);
            if (invocationTool(invocations[index]) != QStringLiteral("ffmpeg")) {
                continue;
            }
            if (arguments.contains(QStringLiteral("%08d.png"))
                || arguments.last().contains(QStringLiteral("%08d.png"))) {
                extraction = index;
                check(!arguments.contains(QStringLiteral("-noautorotate")),
                      QStringLiteral("rotated extraction disabled autorotation"));
            } else if (arguments.contains(QStringLiteral("libx264"))) {
                encode = index;
            }
        }
        check(extraction >= 0 && encode > extraction,
              QStringLiteral("rotated video did not complete extraction and encode"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-aac-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create AAC input"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
            { QStringLiteral("audioCodecs"), QJsonArray {
                  QStringLiteral("aac"), QStringLiteral("mp3") } },
            // A nominally compatible stream can still be rejected by the muxer.
            { QStringLiteral("rejectAudioCopy"), true }
        };
        check(writePlan(temporary.path(), plan), QStringLiteral("could not write AAC plan"));
        const ScenarioResult result = runScenario(videoOptions(input, output), temporary.path());
        check(result.finished == 1 && result.failed == 0,
              QStringLiteral("AAC retry scenario failed: ") + result.error);
        const QByteArray log = [&] {
            QFile file(QDir(temporary.path()).filePath(QStringLiteral("invocations.jsonl")));
            if (!file.open(QIODevice::ReadOnly)) {
                return QByteArray();
            }
            return file.readAll();
        }();
        check(log.contains("copy") && log.contains("192k"),
              QStringLiteral("AAC retry command was not observed"));
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        int copyEncode = -1;
        int aacEncode = -1;
        for (int index = 0; index < invocations.size(); ++index) {
            const QStringList arguments = invocationArguments(invocations[index]);
            if (invocationTool(invocations[index]) != QStringLiteral("ffmpeg")
                || !arguments.contains(QStringLiteral("libx264"))) {
                continue;
            }
            const int codec = arguments.indexOf(QStringLiteral("-c:a"));
            if (codec >= 0 && arguments.value(codec + 1) == QStringLiteral("copy")) {
                copyEncode = index;
            } else if (codec >= 0
                       && arguments.value(codec + 1) == QStringLiteral("aac")) {
                aacEncode = index;
                check(arguments.value(codec + 2) == QStringLiteral("-b:a")
                          && arguments.value(codec + 3) == QStringLiteral("192k"),
                      QStringLiteral("AAC retry bitrate contract is incomplete"));
            }
        }
        check(copyEncode >= 0 && aacEncode > copyEncode,
              QStringLiteral("AAC retry did not follow the copy attempt"));
    }

    for (const auto& fault : {
             qMakePair(QStringLiteral("omitFrames"), QJsonValue(true)),
             qMakePair(QStringLiteral("finalProbeFailure"), QJsonValue(true)),
             qMakePair(QStringLiteral("malformedFinalProbe"), QJsonValue(true)),
             qMakePair(QStringLiteral("formatName"), QJsonValue(QStringLiteral("matroska,webm"))),
             qMakePair(QStringLiteral("unexpectedStream"), QJsonValue(true)),
             qMakePair(QStringLiteral("decodeFailure"), QJsonValue(true)),
             qMakePair(QStringLiteral("decodeWarning"), QJsonValue(true)),
             qMakePair(QStringLiteral("decodedFrames"), QJsonValue(7)),
             qMakePair(QStringLiteral("finalCodec"), QJsonValue(QStringLiteral("hevc"))),
             qMakePair(QStringLiteral("finalPixelFormat"), QJsonValue(QStringLiteral("yuv444p"))),
             qMakePair(QStringLiteral("finalWidth"), QJsonValue(31)),
             qMakePair(QStringLiteral("finalHeight"), QJsonValue(23)),
             qMakePair(QStringLiteral("finalFrames"), QJsonValue(7)),
             qMakePair(QStringLiteral("finalFps"), QJsonValue(QStringLiteral("9/1"))),
             qMakePair(QStringLiteral("finalDuration"), QJsonValue(QStringLiteral("0"))) }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-failure-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create failure input"));
        check(writeFile(output, QByteArrayLiteral("previous")), QStringLiteral("could not seed destination"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 }
        };
        plan.insert(fault.first, fault.second);
        check(writePlan(temporary.path(), plan), QStringLiteral("could not write failure plan"));
        const ScenarioResult result = runScenario(videoOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0,
              QStringLiteral("fault was not rejected: ") + fault.first);
        const QVector<QJsonObject> invocations = readInvocations(temporary.path());
        bool sawFinalDecode = false;
        for (const QJsonObject& invocation : invocations) {
            sawFinalDecode = sawFinalDecode
                || (invocationTool(invocation) == QStringLiteral("ffmpeg")
                    && invocationArguments(invocation).contains(QStringLiteral("-xerror")));
        }
        check(sawFinalDecode == (fault.first == QStringLiteral("decodeFailure")
                  || fault.first == QStringLiteral("decodeWarning")
                  || fault.first == QStringLiteral("decodedFrames")),
              QStringLiteral("pipeline continued after failed validation: ") + fault.first);
        if (fault.first == QStringLiteral("omitFrames")) {
            check(invocations.size() == 2,
                  QStringLiteral("missing extracted frames did not stop before the engines"));
        }
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly),
              QStringLiteral("could not reopen existing destination"));
        check(destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("fault replaced existing destination: ") + fault.first);
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("fault left commit artifacts: ") + fault.first);
    }

    for (const auto& audioFault : {
             qMakePair(QJsonArray { QStringLiteral("aac") },
                       QStringLiteral("missing audio stream")),
             qMakePair(QJsonArray { QStringLiteral("opus"), QStringLiteral("mp3") },
                       QStringLiteral("changed copied audio codec")) }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-audio-contract-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create audio-contract input"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
            { QStringLiteral("audioCodecs"), QJsonArray {
                  QStringLiteral("aac"), QStringLiteral("mp3") } },
            { QStringLiteral("finalAudioCodecs"), audioFault.first }
        };
        check(writePlan(temporary.path(), plan),
              QStringLiteral("could not write audio-contract plan"));
        const ScenarioResult result = runScenario(
            videoOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0,
              QStringLiteral("final audio contract accepted ") + audioFault.second);
    }

    for (int timingCase = 0; timingCase < 5; ++timingCase) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-video-timeline-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mkv"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create video timing input"));
        QJsonObject plan {
            { "frames", 4 }, { "fps", 4.0 }, { "containerDuration", "3.0" },
            { "audioCodecs", QJsonArray { "aac" } }
        };
        if (timingCase == 0 || timingCase == 2 || timingCase == 4)
            plan.insert("sourceDuration", "N/A");
        if (timingCase == 1 || timingCase == 2 || timingCase == 4)
            plan.insert("sourceFrames", "N/A");
        if (timingCase == 3)
            plan.insert("sourceFps", "0/0");
        if (timingCase == 4) {
            plan.insert("timestamps", QJsonArray { 10.0, 10.1, 10.4, 10.7 });
            plan.insert("omitFrameDuration", true);
        }
        check(writePlan(temporary.path(), plan), QStringLiteral("could not write video timing plan"));
        const auto result = runScenario(videoOptions(input, output), temporary.path());
        check(result.finished == 1 && result.failed == 0 && result.cancelled == 0,
              QStringLiteral("video timeline fallback failed (%1): %2").arg(timingCase).arg(result.error));
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly), QStringLiteral("video timeline result is missing"));
        const auto contract = QJsonDocument::fromJson(destination.readAll()).object();
        check(contract.value("frames").toInt() == 8 && qAbs(contract.value("fps").toDouble() - 8.0) < 1e-9,
              QStringLiteral("video timeline used container duration instead of visual timing"));
        int timelineProbes = 0;
        for (const auto& invocation : readInvocations(temporary.path()))
            timelineProbes += invocationArguments(invocation).contains("-show_frames");
        check(timelineProbes == 1, QStringLiteral("video timing fallback did not run exactly once"));
    }

    for (const auto& sourceFault : {
             qMakePair(QStringLiteral("probeFailure"), QJsonValue(true)),
             qMakePair(QStringLiteral("malformedProbe"), QJsonValue(true)),
             qMakePair(QStringLiteral("missingVideoStream"), QJsonValue(true)),
             qMakePair(QStringLiteral("invalidTimeline"), QJsonValue(true)) }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-source-failure-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create source-failure input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed source-failure destination"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 }
        };
        plan.insert(sourceFault.first, sourceFault.second);
        if (sourceFault.first == QStringLiteral("invalidTimeline")) {
            plan.insert("sourceDuration", "N/A");
            plan.insert("sourceFrames", "N/A");
        }
        check(writePlan(temporary.path(), plan),
              QStringLiteral("could not write source-failure plan"));
        const ScenarioResult result = runScenario(
            videoOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0,
              QStringLiteral("source fault was not rejected: ") + sourceFault.first);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly),
              QStringLiteral("could not reopen source-failure destination"));
        check(destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("source fault replaced existing destination: ")
                  + sourceFault.first);
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-odd-video-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create odd-size video input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 15 }, { QStringLiteral("height"), 13 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 } }),
              QStringLiteral("could not write odd-size video plan"));
        auto options = videoOptions(input, output);
        options.operation = framescale::Operation::Interpolation;
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.failed == 1 && result.finished == 0
                  && result.error.contains(QStringLiteral("têm de ser pares")),
              QStringLiteral("odd H.264 yuv420p dimensions were not rejected"));
        check(readInvocations(temporary.path()).size() == 1,
              QStringLiteral("odd dimensions started processing after probe"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-one-frame-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create one-frame input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 1 }, { QStringLiteral("fps"), 4.0 } }),
              QStringLiteral("could not write one-frame plan"));
        const ScenarioResult result = runScenario(
            videoOptions(input, output), temporary.path());
        check(result.failed == 1
                  && result.error.contains(QStringLiteral("pelo menos dois")),
              QStringLiteral("single-frame interpolation input was accepted"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-low-fps-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create low-FPS input"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 10 }, { QStringLiteral("fps"), 10.0 } }),
              QStringLiteral("could not write low-FPS plan"));
        auto options = videoOptions(input, output);
        options.targetFps = 8;
        const ScenarioResult result = runScenario(options, temporary.path());
        check(result.failed == 1 && result.error.contains(QStringLiteral("tem de ser maior")),
              QStringLiteral("non-increasing interpolation FPS was accepted"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-no-retry-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create encode failure input"));
        QJsonObject plan {
            { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
            { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
            { QStringLiteral("audioCodecs"), QJsonArray { QStringLiteral("aac") } },
            { QStringLiteral("encodeFailure"), true }
        };
        check(writePlan(temporary.path(), plan), QStringLiteral("could not write encode failure plan"));
        const ScenarioResult result = runScenario(videoOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0,
              QStringLiteral("unrelated encode failure was not reported"));
        QFile logFile(QDir(temporary.path()).filePath(QStringLiteral("invocations.jsonl")));
        check(logFile.open(QIODevice::ReadOnly),
              QStringLiteral("could not read invocation log"));
        check(!logFile.readAll().contains("192k"),
              QStringLiteral("unrelated encode failure triggered AAC retry"));
    }

    for (const auto& engineFault : {
             qMakePair(QStringLiteral("engineFailure"), QStringLiteral("engine failure")),
             qMakePair(QStringLiteral("omitEngineOutput"), QStringLiteral("missing engine output")) }) {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-engine-failure-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::darkYellow);
        check(image.save(input), QStringLiteral("could not create engine-failure input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed engine-failure output"));
        check(writePlan(temporary.path(), QJsonObject {
                  { engineFault.first, true } }),
              QStringLiteral("could not write engine-failure plan"));
        const ScenarioResult result = runScenario(
            imageOptions(input, output), temporary.path());
        check(result.failed == 1 && result.finished == 0,
              QStringLiteral("pipeline accepted ") + engineFault.second);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly),
              QStringLiteral("could not reopen engine-failure destination"));
        check(destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("engine fault replaced existing destination"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-cancel-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::green);
        check(image.save(input), QStringLiteral("could not create cancellation input"));
        check(writeFile(output, QByteArrayLiteral("previous")), QStringLiteral("could not seed cancellation output"));
        check(writePlan(temporary.path(), QJsonObject { { QStringLiteral("blockEngine"), true } }),
              QStringLiteral("could not write cancellation plan"));
        const ScenarioResult result = runScenario(imageOptions(input, output), temporary.path(), true);
        check(result.cancelled == 1 && result.finished == 0 && result.failed == 0,
              QStringLiteral("cancellation signal contract failed: ") + result.error);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly),
              QStringLiteral("could not reopen existing destination"));
        check(destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("cancellation replaced existing destination"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("cancellation left commit artifacts"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-reuse-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::darkGreen);
        check(image.save(input), QStringLiteral("could not create reused-job input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed reused-job destination"));
        check(writePlan(temporary.path(), {}),
              QStringLiteral("could not write reused-job plan"));

        framescale::ProcessingJob job(
            imageOptions(input, output), framescale::RuntimePaths(temporary.path()));
        bool cancelAtCommitBoundary = true;
        const auto cancelHook = [&cancelAtCommitBoundary](
            framescale::ProcessingJob& currentJob, const QString& stage) {
            if (cancelAtCommitBoundary
                && stage == QStringLiteral("Gravando resultado")) {
                cancelAtCommitBoundary = false;
                currentJob.cancel();
            }
        };
        const ScenarioResult cancelledRun = runExistingJob(job, false, cancelHook);
        check(cancelledRun.cancelled == 1 && cancelledRun.finished == 0
                  && cancelledRun.failed == 0,
              QStringLiteral("step-boundary cancellation failed: ")
                  + cancelledRun.error);
        QFile previous(output);
        check(previous.open(QIODevice::ReadOnly)
                  && previous.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("step-boundary cancellation replaced destination"));
        previous.close();
        const QVector<QJsonObject> cancelledInvocations = readInvocations(temporary.path());
        int imageWrites = 0;
        for (const QJsonObject& invocation : cancelledInvocations) {
            if (invocationTool(invocation) == QStringLiteral("ffmpeg")) {
                ++imageWrites;
            }
        }
        check(imageWrites == 0,
              QStringLiteral("cancelled boundary started the final image write"));

        const ScenarioResult reusedRun = runExistingJob(job);
        check(reusedRun.finished == 1 && reusedRun.failed == 0
                  && reusedRun.cancelled == 0,
              QStringLiteral("reused job did not finish: ") + reusedRun.error);
        check(QImage(output).size() == QSize(32, 24),
              QStringLiteral("reused job result has wrong dimensions"));
        const QVector<QJsonObject> reusedInvocations = readInvocations(temporary.path());
        check(firstInvocation(reusedInvocations, QStringLiteral("realesrgan-ncnn-vulkan")) >= 0
                  && reusedInvocations.size() > cancelledInvocations.size(),
              QStringLiteral("reused job did not start a fresh pipeline"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("reused job left commit artifacts"));

        const QString model = QDir(temporary.path()).filePath(
            QStringLiteral("models/realesrgan/realesr-animevideov3-x2.bin"));
        check(QFile::remove(model),
              QStringLiteral("could not remove model before reused preflight"));
        const int invocationsBeforePreflight = readInvocations(temporary.path()).size();
        const ScenarioResult failedReuse = runExistingJob(job);
        check(failedReuse.failed == 1 && failedReuse.finished == 0
                  && failedReuse.cancelled == 0
                  && failedReuse.error.contains(QStringLiteral("incompletos")),
              QStringLiteral("reused job did not report preflight failure"));
        check(readInvocations(temporary.path()).size() == invocationsBeforePreflight,
              QStringLiteral("reused preflight failure started a child process"));
        check(QImage(output).size() == QSize(32, 24),
              QStringLiteral("reused preflight failure replaced destination"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-aac-cancel-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create AAC-cancel input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed AAC-cancel destination"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
                  { QStringLiteral("audioCodecs"), QJsonArray { QStringLiteral("aac") } },
                  { QStringLiteral("rejectAudioCopy"), true } }),
              QStringLiteral("could not write AAC-cancel plan"));
        framescale::ProcessingJob job(
            videoOptions(input, output), framescale::RuntimePaths(temporary.path()));
        bool cancelAtRetry = true;
        const ScenarioResult result = runExistingJob(
            job, false,
            [&cancelAtRetry](framescale::ProcessingJob& currentJob,
                             const QString& stage) {
                if (cancelAtRetry
                    && stage == QStringLiteral(
                        "Áudio incompatível; tentando novamente em AAC")) {
                    cancelAtRetry = false;
                    currentJob.cancel();
                }
            });
        check(result.cancelled == 1 && result.finished == 0 && result.failed == 0,
              QStringLiteral("AAC-retry cancellation failed: ") + result.error);
        int copyEncodes = 0;
        int aacEncodes = 0;
        for (const QJsonObject& invocation : readInvocations(temporary.path())) {
            const QStringList arguments = invocationArguments(invocation);
            if (invocationTool(invocation) != QStringLiteral("ffmpeg")
                || !arguments.contains(QStringLiteral("libx264"))) {
                continue;
            }
            const int codec = arguments.indexOf(QStringLiteral("-c:a"));
            if (arguments.value(codec + 1) == QStringLiteral("copy")) {
                ++copyEncodes;
            } else if (arguments.value(codec + 1) == QStringLiteral("aac")) {
                ++aacEncodes;
            }
        }
        check(copyEncodes == 1 && aacEncodes == 0,
              QStringLiteral("AAC encode started after synchronous cancellation"));
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly)
                  && destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("AAC-retry cancellation replaced destination"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("AAC-retry cancellation left commit artifacts"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-destruction-XXXXXX"));
        rootFor(temporary);
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.mp4"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.mp4"));
        check(writeFile(input), QStringLiteral("could not create destruction input"));
        check(writeFile(output, QByteArrayLiteral("previous")),
              QStringLiteral("could not seed destruction destination"));
        check(writePlan(temporary.path(), QJsonObject {
                  { QStringLiteral("width"), 16 }, { QStringLiteral("height"), 12 },
                  { QStringLiteral("frames"), 4 }, { QStringLiteral("fps"), 4.0 },
                  { QStringLiteral("blockAfterEncodeOutput"), true } }),
              QStringLiteral("could not write destruction plan"));

        int finished = 0;
        int failed = 0;
        int cancelled = 0;
        bool resetScheduled = false;
        QString partial;
        QString workspace;
        QEventLoop loop;
        auto job = std::make_unique<framescale::ProcessingJob>(
            videoOptions(input, output), framescale::RuntimePaths(temporary.path()));
        QObject::connect(job.get(), &framescale::ProcessingJob::finished,
                         &loop, [&finished] { ++finished; });
        QObject::connect(job.get(), &framescale::ProcessingJob::failed,
                         &loop, [&failed] { ++failed; });
        QObject::connect(job.get(), &framescale::ProcessingJob::cancelled,
                         &loop, [&cancelled] { ++cancelled; });
        QObject::connect(job.get(), &framescale::ProcessingJob::progressChanged,
            &loop, [&] (const int, const QString& stage) {
                if (resetScheduled
                    || stage != QStringLiteral("Codificando vídeo H.264")) {
                    return;
                }
                const QVector<QJsonObject> invocations = readInvocations(temporary.path());
                for (const QJsonObject& invocation : invocations) {
                    const QStringList arguments = invocationArguments(invocation);
                    if (invocationTool(invocation) == QStringLiteral("ffmpeg")
                        && arguments.contains(QStringLiteral("libx264"))) {
                        partial = arguments.last();
                    } else if (invocationTool(invocation)
                                   == QStringLiteral("realesrgan-ncnn-vulkan")) {
                        const QString outputDirectory = arguments.value(
                            arguments.indexOf(QStringLiteral("-o")) + 1);
                        if (!outputDirectory.isEmpty()) {
                            workspace = QFileInfo(outputDirectory).absolutePath();
                        }
                    }
                }
                if (!partial.isEmpty() && QFileInfo::exists(partial)) {
                    check(!workspace.isEmpty() && QFileInfo::exists(workspace),
                          QStringLiteral("destruction workspace was missing before reset"));
                    resetScheduled = true;
                    QTimer::singleShot(0, &loop, [&] {
                        job.reset();
                        loop.quit();
                    });
                }
            });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        QObject::connect(&watchdog, &QTimer::timeout, &loop, [&] {
            if (job) {
                job.reset();
            }
            loop.quit();
        });
        watchdog.start(20000);
        QTimer::singleShot(0, &loop, [&] { job->start(); });
        loop.exec();
        check(resetScheduled, QStringLiteral("destruction test never observed partial output"));
        check(finished == 0 && failed == 0 && cancelled == 0,
              QStringLiteral("destructor emitted a terminal processing signal"));
        check(!partial.isEmpty() && !QFileInfo::exists(partial),
              QStringLiteral("destructor left partial output"));
        check(QThreadPool::globalInstance()->waitForDone(5000),
              QStringLiteral("background workspace cleanup did not complete"));
        check(!workspace.isEmpty() && !QFileInfo::exists(workspace),
              QStringLiteral("background cleanup left temporary workspace"));
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly)
                  && destination.readAll() == QByteArrayLiteral("previous"),
              QStringLiteral("destruction replaced destination"));
        check(QDir(temporary.path()).entryList(
                  { QStringLiteral("*.part.*"), QStringLiteral("*.backup"),
                    QStringLiteral("*.staged") }, QDir::Files).isEmpty(),
              QStringLiteral("destruction left commit artifacts"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-incomplete-model-XXXXXX"));
        rootFor(temporary);
        const QString model = QDir(temporary.path()).filePath(
            QStringLiteral("models/realesrgan/realesr-animevideov3-x2.bin"));
        check(QFile::remove(model), QStringLiteral("could not remove fake model file"));
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::magenta);
        check(image.save(input), QStringLiteral("could not create incomplete-model input"));
        check(writePlan(temporary.path(), {}),
              QStringLiteral("could not write incomplete-model plan"));
        const ScenarioResult result = runScenario(
            imageOptions(input, output), temporary.path());
        check(result.failed == 1
                  && result.error.contains(QStringLiteral("incompletos")),
              QStringLiteral("incomplete model was not rejected"));
        check(!QFileInfo::exists(QDir(temporary.path()).filePath(
                  QStringLiteral("invocations.jsonl"))),
              QStringLiteral("incomplete-model preflight invoked a child process"));
    }

    {
        QTemporaryDir temporary(QStringLiteral("FrameScale-fake-missing-XXXXXX"));
        rootFor(temporary);
        const QString missing = QDir(temporary.path()).filePath(
            QStringLiteral("runtime/bin/") + executableName(QStringLiteral("realesrgan-ncnn-vulkan")));
        check(QFile::remove(missing), QStringLiteral("could not remove fake engine"));
        const QString input = QDir(temporary.path()).filePath(QStringLiteral("entrada.png"));
        const QString output = QDir(temporary.path()).filePath(QStringLiteral("resultado.png"));
        QImage image(16, 12, QImage::Format_RGBA8888);
        image.fill(Qt::blue);
        check(image.save(input), QStringLiteral("could not create missing-tool input"));
        check(writePlan(temporary.path(), {}), QStringLiteral("could not write missing-tool plan"));
        const ScenarioResult result = runScenario(imageOptions(input, output), temporary.path());
        check(result.failed == 1 && result.error.contains(QStringLiteral("Ferramenta necessária")),
              QStringLiteral("strict runtime did not reject missing tool"));
        check(!QFileInfo::exists(QDir(temporary.path()).filePath(QStringLiteral("invocations.jsonl"))),
              QStringLiteral("preflight failure invoked a child process"));
    }

    for (bool approve : { false, true }) {
        QTemporaryDir temporary;
        rootFor(temporary);
        const QString input = temporary.filePath("input.png");
        const QString output = temporary.filePath("existing.png");
        QImage source(16, 12, QImage::Format_RGB32);
        source.fill(Qt::blue);
        check(source.save(input), QStringLiteral("overwrite source failed"));
        check(writeFile(output, "original"), QStringLiteral("overwrite destination failed"));
        check(writePlan(temporary.path(), {}), QStringLiteral("overwrite plan failed"));
        framescale::ProcessingJob job(imageOptions(input, output), framescale::RuntimePaths(temporary.path()));
        int prompts = 0;
        job.confirmOverwrite = [&](const QString& path) {
            ++prompts;
            check(path == output, QStringLiteral("wrong overwrite path"));
            return approve;
        };
        const auto result = runExistingJob(job);
        QFile destination(output);
        check(destination.open(QIODevice::ReadOnly), QStringLiteral("destination missing"));
        const auto bytes = destination.readAll();
        check(prompts == 1, QStringLiteral("overwrite confirmation was not requested exactly once"));
        check(approve ? result.finished == 1 && bytes != "original"
                      : result.failed == 1 && bytes == "original",
            QStringLiteral("overwrite decision was ignored"));
    }
    if (failures == 0) {
        std::cout << "FrameScale deterministic processing smoke: OK\n";
    }
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    if(argc==2 && argument(1,argv)=="--pause-heartbeat") {
        for(int i=0;i<500;++i) { std::cout<<"tick"<<std::endl; QThread::msleep(20); }
        return 0;
    }
    if(argc==2 && argument(1,argv)=="--partial-test") {
        const bool ok=framescale::ProcessingJobTestAccess::partialRegression();
        std::cout<<(ok ? "Partial recovery OK" : "Partial recovery failed")<<std::endl;
        return ok ? 0 : 1;
    }
    if(argc==2 && argument(1,argv)=="--pause-test") {
        const bool ok=framescale::ProcessingJobTestAccess::pauseRegression();
        std::cout<<(ok ? "Pause/resume/cancel OK" : "Pause test failed")<<std::endl;
        return ok ? 0 : 1;
    }

    if (argc == 3 && argument(1, argv) == QStringLiteral("--fake-suite")) {
        return runFakeSuite(argument(2, argv));
    }
    if (argc < 7) {
        std::cerr << "usage: smoke input output image|video upscale|interpolate|both engine model [scale] [fps] [denoise] [cancel-ms]\n"
                  << "       optional: --codec=libx265 --preset=fast --crf=23 --pixel-format=yuv444p --no-audio --trim-start=0.5 --trim-end=1.5\n"
                  << "       smoke --fake-suite fake-tool\n";
        return 64;
    }

    framescale::ProcessingOptions options;
    options.inputPath = toPath(argument(1, argv));
    options.outputPath = toPath(argument(2, argv));
    const QString media = argument(3, argv);
    const QString operation = argument(4, argv);
    const QString engine = argument(5, argv);
    options.mediaType = media == QStringLiteral("video")
        ? framescale::MediaType::Video
        : framescale::MediaType::Image;
    options.operation = operation == QStringLiteral("interpolate")
        ? framescale::Operation::Interpolation
        : operation == QStringLiteral("both")
        ? framescale::Operation::UpscaleAndInterpolation
        : operation == QStringLiteral("copy") ? framescale::Operation::Copy : framescale::Operation::Upscale;
    if (media != QStringLiteral("image") && media != QStringLiteral("video")) {
        std::cerr << "unknown media type\n";
        return 64;
    }
    if (operation != QStringLiteral("upscale")
        && operation != QStringLiteral("interpolate")
        && operation != QStringLiteral("both")
        && operation != QStringLiteral("copy")) {
        std::cerr << "unknown operation\n";
        return 64;
    }
    if (engine == QStringLiteral("cugan")) {
        options.upscaleEngine = framescale::UpscaleEngine::RealCUGAN;
    } else if (engine == QStringLiteral("anime4k")) {
        options.upscaleEngine = framescale::UpscaleEngine::Anime4K;
    } else if (engine == QStringLiteral("esrgan")) {
        options.upscaleEngine = framescale::UpscaleEngine::RealESRGAN;
    } else {
        std::cerr << "unknown upscale engine\n";
        return 64;
    }
    options.upscaleModelId = argument(6, argv).toStdString();
    options.rifeModelId = "rife-v4.6";
    int positional = 0;
    bool overwrite = false;
    bool preservePartial = false;
    int cancelAtFrame = 0;
    int cancelMilliseconds = -1;
    for (int index = 7; index < argc; ++index) {
        const QString value = argument(index, argv);
        if (!value.startsWith(QStringLiteral("--"))) {
            bool valid = false;
            const double number = value.toDouble(&valid);
            if (!valid || positional > 3) {
                std::cerr << "invalid positional option\n";
                return 64;
            }
            switch (positional++) {
            case 0:
                options.scaleFactor = number;
                break;
            case 1:
                options.targetFps = number;
                break;
            case 2:
                options.denoise = static_cast<framescale::DenoiseLevel>(number);
                break;
            case 3:
                cancelMilliseconds = number;
                break;
            }
        } else if (value == QStringLiteral("--overwrite")) {
            overwrite = true;
        } else if (value == QStringLiteral("--no-audio")) {
            options.keepAudio = false;
        } else if (value == QStringLiteral("--no-metadata")) {
            options.keepMetadata = false;
        } else {
            const QString name = value.section(QLatin1Char('='), 0, 0);
            const QString setting = value.section(QLatin1Char('='), 1);
            bool valid = true;
            if (name == "--preserve-partial") preservePartial = setting.toInt(&valid) != 0;
            else if (name == "--cancel-at-frame") cancelAtFrame = setting.toInt(&valid);
            else if (name == "--quarter-turns") options.orientation.rotate(setting.toInt(&valid));
            else if (name == "--flip-horizontal") options.orientation.horizontalFlip = setting.toInt(&valid) != 0;
            else if (name == "--flip-vertical") options.orientation.verticalFlip = setting.toInt(&valid) != 0;
            else if (name == "--enhancement") options.enhancement.enabled = setting.toInt(&valid) != 0;
            else if (name == "--enhance-model") options.enhancement.model = setting.toInt(&valid);
            else if (name == "--enhance-denoise") options.enhancement.denoise = setting.toInt(&valid);
            else if (name == "--enhance-sharpen") options.enhancement.sharpen = setting.toInt(&valid);
            else if (name == "--enhance-deblock") options.enhancement.deblock = setting.toInt(&valid);
            else if (name == "--enhance-deband") options.enhancement.deband = setting.toInt(&valid);
            else if (name == "--enhance-grain") options.enhancement.grain = setting.toInt(&valid);
            else if (name == "--enhance-clarity") options.enhancement.clarity = setting.toInt(&valid);
            else if (name == "--enhance-contrast") options.enhancement.contrast = setting.toInt(&valid);
            else if (name == "--enhance-vibrance") options.enhancement.vibrance = setting.toInt(&valid);
            else if (name == "--enhance-deinterlace") options.enhancement.deinterlace = setting.toInt(&valid);
            else if (name == "--layers") {
                QFile file(setting);
                valid = file.open(QIODevice::ReadOnly);
                QJsonParseError error;
                const auto doc=QJsonDocument::fromJson(file.readAll(), &error);
                valid = valid && error.error==QJsonParseError::NoError && doc.isArray();
                options.layered=true;
                for(const auto entry:doc.array()) {
                    const auto obj=entry.toObject(); framescale::MediaLayer layer;
                    layer.path=toPath(obj.value("path").toString());
                    layer.kind=obj.value("kind").toString()=="audio" ? framescale::LayerKind::Audio : framescale::LayerKind::Video;
                    layer.enabled=obj.value("enabled").toBool(true);
                    layer.sourceDuration=obj.value("duration").toDouble();
                    layer.audioTimeScale=obj.value("audioTimeScale").toDouble(1);
                    layer.sourceFps=obj.value("fps").toDouble(25);
                    layer.width=obj.value("width").toInt(96); layer.height=obj.value("height").toInt(64);
                    layer.start=obj.value("start").toDouble(); layer.in=obj.value("in").toDouble();
                    layer.out=obj.value("out").toDouble(layer.sourceDuration); layer.speed=obj.value("speed").toDouble(1);
                    options.layers.push_back(layer);
                }
            }
            else if (name == "--speed") options.playbackSpeed = setting.toDouble(&valid);
            else if (name == "--audio-offset-ms") options.audioOffsetMs = setting.toInt(&valid);
            else if (name == "--reduce-fps") options.reductionFps = setting.toInt(&valid);
            else if (name == QStringLiteral("--codec"))
                options.videoCodec = setting.toStdString();
            else if (name == QStringLiteral("--prores-profile"))
                options.proresProfile = setting.toInt(&valid);
            else if (name == QStringLiteral("--preset"))
                options.encoderPreset = setting.toStdString();
            else if (name == QStringLiteral("--pixel-format"))
                options.pixelFormat = setting.toStdString();
            else if (name == QStringLiteral("--format"))
                options.outputSuffix = setting.toStdString();
            else if (name == QStringLiteral("--audio-codec"))
                options.audioCodec = setting.toStdString();
            else if (name == QStringLiteral("--audio-rate"))
                options.audioBitRate = setting.toInt(&valid);
            else if (name == QStringLiteral("--sample-rate"))
                options.audioSampleRate = setting.toInt(&valid);
            else if (name == QStringLiteral("--channels"))
                options.audioChannels = setting.toInt(&valid);
            else if (name == QStringLiteral("--rate-control"))
                options.rateControl = setting.toStdString();
            else if (name == QStringLiteral("--bit-rate"))
                options.bitRate = setting.toInt(&valid);
            else if (name == QStringLiteral("--profile"))
                options.videoProfile = setting.toStdString();
            else if (name == QStringLiteral("--level"))
                options.videoLevel = setting.toStdString();
            else if (name == QStringLiteral("--crf"))
                options.crf = setting.toInt(&valid);
            else if (name == QStringLiteral("--trim-start"))
                options.trimStartSeconds = setting.toDouble(&valid);
            else if (name == QStringLiteral("--trim-end"))
                options.trimEndSeconds = setting.toDouble(&valid);
            else
                valid = false;
            if (!valid || setting.isEmpty()) {
                std::cerr << "invalid option: " << value.toStdString() << '\n';
                return 64;
            }
        }
    }

    if (options.outputSuffix == ".mp4")
        options.outputSuffix = options.outputPath.extension().string();
    framescale::ProcessingJob job(options);
    job.preservePartialOnCancel = preservePartial;
    if (cancelAtFrame > 0) {
        auto stage = std::make_shared<QString>();
        QObject::connect(&job, &framescale::ProcessingJob::progressChanged, &job, [stage](int,const QString& text){*stage=text;});
        QObject::connect(&job, &framescale::ProcessingJob::frameProgressChanged, &job, [&job,stage,cancelAtFrame](qint64 current,qint64){
            if(current>=cancelAtFrame && (stage->contains("RIFE") || stage->contains("Codificando"))) job.cancel();
        });
    }
    job.confirmOverwrite = [overwrite](const QString&) { return overwrite; };
    QObject::connect(&job, &framescale::ProcessingJob::progressChanged,
        [](const int percent, const QString& stage) {
            std::cout << percent << "% " << stage.toStdString() << '\n';
        });
    QObject::connect(&job, &framescale::ProcessingJob::frameProgressChanged,
        [](const qint64 current, const qint64 total) {
            if (total > 0) {
                std::cout << current << '/' << total << " frames\n";
            }
        });
    QObject::connect(&job, &framescale::ProcessingJob::finished,
        [&application](const QString& output) {
            std::cout << "finished: " << output.toStdString() << '\n';
            application.exit(0);
        });
    QObject::connect(&job, &framescale::ProcessingJob::failed,
        [&application](const QString& message) {
            std::cerr << "failed: " << message.toStdString() << '\n';
            application.exit(1);
        });
    QObject::connect(&job, &framescale::ProcessingJob::cancelled,
        [&application] {
            std::cerr << "cancelled\n";
            application.exit(2);
        });
    QTimer::singleShot(0, &job, &framescale::ProcessingJob::start);
    if (cancelMilliseconds >= 0) {
        QTimer::singleShot(cancelMilliseconds, &job,
            &framescale::ProcessingJob::cancel);
    }
    return application.exec();
}
