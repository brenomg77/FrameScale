#pragma once

#include "core/ProcessingOptions.h"
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QTemporaryDir>
#include <QVector>
#include <memory>

namespace framescale {
class ProcessingJob;
}
class QAction;
class QParallelAnimationGroup;
class QCheckBox;
class QCloseEvent;
class QDialog;
class QComboBox;
class QDoubleSpinBox;
class ScaleControl;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class ExportDialog;
class EncoderOptionsWidget;
class EnhancementWidget;
class MediaPreviewWidget;
class LayerEditor;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void chooseInputFile();
    void openInputFile(const QString& path);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message,
        qintptr* result) override;

private:
    struct Project {
        QString path;
        framescale::ProcessingOptions options;
    };
    QVector<Project> projects_;
    QTabBar* projectTabs_ = nullptr;
    int activeProject_ = -1;
    QCheckBox* upscaleCheck_ = nullptr;
    bool exitConfirmed_ = false;
    EncoderOptionsWidget* encoderOptions_ = nullptr;
    EnhancementWidget* enhancement_ = nullptr;
    void saveProject();
    void activateProject(int index);
    void addToQueue();
    void buildInterface();
    QWidget* buildTitleBar();
    QWidget* buildWelcomePage();
    QWidget* buildEditorPage();
    QWidget* buildInspector();
    void showSettings();
    void applyAppearance();
    void updateWindowCorners();
    void showWelcome();
    void enterDocument();
    QLabel* emptyDocument_ = nullptr;
    QPointer<QDialog> welcomeDialog_;
    void startNewProject();
    bool trySetInputFile(const QString& path);
    void pasteMedia();
    void openUrl();
    bool isProcessing() const;
    framescale::Operation operation() const;
    framescale::ProcessingOptions processingOptions(const QString& output) const;
    void updateOperationControls();
    void updateModelChoices();
    void updateDenoiseChoices();
    void copyToClipboard();
    void showExportDialog();
    void beginExport(const QString& output);
    void cancelProcessing();
    void processingStopped();
    void setStatusText(const QString& text);

    QStackedWidget* pageStack_ = nullptr;
    QWidget* welcomePage_ = nullptr;
    QWidget* editorPage_ = nullptr;
    QWidget* inspector_ = nullptr;
    QWidget* upscaleFields_ = nullptr;
    QWidget* interpolationFields_ = nullptr;
    QWidget* denoiseField_ = nullptr;
    MediaPreviewWidget* preview_ = nullptr;
    LayerEditor* layers_ = nullptr;

    QLabel* validationLabel_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QCheckBox* combinedCheck_ = nullptr;
    QComboBox* engineCombo_ = nullptr;
    QComboBox* modelCombo_ = nullptr;
    QComboBox* denoiseCombo_ = nullptr;
    ScaleControl* scaleCombo_ = nullptr;
    QComboBox* rifeCombo_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QDoubleSpinBox* playbackSpeed_ = nullptr;
    QWidget* speedField_ = nullptr;
    QSpinBox* reductionFps_ = nullptr;
    QWidget* reductionField_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QAction* newAction_ = nullptr;
    QAction* openAction_ = nullptr;
    QPointer<ExportDialog> exportDialog_;
    std::unique_ptr<framescale::ProcessingJob> processingJob_;
    std::unique_ptr<QTemporaryDir> clipboardDirectory_;
    std::unique_ptr<QTemporaryDir> pendingClipboardDirectory_;
    QString inputPath_;
    bool cancellationPending_ = false;
    bool clipboardCopyPending_ = false;
    bool closeWhenStopped_ = false;
};