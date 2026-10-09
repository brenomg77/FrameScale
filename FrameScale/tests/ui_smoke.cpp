#include <QShortcut>
#include "ui/EditorControls.h"
#include <QScreen>
#include "MainWindow.h"
#include "ui/Typography.h"
#include "ui/Appearance.h"
#include "ui/SelectionComboBox.h"
#include "ui/ComparisonDialog.h"
#include "ui/EncoderOptionsWidget.h"
#include "ui/EnhancementWidget.h"
#include "ui/ExportDialog.h"
#include "ui/InterfaceTranslator.h"
#include "ui/MediaPreviewWidget.h"
#include "ui/TrimTimeline.h"
#include "ui/AudioTrack.h"
#include "ui/HelpPopover.h"
#include <QAction>
#include <QAbstractItemView>
#include <QApplication>
#include <QAudioOutput>
#include <QAudioSink>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QAudioBufferOutput>
#include <QAudioBuffer>
#endif
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFontDatabase>
#include <QGroupBox>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMediaPlayer>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QProcess>
#include "processing/RuntimePaths.h"
#include "processing/ProcessingJob.h"
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <functional>
#include <iostream>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

bool waitUntil(const std::function<bool()>& predicate, int timeout = 15000)
{
    QEventLoop loop;
    QTimer tick, watchdog;
    tick.setInterval(25);
    watchdog.setSingleShot(true);
    QObject::connect(&tick, &QTimer::timeout, &loop, [&] {
        if (predicate())
            loop.quit();
    });
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    tick.start();
    watchdog.start(timeout);
    if (!predicate())
        loop.exec();
    return predicate();
}
int main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName("FrameScale");
    QCoreApplication::setOrganizationName("FrameScaleTests");
    QCoreApplication::setApplicationVersion(FRAMESCALE_VERSION);
#ifdef Q_OS_WIN
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
#endif
    Typography::apply();
    int failures = 0;
    auto check = [&](bool value, const char* message) {
        if (!value) {
            std::cerr << message << '\n';
            ++failures;
        }
    };
    {
        QWidget host;
        QSpinBox field(&host);
        field.setToolTip("Stable field help");
        host.show();
        app.processEvents();
        HelpPopoverFilter filter(nullptr);
        struct VisibilityCounter : QObject {
            int changes = 0;
            bool eventFilter(QObject*, QEvent* e) override {
                if (e->type() == QEvent::Show || e->type() == QEvent::Hide) ++changes;
                return false;
            }
        } counter;
        const QPoint local(5, 5);
        QHelpEvent first(QEvent::ToolTip, local, field.mapToGlobal(local));
        filter.eventFilter(&field, &first);
        QWidget* bubble = nullptr;
        for (auto* window : QApplication::topLevelWidgets())
            if (window->objectName() == "helpPopover" && window->isVisible()) bubble = window;
        check(bubble != nullptr, "field tooltip did not appear");
        if (bubble) {
            bubble->installEventFilter(&counter);
            const QRect original = bubble->geometry();
            auto* editor = field.findChild<QLineEdit*>();
            for (int i = 0; i < 10; ++i) {
                QHelpEvent repeated(QEvent::ToolTip, QPoint(5+i,5), field.mapToGlobal(QPoint(5+i,5)));
                filter.eventFilter(&field, &repeated);
                filter.eventFilter(editor, &repeated);
            }
            check(counter.changes == 0 && bubble->geometry() == original,
                "repeated tooltip events flicker or move the balloon");
            field.setToolTip("Updated field help");
            filter.eventFilter(&field, &first);
            check(bubble->accessibleDescription() == "Updated field help", "tooltip content failed to refresh");
            QEvent click(QEvent::MouseButtonPress);
            filter.eventFilter(&field, &click);
            check(!bubble->isVisible(), "tooltip did not dismiss on interaction");
        }
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (app.arguments().contains("--audio-diagnostic")) {
        MediaPreviewWidget preview;
        preview.resize(1280, 900);
        preview.show();
        preview.setMedia(qEnvironmentVariable("FRAMESCALE_AUDIO_SOURCE"));
        if (!waitUntil([&] { return preview.isPlayable(); }, 30000)) return 2;
        const double diagnosticSpeed=qEnvironmentVariableIsSet("FRAMESCALE_AUDIO_SPEED") ? qEnvironmentVariable("FRAMESCALE_AUDIO_SPEED").toDouble() : 1.;
        preview.setPlaybackSpeed(diagnosticSpeed);
        auto* player = preview.findChild<QMediaPlayer*>("previewPlayer");
        QMediaPlayer plain;
        QAudioOutput plainAudio;
        const bool standalone = qEnvironmentVariableIsSet("FRAMESCALE_AUDIO_STANDALONE");
        if (standalone) {
            preview.clear();
            plain.setAudioOutput(&plainAudio);
            plainAudio.setVolume(.35);
            plain.setSource(QUrl::fromLocalFile(qEnvironmentVariable("FRAMESCALE_AUDIO_SOURCE")));
            player = &plain;
            if (!waitUntil([&] { return plain.mediaStatus() == QMediaPlayer::LoadedMedia; }, 15000)) return 5;
        }
        QAudioBufferOutput output;
        const bool continuous = preview.property("continuousPreviewAudio").toBool() && !standalone;
        if (!continuous) player->setAudioBufferOutput(&output);
        QFile pcm(qEnvironmentVariable("FRAMESCALE_AUDIO_PCM"));
        if (!pcm.open(QIODevice::WriteOnly)) return 3;
        int count = 0;
        QObject::connect(&output, &QAudioBufferOutput::audioBufferReceived, &preview, [&](const QAudioBuffer& b) {
            if (!b.isValid()) return;
            pcm.write(b.constData<char>(), b.byteCount());
            std::cout << b.startTime() << "," << b.duration() << "," << b.frameCount() << "," << b.format().sampleRate() << "," << b.format().channelCount() << "," << int(b.format().sampleFormat()) << std::endl;
            ++count;
        });
        if (standalone) player->play(); else preview.play();
        QElapsedTimer elapsed; elapsed.start();
        waitUntil([&] { return elapsed.elapsed() >= 12000; }, 15000);
        std::cerr << "Final position: " << player->position() << std::endl;
        if (continuous) {
            auto* sink = preview.findChild<QAudioSink*>();
            check(sink && sink->processedUSecs() > 11000000 && sink->processedUSecs() < 13000000,
                "continuous audio is not advancing at native sample rate");
            check(player->position() > 11000*diagnosticSpeed && player->position() < 13000*diagnosticSpeed,
                "video clock does not follow real time");
            preview.pause();
            preview.seek(3000);
            preview.setVolume(72);
            preview.play();
            check(waitUntil([&] { return player->position() > 4000; }, 4000), "audio preview seek did not resume");
            check(sink && qAbs(sink->volume() - .72) < .01, "continuous audio ignores volume");
            preview.pause();
            check(sink && sink->state() == QAudio::SuspendedState, "continuous audio did not pause");
        } else player->pause();
        player->setAudioBufferOutput(nullptr);
        return failures ? 6 : player->position() > 3000 ? 0 : 4;
    }
#endif
    if (app.arguments().contains("--fps-controls")) {
        MainWindow window;
        window.show();
        window.openInputFile(qEnvironmentVariable("FRAMESCALE_UI_VIDEO"));
        auto* preview = window.findChild<MediaPreviewWidget*>();
        check(waitUntil([&] { return preview && preview->durationMs() > 0; }), "FPS metadata not loaded");
        auto* interpolation = window.findChild<QCheckBox*>("enableInterpolation");
        auto* target = window.findChild<QSpinBox*>("targetFps");
        auto* reduction = window.findChild<QSpinBox*>("reductionFps");
        interpolation->setChecked(true);
        target->setValue(24);
        check(target->value() == 61 && target->minimum() == 61, "interpolation accepts FPS below source");
        check(!reduction->isVisible(), "reduction and interpolation shown together");
        interpolation->setChecked(false);
        reduction->setValue(24);
        check(reduction->isVisible() && reduction->value() == 24 && reduction->maximum() == 59, "FPS reduction range incorrect");
        return failures ? 1 : 0;
    }
    if (app.arguments().contains("--audio-tracks")) {
        QApplication::setStyle("Fusion"); Appearance::palette();
        MediaPreviewWidget preview;
        preview.resize(1100, 800); preview.show();
        preview.setMedia(qEnvironmentVariable("FRAMESCALE_UI_VIDEO"));
        check(waitUntil([&] { return preview.isPlayable(); }), "audio track media not ready");
        auto* track = static_cast<AudioTrack*>(preview.findChild<QWidget*>("audioTrack"));
        auto* timeline=static_cast<TrimTimeline*>(preview.findChild<QWidget*>("trimTimeline"));
        check(track && waitUntil([&] { return track->property("waveformReady").toBool(); }), "waveform did not finish");
        check(track->parentWidget()==timeline, "audio not embedded in common timeline");
        check(!preview.findChild<QWidget*>("linkAudioVideo") && !preview.findChild<QWidget*>("audioOffsetMs") && !preview.findChild<QWidget*>("resetAudioOffset"), "obsolete audio controls remain");
        check(preview.audioLinked(), "tracks not linked by default");
        preview.setAudioLinked(true);
        check(!track->property("selected").toBool() && !timeline->property("selected").toBool(), "media starts with selected tracks");
        auto toggleLink=[&] {
            QTimer::singleShot(30,[&] {
                auto* menu=qobject_cast<QMenu*>(QApplication::activePopupWidget());
                check(menu!=nullptr,"timeline context menu missing");
                if(menu) { const QString captures=qEnvironmentVariable("FRAMESCALE_UI_CAPTURES"); if(!captures.isEmpty()) menu->grab().save(captures+"/timeline-menu.png"); check(menu->actions().size()==1,"timeline menu contains extra actions"); menu->setActiveAction(menu->actions().first()); QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier); QApplication::sendEvent(menu,&enter); }
            });
            QContextMenuEvent context(QContextMenuEvent::Mouse,QPoint(200,80),timeline->mapToGlobal(QPoint(200,80)));
            QApplication::sendEvent(timeline,&context);
        };
        toggleLink(); check(!preview.audioLinked(),"context menu did not unlink");
        preview.undoTimeline(); check(preview.audioLinked(),"undo did not restore linked state");
        preview.redoTimeline(); check(!preview.audioLinked(),"redo did not restore unlinked state");
        auto drag=[](QWidget* widget,QPointF from,QPointF to) {
            QMouseEvent down(QEvent::MouseButtonPress,from,from,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent motion(QEvent::MouseMove,to,to,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent up(QEvent::MouseButtonRelease,to,to,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(widget,&down); QApplication::sendEvent(widget,&motion); QApplication::sendEvent(widget,&up);
        };
        drag(track,{200,27},{320,27});
        check(preview.audioOffset()>0 && track->property("selected").toBool() && !timeline->property("selected").toBool(),"audio drag or selection failed");
        const int moved=preview.audioOffset();
        QApplication::setActiveWindow(&preview); timeline->setFocus(); QApplication::processEvents();
        QKeyEvent undoKey(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier);
        QApplication::sendEvent(timeline,&undoKey);
        check(preview.audioOffset()==0,"Ctrl+Z did not revert whole audio drag");
        QKeyEvent redoKey(QEvent::KeyPress,Qt::Key_Z,Qt::ControlModifier|Qt::ShiftModifier);
        QApplication::sendEvent(timeline,&redoKey);
        check(preview.audioOffset()==moved,"Ctrl+Shift+Z did not restore audio drag");
        const int oldVideo=preview.videoTimelineStart();
        drag(timeline,{300,80},{390,80});
        check(preview.videoTimelineStart()>oldVideo && preview.audioOffset()<moved,"video could not move independently");
        check(timeline->property("selected").toBool() && !track->property("selected").toBool(),"video selection not exclusive");
        preview.undoTimeline(); check(preview.videoTimelineStart()==oldVideo && preview.audioOffset()==moved,"video movement undo failed");
        preview.setAudioOffset(120);
        const QPointF snapFrom(220,27), snapTo(220-.100/timeline->visibleDuration()*(track->width()-36),27);
        QMouseEvent snapDown(QEvent::MouseButtonPress,snapFrom,snapFrom,Qt::LeftButton,Qt::LeftButton,Qt::ShiftModifier);
        QMouseEvent snapMove(QEvent::MouseMove,snapTo,snapTo,Qt::NoButton,Qt::LeftButton,Qt::ShiftModifier);
        QMouseEvent snapUp(QEvent::MouseButtonRelease,snapTo,snapTo,Qt::LeftButton,Qt::NoButton,Qt::ShiftModifier);
        QApplication::sendEvent(track,&snapDown); QApplication::sendEvent(track,&snapMove); QApplication::sendEvent(track,&snapUp);
        check(preview.audioOffset()==0,"Shift did not snap audio precisely to video start");
        preview.undoTimeline(); check(preview.audioOffset()==120,"snapped audio drag undo failed");
        drag(track,{200,27},{200,27});
        QMouseEvent footerClick(QEvent::MouseButtonPress,QPointF(200,175),QPointF(200,175),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(timeline,&footerClick);
        check(!track->property("selected").toBool() && !timeline->property("selected").toBool(),"timeline footer did not clear selection");
        drag(track,{200,27},{200,27});
        QWidget outside(&preview); outside.setObjectName("deselectTest"); outside.setGeometry(0,0,20,20); outside.show();
        QMouseEvent outsideClick(QEvent::MouseButtonPress,QPointF(2,2),QPointF(2,2),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&outside,&outsideClick);
        check(!track->property("selected").toBool() && !timeline->property("selected").toBool(),"click outside tracks did not clear selection");
        preview.setAudioOffset(500);
        check(qAbs(timeline->visibleDuration()-4.)<.01,"audio shift extended video ruler and left trailing gap");
        preview.seek(0); preview.play();
        auto* audio = preview.findChild<QMediaPlayer*>("shiftedAudioPlayer");
        check(audio != nullptr, "separate audio player missing");
        check(waitUntil([&] { return preview.positionMs() > 1200; }), "preview did not play");
        check(audio && qAbs(preview.positionMs() - audio->position() - 500) < 200, "positive audio offset is not applied");
        preview.pause();
        check(audio && audio->playbackState() != QMediaPlayer::PlayingState, "shifted audio continued after pause");
        preview.setAudioOffset(-500); preview.seek(0); preview.play();
        audio = preview.findChild<QMediaPlayer*>("shiftedAudioPlayer");
        check(waitUntil([&] { return preview.positionMs() > 700; }), "negative offset did not play");
        check(audio && qAbs(audio->position() - preview.positionMs() - 500) < 200, "negative audio offset is not applied");
        preview.pause();
        for(double speed : {.5,2.}) {
            preview.setPlaybackSpeed(speed); preview.setAudioOffset(0); preview.seek(0);
            QElapsedTimer timer; timer.start(); preview.play();
            check(waitUntil([&] { return timer.elapsed()>900; },3000),"speed clock wait failed");
            const double elapsed=timer.elapsed();
            check(qAbs(preview.positionMs()-elapsed*speed)<400,"preview speed did not change source clock correctly");
            preview.pause();
        }
        preview.setPlaybackSpeed(1.);
        drag(timeline,{300,80},{300,80});
        check(timeline->property("selected").toBool() && !track->property("selected").toBool(),"plain click did not select only video");
        QContextMenuEvent singleContext(QContextMenuEvent::Mouse,QPoint(300,80),timeline->mapToGlobal(QPoint(300,80)));
        QApplication::sendEvent(timeline,&singleContext);
        check(!QApplication::activePopupWidget() && !preview.audioLinked(),"single selection allowed relinking");
        QMouseEvent ctrlClick(QEvent::MouseButtonPress,QPointF(200,27),QPointF(200,27),Qt::LeftButton,Qt::LeftButton,Qt::ControlModifier);
        QApplication::sendEvent(track,&ctrlClick);
        check(track->property("selected").toBool() && timeline->property("selected").toBool(),"Ctrl click did not select both tracks");
        toggleLink(); check(preview.audioLinked(),"context menu did not relink selected pair");
        check(timeline->cursor().shape()==Qt::ArrowCursor,"timeline still uses hand cursor");
        const int locked=preview.audioOffset(); drag(track,{200,27},{260,27});
        check(preview.audioOffset()==locked && track->property("selected").toBool() && timeline->property("selected").toBool(),"linked selection or position changed");
        preview.setAudioOffset(0);
        const double endBefore=preview.trimEndSeconds();
        drag(timeline,{double(timeline->width()-18),70},{double(timeline->width()-90),70});
        check(preview.trimEndSeconds()>0,"trim did not change");
        preview.undoTimeline(); check(preview.trimEndSeconds()==endBefore,"trim undo failed");
        const QString capture = qEnvironmentVariable("FRAMESCALE_UI_CAPTURES");
        if (!capture.isEmpty()) { QDir().mkpath(capture); preview.grab().save(capture+"/audio-tracks.png"); }
        preview.clear();
        preview.undoTimeline();
        check(!track->isVisible() && preview.audioOffset() == 0 && preview.audioLinked(), "media clear left stale audio state or undo history");
        return failures ? 1 : 0;
    }
    QTemporaryDir fixture;
    qputenv("FRAMESCALE_PRESET_DIR", (fixture.path() + "/Presets").toUtf8());
    {
        TrimTimeline timeline;
        timeline.resize(1000, 160);
        for (double fps : {24., 60., 120.}) {
            timeline.setFrameRate(fps);
            for (double start : {0., 19., 38. - 1. / fps}) {
                for (bool right : {false, true}) {
                    if ((!right && start == 0.) || (right && start > 37.)) continue;
                    const double end = start + 1. / fps;
                    timeline.setRange(38., start, end, 10.);
                    double editedStart = start, editedEnd = end;
                    int edits = 0, seeks = 0;
                    timeline.rangeEdited = [&](double a, double b) {
                        editedStart = a; editedEnd = b == 0 ? 38. : b; ++edits;
                        timeline.setRange(38., a, b, 10.);
                    };
                    timeline.seekRequested = [&](double) { ++seeks; };
                    const QPointF from(18. + (start + end) / 2. / 38. * 964., 70.);
                    const QPointF to(from.x() + (right ? 120. : -120.), 70.);
                    QMouseEvent down(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent motion(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent up(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(&timeline, &down);
                    check(edits == 0 && seeks == 0, "overlapping handle press changed range or sought video");
                    QApplication::sendEvent(&timeline, &motion);
                    QApplication::sendEvent(&timeline, &up);
                    check(edits > 0 && seeks == 0, "overlapping handles failed to edit range");
                    check(right ? editedEnd > end + 1. && qAbs(editedStart - start) < .00001
                                : editedStart < start - 1. && qAbs(editedEnd - end) < .00001,
                        "one-frame selection cannot be expanded in drag direction");
                }
            }
        }
    }
    {
        TrimTimeline timeline;
        timeline.resize(1000, 194);
        timeline.setFrameRate(60.);
        timeline.setRange(40., 19., 21., 20.);
        int edits = 0, seeks = 0;
        double sought = -1., editedEnd = -1.;
        timeline.rangeEdited = [&](double, double end) { ++edits; editedEnd = end; };
        timeline.seekRequested = [&](double t) { ++seeks; sought = t; };
        timeline.setZoom(10., 500.);
        check(qAbs(timeline.visibleStart() - 18.) < .00001 && qAbs(timeline.visibleDuration() - 4.) < .00001,
            "timeline zoom did not preserve anchor");
        timeline.setRange(40., 19., 21., 20.);
        check(timeline.zoomFactor() == 10. && edits == 0 && seeks == 0,
            "decoder update reset zoom or zoom changed selection");
        QWheelEvent wheel(QPointF(500,70), QPointF(500,70), QPoint(), QPoint(0,240),
            Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&timeline, &wheel);
        check(qAbs(timeline.zoomFactor() - 20.) < .00001 && qAbs(timeline.visibleStart() - 19.) < .00001,
            "Ctrl wheel zoom anchor incorrect");
        auto* scroll = timeline.findChild<QScrollBar*>("timelineScroll");
        scroll->setValue(scroll->maximum());
        check(qAbs(timeline.visibleStart() - 38.) < .00001, "zoomed timeline cannot scroll to end");
        scroll->setValue(0);
        check(timeline.visibleStart() == 0., "zoomed timeline cannot scroll to start");
        timeline.resetView();
        timeline.setRange(40., 19., 20., 20.);
        timeline.setZoom(1.e9, 500.);
        check(qAbs(timeline.visibleDuration() * 60. - 8.) < .00001, "maximum zoom must show eight frames");
        const double endpoint = 18. + (20. - timeline.visibleStart()) / timeline.visibleDuration() * 964.;
        const QPointF from(endpoint, 70.), to(endpoint + 964. / 8., 70.);
        QMouseEvent down(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent motion(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&timeline, &down);
        QApplication::sendEvent(&timeline, &motion);
        QApplication::sendEvent(&timeline, &up);
        check(edits > 0 && seeks == 0 && qAbs(editedEnd - (20. + 1. / 60.)) < .00001,
            "zoomed trim cannot select exactly one frame");
        const QPointF ruler(500., 20.);
        QMouseEvent seek(QEvent::MouseButtonPress, ruler, ruler, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&timeline, &seek);
        QApplication::sendEvent(&timeline, &up);
        check(seeks == 1 && qAbs(sought - 20.) < .00001, "zoomed ruler seek maps to wrong time");
        QKeyEvent fit(QEvent::KeyPress, Qt::Key_0, Qt::NoModifier);
        QApplication::sendEvent(&timeline, &fit);
        check(timeline.zoomFactor() == 1. && timeline.visibleStart() == 0., "fit failed to restore whole video");
        timeline.setZoom(1.00000001);
        check(scroll->pageStep() > 0, "near-unity zoom overflowed scrollbar");
        timeline.setRange(5., 0., 5., 0.);
        check(timeline.zoomFactor() == 1., "new duration kept stale zoom");
        const QString captures = qEnvironmentVariable("FRAMESCALE_UI_CAPTURES");
        if (!captures.isEmpty()) {
            QDir().mkpath(captures);
            timeline.setRange(40., 19., 21., 20.);
            timeline.setZoom(10., 500.);
            timeline.show();
            app.processEvents();
            timeline.grab().save(captures + "/timeline-zoom.png");
        }

    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
        fixture.path());
    if (app.arguments().contains("--processing-regressions")) {
        // Generate a short 24 FPS fixture for real CPU copy processing. An
        // explicit fixture can be supplied to reproduce a particular input.
        QString source = qEnvironmentVariable("FRAMESCALE_UI_VIDEO");
        if (source.isEmpty()) {
            source = fixture.filePath("clipboard-and-orientation.mp4");
            QProcess generate;
            generate.start(framescale::RuntimePaths().tool("ffmpeg"), {
                "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc2=size=96x64:rate=24:duration=2",
                "-f", "lavfi", "-i", "sine=frequency=440:duration=2", "-c:v", "libx264",
                "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest", source });
            check(generate.waitForFinished(15000) && generate.exitCode() == 0, "UI regression video generation failed");
        }
        check(QFileInfo::exists(source), "UI regression video fixture missing");
        if (failures) return 1;
        {
            MainWindow owner;
            owner.openInputFile(source);
            owner.findChild<QCheckBox*>("enableUpscale")->setChecked(false);
            auto* preview = owner.findChild<MediaPreviewWidget*>("mediaPreview");
            preview->copyRequested();
            check(!owner.findChildren<framescale::ProcessingJob*>().isEmpty(), "clipboard job did not start");
            owner.findChild<QPushButton*>("addToQueueButton")->click();
            auto* queue = owner.findChild<ExportDialog*>();
            auto* start = queue->findChild<QPushButton*>("exportStartButton");
            auto* output = queue->findChild<QLineEdit*>("exportOutputPath");
            auto* rows = queue->findChild<QTableWidget*>("exportQueue");
            check(!start->isEnabled() && output->isEnabled(), "copy did not block dispatch while preserving pending edits");
            const QString destination = fixture.filePath("after-copy.mp4");
            output->setText(destination);
            start->click();
            check(rows->item(0, 2)->text() == "Na fila", "copy created a phantom running export");
            check(waitUntil([&] { return owner.findChildren<framescale::ProcessingJob*>().isEmpty(); }, 30000),
                "clipboard job did not release its worker");
            check(start->isEnabled(), "clipboard completion left export locked");
            start->click();
            check(waitUntil([&] { const auto state = rows->item(0, 2)->text();
                return state == "Concluído" || state == "Falhou"; }, 30000), "export after clipboard timed out");
            check(rows->item(0, 2)->text() == "Concluído" && QFileInfo::exists(destination),
                "export after clipboard did not finish");
            check(waitUntil([&] { return owner.findChildren<framescale::ProcessingJob*>().isEmpty(); }),
                "export did not release its worker");
            output->setText(fixture.filePath("after-cancelled-copy.mp4"));
            preview->copyRequested();
            check(!start->isEnabled(), "copy did not suspend an existing queue");
            const auto jobs = owner.findChildren<framescale::ProcessingJob*>();
            check(jobs.size() == 1, "second clipboard job missing");
            if (!jobs.isEmpty()) jobs.first()->cancel();
            check(waitUntil([&] { return owner.findChildren<framescale::ProcessingJob*>().isEmpty(); }),
                "cancelled clipboard job did not release its worker");
            check(start->isEnabled() && rows->item(0, 2)->text() == "Na fila",
                "clipboard cancellation locked or changed the pending export");
            const auto copied = QApplication::clipboard()->mimeData()->urls();
            QApplication::clipboard()->clear();
            for (const auto& url : copied)
                if (url.isLocalFile()) QFile::remove(url.toLocalFile());
        }
        framescale::ProcessingOptions options;
        options.inputPath = std::filesystem::u8path(source.toUtf8().constData());
        options.mediaType = framescale::MediaType::Video;
        options.operation = framescale::Operation::Copy;
        auto compare = [&](const framescale::VideoOrientation& orientation, bool expectHit) {
            options.orientation = orientation;
            ComparisonDialog comparison(options, 0, .5, 24);
            check(waitUntil([&] { return comparison.property("comparisonReady").toBool()
                || comparison.property("comparisonFailed").toBool(); }, 30000), "comparison timed out");
            check(comparison.property("comparisonReady").toBool(), "comparison processing failed");
            if (expectHit) check(comparison.property("comparisonCacheHit").toBool(), "unchanged comparison missed cache");
            else check(!comparison.property("comparisonCacheHit").toBool(), "changed orientation reused stale comparison");
            return comparison.findChild<MediaPreviewWidget*>("comparisonProcessed")->mediaPath();
        };
        QString identity = compare({}, false);
        for (int variant = 0; variant < 3; ++variant) {
            framescale::VideoOrientation orientation;
            if (variant == 0) orientation.rotate(1);
            else if (variant == 1) orientation.horizontalFlip = true;
            else orientation.verticalFlip = true;
            const QString transformed = compare(orientation, false);
            check(!transformed.isEmpty() && transformed != identity, "orientation reused untransformed output");
            check(compare(orientation, true) == transformed, "same orientation did not reuse its own output");
            // The bounded cache may evict identity after another transform;
            // reset must still select or regenerate the untransformed result.
            options.orientation = {};
            ComparisonDialog reset(options, 0, .5, 24);
            check(waitUntil([&] { return reset.property("comparisonReady").toBool()
                || reset.property("comparisonFailed").toBool(); }, 30000), "orientation reset timed out");
            check(reset.property("comparisonReady").toBool(), "orientation reset failed");
            identity = reset.findChild<MediaPreviewWidget*>("comparisonProcessed")->mediaPath();
            check(!identity.isEmpty() && identity != transformed, "orientation reset retained transformed output");
        }
        if (!failures) std::cout << "UI processing regressions: clipboard queue, rotation, both flips and reset OK\n";
        return failures ? 1 : 0;
    }
    auto checkOrientation = [&] {
        MediaPreviewWidget oriented;
        Appearance::apply(&oriented);
        oriented.resize(480,320);
        oriented.show();
        QImage corners(3, 2, QImage::Format_RGB32);
        corners.fill(Qt::black);
        corners.setPixelColor(0, 0, Qt::red);
        corners.setPixelColor(2, 0, Qt::green);
        corners.setPixelColor(0, 1, Qt::blue);
        const auto media = fixture.filePath("orientation.png");
        check(corners.save(media), "orientation fixture failed");
        oriented.setMedia(media);
        oriented.presentPairedFrame(corners);
        framescale::VideoOrientation transform;
        transform.rotate(1);
        oriented.setOrientation(transform);
        auto rotated = oriented.currentFrame();
        check(rotated.size() == QSize(2,3) && rotated.pixelColor(1,0) == QColor(Qt::red)
            && rotated.pixelColor(0,0) == QColor(Qt::blue), "clockwise preview pixels incorrect");
        transform.horizontalFlip = true;
        oriented.setOrientation(transform);
        check(oriented.currentFrame().pixelColor(0,0) == QColor(Qt::red), "horizontal flip must use display axes");
        transform.rotate(-1);
        oriented.setOrientation(transform);
        check(oriented.currentFrame().pixelColor(0,1) == QColor(Qt::red), "rotation after mirror composition incorrect");
        oriented.setOrientation({});
        check(oriented.currentFrame() == corners, "orientation reset must preserve original pixels");
        QTimer::singleShot(80, &oriented, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            check(menu && menu->property("orientationMenu").toBool(), "orientation popup missing");
            if (!menu) return;
            check(menu->findChildren<QToolButton*>().size() == 4, "four orientation icons missing");
            const auto captures = qEnvironmentVariable("FRAMESCALE_UI_CAPTURES");
            if (!captures.isEmpty()) {
                QDir().mkpath(captures);
                menu->grab().save(captures + "/rotation-menu.png");
                const auto region = menu->frameGeometry();
                menu->screen()->grabWindow(0, region.x(), region.y(), region.width(), region.height())
                    .save(captures + "/rotation-screen.png");
            }
            auto* right = menu->findChild<QAction*>("orientationAction1");
            check(right != nullptr, "rotate right action missing");
            if (right) right->trigger();
            else menu->close();
        });
        oriented.showOrientationMenu(oriented.mapToGlobal(QPoint(40,40)));
        check(oriented.orientation().quarterTurns == 1, "rotation button did not apply");
        MainWindow projects;
        projects.openInputFile(media);
        auto* preview = projects.findChild<MediaPreviewWidget*>();
        auto* tabs = projects.findChild<QTabBar*>("projectTabs");
        preview->setOrientation(oriented.orientation());
        const auto second = fixture.filePath("orientation-second.png");
        corners.save(second);
        QTimer::singleShot(0, &projects, [&] {
            auto* prompt = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(prompt && prompt->objectName() == "openProjectDialog", "new project prompt missing");
            if (prompt) prompt->accept(); // Default choice opens a new tab.
        });
        projects.openInputFile(second);
        check(tabs->count() == 2 && preview->orientation().isIdentity(), "new project inherited orientation");
        tabs->setCurrentIndex(0);
        check(preview->orientation().quarterTurns == 1, "project rotation lost on tab switch");
        tabs->setCurrentIndex(1);
        check(preview->orientation().isIdentity(), "orientation leaked between projects");
    };
    if (app.arguments().contains("--orientation-only")) {
        checkOrientation();
        if (!failures) std::cout << "Orientation UI: pixel transforms, four buttons and project isolation OK\n";
        return failures ? 1 : 0;
    }
    // The dedicated FrameScaleOrientationUi CTest runs the popup/project check
    // in its own process, without native activation events from other windows.
    TrimTimeline frameTimeline;
    frameTimeline.setFrameRate(24);
    frameTimeline.setRange(5, 0, 1, 1);
    check(frameTimeline.selectionFrameCount() == 24, "one second at 24 FPS must contain 24 frames");
    check(frameTimeline.timecode(1) == "00:01:00" && frameTimeline.timecode(.5) == "00:00:12"
        && frameTimeline.timecode(61.5) == "01:01:12" && frameTimeline.timecode(3661.5) == "01:01:01:12",
        "compact timeline timecode mislabels hours, minutes, seconds or frames");
    frameTimeline.setRange(5, 2, 3, 2.5);
    check(frameTimeline.selectionFrameCount() == 24, "offset one-second trim frame count incorrect");
    const QString path = fixture.filePath("Preview sample.png");
    QImage image(960, 540, QImage::Format_RGB32);
    QPainter paint(&image);
    QLinearGradient gradient(0, 0, 960, 540);
    gradient.setColorAt(0, QColor("#194667"));
    gradient.setColorAt(.5, QColor("#6a8ea2"));
    gradient.setColorAt(1, QColor("#efb580"));
    paint.fillRect(image.rect(), gradient);
    paint.setPen(Qt::NoPen);
    paint.setBrush(QColor("#203d47"));
    paint.drawPolygon(QPolygon { QPoint(0, 480), QPoint(240, 180), QPoint(550, 540),
        QPoint(0, 540) });
    paint.setBrush(QColor("#52676a"));
    paint.drawPolygon(QPolygon { QPoint(330, 540), QPoint(660, 170),
        QPoint(960, 510), QPoint(960, 540) });
    paint.end();
    check(image.save(path), "could not create image fixture");
    QSettings().setValue("encoderPresets", QVariantMap { { "Legacy preset", QVariantMap { { "codec", "libx264" }, { "crf", 21 } } } });
    const QImage icon = QIcon(":/brand/framescale.ico").pixmap(32, 32).toImage();
    check(!icon.isNull() && icon.size() == QSize(32, 32)
        && icon.pixelColor(16, 16) != icon.pixelColor(0, 0), "application icon is missing or blank");
    QWidget desktopFixture;
    const bool captureDesktop = qEnvironmentVariableIsSet("FRAMESCALE_GLASS_DESKTOP");
    if (captureDesktop) {
        desktopFixture.setProperty("appearanceManaged", true);
        desktopFixture.setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        desktopFixture.setStyleSheet("background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #cfbbaa,stop:0.4 #c6d2cf,stop:0.7 #a3aecb,stop:1 #b6a8bb);");
        desktopFixture.setGeometry(QApplication::primaryScreen()->geometry());
        desktopFixture.show();
    }
    MainWindow window;
    if (captureDesktop) window.setWindowFlag(Qt::WindowStaysOnTopHint);
    check(window.findChild<QComboBox*>("encoderPresets")->findText("Legacy preset") > 0, "legacy presets were not migrated");
    window.show();
    check(window.isMaximized(), "window must open maximized");
    // Stable screenshot dimensions independent of CI monitor.
    window.showNormal();
    window.resize(1440, 900);
    if (captureDesktop) window.move(100,70);
    check(window.mask().isEmpty() && window.graphicsEffect(),"window must use smooth alpha corners rather than a polygon mask");
    QCoreApplication::processEvents();
    const QString captures = qEnvironmentVariable("FRAMESCALE_UI_CAPTURES");
    if (!captures.isEmpty())
        QDir().mkpath(captures);
    auto capture = [&](QWidget* widget, const QString& name) {
        QEventLoop reveal;
        QTimer::singleShot(180, &reveal, &QEventLoop::quit);
        reveal.exec();
        QCoreApplication::processEvents();
        if (!captures.isEmpty())
            check(widget->grab().save(QDir(captures).filePath(name)),
                "could not save screenshot");
        if (captureDesktop && !captures.isEmpty()) {
            widget->raise();
            QEventLoop settle;
            QTimer::singleShot(100, &settle, &QEventLoop::quit);
            settle.exec();
            const QRect region(widget->mapToGlobal(QPoint()),widget->size());
            check(widget->screen()->grabWindow(0,region.x(),region.y(),region.width(),region.height()).save(QDir(captures).filePath("desktop-"+name)),"could not capture compositor");
        }
    };
    auto* pages = window.findChild<QStackedWidget*>("mainPages");
    auto* menus = window.findChild<QMenuBar*>("mainMenu");
    auto* tabs = window.findChild<QTabBar*>("projectTabs");
    auto* preview = window.findChild<MediaPreviewWidget*>("mediaPreview");
    check(pages && menus && tabs && preview,
        "required interface widgets missing");
    if (!pages || !menus || !tabs || !preview)
        return 1;
    for (auto* action : menus->actions())
        for (auto* command : action->menu()->actions())
            check(!command->text().contains(QChar(0x2026)), "command menu still contains ellipses");
    check(menus->actions().size() == 2 && menus->actions()[0]->text() == QString::fromUtf8("Arquivo") && menus->actions()[1]->text() == QString::fromUtf8("Configurações"),
        "incorrect main menus");
    check(!window.windowIcon().isNull() && !window.findChild<QLabel*>("titleLogo")->pixmap().isNull(),
        "brand icons missing");
    check(pages->currentWidget()->objectName() == "editorPage" && window.findChild<QDialog*>("welcomeDialog")->isVisible(),
        "welcome page not shown");
    check(!window.findChild<QWidget*>("recentPanel") && !window.findChild<QWidget*>("versionLabel"),
        "removed content remains");
    auto* welcomeNew = window.findChild<QPushButton*>("welcomeNewButton");
    check(welcomeNew->width() >= welcomeNew->fontMetrics().horizontalAdvance(welcomeNew->text()) + 28, "welcome action is clipped on first show");
    check(window.findChild<QLabel*>("emptyDocumentTitle")->isVisible(), "empty document title is missing behind welcome");
    check(!window.findChild<QWidget*>("previewViewHeader"), "empty composition header remains");
    capture(&window, "empty-editor.png");
    check(window.findChild<QPushButton*>("welcomeHomeButton")->isChecked(), "Welcome did not start in Home");
    capture(window.findChild<QDialog*>("welcomeDialog"), "welcome.png");
    check(window.findChild<QLabel*>("welcomeTitle")->font().family()==Typography::displayFamily,"welcome title is not using Display font");
    window.findChild<QDialog*>("welcomeDialog")->close();
    auto* emptyChoose = window.findChild<QPushButton*>("emptyChooseMediaButton");
    check(emptyChoose && emptyChoose->isVisible() && emptyChoose->isEnabled(),
        "empty editor file selection is unavailable after closing welcome");
    bool emptyOpenedPicker = false;
    QTimer::singleShot(0, &window, [&] {
        auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        emptyOpenedPicker = picker != nullptr;
        if (picker) picker->reject();
    });
    if (emptyChoose) emptyChoose->click();
    QCoreApplication::processEvents();
    check(emptyOpenedPicker, "empty editor button did not open the file picker");
    check(emptyChoose && emptyChoose->isEnabled(), "cancelling the picker disabled file selection");
    capture(&window, "empty-file-button.png");
    window.openInputFile(path);
    check(waitUntil([&] {
        return !window.findChild<QLabel*>("previewImage")->pixmap().isNull();
    }),
        "image preview failed");
    check(pages->currentWidget()->objectName() == "editorPage",
        "editor did not open");
    check(tabs->tabText(tabs->currentIndex()) == QFileInfo(path).fileName(),
        "filename not used as document title");
    check(!window.findChild<QCheckBox*>("enableInterpolation")->isEnabled(),
        "interpolation must be disabled for images");
    check(!window.findChild<QWidget*>("interpolationFields")->isVisible(),
        "upscale shows interpolation fields");
    auto* initialUpscale = window.findChild<QCheckBox*>("enableUpscale");
    check(initialUpscale && !initialUpscale->isChecked(), "new project unexpectedly enables processing");
    if (initialUpscale) initialUpscale->setChecked(true);
    QCoreApplication::processEvents();
    auto* zoomPercent = window.findChild<QComboBox*>("previewZoom");
    check(QRegularExpression("^[0-9]+%$").match(zoomPercent->currentText()).hasMatch(), "fit zoom does not display its current percentage");
    check(!window.findChild<QLabel*>("emptyDocumentTitle")->isVisible(), "empty title was not replaced");
    check(GlassMaterial::reduced() || !window.property("glassBackdrop").value<QImage>().isNull(), "panel material has no source image");
    capture(&window, "editor-image.png");
    check(tabs->height() <= 28, "project title bar is oversized");
    auto* compactPreview = window.findChild<QPushButton*>("comparePreviewButton");
    check(compactPreview && compactPreview->height() <= 32
        && compactPreview->width() >= compactPreview->fontMetrics().horizontalAdvance(compactPreview->text()) + 12
        && compactPreview->width() <= 120,
        "Preview button is clipped or oversized");
#ifdef Q_OS_WIN
    if (window.property("nativeGlassActive").toBool()) {
        HRGN region = CreateRectRgn(0,0,0,0);
        check(GetWindowRgn(reinterpret_cast<HWND>(window.winId()), region) != ERROR
            && !PtInRegion(region, 0, 0), "native blur extends outside rounded corners");
        DeleteObject(region);
    }
#endif
    check(window.findChild<QPushButton*>("windowCloseButton")->toolTip().isEmpty(),"window close tooltip remains");
    check(dynamic_cast<SwitchControl*>(window.findChild<QCheckBox*>("encoderKeepMetadata")),"metadata control is not the shared switch");
    check(!window.findChild<QTabWidget*>("inspectorTabs")->tabBar()->drawBase(),"inspector tab base line remains");
    if (window.property("nativeGlassActive").toBool()) {
        auto* engine=window.findChild<QComboBox*>("upscaleEngine");
        const QPoint sample(window.width()-80,window.height()-180);
        const auto alphaBefore=window.grab().toImage().pixelColor(sample).alpha();
        for(int i=0;i<16;++i) { if(engine) engine->repaint(); QCoreApplication::processEvents(); }
        check(window.grab().toImage().pixelColor(sample).alpha()==alphaBefore,"partial repaint accumulates glass opacity");
    }
    if (window.property("nativeGlassActive").toBool()) {
        const auto frame=window.grab().toImage();
        const auto panelPixel=frame.pixelColor(frame.width()-100,frame.height()-150);
        // InspectorSurface composites a 94%-opaque cached backdrop and a tint.
        // Its resulting alpha is near opaque; it is not the native window tint.
        check(panelPixel.alpha()>=240,"glass inspector is too transparent to read");
        check(frame.pixelColor(frame.width()/3,frame.height()/2).alpha()==255,"media must remain opaque");
    }
    const auto speaker=window.findChild<QToolButton*>("previewVolumeButton")->icon().pixmap(24,24).toImage();
    check(speaker.pixelColor(8,12).lightness()<128,"speaker icon has insufficient contrast in light appearance");
    for (int i=0;i<menus->actions().size();++i) {
        auto* commandMenu=menus->actions()[i]->menu();
        commandMenu->popup(menus->mapToGlobal(menus->actionGeometry(menus->actions()[i]).bottomLeft()));
        QCoreApplication::processEvents();
        check(commandMenu->windowFlags().testFlag(Qt::FramelessWindowHint),"command menu lacks rounded glass surface");
        capture(commandMenu, i==0?"file-menu.png":"settings-menu.png");
        commandMenu->hide();
    }
    {
        auto* engines = window.findChild<QComboBox*>("upscaleEngine");
        check(!Typography::textFamily.isEmpty() && !Typography::displayFamily.isEmpty(), "system font selection failed");
        check(engines->font().family()==Typography::textFamily, "control font was overridden by legacy stylesheet");
        const int before = engines->currentIndex();
        // Studio deliberately uses Qt's normal popup; retain separate coverage
        // of the custom rounded picker used by both macOS-style themes below.
        QSettings().setValue("appearance", "studio");
        Appearance::refresh();
        engines->showPopup();
        QCoreApplication::processEvents();
        check(engines->view()->isVisible() && engines->view()->model()->rowCount() == engines->count(),
            "Studio selection popup did not expose all choices");
        QKeyEvent studioDown(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        QKeyEvent studioEnter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(engines->view(), &studioDown);
        QApplication::sendEvent(engines->view(), &studioEnter);
        check(engines->currentIndex() == (before + 1) % engines->count(), "Studio popup keyboard selection did not apply");
        engines->hidePopup();
        engines->setCurrentIndex(before);
        engines->showPopup();
        QKeyEvent studioEscape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(engines->view(), &studioEscape);
        engines->hidePopup();
        check(engines->currentIndex() == before && !engines->view()->isVisible(), "Studio popup cancellation changed selection or stayed open");
        QSettings().setValue("appearance", "macos-light");
        Appearance::refresh();
        engines->showPopup();
        QCoreApplication::processEvents();
        auto* menu = engines->findChild<QMenu*>("selectionPopup");
        check(menu && menu->isVisible(), "rounded selection popup missing");
        if (menu) {
            check(menu->font().family()==Typography::textFamily,"popup font differs from controls");
            check(menu->actions()[before]->isChecked(), "selected item checkmark missing");
            menu->setActiveAction(nullptr);
            QCoreApplication::processEvents();
            check(menu->windowFlags().testFlag(Qt::FramelessWindowHint), "popup alpha composition requires frameless Windows flag");
            capture(menu, "macos-selection-popup.png");
            if (qEnvironmentVariableIsSet("FRAMESCALE_UI_CAPTURES")) {
                QEventLoop settle; QTimer::singleShot(150,&settle,&QEventLoop::quit); settle.exec();
                const QRect region = menu->frameGeometry().adjusted(-12,-12,12,12);
                menu->screen()->grabWindow(0,region.x(),region.y(),region.width(),region.height()).save(qEnvironmentVariable("FRAMESCALE_UI_CAPTURES")+"/macos-selection-desktop.png");
            }
            menu->setActiveAction(menu->actions()[before]);
            QKeyEvent down(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);
            QApplication::sendEvent(menu,&down);
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
            QApplication::sendEvent(menu,&enter);
            check(engines->currentIndex() == (before+1)%engines->count(), "popup keyboard selection did not apply");
            engines->setCurrentIndex(before);
            engines->showPopup();
            auto* cancelled = engines->findChild<QMenu*>("selectionPopup");
            if (cancelled) { QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier); QApplication::sendEvent(cancelled,&escape); }
            engines->hidePopup();
            check(engines->currentIndex()==before, "cancelling picker changed value");
#ifdef Q_OS_WIN
            if (qEnvironmentVariableIsSet("FRAMESCALE_NATIVE_PICKER")) {
                window.raise(); window.activateWindow();
                SetForegroundWindow(reinterpret_cast<HWND>(window.winId()));
                QEventLoop activate; QTimer::singleShot(150,&activate,&QEventLoop::quit); activate.exec();
                POINT previousCursor; GetCursorPos(&previousCursor);
                auto click = [&](QPoint global) {
                    const QPoint local=window.mapFromGlobal(global);
                    POINT physical{qRound(local.x()*window.devicePixelRatioF()),qRound(local.y()*window.devicePixelRatioF())};
                    ClientToScreen(reinterpret_cast<HWND>(window.winId()),&physical);
                    SetCursorPos(physical.x,physical.y);
                    INPUT events[2]{}; events[0].type=events[1].type=INPUT_MOUSE;
                    events[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN; events[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
                    check(SendInput(2,events,sizeof(INPUT)) == 2,"Windows did not accept test mouse input");
                    QEventLoop settle;QTimer::singleShot(80,&settle,&QEventLoop::quit);settle.exec();
                };
                for (int repeat=0;repeat<8;++repeat) {
                    click(engines->mapToGlobal(engines->rect().center()));
                    auto* active = engines->findChild<QMenu*>("selectionPopup");
                    check(active && active->isVisible(),"native mouse did not open picker");
                    if (!active) {
                        std::cerr << "picker failed cycle " << repeat << " dpr " << window.devicePixelRatioF() << '\n';
                        capture(&window,"native-picker-failure.png");
                        std::cerr << "foreground " << (GetForegroundWindow()==reinterpret_cast<HWND>(window.winId())) << " enabled " << engines->isEnabled() << " global " << engines->mapToGlobal(engines->rect().center()).x() << "," << engines->mapToGlobal(engines->rect().center()).y() << '\n';
                        break;
                    }
                    check(GlassMaterial::reduced() || active->property("frostedBackdropReady").toBool(),"picker blur has no backing content");
                    // Dismiss in the exposed top edge of the same trigger; this
                    // used to replay into QComboBox and immediately reopen.
                    click(engines->mapToGlobal(QPoint(engines->width()/2,1)));
                    auto* reopened = engines->findChild<QMenu*>("selectionPopup");
                    check(!reopened || !reopened->isVisible(),"dismissed picker reopened from mouse replay");
                    engines->hidePopup();
                }
                check(engines->currentIndex()==before,"repeat clicking changed selection");
                click(engines->mapToGlobal(engines->rect().center()));
                if (auto* active = engines->findChild<QMenu*>("selectionPopup")) {
                    const int next=(before+1)%engines->count();
                    click(active->mapToGlobal(active->actionGeometry(active->actions()[next]).center()));
                    check(engines->currentIndex()==next,"native mouse selection did not apply");
                } else check(false,"picker did not reopen for mouse selection");
                engines->setCurrentIndex(before);
                click(menus->mapToGlobal(menus->actionGeometry(menus->actions()[0]).center()));
                check(menus->actions()[0]->menu()->isVisible(),"native click did not open File menu");
                // With a menu open, moving across the bar switches menus;
                // clicking the newly opened heading again would close it.
                const QPoint settingsPoint=menus->mapTo(&window,menus->actionGeometry(menus->actions()[1]).center());
                POINT settingsPhysical{qRound(settingsPoint.x()*window.devicePixelRatioF()),qRound(settingsPoint.y()*window.devicePixelRatioF())};
                ClientToScreen(reinterpret_cast<HWND>(window.winId()),&settingsPhysical);
                SetCursorPos(settingsPhysical.x,settingsPhysical.y);
                check(waitUntil([&]{return menus->actions()[1]->menu()->isVisible();},1000),"moving across menu bar did not open Settings");
                menus->actions()[1]->menu()->hide();
                QCoreApplication::processEvents();
                click(menus->mapToGlobal(menus->actionGeometry(menus->actions()[1]).center()));
                check(menus->actions()[1]->menu()->isVisible(),"native click did not open Settings menu");
                menus->actions()[1]->menu()->hide();
                SetCursorPos(previousCursor.x,previousCursor.y);
            }
#endif

        }
        QSettings().setValue("appearance", "studio");
        Appearance::refresh();
    }

    {
        auto* canvas = preview->findChild<QLabel*>("previewImage");
        check(canvas->cursor().shape() == Qt::ArrowCursor, "preview default cursor is not normal");
        canvas->setFocus();
        QKeyEvent overrideSpace(QEvent::ShortcutOverride, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(canvas, &overrideSpace);
        check(overrideSpace.isAccepted(), "space panning did not reserve shortcut");
        QKeyEvent spaceDown(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier), spaceUp(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(canvas, &spaceDown);
        check(canvas->cursor().shape() == Qt::OpenHandCursor, "holding space did not activate hand");
        const QPointF from(100, 100), to(155, 135);
        QMouseEvent down(QEvent::MouseButtonPress, from, canvas->mapToGlobal(from.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent move(QEvent::MouseMove, to, canvas->mapToGlobal(to.toPoint()), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent up(QEvent::MouseButtonRelease, to, canvas->mapToGlobal(to.toPoint()), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &down);
        QApplication::sendEvent(canvas, &move);
        QApplication::sendEvent(canvas, &up);
        check(preview->property("viewPan").toPointF() == QPointF(55, 35), "space drag cannot move fit view freely");
        const auto originalPan = preview->property("viewPan").toPointF();
        const QPointF anchor(120,90), center(canvas->width()/2.,canvas->height()/2.);
        const double scaleBefore=preview->property("zoomPercent").toDouble();
        QWheelEvent anchoredWheel(anchor,canvas->mapToGlobal(anchor),QPoint(),QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(canvas,&anchoredWheel);
        const double ratio=preview->property("zoomPercent").toDouble()/scaleBefore;
        const auto expectedPan=anchor-center-(anchor-center-originalPan)*ratio;
        check(QLineF(expectedPan,preview->property("viewPan").toPointF()).length()<.01,"wheel zoom moved the image point under the cursor");
        QApplication::sendEvent(canvas, &spaceUp);
        check(canvas->cursor().shape() == Qt::ArrowCursor, "space release did not restore arrow");
        const auto pan = preview->property("viewPan").toPointF();
        QApplication::sendEvent(canvas, &down);
        QApplication::sendEvent(canvas, &move);
        QApplication::sendEvent(canvas, &up);
        check(preview->property("viewPan").toPointF() == pan, "preview pans without space");
        QMouseEvent reset(QEvent::MouseButtonDblClick, from, canvas->mapToGlobal(from.toPoint()), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &reset);
        check(preview->property("viewPan").toPointF().isNull(), "fit did not recenter a panned fit view");
        const double before = preview->property("zoomPercent").toDouble();
        QWheelEvent wheel(from, canvas->mapToGlobal(from.toPoint()), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        for (int i = 0; i < 22; ++i)
            QApplication::sendEvent(canvas, &wheel);
        check(preview->property("zoomPercent").toDouble() < before * .02, "zoom out stuck at a preset minimum");
        zoomPercent->setCurrentIndex(0);
    }
    {
        TrimTimeline track;
        track.resize(1000, 150);
        track.setFrameRate(25);
        track.setRange(10, 0, 10, 5);
        double cut = -1;
        track.rangeEdited = [&](double start, double) { cut = start; };
        const QPointF from(18, 75), to(18 + 964 * .5 + 7, 75);
        auto drag = [&](Qt::KeyboardModifiers modifiers) {
            QMouseEvent down(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, modifiers);
            QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, modifiers);
            QMouseEvent up(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, modifiers);
            QApplication::sendEvent(&track, &down);
            QApplication::sendEvent(&track, &move);
            QApplication::sendEvent(&track, &up);
        };
        drag(Qt::NoModifier);
        check(cut > 5.04, "timeline always snaps to playhead");
        track.setRange(10, 0, 10, 5);
        drag(Qt::ShiftModifier);
        check(qAbs(cut - 5) < .001, "Shift did not snap trim to playhead");
    }

    check(!window.findChild<QWidget*>("appHeader"), "secondary header remains");
    auto* scale = dynamic_cast<ScaleControl*>(window.findChild<QComboBox*>("upscaleFactor"));
    check(scale && scale->count()==19 && scale->itemData(0).toDouble()==1 && scale->itemData(18).toDouble()==10,
        "scale dropdown must offer 1..10 in half steps");
    scale->showPopup();
    check(scale->view()->isVisible() && scale->view()->model()->rowCount() == 19, "scale dropdown did not open all choices");
    scale->view()->setCurrentIndex(scale->model()->index(1, 0));
    QKeyEvent selectScale(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(scale->view(), &selectScale);
    scale->hidePopup();
    check(scale->value()==1.5, "scale choice did not apply");
    scale->setValue(2.25);
    check(scale->value()==2.25, "existing fractional scale was not preserved");
    scale->setValue(10);
    auto* enhancement = window.findChild<QWidget*>("enhancementPanel");
    auto* enableEnhancement = enhancement->findChild<QCheckBox*>("enableEnhancement");
    QCoreApplication::processEvents();
    const QPoint enhancementTogglePosition=enableEnhancement->mapTo(window.findChild<QWidget*>("inspectorContent"),QPoint());
    const int inspectorWidth=window.findChild<QWidget*>("inspectorPanel")->width();
    const auto* inspector=window.findChild<QWidget*>("inspectorPanel");
    check(qAbs(inspector->mapTo(&window,inspector->rect().topRight()).x()-(window.width()-1))<4,"inspector leaves unused space at window edge");
    enableEnhancement->setChecked(true);
    QCoreApplication::processEvents();
    check(window.findChild<QWidget*>("inspectorPanel")->width()==inspectorWidth,"enhancement changes inspector width");
    const QPoint afterEnhancement=enableEnhancement->mapTo(window.findChild<QWidget*>("inspectorContent"),QPoint());
    check(afterEnhancement==enhancementTogglePosition,"enhancement shifts its toggle");
    if (afterEnhancement!=enhancementTogglePosition)
        std::cerr << "toggle position " << enhancementTogglePosition.x() << "," << enhancementTogglePosition.y() << " -> " << afterEnhancement.x() << "," << afterEnhancement.y() << '\n';
    check(!enhancement->findChild<QComboBox*>("enhancementProfile"), "built-in enhancement presets remain");
    check(!enhancement->findChild<QPushButton*>("enhancementCompare"), "extra compare button remains");
    auto* intensity = enhancement->findChild<QSpinBox*>("enhanceDenoise");
    auto* input = intensity->findChild<QLineEdit*>();
    input->setText("100"); intensity->interpretText();
    check(intensity->value()==100 && input->text()=="100" && input->contentsRect().width() >= input->fontMetrics().horizontalAdvance("100"), "intensity 100 is truncated");
    QWheelEvent unwanted(QPointF(10,10),QPointF(10,10),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QApplication::sendEvent(intensity,&unwanted);
    check(intensity->value()==100, "wheel changes intensity");
    check(!enhancement->findChild<QComboBox*>("enhanceDeinterlace"), "removed deinterlace control still present");
    enhancement->findChild<QSpinBox*>("enhanceDenoise")->setValue(40);
    const QString second = fixture.filePath("Second.png");
    check(image.save(second), "second image fixture failed");
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        check(message && message->objectName() == "openProjectDialog", "opening another project did not ask");
        if (message) message->accept();
    });
    window.openInputFile(second);
    check(tabs->count() == 2, "new file did not create a project tab");
    scale->setValue(1);
    tabs->setCurrentIndex(0);
    check(scale->value() == 10, "project options were lost");
    check(enableEnhancement->isChecked() && enhancement->findChild<QSpinBox*>("enhanceDenoise")->value() == 40, "enhancement project options lost");
    tabs->setCurrentIndex(1);
    check(scale->value() == 1, "second project options were lost");
    tabs->setCurrentIndex(0);
    scale->setValue(2);
    check(waitUntil([&] { return !preview->currentFrame().isNull(); }), "enhancement project preview missing");
    for (auto* area : window.findChildren<QScrollArea*>())
        if (area->widget() && area->widget()->isAncestorOf(enhancement))
            area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
    enhancement->findChild<QToolButton*>("enhancementModel1")->click();
    capture(&window, "enhancement-controls.png");
    enhancement->findChild<QToolButton*>("enhancementModel0")->click();
    for (auto* area : window.findChildren<QScrollArea*>())
        if (area->widget() && area->widget()->isAncestorOf(enhancement))
            area->verticalScrollBar()->setValue(0);
    enableEnhancement->setChecked(false);
    const QString third = fixture.filePath("Third.png");
    check(image.save(third), "third fixture failed");
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        check(message && message->objectName() == "openProjectDialog", "new project cancellation prompt missing");
        if (message)
            message->reject();
    });
    window.openInputFile(third);
    check(tabs->count() == 2 && tabs->currentIndex() == 0,
        "cancel changed the project");
    bool repeatPrompt = false;
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        repeatPrompt = message && message->objectName() == "openProjectDialog";
        if (message) {
            bool selected = false;
            for (auto* button : message->findChildren<QPushButton*>())
                if (button->text() == QString::fromUtf8("Substituir projeto atual"))
                    { button->click(); selected = true; break; }
            check(selected, "replace project action missing");
            if (!selected) message->reject();
        }
    });
    window.openInputFile(path);
    check(repeatPrompt && tabs->count() == 2,
        "same input did not offer replacement");
    auto* upscale = window.findChild<QCheckBox*>("enableUpscale");
    check(upscale && upscale->isChecked(), "replacing input lost current processing options");
    upscale->setChecked(true);
    check(window.findChild<QWidget*>("upscaleFields")->isEnabled(),
        "upscale switch did not enable settings");
    upscale->setChecked(false);
    check(!window.findChild<QWidget*>("upscaleFields")->isEnabled(),
        "upscale switch did not disable settings");
    upscale->setChecked(true);
    auto* engine = window.findChild<QComboBox*>("upscaleEngine");
    engine->setCurrentIndex(
        engine->findData(static_cast<int>(framescale::UpscaleEngine::RealCUGAN)));
    auto* model = window.findChild<QComboBox*>("upscaleModel");
    auto* denoise = window.findChild<QComboBox*>("denoiseLevel");
    check(model->currentData().toString() == "realcugan-se",
        "CUGAN default does not support denoising");
    check(denoise->count() == 5, "CUGAN SE denoising levels missing");
    engine->setCurrentIndex(0);
    capture(&window, "editor-tabs.png");
    auto* exportButton = window.findChild<QPushButton*>("addToQueueButton");
    check(exportButton && exportButton->text() == QString::fromUtf8("Adicionar à fila"),
        "export action missing");
    exportButton->click();
    auto* dialog = window.findChild<ExportDialog*>("exportDialog");
    check(dialog && dialog->isVisible(), "Exportar did not open export screen");
    if (dialog) {
        capture(dialog, "export-ready.png");
        // Validate the actual error path without invoking GPU processing in CTest.
        dialog->findChild<QLineEdit*>("exportOutputPath")->setText(path);
        dialog->findChild<QPushButton*>("exportStartButton")->click();
        check(dialog->findChild<QTableWidget*>("exportQueue")->item(0, 2)->text() == "Falhou",
            "input overwrite was not rejected");
        check(dialog->findChild<QPushButton*>("exportStartButton")->isEnabled(),
            "validation failure left export locked");
        dialog->findChild<QAbstractButton*>("exportTrashButton")->click();
        dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    bool newOpenedPicker = false;
    QTimer::singleShot(0, &window, [&] {
        auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
        newOpenedPicker = picker != nullptr;
        if (picker)
            picker->reject();
    });
    window.findChild<QAction*>("newFileAction")->trigger();
    check(newOpenedPicker, "File > New did not open a file picker");
    check(pages->currentWidget()->objectName() == "editorPage",
        "cancelling New discarded the editor");
    const QString video = qEnvironmentVariable("FRAMESCALE_UI_VIDEO");
    if (!video.isEmpty()) {
        QTimer::singleShot(0, &window, [&] {
            auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(message && message->objectName() == "openProjectDialog", "video project prompt missing");
            if (message) message->accept();
        });
        window.openInputFile(video);
        auto* play = window.findChild<QPushButton*>("previewPlayButton");
        check(
            waitUntil([&] {
                return play->isEnabled() && window.findChild<QLabel*>("previewImage")->isVisible() && !window.findChild<QLabel*>("previewImage")->pixmap().isNull();
            }),
            "video first frame failed");
        check(window.findChild<QCheckBox*>("enableInterpolation")->isEnabled(),
            "video interpolation tab disabled");
        check(window.findChild<QMediaPlayer*>("previewPlayer")->playbackState() != QMediaPlayer::PlayingState,
            "video played on load");
        auto* timeline = window.findChild<QWidget*>("trimTimeline");
        check(timeline && timeline->isVisible(), "trim timeline missing");
        if (timeline) {
            const QPointF start(18, 75),
                target(18 + (timeline->width() - 36) * 0.25, 75);
            QMouseEvent press(QEvent::MouseButtonPress, start,
                timeline->mapToGlobal(start.toPoint()), Qt::LeftButton,
                Qt::LeftButton, Qt::NoModifier);
            QMouseEvent drag(QEvent::MouseMove, target,
                timeline->mapToGlobal(target.toPoint()), Qt::NoButton,
                Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, target,
                timeline->mapToGlobal(target.toPoint()),
                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(timeline, &press);
            QApplication::sendEvent(timeline, &drag);
            QApplication::sendEvent(timeline, &release);
            check(preview->trimStartSeconds() > 0.4 && preview->trimStartSeconds() < 0.6,
                "timeline handle did not trim");
        }
        check(!window.findChild<QWidget*>("previewTrimBar")->isVisible(), "numeric trim bar remains visible");
        preview->setTrimRange(0.25, 0.75);
        QTimer::singleShot(0, preview, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            check(menu && menu->property("orientationMenu").toBool(), "rotation popup missing");
            if (!menu) return;
            check(menu->findChildren<QToolButton*>().size() == 4, "rotation popup must have four icon buttons");
            capture(menu, "rotation-menu.png");
            auto* right = menu->findChild<QAction*>("orientationAction1");
            check(right != nullptr, "rotate right action missing");
            if (right) right->trigger();
            else menu->close();
        });
        preview->showOrientationMenu(preview->mapToGlobal(QPoint(40,40)));
        check(preview->orientation().quarterTurns == 1, "rotate right action did not change orientation");
        auto* speedControl=window.findChild<QDoubleSpinBox*>("playbackSpeed");
        check(speedControl && speedControl->minimum()==.25 && speedControl->maximum()==4.,"speed control missing or invalid range");
        speedControl->setValue(.5); check(preview->playbackSpeed()==.5,"speed control did not reach preview");
        const int videoTab = tabs->currentIndex();
        tabs->setCurrentIndex(0);
        check(preview->orientation().isIdentity(), "orientation leaked into another project");
        tabs->setCurrentIndex(videoTab);
        check(preview->orientation().quarterTurns == 1, "orientation lost after tab switch");
        check(speedControl->value()==.5 && preview->playbackSpeed()==.5,"speed did not survive tab switch"); speedControl->setValue(1.);
        preview->setOrientation({});
        check(
            waitUntil([&] {
                return play->isEnabled() && window.findChild<QLabel*>("previewImage")->isVisible() && !window.findChild<QLabel*>("previewImage")->pixmap().isNull();
            }),
            "video did not reload after tab switch");
        check(window.findChild<QMediaPlayer*>("previewPlayer")->playbackState() != QMediaPlayer::PlayingState,
            "video auto-played after tab switch");
        check(preview->trimStartSeconds() == 0.25 && preview->trimEndSeconds() == 0.75,
            "trim selection did not survive tab switch");
        capture(&window, "editor-video.png");
        qint64 furthestPosition = 0;
        const auto positionConnection = QObject::connect(window.findChild<QMediaPlayer*>("previewPlayer"), &QMediaPlayer::positionChanged,
            &window, [&](qint64 position) { furthestPosition = qMax(furthestPosition, position); });
        play->click();
        auto* seek = window.findChild<QSlider*>("previewSeekSlider");
        check(waitUntil(
                  [&] {
                      return furthestPosition > 350;
                  },
                  5000),
            "video playback did not advance");
        QObject::disconnect(positionConnection);
        preview->pause();
        auto* filmstrip = window.findChild<QWidget*>("trimTimeline");
        check(waitUntil([&] { return filmstrip->property("thumbnailCount").toInt() == 16; }), "timeline filmstrip did not load all thumbnails");
        auto* volume = window.findChild<QToolButton*>("previewVolumeButton");
        volume->click();
        auto* volumePopup = window.findChild<QWidget*>("previewVolumePopup");
        check(volumePopup->isVisible(), "volume popup did not open");
        window.findChild<QSlider*>("previewVolumeSlider")->setValue(72);
        check(qAbs(window.findChild<QAudioOutput*>()->volume() - .72) < .01, "volume did not change preview audio");
        capture(volumePopup, "volume-popup.png");
        check(volumePopup->testAttribute(Qt::WA_NoMouseReplay), "volume opener click can replay");
        volume->click();
        check(!volumePopup->isVisible(), "volume button does not close popup");
        const QImage beforeScrub = preview->currentFrame();
        auto point = [&](double fraction) { return QPointF(18 + (filmstrip->width() - 36) * fraction, 20); };
        QMouseEvent press(QEvent::MouseButtonPress, point(.05), point(.05), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(filmstrip, &press);
        int moves = 0;
        bool frameDuringDrag = false;
        QTimer fastDrag;
        fastDrag.setInterval(15);
        QObject::connect(&fastDrag, &QTimer::timeout, &window, [&] {
            const auto p = point(.05 + .7 * std::min(39, moves) / 39.);
            QMouseEvent move(QEvent::MouseMove, p, p, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(filmstrip, &move);
            if (preview->property("scrubFramePosition").toLongLong() > 0)
                frameDuringDrag = true;
            if (++moves >= 40)
                fastDrag.stop();
        });
        fastDrag.start();
        check(waitUntil([&] { return moves >= 40; }, 3000), "fast scrubbing stalled the UI");
        QMouseEvent release(QEvent::MouseButtonRelease, point(.75), point(.75), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(filmstrip, &release);
        check(frameDuringDrag, "frames only changed after releasing the timeline");
        check(waitUntil([&] { return preview->property("scrubFramePosition").toLongLong() == 1500; }, 6000), "last fast seek did not win");
        check(preview->currentFrame() != beforeScrub, "rapid scrubbing left the image frozen");
        preview->pause();
        preview->seek(1250);
        check(waitUntil([&] {return preview->property("scrubFramePosition").toLongLong()==1250;}),"one-frame backward seek did not finish");
        const QImage held = preview->currentFrame();
        preview->seek(750);
        preview->seek(1000);
        bool staleImage = false;
        QTimer watchFrames;
        QObject::connect(&watchFrames,&QTimer::timeout,&window,[&] {
            if(preview->currentFrame()!=held && preview->property("scrubFramePosition").toLongLong()!=1000) staleImage=true;
        });
        watchFrames.start(1);
        check(waitUntil([&] {return preview->property("scrubFramePosition").toLongLong()==1000;}),"latest frame seek did not finish");
        watchFrames.stop();
        check(!staleImage,"superseded seek flashed an intermediate frame");
        capture(&window, "timeline-filmstrip.png");
        window.findChild<QCheckBox*>("enableInterpolation")->setChecked(true);
        check(window.findChild<QWidget*>("interpolationFields")->isVisible(),
            "interpolation fields hidden");
        check(window.findChild<QWidget*>("upscaleFields")->isVisible(),
            "unified upscale controls missing");

        check(window.findChild<QWidget*>("upscaleFields")->isVisible(),
            "combined mode cannot configure upscale");
        window.findChild<QSpinBox*>("targetFps")->setValue(8);
        capture(&window, "editor-combined.png");
        {
            auto* enhancementToggle = window.findChild<QCheckBox*>("enableEnhancement");
            const bool previous = enhancementToggle->isChecked();
            enhancementToggle->setChecked(true);
            window.resize(1000, 680);
            QEventLoop settle;
            QTimer::singleShot(220, &settle, &QEventLoop::quit);
            settle.exec();
            auto* content = window.findChild<QWidget*>("inspectorContent");
            check(content->width() <= content->parentWidget()->width(), "all options expand beyond the inspector viewport");
            for (auto* control : content->findChildren<QWidget*>()) {
                if (!control->isVisibleTo(content)) continue;
                if (!qobject_cast<QComboBox*>(control) && !qobject_cast<QAbstractSpinBox*>(control)
                    && !qobject_cast<QAbstractButton*>(control) && !qobject_cast<QSlider*>(control)) continue;
                const QRect bounds(control->mapTo(content,QPoint()), control->size());
                check(bounds.left() >= 0 && bounds.right() < content->width(), "enabled inspector control is horizontally clipped");
            }
            capture(&window, "all-options-compact.png");
            auto* scroll = qobject_cast<QScrollArea*>(content->parentWidget()->parentWidget());
            check(scroll && scroll->verticalScrollBar()->maximum()>0, "all options cannot scroll vertically");
            if (scroll) { scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum()); capture(&window,"all-options-bottom.png"); scroll->verticalScrollBar()->setValue(0); }
            enhancementToggle->setChecked(previous);
            window.resize(1440,900);
        }
        exportButton->click();
        dialog = window.findChild<ExportDialog*>("exportDialog");
        if (dialog) {
            dialog->findChild<QLineEdit*>("exportOutputPath")
                ->setText(fixture.filePath("result.mp4"));
            // A second snapshot targets the same output to exercise the real
            // overwrite dialog.
            scale->setValue(10);
            window.findChild<QPushButton*>("addToQueueButton")->click();
            dialog->findChild<QLineEdit*>("exportOutputPath")
                ->setText(fixture.filePath("result.mp4"));
            auto* queueTable = dialog->findChild<QTableWidget*>("exportQueue");
            int confirmations = 0;
            QTimer confirm;
            confirm.setInterval(20);
            QObject::connect(&confirm, &QTimer::timeout, dialog, [&] {
                auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (message && message->windowTitle() == QString::fromUtf8("Substituir arquivo?")) {
                    ++confirmations;
                    for (auto* button : message->buttons())
                        if (button->text() == QString::fromUtf8("Substituir"))
                            button->click();
                }
            });
            confirm.start();
            bool sawFrames = false;
            QTimer observe;
            observe.setInterval(10);
            QObject::connect(&observe, &QTimer::timeout, dialog, [&] {
                const auto text = dialog->findChild<QLabel*>("exportFrameCount")->text();
                if (!text.contains(QChar(0x2014)) && !text.startsWith("0 /"))
                    sawFrames = true;
            });
            observe.start();
            dialog->findChild<QPushButton*>("exportStartButton")->click();
            check(waitUntil(
                      [&] {
                          const auto status = queueTable->item(1, 2)->text();
                          return queueTable->item(1, 2)->text() == "Concluído" || status == "Falhou";
                      },
                      120000),
                "UI export timed out");
            observe.stop();
            confirm.stop();
            check(confirmations == 1, "existing output did not ask for confirmation");
            check(queueTable->item(1, 2)->text() == "Concluído",
                "UI real export failed");
            check(QFileInfo::exists(fixture.filePath("result.mp4")),
                "UI real export missing output");
            check(sawFrames, "UI never showed real frame progress");
            check(!dialog->findChild<QLabel*>("exportFrameCount")->isVisible() && !dialog->findChild<QLabel*>("exportSpeed")->isVisible(), "removed counters are visible");
            const auto originalSize = QFileInfo(fixture.filePath("result.mp4")).size();
            queueTable->item(0, 0)->setCheckState(Qt::Checked);
            bool keptBoth = false;
            QTimer keep;
            keep.setInterval(20);
            QObject::connect(&keep, &QTimer::timeout, dialog, [&] {
                if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                    for (auto* button : prompt->buttons())
                        if (button->text() == QString::fromUtf8("Manter ambos")) {
                            keptBoth = true;
                            button->click();
                        }
            });
            keep.start();
            dialog->findChild<QPushButton*>("exportStartButton")->click();
            check(waitUntil([&] { return QFileInfo::exists(fixture.filePath("result_2.mp4")); }, 30000), "keep both did not create suffixed output");
            keep.stop();
            check(keptBoth && QFileInfo(fixture.filePath("result.mp4")).size() == originalSize, "keep both changed original output");
            capture(dialog, "export-finished.png");
            dialog->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        } else
            check(false, "real export dialog missing");
    }
    if (!video.isEmpty()) {
        framescale::ProcessingOptions comparisonOptions;
        comparisonOptions.inputPath = std::filesystem::path(video.toStdWString());
        comparisonOptions.mediaType = framescale::MediaType::Video;
        comparisonOptions.operation = framescale::Operation::UpscaleAndInterpolation;
        comparisonOptions.enhancement.enabled = true;
        comparisonOptions.enhancement.denoise = 20;
        comparisonOptions.enhancement.sharpen = 30;
        comparisonOptions.scaleFactor = 1.5;
        comparisonOptions.targetFps = 8;
        comparisonOptions.trimStartSeconds = .5;
        comparisonOptions.trimEndSeconds = 1.75;
        ComparisonDialog comparison(comparisonOptions, .5, 2, 4, &window);
        comparison.show();
        QCoreApplication::processEvents();
        check(comparison.windowTitle() == "Preview", "comparison title is not Preview");
        check(comparison.property("roundedWindowInstalled").toBool()
                && comparison.testAttribute(Qt::WA_TranslucentBackground)
                && comparison.graphicsEffect()
                && comparison.mask().isEmpty(),
            "Preview must use the shared antialiased translucent window surface");
        check(!comparison.findChild<QPushButton*>("comparisonClose"), "Preview footer Close remains");
        check(waitUntil([&] { return comparison.property("comparisonReady").toBool() || comparison.property("comparisonFailed").toBool(); }, 60000), "comparison render timed out");
        check(comparison.property("comparisonReady").toBool(), "comparison render failed");
        auto* left = comparison.findChild<MediaPreviewWidget*>("comparisonOriginal");
        auto* right = comparison.findChild<MediaPreviewWidget*>("comparisonProcessed");
        check(waitUntil([&] { return left->isPlayable() && right->isPlayable() && !left->currentFrame().isNull() && !right->currentFrame().isNull() && static_cast<TrimTimeline*>(comparison.findChild<QWidget*>("comparisonTimeline"))->isEnabled(); }, 10000), "comparison videos did not load");
        check(right->durationMs() <= 1251, "comparison exceeded the selected range");
        check(QFileInfo(left->mediaPath()).canonicalFilePath() == QFileInfo(video).canonicalFilePath(), "Original is not the selected source file");
        check(left->mediaPath() != right->mediaPath(), "comparison uses the same file on both sides");
        check(left->findChild<QMediaPlayer*>("previewPlayer")->source().toLocalFile() == QFileInfo(video).absoluteFilePath(), "Original is a transcoded proxy");
        check(waitUntil([&] { return qAbs(left->positionMs() - 500) < 50; }), "Original did not seek to the source trim start");
        check(!left->currentFrame().isNull() && !right->currentFrame().isNull(), "comparison displays empty images");
        check(waitUntil([&] { return left->property("scrubFramePosition").toLongLong() == 500; }), "Original still frame is not at the selected source time");
        QProcess reference;
        reference.start(framescale::RuntimePaths().tool("ffmpeg"), {"-v", "error", "-ss", "0.5", "-i", video, "-frames:v", "1", "-f", "image2pipe", "-c:v", "png", "pipe:1"});
        check(reference.waitForFinished(10000) && reference.exitCode() == 0, "source reference decode failed");
        const QImage expected = QImage::fromData(reference.readAllStandardOutput(), "PNG").convertToFormat(QImage::Format_RGB32);
        const QImage actual = left->currentFrame().convertToFormat(QImage::Format_RGB32);
        double difference = 0;
        if (!expected.isNull() && actual.size() == expected.size()) {
            for (int y = 0; y < actual.height(); ++y)
                for (int x = 0; x < actual.width(); ++x) {
                    const QColor a(actual.pixel(x,y)), b(expected.pixel(x,y));
                    difference += qAbs(a.red()-b.red())+qAbs(a.green()-b.green())+qAbs(a.blue()-b.blue());
                }
            difference /= actual.width()*actual.height()*3;
        } else difference = 1000;
        check(difference < 3, "Original pixels do not match an independent source decode");
        auto* comparisonTrack=static_cast<TrimTimeline*>(comparison.findChild<QWidget*>("comparisonTimeline"));
        check(!comparison.findChild<QPushButton*>("comparisonPlay"), "play button remains");
        comparisonTrack->playbackRequested();
        check(waitUntil([&] { return left->positionMs() > 500 && right->positionMs() > 400; }, 5000), "comparison playback is not synchronized");
        comparisonTrack->seekRequested(.625);
        check(waitUntil([&] { return qAbs(left->positionMs() - 500 - right->positionMs()) < 100; }), "comparison seek diverged");
        check(!left->isPlaying() && !right->isPlaying(), "comparison seek did not pause both videos");
        {
            auto* canvas = right->findChild<QLabel*>("previewImage");
            const double initial = right->property("zoomPercent").toDouble();
            const QPointF point(100, 100);
            QWheelEvent wheel(point, canvas->mapToGlobal(point.toPoint()), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            for (int i = 0; i < 25; ++i)
                QApplication::sendEvent(canvas, &wheel);
            check(right->property("zoomPercent").toDouble() < initial * .01, "comparison zoom cannot zoom out");
            check(qAbs(left->property("zoomPercent").toDouble()-right->property("zoomPercent").toDouble())>1., "comparison zoom changes both panes");
            right->findChild<QComboBox*>("previewZoom")->setCurrentIndex(0);
        }
        check(waitUntil([&] {return comparisonTrack->property("thumbnailCount").toInt()==16;}), "comparison filmstrip missing");
        capture(&comparison, "comparison.png");
        comparison.close();
        QElapsedTimer cachedTimer; cachedTimer.start();
        ComparisonDialog cached(comparisonOptions, .5, 2, 4, &window);
        cached.show();
        check(waitUntil([&]{return cached.property("comparisonReady").toBool();},2000) && cached.property("comparisonCacheHit").toBool(), "unchanged comparison rerenders");
        cached.close();
        for (double speed : {.5,2.}) {
            auto speedOptions=comparisonOptions; speedOptions.playbackSpeed=speed;
            ComparisonDialog changed(speedOptions,.5,2,4,&window); changed.show();
            check(waitUntil([&] {return changed.property("comparisonReady").toBool() || changed.property("comparisonFailed").toBool();},30000),"speed comparison timed out");
            check(changed.property("comparisonReady").toBool() && !changed.property("comparisonCacheHit").toBool(),"speed comparison failed or reused stale cache");
            auto* source=changed.findChild<MediaPreviewWidget*>("comparisonOriginal");
            auto* output=changed.findChild<MediaPreviewWidget*>("comparisonProcessed");
            auto* strip=static_cast<TrimTimeline*>(changed.findChild<QWidget*>("comparisonTimeline"));
            check(waitUntil([&] {return source->isPlayable() && output->isPlayable();}),"speed comparison players not ready");
            strip->seekRequested(.5);
            check(waitUntil([&] {return qAbs(source->positionMs()-500-output->positionMs()*speed)<150;}),"speed comparison time mapping incorrect");
            changed.close();
        }
        comparisonOptions.enhancement.sharpen=71;
        ComparisonDialog cancelled(comparisonOptions, 0, 2, 4, &window);
        cancelled.show();
        QCoreApplication::processEvents();
        QElapsedTimer closeTimer; closeTimer.start();
        cancelled.close();
        check(closeTimer.elapsed()<250, "comparison close blocked on rendering");
        check(waitUntil([&] { return !cancelled.isVisible(); }, 5000), "comparison cannot cancel while rendering");
        const QString fractionalPath = fixture.filePath("fractional.mp4");
        QProcess fractionalSource;
        fractionalSource.start(framescale::RuntimePaths().tool("ffmpeg"), {"-v","error","-f","lavfi","-i","testsrc2=size=64x48:rate=24000/1001:duration=3","-c:v","libx264","-preset","ultrafast",fractionalPath});
        check(fractionalSource.waitForFinished(10000) && fractionalSource.exitCode()==0, "fractional fixture failed");
        auto singleOptions = comparisonOptions;
        singleOptions.inputPath = std::filesystem::path(fractionalPath.toStdWString());
        singleOptions.operation = framescale::Operation::Copy;
        singleOptions.enhancement.model = 0;
        const double fractionalFps = 24000./1001;
        singleOptions.trimStartSeconds = 48 / fractionalFps;
        singleOptions.trimEndSeconds = 49 / fractionalFps;
        ComparisonDialog singleFrame(singleOptions, singleOptions.trimStartSeconds, 3, fractionalFps, &window);
        singleFrame.show();
        check(waitUntil([&]{return singleFrame.property("comparisonReady").toBool() || singleFrame.property("comparisonFailed").toBool();},15000), "single fractional frame timed out");
        check(singleFrame.property("comparisonReady").toBool(), "one-frame fractional preview rejected");
        singleFrame.close();
    }
    {
        ExportDialog queue({ }, { }, &window);
        framescale::ProcessingOptions a, b;
        a.inputPath = std::filesystem::path("first.png");
        a.outputPath = std::filesystem::path("first-out.png");
        a.scaleFactor = 10;
        b = a;
        b.inputPath = std::filesystem::path("second.png");
        b.outputPath = std::filesystem::path("second-out.png");
        b.scaleFactor = 1;
        queue.addItem(a, "10x");
        queue.addItem(b, "1x");
        auto* table = queue.findChild<QTableWidget*>("exportQueue");
        table->item(1, 0)->setCheckState(Qt::Unchecked);
        int exports = 0;
        QObject::connect(
            &queue, &ExportDialog::exportRequested, &queue,
            [&](const QString& output) {
                ++exports;
                const auto options = queue.activeOptions();
                check(options.scaleFactor == (exports == 1 ? 10 : 1),
                    "queue snapshot changed");
                queue.setProgress(40, "Inference");
                queue.setFrameProgress(2, 8);
                capture(&queue, "frame-progress.png");
                check(queue.findChild<QProgressBar*>("exportProgressBar")->value() == 250,
                    "processing progress is incorrect");
                queue.setProgress(39, "Inference");
                check(queue.findChild<QProgressBar*>("exportProgressBar")->value() == 250,
                    "overall progress regressed");
                check(!queue.findChild<QLabel*>("exportPercent"),
                    "removed total percentage remains");
                queue.setFinished(output);
                queue.jobReleased();
            });
        queue.findChild<QPushButton*>("exportStartButton")->click();
        check(waitUntil([&] { return exports == 1; }),
            "selected queue item did not export");
        QCoreApplication::processEvents();
        check(exports == 1, "unchecked queue item exported");
        table->item(1, 0)->setCheckState(Qt::Checked);
        queue.findChild<QPushButton*>("exportStartButton")->click();
        check(waitUntil([&] { return exports == 2; }),
            "checked pending item was not exported");
        check(table->item(0, 2)->text() == QString::fromUtf8("Concluído") && table->item(1, 2)->text() == QString::fromUtf8("Concluído"),
            "queue statuses incorrect");
    }
    auto* encoder = window.findChild<EncoderOptionsWidget*>("encoderOptions");
    check(encoder != nullptr, "encoder panel missing");
    if (encoder) {
        framescale::ProcessingOptions saved;
        saved.mediaType = framescale::MediaType::Video;
        saved.videoCodec = "libx265";
        saved.encoderPreset = "fast";
        saved.crf = 24;
        saved.pixelFormat = "yuv444p";
        saved.keepAudio = false;
        saved.enhancement.enabled = true;
        saved.enhancement.model = 2;
        saved.enhancement.denoise = 36;
        saved.enhancement.sharpen = 24;
        encoder->applyOptions(saved);
        encoder->savePreset("Test preset");
        const QDir presetFolder(fixture.path() + "/Presets");
        check(presetFolder.entryList({ "*.fspreset" }, QDir::Files).size() == 2, "preset did not create its own file type");
        check(!encoder->findChild<QPushButton*>("exportEncoderPreset"), "separate save-as button remains");
        check(encoder->findChild<QComboBox*>("encoderPresets")->currentText() == "Test preset", "saved file did not appear in presets");
        EncoderOptionsWidget restored;
        check(restored.loadPreset("Test preset"), "saved preset did not persist");
        framescale::ProcessingOptions result;
        restored.writeOptions(result);
        check(result.enhancement.enabled && result.enhancement.model == 2 && result.enhancement.denoise == 36 && result.enhancement.sharpen == 24, "preset lost Enhancement");
        check(result.videoCodec == saved.videoCodec && result.encoderPreset == saved.encoderPreset && result.crf == saved.crf && result.pixelFormat == saved.pixelFormat && result.keepAudio == saved.keepAudio,
            "encoder preset settings changed");
        QTimer::singleShot(0, &window, [&] {
            if (auto* prompt = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
                prompt->button(QMessageBox::Yes)->click();
        });
        encoder->findChild<QPushButton*>("removeEncoderPreset")->click();
        check(!restored.loadPreset("Test preset"), "preset removal did not persist");
        const QString portable = fixture.filePath("Portable quality.fspreset");
        bool pickerOpened = false;
        QTimer::singleShot(0, &window, [&] {
            auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            pickerOpened = picker != nullptr;
            if (picker) {
                picker->selectFile(portable);
                QMetaObject::invokeMethod(picker, "accept", Qt::QueuedConnection);
            }
        });
        encoder->findChild<QPushButton*>("saveEncoderPreset")->click();
        check(pickerOpened && QFileInfo::exists(portable), "Save preset did not write to the chosen Windows location");
        check(encoder->findChild<QComboBox*>("encoderPresets")->currentText() == "Portable quality", "portable preset was not added to the library");
        EncoderOptionsWidget portableRestored;
        check(portableRestored.loadPreset("Portable quality"), "portable preset did not persist in library");
        portableRestored.writeOptions(result);
        check(result.crf == 24 && result.videoCodec == "libx265", "portable preset changed encoding options");
        const int presetCount = encoder->findChild<QComboBox*>("encoderPresets")->count();
        QTimer::singleShot(0, &window, [&] {
            if (auto* picker = qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))
                picker->reject();
        });
        encoder->findChild<QPushButton*>("saveEncoderPreset")->click();
        check(encoder->findChild<QComboBox*>("encoderPresets")->count() == presetCount, "cancelling Save changed presets");
        check(!encoder->findChild<QGroupBox*>("advancedEncoderSection"), "Advanced Options still exists");
        check(encoder->findChild<QSpinBox*>("encoderCrf")->buttonSymbols() == QAbstractSpinBox::NoButtons, "CRF arrows remain");
        QTimer::singleShot(0, &window, [&] {
            auto* options = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(options && options->objectName() == "formatOptionsDialog", "Format Options did not open");
            if (options) {
                check(options->findChild<QComboBox*>("videoCodec")->currentData() == "libx265", "wrong codec in Format Options");
                auto* audioTabs = options->findChild<QTabWidget*>("formatDetails");
                audioTabs->setCurrentIndex(1);
                auto* enabled = options->findChild<QCheckBox*>("encoderKeepAudio");
                enabled->setChecked(true);
                auto* audioCodec = options->findChild<QComboBox*>("encoderAudioCodec");
                audioCodec->setCurrentIndex(audioCodec->findData("copy"));
                check(!options->findChild<QComboBox*>("encoderAudioRate")->isVisible() && !options->findChild<QComboBox*>("encoderSampleRate")->isVisible(), "copy audio shows irrelevant options");
                audioCodec->setCurrentIndex(audioCodec->findData("aac"));
                check(options->findChild<QComboBox*>("encoderAudioRate")->isVisible(), "AAC bitrate options did not return");
                enabled->setChecked(false);
                check(!audioCodec->isVisible(), "disabled audio still displays options");
                audioTabs->setCurrentIndex(0);
                options->findChild<QSpinBox*>("encoderCrf")->setValue(31);
                capture(options, "format-options.png");
                options->reject();
            }
        });
        encoder->findChild<QPushButton*>("formatOptionsButton")->click();
        check(encoder->findChild<QSpinBox*>("encoderCrf")->value() == 24, "Cancel committed format settings");
        auto* formats = encoder->findChild<QComboBox*>("encoderSuffix");
        for (auto suffix : { ".avi", ".mp3", ".wav", ".png", ".jpg" })
            check(formats->findData(suffix) >= 0, "new output format missing");
        formats->setCurrentIndex(formats->findData(".avi"));
        check(encoder->findChild<QComboBox*>("videoCodec")->findData("prores_ks") == -1, "AVI offers incompatible codec");
        formats->setCurrentIndex(formats->findData(".mkv"));
        check(encoder->findChild<QComboBox*>("videoCodec")->findData("prores_ks") == -1, "Matroska still offers ProRes");
        formats->setCurrentIndex(formats->findData(".mp3"));
        check(encoder->findChild<QTabWidget*>("formatDetails")->count() == 1, "audio-only format shows video options");
        QTimer::singleShot(0, &window, [&] {
            auto* options = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (options) {
                capture(options, "format-audio.png");
                options->accept();
            }
        });
        encoder->findChild<QPushButton*>("formatOptionsButton")->click();
        formats->setCurrentIndex(formats->findData(".mp4"));
        encoder->findChild<QComboBox*>("videoCodec")->setCurrentIndex(0);
        auto* pixelFormat = encoder->findChild<QComboBox*>("encoderPixelFormat");
        pixelFormat->setCurrentIndex(pixelFormat->findData("yuv444p"));
        auto* profile = encoder->findChild<QComboBox*>("encoderProfile");
        profile->setCurrentIndex(profile->findData("high"));
        check(pixelFormat->currentData() == "yuv420p", "H.264 profile allowed incompatible chroma");
        QTimer::singleShot(0, &window, [&] {
            auto* options = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (options) {
                auto* rate = options->findChild<QComboBox*>("encoderRateControl");
                rate->setCurrentIndex(rate->findData("cbr"));
                options->findChild<QDoubleSpinBox*>("encoderTargetRate")->setValue(12.5);
                capture(options, "format-bitrate.png");
                options->accept();
            }
        });
        encoder->findChild<QPushButton*>("formatOptionsButton")->click();
        encoder->writeOptions(result);
        check(result.rateControl == "cbr" && result.bitRate == 12500000, "accepted bitrate settings did not apply");
        encoder->applyOptions(saved);
        window.findChild<QTabWidget*>("inspectorTabs")->setCurrentIndex(1);
        capture(&window, "encoder-options.png");
    }
    const QString heavy = qEnvironmentVariable("FRAMESCALE_UI_HEAVY");
    if (!heavy.isEmpty()) {
        QTimer::singleShot(0, &window, [&] {
            auto* prompt = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            check(prompt && prompt->objectName() == "openProjectDialog", "large video project prompt missing");
            if (prompt) prompt->accept();
        });
        QElapsedTimer loading;
        loading.start();
        window.openInputFile(heavy);
        check(waitUntil([&] {auto* player=window.findChild<QMediaPlayer*>("previewPlayer");
            return player && player->isAvailable() && window.findChild<QPushButton*>("previewPlayButton")->isEnabled() && window.findChild<QLabel*>("previewImage")->isVisible() && !window.findChild<QLabel*>("previewImage")->pixmap().isNull(); }, 30000),
            "large video proxy did not become playable");
        std::cout << "4K preview playable in " << loading.elapsed() << " ms\n";
        if (auto* player = window.findChild<QMediaPlayer*>("previewPlayer"))
            check(player->source().toLocalFile() != QFileInfo(heavy).absoluteFilePath(), "large preview is decoding original resolution");
        auto* track = window.findChild<QWidget*>("trimTimeline");
        window.activateWindow();
        track->setFocus();
        QCoreApplication::processEvents();
        QKeyEvent spacePress(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
        QKeyEvent spaceRelease(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
        QApplication::sendEvent(track, &spacePress);
        QApplication::sendEvent(track, &spaceRelease);
        check(waitUntil([&] { return window.findChild<QMediaPlayer*>("previewPlayer")->playbackState() == QMediaPlayer::PlayingState; }),
            "space did not start timeline playback");
        QApplication::sendEvent(track, &spacePress);
        QApplication::sendEvent(track, &spaceRelease);
        check(waitUntil([&] { return window.findChild<QMediaPlayer*>("previewPlayer")->playbackState() != QMediaPlayer::PlayingState; }),
            "space did not pause timeline playback");
        preview->setTrimRange(0.25, 0.75);
        window.findChild<QPushButton*>("previewPlayButton")->click();
        check(waitUntil([&] {auto* player=window.findChild<QMediaPlayer*>("previewPlayer"); return player->playbackState()!=QMediaPlayer::PlayingState && qAbs(player->position()-250)<80; }, 5000),
            "trim end did not return to start");
        capture(&window, "preview-large.png");
        if (qEnvironmentVariableIsSet("FRAMESCALE_UI_SEGMENTS")) {
            preview->setTrimRange(0, 0);
            const QRect timelineBefore = track->geometry();
            auto seekTimeline = [&](double fraction) {
                const QPointF point(18 + (track->width() - 36) * fraction, 20);
                QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(track, &press);
                QApplication::sendEvent(track, &release);
            };
            seekTimeline(.55);
            check(waitUntil([&] { return window.findChild<QLabel*>("previewNotice")->isVisible(); }, 2000), "proxy preparation notice did not appear");
            check(track->geometry() == timelineBefore, "preparing a proxy displaced the timeline");
            seekTimeline(.95);
            seekTimeline(.75);
            // Last seek must win even while no QMediaPlayer exists.

            check(waitUntil([&] { auto* player = window.findChild<QMediaPlayer*>("previewPlayer"); return player && player->mediaStatus() == QMediaPlayer::LoadedMedia && qAbs(preview->positionMs() - qint64(preview->durationMs() * .75)) < 250; }, 15000), "seeking beyond first proxy segment failed");
            check(window.findChild<QMediaPlayer*>("previewPlayer")->duration() <= 4100, "proxy converted the complete long video");
            check(window.findChild<QMediaPlayer*>("previewPlayer")->playbackState() != QMediaPlayer::PlayingState, "scrubbing started playback");
            auto* zoom = window.findChild<QComboBox*>("previewZoom");
            zoom->setCurrentIndex(zoom->findData(2.0));
            check(!window.findChild<QLabel*>("previewImage")->pixmap().isNull(), "zoom lost the frame");
            capture(&window, "preview-zoom.png");
            zoom->setCurrentIndex(0);
            // 15 seconds is in the second chunk; playback must cross 16 seconds.
            window.findChild<QPushButton*>("previewPlayButton")->click();
            check(waitUntil([&] { auto* player = window.findChild<QMediaPlayer*>("previewPlayer"); return player && player->playbackState() == QMediaPlayer::PlayingState && preview->positionMs() > 16150 && player->position() < 4000; }, 15000), "playback did not advance to next proxy segment");
            preview->pause();
        }
    }
    bool settingsSeen = false;
    QTimer::singleShot(0, &window, [&] {
        auto* settings = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!settings)
            return;
        auto* choices = settings->findChild<QComboBox*>("languageChoice");
        settingsSeen = choices && choices->findData("pt-PT") >= 0 && choices->findData("pt-BR") >= 0;
        auto* themes = settings->findChild<QComboBox*>("appearanceChoice");
        check(themes && themes->count() == 3, "macOS theme choices missing");
        auto* transparency=settings->findChild<QCheckBox*>("glassTransparencyChoice");
        check(transparency && transparency->isChecked(),"panel transparency control missing");
        check(!settings->findChild<QLabel*>("brandLogo"),
            "settings contains removed branding");
        check(settings->findChild<QDialogButtonBox*>()
                    ->button(QDialogButtonBox::Ok)
                    ->text()
                == "OK",
            "settings confirmation is not OK");
        check(choices && choices->width() < 230 && choices->width() >= choices->fontMetrics().horizontalAdvance(choices->currentText()) + 28,
            "language field is oversized or clips text");
        check(themes && themes->width() < 230, "appearance field is oversized");
        capture(settings, "settings.png");
        settings->reject();
    });
    window.findChild<QAction*>("preferencesAction")->trigger();
    check(settingsSeen, "language settings missing");
    QSettings().setValue("appearance", "macos-dark");
    Appearance::refresh();
    check(qApp->palette().color(QPalette::Window).lightness() < 100, "dark theme palette missing");
    capture(&window, "macos-dark.png");
    QTimer::singleShot(0,&window,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        check(dialog && dialog->objectName()=="formatOptionsDialog","dark format dialog not opened");
        if (!dialog) return;
        capture(dialog,"dark-format-video.png");
        auto* details=dialog->findChild<QTabWidget*>("formatDetails");
        if (!details->isTabEnabled(1)) {dialog->reject(); return;}
        details->setCurrentIndex(1);
        capture(dialog,"dark-format-audio.png");
        auto* page=dialog->findChild<QWidget*>("formatAudioPage");
        if(dialog->property("nativeGlassActive").toBool()) {
            const QPoint sample=page->mapTo(dialog,QPoint(page->width()/2,page->height()-30));
            check(dialog->grab().toImage().pixelColor(sample).alpha()<245,"format audio page blocks the glass background");
        }
        for(auto* label:page->findChildren<QLabel*>())
            check(!label->isVisible() || !label->text().contains("original."),"audio explanatory text remains");
        details->setCurrentIndex(0);
        dialog->reject();
    });
    window.findChild<QPushButton*>("formatOptionsButton")->click();
    check(window.findChild<QComboBox*>("previewZoom")->count()<=7,"zoom popup has too many entries");
    QSettings().setValue("appearance", "studio");
    Appearance::refresh();
    capture(&window, "studio-theme.png");
    QSettings().setValue("appearance", "macos-light");
    Appearance::refresh();
    check(qApp->palette().color(QPalette::Window).lightness() > 200, "light theme palette missing");
    capture(&window, "macos-light.png");
    QSettings().setValue("glassTransparency",false);
    check(GlassMaterial::reduced(), "in-app reduce transparency setting ignored");
    Appearance::refresh();
    check(!window.property("nativeGlassActive").toBool(),"native blur remains active with transparency disabled");
    check(window.grab().toImage().pixelColor(window.width()-100,window.height()-150).alpha()==255,"reduced-transparency panel is not opaque");
    QSettings().setValue("glassTransparency",true);
    Appearance::refresh();
    window.resize(900, 620);
    capture(&window, "compact-studio.png");
    window.resize(1440, 900);
    InterfaceTranslator::applyLanguage(true);
    check(menus->actions()[1]->text() == "Settings", "language change did not update open menus");
    check(tabs->count() >= 2, "language change lost projects");
    check(!window.findChild<QLabel*>("titleLogo")->pixmap().isNull(), "language change cleared logo");
    InterfaceTranslator::applyLanguage(false);
    check(window.findChild<QCheckBox*>("enableEnhancement")->text() == QString::fromUtf8("Melhoria de imagem"), "Enhancement not translated back to Portuguese");
    check(menus->actions()[1]->text() == QString::fromUtf8("Definições"), "language did not switch back to Portugal");
    InterfaceTranslator::applyLanguage(QStringLiteral("pt-BR"));
    check(menus->actions()[1]->text() == QString::fromUtf8("Configurações"), "Brazilian Portuguese missing");
    check(window.findChild<QPushButton*>("comparePreviewButton")->text() == "Preview", "Preview label not translated");
    InterfaceTranslator::applyLanguage(QStringLiteral("pt-PT"));
    InterfaceTranslator translator;
    check(translator.translate("", "Novo") == "New",
        "English translation missing");
    bool hasQuitShortcut = false;
    for (auto* shortcut : window.findChildren<QShortcut*>())
        hasQuitShortcut |= shortcut->key() == QKeySequence(Qt::CTRL | Qt::Key_Q);
    check(hasQuitShortcut && window.findChild<QAction*>("quitAction")->shortcuts().isEmpty(),
        "Exit shortcut must work without appearing in the menu");
    QTimer::singleShot(0, &window, [&] {
        auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        check(message && message->objectName()=="exitConfirmation", "closing projects did not open themed confirmation");
        if (message) {
            check(message->property("roundedWindowInstalled").toBool(), "exit confirmation lacks shared corners");
            capture(message, "confirmation.png");
            message->reject();
        }
    });
    window.close();
    check(window.isVisible(), "declining close closed the window");
    QTimer::singleShot(0, &window, [&] {
        if (auto* message = qobject_cast<QDialog*>(QApplication::activeModalWidget())) message->accept();
    });
    window.close();
    if (!failures)
        std::cout << "FrameScale UI smoke: menus, image preview, options, export "
                     "flow, branding OK\n";
    return failures ? 1 : 0;
}
