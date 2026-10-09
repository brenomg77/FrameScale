#pragma once
#include "core/ProcessingOptions.h"
#include <QVariantMap>
#include <QWidget>
#include <functional>
class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QTabWidget;
class QDoubleSpinBox;

class EncoderOptionsWidget final : public QWidget {
    Q_OBJECT
public:
    explicit EncoderOptionsWidget(QWidget* parent = nullptr);
    void setMediaType(framescale::MediaType type);
    void applyOptions(const framescale::ProcessingOptions& options);
    void writeOptions(framescale::ProcessingOptions& options) const;
    std::function<QVariantMap()> enhancementPresetValues;
    std::function<void(const QVariantMap&)> applyEnhancementPresetValues;
    void savePreset(const QString& name);
    bool loadPreset(const QString& name);

protected:
    void changeEvent(QEvent* event) override;

private:
    void setPresetFeedback(const QString& message);
    QString presetDirectory() const;
    bool writePresetFile(const QString& name, const QVariantMap& options);
    QMap<QString, QString> presetFiles_;
    QVariantMap values() const;
    void applyValues(const QVariantMap& values);
    QComboBox* suffix_ = nullptr;
    void syncFormat();
    void syncControls();
    void updateHelp();
    void showFormatOptions();
    framescale::MediaType mediaType_ = framescale::MediaType::Video;
    QVariantMap legacy_;
    QTabWidget* details_ = nullptr;
    QWidget *videoPage_ = nullptr, *audioPage_ = nullptr;
    QLabel* summary_ = nullptr;
    QComboBox *rate_ = nullptr, *profile_ = nullptr, *level_ = nullptr;
    QComboBox *audioCodec_ = nullptr, *audioRate_ = nullptr, *sampleRate_ = nullptr, *channels_ = nullptr;
    QDoubleSpinBox* targetRate_ = nullptr;
    void refreshPresets(const QString& selected = { });
    QComboBox* codec_ = nullptr;
    QComboBox* proresProfile_ = nullptr;
    QComboBox* speed_ = nullptr;
    QComboBox* pixels_ = nullptr;
    QComboBox* presets_ = nullptr;
    QSpinBox* quality_ = nullptr;
    QCheckBox* audio_ = nullptr;
    QCheckBox* metadata_ = nullptr;
    QLabel* feedback_ = nullptr;
};
