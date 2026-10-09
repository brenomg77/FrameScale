#include "ui/LayerEditor.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QFileInfo>
#include <iostream>

// Model/command checks only: never show a window, capture pixels or test styling.
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    int failures=0;
    auto check=[&](bool condition,const char* message) { if(!condition) { ++failures; std::cerr<<message<<'\n'; } };
    framescale::MediaLayer video;
    video.path="missing-fixture.mp4"; video.sourceDuration=video.out=2;
    video.width=96; video.height=64; video.link=0;
    auto audio=video; audio.kind=framescale::LayerKind::Audio;
    framescale::ProcessingOptions project;
    project.layered=true; project.layers={video,audio};
    {
        LayerEditor editor;
        editor.setProject("missing-fixture.mp4",project,true);
        auto* timeline=dynamic_cast<TrimTimeline*>(editor.findChild<QWidget*>("layerTracks"));
        check(timeline!=nullptr,"layers must reuse the original timeline");
        if(timeline) {
            timeline->resize(900,210);
            const int width=timeline->width();
            const double anchor=timeline->timeAt(450);
            timeline->setZoom(4,450);
            check(timeline->width()==width,"zoom resized the panel instead of its time viewport");
            check(std::abs(timeline->timeAt(450)-anchor)<1e-6,"zoom lost the time under the pointer");
            check(timeline->height()==210,"two-layer timeline no longer uses original height");
            timeline->resetView();
        }
        check(!editor.hasSelection(),"opening a project selected a layer automatically");
        if(timeline) {
            auto mouse=[&](QEvent::Type type,double x,double y) {
                QMouseEvent event(type,QPointF(x,y),QPointF(x,y),type==QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,type==QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,Qt::NoModifier);
                QApplication::sendEvent(timeline,&event);
            };
            int invalidations=0;
            editor.previewInvalidated=[&]{++invalidations;};
            mouse(QEvent::MouseButtonPress,timeline->pixelAt(.5),75);
            mouse(QEvent::MouseButtonRelease,timeline->pixelAt(.5),75);
            check(invalidations==0,"selecting an unchanged layer rebuilt the preview");
            mouse(QEvent::MouseButtonPress,timeline->pixelAt(0),40);
            mouse(QEvent::MouseMove,timeline->pixelAt(.4),40);
            mouse(QEvent::MouseButtonRelease,timeline->pixelAt(.4),40);
            check(std::abs(editor.exportStart()-.4)<.01,"export start marker did not move");
            check(invalidations==0,"export marker rebuilt the preview");
            editor.previewInvalidated={};
            double sought=-1;
            editor.seekRequested=[&](double value){sought=value;};
            mouse(QEvent::MouseButtonPress,timeline->pixelAt(.2),35);
            mouse(QEvent::MouseMove,timeline->pixelAt(1.2),35);
            check(timeline->property("currentFrame").toLongLong()==30,"needle waited for the playback timer");
            editor.setPosition(.1);
            check(timeline->property("currentFrame").toLongLong()==30,"playback timer overwrote mouse position");
            mouse(QEvent::MouseButtonRelease,timeline->pixelAt(1.2),35);
            check(std::abs(sought-1.2)<.01,"playhead did not follow mouse drag");
            mouse(QEvent::MouseButtonPress,timeline->pixelAt(2)+5,75);
            mouse(QEvent::MouseMove,timeline->pixelAt(1)+5,75);
            mouse(QEvent::MouseButtonRelease,timeline->pixelAt(1)+5,75);
            check(std::abs(editor.layers()[0].duration()-1)<.01,"wide edge target did not trim video");
            mouse(QEvent::MouseButtonPress,timeline->pixelAt(.5),75);
            mouse(QEvent::MouseMove,timeline->pixelAt(1),75);
            mouse(QEvent::MouseButtonRelease,timeline->pixelAt(1),75);
            check(std::abs(editor.layers()[0].start-.5)<.01,"trimmed layer could not move independently");
            check(editor.layers()[1].start==0,"moving video displaced unselected audio");
            editor.setProject("missing-fixture.mp4",project,true);
        }
        editor.selectLayer(0);
        editor.toggleSelectedLink();
        check(editor.isLayerSelected(0) && editor.isLayerSelected(1),"linked tracks were not both selected");
        editor.selectLayer(1);
        check(editor.isLayerSelected(0) && editor.isLayerSelected(1),"selecting audio did not highlight linked video");
        editor.selectLayer(0);
        check(editor.layers()[0].link!=0 && editor.layers()[0].link==editor.layers()[1].link,"join did not link audio and video");
        editor.toggleSelectedLink();
        check(editor.layers()[0].link==0 && editor.layers()[1].link==0,"separate did not unlink tracks");
        editor.toggleLayerEnabled(0);
        check(!editor.layers()[0].enabled && editor.layers()[1].enabled,"eye toggle also muted audio");
        editor.undo();
        check(editor.layers()[0].enabled,"undo did not restore visibility");
        editor.setSpeed(2);
        check(editor.layers()[0].speed==2 && editor.layers()[1].speed==1,"speed changed unselected audio");
        editor.removeSelected();
        check(editor.layers().size()==1 && editor.layers()[0].kind==framescale::LayerKind::Audio,"Delete removed the wrong layer");
        editor.setSpeed(.5);
        check(editor.layers()[0].speed==.5,"audio layer speed was not editable");
        editor.undo();
        check(editor.layers()[0].speed==1,"undo audio speed failed");
        editor.undo();
        check(editor.layers().size()==2 && editor.layers()[0].speed==2,"undo deletion lost layer data");
        editor.redo();
        check(editor.layers().size()==1,"redo deletion failed");
        auto saved=project; saved.layers=editor.layers();
        editor.setProject("other.mp4",project,true);
        check(editor.layers()[0].speed==1,"speed leaked to another project");
        editor.setProject("missing-fixture.mp4",saved,true);
        check(editor.layers().size()==1 && editor.layers()[0].kind==framescale::LayerKind::Audio,"project restore resurrected deleted layer");
        editor.selectLayer(0);
        editor.removeSelected();
        check(editor.layers().empty() && !editor.hasSelection(),"final layer deletion failed");
        editor.setSpeed(4); editor.removeSelected(); // Empty project operations are safe.
        editor.undo();
        check(editor.layers().size()==1,"empty project undo failed");
        editor.setProject({}, {}, false);
        check(!editor.active() && editor.layers().empty(),"closing project retained layers");
    }
    {
        LayerEditor editor;
        auto linked=project;
        for(auto& layer:linked.layers) layer.link=7;
        editor.setProject("missing-fixture.mp4",linked,true);
        editor.selectLayer(1);
        editor.setSpeed(2);
        check(editor.layers()[0].speed==2 && editor.layers()[1].speed==2,"linked speed did not affect both tracks");
        editor.undo();
        auto* timeline=dynamic_cast<TrimTimeline*>(editor.findChild<QWidget*>("layerTracks"));
        timeline->resize(900,210);
        auto mouse=[&](QEvent::Type type,double time,double y) {
            QPointF point(timeline->pixelAt(time),y);
            QMouseEvent event(type,point,point,type==QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,type==QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(timeline,&event);
        };
        mouse(QEvent::MouseButtonPress,0,75);
        mouse(QEvent::MouseMove,.5,75);
        mouse(QEvent::MouseButtonRelease,.5,75);
        check(std::abs(editor.layers()[0].in-.5)<.01 && std::abs(editor.layers()[1].in-.5)<.01,"linked left trim did not affect both tracks");
        check(std::abs(editor.layers()[0].start-editor.layers()[1].start)<1e-8,"linked trim lost synchronization");
        mouse(QEvent::MouseButtonPress,2,125);
        mouse(QEvent::MouseMove,1.5,125);
        mouse(QEvent::MouseButtonRelease,1.5,125);
        check(std::abs(editor.layers()[0].out-1.5)<.01 && std::abs(editor.layers()[1].out-1.5)<.01,"linked right trim from audio did not affect video");
        editor.toggleLayerEnabled(1);
        check(editor.layers()[0].enabled && !editor.layers()[1].enabled,"muting linked audio hid the video");
        check(editor.layers()[0].link==7 && editor.layers()[1].link==7,"muting audio broke the link");
        editor.toggleLayerEnabled(1);
        check(editor.layers()[0].enabled && editor.layers()[1].enabled,"unmuting linked audio changed visibility");
        editor.toggleLayerEnabled(0);
        check(!editor.layers()[0].enabled && editor.layers()[1].enabled,"hiding linked video muted the audio");
        editor.undo();
        check(editor.layers()[0].enabled && editor.layers()[1].enabled,"undo visibility changed audio state");
        editor.selectLayer(1); editor.removeSelected();
        check(editor.layers().empty(),"linked deletion left an orphan track");
        editor.undo();
        check(editor.layers().size()==2 && editor.layers()[0].link==7 && editor.layers()[1].link==7,"undo did not restore the complete linked group");
        check(std::abs(editor.layers()[0].out-1.5)<.01,"undo deletion lost linked trim");
    }
    if(argc>1) {
        LayerEditor editor;
        QEventLoop loop;
        QString result,error;
        editor.previewReady=[&](const QString& path){ result=path; loop.quit(); };
        editor.previewFailed=[&](const QString& message){ error=message; loop.quit(); };
        QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
        editor.setProject(QString::fromLocal8Bit(argv[1]),{},true);
        timeout.start(125000); loop.exec();
        check(editor.layers().size()==2 && editor.layers()[0].link && editor.layers()[0].link==editor.layers()[1].link,"import did not link audio and video by default");
        check(!result.isEmpty() && QFileInfo::exists(result),"real media preview never completed");
        if(!error.isEmpty()) std::cerr<<error.toStdString()<<'\n';
        if(!result.isEmpty()) {
            framescale::ProcessingOptions moved; moved.layered=true; moved.layers=editor.layers();
            for(auto& layer:moved.layers) if(layer.kind==framescale::LayerKind::Video) {
                layer.start=4.1; layer.in=4.1; layer.out=std::min(layer.sourceDuration,35.1);
            }
            result.clear(); error.clear();
            editor.setProject(QString::fromLocal8Bit(argv[1]),moved,true);
            timeout.start(125000); loop.exec();
            check(!result.isEmpty() && QFileInfo::exists(result),"moved/trimmed real preview never completed");
            if(!error.isEmpty()) std::cerr<<error.toStdString()<<'\n';
        }
        if(!result.isEmpty()) {
            // Keep an artifact in the test build directory, never beside user media.
            const QString artifact=QCoreApplication::applicationDirPath()+"/preview-regression.mp4";
            QFile::remove(artifact); QFile::copy(result,artifact);
            std::cout<<"Real preview: "<<artifact.toStdString()<<'\n';
        }
    }
    if(!failures) std::cout<<"Layer commands: independent speed, deletion, undo/redo and project isolation OK\n";
    return failures ? 1 : 0;
}
