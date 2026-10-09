#pragma once

#include "core/ProcessingOptions.h"
#include "processing/RuntimePaths.h"

#include <QObject>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace framescale {

class ProcessingJob final : public QObject {
    Q_OBJECT

public:
    explicit ProcessingJob(ProcessingOptions options, QObject* parent = nullptr);
    ProcessingJob(
        ProcessingOptions options,
        RuntimePaths runtimePaths,
        QObject* parent = nullptr);
    ~ProcessingJob() override;

    std::function<bool(const QString&)> confirmOverwrite;
    std::function<QString(const QString&)> resolveOutputConflict;
    void start();
    void cancel();
    bool setPaused(bool paused);
    bool isPaused() const { return paused_; }
    bool preservePartialOnCancel = false;
    QString savedPartialPath() const { return savedPartialPath_; }
    bool isRunning() const;

signals:
    void progressChanged(int percent, const QString& stage);
    // Counts refer to the current stage; total == 0 means it has no frame count.
    void frameProgressChanged(qint64 current, qint64 total);
    void failed(const QString& message);
    void finished(const QString& outputPath);
    void cancelled();

private:
    friend struct ProcessingJobTestAccess;

    struct VideoOutputContract {
        qint64 frameCount = 0;
        int width = 0;
        int height = 0;
        double fps = 0.0;
        double durationSeconds = 0.0;
        int audioStreamCount = 0;
        QStringList audioCodecs;
    };

    struct Step {
        QString title;
        QString program;
        QStringList arguments;
        int weight = 1;
        std::function<bool(QString&)> verify;
        bool retryWithAac = false;
        qint64 progressFrameCount = 0;
        QString progressFramesDirectory;
        bool verifyInBackground = false;
        bool rejectDiagnostics = false;
    };

    struct VerificationResult {
        std::atomic<bool> complete { false };
        bool valid = false;
        QString error;
    };

    struct FrameScanResult {
        std::atomic<bool> complete { false };
        QSet<QString> files;
    };

    void probeMedia();
    void probeVideoTimeline();
    void consumeVideoTimeline(const QByteArray& bytes);
    bool prepareSteps(QString& error);
    bool preflight(QString& error) const;
    bool validateModelFiles(QString& error) const;
    bool prepareAudioSteps(QString& error);
    bool prepareImageSteps(QString& error);
    bool appendUpscaleSteps(const QString& input, bool sequence, qint64 frameCount,
        QString& result, QString& error);
    bool prepareVideoSteps(QString& error);
    bool verifyFinalVideoProbe(QString& error) const;
    bool retryCurrentVideoEncodeWithAac();
    void runNextStep();
    void handleOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void finishSuccessfully();
    void fail(const QString& message);
    void finishCancellation();
    bool recoverPartialVideo();
    void cleanupOutputArtifacts();
    void updateCurrentProgress(double fraction);
    void updateCurrentFrameProgress(qint64 frameCount);
    void pollCompletedFrames();
    void completeVerifiedStep(bool valid, const QString& error);

    QString toolPath(const QString& name) const;
    QString modelPath(const std::filesystem::path& relative) const;
    QString realEsrganModelPath(const UpscaleModel& model, double scaleFactor) const;
    QString outputTemporaryPath() const;
    QString stagedOutputPath() const;
    QString backupOutputPath() const;
    QString createFramesDirectory(const QString& name);
    bool validateSequenceDestination(const QString& destination, QString& error) const;
    bool copyOrCommitOutput(const QString& source, QString& error);

    ProcessingOptions options_;
    ProcessingOptions requestedOptions_;
    bool composingLayers_ = false;
    std::shared_ptr<QTemporaryDir> compositionDirectory_;
    QString compositionPath_;
    RuntimePaths runtimePaths_;
    std::shared_ptr<QTemporaryDir> temporaryDirectory_;
    std::shared_ptr<VerificationResult> verification_;
    std::shared_ptr<FrameScanResult> frameScan_;
    QProcess process_;
    QTimer cancellationTimer_;
    QTimer frameProgressTimer_;
    QTimer verificationTimer_;
    QSet<QString> completedFrameFiles_;
    std::vector<Step> steps_;
    int stepIndex_ = -1;
    int completedWeight_ = 0;
    int totalWeight_ = 1;
    double sourceFps_ = 0.0;
    double durationSeconds_ = 0.0;
    qint64 sourceFrameCount_ = 0;
    qint64 trimStartFrame_ = 0;
    qint64 trimEndFrame_ = 0;
    double effectiveTrimStartSeconds_ = 0.0;
    int sourceWidth_ = 0;
    int sourceHeight_ = 0;
    int sourceAudioStreamCount_ = 0;
    QStringList sourceAudioCodecs_;
    VideoOutputContract videoOutputContract_;
    bool aacRetryAttempted_ = false;
    bool running_ = false;
    bool paused_ = false;
    bool cancelling_ = false;
    bool cancellationSignalled_ = false;
    bool probingVideoTimeline_ = false;
    bool videoTimelineProbed_ = false;
    bool invalidVideoTimeline_ = false;
    qint64 timelineFrameCount_ = 0;
    double timelineFirstTimestamp_ = 0.0;
    double timelineLastTimestamp_ = 0.0;
    double timelineLastDuration_ = 0.0;
    double timelinePreviousInterval_ = 0.0;
    double timelineDurationSeconds_ = 0.0;
    QByteArray sourceProbeMetadata_;
    QByteArray timelineBuffer_;
    QByteArray outputBuffer_;
    QByteArray progressBuffer_;
    QString outputTemporaryPath_;
    QString commitToken_;
    QString finalFramesDirectory_, savedPartialPath_;
    QStringList recoveryEncodeArguments_;
    bool recoveryAttempted_ = false, directVideo_ = false;
    int videoEncodeStep_ = -1;
    int lastProgress_ = 0;
    qint64 currentFrameProgress_ = 0;
    qint64 decodedFrameCount_ = 0;
};

} // namespace framescale
