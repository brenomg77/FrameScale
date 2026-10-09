#include <QGroupBox>
#include <QClipboard>
#include <QMimeData>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QDateTime>
#include "processing/PreviewFrameReader.h"
#include "ui/SelectionComboBox.h"
#include "ui/MediaPreviewWidget.h"
#include "ui/TrimTimeline.h"
#include "ui/AudioTrack.h"
#include <QImageReader>
#include <QKeyEvent>

#include "processing/RuntimePaths.h"

#include <QApplication>
#include <QAudioOutput>
#include "ui/ContinuousPreviewAudio.h"
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMediaPlayer>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QSet>
#include <QSettings>
#include <QLocale>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoFrame>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {
constexpr qint64 segmentLength = 4000;
constexpr qint64 directVideoPixels = 1920 * 1080;
// These are display copies only. The processed file and its reported dimensions
// stay unchanged, including for output larger than Qt's image allocation limit.
QString stillPreviewFilter()
{
    return QStringLiteral("scale=w='min(iw,4096)':h='min(ih,4096)':force_original_aspect_ratio=decrease:flags=lanczos,format=rgba");
}
QString playbackPreviewFilter()
{
    return QStringLiteral("scale=w='min(iw,1920)':h='min(ih,1080)':force_original_aspect_ratio=decrease:force_divisible_by=2,pad=ceil(iw/2)*2:ceil(ih/2)*2");
}

// Paint the entire outer dock, including its corner gutters. Transparent
// children alone expose the rectangular window material underneath them.
class PreviewDock final : public QFrame {
public:
    explicit PreviewDock(QWidget* parent) : QFrame(parent) {
        setAutoFillBackground(false);
        setAttribute(Qt::WA_StyledBackground, false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (window()->property("documentOpen").toBool())
            painter.fillRect(rect(), QColor("#181818"));
        if (Appearance::theme() != "studio" && !property("zoomOnly").toBool()) {
            if (auto* controls = findChild<QWidget*>("previewZoomBar")) {
                const QRectF surface(controls->mapTo(this, QPoint(0, 0)), controls->size());
                GlassMaterial::paint(painter, surface,
                    window()->property("glassBackdrop").value<QImage>(),
                    Appearance::light(), Appearance::light() ? 190 : 185, 12, true);
            }
        }

    }
};

QString playbackTime(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    if (seconds >= 3600) {
        return QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600)
            .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QIcon playbackIcon(bool playing)
{
    static QIcon cached[2];
    QIcon& icon = cached[playing ? 1 : 0];
    if (!icon.isNull()) {
        return icon;
    }
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#ededed")));
    if (playing) {
        painter.drawRoundedRect(QRectF(9, 7, 5, 18), 1, 1);
        painter.drawRoundedRect(QRectF(19, 7, 5, 18), 1, 1);
    } else {
        QPainterPath triangle;
        triangle.moveTo(11, 6);
        triangle.lineTo(25, 16);
        triangle.lineTo(11, 26);
        triangle.closeSubpath();
        painter.drawPath(triangle);
    }
    icon = QIcon(pixmap);
    return icon;
}

} // namespace

MediaPreviewWidget::MediaPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("mediaPreview"));
    qApp->installEventFilter(this);
    setMinimumSize(200, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet(QStringLiteral(
        "QWidget#mediaPreview { background: #181818; }"
        "QLabel#previewImage, QLabel#previewPlaceholder { background: #181818; "
        "color: #a8a8a8; border: none; }"
        "QWidget#previewToolbar { background: #232323; border-top: 1px solid "
        "#373737; }"
        "QWidget#previewTrimBar { background: #232323; border-top: 1px solid "
        "#373737; }"
        "QWidget#previewTrimBar QLabel { background: transparent; border: none; "
        "color: #c6c6c6; font-size: 11px; }"
        "QWidget#previewTrimBar QDoubleSpinBox { background: #171717; color: "
        "#e8e8e8; border: 1px solid #494949; border-radius: 3px; padding: 4px; "
        "font-size: 11px; }"
        "QWidget#previewTrimBar QPushButton { background: #333333; color: "
        "#dddddd; border: 1px solid #494949; border-radius: 4px; padding: 4px "
        "9px; min-width: 0; font-size: 11px; }"
        "QWidget#previewTrimBar QPushButton:hover { background: #444444; }"
        "QLabel#previewTime { color: #c6c6c6; font-size: 11px; border: none; "
        "background: transparent; }"
        "QLabel#previewNotice { color: #b7b7b7; background: #232323; padding: "
        "8px 12px; font-size: 11px; }"
        "QPushButton#previewPlayButton { background: transparent; border: none; "
        "border-radius: 4px; padding: 3px; min-width: 0; }"
        "QPushButton#previewPlayButton:hover { background: #3b3b3b; }"
        "QPushButton#previewPlayButton:disabled { background: transparent; }"
        "QSlider#previewSeekSlider::groove:horizontal { height: 3px; background: "
        "#4c4c4c; border-radius: 1px; }"
        "QSlider#previewSeekSlider::sub-page:horizontal { background: #1473e6; "
        "border-radius: 1px; }"
        "QSlider#previewSeekSlider::handle:horizontal { width: 10px; margin: "
        "-4px 0; background: #dedede; border-radius: 5px; }"));

    spaceShortcut_ = new QShortcut(QKeySequence(Qt::Key_Space), this);
    spaceShortcut_->setContext(Qt::WidgetWithChildrenShortcut);
    spaceShortcut_->setAutoRepeat(false);
    connect(spaceShortcut_, &QShortcut::activated, this, [this] {
        if (!spaceHeld_ && playButton_->isEnabled())
            playButton_->click();
    });
    auto* layout = new QVBoxLayout(this);
    // Timeline card inset (10) + dock margin (6) matches the 16-pixel panel inset.
    layout->setContentsMargins(0, 48, 0, 0);
    layout->setSpacing(0);
    zoomChoice_ = new SelectionComboBox(this);
    zoomChoice_->setObjectName("previewZoom");
    zoomChoice_->setAccessibleName(tr("Zoom da prévia"));
    zoomChoice_->addItem("100%", 0.0);
    for (double scale : { .25, .5, 1., 2., 4. })
        zoomChoice_->addItem(QString::number(scale * 100) + "%", scale);
    zoomChoice_->setFixedWidth(108);
    zoomChoice_->setIconSize(QSize(15,15));
    for(int i=0;i<zoomChoice_->count();++i) zoomChoice_->setItemIcon(i,ControlSymbols::icon(ControlSymbols::Symbol::Zoom));
    zoomChoice_->setEnabled(false);
    zoomChoice_->setToolTip(tr("Duplo clique na prévia ou Ctrl+0 para ajustar à janela."));
    zoomChoice_->setStyleSheet("QComboBox {padding:1px 8px;min-height:18px;background:#292929;color:#ededed;border:1px solid #505050;border-radius:6px;} QComboBox::down-arrow {image:url(:/brand/chevron-dark.svg);width:12px;height:8px;}");
    connect(zoomChoice_, &QComboBox::currentIndexChanged, this, [this] { setZoom(zoomChoice_->currentData().toDouble()); });
    auto* fit = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
    fit->setContext(Qt::WidgetWithChildrenShortcut);
    connect(fit, &QShortcut::activated, this, [this] { zoomChoice_->setCurrentIndex(0); setZoom(0); });

    stack_ = new QStackedWidget(this);
    stack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    placeholder_ = new QLabel(tr("Selecione uma imagem ou um vídeo"), stack_);
    placeholder_->setObjectName(QStringLiteral("previewPlaceholder"));
    placeholder_->setAlignment(Qt::AlignCenter);
    placeholder_->setWordWrap(true);
    stack_->addWidget(placeholder_);

    imageLabel_ = new QLabel(stack_);
    imageLabel_->setObjectName(QStringLiteral("previewImage"));
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setContentsMargins(0, 0, 0, 0);
    imageLabel_->setFocusPolicy(Qt::StrongFocus);
    imageLabel_->setCursor(Qt::ArrowCursor);
    imageLabel_->setToolTip(QString());
    imageLabel_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(imageLabel_, &QLabel::customContextMenuRequested, this, [this](const QPoint& point) {
        if (mediaPath_.isEmpty()) return;
        SelectionMenu menu(this);
        auto* copy = menu.addAction(ControlSymbols::icon(ControlSymbols::Symbol::Frames),tr("Copiar"));
        connect(copy, &QAction::triggered, this, [this] { emit copyRequested(); });
        auto* copyFrame = menu.addAction(ControlSymbols::icon(ControlSymbols::Symbol::Save),tr("Copiar quadro atual"));
        copyFrame->setEnabled(!currentFrame().isNull());
        connect(copyFrame, &QAction::triggered, this, [this] {
            const QImage frame = currentFrame();
            if (!frame.isNull())
                QApplication::clipboard()->setImage(frame);
        });
        if (originalDuration_ > 0)
            addOrientationMenu(&menu);
        auto* properties = menu.addAction(ControlSymbols::icon(ControlSymbols::Symbol::Info),tr("Propriedades"));
        connect(properties, &QAction::triggered, this, [this] {
            QDialog dialog(this);dialog.setWindowTitle(tr("Propriedades"));dialog.resize(580,570);
            auto* layout = new QVBoxLayout(&dialog);
            auto* videoGroup = new QGroupBox(tr("Vídeo"), &dialog);
            auto* videoLayout = new QVBoxLayout(videoGroup);
            auto* details = new QPlainTextEdit(videoGroup);details->setReadOnly(true);videoLayout->addWidget(details);
            layout->addWidget(videoGroup, 3);
            auto* audioGroup = new QGroupBox(tr("Áudio"), &dialog);
            auto* audioLayout = new QVBoxLayout(audioGroup);
            auto* audioDetails = new QPlainTextEdit(audioGroup);audioDetails->setReadOnly(true);audioLayout->addWidget(audioDetails);
            layout->addWidget(audioGroup, 2);
            audioDetails->setPlainText(tr("A ler os dados de áudio..."));
            const QFileInfo file(projectPath_.isEmpty() ? mediaPath_ : projectPath_);
            QString summary = tr("Nome: %1\nLocal: %2\nFormato: %3\nTamanho: %4 MB\nModificado em: %5\n")
                .arg(file.fileName(),QDir::toNativeSeparators(file.absoluteFilePath()),file.suffix().toUpper(),
                     QString::number(file.size()/1048576.,'f',2),file.lastModified().toString("dd/MM/yyyy HH:mm"));
            QSize propertySize=projectSize_.isValid() ? projectSize_ : sourceSize_;
            if(orientation_.quarterTurns%2) propertySize.transpose();
            if (propertySize.isValid()) summary += tr("Resolução: %1 x %2 px\n").arg(propertySize.width()).arg(propertySize.height());
            if (durationMs()>0) summary += tr("Duração: %1\nFPS: %2\n")
                .arg(timeline_->timecode(durationMs()/1000.),QString::number(fps_,'f',3));
            details->setPlainText(summary);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(buttons);
            connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
            Appearance::apply(&dialog);
            details->setProperty("appearanceManaged",true);
            details->setStyleSheet(QString("QPlainTextEdit {background:%1;color:%2;border:1px solid %3;border-radius:10px;padding:10px;}")
                .arg(Appearance::light()?"#fafafa":"#292929",Appearance::light()?"#292929":"#eeeeee",Appearance::light()?"#d0d0d4":"#505050"));
            audioDetails->setProperty("appearanceManaged",true);
            audioDetails->setStyleSheet(details->styleSheet());
            QProcess probe;
            QTimer timeout;timeout.setSingleShot(true);
            connect(&timeout,&QTimer::timeout,&probe,[&probe]{probe.kill();});
            connect(&probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),&dialog,[&,summary](int code,QProcess::ExitStatus state) {
                timeout.stop();
                if(code!=0 || state!=QProcess::NormalExit){audioDetails->setPlainText(tr("\nNão foi possível ler os dados adicionais do ficheiro."));return;}
                const auto data=QJsonDocument::fromJson(probe.readAllStandardOutput()).object();
                const auto format=data.value("format").toObject();
                auto rate=[](const QJsonValue& value){bool ok=false;double bits=value.toVariant().toString().toDouble(&ok);return ok && bits>0 ? QString::number(bits/1000.,'f',0)+" kbps" : QObject::tr("Não informado");};
                QString text=summary;
                QString audioText;
                text+=tr("Bitrate total: %1\n").arg(rate(format.value("bit_rate")));
                int audioCount=0;
                for(const auto& entry:data.value("streams").toArray()){
                    const auto stream=entry.toObject();
                    const auto type=stream.value("codec_type").toString();
                    if(type=="video")text+=tr("Codec de vídeo: %1\nBitrate do vídeo: %2\n").arg(stream.value("codec_name").toString(),rate(stream.value("bit_rate")));
                    if(type=="audio"){
                        ++audioCount;
                        const auto channels=stream.value("channels").toInt();
                        const auto sample=stream.value("sample_rate").toString().toDouble();
                        audioText+=tr("Faixa %1\nCodec: %2\nBitrate: %3\nCanais: %4 (%5)\nTaxa de amostragem: %6\n")
                            .arg(audioCount).arg(stream.value("codec_name").toString(),rate(stream.value("bit_rate")))
                            .arg(channels).arg(stream.value("channel_layout").toString(tr("Não informado")),sample>0?QString::number(sample/1000.,'f',1)+" kHz":tr("Não informado"));
                    }
                }
                if(!audioCount)audioText=tr("Sem faixa de áudio.");
                details->setPlainText(text);
                audioDetails->setPlainText(audioText.trimmed());
            });
            probe.start(framescale::RuntimePaths().tool("ffprobe"),{"-v","error","-show_entries","format=bit_rate:stream=codec_type,codec_name,bit_rate,channels,channel_layout,sample_rate","-of","json",file.absoluteFilePath()});
            timeout.start(15000);
            dialog.exec();
            if(probe.state()!=QProcess::NotRunning){probe.kill();probe.waitForFinished(1000);}
        });
        menu.exec(QCursor::pos());
    });
    imageLabel_->setMinimumSize(1, 1);
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel_->installEventFilter(this);
    stack_->addWidget(imageLabel_);

    videoSurface_ = new QWidget(stack_);
    videoSurface_->setObjectName("previewVideoSurface");
    videoSurface_->setProperty("appearanceManaged", true);
    videoSurface_->setAutoFillBackground(true);
    QPalette videoPalette = videoSurface_->palette();
    videoPalette.setColor(QPalette::Window, QColor("#181818"));
    videoSurface_->setPalette(videoPalette);
    videoSurface_->setStyleSheet("QWidget#previewVideoSurface { background: #181818; }");
    videoSurface_->installEventFilter(this);
    videoWidget_ = new QVideoWidget(videoSurface_);
    videoWidget_->setObjectName(QStringLiteral("previewVideo"));
    videoWidget_->installEventFilter(this);
    videoWidget_->setFocusPolicy(Qt::StrongFocus);
    videoWidget_->setCursor(Qt::ArrowCursor);
    videoWidget_->setToolTip(imageLabel_->toolTip());
    videoWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(videoWidget_, &QWidget::customContextMenuRequested, imageLabel_, &QWidget::customContextMenuRequested);
    videoWidget_->setAspectRatioMode(Qt::KeepAspectRatio);
    videoWidget_->setMinimumSize(1, 1);
    videoWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    stack_->addWidget(videoSurface_);
    layout->addWidget(stack_, 1);
    dimensionsLabel_ = new QLabel(this);
    dimensionsLabel_->setObjectName("previewDimensions");
    dimensionsLabel_->setProperty("appearanceManaged", true);
    dimensionsLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    dimensionsLabel_->setStyleSheet("QLabel {color:#eeeeee;background:transparent;border:0;padding:3px 18px 3px 8px;font-size:12px;}");
    dimensionsLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dimensionsLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(dimensionsLabel_, 0, Qt::AlignRight);
    dimensionsLabel_->hide();
    stack_->installEventFilter(this);
    connect(stack_, &QStackedWidget::currentChanged, this, [this] { updateMediaDimensions(); });


    // Zoom is a view action, so keep it in the lower trailing corner instead
    // of competing with the preview title.  The first item is updated to the
    // actual fitted percentage whenever the viewport changes.
    auto* dock = new PreviewDock(this);
    dock_ = dock;
    dock->setObjectName("previewDock");
    dock->setAutoFillBackground(false);
    dock->setProperty("appearanceManaged", true);
    auto* dockLayout = new QVBoxLayout(dock);
    dockLayout->setContentsMargins(8, 5, 8, 6);
    dockLayout->setSpacing(13);
    layout->addWidget(dock);
    auto* zoomBar = new QWidget(dock);
    zoomBar_ = zoomBar;
    zoomBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    zoomBar->setObjectName(QStringLiteral("previewZoomBar"));
    auto* zoomLayout = new QHBoxLayout(zoomBar);
    zoomLayout->setContentsMargins(10, 7, 10, 7);
    zoomLayout->setSpacing(6);

    notice_ = new QLabel(this);
    notice_->setObjectName(QStringLiteral("previewNotice"));
    notice_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    notice_->setMaximumHeight(28);
    notice_->clear();
    notice_->hide();
    zoomLayout->addWidget(notice_);
    comparisonButton_ = new QPushButton(tr("Preview"), zoomBar);
    comparisonButton_->setObjectName("comparePreviewButton");
    comparisonButton_->setMinimumWidth(0);
    comparisonButton_->setFixedHeight(24);
    comparisonButton_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    comparisonButton_->setStyleSheet("QPushButton#comparePreviewButton { color:white; background:#1473e6; border:1px solid #3f92f2; border-radius:5px; padding:0 10px; font-size:12px; font-weight:600; } QPushButton#comparePreviewButton:hover { background:#0962c7; } QPushButton#comparePreviewButton:disabled { color:#8b8b8b; background:#353535; border-color:#4a4a4a; }");
    comparisonButton_->setEnabled(false);
    comparisonButton_->setToolTip(tr("Gerar uma prévia processada e comparar com o original."));
    connect(comparisonButton_, &QPushButton::clicked, this, &MediaPreviewWidget::comparisonRequested);
    zoomLayout->addWidget(comparisonButton_);
    volume_ = std::clamp(QSettings().value("previewVolume", 35).toInt(), 0, 100);
    volumeButton_ = new QToolButton(zoomBar);
    volumeButton_->setObjectName("previewVolumeButton");
    volumeButton_->setEnabled(false);
    volumeButton_->setAccessibleName(tr("Volume da prévia"));
    volumeButton_->setToolTip(tr("Volume da prévia"));
    volumeButton_->setFixedSize(26, 24);
    volumeButton_->setIconSize(QSize(16,16));
    volumeButton_->setCursor(Qt::PointingHandCursor);
    auto* volumePopup = new QFrame(this, Qt::Popup);
    volumePopup->setObjectName("previewVolumePopup");
    // A click on the opener closes the popup; do not replay that same click
    // into the button, which would immediately open it again.
    volumePopup->setAttribute(Qt::WA_NoMouseReplay);
    volumePopup->setStyleSheet("QFrame#previewVolumePopup {background:#292929;border:1px solid #505050;border-radius:8px;} QSlider::groove:horizontal {height:4px;background:#555;border-radius:2px;} QSlider::sub-page:horizontal {background:#1473e6;border-radius:2px;} QSlider::handle:horizontal {background:white;width:14px;margin:-5px 0;border-radius:7px;}");
    auto* volumeLayout = new QVBoxLayout(volumePopup);
    volumeLayout->setContentsMargins(12, 10, 12, 12);
    volumeLayout->setSpacing(8);
    auto* volumeValue = new QLabel(volumePopup);
    volumeSlider_ = new QSlider(Qt::Horizontal, volumePopup);
    volumeSlider_->setObjectName("previewVolumeSlider");
    volumeSlider_->setAccessibleName(tr("Volume da prévia"));
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(volume_);
    volumeSlider_->setMinimumWidth(144);
    volumeSlider_->setFixedHeight(22);
    volumeValue->setText(tr("Volume") + "  " + QString::number(volume_) + "%");
    volumeLayout->addWidget(volumeValue);
    volumeLayout->addWidget(volumeSlider_);
    installRoundedWindow(volumePopup);
    connect(volumeSlider_, &QSlider::valueChanged, this, [this, volumeValue](int value) { setVolume(value); volumeValue->setText(tr("Volume")+"  "+QString::number(value)+"%"); });
    connect(volumeButton_, &QToolButton::clicked, this, [this, volumePopup] {
        if (volumePopup->isVisible()) {
            volumePopup->hide();
            return;
        }
        volumePopup->adjustSize();
        QPoint point = volumeButton_->mapToGlobal(QPoint(0, -volumePopup->height() - 6));
        const auto area = screen()->availableGeometry();
        point.setX(std::clamp(point.x(), area.left(), std::max(area.left(), area.right() - volumePopup->width())));
        point.setY(std::clamp(point.y(), area.top(), std::max(area.top(), area.bottom() - volumePopup->height())));
        volumePopup->move(point);
        volumePopup->show();
        volumeSlider_->setFocus();
    });
    setVolume(volume_);
    zoomLayout->addWidget(volumeButton_);
    zoomLayout->addWidget(zoomChoice_);
    zoomBar->setFixedHeight(38);
    // Match the timeline card's eight-pixel inset, not its widget bounds.
    auto* controlsRow = new QHBoxLayout;
    controlsRow->setContentsMargins(8, 0, 8, 0);
    controlsRow->setSpacing(0);
    controlsRow->addStretch();
    controlsRow->addWidget(zoomBar, 0, Qt::AlignRight);
    dockLayout->addLayout(controlsRow);

    timeline_ = new TrimTimeline(this);
    timeline_->hide();
    dockLayout->addWidget(timeline_);
    audioTrack_ = new AudioTrack(timeline_, this); audioTrack_->hide();
    timeline_->setEmbeddedAudio(audioTrack_);
    audioTrack_->editStarted = timeline_->editStarted = [this] { beginTimelineEdit(); };
    audioTrack_->editFinished = timeline_->editFinished = [this] { finishTimelineEdit(); };
    audioTrack_->selectedChanged = [this](bool additive) { selectTimelineTrack(true, additive); };
    timeline_->selectedChanged = [this](bool additive) { selectTimelineTrack(false, additive); };
    audioTrack_->menuRequested = timeline_->menuRequested = [this](const QPoint& p) { showTimelineMenu(p); };
    timeline_->videoMoved = [this](double seconds) {
        const int next=qRound(seconds*1000), delta=next-videoTimelineStartMs_;
        setVideoTimelineStart(next); setAudioOffset(audioOffsetMs_-delta);
    };
    auto* undo = new QShortcut(QKeySequence::Undo, this);
    undo->setObjectName("timelineUndo"); undo->setContext(Qt::WindowShortcut);
    connect(undo, &QShortcut::activated, this, &MediaPreviewWidget::undoTimeline);
    auto* redo = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), this);
    redo->setObjectName("timelineRedo"); redo->setContext(Qt::WindowShortcut);
    connect(redo, &QShortcut::activated, this, &MediaPreviewWidget::redoTimeline);
    audioTrack_->offsetEdited = [this](int ms) { setAudioOffset(ms); };
    audioTrack_->seekRequested = [this](double seconds) { pause(); seekTo(qRound64(seconds*1000)); };
    shiftedAudioTimer_ = new QTimer(this); shiftedAudioTimer_->setInterval(10);
    connect(shiftedAudioTimer_, &QTimer::timeout, this, [this] { syncShiftedAudio(false); });
    timeline_->rangeEdited = [this](double start, double end) {
        pause();
        setTrimRange(start, end);
        emit trimChanged(start, end);
    };
    seekTimer_ = new QTimer(this);
    seekTimer_->setSingleShot(true);
    seekTimer_->setInterval(40);
    connect(seekTimer_, &QTimer::timeout, this, [this] { seekTo(lastRequestedPosition_); });
    timeline_->seekRequested = [this](double seconds) {
        pause();
        pendingPosition_ = lastRequestedPosition_ = qRound64(seconds * 1000);
        requestScrubFrame(pendingPosition_);
        // Throttle, rather than restart a debounce timer on every mouse event.
        if (!seekTimer_->isActive())
            seekTimer_->start();
    };
    toolbar_ = new QWidget(this);
    toolbar_->setObjectName(QStringLiteral("previewToolbar"));
    auto* toolbarLayout = new QHBoxLayout(toolbar_);
    toolbarLayout->setContentsMargins(12, 6, 16, 6);
    toolbarLayout->setSpacing(12);
    playButton_ = new QPushButton(toolbar_);
    playButton_->setObjectName(QStringLiteral("previewPlayButton"));
    playButton_->setFixedSize(30, 28);
    playButton_->setIconSize(QSize(21, 21));
    playButton_->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(playButton_);
    seekSlider_ = new QSlider(Qt::Horizontal, toolbar_);
    seekSlider_->setObjectName(QStringLiteral("previewSeekSlider"));
    seekSlider_->setAccessibleName(tr("Posição do vídeo"));
    seekSlider_->setRange(0, 10000);
    toolbarLayout->addWidget(seekSlider_, 1);
    timeLabel_ = new QLabel(QStringLiteral("00:00 / 00:00"), toolbar_);
    timeLabel_->setObjectName(QStringLiteral("previewTime"));
    timeLabel_->setMinimumWidth(92);
    timeLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    toolbarLayout->addWidget(timeLabel_);
    toolbar_->hide();
    layout->addWidget(toolbar_);

    trimBar_ = new QWidget(this);
    trimBar_->setObjectName(QStringLiteral("previewTrimBar"));
    auto* trimLayout = new QHBoxLayout(trimBar_);
    trimLayout->setContentsMargins(12, 8, 12, 8);
    trimLayout->setSpacing(8);
    trimLayout->addWidget(new QLabel(tr("Cortar"), trimBar_));
    trimLayout->addSpacing(8);
    trimLayout->addWidget(new QLabel(tr("Início"), trimBar_));
    trimStart_ = new QDoubleSpinBox(trimBar_);
    trimStart_->setObjectName(QStringLiteral("trimStart"));
    trimStart_->setAccessibleName(tr("Início do corte, em segundos"));
    trimStart_->setRange(0.0, 8640000.0);
    trimStart_->setDecimals(3);
    trimStart_->setSingleStep(0.1);
    trimStart_->setSuffix(tr(" s"));
    trimStart_->setMinimumWidth(98);
    trimStart_->setKeyboardTracking(false);
    trimLayout->addWidget(trimStart_);
    auto* markStart = new QPushButton(tr("Marcar início"), trimBar_);
    markStart->setObjectName(QStringLiteral("trimMarkStart"));
    markStart->setToolTip(tr("Usar a posição atual como início do corte"));
    trimLayout->addWidget(markStart);
    trimLayout->addSpacing(10);
    trimLayout->addWidget(new QLabel(tr("Fim"), trimBar_));
    trimEnd_ = new QDoubleSpinBox(trimBar_);
    trimEnd_->setObjectName(QStringLiteral("trimEnd"));
    trimEnd_->setAccessibleName(tr("Fim do corte, em segundos"));
    trimEnd_->setRange(0.0, 8640000.0);
    trimEnd_->setDecimals(3);
    trimEnd_->setSingleStep(0.1);
    trimEnd_->setSuffix(tr(" s"));
    trimEnd_->setSpecialValueText(tr("Fim do vídeo"));
    trimEnd_->setMinimumWidth(110);
    trimEnd_->setKeyboardTracking(false);
    trimLayout->addWidget(trimEnd_);
    auto* markEnd = new QPushButton(tr("Marcar fim"), trimBar_);
    markEnd->setObjectName(QStringLiteral("trimMarkEnd"));
    markEnd->setToolTip(tr("Usar a posição atual como fim do corte"));
    trimLayout->addWidget(markEnd);
    trimLayout->addStretch();
    auto* resetTrim = new QPushButton(tr("Repor"), trimBar_);
    resetTrim->setObjectName(QStringLiteral("trimReset"));
    resetTrim->setToolTip(tr("Usar o vídeo completo"));
    trimLayout->addWidget(resetTrim);
    trimBar_->hide();
    timeline_->hide();
    layout->addWidget(trimBar_);
    connect(trimStart_, &QDoubleSpinBox::valueChanged, this,
        [this] { applyTrimEdit(true); });
    connect(trimEnd_, &QDoubleSpinBox::valueChanged, this,
        [this] { applyTrimEdit(false); });
    connect(markStart, &QPushButton::clicked, this, [this] {
        if (player_) {
            trimStart_->setValue(positionMs() / 1000.0);
        }
    });
    connect(markEnd, &QPushButton::clicked, this, [this] {
        if (player_ && positionMs() > 0) {
            trimEnd_->setValue(positionMs() / 1000.0);
        }
    });
    connect(resetTrim, &QPushButton::clicked, this, [this] {
        setTrimRange(0.0, 0.0);
        emit trimChanged(trimStartSeconds_, trimEndSeconds_);
    });

    connect(playButton_, &QPushButton::clicked, this, [this] {
        if (!player_ || fallingBack_ || !mediaLoaded_) {
            return;
        }
        if (player_->playbackState() == QMediaPlayer::PlayingState) {
            pause();
        } else {
            const qint64 start = qRound64(trimStartSeconds_ * 1000.0);
            const qint64 end = trimEndSeconds_ > 0.0
                ? qRound64(trimEndSeconds_ * 1000.0)
                : durationMs();
            if (player_->mediaStatus() == QMediaPlayer::EndOfMedia || positionMs() < start || (end > 0 && positionMs() >= end)) {
                seekTo(start);
            }
            if (!player_) {
                pendingResume_ = true;
                return;
            }
            requestedScrub_ = -1;
            playbackRequested_ = true;
            audioOutput_->setMuted(false);
            player_->play();
        }
    });
    connect(seekSlider_, &QSlider::sliderPressed, this, [this] {
        if (!player_) {
            return;
        }
        scrubbing_ = true;
        resumeAfterSeek_ = player_->playbackState() == QMediaPlayer::PlayingState;
        playbackRequested_ = false;
        audioOutput_->setMuted(true);
        player_->pause();
    });
    connect(seekSlider_, &QSlider::sliderMoved, this, [this](int value) {
        if (player_ && player_->isSeekable()) {
            seekTo(durationMs() * value / 10000);
            updatePlaybackControls();
        }
    });
    connect(seekSlider_, &QSlider::sliderReleased, this, [this] {
        scrubbing_ = false;
        if (player_ && resumeAfterSeek_) {
            requestedScrub_ = -1;
            playbackRequested_ = true;
            audioOutput_->setMuted(false);
            player_->play();
        }
        resumeAfterSeek_ = false;
        updatePlaybackControls();
    });
    connect(seekSlider_, &QSlider::valueChanged, this, [this](int value) {
        // Keyboard and page-step seeking do not send sliderMoved.
        if (!scrubbing_ && player_ && player_->isSeekable()) {
            seekTo(durationMs() * value / 10000);
        }
    });
    updatePlaybackControls();
}

MediaPreviewWidget::~MediaPreviewWidget()
{
    qApp->removeEventFilter(this);
    ++generation_;
    ++seekSerial_;
    playbackSpeed_=1.; timeline_->setPlaybackSpeed(1.);
    audioOffsetMs_ = 0; videoTimelineStartMs_=0; undoEdits_.clear(); redoEdits_.clear();
    timeline_->setPlacement(0,0); timeline_->setTrackMode(false); timeline_->setSelected(false);
    audioTrack_->clear(); audioTrack_->setSelected(false);
    if (QWidget::mouseGrabber() == imageLabel_)
        imageLabel_->releaseMouse();
    spaceHeld_ = panning_ = false;
    stopPlayer();
    stopDecoder();
    stopProxy();
    stopFrameReaders();
}

void MediaPreviewWidget::stopPlayer()
{
    freezeVideoFrame();
    if (shiftedAudioTimer_) shiftedAudioTimer_->stop();
    delete shiftedAudioPlayer_; shiftedAudioPlayer_ = nullptr;
    delete continuousAudio_;
    continuousAudio_ = nullptr;
    playbackRequested_ = false;
    mediaLoaded_ = false;
    if (audioOutput_) {
        audioOutput_->setMuted(true);
    }
    if (player_) {
        disconnect(player_, nullptr, this, nullptr);
        if (videoSink_) {
            disconnect(videoSink_, nullptr, this, nullptr);
        }
        player_->stop();
        player_->setVideoSink(nullptr);
        player_->setAudioOutput(nullptr);
        player_->setObjectName(QString());
        player_->deleteLater();
        player_ = nullptr;
        videoSink_ = nullptr;
    }
    videoWidget_->videoSink()->setVideoFrame(QVideoFrame());
    if (audioOutput_) {
        audioOutput_->deleteLater();
        audioOutput_ = nullptr;
    }
}

void MediaPreviewWidget::stopDecoder()
{
    if (!decoder_) {
        return;
    }
    QProcess* process = decoder_;
    decoder_ = nullptr;
    disconnect(process, nullptr, this, nullptr);
    // The finished connection owns cleanup, avoiding a blocking QProcess
    // destructor.
    process->setParent(nullptr);
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater();
    } else {
        process->kill();
    }
}

void MediaPreviewWidget::clear()
{
    layerPreviewPending_=false;
    if(layerBlank_) layerBlank_->hide();
    setProperty("hasAudio", false);
    playbackSpeed_=1.; timeline_->setPlaybackSpeed(1.);
    audioOffsetMs_ = 0; videoTimelineStartMs_=0; undoEdits_.clear(); redoEdits_.clear();
    timeline_->setPlacement(0,0); timeline_->setTrackMode(false); timeline_->setSelected(false);
    audioTrack_->clear(); audioTrack_->setSelected(false);
    ++generation_;
    ++seekSerial_;
    if (QWidget::mouseGrabber() == imageLabel_)
        imageLabel_->releaseMouse();
    spaceHeld_ = panning_ = false;
    stopPlayer();
    stopDecoder();
    stopProxy();
    stopFrameReaders();
    seekTimer_->stop();
    requestedScrub_ = completedScrub_ = -1;
    completedScrubImage_ = {};
    layout()->setAlignment(dock_, {});
    dock_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fps_ = 25;
    timeline_->setFrameRate(fps_);
    filmstrip_.clear();
    timeline_->resetView();
    timeline_->setThumbnails({ });
    comparisonButton_->setEnabled(false);
    mediaPath_.clear();
    zoomChoice_->setEnabled(false);
    volumeButton_->setEnabled(false);
    segmented_ = false;
    originalDuration_ = segmentOffset_ = pendingPosition_ = 0;
    pendingResume_ = seekAfterLoad_ = false;
    activeProxy_.reset();
    zoomChoice_->setCurrentIndex(0);
    pan_ = { };
    image_ = { };
    sourceSize_ = { };
    projectSize_ = {}; projectPath_.clear();
    orientation_ = {};
    updateMediaDimensions();
    zoomChoice_->setItemText(0, "100%");
    imageLabel_->clear();
    firstVideoFrame_ = false;
    fallingBack_ = false;
    scrubbing_ = false;
    resumeAfterSeek_ = false;
    playbackRequested_ = false;
    mediaLoaded_ = false;
    trimStartSeconds_ = 0.0;
    trimEndSeconds_ = 0.0;
    updateTrimControls();
    placeholder_->setText(tr("Selecione uma imagem ou um vídeo"));
    stack_->setCurrentWidget(emptyPage_ ? emptyPage_ : placeholder_);
    notice_->clear();
    notice_->hide();
    toolbar_->hide();
    trimBar_->hide();
    timeline_->hide();
    updatePlaybackControls();
}

void MediaPreviewWidget::setEmptyPromptVisible(bool visible)
{
    if (emptyPage_)
        for (auto* child : emptyPage_->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            child->setVisible(visible);
}

void MediaPreviewWidget::enableFileSelection()
{
    if (emptyPage_) return;
    emptyPage_ = new QWidget(stack_);
    auto* layout = new QVBoxLayout(emptyPage_);
    layout->setSpacing(14);
    layout->addStretch();
    auto* hint = new QLabel(placeholder_->text(), emptyPage_);
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* choose = new QPushButton(tr("Selecionar ficheiro"), emptyPage_);
    choose->setObjectName("emptyChooseMediaButton");
    choose->setProperty("variant", "primary");
    choose->setCursor(Qt::PointingHandCursor);
    layout->addWidget(choose, 0, Qt::AlignHCenter);
    layout->addStretch();
    stack_->addWidget(emptyPage_);
    connect(choose, &QPushButton::clicked, this, &MediaPreviewWidget::fileSelectionRequested);
    if (mediaPath_.isEmpty()) stack_->setCurrentWidget(emptyPage_);
}

void MediaPreviewWidget::setLayerVideoVisible(bool visible)
{
    if(!layerBlank_) {
        layerBlank_=new QLabel(stack_);
        layerBlank_->setAttribute(Qt::WA_TransparentForMouseEvents);
        layerBlank_->setProperty("appearanceManaged",true);
        layerBlank_->setStyleSheet("background:#181818;border:0;");
    }
    layerBlank_->setGeometry(stack_->rect());
    layerBlank_->setVisible(layerEditing_ && !visible && !layerPreviewPending_);
    layerBlank_->raise();
}

void MediaPreviewWidget::failLayerPreview(const QString& message)
{
    layerPreviewPending_=false;
    placeholder_->setText(message);
    stack_->setCurrentWidget(placeholder_);
    emit previewError(message);
}

void MediaPreviewWidget::invalidateLayerPreview(bool empty)
{
    const auto position=positionMs();
    pause();
    ++generation_; ++seekSerial_;
    stopPlayer(); stopDecoder(); stopProxy(); stopFrameReaders(); seekTimer_->stop();
    pendingPosition_=position;
    layerPreviewPending_=true;
    if(layerBlank_) layerBlank_->hide();
    image_={}; imageLabel_->clear();
    placeholder_->setProperty("appearanceManaged",true);
    placeholder_->setStyleSheet("background:#181818;color:#dddddd;border:0;");
    placeholder_->setText(empty ? tr("Adicione uma camada ao projeto") : tr("A carregar..."));
    stack_->setCurrentWidget(placeholder_);
    comparisonButton_->setEnabled(false);
}

void MediaPreviewWidget::showLoading()
{
    placeholder_->setText(tr("Carregando…"));
    stack_->setCurrentWidget(placeholder_);
}

void MediaPreviewWidget::pause()
{
    pendingPosition_ = positionMs();
    freezeVideoFrame();
    if (prefetch_ && !prefetch_->property("activateWhenReady").toBool()) {
        auto* process = prefetch_;
        prefetch_ = nullptr;
        disconnect(process, nullptr, this, nullptr);
        process->setParent(nullptr);
        if (process->state() == QProcess::NotRunning) process->deleteLater();
        else process->kill();
    }
    if (shiftedAudioPlayer_) shiftedAudioPlayer_->pause();
    if (shiftedAudioTimer_) shiftedAudioTimer_->stop();
    pendingResume_ = false;
    resumeAfterSeek_ = false;
    playbackRequested_ = false;
    if (audioOutput_) {
        audioOutput_->setMuted(true);
    }
    if (player_) {
        player_->pause();
    }
}

void MediaPreviewWidget::setTrimRange(double startSeconds, double endSeconds)
{
    trimStartSeconds_ = std::isfinite(startSeconds) ? std::max(0.0, startSeconds) : 0.0;
    trimEndSeconds_ = std::isfinite(endSeconds) ? std::max(0.0, endSeconds) : 0.0;
    if (trimEndSeconds_ > 0.0 && trimStartSeconds_ >= trimEndSeconds_) {
        trimStartSeconds_ = std::max(0.0, trimEndSeconds_ - 0.001);
    }
    updateTrimControls();
}

double MediaPreviewWidget::trimStartSeconds() const
{
    return trimStartSeconds_;
}

double MediaPreviewWidget::trimEndSeconds() const { return trimEndSeconds_; }

QImage MediaPreviewWidget::orientedFrame(const QImage& frame) const
{
    if (orientation_.isIdentity() || frame.isNull()) return frame;
    QTransform rotation;
    rotation.rotate(90 * orientation_.quarterTurns);
    QTransform mirror;
    mirror.scale(orientation_.horizontalFlip ? -1 : 1, orientation_.verticalFlip ? -1 : 1);
    return frame.transformed(rotation * mirror);
}

void MediaPreviewWidget::setOrientation(const framescale::VideoOrientation& orientation)
{
    freezeVideoFrame();
    orientation_ = orientation;
    pan_ = {};
    updateImageSize();
    updateMediaDimensions();
}

void MediaPreviewWidget::showOrientationMenu(const QPoint& globalPosition)
{
    if (mediaPath_.isEmpty()) return;
    std::unique_ptr<QMenu> menu(createOrientationMenu(nullptr));
    menu->exec(globalPosition);
}

void MediaPreviewWidget::addOrientationMenu(QMenu* parent)
{
    auto* popup=createOrientationMenu(parent);
    popup->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::RotateLeft));
    auto* trigger=parent->addMenu(popup);
    auto* timer=new QTimer(parent);
    timer->setInterval(100);
    connect(timer,&QTimer::timeout,parent,[parent,popup,trigger] {
        if(!popup->isVisible()) return;
        const auto cursor=QCursor::pos();
        const bool overPopup=popup->rect().contains(popup->mapFromGlobal(cursor));
        const bool overTrigger=parent->actionGeometry(trigger).adjusted(-4,-4,4,4).contains(parent->mapFromGlobal(cursor));
        if(!parent->isVisible() || (!overPopup && !overTrigger)) popup->hide();
    });
    timer->start();
}

QMenu* MediaPreviewWidget::createOrientationMenu(QMenu* parent)
{
    auto* submenu = new SelectionMenu(parent ? static_cast<QWidget*>(parent) : this);
    auto& menu = *submenu;
    menu.setTitle(tr("Rotacionar"));
    menu.setProperty("orientationMenu", true);
    const QStringList labels {tr("Girar 90° para a esquerda"), tr("Girar 90° para a direita"),
        tr("Espelhar horizontalmente"), tr("Espelhar verticalmente")};
    const ControlSymbols::Symbol symbols[] {ControlSymbols::Symbol::RotateLeft,
        ControlSymbols::Symbol::RotateRight, ControlSymbols::Symbol::FlipHorizontal,
        ControlSymbols::Symbol::FlipVertical};
    auto* row = new QWidget(&menu);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);
    for (int i = 0; i < 4; ++i) {
        auto* action = new QAction(ControlSymbols::icon(symbols[i]), labels[i], row);
        action->setObjectName(QStringLiteral("orientationAction%1").arg(i));
        auto* button = new QToolButton(row);
        button->setDefaultAction(action);
        button->setAccessibleName(labels[i]);
        button->setToolTip(labels[i]);
        button->setAutoRaise(true);
        button->setFixedSize(32, 30);
        button->setIconSize(QSize(20, 20));
        layout->addWidget(button);
        connect(action, &QAction::triggered, &menu, &QMenu::close);
        if (parent)
            connect(action, &QAction::triggered, parent, &QMenu::close);
        connect(action, &QAction::triggered, this, [this, i] {
            auto next = orientation_;
            if (i < 2) next.rotate(i == 0 ? -1 : 1);
            else if (i == 2) next.horizontalFlip = !next.horizontalFlip;
            else next.verticalFlip = !next.verticalFlip;
            setOrientation(next);
        });
    }
    auto* widgetAction = new QWidgetAction(&menu);
    widgetAction->setDefaultWidget(row);
    menu.addAction(widgetAction);
    return submenu;
}

QImage MediaPreviewWidget::currentFrame() const
{
    if (stack_ && stack_->currentWidget() == videoSurface_ && videoWidget_ && videoWidget_->videoSink())
        return orientedFrame(videoWidget_->videoSink()->videoFrame().toImage());
    return orientedFrame(image_);
}

void MediaPreviewWidget::updateTrimControls()
{
    // A zero end means the complete video. Keep that sentinel while metadata is
    // loading so activating a project cannot replace a restored trim range.
    const double duration = player_ && durationMs() > 0
        ? durationMs() / 1000.0
        : 8640000.0;
    const QSignalBlocker startBlocked(trimStart_);
    const QSignalBlocker endBlocked(trimEnd_);
    trimStart_->setMaximum(std::max(0.0, duration - 0.001));
    trimEnd_->setMaximum(duration);
    trimStartSeconds_ = std::clamp(trimStartSeconds_, 0.0, trimStart_->maximum());
    trimEndSeconds_ = std::clamp(trimEndSeconds_, 0.0, duration);
    if (trimEndSeconds_ > 0.0 && trimStartSeconds_ >= trimEndSeconds_) {
        trimStartSeconds_ = std::max(0.0, trimEndSeconds_ - 0.001);
    }
    trimStart_->setValue(trimStartSeconds_);
    trimEnd_->setValue(trimEndSeconds_);
    if (timeline_)
        timeline_->setRange(
            player_ && durationMs() > 0 ? durationMs() / 1000. : 1.,
            trimStartSeconds_, trimEndSeconds_,
            player_ ? positionMs() / 1000. : 0);
}

void MediaPreviewWidget::applyTrimEdit(bool startChanged)
{
    trimStartSeconds_ = trimStart_->value();
    trimEndSeconds_ = trimEnd_->value();
    if (trimEndSeconds_ > 0.0 && trimStartSeconds_ >= trimEndSeconds_) {
        if (startChanged) {
            trimEndSeconds_ = std::min(trimEnd_->maximum(), trimStartSeconds_ + 0.001);
        } else {
            trimStartSeconds_ = std::max(0.0, trimEndSeconds_ - 0.001);
        }
    }
    updateTrimControls();
    emit trimChanged(trimStartSeconds_, trimEndSeconds_);
}

void MediaPreviewWidget::setMedia(const QString& path, qint64 initialPosition)
{
    const QFileInfo info(path);
    if (!path.isEmpty() && !mediaPath_.isEmpty() && info.absoluteFilePath() == mediaPath_ && info.isFile() && !layerPreviewPending_) {
        pause();
        return;
    }
    clear();
    if (path.isEmpty()) {
        return;
    }
    mediaPath_ = info.absoluteFilePath();
    pendingPosition_ = std::max(qint64(0), initialPosition);
    seekAfterLoad_ = pendingPosition_ > 0;
    if (!info.isFile()) {
        showError(tr("Não foi possível encontrar o ficheiro selecionado."));
        return;
    }
    zoomChoice_->setEnabled(true);
    volumeButton_->setEnabled(true);
    showLoading();
    static const QSet<QString> imageExtensions {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("tif"),
        QStringLiteral("tiff")
    };
    if (imageExtensions.contains(info.suffix().toLower())) {
        layout()->setAlignment(dock_, Qt::AlignRight);
        dock_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        QImageReader reader(mediaPath_);
        sourceSize_ = reader.size();
        updateMediaDimensions();
        decodeThumbnail(mediaPath_, false);
        return;
    }

    prepareVideoPreview();
}
void MediaPreviewWidget::loadVideo(const QString& playbackPath)
{
    const quint64 request = generation_;
    firstVideoFrame_ = false;
    mediaLoaded_ = false;
    player_ = new QMediaPlayer(this);
    player_->setPlaybackRate(playbackSpeed_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    player_->setPitchCompensation(false);
#endif
    player_->setObjectName(QStringLiteral("previewPlayer"));
    audioOutput_ = new QAudioOutput(this);
    audioOutput_->setVolume(volume_ / 100.f);
    // Playback is allowed only after an explicit action, including after tab
    // changes.
    audioOutput_->setMuted(true);
    if (sparseAudioTimestamps_ && !segmented_) {
        continuousAudio_ = new ContinuousPreviewAudio(this);
        continuousAudio_->setSource(playbackPath);
        continuousAudio_->setPlaybackSpeed(playbackSpeed_);
        continuousAudio_->setVolume(0);
        continuousAudio_->failed = [this](const QString& error) { emit previewError(error); };
        const auto updateAudioVolume = [this] {
            if (continuousAudio_) continuousAudio_->setVolume(audioOutput_->isMuted() ? 0.f : audioOutput_->volume());
        };
        connect(audioOutput_, &QAudioOutput::volumeChanged, this, updateAudioVolume);
        connect(audioOutput_, &QAudioOutput::mutedChanged, this, updateAudioVolume);
        connect(player_, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
            if (!continuousAudio_) return;
            if (state == QMediaPlayer::PlayingState && playbackRequested_) continuousAudio_->play();
            else continuousAudio_->pause();
        });
    } else player_->setAudioOutput(audioOutput_);
    videoSink_ = new QVideoSink(player_);
    player_->setVideoSink(videoSink_);
    toolbar_->hide();
    trimBar_->hide();
    timeline_->setVisible(!compact_ && !layerEditing_);
    connect(player_, &QMediaPlayer::positionChanged, this,
        [this, request](qint64 position) {
            if (request == generation_) {
                if (playbackRequested_ && trimEndSeconds_ > 0.0 && position + segmentOffset_ >= qRound64(trimEndSeconds_ * 1000.0)) {
                    pause();
                    seekTo(qRound64(trimStartSeconds_ * 1000.0));
                }
                updatePlaybackControls();
                if (segmented_ && playbackRequested_ && position >= segmentLength / 3
                    && segmentOffset_ + segmentLength < durationMs())
                    prepareSegment(segmentOffset_ + segmentLength, true);
            }
        });
    connect(player_, &QMediaPlayer::durationChanged, this,
        [this, request](qint64) {
            if (request == generation_) {
                updateTrimControls();
                updatePlaybackControls();
            }
        });
    connect(player_, &QMediaPlayer::mediaStatusChanged, this,
        [this, request](QMediaPlayer::MediaStatus status) {
            if (request != generation_ || !player_) {
                return;
            }
            if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::BufferingMedia || status == QMediaPlayer::EndOfMedia) {
                mediaLoaded_ = true;
            }
            if (status == QMediaPlayer::EndOfMedia) {
                const qint64 end = trimEndSeconds_ > 0 ? qRound64(trimEndSeconds_ * 1000) : durationMs();
                if (segmented_ && playbackRequested_ && segmentOffset_ + segmentLength < end) {
                    seekTo(segmentOffset_ + segmentLength);
                    return;
                }
                pause();
                seekTo(qRound64(trimStartSeconds_ * 1000.0));
            } else if (mediaLoaded_ && seekAfterLoad_) {
                seekAfterLoad_ = false;
                if (continuousAudio_) continuousAudio_->seek(pendingPosition_ - audioOffsetMs_);
                player_->setPosition(std::max(qint64(0), pendingPosition_ - segmentOffset_));
                if (pendingResume_) {
                    pendingResume_ = false;
                    playbackRequested_ = true;
                    audioOutput_->setMuted(false);
                    player_->play();
                }
            }
            updatePlaybackControls();
        });
    connect(player_, &QMediaPlayer::seekableChanged, this, [this, request](bool) {
        if (request == generation_) {
            updatePlaybackControls();
        }
    });
    connect(player_, &QMediaPlayer::playbackStateChanged, this,
        [this, request](QMediaPlayer::PlaybackState) {
            if (request == generation_) {
                if (player_ && player_->playbackState() == QMediaPlayer::PlayingState && !playbackRequested_) {
                    player_->pause();
                    return;
                }
                if (player_ && firstVideoFrame_) {
                    if (player_->playbackState() == QMediaPlayer::PlayingState) {
                        stack_->setCurrentWidget(imageLabel_);
                    } else {
                        const QImage still = videoSink_->videoFrame().toImage();
                        if (!still.isNull()) {
                            image_ = still;
                            stack_->setCurrentWidget(imageLabel_);
                            updateImageSize();
                        }
                    }
                }
                updatePlaybackControls();
            }
        });
    connect(player_, &QMediaPlayer::errorOccurred, this,
        [this, request](QMediaPlayer::Error, const QString& reason) {
            if (request == generation_) {
                videoFallback(reason);
            }
        });
    connect(videoSink_, &QVideoSink::videoFrameChanged, this,
        [this, request](const QVideoFrame& frame) {
            if (request != generation_ || fallingBack_ || !player_ || !frame.isValid()) {
                return;
            }
            if (!playbackRequested_ && requestedScrub_ >= 0)
                return;
            const bool firstFrame = !firstVideoFrame_;
            firstVideoFrame_ = true;
            if (decoder_) {
                stopDecoder();
            }
            if (orientation_.isIdentity() && !segmented_ && zoom_ == 0 && pan_.isNull() && !panning_ && playbackRequested_ && player_->playbackState() == QMediaPlayer::PlayingState) {
                // The default fit view retains Qt's accelerated video path.
                videoWidget_->videoSink()->setVideoFrame(frame);
                updateVideoGeometry();
                stack_->setCurrentWidget(videoSurface_);
            } else {
                const QImage still = frame.toImage();
                if (!still.isNull()) {
                    image_ = still;
                    stack_->setCurrentWidget(imageLabel_);
                    updateImageSize();
                }
            }
            if (firstFrame) {
                updatePlaybackControls();
                emit mediaReady();
            }
        });
    player_->setSource(QUrl::fromLocalFile(playbackPath));
    configureShiftedAudio();
    // Decode a thumbnail independently: briefly playing to obtain a first frame
    // can leave a newly activated project playing after asynchronous callbacks.
    if (player_ && !fallingBack_ && !firstVideoFrame_ && !seekAfterLoad_) {
        decodeThumbnail(playbackPath, true, { }, false);
    }
    updatePlaybackControls();
    QTimer::singleShot(120000, this, [this, request] {
        if (request == generation_ && !mediaLoaded_ && !fallingBack_ && player_) {
            videoFallback(tr("O leitor de vídeo não conseguiu carregar a prévia."));
        }
    });
}

qint64 MediaPreviewWidget::durationMs() const
{
    return originalDuration_ > 0 ? originalDuration_ : player_ ? player_->duration()
                                                               : 0;
}
qint64 MediaPreviewWidget::positionMs() const
{
    return player_ && !seekAfterLoad_ && !(requestedScrub_ >= 0 && !playbackRequested_) ? segmentOffset_ + player_->position() : pendingPosition_;
}
void MediaPreviewWidget::stopProxy()
{
    for (QProcess** reader : { &proxy_, &prefetch_ }) {
        auto* process = *reader;
        *reader = nullptr;
        if (!process) continue;
        disconnect(process, nullptr, this, nullptr);
        process->setParent(nullptr);
        if (process->state() == QProcess::NotRunning) process->deleteLater();
        else process->kill();
    }
}
void MediaPreviewWidget::seekTo(qint64 position)
{
    if(layerPreviewPending_) { pendingPosition_=std::max<qint64>(0,position); return; }
    position = std::clamp(position, qint64(0), std::max(qint64(0), durationMs() - 1));
    pendingPosition_ = position;
    if (shiftedAudioPlayer_) { shiftedAudioPlayer_->pause(); shiftedAudioPlayer_->setPosition(std::max<qint64>(0, position - audioOffsetMs_)); }
    if (continuousAudio_) continuousAudio_->seek(position - audioOffsetMs_);
    if (!playbackRequested_)
        requestScrubFrame(position);
    if (proxy_ && proxy_->property("segmentOffset").toLongLong() == position / segmentLength * segmentLength)
        return;
    if (!segmented_ || (player_ && position >= segmentOffset_ && position < segmentOffset_ + segmentLength)) {
        if (player_)
            player_->setPosition(position - segmentOffset_);
        return;
    }
    pendingResume_ = pendingResume_ || playbackRequested_;
    pendingPosition_ = position;
    stopPlayer();
    if (prefetch_ && prefetch_->property("segmentOffset").toLongLong() == position / segmentLength * segmentLength) {
        prefetch_->setProperty("activateWhenReady", true);
        notice_->setText(tr("A preparar este trecho da prévia…"));
        notice_->show();
        return;
    }
    stopProxy();
    const auto serial = ++seekSerial_;
    const auto request = generation_;
    updatePlaybackControls();
    // Debounce scrubbing: only the latest requested segment consumes a decoder.
    QTimer::singleShot(100, this, [this, serial, request] {
        if (request == generation_ && serial == seekSerial_)
            prepareSegment(pendingPosition_);
    });
}
void MediaPreviewWidget::prepareVideoPreview(bool forceProxy)
{
    stopDecoder();
    const QString source = mediaPath_;
    const auto request = generation_;
    auto* probe = new QProcess(this);
    decoder_ = probe;
    connect(probe, &QProcess::finished, this, [this, probe, request, source, forceProxy](int code) {
        if (request != generation_ || decoder_ != probe)
            return;
        decoder_ = nullptr;
        const auto document = QJsonDocument::fromJson(probe->readAllStandardOutput()).object();
        const auto streams = document.value("streams").toArray();
        QJsonObject stream;
        sparseAudioTimestamps_ = false;
        bool firstAudio = true;
        for (const auto& entry : streams) {
            const auto info = entry.toObject();
            if (info.value("codec_type").toString() == "video" && stream.isEmpty()) stream = info;
            if (info.value("codec_type").toString() == "audio" && firstAudio) {
                firstAudio = false;
                const double samples = info.value("nb_frames").toString().toDouble() * 2048.;
                const double rate = info.value("sample_rate").toString().toDouble();
                const double duration = info.value("duration").toString().toDouble();
                sparseAudioTimestamps_ = info.value("profile").toString() == "HE-AACv2"
                    && rate > 0 && duration > 0 && samples > 0 && samples / rate < duration * .8;
            }
        }
        setProperty("continuousPreviewAudio", sparseAudioTimestamps_);
        setProperty("hasAudio", !firstAudio);
        originalDuration_ = qRound64(document.value("format").toObject().value("duration").toString().toDouble() * 1000);
        sourceSize_ = QSize(stream.value("width").toInt(), stream.value("height").toInt());
        updateMediaDimensions();
        const auto parseRate = [](const QString& text) {
            const auto parts = text.split('/');
            const double value = parts.size() == 2 && parts[1].toDouble() > 0
                ? parts[0].toDouble() / parts[1].toDouble() : text.toDouble();
            return std::isfinite(value) && value > 0 ? value : 0.;
        };
        const double averageRate = parseRate(stream.value("avg_frame_rate").toString());
        const double nominalRate = parseRate(stream.value("r_frame_rate").toString());
        fps_ = averageRate > 0 ? averageRate : nominalRate > 0 ? nominalRate : 25.;
        if(std::abs(fps_-std::round(fps_))<.01) fps_=std::round(fps_);
        updateMediaDimensions();
        setProperty("sourceFrameRate", fps_);
        timeline_->setFrameRate(fps_);
        if (!firstAudio && !layerEditing_) audioTrack_->load(source, originalDuration_ / 1000., !compact_, sparseAudioTimestamps_);
        timeline_->setTrackMode(!firstAudio && !compact_);
        timeline_->setTracksLinked(audioTrack_->linked());
        emit videoMetadataReady();
        if (!compact_ || externalFilmstrip_)
            startFilmstrip();
        comparisonButton_->setEnabled(comparisonAllowed_);
        timeline_->setClipName(QFileInfo(source).fileName());
        if (code == 0 && !forceProxy && stream.value("codec_name").toString() != "av1" && sourceSize_.isValid()
            && qint64(sourceSize_.width()) * sourceSize_.height() <= directVideoPixels) {
            loadVideo(source);
            if(!playbackRequested_) requestScrubFrame(pendingPosition_);
            return;
        }
        if (originalDuration_ <= 0) {
            showError(tr("Não foi possível ler a duração do vídeo."));
            return;
        }
        segmented_ = true;
        fallingBack_ = false;
        updateMediaDimensions();
        timeline_->setVisible(!compact_ && !layerEditing_);
        updatePlaybackControls();
        decodeThumbnail(source, true, { }, false);
        prepareSegment(pendingPosition_);
    });
    connect(probe, &QProcess::finished, probe, &QObject::deleteLater);
    connect(probe, &QProcess::errorOccurred, this, [this, probe, request](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && request == generation_ && decoder_ == probe) {
            decoder_ = nullptr;
            showError(tr("Não foi possível iniciar o FFprobe."));
            probe->deleteLater();
        }
    });
    QTimer::singleShot(15000, probe, [probe] { if (probe->state() != QProcess::NotRunning) probe->kill(); });
    probe->start(framescale::RuntimePaths().tool("ffprobe"), { "-v", "error", "-show_entries", "stream=codec_type,codec_name,profile,sample_rate,nb_frames,duration,width,height,avg_frame_rate,r_frame_rate:format=duration", "-of", "json", source });
}
void MediaPreviewWidget::prepareSegment(qint64 position, bool prefetch)
{
    const qint64 offset = position / segmentLength * segmentLength;
    const QString source = mediaPath_;
    const QString key = source + ":" + QString::number(QFileInfo(source).lastModified().toMSecsSinceEpoch()) + ":" + QString::number(offset);
    if (prefetch && (proxy_ || prefetch_ || proxies_.contains(key))) return;
    if (!prefetch) {
        stopProxy();
        pendingPosition_ = position;
    }
    auto activate = [this, offset](const std::shared_ptr<QTemporaryDir>& temporary) {
        stopPlayer();
        stopDecoder();
        segmentOffset_ = offset;
        activeProxy_ = temporary;
        fallingBack_ = false;
        seekAfterLoad_ = true;
        notice_->clear();
        notice_->hide();
        loadVideo(temporary->filePath("preview.mp4"));
        // A stopped Qt player may not emit a frame when seeking to zero. The
        // proxy can finish before the source poster and cancel that decoder;
        // obtain a still independently so this race cannot leave a blank view.
        if (!pendingResume_ && !playbackRequested_)
            requestScrubFrame(pendingPosition_);
    };
    if (proxies_.contains(key)) {
        activate(proxies_.value(key));
        return;
    }
    auto temporary = std::shared_ptr<QTemporaryDir>(new QTemporaryDir, [](QTemporaryDir* dir) {
        const QString path = dir->path();
        dir->setAutoRemove(false);
        delete dir;
        if (!path.isEmpty()) {
            if (auto* pool = QThreadPool::globalInstance()) pool->start([path] { QDir(path).removeRecursively(); });
            else QDir(path).removeRecursively();
        }
    });
    if (!temporary->isValid()) {
        if (!prefetch) showError(tr("Não foi possível criar a prévia temporária."));
        return;
    }
    if (!prefetch) {
        notice_->setText(tr("A preparar este trecho da prévia…"));
        notice_->show();
    }
    auto* proxy = new QProcess(this);
    (prefetch ? prefetch_ : proxy_) = proxy;
    proxy->setProperty("segmentOffset", offset);
    proxy->setProperty("activateWhenReady", !prefetch);
    const auto request = generation_;
    connect(proxy, &QProcess::finished, this, [this, proxy, request, temporary, key, activate](int code) {
        if (request != generation_ || (proxy_ != proxy && prefetch_ != proxy)) return;
        if (proxy_ == proxy) proxy_ = nullptr;
        if (prefetch_ == proxy) prefetch_ = nullptr;
        const bool show = proxy->property("activateWhenReady").toBool();
        if (code != 0) {
            if (show) {
                pendingResume_ = false;
                showError(tr("Não foi possível preparar a reprodução da preview.")
                    + "\n" + QString::fromLocal8Bit(proxy->readAllStandardError()).right(1200));
            }
            return;
        }
        if (proxies_.size() >= 6) proxies_.erase(proxies_.begin());
        proxies_.insert(key, temporary);
        if (show) activate(temporary);
    });
    connect(proxy, &QProcess::finished, proxy, [proxy, temporary] { proxy->deleteLater(); });
    connect(proxy, &QProcess::errorOccurred, this, [this, proxy, request](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && request == generation_ && (proxy_ == proxy || prefetch_ == proxy)) {
            if (proxy_ == proxy) proxy_ = nullptr;
            if (prefetch_ == proxy) prefetch_ = nullptr;
            if (proxy->property("activateWhenReady").toBool()) showError(tr("Não foi possível iniciar o FFmpeg para a prévia."));
            proxy->deleteLater();
        }
    });
    QTimer::singleShot(120000, proxy, [proxy] { if (proxy->state() != QProcess::NotRunning) proxy->kill(); });
    proxy->start(framescale::RuntimePaths().tool("ffmpeg"), { "-v", "error", "-nostdin", "-y", "-threads", "2", "-ss", QString::number(offset / 1000., 'f', 3), "-i", source, "-t", QString::number(segmentLength / 1000.), "-map", "0:v:0", "-map", "0:a:0?", "-filter_threads", "2", "-vf", playbackPreviewFilter() + QStringLiteral(",fps=%1").arg(fps_, 0, 'f', 6), "-c:v", "libx264", "-preset", "ultrafast", "-crf", compact_ ? "18" : "22", "-threads", "2", "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "96k", "-movflags", "+faststart", temporary->filePath("preview.mp4") });
}

void MediaPreviewWidget::freezeVideoFrame()
{
    if (stack_->currentWidget() != videoSurface_) return;
    const QImage frame = videoWidget_->videoSink()->videoFrame().toImage();
    if (!frame.isNull()) image_ = frame;
    if (!image_.isNull()) {
        updateImageSize();
        stack_->setCurrentWidget(imageLabel_);
    }
}

void MediaPreviewWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    // Paint the space left beside the compact image dock as part of the viewer.
    if (!compact_ && !mediaPath_.isEmpty()) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#181818"));
    }
}

void MediaPreviewWidget::updateMediaDimensions()
{
    if (!dimensionsLabel_) return;
    const bool visible = sourceSize_.isValid() && !sourceSize_.isEmpty();
    dimensionsLabel_->setVisible(visible);
    if (!visible) return;
    QSize displayed = projectSize_.isValid() ? projectSize_ : sourceSize_;
    if (orientation_.quarterTurns % 2 != 0) displayed.transpose();
    dimensionsLabel_->setText(QStringLiteral("%1 × %2 px").arg(displayed.width()).arg(displayed.height()));
    if (originalDuration_ > 0)
        dimensionsLabel_->setText(dimensionsLabel_->text() + QStringLiteral(" · %1 fps").arg(QLocale().toString(fps_, 'g', 6)));
    dimensionsLabel_->updateGeometry();
}

void MediaPreviewWidget::updateVideoGeometry()
{
    QSize size = videoWidget_->videoSink()->videoFrame().size();
    if (!size.isValid()) size = sourceSize_;
    if (!size.isValid()) return;
    size.scale(videoSurface_->size(), Qt::KeepAspectRatio);
    videoWidget_->setGeometry(QRect(QPoint((videoSurface_->width()-size.width())/2,
        (videoSurface_->height()-size.height())/2), size));
}

void MediaPreviewWidget::showImage(const QImage& image)
{
    image_ = image;
    comparisonButton_->setEnabled(comparisonAllowed_);
    stack_->setCurrentWidget(imageLabel_);
    updateImageSize();
    emit mediaReady();
}

bool MediaPreviewWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type()==QEvent::MouseButtonPress && timeline_ && audioTrack_ && isVisible()) {
        auto* widget=qobject_cast<QWidget*>(watched);
        if (widget && widget->window()==window() && !qobject_cast<QMenu*>(widget)) {
            const auto* mouse=static_cast<QMouseEvent*>(event);
            const QPoint local=timeline_->mapFromGlobal(widget->mapToGlobal(mouse->position().toPoint()));
            const bool onVideo=QRect(14,54,timeline_->width()-28,51).contains(local);
            const bool onAudio=audioTrack_->isVisible() && QRect(18,108,timeline_->width()-36,46).contains(local);
            if (!onVideo && !onAudio) { timeline_->setSelected(false); audioTrack_->setSelected(false); }
        }
    }

    const bool spaceEvent = (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease
        || event->type() == QEvent::ShortcutOverride)
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space;
    if (spaceEvent && isVisible() && window()->isActiveWindow()
        && (imageLabel_->underMouse() || videoWidget_->underMouse() || spaceHeld_))
        watched = imageLabel_;
    if (watched == stack_ && event->type() == QEvent::Resize) updateMediaDimensions();
    if (watched == videoSurface_ && event->type() == QEvent::Resize) updateVideoGeometry();
    if (watched == imageLabel_ || watched == videoWidget_) {
        if (event->type() == QEvent::ShortcutOverride && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space) {
            event->accept();
            return true;
        }
        if (event->type() == QEvent::FocusOut || event->type() == QEvent::WindowDeactivate) {
            if (QWidget::mouseGrabber() == imageLabel_)
                imageLabel_->releaseMouse();
            spaceHeld_ = panning_ = false;
            imageLabel_->setCursor(Qt::ArrowCursor);
            videoWidget_->setCursor(Qt::ArrowCursor);
        }
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space) {
            spaceHeld_ = true;
            imageLabel_->setCursor(Qt::OpenHandCursor);
            videoWidget_->setCursor(Qt::OpenHandCursor);
            return true;
        } else if (event->type() == QEvent::KeyRelease && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space) {
            if (static_cast<QKeyEvent*>(event)->isAutoRepeat())
                return true;
            spaceHeld_ = false;
            panning_ = false;
            if (QWidget::mouseGrabber() == imageLabel_)
                imageLabel_->releaseMouse();
            imageLabel_->setCursor(Qt::ArrowCursor);
            videoWidget_->setCursor(Qt::ArrowCursor);
            return true;
        } else if (event->type() == QEvent::Resize)
            updateImageSize();
        else if (event->type() == QEvent::Wheel) {
            auto* wheel = static_cast<QWheelEvent*>(event);
            if (wheel->angleDelta().y() == 0)
                return true;
            const double value = std::clamp((comparisonZoom_ ? (zoom_ > 0 ? zoom_ : 1.) : actualZoom_) * (wheel->angleDelta().y() > 0 ? 1.25 : .8), .001, 64.);
            const auto* surface = qobject_cast<QWidget*>(watched);
            const QPointF anchor = surface->mapTo(stack_, wheel->position());
            const QPointF center(stack_->width()/2., stack_->height()/2.);
            const QPointF previousPan = pan_;
            const double previousScale = actualZoom_;
            setViewZoom(value);
            if (previousScale > 0) {
                pan_ = anchor-center-(anchor-center-previousPan)*(actualZoom_/previousScale);
                updateImageSize();
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            zoomChoice_->setCurrentIndex(0);
            setZoom(0);
            return true;
        } else if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton && spaceHeld_) {
                qobject_cast<QWidget*>(watched)->setFocus();
                if (stack_->currentWidget() == videoSurface_) {
                    const auto frame = videoWidget_->videoSink()->videoFrame().toImage();
                    if (!frame.isNull())
                        image_ = frame;
                }
                stack_->setCurrentWidget(imageLabel_);
                imageLabel_->setFocus();
                spaceHeld_ = panning_ = true;
                if (imageLabel_->isVisible())
                    imageLabel_->grabMouse();
                dragPoint_ = mouse->position();
                imageLabel_->setCursor(Qt::ClosedHandCursor);
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && panning_) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            pan_ += mouse->position() - dragPoint_;
            dragPoint_ = mouse->position();
            updateImageSize();
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            panning_ = false;
            if (QWidget::mouseGrabber() == imageLabel_)
                imageLabel_->releaseMouse();
            imageLabel_->setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
            videoWidget_->setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor);
        }
    }
    return QWidget::eventFilter(watched, event);
}
void MediaPreviewWidget::setZoom(double value)
{
    if (mediaPath_.isEmpty()) return;
    if (stack_->currentWidget() == videoSurface_) {
        const auto frame = videoWidget_->videoSink()->videoFrame().toImage();
        if (!frame.isNull())
            image_ = frame;
    }
    zoom_ = value;
    pan_ = { };
    stack_->setCurrentWidget(imageLabel_);
    updateImageSize();
    if (!playbackRequested_ && originalDuration_ > 0) {
        const quint64 generation = generation_;
        QTimer::singleShot(120, this, [this, value, generation] {
            if (generation == generation_ && zoom_ == value && !playbackRequested_)
                requestScrubFrame(requestedScrub_ >= 0 ? requestedScrub_ : positionMs());
        });
    }
    emit viewZoomChanged(value);
}
void MediaPreviewWidget::useComparisonZoom() { comparisonZoom_=true; zoomChoice_->setToolTip(QString()); }
void MediaPreviewWidget::setViewZoom(double value)
{
    QSignalBlocker blocker(zoomChoice_);
    if (zoomChoice_->count() > 6) zoomChoice_->removeItem(6);
    int index = value == 0 ? 0 : zoomChoice_->findData(value);
    if (index < 0) {
        zoomChoice_->addItem(QString::number(value*100.,'f',value>=.1 ? 0 : 1)+"%",value);
        index=zoomChoice_->count()-1;
        zoomChoice_->setItemIcon(index,ControlSymbols::icon(ControlSymbols::Symbol::Zoom));
    }
    zoomChoice_->setCurrentIndex(index);
    setZoom(value);
}
void MediaPreviewWidget::updateImageSize()
{
    if (image_.isNull())
        return;
    const QSize bounds = imageLabel_->contentsRect().size();
    if (bounds.isEmpty())
        return;
    QSize reference = sourceSize_.isValid() ? sourceSize_ : image_.size();
    // FFmpeg/Qt have already applied display rotation to the preview pixels.
    // Account for a quarter turn while keeping zoom relative to source pixels.
    const double imageAspect = double(image_.width()) / image_.height();
    if (qAbs(imageAspect - double(reference.height()) / reference.width()) < .01
        && qAbs(imageAspect - double(reference.width()) / reference.height()) > .01)
        reference.transpose();
    if (orientation_.quarterTurns % 2 != 0) reference.transpose();
    const double fittedScale = std::min(double(bounds.width()) / reference.width(), double(bounds.height()) / reference.height());
    const double scale = comparisonZoom_ ? fittedScale * (zoom_ > 0 ? zoom_ : 1.) : zoom_ > 0 ? zoom_ : fittedScale;
    actualZoom_ = scale;
    setProperty("zoomPercent", (comparisonZoom_ ? (zoom_ > 0 ? zoom_ : 1.) : scale) * 100.);
    if (zoomChoice_->currentIndex() == 0)
        zoomChoice_->setItemText(0, QString::number(comparisonZoom_ ? 100 : qRound(fittedScale * 100)) + "%");
    const QSizeF target(reference.width() * scale, reference.height() * scale);
    // Space-drag is a free view translation, even at fit or below 100%.
    setProperty("viewPan", pan_);
    const qreal density = devicePixelRatioF();
    QPixmap pixels(qCeil(bounds.width() * density), qCeil(bounds.height() * density));
    pixels.setDevicePixelRatio(density);
    pixels.fill(QColor("#181818"));
    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(QRectF(QPointF((bounds.width() - target.width()) / 2, (bounds.height() - target.height()) / 2) + pan_, target), orientedFrame(image_));
    painter.end();
    imageLabel_->setPixmap(pixels);
}

void MediaPreviewWidget::updatePlaybackControls()
{
    const bool available = player_ && mediaLoaded_ && !fallingBack_;
    const bool playing = available && player_->playbackState() == QMediaPlayer::PlayingState;
    playButton_->setEnabled(available);
    playButton_->setIcon(playbackIcon(playing));
    playButton_->setToolTip(playing ? tr("Pausar") : tr("Reproduzir"));
    playButton_->setAccessibleName(playButton_->toolTip());
    seekSlider_->setEnabled(available && player_->isSeekable());
    const qint64 duration = durationMs();
    const qint64 position = positionMs();
    timeline_->setRange(duration > 0 ? duration / 1000. : 1., trimStartSeconds_,
        trimEndSeconds_, position / 1000.);
    timeLabel_->setText(playbackTime(position) + QStringLiteral(" / ") + playbackTime(duration));
    if (!scrubbing_) {
        const QSignalBlocker blocked(seekSlider_);
        seekSlider_->setValue(
            duration > 0 ? static_cast<int>(position * 10000 / duration) : 0);
    }
}

void MediaPreviewWidget::videoFallback(const QString& reason)
{
    if (fallingBack_) {
        return;
    }
    if (segmented_) {
        pause();
        showError(tr("Não foi possível reproduzir a preview.") + "\n" + reason);
        return;
    }
    fallingBack_ = true;
    stopPlayer();
    toolbar_->hide();
    placeholder_->setText(tr("A carregar o primeiro frame…"));
    stack_->setCurrentWidget(placeholder_);
    updatePlaybackControls();
    prepareVideoPreview(true);
}

void MediaPreviewWidget::thumbnailFailed(const QString& message, bool video, bool staticOnly)
{
    if (video && !staticOnly && (player_ || proxy_ || prefetch_)) {
        // The independent player may still produce a frame. Report the actual
        // thumbnail failure without replacing it with a misleading Play prompt.
        if (!firstVideoFrame_) placeholder_->setText(message);
        notice_->setText(message);
        notice_->show();
        return;
    }
    showError(message);
}

void MediaPreviewWidget::decodeThumbnail(const QString& path, bool video,
    const QString& reason,
    bool staticOnly)
{
    stopDecoder();
    const QString ffmpeg = framescale::RuntimePaths().tool(QStringLiteral("ffmpeg"));
    const auto temporary = std::make_shared<QTemporaryDir>();
    if (!QFileInfo::exists(ffmpeg) || !temporary->isValid()) {
        thumbnailFailed(tr("Não foi possível preparar a imagem da preview."), video, staticOnly);
        return;
    }
    const QString output = temporary->filePath(QStringLiteral("preview.png"));
    const quint64 request = generation_;
    auto* process = new QProcess(this);
    decoder_ = process;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(
        process, &QProcess::finished, this,
        [this, process, request, temporary, output, video, reason,
            staticOnly](int code, QProcess::ExitStatus status) {
            if (request != generation_ || decoder_ != process) {
                return;
            }
            decoder_ = nullptr;
            const QImage thumbnail(output);
            if (code != 0 || status != QProcess::NormalExit || thumbnail.isNull()) {
                thumbnailFailed(tr("Não foi possível carregar a imagem da preview."), video, staticOnly);
                return;
            }
            if (video && staticOnly) {
                notice_->setText(tr("Prévia estática · A reprodução deste vídeo não "
                                    "está disponível."));
                notice_->setToolTip(reason);
                notice_->show();
            }
            if (video && !staticOnly) {
                // An explicit Play may have delivered a newer frame while
                // FFmpeg was decoding; never replace it with the first frame.
                if (firstVideoFrame_ || playbackRequested_) {
                    return;
                }
                firstVideoFrame_ = true;
                updatePlaybackControls();
            }
            showImage(thumbnail);
        });
    // This connection survives cancellation and owns the temporary image until
    // FFmpeg exits.
    connect(process, &QProcess::finished, process,
        [process, temporary](int, QProcess::ExitStatus) {
            process->deleteLater();
        });
    connect(
        process, &QProcess::errorOccurred, this,
        [this, process, request, video,
            staticOnly](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && request == generation_ && decoder_ == process) {
                decoder_ = nullptr;
                thumbnailFailed(tr("Não foi possível iniciar o FFmpeg para a prévia."), video, staticOnly);
            }
        });
    connect(process, &QProcess::errorOccurred, process,
        [process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                process->deleteLater();
            }
        });
    QTimer::singleShot(120000, process, [process] {
        if (process->state() != QProcess::NotRunning) {
            process->kill();
        }
    });
    process->start(
        ffmpeg,
        { QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
            QStringLiteral("error"), QStringLiteral("-nostdin"),
            QStringLiteral("-y"), QStringLiteral("-threads"), QStringLiteral("2"), QStringLiteral("-i"), path,
            QStringLiteral("-frames:v"), QStringLiteral("1"), QStringLiteral("-vf"),
            stillPreviewFilter(),
            QStringLiteral("-update"), QStringLiteral("1"), output });
}

void MediaPreviewWidget::showError(const QString& message)
{
    placeholder_->setText(message);
    stack_->setCurrentWidget(placeholder_);
    toolbar_->hide();
    trimBar_->hide();
    timeline_->hide();
    notice_->hide();
    emit previewError(message);
}

void MediaPreviewWidget::stopFrameReaders()
{
    if (frameReader_) frameReader_->clear();
    if (filmstripReader_) filmstripReader_->clear();
    filmstripPending_ = false;
}
void MediaPreviewWidget::requestScrubFrame(qint64 position)
{
    if (mediaPath_.isEmpty() || originalDuration_ <= 0)
        return;
    stopDecoder();
    requestedScrub_ = std::clamp(position, qint64(0), std::max(qint64(0), durationMs() - qRound64(1000. / fps_)));
    startScrubFrame();
}
void MediaPreviewWidget::startScrubFrame()
{
    if (requestedScrub_ < 0 || mediaPath_.isEmpty()) return;
    if (!frameReader_) {
        frameReader_ = new PreviewFrameReader(this);
        connect(frameReader_, &PreviewFrameReader::frameReady, this, [this](qint64 index, const QImage& frame) {
            if (playbackRequested_ || index != scrubFrameIndex_ || requestedScrub_ < 0) return;
            firstVideoFrame_ = true;
            completedScrub_ = requestedScrub_;
            completedScrubImage_ = frame;
            setProperty("scrubFramePosition", requestedScrub_);
            if (pairedScrubbing_) emit scrubFrameReady(frame, requestedScrub_);
            else showImage(frame);
        });
        connect(frameReader_, &PreviewFrameReader::failed, this, [this](const QString& detail) {
            if (!playbackRequested_) emit previewError(tr("Não foi possível carregar este fotograma para comparação.") + "\n" + detail);
        });
    }
    // Decode enough pixels for the display and requested zoom in both views.
    const double fit = sourceSize_.isValid() ? std::min(double(imageLabel_->width()) / sourceSize_.width(),
        double(imageLabel_->height()) / sourceSize_.height()) : 1.;
    const double magnification = comparisonZoom_ ? std::max(1., zoom_)
        : (fit > 0 && zoom_ > 0 ? std::max(1., zoom_ / fit) : 1.);
    const double density = devicePixelRatioF() * magnification;
    QSize bounds(std::clamp(qCeil(qMax(640,imageLabel_->width())*density),64,4096),
                       std::clamp(qCeil(qMax(360,imageLabel_->height())*density),64,4096));
    if (sourceSize_.isValid()) bounds = sourceSize_.scaled(bounds, Qt::KeepAspectRatio);
    const double pixels = double(bounds.width()) * bounds.height();
    if (pixels > 33554432.) {
        const double reduction = std::sqrt(33554432. / pixels);
        bounds = QSize(qMax(1,int(bounds.width()*reduction)),qMax(1,int(bounds.height()*reduction)));
    }
    // Paused inspection always reads the source, not the playback proxy.
    const qint64 count = std::max(qint64(1),qRound64(durationMs()*fps_/1000.));
    scrubFrameIndex_ = std::clamp(qint64(std::floor((requestedScrub_+0.5)*fps_/1000.)),qint64(0),count-1);
    frameReader_->configure(framescale::RuntimePaths().tool("ffmpeg"),mediaPath_,bounds,fps_,count);
    frameReader_->request(scrubFrameIndex_);
}

void MediaPreviewWidget::startFilmstrip()
{
    if (originalDuration_ <= 0 || filmstripPending_) return;
    filmstrip_ = QVector<QImage>(16);
    filmstripIndex_ = 0;
    timeline_->setThumbnails(filmstrip_);
    nextFilmstripFrame();
}
void MediaPreviewWidget::nextFilmstripFrame()
{
    if (filmstripPending_ || filmstripIndex_ >= filmstrip_.size() || mediaPath_.isEmpty()) return;
    if (proxy_ || (frameReader_ && frameReader_->busy())) {
        QTimer::singleShot(150,this,&MediaPreviewWidget::nextFilmstripFrame);
        return;
    }
    if (!filmstripReader_) {
        filmstripReader_ = new PreviewFrameReader(this);
        connect(filmstripReader_,&PreviewFrameReader::frameReady,this,[this](qint64 frame,const QImage& image) {
            if (!filmstripPending_ || frame!=filmstripFrameIndex_ || filmstripIndex_>=filmstrip_.size()) return;
            filmstripPending_ = false;
            QImage thumbnail(160,90,QImage::Format_RGB32);
            thumbnail.fill(Qt::black);
            { QPainter painter(&thumbnail);painter.drawImage((160-image.width())/2,(90-image.height())/2,image); }
            filmstrip_[filmstripIndex_++] = thumbnail;
            timeline_->setThumbnails(filmstrip_);
            emit thumbnailsChanged(filmstrip_);
            QTimer::singleShot(0,this,&MediaPreviewWidget::nextFilmstripFrame);
        });
        connect(filmstripReader_,&PreviewFrameReader::failed,this,[this](const QString&) {
            filmstripPending_=false;
            ++filmstripIndex_;
            QTimer::singleShot(0,this,&MediaPreviewWidget::nextFilmstripFrame);
        });
    }
    const qint64 start = std::clamp(filmstripStart_,qint64(0),originalDuration_);
    const qint64 length = filmstripLength_>0 ? std::min(filmstripLength_,originalDuration_-start) : originalDuration_-start;
    const qint64 time = std::min(start+qRound64(length*filmstripIndex_/16.),std::max(qint64(0),originalDuration_-qRound64(1000./fps_)));
    const qint64 count = std::max(qint64(1),qRound64(originalDuration_*fps_/1000.));
    filmstripFrameIndex_=std::clamp(qint64(std::floor((time+0.5)*fps_/1000.)),qint64(0),count-1);
    filmstripPending_=true;
    filmstripReader_->configure(framescale::RuntimePaths().tool("ffmpeg"),mediaPath_,QSize(160,90),fps_,count);
    filmstripReader_->request(filmstripFrameIndex_);
}
void MediaPreviewWidget::setVolume(int percent)
{
    volume_ = std::clamp(percent, 0, 100);
    if (audioOutput_)
        audioOutput_->setVolume(volume_ / 100.f);
    if (volumeSlider_) {
        QSignalBlocker block(volumeSlider_);
        volumeSlider_->setValue(volume_);
    }
    if (!compact_)
        QSettings().setValue("previewVolume", volume_);
    updateVolumeIcon();
}
void MediaPreviewWidget::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type()==QEvent::PaletteChange && volumeButton_)
        updateVolumeIcon();
}
void MediaPreviewWidget::updateVolumeIcon()
{
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink = Appearance::light() ? QColor("#302d36") : QColor("#f1f0f5");
    p.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(ink);
    p.drawPolygon(QPolygonF { { 4, 9 }, { 8, 9 }, { 12, 5 }, { 12, 19 }, { 8, 15 }, { 4, 15 } });
    p.setBrush(Qt::NoBrush);
    if (volume_) {
        p.drawArc(QRectF(8, 7, 10, 10), -55 * 16, 110 * 16);
        if (volume_ > 50)
            p.drawArc(QRectF(5, 3, 17, 18), -50 * 16, 100 * 16);
    } else {
        p.drawLine(16, 9, 21, 15);
        p.drawLine(16, 15, 21, 9);
    }
    p.end();
    volumeButton_->setIcon(QIcon(pixmap));
}
void MediaPreviewWidget::setCompact(bool compact)
{
    layout()->setContentsMargins(0, compact ? 0 : 48, 0, 0);
    compact_ = compact;
    dock_->setProperty("zoomOnly", compact);
    zoomBar_->setObjectName(compact ? "previewZoomOnly" : "previewZoomBar");
    zoomBar_->setStyleSheet(compact ? "QWidget#previewZoomOnly {background:transparent;border:0;}" : "");
    zoomBar_->layout()->setContentsMargins(compact ? 0 : 10, compact ? 0 : 7, compact ? 0 : 10, compact ? 0 : 7);
    zoomBar_->setFixedHeight(compact ? 26 : 38);
    dock_->update();
    if (compact) {
        dimensionsLabel_->setProperty("appearanceManaged", false);
        dimensionsLabel_->setStyleSheet("QLabel {color:#dddddd;background:transparent;border:0;padding:5px 16px;font-size:12px;}");
    }
    if (compact && !imageLabel_->graphicsEffect()) imageLabel_->setGraphicsEffect(new RoundedWindowEffect(imageLabel_));
    zoomBar_->setVisible(true);
    comparisonButton_->setVisible(!compact);
    volumeButton_->setVisible(!compact && !layerEditing_);
    timeline_->setVisible(!compact && !layerEditing_ && originalDuration_ > 0);
    audioTrack_->setVisible(!compact && originalDuration_ > 0 && property("hasAudio").toBool());
    timeline_->setTrackMode(!compact && property("hasAudio").toBool());
    for (auto* shortcut : findChildren<QShortcut*>()) if(shortcut->objectName()=="timelineUndo" || shortcut->objectName()=="timelineRedo") shortcut->setEnabled(!compact);
    spaceShortcut_->setEnabled(!compact);
}
bool MediaPreviewWidget::isPlaying() const { return player_ && player_->playbackState() == QMediaPlayer::PlayingState; }
bool MediaPreviewWidget::isPlayable() const { return player_ && mediaLoaded_; }
void MediaPreviewWidget::play()
{
    if (!isPlaying())
        playButton_->click();
}

void MediaPreviewWidget::setComparisonAllowed(bool allowed)
{
    comparisonAllowed_ = allowed;
    comparisonButton_->setEnabled(allowed && !mediaPath_.isEmpty());
}

void MediaPreviewWidget::setAudioLinked(bool linked) { audioTrack_->setLinked(linked); timeline_->setTracksLinked(linked); timeline_->setSelected(false); audioTrack_->setSelected(false); }
bool MediaPreviewWidget::audioLinked() const { return audioTrack_->linked(); }
void MediaPreviewWidget::setAudioOffset(int milliseconds)
{
    const int value = std::clamp(milliseconds, -600000, 600000);
    if (value == audioOffsetMs_) { audioTrack_->setOffset(value); return; }
    pause(); audioOffsetMs_ = value; audioTrack_->setOffset(value);
    timeline_->setPlacement(videoTimelineStartMs_/1000., (videoTimelineStartMs_+audioOffsetMs_)/1000.);
    if (continuousAudio_) continuousAudio_->seek(positionMs() - audioOffsetMs_);
    else configureShiftedAudio();
}
void MediaPreviewWidget::configureShiftedAudio()
{
    shiftedAudioTimer_->stop();
    delete shiftedAudioPlayer_; shiftedAudioPlayer_ = nullptr;
    if (!player_ || !audioOutput_) return;
    if (continuousAudio_) { continuousAudio_->seek(positionMs() - audioOffsetMs_); return; }
    if (!audioOffsetMs_) { if (player_->audioOutput() != audioOutput_) player_->setAudioOutput(audioOutput_); return; }
    player_->setAudioOutput(nullptr);
    shiftedAudioPlayer_ = new QMediaPlayer(this);
    shiftedAudioPlayer_->setObjectName("shiftedAudioPlayer");
    shiftedAudioPlayer_->setPlaybackRate(playbackSpeed_);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    shiftedAudioPlayer_->setPitchCompensation(false);
#endif
    shiftedAudioPlayer_->setAudioOutput(audioOutput_);
    connect(shiftedAudioPlayer_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && !shiftedAudioPlayer_->property("audioTrackReady").toBool()) {
            shiftedAudioPlayer_->setProperty("audioTrackReady", true);
            shiftedAudioPlayer_->setActiveVideoTrack(-1);
            QTimer::singleShot(0, shiftedAudioPlayer_, [this] { syncShiftedAudio(true); });
        }
    });
    connect(shiftedAudioPlayer_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& text) { emit previewError(text); });
    shiftedAudioPlayer_->setSource(QUrl::fromLocalFile(mediaPath_));
    connect(player_, &QMediaPlayer::playbackStateChanged, shiftedAudioPlayer_, [this](QMediaPlayer::PlaybackState state) {
        if (state == QMediaPlayer::PlayingState && playbackRequested_) { syncShiftedAudio(true); shiftedAudioTimer_->start(); }
        else { shiftedAudioTimer_->stop(); shiftedAudioPlayer_->pause(); }
    });
}
void MediaPreviewWidget::syncShiftedAudio(bool seek)
{
    if (!shiftedAudioPlayer_) return;
    const qint64 position = positionMs() - audioOffsetMs_;
    if (!playbackRequested_ || position < 0) { shiftedAudioPlayer_->pause(); if (seek) shiftedAudioPlayer_->setPosition(std::max<qint64>(0, position)); return; }
    if (seek || shiftedAudioPlayer_->playbackState() != QMediaPlayer::PlayingState) {
        if (shiftedAudioPlayer_->duration() > 0 && position >= shiftedAudioPlayer_->duration()) return;
        shiftedAudioPlayer_->setPosition(position); shiftedAudioPlayer_->play();
    }
}

void MediaPreviewWidget::setVideoTimelineStart(int ms) {
    videoTimelineStartMs_=std::clamp(ms,0,600000);
    timeline_->setPlacement(videoTimelineStartMs_/1000., (videoTimelineStartMs_+audioOffsetMs_)/1000.);
}
MediaPreviewWidget::TimelineEdit MediaPreviewWidget::timelineState() const {
    return {audioOffsetMs_,videoTimelineStartMs_,audioLinked(),trimStartSeconds_,trimEndSeconds_};
}
void MediaPreviewWidget::beginTimelineEdit() { pause(); editBefore_=timelineState(); }
void MediaPreviewWidget::finishTimelineEdit() {
    const auto now=timelineState();
    if(now.audio==editBefore_.audio && now.video==editBefore_.video && now.linked==editBefore_.linked && now.start==editBefore_.start && now.end==editBefore_.end) return;
    undoEdits_.append(editBefore_); if(undoEdits_.size()>100) undoEdits_.removeFirst(); redoEdits_.clear();
}
void MediaPreviewWidget::restoreTimelineEdit(const TimelineEdit& state) {
    pause(); setVideoTimelineStart(state.video); setAudioOffset(state.audio); setAudioLinked(state.linked);
    setTrimRange(state.start,state.end); emit trimChanged(state.start,state.end);
}
void MediaPreviewWidget::undoTimeline() {
    if(undoEdits_.isEmpty()) return; redoEdits_.append(timelineState()); restoreTimelineEdit(undoEdits_.takeLast());
}
void MediaPreviewWidget::redoTimeline() {
    if(redoEdits_.isEmpty()) return; undoEdits_.append(timelineState()); restoreTimelineEdit(redoEdits_.takeLast());
}
void MediaPreviewWidget::selectTimelineTrack(bool audio, bool additive) {
    if (audioLinked()) { timeline_->setSelected(true); audioTrack_->setSelected(true); return; }
    if (additive) {
        QWidget* track=audio ? static_cast<QWidget*>(audioTrack_) : static_cast<QWidget*>(timeline_);
        const bool selected=!track->property("selected").toBool();
        if(audio) audioTrack_->setSelected(selected); else timeline_->setSelected(selected);
    } else { timeline_->setSelected(!audio); audioTrack_->setSelected(audio); }
}
void MediaPreviewWidget::showTimelineMenu(const QPoint& position) {
    if(compact_ || !property("hasAudio").toBool()) return;
    if (!audioLinked() && !(timeline_->property("selected").toBool() && audioTrack_->property("selected").toBool())) return;
    SelectionMenu menu(this);
    auto* link=menu.addAction(audioLinked() ? tr("Separar áudio e vídeo") : tr("Juntar áudio e vídeo"));
    link->setObjectName("toggleAudioVideoLink");
    auto* chosen=menu.exec(position);
    if(chosen==link) { beginTimelineEdit(); setAudioLinked(!audioLinked()); finishTimelineEdit(); }
}

void MediaPreviewWidget::setLayerEditorWidget(QWidget* editor) {
    if(auto* layout=qobject_cast<QVBoxLayout*>(dock_->layout()))
        layout->insertWidget(layout->indexOf(timeline_),editor);
}

void MediaPreviewWidget::showLayerVolume(const QPoint& position) {
    auto* popup=findChild<QFrame*>("previewVolumePopup");
    if(!popup) return;
    popup->adjustSize();
    const auto area=screen()->availableGeometry();
    const QPoint anchor(std::clamp(position.x(),area.left(),std::max(area.left(),area.right()-popup->width())),
        std::clamp(position.y()-popup->height()-6,area.top(),std::max(area.top(),area.bottom()-popup->height())));
    popup->move(anchor); popup->show(); volumeSlider_->setFocus();
}

void MediaPreviewWidget::setLayerEditing(bool enabled) {
    layerEditing_ = enabled;
    volumeButton_->setVisible(!compact_ && !enabled);
    for(const auto& name : {QStringLiteral("timelineUndo"),QStringLiteral("timelineRedo")})
        if(auto* shortcut=findChild<QShortcut*>(name)) shortcut->setEnabled(!enabled);
    if (enabled) { timeline_->hide(); audioTrack_->clear(); }
    else timeline_->setVisible(!compact_ && originalDuration_ > 0);
}

void MediaPreviewWidget::setPlaybackSpeed(double speed) {
    speed=std::isfinite(speed) ? std::clamp(speed,.25,4.) : 1.;
    if(playbackSpeed_==speed) return;
    pause(); playbackSpeed_=speed; timeline_->setPlaybackSpeed(speed);
    if(player_) player_->setPlaybackRate(speed);
    if(shiftedAudioPlayer_) shiftedAudioPlayer_->setPlaybackRate(speed);
    if(continuousAudio_) { continuousAudio_->setPlaybackSpeed(speed); continuousAudio_->seek(positionMs()-audioOffsetMs_); }
}
