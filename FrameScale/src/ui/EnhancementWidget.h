#pragma once
#include "EditorControls.h"
#include "HelpToolTip.h"
#include "core/ProcessingOptions.h"
#include <QButtonGroup>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantMap>
#include <array>
#include <functional>

class FilterSlider final : public QSlider {
public:
    explicit FilterSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent) {}
protected:
    void mouseMoveEvent(QMouseEvent* event) override {
        QSlider::mouseMoveEvent(event);
        if (isSliderDown()) snap();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (isSliderDown()) snap();
        QSlider::mouseReleaseEvent(event);
    }
private:
    void snap() {
        const int mark = qRound(sliderPosition() / 25.) * 25;
        if (mark >= minimum() && mark <= maximum() && qAbs(sliderPosition() - mark) <= 2)
            setSliderPosition(mark);
    }
};

inline QVariantMap enhancementValues(const framescale::EnhancementOptions& o)
{
    return { { "enabled", o.enabled }, { "model", o.model }, { "denoise", o.denoise }, { "sharpen", o.sharpen },
        { "deblock", o.deblock }, { "deband", o.deband }, { "grain", o.grain }, { "deinterlace", o.deinterlace },
        { "contrast", o.contrast }, { "vibrance", o.vibrance } };
}
inline framescale::EnhancementOptions enhancementFromValues(const QVariantMap& values)
{
    framescale::EnhancementOptions o;
    o.enabled = values.value("enabled", false).toBool();
    o.model = std::clamp(values.value("model", 0).toInt(), 0, 3);
    o.denoise = std::clamp(values.value("denoise").toInt(), 0, 100);
    o.sharpen = std::clamp(values.value("sharpen").toInt(), 0, 100);
    o.deblock = std::clamp(values.value("deblock").toInt(), 0, 100);
    o.deband = std::clamp(values.value("deband").toInt(), 0, 100);
    o.grain = std::clamp(values.value("grain").toInt(), 0, 100);
    o.clarity = 0; // Ignore the removed filter in older presets.
    o.contrast = std::clamp(values.value("contrast").toInt(), -100, 100);
    o.vibrance = std::clamp(values.value("vibrance").toInt(), -100, 100);
    o.deinterlace = std::clamp(values.value("deinterlace").toInt(), 0, 3);
    return o;
}

class EnhancementWidget final : public QWidget {
public:
    explicit EnhancementWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("enhancementPanel");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(12);
        toggle_ = new SwitchControl(tr("Melhoria de imagem"), this);
        toggle_->setObjectName("enableEnhancement");
        toggle_->setToolTip(helpToolTip(tr("Melhoria de imagem"),
            tr("Aplica filtros ou restauração antes de exportar. Use quando houver defeitos na origem; tratamento excessivo pode apagar detalhes de uma imagem já limpa.")));
        layout->addWidget(toggle_);
        fields_ = new QWidget(this);
        fields_->setObjectName("enhancementFields");
        auto* form = new QVBoxLayout(fields_);
        form->setContentsMargins(0, 0, 0, 0);
        form->setSpacing(10);
        form->addWidget(new ControlSymbols::Label(tr("Modelo de restauração"), ControlSymbols::Symbol::Layers, fields_));
        models_ = new QButtonGroup(this);
        auto* grid = new QGridLayout;
        grid->setSpacing(6);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        const QStringList modelNames { tr("Somente filtros"), "Real-ESRGAN Plus", "Real-ESRGAN Anime", "Real-CUGAN SE" };
        const QStringList modelHints {
            tr("Usa apenas os filtros abaixo, sem IA.\nCom todos a zero, mantém a imagem original."),
            tr("Restaura fotografias e filmagens.\nPode alterar texturas; compare em Preview."),
            tr("Restaura desenhos e anime.\nDefine contornos, mas pode simplificar detalhes."),
            tr("Limpa anime com ruído ou compressão.\nCompare as linhas e texturas em Preview.") };
        for (int i = 0; i < modelNames.size(); ++i) {
            auto* button = new QToolButton(fields_);
            button->setObjectName(QString("enhancementModel%1").arg(i));
            button->setText(modelNames[i]);
            button->setAccessibleName(modelNames[i]);
            button->setToolTip(helpToolTip(modelNames[i], modelHints[i]));
            button->setCheckable(true);
            button->setFixedHeight(28);
            button->setStyleSheet("QToolButton {font-size:12px;padding:2px 3px;border-radius:6px;} QToolButton:hover {background:rgba(0,0,0,40);}");
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            models_->addButton(button, i);
            grid->addWidget(button, i / 2, i % 2);
        }
        models_->button(0)->setChecked(true);
        form->addLayout(grid);
        connect(models_, &QButtonGroup::idClicked, this, [this](int) { updateHint(); });

        // Captions have their own rows. Every slider shares the same two
        // columns, so text length and translated labels cannot shorten it.
        auto* filters = new QGridLayout;
        filters->setContentsMargins(0, 0, 0, 0);
        filters->setHorizontalSpacing(8);
        filters->setVerticalSpacing(3);
        filters->setColumnStretch(0, 1);
        filters->setColumnStretch(1, 0);
        const QStringList labels { tr("Redução de ruído"), tr("Nitidez"), tr("Reduzir blocos"), tr("Reduzir banding"), tr("Grão"), tr("Contraste"), tr("Intensidade das cores") };
        const QStringList names { "enhanceDenoise", "enhanceSharpen", "enhanceDeblock", "enhanceDeband", "enhanceGrain", "enhanceContrast", "enhanceVibrance" };
        const QStringList hints {
            tr("Suaviza pontos e manchas, sobretudo nas áreas escuras. Reduza a intensidade se pele, cabelo ou texturas ficarem lisos demais."),
            tr("Realça as bordas para dar uma aparência mais nítida. Valores altos podem criar halos e destacar ruído, sem recuperar detalhes perdidos."),
            tr("Suaviza os quadrados de vídeos muito comprimidos. Em material sem esses blocos, pode reduzir detalhes sem melhorar a imagem."),
            tr("Suaviza faixas de cor em céus e sombras. Use pouca intensidade para preservar variações delicadas; a compressão final pode recriar as faixas."),
            tr("Adiciona grão para dar textura ou disfarçar gradientes muito lisos. Pode aumentar o ficheiro; deixe a zero para um resultado limpo."),
            tr("Separa os tons claros dos escuros com uma curva suave. Valores negativos suavizam a imagem; positivos deixam o visual mais marcado."),
            tr("Reforça principalmente as cores menos saturadas. Use pouco em rostos; valores negativos deixam as cores mais discretas.") };
        const int valueWidth = qMax(50, fontMetrics().horizontalAdvance("-100") + 20);
        for (int i = 0; i < int(values_.size()); ++i) {
            auto* label = new ControlSymbols::Label(labels[i], ControlSymbols::forControl(names[i]), fields_);
            auto* slider = new FilterSlider(fields_);
            slider->setRange(i >= 5 ? -100 : 0, 100);
            slider->setObjectName(names[i] + "Slider");
            slider->setAccessibleName(labels[i]);
            slider->setFixedHeight(24);
            slider->setMinimumWidth(0);
            slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            auto* value = new QSpinBox(fields_);
            values_[i] = value;
            value->setRange(i >= 5 ? -100 : 0, 100);
            value->setButtonSymbols(QAbstractSpinBox::NoButtons);
            value->setFixedWidth(valueWidth);
            value->setKeyboardTracking(false);
            value->setAlignment(Qt::AlignCenter);
            value->setStyleSheet("QSpinBox {padding:1px 6px;min-height:18px;}");
            value->setFixedHeight(24);
            value->setObjectName(names[i]);
            value->setAccessibleName(labels[i]);
            const QString hint = helpToolTip(labels[i], hints[i]);
            value->setToolTip(hint);
            slider->setToolTip(hint);
            label->setToolTip(hint);
            label->setBuddy(value);
            label->setMinimumWidth(0);
            label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            filters->addWidget(label, i * 3, 0, 1, 2);
            filters->addWidget(slider, i * 3 + 1, 0);
            filters->addWidget(value, i * 3 + 1, 1);
            if (i < int(values_.size()) - 1) filters->setRowMinimumHeight(i * 3 + 2, 5);
            connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
            connect(value, &QSpinBox::valueChanged, slider, &QSlider::setValue);
            connect(value, &QSpinBox::valueChanged, this, [this] {
                if (!restoring_) updateHint();
            });
        }
        form->addLayout(filters);
        auto* reset = new QPushButton(tr("Repor ajustes"), fields_);
        reset->setObjectName("resetEnhancement");
        reset->setFixedHeight(24);
        reset->setStyleSheet("QPushButton {padding:1px 8px;min-height:18px;font-weight:500;}");
        connect(reset, &QPushButton::clicked, this, [this] {
            auto o = options();
            o.denoise = o.sharpen = o.deblock = o.deband = o.grain = o.deinterlace = 0;
            o.clarity = o.contrast = o.vibrance = 0;
            setOptions(o);
        });
        form->addWidget(reset, 0, Qt::AlignLeft);
        layout->addWidget(fields_);
        layout->setAlignment(Qt::AlignTop);
        fields_->hide();
        connect(toggle_, &QCheckBox::toggled, this, [this](bool enabled) {
            Appearance::revealSection(fields_, enabled);
            if (!restoring_) updateHint();
        });
        updateHint();
        setStyleSheet("QToolButton {background:#292929;border:1px solid #444;border-radius:6px;color:#ddd;padding:4px;font-size:11px;} QToolButton:hover {background:#343434;} QToolButton:checked {border:1px solid #eee;background:#363636;color:white;} QLabel#enhancementPreviewHint {color:#bcbcbc;font-size:11px;} QSlider::groove:horizontal {height:4px;background:#484848;border-radius:2px;} QSlider::sub-page:horizontal {background:#1473e6;} QSlider::handle:horizontal {background:#dedede;width:12px;margin:-4px 0;border-radius:6px;}");
    }
    framescale::EnhancementOptions options() const
    {
        framescale::EnhancementOptions o;
        o.enabled = toggle_->isChecked();
        o.model = models_->checkedId();
        o.denoise = values_[0]->value();
        o.sharpen = values_[1]->value();
        o.deblock = values_[2]->value();
        o.deband = values_[3]->value();
        o.grain = values_[4]->value();
        o.clarity = 0;
        o.contrast = values_[5]->value();
        o.vibrance = values_[6]->value();
        o.deinterlace = 0;
        return o;
    }
    void setOptions(const framescale::EnhancementOptions& o)
    {
        restoring_ = true;
        models_->button(std::clamp(o.model, 0, 3))->setChecked(true);
        toggle_->setChecked(o.enabled);
        const std::array<int, 7> values { o.denoise, o.sharpen, o.deblock, o.deband, o.grain, o.contrast, o.vibrance };
        for (int i = 0; i < int(values.size()); ++i)
            values_[i]->setValue(values[i]);
        restoring_ = false;
        updateHint();
    }

    std::function<void()> restorationChanged;

private:
    bool allFiltersOff() const
    {
        return std::all_of(values_.begin(), values_.end(), [](const QSpinBox* value) { return value->value() == 0; });
    }
    void updateHint()
    {
        if (restoring_) return;
        if (restorationChanged)
            restorationChanged();
    }
    bool restoring_ = false;
    QButtonGroup* models_;
    SwitchControl* toggle_;
    QWidget* fields_;
    std::array<QSpinBox*, 7> values_;
};
