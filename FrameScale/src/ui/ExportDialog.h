#pragma once

#include "core/ProcessingOptions.h"
#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <QVector>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;

class ExportDialog final : public QDialog {
    Q_OBJECT

public:
    explicit ExportDialog(const QString& inputPath, const QString& suggestedOutput,
        QWidget* parent = nullptr);

    void addItem(const framescale::ProcessingOptions& options, const QString& summary);
    int queueCount() const { return entries_.size(); }
    framescale::ProcessingOptions activeOptions() const;
    QString outputPath() const;
    void setProcessingSummary(const QString& summary);
    void setProgress(int percent, const QString& stage);
    void setFrameProgress(qint64 current, qint64 total);
    void setRunning(bool running);
    void setFinished(const QString& outputPath);
    void setFailed(const QString& message);
    void setPaused(bool paused);
    void setCancelled(const QString& partialPath = {});
    // Clipboard preparation shares the owner's worker. Keep snapshots queued
    // until that worker is released, including the gap between two exports.
    void setProcessingAvailable(bool available);

    // Call after releasing the previous ProcessingJob (or after validation fails
    // without creating a job). Dispatch never races process destruction.
    void jobReleased();

signals:
    void exportRequested(const QString& outputPath);
    void cancelRequested();
    void pauseRequested(bool paused);

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    enum class State { Ready, Queued, Running, Finished, Failed, Cancelled };
    struct Entry {
        quint64 id = 0;
        framescale::ProcessingOptions options;
        QString summary;
        State state = State::Ready;
    };

    int indexOf(quint64 id) const;
    bool busy() const;
    bool editable(quint64 id) const;
    void selectItem(int row);
    void saveOutput();
    void removeItem(quint64 id);
    void removeSelectedItems();
    void updateControls();
    void updateRow(int row, const QString& detail = {});
    void startQueue();
    void startNext();
    void finishItem(State state, const QString& detail = {});
    void clearPending();
    void chooseOutput();
    void requestCancel();
    void updateElapsed();
    void updateSpeed();

    QVector<Entry> entries_;
    QList<quint64> pending_;
    quint64 nextId_ = 1;
    quint64 selected_ = 0;
    quint64 active_ = 0;
    framescale::ProcessingOptions activeSnapshot_;
    bool running_ = false;
    bool processingAvailable_ = true;
    bool awaitingRelease_ = false;
    bool nextScheduled_ = false;
    bool cancellationPending_ = false;
    bool updatingSelection_ = false;
    bool video_ = false;
    int overallPercent_ = 0;
    QString currentStage_;
    qint64 stageFrames_ = 0;
    qint64 stageTotal_ = 0;
    QElapsedTimer stageTimer_;
    QElapsedTimer elapsed_;
    qint64 stoppedElapsed_ = 0;
    QList<QPair<qint64,qint64>> rateSamples_;

    QTableWidget* table_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLabel* stageLabel_ = nullptr;
    QLabel* frameCountLabel_ = nullptr;
    QLabel* speedLabel_ = nullptr;
    QLabel* elapsedLabel_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QProgressBar* progressBar_ = nullptr;
    QTimer* elapsedTimer_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QPushButton* startButton_ = nullptr;
    bool paused_=false;
    qint64 pauseStarted_=0,pausedTime_=0;
    QPushButton* pauseButton_=nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* closeButton_ = nullptr;
    QPushButton* folderButton_ = nullptr;
};
