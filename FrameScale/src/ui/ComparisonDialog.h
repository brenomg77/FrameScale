#pragma once
#include "core/ProcessingOptions.h"
#include <QDialog>
#include <QImage>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <memory>

namespace framescale {
class ProcessingJob;
}
class MediaPreviewWidget;
class QLabel;
class QProgressBar;
class TrimTimeline;
class QTimer;
class QProcess;

class ComparisonDialog final : public QDialog {
public:
    ComparisonDialog(framescale::ProcessingOptions options, double position, double duration,
        double fps, QWidget* parent = nullptr);
    ~ComparisonDialog() override;

protected:
    void reject() override;
    void closeEvent(QCloseEvent* event) override;

private:
    void render();
    void prepareClip(framescale::ProcessingOptions options);
    void detachPreparation();
    void startJob(framescale::ProcessingOptions options);
    void togglePlayback();
    void detachJob();
    void showResult(const QString& output);
    void seekBoth(qint64 position);
    void presentPair();
    qint64 comparisonLength() const;
    void stopPlayback();
    void fail(const QString& message);
    framescale::ProcessingOptions options_;
    std::unique_ptr<framescale::ProcessingJob> job_;
    std::shared_ptr<QTemporaryDir> temporary_;
    QByteArray cacheKey_;
    MediaPreviewWidget *original_ = nullptr, *processed_ = nullptr;
    QLabel* status_ = nullptr;
    QProgressBar* progress_ = nullptr;
    TrimTimeline* timeline_ = nullptr;
    QTimer* clock_ = nullptr;
    QElapsedTimer syncCorrection_;
    QProcess* preparation_ = nullptr;
    double sourceFps_ = 25, sourceDuration_ = 0;
    QString processedPath_;
    qint64 sourceOffset_ = 0, clipLength_ = 0;
    bool closing_ = false, ready_ = false, playing_ = false;
    bool waitingForPlayers_ = false;
    qint64 resumePosition_ = 0, seekPosition_ = 0;
    qint64 expectedOriginal_ = -1, expectedProcessed_ = -1;
    QImage originalFrame_, processedFrame_;
};
