#pragma once
#include <QImage>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVector>
#include <QWidget>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
class TrimTimeline : public QWidget {
public:
    explicit TrimTimeline(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("trimTimeline");
        setAutoFillBackground(false);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        setAttribute(Qt::WA_StyledBackground, false);
        setMinimumHeight(160);
        setMaximumHeight(160);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName(tr("Timeline de corte"));
        setMouseTracking(true);
        scroll_ = new QScrollBar(Qt::Horizontal, this);
        scroll_->setObjectName("timelineScroll");
        scroll_->setAccessibleName(tr("Navegar na timeline ampliada"));
        connect(scroll_, &QScrollBar::valueChanged, this, [this](int value) {
            viewStart_ = value / 1000000. * std::max(0., extent() - visibleDuration());
            updateView();
        });
        updateView();
    }
    void setThumbnails(const QVector<QImage>& frames)
    {
        thumbnails_ = frames;
        setProperty("thumbnailCount", std::count_if(frames.cbegin(), frames.cend(), [](const QImage& image) { return !image.isNull(); }));
        update();
    }
    void setSeekingOnly(bool enabled)
    {
        seekingOnly_ = enabled;
        setAttribute(Qt::WA_OpaquePaintEvent, !enabled);
        if (enabled) {
            setAccessibleName(tr("Timeline da comparação"));
            setToolTip(QString());
        }
        update();
    }
    // Layer editing reuses this timeline's original frame, ruler, footer and navigation.
    std::function<void(QPainter&)> paintLayerTracks;
    void setLayerRows(int rows) { layerRows_=std::max(1,rows); setFixedHeight(160+50*(layerRows_-1)); updateView(); }
    double contentLeft() const { return paintLayerTracks ? 52. : 18.; }
    double contentWidth() const { return std::max(1.,width()-contentLeft()-18.); }
    double timeAt(double pixel) const { return seconds(pixel); }
    double pixelAt(double time) const { return x(time); }
    std::function<void()> editStarted, editFinished;
    std::function<void(double)> videoMoved;
    std::function<void(bool)> selectedChanged;
    std::function<void(const QPoint&)> menuRequested;
    void setTrackMode(bool enabled) { trackMode_=enabled; update(); }
    void setTracksLinked(bool linked) { tracksLinked_=linked; }
    void setSelected(bool value) { selected_=value; setProperty("selected",value); update(); }
    bool movingVideo() const { return drag_==6; }
    double audioStart() const { return audioStart_; }
    double videoStart() const { return videoStart_; }
    void setPlacement(double video, double audio) { videoStart_=std::max(0.,video); audioStart_=audio; updateView(); }
    void setEmbeddedAudio(QWidget* audio) {
        audio_=audio; audio_->setParent(this); setFixedHeight(audio_->isVisible() ? 210 : 160); layoutAudio(); update();
    }
    void setAudioExpanded(bool visible) { setFixedHeight(visible ? 210 : 160); scroll_->move(22,visible ? 190 : 140); update(); }
    void setPlaybackSpeed(double value) { playbackSpeed_=value; update(); }
    double zoomFactor() const { return zoom_; }
    double visibleStart() const { return viewStart_; }
    double visibleDuration() const { return extent() / zoom_; }
    void resetView() { zoom_ = 1.; viewStart_ = 0.; updateView(); }
    void setZoom(double value, double anchorPixel = -1.)
    {
        if (drag_ || !std::isfinite(value)) return;
        if (anchorPixel < 0.)
            anchorPixel = position_+videoStart_ >= viewStart_ && position_+videoStart_ <= viewStart_ + visibleDuration()
                ? x(position_) : width() / 2.;
        const double fraction = std::clamp((anchorPixel - contentLeft()) / contentWidth(), 0., 1.);
        const double anchor = viewStart_ + fraction * visibleDuration();
        zoom_ = std::clamp(value, 1., maximumZoom());
        viewStart_ = anchor - fraction * visibleDuration();
        updateView();
    }
    std::function<void()> playbackRequested;
    void setFrameRate(double fps)
    {
        fps_ = std::isfinite(fps) && fps > 0 ? fps : 25;
        setProperty("sourceFrameRate", fps_);
        updateView();
    }
    qint64 frameNumber(double seconds) const { return qRound64(std::max(0., seconds) * fps_); }
    qint64 selectionFrameCount() const { return std::max<qint64>(0, frameNumber(end_) - frameNumber(start_)); }
    QString timecode(double seconds) const
    {
        // The clock is wall time; the last field counts frames within that second.
        // Dividing by a rounded nominal FPS drifts for 23.976/29.97 sources.
        const qint64 whole = qFloor(std::max(0., seconds) + 1.e-7);
        const int within = std::clamp(int(frameNumber(seconds) - frameNumber(whole)), 0, qMax(0, qCeil(fps_) - 1));
        QString result = QString("%1:%2").arg(whole % 60, 2, 10, QChar('0')).arg(within, 2, 10, QChar('0'));
        result = QString("%1:").arg(whole / 60 % 60, 2, 10, QChar('0')) + result;
        if (whole >= 3600) result = QString("%1:").arg(whole / 3600, 2, 10, QChar('0')) + result;
        return result;
    }
    void setClipName(const QString& name)
    {
        clipName_ = name;
        update();
    }
    std::function<void(double, double)> rangeEdited;
    bool isScrubbing() const { return drag_ == 3; }
    std::function<void(double)> seekRequested;
    void setRange(double duration, double start, double end, double position)
    {
        const double nextDuration = std::isfinite(duration) ? std::max(.001, duration) : 1.;
        if (std::abs(nextDuration - duration_) > .000001) { zoom_ = 1.; viewStart_ = 0.; }
        duration_ = nextDuration;
        const double step = std::min(1. / fps_, duration_);
        // A decoder update must not overwrite the pointer's in-flight edit.
        if (!drag_) {
            start_ = std::clamp(std::isfinite(start) ? start : 0., 0., duration_ - step);
            end_ = end > 0 && std::isfinite(end) ? std::clamp(end, start_ + step, duration_) : duration_;
            position_ = std::clamp(std::isfinite(position) ? position : 0., 0., duration_);
        }
        setProperty("selectionFrameCount", selectionFrameCount());
        setProperty("currentFrame", frameNumber(position_));
        updateView();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        scroll_->setGeometry(22, 140 + extraTrackHeight(), std::max(1, width() - 44), 10);
        layoutAudio();
    }
    void wheelEvent(QWheelEvent* event) override
    {
        if (drag_) { event->accept(); return; }
        const QPoint delta = event->angleDelta();
        const int amount = delta.y() ? delta.y() : delta.x();
        if (event->modifiers().testFlag(Qt::ControlModifier))
            setZoom(zoom_ * std::pow(2., amount / 240.), event->position().x());
        else {
            const QPoint pixels = event->pixelDelta();
            const double motion = pixels.isNull() ? amount / 120. * visibleDuration() * .1
                : (pixels.x() ? pixels.x() : pixels.y()) / contentWidth() * visibleDuration();
            viewStart_ -= motion;
            updateView();
        }
        event->accept();
    }

    double seconds(double x) const
    {
        return std::clamp(viewStart_ - videoStart_ + (x - contentLeft()) / contentWidth() * visibleDuration(), 0.,
            duration_);
    }
    double x(double sec) const { return contentLeft() + (sec + videoStart_ - viewStart_) / visibleDuration() * contentWidth(); }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        if (!seekingOnly_) p.fillRect(rect(), QColor("#181818"));
        p.setRenderHint(QPainter::Antialiasing);
        const bool light = palette().color(QPalette::Window).lightnessF() > .5;
        const QColor background = palette().color(QPalette::Window);
        const QColor muted = light ? QColor("#72747b") : QColor("#bac0ca");
        const QColor divider = light ? QColor("#c8c9d0") : QColor("#535860");
        const QColor accent = palette().color(QPalette::Highlight);
        const QRectF card = QRectF(rect()).adjusted(8, 3, -8, -10);
        p.setPen(Qt::NoPen);
        for (int spread = 7; spread > 0; --spread) {
            p.setBrush(QColor(0, 0, 0, light ? 4 : 7));
            p.drawRoundedRect(card.translated(0, 3).adjusted(-spread, -spread / 2., spread, spread / 2.), 12 + spread, 12 + spread);
        }
        QPainterPath outline;
        outline.addRoundedRect(card, 12, 12);
        if (paintLayerTracks) {
            // Match the translucent document material; keep media pixels opaque.
            const QImage backdrop=window()->property("glassBackdrop").value<QImage>();
            p.save(); p.setClipPath(outline);
            if(!backdrop.isNull()) { p.setOpacity(.65); p.drawImage(card,backdrop); p.setOpacity(1.); }
            QColor tint=background; tint.setAlpha(light ? 205 : 185);
            p.fillPath(outline,tint); p.restore();
        } else p.fillPath(outline, background);
        p.setClipPath(outline);
        p.setPen(muted);
        auto f = font();
        f.setPixelSize(12);
        p.setFont(f);
        p.setPen(accent);
        p.drawText(QRect(24, 112 + extraTrackHeight(), 135, 28), Qt::AlignVCenter, timecode(position_+videoStart_));
        p.setPen(muted);
        const QString selection = timecode(start_+videoStart_) + "  —  " + timecode(end_+videoStart_)
            + (playbackSpeed_==1. ? QString() : tr("   |   Saída: %1 s").arg((end_-start_)/playbackSpeed_,0,'f',2));
        p.drawText(QRect(166, 112 + extraTrackHeight(), width() - 190, 28), Qt::AlignRight | Qt::AlignVCenter,
            (width() >= 580 ? tr("%1 fotogramas").arg(selectionFrameCount()) + "   ·   " : QString()) + selection);
        p.setPen(divider);
        p.drawLine(24, 108 + extraTrackHeight(), width() - 24, 108 + extraTrackHeight());
        // Frame-aligned, human-sized intervals keep one second at :01:00,
        // rather than distributing arbitrary fractions across the ruler.
        const qint64 totalFrames = std::max<qint64>(1, frameNumber(extent()));
        const int labels = std::max(1, (width() - 36) / 110);
        const qint64 desired = std::max<qint64>(1, qCeil(visibleDuration() * fps_ / labels));
        qint64 step = 1;
        if (desired < fps_) {
            for (qint64 candidate : { 1, 2, 5, 10, 15, 20, 30, 60 }) {
                step = candidate;
                if (step >= desired)
                    break;
            }
        } else {
            double secondsStep = 1;
            for (double candidate : { 1., 2., 5., 10., 15., 30., 60., 120., 300., 600., 1800., 3600. }) {
                secondsStep = candidate;
                if (candidate * fps_ >= desired)
                    break;
            }
            step = std::max<qint64>(1, qRound64(secondsStep * fps_));
        }
        qreal previousLabelRight = -100;
        const qreal endLabelLeft = width() - 110.;
        for (qint64 frame = std::max<qint64>(0, qint64(std::ceil(viewStart_ * fps_ / step)) * step);
             frame <= std::min(totalFrames, frameNumber(viewStart_ + visibleDuration())); frame += step) {
            const double t = frame / fps_;
            const double px = x(std::min(t,extent())-videoStart_);
            p.setPen(muted);
            p.drawLine(QPointF(px, 31), QPointF(px, 37));
            const QRectF label(std::clamp(px - 44., 20., std::max(20., endLabelLeft)), 9, 90, 18);
            if (frame < totalFrames && label.left() >= previousLabelRight + 8
                && (frame == 0 || label.right() + 8 <= endLabelLeft)) {
                p.drawText(label, Qt::AlignCenter, timecode(t));
                previousLabelRight = label.right();
            }
        }
        if (endLabelLeft >= previousLabelRight + 8)
            p.drawText(QRectF(endLabelLeft, 9, 90, 18), Qt::AlignCenter, timecode(std::min(extent(), viewStart_ + visibleDuration())));
        const QRectF track(18, 56, width() - 36, 47);
        p.fillRect(QRectF(contentLeft(), 37, contentWidth(), 7), light ? QColor("#d9dae0") : QColor("#353535"));
        if (paintLayerTracks) {
            p.save();
            p.setClipRect(QRectF(14, 30, width()-28, 77+extraTrackHeight()), Qt::IntersectClip);
            paintLayerTracks(p);
            const double px=x(position_);
            p.setPen(QPen(accent,1.5)); p.drawLine(QPointF(px,32),QPointF(px,106+extraTrackHeight()));
            p.setBrush(accent);
            p.drawPolygon(QPolygonF{QPointF(px-5,31),QPointF(px+5,31),QPointF(px,37)});
            p.restore();
            return;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(light ? QColor("#dfe0e6") : QColor("#282828"));
        p.drawRoundedRect(track, 7, 7);
        p.save();
        QPainterPath filmstrip;
        filmstrip.addRoundedRect(track, 7, 7);
        p.setClipPath(filmstrip, Qt::IntersectClip);
        p.setClipRect(QRectF(x(0),56,x(duration_)-x(0),47),Qt::IntersectClip);
        const int cells = std::max(1, int(std::ceil(track.width() / 90.)));
        for (int i = 0; i < cells; ++i) {
            QRectF cell(track.x() + i * track.width() / cells, track.y(), track.width() / cells, track.height());
            const int index = thumbnails_.isEmpty() ? -1 : std::min(int(thumbnails_.size()) - 1, int(std::clamp(seconds(cell.center().x()) / duration_, 0., 1.) * thumbnails_.size()));
            if (index >= 0 && !thumbnails_[index].isNull())
                p.drawImage(cell, thumbnails_[index]);
            p.setPen(QColor("#161616"));
            p.drawLine(cell.topRight(), cell.bottomRight());
        }
        p.fillRect(QRectF(track.x(), track.y(), std::clamp(x(start_), track.left(), track.right()) - track.x(), track.height()), QColor(0, 0, 0, 160));
        p.fillRect(QRectF(std::clamp(x(end_), track.left(), track.right()), track.y(), track.right() - std::clamp(x(end_), track.left(), track.right()), track.height()), QColor(0, 0, 0, 160));
        p.restore();
        if (selected_ || hovered_) { QColor shade=accent; shade.setAlpha(selected_ ? 65 : 28); p.fillRect(track,shade); }
        if (selected_) { p.setPen(QPen(accent,2)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(track,7,7); }
        p.setClipRect(QRectF(14, 30, width() - 28, 77), Qt::IntersectClip);
        p.setPen(accent);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(x(start_), 56, std::max(1., x(end_) - x(start_)), 47), 7, 7);
        if (!seekingOnly_)
            for (double t : { start_, end_ }) {
                p.setPen(Qt::NoPen); p.setBrush(accent);
                p.drawRect(QRectF(x(t) - 1, 54, 2, 51));
            }
        double px = x(position_);
        p.setPen(QPen(accent, 1.5));
        p.drawLine(QPointF(px, 32), QPointF(px, 106));
        p.setBrush(accent);
        p.drawPolygon(
            QPolygonF { QPointF(px - 5, 31), QPointF(px + 5, 31), QPointF(px, 37) });
    }
    void contextMenuEvent(QContextMenuEvent* e) override { if(!selected_ && selectedChanged) selectedChanged(false); if(menuRequested) menuRequested(e->globalPos()); e->accept(); }
    void leaveEvent(QEvent*) override { hovered_=false; update(); }
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::MiddleButton && e->position().y() < 108) {
            setFocus(); drag_ = 5; pressX_ = e->position().x(); panStart_ = viewStart_;
            setCursor(Qt::ArrowCursor); return;
        }
        if (e->button() != Qt::LeftButton || e->position().y() > 108)
            return;
        setFocus();
        if (e->position().y()>=54 && selectedChanged) selectedChanged(e->modifiers().testFlag(Qt::ControlModifier));
        if (e->modifiers().testFlag(Qt::ControlModifier)) return;
        const double startDistance = qAbs(e->position().x() - x(start_));
        const double endDistance = qAbs(e->position().x() - x(end_));
        const bool handles = !seekingOnly_ && e->position().y() >= 54 && e->position().y() <= 105;
        pressX_ = e->position().x();
        // When handles occupy the same pixels, defer the choice until motion:
        // dragging right expands the end, dragging left expands the start.
        // Otherwise pick the nearest handle, never always the start first.
        if (handles && startDistance < 12 && endDistance < 12 && x(end_) - x(start_) < 8)
            drag_ = 4;
        else if (handles && (startDistance < 12 || endDistance < 12))
            drag_ = startDistance <= endDistance ? 1 : 2;
        else
            drag_ = 3;
        if (drag_ == 3 && trackMode_ && !tracksLinked_ && e->position().y() >= 56) { dragSpan_=visibleDuration(); dragExtent_=extent(); drag_=6; placementAtPress_=videoStart_; }
        if ((drag_==1 || drag_==2 || drag_==4 || drag_==6) && editStarted) editStarted();
        setCursor(Qt::ArrowCursor);
        if (drag_==6) return;
        move(e->position().x(), e->modifiers().testFlag(Qt::ShiftModifier));
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        hovered_=e->position().y()>=56 && e->position().y()<=103; update();
        if (drag_ == 6) {
            setPlacement(std::clamp(placementAtPress_+(e->position().x()-pressX_)/std::max(1,width()-36)*dragSpan_,0.,600.),audioStart_);
        } else if (drag_ == 5) {
            viewStart_ = panStart_ - (e->position().x() - pressX_) / contentWidth() * visibleDuration();
            updateView();
        } else if (drag_) {
            setCursor(drag_==3 ? Qt::ArrowCursor : Qt::SizeHorCursor);
            move(e->position().x(), e->modifiers().testFlag(Qt::ShiftModifier));
        } else
            setCursor(!seekingOnly_ && e->position().y()>=54 && e->position().y()<=105 && (qAbs(e->position().x() - x(start_)) < 12 || qAbs(e->position().x() - x(end_)) < 12)
                    ? Qt::SizeHorCursor
                    : Qt::ArrowCursor);
    }
    void mouseReleaseEvent(QMouseEvent*) override
    {
        const bool edited=drag_==1 || drag_==2 || drag_==4 || drag_==6;
        if (drag_==6 && videoMoved) videoMoved(videoStart_);
        drag_ = 0; updateView();
        if (edited && editFinished) editFinished();
        setCursor(Qt::ArrowCursor);
    }
    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Plus || e->key() == Qt::Key_Equal || e->key() == Qt::Key_Minus || e->key() == Qt::Key_0) {
            if (e->key() == Qt::Key_0) resetView();
            else setZoom(zoom_ * (e->key() == Qt::Key_Minus ? .5 : 2.));
            e->accept(); return;
        }
        if (e->key() == Qt::Key_Space && playbackRequested) {
            if (!e->isAutoRepeat())
                playbackRequested();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) {
            position_ = std::clamp(
                position_ + (e->key() == Qt::Key_Right ? 1. / fps_ : -1. / fps_), 0., duration_);
            if (position_+videoStart_ < viewStart_) viewStart_ = position_+videoStart_;
            else if (position_+videoStart_ > viewStart_ + visibleDuration()) viewStart_ = position_+videoStart_ - visibleDuration();
            updateView();
            if (seekRequested)
                seekRequested(position_);
            update();
            e->accept();
        } else
            QWidget::keyPressEvent(e);
    }

private:
    int layerRows_=1;
    int extraTrackHeight() const { return paintLayerTracks ? (layerRows_-1)*50 : audio_ && audio_->isVisible() ? 50 : 0; }
    double extent() const { if(drag_==6) return dragExtent_; return duration_+videoStart_; }
    void layoutAudio() { if(audio_) audio_->setGeometry(0,106,width(),50); }
    QWidget* audio_=nullptr;
    bool trackMode_=false, tracksLinked_=true, selected_=false, hovered_=false;
    double videoStart_=0., audioStart_=0., placementAtPress_=0., dragSpan_=1., dragExtent_=1.;
    double maximumZoom() const { return std::max(1., duration_ * fps_ / 8.); }
    void updateView()
    {
        zoom_ = std::clamp(zoom_, 1., maximumZoom());
        viewStart_ = std::clamp(viewStart_, 0., std::max(0., extent() - visibleDuration()));
        const QSignalBlocker blockScroll(scroll_);
        const double available = extent() - visibleDuration();
        scroll_->setVisible(available > .0000001);
        scroll_->setRange(0, available > .0000001 ? 1000000 : 0);
        scroll_->setPageStep(available > .0000001 ? qRound(std::min(1000000., visibleDuration() / available * 1000000)) : 1000000);
        scroll_->setSingleStep(std::max(1, scroll_->pageStep() / 10));
        scroll_->setValue(available > .0000001 ? qRound(viewStart_ / available * 1000000) : 0);
        setProperty("timelineZoom", zoom_);
        setProperty("visibleStart", viewStart_);
        update();
    }
    QScrollBar* scroll_ = nullptr;
    double zoom_ = 1., viewStart_ = 0., panStart_ = 0.;
    bool seekingOnly_ = false;
    void move(double px, bool snapToFrame)
    {
        if (drag_ == 4) {
            if (qAbs(px - pressX_) < 1.) return;
            drag_ = px > pressX_ ? 2 : 1;
        }
        double t = std::round(seconds(px) * fps_) / fps_;
        if (snapToFrame) {
            double nearest = 11.;
            const std::array<double, 4> anchors { 0., duration_, drag_ == 3 ? start_ : position_, drag_ == 2 ? start_ : end_ };
            for (double anchor : anchors) {
                const double distance = std::abs(px - x(anchor));
                if (distance < nearest) {
                    nearest = distance;
                    t = std::round(anchor * fps_) / fps_;
                }
            }
        }
        t = std::clamp(t, 0., duration_);
        if (drag_ == 1)
            start_ = std::min(t, std::max(0., end_ - 1. / fps_));
        else if (drag_ == 2)
            end_ = std::max(t, std::min(duration_, start_ + 1. / fps_));
        else {
            position_ = t;
            if (seekRequested)
                seekRequested(t);
        }
        setProperty("selectionFrameCount", selectionFrameCount());
        setProperty("currentFrame", frameNumber(position_));
        if (drag_ != 3 && rangeEdited)
            rangeEdited(start_, end_ >= duration_ ? 0 : end_);
        update();
    }
    double duration_ = 1, start_ = 0, end_ = 1, position_ = 0;
    double playbackSpeed_=1.;
    double fps_ = 25;
    QString clipName_;
    QVector<QImage> thumbnails_;
    int drag_ = 0;
    double pressX_ = 0;
};
