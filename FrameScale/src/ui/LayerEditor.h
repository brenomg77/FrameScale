#pragma once
#include "processing/LayerComposition.h"
#include "processing/RuntimePaths.h"
#include "ui/SelectionComboBox.h"
#include "ui/ControlSymbols.h"
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include "ui/TrimTimeline.h"
#include <QToolButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QShortcut>
#include <QCache>
#include <QSet>
#include <memory>
#include <functional>
#include <limits>

class LayerEditor final : public QWidget {
    using Layer = framescale::MediaLayer;
    using Kind = framescale::LayerKind;
    class Tracks final : public TrimTimeline {
    public:
        LayerEditor* owner;
        explicit Tracks(LayerEditor* editor) : TrimTimeline(editor), owner(editor) {
            setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
        }
        bool event(QEvent* e) override {
            if(e->type()==QEvent::ShortcutOverride) {
                auto* key=static_cast<QKeyEvent*>(e);
                if(key->matches(QKeySequence::Undo) || key->matches(QKeySequence::Redo)
                    || key->key()==Qt::Key_Delete || key->key()==Qt::Key_Space) { e->accept(); return true; }
            }
            return TrimTimeline::event(e);
        }
        bool navigating_ = false, suppressContext_=false;
        void mousePressEvent(QMouseEvent* e) override {
            setFocus();
            if(e->button()==Qt::RightButton && e->modifiers().testFlag(Qt::ControlModifier)) {
                suppressContext_=true;
                owner->context(e->position().toPoint(),e->globalPosition().toPoint());
                QTimer::singleShot(300,this,[this]{suppressContext_=false;});
                e->accept(); return;
            }
            if(e->button()==Qt::MiddleButton) { navigating_=true; TrimTimeline::mousePressEvent(e); }
            else owner->press(e);
        }
        void mouseMoveEvent(QMouseEvent* e) override {
            if(navigating_) TrimTimeline::mouseMoveEvent(e); else owner->move(e);
        }
        void mouseReleaseEvent(QMouseEvent* e) override {
            if(navigating_) { navigating_=false; TrimTimeline::mouseReleaseEvent(e); }
            else owner->release();
        }
        void leaveEvent(QEvent* e) override { unsetCursor(); TrimTimeline::leaveEvent(e); }
        void contextMenuEvent(QContextMenuEvent* e) override { if(suppressContext_) { suppressContext_=false; e->accept(); return; } owner->context(e->pos(), e->globalPos()); }
        void keyPressEvent(QKeyEvent* e) override {
            if (e->key() == Qt::Key_Delete) { owner->removeSelected(); e->accept(); }
            else if (e->matches(QKeySequence::Undo)) { owner->undo(); e->accept(); }
            else if (e->matches(QKeySequence::Redo)) { owner->redo(); e->accept(); }
            else if (e->key() == Qt::Key_Space && owner->playRequested) { owner->playRequested(); e->accept(); }
            else TrimTimeline::keyPressEvent(e);
        }
        void wheelEvent(QWheelEvent* e) override {
            if(owner->drag_) { e->accept(); return; }
            TrimTimeline::wheelEvent(e);
        }
    };
public:
    explicit LayerEditor(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("layerEditor"); setAcceptDrops(true);
        setMinimumWidth(0); setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
        tracks_ = new Tracks(this); tracks_->setObjectName("layerTracks");
        tracks_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
        tracks_->paintLayerTracks = [this](QPainter& painter) { paintTracks(painter); };
        tracks_->seekRequested = [this](double seconds) { if(seekRequested) seekRequested(seconds); };
        layout->addWidget(tracks_);
        renderTimer_.setSingleShot(true); renderTimer_.setInterval(350);
        connect(&renderTimer_, &QTimer::timeout, this, [this] { renderPreview(); });
        auto* undoShortcut=new QShortcut(QKeySequence::Undo,this);
        connect(undoShortcut,&QShortcut::activated,this,[this]{if(active_) undo();});
        auto* redoShortcut=new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z),this);
        connect(redoShortcut,&QShortcut::activated,this,[this]{if(active_) redo();});
        qApp->installEventFilter(this);
        hide();
    }
    ~LayerEditor() override { cancelRender(); cancelReaders(); }
    std::function<void()> changed, selectionChanged, playRequested, previewInvalidated;
    std::function<void(double)> seekRequested;
    std::function<void(const QPoint&)> volumeRequested;
    std::function<void(const QString&)> previewReady, status, previewFailed;
    const std::vector<Layer>& layers() const { return layers_; }
    bool active() const { return active_; }
    bool initialized() const { return initialized_; }
    bool ready() const { return active_ && pendingImports_ == 0 && !render_ && !renderTimer_.isActive(); }
    double selectedSpeed() const { return selected_ >= 0 && selected_ < int(layers_.size()) ? layers_[selected_].speed : 1.; }
    void selectLayer(int index) { selected_ = index>=0 && index<int(layers_.size()) ? index : -1; refresh(); }
    double exportStart() const { return workStart_; }
    double exportEnd() const { return workEnd_; }
    bool isLayerSelected(int index) const {
        return index>=0 && index<int(layers_.size()) && hasSelection() &&
            (index==selected_ || (layers_[index].link && layers_[index].link==layers_[selected_].link));
    }
    void toggleLayerEnabled(int index) {
        if(index<0 || index>=int(layers_.size())) return;
        remember();
        // Mute and visibility belong to individual tracks, even in a linked group.
        layers_[index].enabled=!layers_[index].enabled;
        edited();
    }
    void toggleSelectedLink() { toggleLink(); }
    bool originalLayers() const {
        if(layers_.size()!=2) return false;
        return layers_[0].path==layers_[1].path && layers_[0].kind!=layers_[1].kind
            && std::all_of(layers_.begin(),layers_.end(),[](const Layer& l) {
                return l.enabled && l.start==0 && l.in==0 && l.out==l.sourceDuration && l.speed==1;
            });
    }
    bool hasSelection() const { return selected_ >= 0 && selected_ < int(layers_.size()); }
    void setSpeed(double speed) {
        if (!hasSelection() || !std::isfinite(speed)) return;
        speed = std::clamp(speed, .25, 4.);
        bool differs=false;
        for(int i=0;i<int(layers_.size());++i) if(isLayerSelected(i) && layers_[i].speed!=speed) differs=true;
        if(!differs) return;
        remember();
        for(int i=0;i<int(layers_.size());++i) if(isLayerSelected(i)) layers_[i].speed=speed;
        edited();
    }
    void setPosition(double seconds) { if(seeking_) return; position_ = std::max(0., seconds); updateRange(); }
    void setProject(const QString& path, const framescale::ProcessingOptions& options, bool active) {
        ++generation_; cancelRender(); cancelReaders(); renderTimer_.stop();
        initialized_ = options.layered;
        workStart_=options.trimStartSeconds; workEnd_=options.trimEndSeconds; projectExtent_=1; seeking_=false; drag_=0; active_ = active; pendingImports_ = 0; layers_.clear(); thumbnails_.clear();
        undo_.clear(); redo_.clear(); selected_ = -1; position_ = 0; tracks_->resetView();
        setVisible(active);
        if (!active) { refresh(); return; }
        if (options.layered) {
            layers_ = options.layers; selected_ = -1; projectExtent_=std::max(1.,framescale::compositionDuration(layers_));
            for (const auto& layer : layers_) { nextLink_ = std::max(nextLink_, layer.link); loadThumbnail(layer); }
            refresh(); if(previewInvalidated) previewInvalidated(); renderTimer_.start();
        } else importMedia(path, true, options);
        refresh();
    }
    void chooseMedia() {
        if (!active_) return;
        const auto files = QFileDialog::getOpenFileNames(this, tr("Adicionar camadas"), {},
            tr("Multimédia (*.mp4 *.mkv *.mov *.avi *.webm *.gif *.m4v *.mp3 *.wav *.flac *.aac *.m4a *.ogg *.opus);;Todos os ficheiros (*)"));
        for (const auto& file : files) importMedia(file);
    }
    void removeSelected() {
        if (!hasSelection()) return;
        remember();
        for(int i=int(layers_.size())-1;i>=0;--i) {
            const int group=undo_.back()[selected_].link;
            if(i==selected_ || (group && layers_[i].link==group)) layers_.erase(layers_.begin()+i);
        }
        for(auto& layer:layers_) if(layer.link && std::count_if(layers_.begin(),layers_.end(),[&](const Layer& other){return other.link==layer.link;})<2) layer.link=0;
        selected_ = layers_.empty() ? -1 : std::min(selected_, int(layers_.size())-1); edited();
    }
    void undo() {
        if (undo_.empty()) return;
        redo_.push_back(layers_); layers_ = undo_.back(); undo_.pop_back();
        selected_ = layers_.empty() ? -1 : 0; edited();
    }
    void redo() {
        if (redo_.empty()) return;
        undo_.push_back(layers_); layers_ = redo_.back(); redo_.pop_back();
        selected_ = layers_.empty() ? -1 : 0; edited();
    }
protected:
    bool eventFilter(QObject* watched,QEvent* event) override {
        if(active_ && hasSelection() && event->type()==QEvent::MouseButtonPress) {
            auto* widget=qobject_cast<QWidget*>(watched);
            if(widget && widget->window()==window() && widget!=this && !isAncestorOf(widget)) {
                bool control=false;
                for(auto* item=widget;item && item!=window();item=item->parentWidget())
                    if(item->inherits("QAbstractButton") || item->inherits("QAbstractSpinBox") || item->inherits("QComboBox") || item->inherits("QAbstractSlider") || item->inherits("QLineEdit")) control=true;
                if(!control) selectLayer(-1);
            }
        }
        return QWidget::eventFilter(watched,event);
    }
    void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); refresh(); }
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (active_ && event->mimeData()->hasUrls()) event->acceptProposedAction();
    }
    void dropEvent(QDropEvent* event) override {
        for (const auto& url : event->mimeData()->urls()) if (url.isLocalFile()) importMedia(url.toLocalFile());
        event->acceptProposedAction();
    }
private:
    void remember() { undo_.push_back(layers_); if (undo_.size()>100) undo_.erase(undo_.begin()); redo_.clear(); }
    double extent() const { return drag_ ? dragExtent_ : projectExtent_; }
    std::vector<int> displayOrder() const {
        std::vector<int> order;
        for(auto kind : {Kind::Video, Kind::Audio})
            for(int i=0;i<int(layers_.size());++i) if(layers_[i].kind==kind) order.push_back(i);
        return order;
    }
    int rowAt(double y) const {
        if(y<56) return -1;
        const int row=int((y-56)/50);
        const auto order=displayOrder();
        return row<int(order.size()) ? order[row] : -1;
    }
    double pixelsPerSecond() const { return tracks_->contentWidth() / tracks_->visibleDuration(); }
    double x(double time) const { return tracks_->pixelAt(time); }
    QRectF clip(int index) const {
        const auto order=displayOrder();
        const int row=int(std::find(order.begin(),order.end(),index)-order.begin());
        const auto& l=layers_[index]; return QRectF(x(l.start), 56+row*50, std::max(2.,l.duration()*pixelsPerSecond()),47);
    }
    bool hasLinkPartner() const {
        return hasSelection() && std::count_if(layers_.begin(),layers_.end(),[&](const Layer& l){return l.path==layers_[selected_].path;})>1;
    }
    QString timecode(double seconds) const {
        double fps=25;
        for(const auto& layer:layers_) if(layer.kind==Kind::Video) {fps=layer.sourceFps;break;}
        const qint64 whole=qFloor(std::max(0.,seconds)+1.e-7);
        const int frame=std::clamp(int(qRound64(seconds*fps)-qRound64(whole*fps)),0,qMax(0,qCeil(fps)-1));
        QString text=QString("%1:%2:%3").arg(whole/60%60,2,10,QChar('0')).arg(whole%60,2,10,QChar('0')).arg(frame,2,10,QChar('0'));
        if(whole>=3600) text=QString("%1:").arg(whole/3600,2,10,QChar('0'))+text;
        return text;
    }
    void updateRange() {
        if(drag_) return;
        const double start=hasSelection() ? layers_[selected_].start : 0;
        const double end=hasSelection() ? start+layers_[selected_].duration() : 0;
        tracks_->setRange(extent(),start,end,position_);
    }
    void refresh() {
        double fps=25;
        for(const auto& layer:layers_) if(layer.kind==Kind::Video) {fps=layer.sourceFps;break;}
        tracks_->setFrameRate(fps);
        tracks_->setLayerRows(int(layers_.size()));
        updateRange();
        tracks_->update();
        if (selectionChanged) selectionChanged();
    }
    void edited() {
        cancelRender(); refresh(); if (changed) changed();
        if(previewInvalidated) previewInvalidated();
        renderTimer_.start();
    }
    void toggleLink() {
        if (!hasSelection()) return;
        remember(); const auto target = layers_[selected_];
        if (target.link) { for (auto& layer : layers_) if(layer.link==target.link) layer.link=0; }
        else {
            const int group=++nextLink_;
            for (auto& layer : layers_) if(layer.path==target.path) layer.link=group;
        }
        refresh(); if (changed) changed();
    }
    void importMedia(const QString& file, bool initial = false, framescale::ProcessingOptions legacy = {}) {
        const auto generation = generation_; ++pendingImports_;
        auto* probe = new QProcess(this);
        auto* timeout = new QTimer(probe); timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, probe, &QProcess::kill); timeout->start(15000);
        connect(probe, &QProcess::errorOccurred, this, [this,probe,generation](QProcess::ProcessError error) {
            if(error==QProcess::FailedToStart) { if(generation==generation_) { --pendingImports_; if(status) status(probe->errorString()); } probe->deleteLater(); }
        });
        connect(probe, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
            [this,probe,timeout,file,generation,initial,legacy](int code,QProcess::ExitStatus exit) {
            timeout->stop(); probe->deleteLater(); if (generation!=generation_) return; --pendingImports_;
            const auto object=QJsonDocument::fromJson(probe->readAllStandardOutput()).object();
            const double duration=object.value("format").toObject().value("duration").toVariant().toDouble();
            if(code || exit!=QProcess::NormalExit || !std::isfinite(duration) || duration<=0) {
                if(status) status(tr("Não foi possível abrir a camada: %1").arg(file)); return;
            }
            const int link=++nextLink_; std::vector<Layer> imported;
            bool video=false,audio=false;
            for(const auto entry:object.value("streams").toArray()) {
                const auto stream=entry.toObject(); const auto type=stream.value("codec_type").toString();
                if(type!="video" && type!="audio") continue;
                if(type=="video" && stream.value("disposition").toObject().value("attached_pic").toInt()) continue;
                if((type=="video" && video) || (type=="audio" && audio)) continue;
                Layer layer;
#ifdef _WIN32
                layer.path=std::filesystem::path(file.toStdWString());
#else
                layer.path=std::filesystem::path(file.toStdString());
#endif
                layer.kind=type=="video" ? Kind::Video : Kind::Audio;
                layer.sourceDuration=duration; layer.out=duration; layer.link=link;
                if(layer.kind==Kind::Audio && stream.value("profile").toString()=="HE-AACv2") {
                    const double samples=stream.value("nb_frames").toString().toDouble()*2048.;
                    const double rate=stream.value("sample_rate").toString().toDouble();
                    const double span=stream.value("duration").toString().toDouble();
                    if(rate>0 && samples>0 && span>0 && samples/rate<span*.8)
                        layer.audioTimeScale=span/(samples/rate);
                }
                layer.width=stream.value("width").toInt(); layer.height=stream.value("height").toInt();
                for(const auto side:stream.value("side_data_list").toArray())
                    if(std::abs(side.toObject().value("rotation").toInt())%180==90) std::swap(layer.width,layer.height);
                const auto rate=stream.value("avg_frame_rate").toString().split('/');
                if(rate.size()==2 && rate[1].toDouble()>0) layer.sourceFps=rate[0].toDouble()/rate[1].toDouble();
                if(std::abs(layer.sourceFps-std::round(layer.sourceFps))<.01) layer.sourceFps=std::round(layer.sourceFps);
                if(!std::isfinite(layer.sourceFps) || layer.sourceFps<=0) layer.sourceFps=25;
                if(initial) {
                    layer.in=legacy.trimStartSeconds;
                    layer.out=legacy.trimEndSeconds>0 ? std::min(duration,legacy.trimEndSeconds) : duration;
                    layer.speed=legacy.playbackSpeed;
                    layer.start=legacy.videoTimelineStartMs/1000.;
                    if(layer.kind==Kind::Audio) layer.start=std::max(0.,layer.start+legacy.audioOffsetMs/1000.);
                    // A new import starts linked; saved layered projects retain their state.
                }
                if(layer.kind==Kind::Video) video=true; else audio=true;
                imported.push_back(layer);
            }
            if(!(video && audio)) for(auto& layer:imported) layer.link=0;
            if(imported.empty()) { if(status) status(tr("O ficheiro não contém vídeo nem áudio.")); return; }
            if(!initial) remember();
            // New visual layers appear above the existing stack.
            layers_.insert(layers_.begin(),imported.begin(),imported.end()); selected_=-1;
            for(const auto& layer:imported) loadThumbnail(layer);
            projectExtent_=std::max(projectExtent_,framescale::compositionDuration(layers_));
            initialized_=true; edited();
        });
        probe->start(framescale::RuntimePaths().tool("ffprobe"), {"-v","error","-show_streams","-show_format","-of","json",file});
    }
    QString key(const Layer& layer) const { return framescale::layerPath(layer.path)+(layer.kind==Kind::Audio ? "#audio" : "#video"); }
    struct ThumbnailTask { Layer layer; QString id; qint64 frame=-1; };
    void loadThumbnail(const Layer& layer) {
        if(layer.kind==Kind::Video) {
            for(int i=0;i<16;++i) queueVideoThumbnail(layer,layer.sourceDuration*i/16.);
            return;
        }
        const auto id=key(layer); if(thumbnails_.contains(id)) return;
        thumbnails_[id]={}; thumbnailQueue_.push_back({layer,id,-1}); startThumbnail();
    }
    QString queueVideoThumbnail(const Layer& layer,double seconds) {
        const qint64 frame=std::clamp(qRound64(seconds*layer.sourceFps),qint64(0),std::max<qint64>(0,qFloor(layer.sourceDuration*layer.sourceFps)-1));
        const QString id=key(layer)+QStringLiteral("#%1").arg(frame);
        if(!videoFrames_.contains(id) && !pendingFrames_.contains(id) && thumbnailQueue_.size()<128) {
            pendingFrames_.insert(id); thumbnailQueue_.push_back({layer,id,frame});
            QTimer::singleShot(0,this,[this]{startThumbnail();});
        }
        return id;
    }
    QImage videoThumbnail(const Layer& layer,double sourceTime) {
        // Sample the source time represented by this cell, including trim and speed.
        // Zoom requests finer samples without stretching a single picture over the clip.
        const double step=std::max(1./layer.sourceFps,layer.sourceDuration/(16.*tracks_->zoomFactor()));
        const auto id=queueVideoThumbnail(layer,std::floor(sourceTime/step)*step);
        if(const auto* frame=videoFrames_.object(id)) return *frame;
        return {};
    }
    void startThumbnail() {
        if(thumbnailRunning_ || thumbnailQueue_.empty()) return;
        const auto task=thumbnailQueue_.front(); thumbnailQueue_.erase(thumbnailQueue_.begin());
        const auto layer=task.layer; const auto id=task.id;
        const auto generation=generation_; thumbnailRunning_=true;
        auto* process=new QProcess(this); const auto file=framescale::layerPath(layer.path);
        auto* timeout=new QTimer(process); timeout->setSingleShot(true);
        connect(timeout,&QTimer::timeout,process,&QProcess::kill); timeout->start(30000);
        QStringList args{"-v","error","-nostdin","-threads","1","-filter_threads","1"};
        if(task.frame>=0) args << "-ss" << QString::number(task.frame/layer.sourceFps,'f',9);
        args << "-i" << file;
        if(layer.kind==Kind::Audio) args << "-filter_complex_threads" << "1" << "-filter_complex" << "[0:a:0]aresample=1000,aformat=channel_layouts=mono,showwavespic=s=960x44:colors=0x168bff[p]" << "-map" << "[p]";
        else args << "-map" << "0:v:0" << "-vf" << "scale=160:90:force_original_aspect_ratio=decrease,pad=160:90:(ow-iw)/2:(oh-ih)/2";
        args << "-frames:v" << "1" << "-threads" << "1" << "-c:v" << "png" << "-f" << "image2pipe" << "pipe:1";
        connect(process,&QProcess::readyReadStandardError,process,[process]{process->readAllStandardError();});
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,timeout,generation,task](int code,QProcess::ExitStatus) {
            timeout->stop();
            if(generation==generation_) {
                const QImage image=code==0 ? QImage::fromData(process->readAllStandardOutput(),"PNG") : QImage();
                if(task.frame<0) thumbnails_[task.id]=image;
                else {
                    videoFrames_.insert(task.id,new QImage(image),std::max(1,int(image.sizeInBytes()/1024)));
                    pendingFrames_.remove(task.id);
                }
                tracks_->update();
            }
            process->deleteLater(); thumbnailRunning_=false; startThumbnail();
        });
        connect(process,&QProcess::errorOccurred,this,[this,process,task](QProcess::ProcessError error) {
            if(error==QProcess::FailedToStart) {
                pendingFrames_.remove(task.id);
                if(task.frame>=0) videoFrames_.insert(task.id,new QImage,1);
                process->deleteLater(); thumbnailRunning_=false; startThumbnail();
            }
        });
        process->start(framescale::RuntimePaths().tool("ffmpeg"),args);
    }
    void cancelReaders() {
        thumbnailQueue_.clear(); thumbnailRunning_=false; pendingFrames_.clear(); videoFrames_.clear();
        for(auto* process:findChildren<QProcess*>(QString(),Qt::FindDirectChildrenOnly)) {
            if(process==render_) continue;
            process->disconnect(this); process->setParent(nullptr);
            if(process->state()==QProcess::NotRunning) process->deleteLater();
            else { connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,&QObject::deleteLater); process->kill(); }
        }
    }
    void cancelRender() {
        if(!render_) return;
        auto* process=render_; render_=nullptr; process->disconnect(this); process->setParent(nullptr);
        const auto directory=pendingPreview_;
        connect(process,&QObject::destroyed,qApp,[directory]{});
        if(process->state()==QProcess::NotRunning) process->deleteLater();
        else { connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,&QObject::deleteLater); process->kill(); }
        pendingPreview_.reset();
    }
    void renderPreview() {
        if(!active_ || pendingImports_) return;
        cancelRender();
        if(layers_.empty()) { if(previewReady) previewReady({}); return; }
        pendingPreview_=std::make_shared<QTemporaryDir>();
        const QString output=pendingPreview_->filePath("layers.mp4");
        const auto plan=framescale::composeLayers(layers_,output,true);
        if(!plan.error.isEmpty()) { if(status) status(plan.error); if(previewFailed) previewFailed(plan.error); return; }
        if(status) status(tr("Atualizando prévia das camadas…"));
        auto* process=new QProcess(this); render_=process;
        auto diagnostics=std::make_shared<QByteArray>();
        connect(process,&QProcess::readyReadStandardError,this,[process,diagnostics] {
            *diagnostics=(*diagnostics+process->readAllStandardError()).right(4000);
        });
        connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,output,diagnostics](int code,QProcess::ExitStatus exit) {
            render_=nullptr; process->deleteLater();
            if(code || exit!=QProcess::NormalExit) {
                const auto message=tr("Falha na prévia das camadas: %1").arg(QString::fromUtf8(*diagnostics));
                if(status) status(message);
                if(previewFailed) previewFailed(message);
                return;
            }
            const auto previous = displayedPreview_;
            if(previewReady) previewReady(output);
            // Let Qt retire the old decoder before releasing its input file.
            QTimer::singleShot(1000, this, [previous] {});
            displayedPreview_=std::move(pendingPreview_);
            if(status) status(tr("Pronto"));
        });
        connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error) {
            if(error==QProcess::FailedToStart) {
                render_=nullptr; process->deleteLater();
                const auto message=tr("Não foi possível iniciar a prévia das camadas.");
                if(status) status(message);
                if(previewFailed) previewFailed(message);
            }
        });
        QTimer::singleShot(120000,process,[process] { if(process->state()!=QProcess::NotRunning) process->kill(); });
        process->start(framescale::RuntimePaths().tool("ffmpeg"),plan.arguments);
    }
    void paintTracks(QPainter& p) {
        const auto background=palette().color(QPalette::Window),accent=palette().color(QPalette::Highlight);
        p.save();
        const double workLeft=x(workStart_),workRight=x(workEnd_>0 ? workEnd_ : extent());
        p.setPen(Qt::NoPen); p.setBrush(QColor(0,122,255,65));
        p.drawRect(QRectF(workLeft,37,workRight-workLeft,7));
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(workLeft-4,35,8,12),2,2);
        p.drawRoundedRect(QRectF(workRight-4,35,8,12),2,2);
        p.restore();
        const int cells=std::max(1,int(std::ceil(tracks_->contentWidth()/90.)));
        const double cellWidth=tracks_->contentWidth()/cells;
        for(int i=0;i<int(layers_.size());++i) {
            const auto& l=layers_[i]; const auto rect=clip(i);
            const QRectF button(24,rect.top()+15,16,16);
            const auto ink=palette().color(QPalette::WindowText);
            ControlSymbols::paint(p,button,l.kind==Kind::Audio ? ControlSymbols::Symbol::Audio : ControlSymbols::Symbol::Eye,ink);
            if(!l.enabled) { p.setPen(QPen(ink,1.5)); p.drawLine(button.topLeft(),button.bottomRight()); }

            p.save(); QPainterPath border; border.addRoundedRect(rect,7,7); p.setClipPath(border,Qt::IntersectClip);
            if(l.kind==Kind::Video) p.fillRect(rect,QColor("#25272b"));
            if(l.kind==Kind::Audio) {
                const auto image=thumbnails_.value(key(l));
                if(!image.isNull()) {
                    const QRectF source(image.width()*l.in/l.sourceDuration,0,image.width()*(l.out-l.in)/l.sourceDuration,image.height());
                    p.drawImage(rect,image,source);
                }
            } else for(int cell=0;cell<cells;++cell) {
                const QRectF target(tracks_->contentLeft()+cell*cellWidth,rect.top(),cellWidth,rect.height());
                if(!target.intersects(rect)) continue;
                const double time=std::clamp(l.in+(tracks_->timeAt(target.center().x())-l.start)*l.speed,l.in,l.out);
                const auto image=videoThumbnail(l,time);
                if(!image.isNull()) p.drawImage(target,image);
                p.setPen(QColor("#161616")); p.drawLine(target.topRight(),target.bottomRight());
            }
            if(isLayerSelected(i)) { QColor color=accent; color.setAlpha(45); p.fillRect(rect,color); }
            if(!l.enabled) p.fillRect(rect,QColor(24,24,24,150));
            p.restore();
            if(isLayerSelected(i)) {
                p.setBrush(Qt::NoBrush); p.setPen(QPen(accent,1)); p.drawRoundedRect(rect,7,7);
                p.fillRect(QRectF(rect.left()-1,rect.top()-2,2,51),accent);
                p.fillRect(QRectF(rect.right()-1,rect.top()-2,2,51),accent);
            }
        }
        if(layers_.empty()) { p.setPen(palette().color(QPalette::WindowText)); p.drawText(QRectF(18,56,tracks_->width()-36,47),Qt::AlignCenter,tr("Adicione ou arraste vídeo e áudio para criar camadas")); }
    }
    void seekFromMouse(double pixel) {
        position_=std::clamp(tracks_->timeAt(pixel),0.,extent());
        updateRange(); tracks_->update();
        if(seekRequested) seekRequested(position_);
    }
    void press(QMouseEvent* event) {
        if(event->button()!=Qt::LeftButton) return;
        if(event->position().x()<tracks_->contentLeft()-8 && event->position().y()>=56) {
            toggleLayerEnabled(rowAt(event->position().y())); return;
        }
        if(event->position().y()>=35 && event->position().y()<=49) {
            const double left=x(workStart_),right=x(workEnd_>0 ? workEnd_ : extent());
            if(std::min(std::abs(event->position().x()-left),std::abs(event->position().x()-right))<=10) {
                dragExtent_=extent(); drag_=std::abs(event->position().x()-left)<std::abs(event->position().x()-right) ? 4 : 5;
                return;
            }
        }
        if(event->position().y()<54) { selectLayer(-1); seeking_=true; seekFromMouse(event->position().x()); return; }
        const int index=rowAt(event->position().y());
        if(index<0 || index>=int(layers_.size())) { selectLayer(-1); return; }
        selected_=index; refresh();
        const auto rect=clip(index);
        if(!rect.adjusted(-12,-3,12,3).contains(event->position())) return;
        dragExtent_=extent(); before_=layers_; pressX_=event->position().x(); pressScale_=pixelsPerSecond();
        drag_=std::min(std::abs(pressX_-rect.left()),std::abs(pressX_-rect.right()))<=12 ? (std::abs(pressX_-rect.left())<=std::abs(pressX_-rect.right()) ? 1 : 2) : 3;
    }
    void move(QMouseEvent* event) {
        if(seeking_) { seekFromMouse(event->position().x()); return; }
        if(!drag_) {
            if(event->position().y()>=35 && event->position().y()<=49 &&
                std::min(std::abs(event->position().x()-x(workStart_)),std::abs(event->position().x()-x(workEnd_>0 ? workEnd_ : extent())))<=10) {
                tracks_->setCursor(Qt::SizeHorCursor); tracks_->setToolTip(tr("Intervalo de exportacao")); return;
            }
            const int index=rowAt(event->position().y());
            if(index>=0 && event->position().x()<tracks_->contentLeft()-8) {
                tracks_->setCursor(Qt::PointingHandCursor);
                tracks_->setToolTip(layers_[index].kind==Kind::Audio ? tr("Clique: ativar/silenciar. Botao direito: volume.") : tr("Mostrar/ocultar camada")); return;
            }
            const bool edge=index>=0 && (std::abs(event->position().x()-clip(index).left())<=12 || std::abs(event->position().x()-clip(index).right())<=12);
            tracks_->setCursor(edge ? Qt::SizeHorCursor : Qt::ArrowCursor);
            tracks_->setToolTip(index>=0 ? (layers_[index].kind==Kind::Audio ? tr("Áudio · ") : tr("Vídeo · "))+QFileInfo(framescale::layerPath(layers_[index].path)).fileName() : QString());
            return;
        }
        if(drag_==4 || drag_==5) {
            const double time=std::clamp(tracks_->timeAt(event->position().x()),0.,extent());
            if(drag_==4) workStart_=std::min(time,(workEnd_>0 ? workEnd_ : extent())-.04);
            else workEnd_=std::max(time,workStart_+.04);
            tracks_->setCursor(Qt::SizeHorCursor); tracks_->update(); return;
        }
        if(!hasSelection()) return;
        tracks_->setCursor(drag_==1 || drag_==2 ? Qt::SizeHorCursor : Qt::ArrowCursor);
        const auto& old=before_[selected_];
        const double delta=(event->position().x()-pressX_)/pressScale_;
        if(drag_==1 || drag_==2) {
            double minimum=-std::numeric_limits<double>::infinity();
            double maximum=std::numeric_limits<double>::infinity();
            for(int i=0;i<int(before_.size());++i) {
                const auto& item=before_[i];
                if(i!=selected_ && (!old.link || item.link!=old.link)) continue;
                const double step=std::min(item.out-item.in,1./std::max(1.,item.sourceFps));
                if(drag_==1) {
                    minimum=std::max(minimum,std::max(-item.in/item.speed,-item.start));
                    maximum=std::min(maximum,(item.out-item.in-step)/item.speed);
                } else {
                    minimum=std::max(minimum,(item.in+step-item.out)/item.speed);
                    maximum=std::min(maximum,(item.sourceDuration-item.out)/item.speed);
                }
            }
            const double movement=std::clamp(delta,minimum,maximum);
            for(int i=0;i<int(layers_.size());++i) {
                const auto& item=before_[i];
                if(i!=selected_ && (!old.link || item.link!=old.link)) continue;
                if(drag_==1) { layers_[i].in=item.in+movement*item.speed; layers_[i].start=item.start+movement; }
                else layers_[i].out=item.out+movement*item.speed;
            }
        }
        else {
            double minimum=-old.start, maximum=dragExtent_-old.start-old.duration();
            if(old.link) for(const auto& item:before_) if(item.link==old.link) {
                minimum=std::max(minimum,-item.start);
                maximum=std::min(maximum,dragExtent_-item.start-item.duration());
            }
            const double movement=std::clamp(delta,minimum,std::max(minimum,maximum));
            for(int i=0;i<int(layers_.size());++i)
                if(i==selected_ || (old.link && before_[i].link==old.link)) layers_[i].start=before_[i].start+movement;
        }
        tracks_->update();
    }
    void release() {
        seeking_=false;
        if(!drag_) return;
        const int finished=drag_; drag_=0; tracks_->unsetCursor();
        if(finished==4 || finished==5) { if(changed) changed(); return; }
        const bool identical=layers_.size()==before_.size() && std::equal(layers_.begin(),layers_.end(),before_.begin(),[](const Layer& a,const Layer& b) {
            return a.path==b.path && a.start==b.start && a.in==b.in && a.out==b.out && a.speed==b.speed && a.link==b.link && a.enabled==b.enabled;
        });
        if(identical) return;
        undo_.push_back(before_); if(undo_.size()>100) undo_.erase(undo_.begin()); redo_.clear(); edited();
    }
    void context(const QPoint& local,const QPoint& global) {
        const int index=rowAt(local.y());
        if(index>=0 && local.x()<tracks_->contentLeft()-8 && layers_[index].kind==Kind::Audio
            && !QApplication::keyboardModifiers().testFlag(Qt::ControlModifier) && volumeRequested) {
            volumeRequested(global); return;
        }
        if(index>=0) selected_=index;
        refresh(); SelectionMenu menu(this);
        auto* add=menu.addAction(ControlSymbols::icon(ControlSymbols::Symbol::Plus),tr("Adicionar camada…"));
        connect(add,&QAction::triggered,this,[this]{chooseMedia();});
        if(hasSelection()) {
            const bool linked=layers_[selected_].link!=0;
            auto* link=menu.addAction(ControlSymbols::icon(linked ? ControlSymbols::Symbol::Unlink : ControlSymbols::Symbol::Link),linked ? tr("Separar áudio e vídeo") : tr("Juntar \u00e1udio e v\u00eddeo"));
            link->setEnabled(hasLinkPartner());
            connect(link,&QAction::triggered,this,[this]{toggleLink();});
            auto* remove=menu.addAction(ControlSymbols::icon(ControlSymbols::Symbol::Trash),tr("Excluir camada"));
            connect(remove,&QAction::triggered,this,[this]{removeSelected();});

        }
        menu.exec(global);
    }
    Tracks* tracks_;
    std::vector<Layer> layers_,before_;
    std::vector<ThumbnailTask> thumbnailQueue_;
    QCache<QString,QImage> videoFrames_{16384}; // 16 MiB, independent of clip length.
    QSet<QString> pendingFrames_;
    bool thumbnailRunning_=false;
    std::vector<std::vector<Layer>> undo_,redo_;
    QHash<QString,QImage> thumbnails_;
    int selected_=-1,nextLink_=0,pendingImports_=0,drag_=0;
    bool active_=false, initialized_=false, seeking_=false;
    double workStart_=0,workEnd_=0,projectExtent_=1,position_=0,pressX_=0,pressScale_=1,dragExtent_=1;
    quint64 generation_=0;
    QTimer renderTimer_; QProcess* render_=nullptr;
    std::shared_ptr<QTemporaryDir> pendingPreview_,displayedPreview_;
};
