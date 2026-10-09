#include "ui/SelectionComboBox.h"
#include "EncoderOptionsWidget.h"
#include "Appearance.h"
#include "ControlSymbols.h"
#include <QHBoxLayout>
#include "EditorControls.h"
#include "EnhancementWidget.h"
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStylePainter>
#include <QStyleOptionButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
class PresetIconButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor background=palette().color(QPalette::Button);
        if(isDown()) background=background.darker(115);
        else if(underMouse() && isEnabled()) background=background.lighter(110);
        painter.setBrush(background);
        painter.setPen(QPen(palette().color(hasFocus() ? QPalette::Highlight : QPalette::Mid),1));
        painter.drawRoundedRect(QRectF(rect()).adjusted(.5,.5,-.5,-.5),6,6);
        const QSize size(18,18);
        const QRect target((width()-size.width())/2,(height()-size.height())/2,size.width(),size.height());
        icon().paint(&painter,target,Qt::AlignCenter,isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }
};
}

EncoderOptionsWidget::EncoderOptionsWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("encoderOptions");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 8, 12, 0);
    root->setSpacing(14);
    auto* formatGroup = new QGroupBox(tr("Módulo de saída"), this);
    auto* formatForm = new QFormLayout(formatGroup);
    formatForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    suffix_ = new SelectionComboBox(this);
    suffix_->setObjectName("encoderSuffix");
    formatForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    formatForm->addRow(tr("Formato"), suffix_);
    root->addWidget(formatGroup);
    auto* output = new QGroupBox(tr("Saída"), this);
    auto* outputLayout = new QVBoxLayout(output);
    summary_ = new QLabel(output);
    summary_->setObjectName("encoderSummary");
    summary_->setWordWrap(true);
    outputLayout->addWidget(summary_);
    auto* options = new QPushButton(tr("Opções de formato…"), output);
    options->setObjectName("formatOptionsButton");
    options->setMinimumHeight(26);
    outputLayout->addWidget(options, 0, Qt::AlignLeft);
    metadata_ = new SwitchControl(tr("Preservar metadados"), output);
    metadata_->setObjectName("encoderKeepMetadata");
    metadata_->setChecked(true);
    outputLayout->addWidget(metadata_);
    root->addWidget(output);
    root->addStretch();

    // One settings surface lives in the modal Format Options dialog. The main
    // inspector shows only the chosen output format and its codec summary.
    details_ = new QTabWidget(this);
    details_->setObjectName("formatDetails");
    details_->tabBar()->setDrawBase(false);
    details_->setAutoFillBackground(false);
    details_->hide();
    videoPage_ = new QWidget;
    videoPage_->setObjectName("formatVideoPage");
    auto* videoLayout = new QVBoxLayout(videoPage_);
    auto* videoGroup = new QGroupBox(tr("Codificação de vídeo"), videoPage_);
    auto* form = new QFormLayout(videoGroup);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setSpacing(14);
    codec_ = new SelectionComboBox;
    codec_->setObjectName("videoCodec");
    form->addRow(tr("Codec de vídeo"), codec_);
    proresProfile_ = new SelectionComboBox;
    proresProfile_->setObjectName("encoderProresProfile");
    for (int profile = 0; profile <= 5; ++profile)
        proresProfile_->addItem(QString::fromStdString(framescale::proresProfileName(profile)), profile);
    proresProfile_->setCurrentIndex(3);
    form->addRow(tr("Perfil ProRes"), proresProfile_);
    rate_ = new SelectionComboBox;
    rate_->setObjectName("encoderRateControl");
    rate_->addItem(tr("Qualidade constante (CRF)"), "crf");
    rate_->addItem(tr("Taxa variável (VBR, 1 passagem)"), "vbr");
    rate_->addItem(tr("Taxa constante (CBR)"), "cbr");
    form->addRow(tr("Controle de qualidade"), rate_);
    quality_ = new QSpinBox;
    quality_->setObjectName("encoderCrf");
    quality_->setRange(0, 51);
    quality_->setValue(18);
    quality_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    quality_->setToolTip(tr("CRF: menor valor aumenta a qualidade e o tamanho do arquivo."));
    form->addRow(tr("Qualidade (CRF)"), quality_);
    targetRate_ = new QDoubleSpinBox;
    targetRate_->setObjectName("encoderTargetRate");
    targetRate_->setRange(.1, 2000);
    targetRate_->setDecimals(1);
    targetRate_->setValue(8);
    targetRate_->setSuffix(" Mbps");
    targetRate_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    form->addRow(tr("Taxa de bits alvo"), targetRate_);
    speed_ = new SelectionComboBox;
    speed_->setObjectName("encoderSpeed");
    const QStringList labels { tr("Ultrarrápida"), tr("Super-rápida"), tr("Muito rápida"), tr("Mais rápida"), tr("Rápida"), tr("Média"), tr("Lenta"), tr("Mais lenta"), tr("Muito lenta") };
    int i = 0;
    for (auto v : { "ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow" })
        speed_->addItem(labels[i++], v);
    speed_->setCurrentIndex(5);
    form->addRow(tr("Velocidade"), speed_);
    pixels_ = new SelectionComboBox;
    pixels_->setObjectName("encoderPixelFormat");
    for (auto v : { "auto", "yuv420p", "yuv444p", "yuv422p10le", "yuvj420p", "yuv444p10le" })
        pixels_->addItem(v, v);
    pixels_->setCurrentIndex(1);
    form->addRow(tr("Formato de píxel"), pixels_);
    profile_ = new SelectionComboBox;
    profile_->setObjectName("encoderProfile");
    profile_->addItem(tr("Automático"), "auto");
    for (auto v : { "baseline", "main", "high", "high444" })
        profile_->addItem(v, v);
    form->addRow(tr("Perfil"), profile_);
    level_ = new SelectionComboBox;
    level_->setObjectName("encoderLevel");
    level_->addItem(tr("Automático"), "auto");
    for (auto v : { "3.1", "4.0", "4.1", "4.2", "5.0", "5.1", "5.2", "6.0", "6.1", "6.2" })
        level_->addItem(v, v);
    form->addRow(tr("Nível"), level_);
    videoLayout->addWidget(videoGroup);
    videoLayout->addStretch();
    audioPage_ = new QWidget;
    audioPage_->setObjectName("formatAudioPage");
    auto* audioForm = new QFormLayout(audioPage_);
    audioForm->setSpacing(14);
    audioForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    audio_ = new SwitchControl(tr("Incluir áudio"));
    audio_->setObjectName("encoderKeepAudio");
    audio_->setChecked(true);
    audioForm->addRow(audio_);
    audioCodec_ = new SelectionComboBox;
    audioCodec_->setObjectName("encoderAudioCodec");
    audioForm->addRow(tr("Formato de áudio"), audioCodec_);

    audioRate_ = new SelectionComboBox;
    audioRate_->setObjectName("encoderAudioRate");
    for (int v : { 64, 96, 128, 160, 192, 256, 320 })
        audioRate_->addItem(QString::number(v) + " kbps", v * 1000);
    audioRate_->setCurrentIndex(4);
    audioForm->addRow(tr("Taxa de bits"), audioRate_);
    sampleRate_ = new SelectionComboBox;
    sampleRate_->setObjectName("encoderSampleRate");
    sampleRate_->addItem(tr("Original"), 0);
    sampleRate_->addItem("44.100 Hz", 44100);
    sampleRate_->addItem("48.000 Hz", 48000);
    audioForm->addRow(tr("Frequência de amostragem"), sampleRate_);
    channels_ = new SelectionComboBox;
    channels_->setObjectName("encoderChannels");
    channels_->addItem(tr("Original"), 0);
    channels_->addItem(tr("Mono"), 1);
    channels_->addItem(tr("Estéreo"), 2);
    audioForm->addRow(tr("Canais de áudio"), channels_);
    connect(options, &QPushButton::clicked, this, &EncoderOptionsWidget::showFormatOptions);
    connect(suffix_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncFormat);
    connect(codec_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncControls);
    connect(proresProfile_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncControls);
    connect(rate_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncControls);
    connect(pixels_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncControls);
    connect(profile_, &QComboBox::currentIndexChanged, this, [this] {
        if (profile_->currentData() != "auto" && profile_->currentData() != "high444" && pixels_->currentData() == "yuv444p")
            pixels_->setCurrentIndex(pixels_->findData("yuv420p"));
        syncControls();
    });
    connect(audio_, &QCheckBox::toggled, this, &EncoderOptionsWidget::syncControls);
    connect(audioCodec_, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::syncControls);
    for (auto* combo : { speed_, level_, audioRate_, sampleRate_, channels_ })
        connect(combo, &QComboBox::currentIndexChanged, this, &EncoderOptionsWidget::updateHelp);
    setMediaType(framescale::MediaType::Video);
    auto* presetActions = new QWidget(this);
    presetActions->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* presetLayout = new QHBoxLayout(presetActions);
    presetLayout->setContentsMargins(0, 0, 0, 0);
    presetLayout->setSpacing(6);
    presetLayout->setAlignment(Qt::AlignVCenter);
    root->addWidget(presetActions);
    presets_ = new SelectionComboBox(this);
    presets_->setObjectName("encoderPresets");
    presets_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    presets_->setMinimumWidth(0);
    presets_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    presets_->setMinimumContentsLength(1);
    presets_->setFixedHeight(32);
    presetLayout->addWidget(presets_,1,Qt::AlignVCenter);
    auto* save = new PresetIconButton(tr("Guardar preset"), this);
    save->setObjectName("saveEncoderPreset");
    save->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    save->setFixedHeight(24);
    presetLayout->addWidget(save);
    auto* load = new PresetIconButton(tr("Abrir preset de arquivo…"), this);
    load->setObjectName("importEncoderPreset");
    load->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    load->setFixedHeight(24);

    auto* removePreset = new PresetIconButton(tr("Remover preset"), this);
    removePreset->setObjectName("removeEncoderPreset");
    removePreset->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    removePreset->setFixedHeight(24);
    presetLayout->addWidget(removePreset);
    presetLayout->addWidget(load);
    const ControlSymbols::Symbol presetSymbols[] {ControlSymbols::Symbol::Save,ControlSymbols::Symbol::Trash,ControlSymbols::Symbol::Import};
    int symbolIndex=0;
    for(auto* button : {save,removePreset,load}) {
        button->setToolTip(button->text());
        button->setAccessibleName(button->text());
        button->setText({});
        button->setIcon(ControlSymbols::icon(presetSymbols[symbolIndex++]));
        button->setIconSize(QSize(18,18));
        button->setFixedSize(34,32);
        button->setProperty("presetIconOnly",true);
        button->setAutoDefault(false);
        presetLayout->setAlignment(button,Qt::AlignVCenter);
        button->setProperty("appearanceManaged",true);
        button->setStyleSheet("QPushButton {margin:0;padding:0;min-width:0;min-height:0;border:1px solid palette(mid);border-radius:6px;}");
    }
    removePreset->setEnabled(presets_->currentIndex()>0);
    connect(presets_,&QComboBox::currentIndexChanged,removePreset,[this,removePreset](int index){removePreset->setEnabled(index>0);});
    connect(removePreset, &QPushButton::clicked, this, [this] {
        if (presets_->currentIndex() <= 0)
            return;
        const QString name = presets_->currentText();
        QDialog dialog(this);
        dialog.setObjectName("removePresetDialog");
        dialog.setWindowTitle(tr("Remover preset"));
        auto* layout=new QVBoxLayout(&dialog);
        layout->setContentsMargins(20,16,20,20);
        layout->setSpacing(18);
        auto* message=new QLabel(tr("Remover o preset %1?").arg(name),&dialog);
        message->setTextFormat(Qt::PlainText);
        message->setWordWrap(true);
        layout->addWidget(message);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel,&dialog);
        buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
        auto* confirm=buttons->addButton(tr("Remover"),QDialogButtonBox::AcceptRole);
        confirm->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Trash));
        confirm->setAutoDefault(false);
        buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        layout->addWidget(buttons);
        Appearance::apply(&dialog);
        dialog.setMinimumWidth(320);
        if(dialog.exec()!=QDialog::Accepted) return;
        const auto path = presetFiles_.value(name);
        if (path.isEmpty() || !QFile::remove(path)) {
            setPresetFeedback(tr("Não foi possível remover o preset."));
            return;
        }
        refreshPresets();
        setPresetFeedback(tr("Preset removido."));
    });
    feedback_ = new QLabel(this);
    feedback_->setObjectName("presetFeedback");
    feedback_->setWordWrap(true);
    feedback_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    feedback_->hide();
    root->insertWidget(root->indexOf(presetActions),feedback_);
    // One-time migration keeps existing presets and creates portable files.
    QDir().mkpath(presetDirectory());
    QSettings settings;
    if (!settings.value("filePresetsMigrated", false).toBool()) {
        const auto saved = settings.value("encoderPresets").toMap();
        bool migrated = true;
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            migrated = writePresetFile(it.key(), it.value().toMap()) && migrated;
        if (migrated)
            settings.setValue("filePresetsMigrated", true);
    }
    refreshPresets();
    auto* watcher = new QFileSystemWatcher(this);
    watcher->addPath(presetDirectory());
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, [this] { refreshPresets(presets_->currentText()); });
    connect(presets_, &QComboBox::activated, this, [this](int i) {
        if (i > 0)
            loadPreset(presets_->itemText(i));
    });
    connect(save, &QPushButton::clicked, this, [this] {
        const QString suggested = presets_->currentIndex() > 0 ? presets_->currentText() : tr("Meu preset");
        const QString defaultPath = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                                        .filePath(suggested + ".fspreset");
        QFileDialog dialog(this, tr("Guardar preset"), defaultPath, tr("Preset do FrameScale (*.fspreset)"));
        dialog.setObjectName("savePresetDialog");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setDefaultSuffix("fspreset");
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
            return;
        const QString output = dialog.selectedFiles().first();
        const QString name = QFileInfo(output).completeBaseName();
        if (presetFiles_.contains(name) && QFileInfo(presetFiles_.value(name)).absoluteFilePath() != QFileInfo(output).absoluteFilePath()
            && QMessageBox::question(this, tr("Substituir preset"), tr("Substituir o preset %1?").arg(name)) != QMessageBox::Yes)
            return;
        QSaveFile file(output);
        const auto data = QJsonDocument(QJsonObject { { "format", "framescale.encoder-preset" },
                                            { "version", 1 }, { "name", name }, { "options", QJsonObject::fromVariantMap(values()) } })
                              .toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            setPresetFeedback(tr("Não foi possível guardar o preset."));
            return;
        }
        // Keep a managed library copy, as when importing a portable preset.
        // Removing an entry from the library never deletes the user's export.
        if (!writePresetFile(name, values())) {
            setPresetFeedback(tr("Arquivo guardado, mas não foi possível adicioná-lo à biblioteca."));
            return;
        }
        refreshPresets(name);
        setPresetFeedback({});
        feedback_->setToolTip(QDir::toNativeSeparators(output));
    });
    connect(load, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getOpenFileName(this, tr("Abrir preset"), { },
            tr("Preset do FrameScale (*.fspreset);;Preset antigo (*.json)"));
        if (path.isEmpty())
            return;
        QFile in(path);
        if (!in.open(QIODevice::ReadOnly))
            return;
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(in.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject() || (!doc.object().contains("codec") && (doc.object().value("format").toString() != "framescale.encoder-preset" || doc.object().value("version").toInt() != 1 || !doc.object().value("options").toObject().contains("codec")))) {
            setPresetFeedback(tr("Arquivo de preset inválido."));
            return;
        }
        const auto object = doc.object();
        applyValues((object.contains("options") ? object.value("options").toObject() : object).toVariantMap());
        QString name = object.value("name").toString(QFileInfo(path).completeBaseName());
        QString unique = name;
        for (int n = 2; presetFiles_.contains(unique); ++n)
            unique = name + "_" + QString::number(n);
        savePreset(unique);
    });
}
void EncoderOptionsWidget::setPresetFeedback(const QString& message)
{
    feedback_->setText(message);
    feedback_->setVisible(!message.isEmpty());
}

QVariantMap EncoderOptionsWidget::values() const
{
    QVariantMap result = legacy_;
    if (enhancementPresetValues)
        result.insert("enhancement", enhancementPresetValues());
    result.insert("codec", codec_->currentData());
    result.insert("prores_profile", proresProfile_->currentData());
    result.insert("suffix", suffix_->currentData());
    result.insert("speed", speed_->currentData());
    result.insert("pixels", pixels_->currentData());
    result.insert("crf", quality_->value());
    result.insert("audio", audio_->isChecked());
    result.insert("metadata", metadata_->isChecked());
    result.insert("rate_control", rate_->currentData());
    result.insert("bit_rate", rate_->currentData() == "crf" ? 0 : qRound(targetRate_->value() * 1000000));
    result.insert("profile", profile_->currentData());
    result.insert("level", level_->currentData());
    result.insert("audio_codec", audioCodec_->currentData());
    result.insert("audio_rate", audioRate_->currentData());
    result.insert("sample_rate", sampleRate_->currentData());
    result.insert("channels", channels_->currentData());
    return result;
}
void EncoderOptionsWidget::applyValues(const QVariantMap& v)
{
    legacy_ = v;
    if (applyEnhancementPresetValues)
        applyEnhancementPresetValues(v.value("enhancement").toMap());
    auto choose = [](QComboBox* combo, const QVariant& value) { const int i=combo->findData(value); if(i>=0)combo->setCurrentIndex(i); };
    choose(suffix_, v.value("codec") == "prores_ks" ? QVariant(".mov") : v.value("suffix", ".mp4"));
    syncFormat();
    choose(codec_, v.value("codec", "libx264"));
    choose(proresProfile_, v.value("prores_profile", 3));
    choose(speed_, v.value("speed", "medium"));
    choose(pixels_, v.value("pixels", "yuv420p"));
    quality_->setValue(v.value("crf", 18).toInt());
    audio_->setChecked(v.value("audio", true).toBool());
    metadata_->setChecked(v.value("metadata", true).toBool());
    choose(rate_, v.value("rate_control", "crf"));
    targetRate_->setValue(v.value("bit_rate", 8000000).toDouble() > 0 ? v.value("bit_rate").toDouble() / 1000000. : 8.);
    choose(profile_, v.value("profile", "auto"));
    choose(level_, v.value("level", "auto"));
    choose(audioCodec_, v.value("audio_codec", "copy"));
    choose(audioRate_, v.value("audio_rate", 192000));
    choose(sampleRate_, v.value("sample_rate", 0));
    choose(channels_, v.value("channels", 0));
    syncControls();
    if (presets_)
        presets_->setCurrentIndex(0);
}
void EncoderOptionsWidget::applyOptions(const framescale::ProcessingOptions& o)
{
    setMediaType(o.mediaType);
    QVariantList custom;
    for (auto& item : o.customEncoderOptions)
        custom.append(QVariantMap { { "option", QString::fromStdString(item.first) }, { "value", QString::fromStdString(item.second) } });
    applyValues({ { "enhancement", enhancementValues(o.enhancement) }, { "codec", QString::fromStdString(o.videoCodec) }, { "prores_profile", o.proresProfile }, { "suffix", QString::fromStdString(o.outputSuffix) },
        { "speed", QString::fromStdString(o.encoderPreset) }, { "pixels", QString::fromStdString(o.pixelFormat) },
        { "crf", o.crf }, { "audio", o.keepAudio }, { "metadata", o.keepMetadata }, { "rate_control", QString::fromStdString(o.rateControl) },
        { "bit_rate", o.bitRate }, { "profile", QString::fromStdString(o.videoProfile) }, { "level", QString::fromStdString(o.videoLevel) },
        { "audio_codec", QString::fromStdString(o.audioCodec) }, { "audio_rate", o.audioBitRate }, { "sample_rate", o.audioSampleRate }, { "channels", o.audioChannels },
        { "buffer", o.bufferSize }, { "min_rate", o.minRate }, { "max_rate", o.maxRate }, { "qmin", o.qmin }, { "qmax", o.qmax }, { "custom", custom } });
}
void EncoderOptionsWidget::writeOptions(framescale::ProcessingOptions& o) const
{
    const auto v = values();
    o.enhancement = enhancementFromValues(v.value("enhancement").toMap());
    o.videoCodec = v.value("codec").toString().toStdString();
    o.proresProfile = v.value("prores_profile", 3).toInt();
    o.outputSuffix = v.value("suffix").toString().toStdString();
    o.encoderPreset = v.value("speed").toString().toStdString();
    o.pixelFormat = v.value("pixels").toString().toStdString();
    o.crf = v.value("crf").toInt();
    o.keepAudio = v.value("audio").toBool();
    o.keepMetadata = v.value("metadata").toBool();
    o.rateControl = v.value("rate_control").toString().toStdString();
    o.bitRate = v.value("bit_rate").toInt();
    o.videoProfile = v.value("profile").toString().toStdString();
    o.videoLevel = v.value("level").toString().toStdString();
    o.audioCodec = v.value("audio_codec").toString().toStdString();
    o.audioBitRate = v.value("audio_rate").toInt();
    o.audioSampleRate = o.audioCodec == "copy" ? 0 : v.value("sample_rate").toInt();
    o.audioChannels = o.audioCodec == "copy" ? 0 : v.value("channels").toInt();
    o.bufferSize = v.value("buffer").toInt();
    o.minRate = v.value("min_rate").toInt();
    o.maxRate = v.value("max_rate").toInt();
    o.qmin = v.value("qmin", -1).toInt();
    o.qmax = v.value("qmax", -1).toInt();
    o.customEncoderOptions.clear();
    for (auto item : v.value("custom").toList()) {
        auto row = item.toMap();
        o.customEncoderOptions.emplace_back(row.value("option").toString().toStdString(), row.value("value").toString().toStdString());
    }
}
void EncoderOptionsWidget::setMediaType(framescale::MediaType type)
{
    mediaType_ = type;
    const QString selected = suffix_->currentData().toString();
    {
        QSignalBlocker block(suffix_);
        suffix_->clear();
        if (type == framescale::MediaType::Image) {
            suffix_->addItem("PNG", ".png");
            suffix_->addItem("JPEG", ".jpg");
            suffix_->addItem("WebP", ".webp");
            suffix_->addItem("BMP", ".bmp");
        } else {
            suffix_->addItem("MP4", ".mp4");
            suffix_->addItem("QuickTime", ".mov");
            suffix_->addItem("Matroska", ".mkv");
            suffix_->addItem("AVI", ".avi");
            suffix_->addItem("MP3", ".mp3");
            suffix_->addItem("WAV", ".wav");
            suffix_->addItem(tr("Sequência PNG"), ".png");
            suffix_->addItem(tr("Sequência JPEG"), ".jpg");
            suffix_->addItem("GIF", ".gif");
        }
        suffix_->setCurrentIndex(qMax(0, suffix_->findData(selected)));
    }
    syncFormat();
}
void EncoderOptionsWidget::syncFormat()
{
    const QString suffix = suffix_->currentData().toString();
    const QString previous = codec_->currentData().toString();
    const QString audioPrevious = audioCodec_->currentData().toString();
    QSignalBlocker videoBlock(codec_), audioBlock(audioCodec_);
    codec_->clear();
    audioCodec_->clear();
    if (suffix == ".mp4" || suffix == ".mkv" || suffix == ".mov") {
        if (suffix == ".mov")
            codec_->addItem("Apple ProRes", "prores_ks");
        codec_->addItem("H.264 / AVC", "libx264");
        codec_->addItem("H.265 / HEVC", "libx265");

    } else if (suffix == ".avi") {
        codec_->addItem("Motion JPEG", "mjpeg");
        codec_->addItem("H.264 / AVC", "libx264");
    } else if (suffix == ".png")
        codec_->addItem("PNG", "png");
    else if (suffix == ".jpg")
        codec_->addItem("JPEG", "mjpeg");
    else if (suffix == ".gif")
        codec_->addItem("GIF", "gif");
    else if (suffix == ".webp")
        codec_->addItem("WebP", "libwebp");
    else if (suffix == ".bmp")
        codec_->addItem("BMP", "bmp");
    codec_->setCurrentIndex(suffix == ".mov" ? 0 : qMax(0, codec_->findData(previous)));
    if (suffix == ".mp3")
        audioCodec_->addItem(tr("MP3 — alta compatibilidade"), "libmp3lame");
    else if (suffix == ".wav")
        audioCodec_->addItem(tr("PCM — sem compressão"), "pcm_s16le");
    else if (suffix == ".avi") {
        audioCodec_->addItem(tr("PCM — sem compressão"), "pcm_s16le");
        audioCodec_->addItem(tr("MP3 — alta compatibilidade"), "libmp3lame");
    } else {
        audioCodec_->addItem(tr("Manter áudio original"), "copy");
        audioCodec_->addItem(tr("AAC — arquivo menor"), "aac");
        if (suffix == ".mov" || suffix == ".mkv")
            audioCodec_->addItem(tr("PCM — sem compressão"), "pcm_s16le");
    }
    audioCodec_->setCurrentIndex(qMax(0, audioCodec_->findData(audioPrevious)));
    while (details_->count())
        details_->removeTab(0);
    const bool audioOnly = suffix == ".mp3" || suffix == ".wav";
    const bool video = mediaType_ == framescale::MediaType::Video;
    if (!audioOnly)
        details_->addTab(videoPage_, video ? tr("Vídeo") : tr("Imagem"));
    if (video && (audioOnly || suffix == ".avi" || suffix == ".mp4" || suffix == ".mov" || suffix == ".mkv"))
        details_->addTab(audioPage_, tr("Áudio"));
    audio_->setEnabled(!audioOnly);
    if (audioOnly)
        audio_->setChecked(true);
    syncControls();
}
void EncoderOptionsWidget::syncControls()
{
    QSignalBlocker profileBlock(profile_), pixelsBlock(pixels_);
    const auto codec = codec_->currentData().toString();
    const bool configurable = codec == "libx264" || codec == "libx265";
    auto* form = qobject_cast<QFormLayout*>(videoPage_->findChild<QGroupBox*>()->layout());
    form->setRowVisible(proresProfile_, codec == "prores_ks");
    form->setRowVisible(rate_, configurable);
    form->setRowVisible(quality_, configurable && rate_->currentData() == "crf");
    form->setRowVisible(targetRate_, configurable && rate_->currentData() != "crf");
    form->setRowVisible(speed_, configurable);
    form->setRowVisible(pixels_, configurable);
    form->setRowVisible(profile_, codec == "libx264");
    form->setRowVisible(level_, configurable);
    if (codec != "libx264" || (pixels_->currentData() == "yuv444p" && profile_->currentData() != "high444"))
        profile_->setCurrentIndex(0);
    if (codec == "prores_ks")
        pixels_->setCurrentIndex(pixels_->findData(QString::fromStdString(framescale::proresPixelFormat(proresProfile_->currentData().toInt()))));
    else if (codec == "mjpeg")
        pixels_->setCurrentIndex(pixels_->findData("yuvj420p"));
    else if (configurable && (pixels_->currentData() == "yuv422p10le" || pixels_->currentData() == "yuv444p10le" || pixels_->currentData() == "yuvj420p"))
        pixels_->setCurrentIndex(pixels_->findData("yuv420p"));
    for (int i = 0; i < pixels_->count(); ++i)
        pixels_->setItemData(i, i < 3 ? QVariant() : QVariant(0), Qt::UserRole - 1);
    audioCodec_->setEnabled(audio_->isChecked());
    const QString audio = audioCodec_->currentData().toString();
    auto* audioForm = qobject_cast<QFormLayout*>(audioPage_->layout());
    const bool audioEnabled = audio_->isChecked();
    audioForm->setRowVisible(audioCodec_, audioEnabled);
    audioForm->setRowVisible(audioRate_, audioEnabled && (audio == "aac" || audio == "libmp3lame"));
    audioForm->setRowVisible(sampleRate_, audioEnabled && audio != "copy");
    audioForm->setRowVisible(channels_, audioEnabled && audio != "copy");
    audioRate_->setEnabled(audioEnabled && (audio == "aac" || audio == "libmp3lame"));
    sampleRate_->setEnabled(audioEnabled && audio != "copy");
    channels_->setEnabled(audioEnabled && audio != "copy");
    const QString suffix = suffix_->currentData().toString();
    const bool onlyAudio = suffix == ".mp3" || suffix == ".wav";
    summary_->setText(onlyAudio ? tr("Apenas áudio · %1").arg(audioCodec_->currentText()) : ((mediaType_ == framescale::MediaType::Video && (suffix == ".png" || suffix == ".jpg")) ? tr("Fotogramas numerados numa pasta · %1").arg(codec_->currentText()) : codec_->currentText()));
    if (codec == "prores_ks")
        summary_->setText("Apple " + proresProfile_->currentText());
    updateHelp();
}
void EncoderOptionsWidget::updateHelp()
{
    auto field = [](QWidget* widget, const QString& title, const QString& text) {
        const QString help = helpToolTip(title, text);
        widget->setToolTip(help);
        widget->setToolTipDuration(15000);
        if (auto* form = qobject_cast<QFormLayout*>(widget->parentWidget()->layout()))
            if (auto* label = form->labelForField(widget)) label->setToolTip(help);
    };
    auto choices = [&](QComboBox* combo, const QString& title, const QString& help, const QMap<QString, QString>& tips) {
        for (int i = 0; i < combo->count(); ++i)
            combo->setItemData(i, helpToolTip(combo->itemText(i), tips.value(combo->itemData(i).toString())), Qt::ToolTipRole);
        // Explain the field once. Each menu item explains its own choice.
        field(combo, title, help);
    };
    // The format selector is self-explanatory; keep guidance for technical
    // encoding settings, rather than announcing every ordinary control.
    field(suffix_, {}, {});
    for (int i=0;i<suffix_->count();++i) suffix_->setItemData(i,{},Qt::ToolTipRole);
    choices(codec_, tr("Codec de vídeo"), tr("Define como o vídeo é comprimido. H.264 facilita a partilha, H.265 pode ocupar menos espaço e ProRes é indicado para edição."), {
        {"libx264", tr("Boa escolha para partilha e reprodução geral.\nCompatível com a maioria dos dispositivos.")},
        {"libx265", tr("Pode manter a qualidade com ficheiros menores.\nDemora mais e exige um leitor compatível.")},
        {"prores_ks", tr("Para edição e ficheiros intermédios.\nO perfil define a qualidade e o tamanho.")},
        {"mjpeg", tr("Comprime cada fotograma como uma imagem JPEG.\nFacilita a edição, mas gera ficheiros grandes.")},
        {"png", tr("Guarda a imagem sem perda.\nTambém preserva a transparência.")},
        {"gif", tr("Cria uma animação com poucas cores.\nNão preserva todos os gradientes.")},
        {"libwebp", tr("Comprime imagens e mantém transparência.\nRequer uma aplicação compatível com WebP.")},
        {"bmp", tr("Guarda uma imagem num formato simples.\nOcupa mais espaço que PNG.")}
    });
    choices(proresProfile_, tr("Perfil ProRes"), tr("Define a qualidade e a informação de cor.\nPerfis maiores também geram ficheiros maiores."), {
        {"0", tr("Ficheiros leves para pré-edição.\nPrefira 422 ou HQ para a entrega final.")},
        {"1", tr("Boa qualidade com menos espaço.\nÉ uma opção intermédia entre Proxy e 422.")},
        {"2", tr("Equilibra qualidade e tamanho para edição.\nOcupa menos espaço que HQ.")},
        {"3", tr("Preserva mais detalhes que o perfil 422.\nIndicado para masters de edição.")},
        {"4", tr("Mantém a resolução completa das cores.\nNesta versão, o vídeo é exportado sem transparência.")},
        {"5", tr("Máxima qualidade de cor entre estes perfis.\nGera ficheiros grandes e não exporta transparência.")}
    });
    choices(rate_, tr("Controlo de qualidade"), tr("CRF prioriza qualidade e deixa o tamanho variar. VBR procura uma taxa média; CBR mantém os dados mais constantes quando o destino exige isso."), {
        {"crf", tr("Mantém uma qualidade visual aproximada.\nO tamanho final varia conforme as cenas.")},
        {"vbr", tr("Procura atingir uma taxa média de dados.\nCenas complexas podem usar mais dados.")},
        {"cbr", tr("Mantém a taxa de dados mais estável.\nUse quando o destino exigir uma taxa fixa.")}
    });
    field(quality_, tr("Qualidade (CRF)"), tr("Quanto menor o valor, mais detalhe e maior o ficheiro. Em H.264, comece em 18; baixar o CRF não recupera detalhes ausentes na origem."));
    field(targetRate_, tr("Taxa de bits"), tr("Quantidade de dados usada por segundo de vídeo.\nAumentar melhora a qualidade e ocupa mais espaço."));
    QMap<QString, QString> speeds;
    const QStringList speedHelp {
        tr("Usa o menor tempo de processamento para salvar o vídeo.\nEm CRF, costuma gerar arquivos maiores que os outros modos."),
        tr("Salva o vídeo muito depressa, com pouca busca por compressão.\nEm CRF, o arquivo pode ficar maior que em Muito rápida."),
        tr("Prioriza terminar a exportação rapidamente.\nEm CRF, costuma ocupar mais espaço que os modos lentos."),
        tr("Dedica menos tempo à compressão que o modo Rápida.\nÉ uma opção quando o tempo de exportação importa mais."),
        tr("Exporta mais depressa que o modo Média.\nEm CRF, pode gerar um arquivo maior com qualidade semelhante."),
        tr("Equilibra o tempo de exportação e o tamanho do arquivo.\nComece por este modo se não tiver uma preferência."),
        tr("Dedica mais tempo à compressão que o modo Média.\nEm CRF, pode gerar um arquivo menor com qualidade semelhante."),
        tr("Procura comprimir melhor que o modo Lenta.\nA exportação demora mais e a economia de espaço varia."),
        tr("Dedica o maior tempo à compressão entre estas opções.\nO ganho de espaço pode ser pequeno em relação ao tempo gasto.")
    };
    for (int i = 0; i < speed_->count(); ++i)
        speeds.insert(speed_->itemData(i).toString(), speedHelp.value(i));
    choices(speed_, tr("Velocidade"), tr("Controla o tempo que o encoder dedica a comprimir o vídeo.\nNão altera os FPS nem a velocidade de reprodução."), speeds);
    choices(pixels_, tr("Formato de píxel"), tr("Define como a cor é guardada. yuv420p tem maior compatibilidade; yuv444p preserva melhor texto e bordas coloridas, mas exige suporte do leitor."), {
        {"auto", tr("Usa yuv420p nesta exportação.\nÉ a escolha mais compatível para H.264 e H.265.")},
        {"yuv420p", tr("Cor de 8 bits com menor resolução de cor.\nCompatível com a maioria dos dispositivos.")},
        {"yuv444p", tr("Cor de 8 bits com resolução de cor completa.\nÚtil para texto; exige um leitor compatível.")},
        {"yuv422p10le", tr("Cor de 10 bits, usada no ProRes 422.\nNão recupera cores perdidas na origem.")},
        {"yuv444p10le", tr("Cor de 10 bits para alimentar o ProRes 4444.\nMantém a resolução completa das cores.")},
        {"yuvj420p", tr("Usado pelo Motion JPEG.\nÉ selecionado automaticamente para esse codec.")}
    });
    choices(profile_, tr("Perfil"), tr("Define os recursos de H.264 exigidos do leitor. Use Automático, salvo se o dispositivo pedir especificamente Baseline, Main ou High."), {
        {"auto", tr("Escolhe um perfil adequado ao formato de píxel.\nUse quando o destino não exigir outro perfil.")},
        {"baseline", tr("Para leitores H.264 antigos.\nComprime menos eficientemente que High.")},
        {"main", tr("Para dispositivos que não aceitam High.\nEquilibra compatibilidade e compressão.")},
        {"high", tr("Boa compressão para vídeo 4:2:0.\nAceite pela maioria dos leitores atuais.")},
        {"high444", tr("Permite manter a resolução completa das cores.\nExige um leitor H.264 compatível com 4:4:4.")}
    });
    QMap<QString, QString> levels;
    for (int i = 0; i < level_->count(); ++i)
        levels.insert(level_->itemData(i).toString(), i == 0
            ? tr("O encoder escolhe o nível adequado ao vídeo.\nEvita limites desnecessários na resolução e no FPS.")
            : tr("Limita a resolução, o FPS e a taxa de bits.\nUse apenas se o dispositivo exigir este nível."));
    choices(level_, tr("Nível"), tr("Limita resolução, FPS e taxa de dados. Use Automático: um nível maior não melhora a imagem e um nível baixo pode impedir a exportação."), levels);
    field(audio_, tr("Áudio"), tr("Inclui o áudio existente no ficheiro original.\nDesative para exportar um vídeo silencioso."));
    choices(audioCodec_, tr("Codec de áudio"), tr("Define como o áudio será guardado.\nCopiar evita compressão quando for compatível."), {
        {"copy", tr("Mantém o áudio original quando possível.\nPode converter para cumprir o formato ou o corte.")},
        {"aac", tr("Áudio compacto para MP4 e MOV.\n192 kbps é um bom início para estéreo.")},
        {"pcm_s16le", tr("Áudio sem compressão, com 16 bits.\nÚtil para edição, mas ocupa mais espaço.")},
        {"libmp3lame", tr("Áudio comprimido com ampla compatibilidade.\nTaxas maiores preservam melhor a música.")}
    });
    choices(audioRate_, tr("Taxa de bits de áudio"), tr("Define os dados usados por segundo de áudio.\nUma taxa maior preserva mais detalhes."), {});
    choices(sampleRate_, tr("Frequência de amostragem"), tr("Define o número de amostras de áudio por segundo.\nMantenha Original, salvo exigência do destino."), {
        {"0", tr("Mantém a frequência do áudio original.\nEvita uma conversão desnecessária.")},
        {"44100", tr("Frequência comum em música e áudio de CD.\nUse se o destino exigir 44.100 Hz.")},
        {"48000", tr("Frequência comum em produção de vídeo.\nUse se o destino exigir 48.000 Hz.")}
    });
    choices(channels_, tr("Canais"), tr("Define quantos canais de áudio serão exportados.\nReduzir os canais mistura o áudio original."), {
        {"0", tr("Mantém os canais da origem quando possível.\nEvita converter áudio multicanal.")},
        {"1", tr("Mistura o áudio num único canal.\nÚtil para voz ou destinos que pedem mono.")},
        {"2", tr("Exporta dois canais: esquerdo e direito.\nÁudio multicanal é misturado para estéreo.")}
    });
    field(metadata_, tr("Metadados"), tr("Copia informações como título e descrição.\nDesative para retirar esses dados da exportação."));
}
void EncoderOptionsWidget::showFormatOptions()
{
    const auto before = values();
    QDialog dialog(this);
    dialog.setObjectName("formatOptionsDialog");
    dialog.setWindowTitle(tr("Opções de formato — %1").arg(suffix_->currentText()));
    dialog.resize(500, 550);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(20);
    auto* scroll = new QScrollArea(&dialog);
    scroll->setObjectName("formatOptionsScroll");
    scroll->viewport()->setObjectName("formatOptionsViewport");
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(details_);
    details_->setAutoFillBackground(false);
    videoPage_->setAutoFillBackground(false);
    audioPage_->setAutoFillBackground(false);
    details_->show();
    layout->addWidget(scroll, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    Appearance::apply(&dialog);
    dialog.setMinimumSize(480, 460);
    dialog.resize(500, 550);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    scroll->takeWidget();
    details_->hide();
    details_->setParent(this);
    if (!accepted)
        applyValues(before);
    syncControls();
}
QString EncoderOptionsWidget::presetDirectory() const
{
    const QString overridePath = qEnvironmentVariable("FRAMESCALE_PRESET_DIR");
    return overridePath.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Presets" : overridePath;
}
bool EncoderOptionsWidget::writePresetFile(const QString& name, const QVariantMap& options)
{
    if (!QDir().mkpath(presetDirectory()))
        return false;
    // Hash only the filename so display names cannot escape the preset directory.
    const auto id = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
    QString readable = name.left(64);
    readable.replace(QRegularExpression("[^a-zA-Z0-9 _-]"), "_");
    const QString path = presetFiles_.value(name, QDir(presetDirectory()).filePath("preset-" + readable + "-" + QString::fromLatin1(id) + ".fspreset"));
    QSaveFile file(path);
    const auto data = QJsonDocument(QJsonObject { { "format", "framescale.encoder-preset" }, { "version", 1 }, { "name", name }, { "options", QJsonObject::fromVariantMap(options) } }).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
void EncoderOptionsWidget::refreshPresets(const QString& selected)
{
    QSignalBlocker block(presets_);
    presets_->clear();
    presets_->addItem(tr("Personalizado"));
    presetFiles_.clear();
    for (const auto& path : QDir(presetDirectory()).entryInfoList({ "*.fspreset" }, QDir::Files, QDir::Name)) {
        QFile file(path.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        auto object = QJsonDocument::fromJson(file.readAll()).object();
        const auto name = object.value("name").toString();
        if (object.value("format").toString() != "framescale.encoder-preset" || object.value("version").toInt() != 1 || name.isEmpty() || !object.value("options").toObject().contains("codec"))
            continue;
        presetFiles_.insert(name, path.absoluteFilePath());
    }
    for (auto it = presetFiles_.cbegin(); it != presetFiles_.cend(); ++it)
        presets_->addItem(it.key(), it.value());
    presets_->setCurrentIndex(qMax(0, presets_->findText(selected)));
    presets_->setToolTip(QDir::toNativeSeparators(presetDirectory()));
}
void EncoderOptionsWidget::savePreset(const QString& name)
{
    if (name.trimmed().isEmpty())
        return;
    const bool saved = writePresetFile(name.trimmed(), values());
    refreshPresets(name.trimmed());
    setPresetFeedback(saved ? QString() : tr("Não foi possível guardar o preset."));
}
bool EncoderOptionsWidget::loadPreset(const QString& name)
{
    refreshPresets(name);
    QFile file(presetFiles_.value(name));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    auto object = QJsonDocument::fromJson(file.readAll()).object();
    applyValues(object.value("options").toObject().toVariantMap());
    presets_->setCurrentText(name);
    setPresetFeedback(tr("Preset aplicado."));
    return true;
}

void EncoderOptionsWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange && summary_)
        syncControls();
}
