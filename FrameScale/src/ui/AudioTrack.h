#pragma once
#include "TrimTimeline.h"
#include "processing/RuntimePaths.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QProcess>
#include <QTimer>
#include <cstring>

class AudioTrack final : public QWidget {
public:
    explicit AudioTrack(TrimTimeline* timeline, QWidget* parent = nullptr) : QWidget(parent), timeline_(timeline) {
        setObjectName("audioTrack"); setFixedHeight(50);
        setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
        timer_.setInterval(80); connect(&timer_, &QTimer::timeout, this, [this] { if (isVisible()) update(); }); timer_.start();
    }
    ~AudioTrack() override { clear(); }
    std::function<void(int)> offsetEdited;
    std::function<void(bool)> linkEdited;
    std::function<void(double)> seekRequested;
    std::function<void()> editStarted, editFinished;
    std::function<void(bool)> selectedChanged;
    std::function<void(const QPoint&)> menuRequested;
    void setOffset(int ms) { offset_ = std::clamp(ms, -600000, 600000); update(); }
    void setLinked(bool linked) { linked_ = linked; update(); }
    bool linked() const { return linked_; }
    void setSelected(bool value) { selected_ = value; setProperty("selected", value); update(); }
    void clear() {
        if (process_) { disconnect(process_, nullptr, this, nullptr); process_->kill(); process_->waitForFinished(1000); delete process_; process_ = nullptr; }
        setProperty("waveformReady", false); waveform_.clear(); tail_.clear(); samples_ = 0; duration_ = 0; setOffset(0); setLinked(true); selected_=false; dragging_=false; hide();
    }
    void load(const QString& path, double duration, bool visible, bool continuous = false) {
        const int offset = offset_; const bool linked = this->linked();
        clear(); setOffset(offset); setLinked(linked); duration_ = std::max(.001, duration);
        waveform_.fill(0.f, 2400); setVisible(visible);
        process_ = new QProcess(this); auto* process = process_;
        connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
            tail_ += process->readAllStandardOutput();
            const int count = tail_.size() / int(sizeof(float));
            for (int i=0; i<count; ++i) {
                float value; std::memcpy(&value, tail_.constData()+i*sizeof(float), sizeof(float));
                const int bin = int(double(samples_++) / 48000. / duration_ * waveform_.size());
                if (bin >= 0 && bin < waveform_.size() && std::isfinite(value)) waveform_[bin] = std::max(waveform_[bin], std::abs(value));
            }
            tail_.remove(0, count * sizeof(float)); update();
        });
        connect(process, &QProcess::finished, this, [this, process](int code) { setProperty("waveformReady", code == 0); process->readAllStandardError(); update(); });
        process->start(framescale::RuntimePaths().tool("ffmpeg"), {"-v","error","-nostdin","-threads","1","-i",path,"-map","0:a:0","-vn","-af",continuous ? "aresample=48000" : "aresample=48000:async=1:first_pts=0","-ac","1","-ar","48000","-f","f32le","pipe:1"});
    }
protected:
    void showEvent(QShowEvent*) override { timeline_->setAudioExpanded(true); }
    void hideEvent(QHideEvent*) override { timeline_->setAudioExpanded(false); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen); p.setBrush(palette().color(QPalette::Window)); p.drawRect(rect().adjusted(8,0,-8,0));
        const QRectF track(18, 2, width()-36, 46); p.setClipRect(track);
        if (selected_ || hovered_) { QColor shade=palette().color(QPalette::Highlight); shade.setAlpha(selected_ ? 65 : 28); p.fillRect(track,shade); }
        if (selected_) { p.setBrush(Qt::NoBrush); p.setPen(QPen(palette().color(QPalette::Highlight), 1)); p.drawRect(track.adjusted(0,0,-1,-1)); }
        const QColor accent = palette().color(QPalette::Highlight); p.setPen(QPen(accent, 1));
        const double span = timeline_->visibleDuration(), start = timeline_->visibleStart(), shift=timeline_->movingVideo() ? timeline_->audioStart() : timeline_->videoStart()+offset_/1000.;
        const double maxPeak = waveform_.isEmpty() ? 1. : std::max(.001f, *std::max_element(waveform_.begin(),waveform_.end()));
        for (int px=18; px<width()-18; ++px) {
            const double from=(start+(px-18.)/std::max(1,width()-36)*span-shift)/duration_;
            const double to=(start+(px-17.)/std::max(1,width()-36)*span-shift)/duration_;
            if (duration_<=0 || to<0 || from>=1 || waveform_.isEmpty()) continue;
            const int a=std::clamp(int(std::floor(from*waveform_.size())),0,int(waveform_.size())-1);
            const int b=std::clamp(int(std::ceil(to*waveform_.size())),a+1,int(waveform_.size()));
            const double peak=*std::max_element(waveform_.begin()+a,waveform_.begin()+b)/maxPeak*19;
            p.drawLine(QPointF(px,27-peak),QPointF(px,27+peak));
        }
        const double cursor=timeline_->videoStart()+timeline_->property("currentFrame").toDouble()/timeline_->property("sourceFrameRate").toDouble();
        const double x=18+(cursor-start)/span*(width()-36); p.drawLine(QPointF(x,4),QPointF(x,50));
    }
    void contextMenuEvent(QContextMenuEvent* e) override { if (!selected_ && selectedChanged) selectedChanged(false); if (menuRequested) menuRequested(e->globalPos()); e->accept(); }
    void enterEvent(QEnterEvent*) override { hovered_=true; update(); }
    void leaveEvent(QEvent*) override { hovered_=false; update(); }
    void wheelEvent(QWheelEvent* e) override { QApplication::sendEvent(timeline_, e); }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button()!=Qt::LeftButton) return;
        setFocus(); if (selectedChanged) selectedChanged(e->modifiers().testFlag(Qt::ControlModifier));
        if (e->modifiers().testFlag(Qt::ControlModifier)) return;
        if (linked()) { if (seekRequested) seekRequested(std::max(0.,timeline_->visibleStart()+(e->position().x()-18)/std::max(1,width()-36)*timeline_->visibleDuration()-timeline_->videoStart())); return; }
        if (editStarted) editStarted();
        dragging_=true; pressX_=e->position().x(); initialOffset_=offset_; setCursor(Qt::ArrowCursor);
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (!dragging_) return;
        int next=initialOffset_+qRound((e->position().x()-pressX_)/std::max(1,width()-36)*timeline_->visibleDuration()*1000);
        // Shift snaps to the video origin within a constant screen-space distance.
        const double tolerance=12.*timeline_->visibleDuration()*1000/std::max(1,width()-36);
        if (e->modifiers().testFlag(Qt::ShiftModifier) && std::abs(next)<=tolerance) next=0;
        setOffset(next);
    }
    void mouseReleaseEvent(QMouseEvent*) override {
        const bool wasDragging=dragging_; const bool changed=dragging_ && offset_!=initialOffset_; dragging_=false;
        unsetCursor(); if (changed && offsetEdited) offsetEdited(offset_);
        if (wasDragging && editFinished) editFinished();
    }
private:
    TrimTimeline* timeline_; bool linked_=true, selected_=false, hovered_=false; int offset_=0; QProcess* process_=nullptr;
    QTimer timer_; QVector<float> waveform_; QByteArray tail_; qint64 samples_=0;
    double duration_=0,pressX_=0; int initialOffset_=0; bool dragging_=false;
};
