#pragma once
#include "processing/TextLayerRenderer.h"
#include "processing/RuntimePaths.h"
#include <QProcess>
#include <QPointer>
#include <QHash>
#include <QApplication>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTimer>
#include <functional>

class CanvasTextEdit final : public QPlainTextEdit {
public:
    QColor caretColor=Qt::white;
    std::function<void(QPainter&,bool)> paintCanvas;
    std::function<int(const QPoint&)> hitPosition;
    explicit CanvasTextEdit(QWidget* parent):QPlainTextEdit(parent) {
        blink_.setInterval(std::max(100,QApplication::cursorFlashTime()/2));
        connect(&blink_,&QTimer::timeout,this,[this]{caretVisible_=!caretVisible_;viewport()->update();});
        blink_.start();
        connect(this,&QPlainTextEdit::cursorPositionChanged,this,[this]{caretVisible_=true;viewport()->update();});
    }
protected:
    void paintEvent(QPaintEvent* event) override {
        if(paintCanvas) {QPainter painter(viewport());paintCanvas(painter,hasFocus() && caretVisible_);return;}
        QPlainTextEdit::paintEvent(event);
        if(hasFocus() && caretVisible_ && !textCursor().hasSelection()) {
            QPainter painter(viewport());painter.fillRect(cursorRect().adjusted(0,1,0,-1),caretColor);
        }
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(!hitPosition || event->button()!=Qt::LeftButton) {QPlainTextEdit::mousePressEvent(event);return;}
        setFocus(Qt::MouseFocusReason);
        auto cursor=textCursor();cursor.setPosition(hitPosition(event->position().toPoint()),event->modifiers().testFlag(Qt::ShiftModifier)?QTextCursor::KeepAnchor:QTextCursor::MoveAnchor);
        setTextCursor(cursor);dragging_=true;event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(dragging_ && hitPosition && event->buttons().testFlag(Qt::LeftButton)) {
            auto cursor=textCursor();cursor.setPosition(hitPosition(event->position().toPoint()),QTextCursor::KeepAnchor);
            setTextCursor(cursor);event->accept();return;
        }
        QPlainTextEdit::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {dragging_=false;event->accept();}
    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if(!hitPosition || event->button()!=Qt::LeftButton) {QPlainTextEdit::mouseDoubleClickEvent(event);return;}
        auto cursor=textCursor();cursor.setPosition(hitPosition(event->position().toPoint()));cursor.select(QTextCursor::WordUnderCursor);
        setTextCursor(cursor);event->accept();
    }
private:
    QTimer blink_;bool caretVisible_=true;
    bool dragging_=false;
};

// A lightweight view layer: typing repaints glyphs over the current frame.
// Media decoding and playback are independent of text content and typography.
class TextLayerOverlay final : public QWidget {
public:
    std::function<void(int)> selected;
    std::function<void(const QPointF&)> createText;
    std::function<void(int,const std::string&)> contentChanged;
    std::function<void(int,const framescale::LayerTransform&,bool)> transformChanged;
    explicit TextLayerOverlay(QWidget* parent):QWidget(parent) {
        setObjectName("textLayerOverlay");setAttribute(Qt::WA_NoSystemBackground);
        setCursor(Qt::IBeamCursor);setFocusPolicy(Qt::StrongFocus);
        editor_=new CanvasTextEdit(this);editor_->setObjectName("canvasTextEditor");
        editor_->setFrameShape(QFrame::NoFrame);editor_->setLineWrapMode(QPlainTextEdit::NoWrap);
        editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        editor_->document()->setDocumentMargin(0);editor_->hide();
        editor_->hitPosition=[this](const QPoint& point) {
            if(editing_<0) return 0;
            return framescale::textLayerGeometry(layers_[editing_].text).cursorAt(
                textPoint(editor_->viewport()->mapTo(this,point)));
        };
        editor_->paintCanvas=[this](QPainter& painter,bool caret){paintEditor(painter,caret);};
        connect(editor_,&QPlainTextEdit::textChanged,this,[this] {
            if(editing_<0 || editing_>=int(layers_.size())) return;
            const int target=editing_;
            const auto text=editor_->toPlainText().left(10000).toStdString();
            if(contentChanged) contentChanged(target,text);
        });
    }
    ~TextLayerOverlay() override {clearVideoSnapshot();}
    void setLayers(const std::vector<framescale::MediaLayer>& layers,int selection) {
        if(editing_!=selection) finishEditing();
        bool videoTransformChanged=false;
        if(layers.size()!=layers_.size()) clearVideoSnapshot();
        else for(int i=0;i<int(layers.size());++i) {
            const auto& a=layers[i];const auto& b=layers_[i];
            if(a.path!=b.path || a.enabled!=b.enabled || a.kind!=b.kind || a.start!=b.start || a.in!=b.in || a.out!=b.out || a.speed!=b.speed) {clearVideoSnapshot();break;}
            if(a.kind==framescale::LayerKind::Video && !(a.transform==b.transform)) videoTransformChanged=true;
        }
        layers_=layers;selection_=selection;
        if(videoTransformChanged) prepareVideoSnapshot();
        if(editing_>=int(layers_.size()) || (editing_>=0 && layers_[editing_].kind!=framescale::LayerKind::Text)) finishEditing();
        updateLayout();
    }
    void setView(const QRectF& frame,const QSize& canvas,double seconds) {
        if(frame==frame_ && canvas==canvas_ && seconds==seconds_) return;
        if(std::abs(seconds-seconds_)>.0001 || canvas!=canvas_) clearVideoSnapshot();
        bool visibilityChanged=false;
        const double previous=seconds_;
        for(int i=0;i<int(layers_.size());++i) {
            const bool before=visibleVisual(i);seconds_=seconds;
            visibilityChanged|=before!=visibleVisual(i);seconds_=previous;
        }
        if(frame==frame_ && canvas==canvas_ && !visibilityChanged) {seconds_=seconds;if(isHidden() && (creationTool_ || selectionTool_)) updateLayout();return;}
        frame_=frame;canvas_=canvas;seconds_=seconds;updateLayout();
    }
    void finishEditing() {editing_=-1;editor_->hide();}
    void setSelectionTool(bool enabled) {
        selectionTool_=enabled;if(enabled) {creationTool_=false;finishEditing();}
        setCursor(enabled?Qt::ArrowCursor:Qt::IBeamCursor);updateLayout();
    }
    void armTextTool() {finishEditing();creationTool_=true;selectionTool_=false;setCursor(Qt::IBeamCursor);updateLayout();}
    bool beginEditing(int index) {
        if(index<0 || index>=int(layers_.size()) || canvas_.isEmpty() || frame_.isEmpty() || !visibleText(index)) return false;
        if(editing_!=index) {editing_=index;selection_=index;updateLayout();editor_->moveCursor(QTextCursor::End);}
        editor_->setCursor(Qt::IBeamCursor);editor_->viewport()->setCursor(Qt::IBeamCursor);
        editor_->show();editor_->setFocus(Qt::OtherFocusReason);return true;
    }
    QRectF textRect(int index) const {
        if(index<0 || index>=int(layers_.size()) || canvas_.isEmpty()) return {};
        const auto& layer=layers_[index];
        QRectF bounds;
        if(layer.kind==framescale::LayerKind::Text) bounds=framescale::textLayerBounds(layer.text,canvas_.width(),canvas_.height());
        else {
            QSizeF size(layer.width,layer.height);size.scale(canvas_,Qt::KeepAspectRatio);
            const auto t=framescale::layerTransform(layer);QTransform matrix;matrix.translate(canvas_.width()*t.x/100.,canvas_.height()*t.y/100.);
            matrix.rotate(t.rotation);matrix.scale(t.scaleX/100.,t.scaleY/100.);
            bounds=matrix.mapRect(QRectF(QPointF(-size.width()/2,-size.height()/2),size));
        }
        return QRectF(frame_.x()+bounds.x()*frame_.width()/canvas_.width(),
            frame_.y()+bounds.y()*frame_.height()/canvas_.height(),
            bounds.width()*frame_.width()/canvas_.width(),bounds.height()*frame_.height()/canvas_.height());
    }
    QRectF canvasCaretRect() const {
        if(editing_<0 || canvas_.isEmpty()) return {};
        const auto g=framescale::textLayerGeometry(layers_[editing_].text);
        const QPointF point=overlayPoint(g.cursorPoint(editor_->textCursor().position()));
        return QRectF(point,QSizeF(1,std::max(1.,g.lineHeight*textScale().y())));
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if(canvas_.isEmpty() || frame_.isEmpty()) return;
        QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
        painter.setClipRect(frame_.intersected(rect()));
        painter.translate(frame_.topLeft());painter.scale(frame_.width()/canvas_.width(),frame_.height()/canvas_.height());
        const bool composite=videoSnapshotReady();
        if(composite) painter.fillRect(QRect(QPoint(),canvas_),Qt::black);
        for(int i=int(layers_.size())-1;i>=0;--i) {
            if(visibleText(i)) framescale::paintTextLayer(painter,layers_[i].text,canvas_.width(),canvas_.height());
            else if(composite && visibleVisual(i)) {
                const auto& l=layers_[i];const auto t=framescale::layerTransform(l);
                QSizeF size(l.width,l.height);size.scale(canvas_,Qt::KeepAspectRatio);
                painter.save();painter.translate(canvas_.width()*t.x/100.,canvas_.height()*t.y/100.);painter.rotate(t.rotation);
                painter.scale(t.scaleX/100.,t.scaleY/100.);painter.setOpacity(t.opacity/100.);
                painter.drawImage(QRectF(QPointF(-size.width()/2,-size.height()/2),size),videoSnapshots_.value(i));painter.restore();
            }
        }
        if(selectionTool_ && selection_>=0 && selection_<int(layers_.size()) && visibleVisual(selection_)) {
            painter.resetTransform();painter.setBrush(Qt::NoBrush);painter.setPen(QPen(QColor("#70b6ff"),1));
            painter.drawRect(textRect(selection_).adjusted(-3,-3,3,3));
        }
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button()!=Qt::LeftButton) {event->ignore();return;}
        setFocus(Qt::MouseFocusReason);
        for(int i=0;i<int(layers_.size());++i) if((visibleText(i) || (selectionTool_ && visibleVisual(i))) && textRect(i).adjusted(-5,-5,5,5).contains(event->position())) {
            dragIndex_=i;dragOrigin_=event->position();dragTransform_=framescale::layerTransform(layers_[i]);dragMoved_=false;
            if(selected) selected(i);
            if(selectionTool_) {selection_=i;finishEditing();updateLayout();grabMouse();event->accept();return;}
            editing_=i;selection_=i;updateLayout();editor_->show();editor_->setFocus(Qt::MouseFocusReason);
            auto cursor=editor_->textCursor();
            cursor.setPosition(framescale::textLayerGeometry(layers_[i].text).cursorAt(textPoint(event->position())));
            editor_->setTextCursor(cursor);event->accept();return;
        }
        if(creationTool_ && frame_.contains(event->position()) && createText) {
            const QPointF point((event->position().x()-frame_.x())*100./frame_.width(),(event->position().y()-frame_.y())*100./frame_.height());
            createText(point);event->accept();return;
        }
        event->ignore();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(!selectionTool_ || dragIndex_<0 || dragIndex_>=int(layers_.size()) || !event->buttons().testFlag(Qt::LeftButton) || frame_.isEmpty()) {event->ignore();return;}
        const QPointF delta=event->position()-dragOrigin_;
        if(!dragMoved_ && delta.manhattanLength()<QApplication::startDragDistance()) return;
        if(!dragMoved_ && layers_[dragIndex_].kind==framescale::LayerKind::Video) prepareVideoSnapshot();
        dragMoved_=true;auto t=dragTransform_;t.x+=delta.x()*100./frame_.width();t.y+=delta.y()*100./frame_.height();
        t.x=std::clamp(t.x,-1000.,1000.);t.y=std::clamp(t.y,-1000.,1000.);
        framescale::setLayerTransform(layers_[dragIndex_],t);updateLayout();
        setCursor(Qt::ClosedHandCursor);if(transformChanged) transformChanged(dragIndex_,t,false);event->accept();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if(event->button()==Qt::LeftButton && dragIndex_>=0) {
            const int index=dragIndex_;dragIndex_=-1;
            if(mouseGrabber()==this) releaseMouse();
            if(dragMoved_ && index<int(layers_.size()) && transformChanged) transformChanged(index,framescale::layerTransform(layers_[index]),true);
            dragMoved_=false;updateLayout();setCursor(selectionTool_?Qt::ArrowCursor:Qt::IBeamCursor);event->accept();return;
        }
        event->ignore();
    }
private:
    void clearVideoSnapshot() {
        ++snapshotGeneration_;snapshotActive_=false;videoSnapshots_.clear();
        for(auto process:snapshotReaders_) if(process) {process->disconnect(this);process->kill();process->deleteLater();}
        snapshotReaders_.clear();
    }
    bool videoSnapshotReady() const {
        if(!snapshotActive_) return false;
        for(int i=0;i<int(layers_.size());++i) if(visibleVisual(i) && layers_[i].kind==framescale::LayerKind::Video && !videoSnapshots_.contains(i)) return false;
        return true;
    }
    void prepareVideoSnapshot() {
        if(snapshotActive_ || canvas_.isEmpty()) return;
        snapshotActive_=true;const auto generation=snapshotGeneration_;
        for(int i=0;i<int(layers_.size());++i) if(visibleVisual(i) && layers_[i].kind==framescale::LayerKind::Video) {
            const auto& l=layers_[i];auto* process=new QProcess(this);snapshotReaders_.append(process);
            auto* timeout=new QTimer(process);timeout->setSingleShot(true);timeout->start(10000);
            connect(timeout,&QTimer::timeout,process,&QProcess::kill);
            connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,timeout,generation,i](int code,QProcess::ExitStatus status){
                timeout->stop();
                if(generation==snapshotGeneration_ && code==0 && status==QProcess::NormalExit) {
                    const QImage frame=QImage::fromData(process->readAllStandardOutput(),"PNG");
                    if(!frame.isNull()) {videoSnapshots_.insert(i,frame);updateLayout();}
                }
                process->deleteLater();
            });
            connect(process,&QProcess::readyReadStandardError,process,[process]{process->readAllStandardError();});
#ifdef _WIN32
            const auto path=QString::fromStdWString(l.path.wstring());
#else
            const auto path=QString::fromStdString(l.path.string());
#endif
            const double time=l.in+std::max(0.,seconds_-l.start)*l.speed;
            process->start(framescale::RuntimePaths().tool("ffmpeg"),{"-v","error","-nostdin","-threads","1","-ss",QString::number(time,'f',9),"-i",path,
                "-vf","scale=w='min(1920,iw)':h='min(1080,ih)':force_original_aspect_ratio=decrease","-frames:v","1","-threads","1","-c:v","png","-f","image2pipe","pipe:1"});
        }
    }
    QTransform textMatrix() const {
        const auto& t=layers_[editing_].text;const auto g=framescale::textLayerGeometry(t);
        QTransform matrix;matrix.translate(frame_.x(),frame_.y());matrix.scale(frame_.width()/canvas_.width(),frame_.height()/canvas_.height());
        matrix.translate(canvas_.width()*t.x/100.,canvas_.height()*t.y/100.);matrix.rotate(t.rotation);
        matrix.scale(t.horizontalScale/100.,t.verticalScale/100.);matrix.translate(-g.width/2,-g.height/2);return matrix;
    }
    QPointF textScale() const {
        const auto& t=layers_[editing_].text;
        return QPointF(frame_.width()/canvas_.width()*t.horizontalScale/100.,frame_.height()/canvas_.height()*t.verticalScale/100.);
    }
    QPointF overlayPoint(const QPointF& point) const {
        return textMatrix().map(point);
    }
    QPointF textPoint(const QPointF& point) const {
        return textMatrix().inverted().map(point);
    }
    void paintEditor(QPainter& painter,bool caretVisible) const {
        if(editing_<0 || canvas_.isEmpty()) return;
        painter.translate(-editor_->viewport()->mapTo(this,QPoint()));
        const auto cursor=editor_->textCursor();
        const auto& text=layers_[editing_].text;const auto g=framescale::textLayerGeometry(text);const auto scale=textScale();
        if(cursor.hasSelection()) {
            QRegion selected;int position=0;
            for(int row=0;row<g.lines.size();++row) {
                const int begin=std::max(cursor.selectionStart(),position),end=std::min(cursor.selectionEnd(),position+int(g.lines[row].size()));
                if(end>begin) {
                    const auto a=g.cursorPoint(begin),b=g.cursorPoint(end);
                    const QRectF box(QPointF(std::min(a.x(),b.x()),a.y()),QSizeF(std::max(1.,std::abs(b.x()-a.x())),g.lineHeight));
                    const auto polygon=textMatrix().map(QPolygonF(box));
                    painter.setPen(Qt::NoPen);painter.setBrush(QColor("#007aff"));painter.drawPolygon(polygon);selected+=QRegion(polygon.toPolygon());
                }
                position+=g.lines[row].size()+1;
            }
            painter.save();painter.setClipRegion(selected,Qt::IntersectClip);
            painter.translate(frame_.topLeft());painter.scale(frame_.width()/canvas_.width(),frame_.height()/canvas_.height());
            auto highlighted=text;highlighted.fill=true;highlighted.fillColor="#ffffffff";
            framescale::paintTextLayer(painter,highlighted,canvas_.width(),canvas_.height());painter.restore();
        } else if(caretVisible) {
            painter.save();painter.setTransform(textMatrix(),true);
            painter.fillRect(QRectF(g.cursorPoint(cursor.position()),QSizeF(1./std::max(.001,scale.x()),g.lineHeight)),editor_->caretColor);painter.restore();
        }
    }
    bool visibleVisual(int i) const {
        const auto& l=layers_[i];return l.kind!=framescale::LayerKind::Audio && l.enabled && seconds_>=l.start && seconds_<l.start+l.duration();
    }
    bool visibleText(int i) const {
        const auto& layer=layers_[i];
        return layer.kind==framescale::LayerKind::Text && layer.enabled
            && seconds_>=layer.start && seconds_<layer.start+layer.duration();
    }
    void updateLayout() {
        QRegion region;
        if(!canvas_.isEmpty() && !frame_.isEmpty()) for(int i=0;i<int(layers_.size());++i) if(visibleText(i) || (selectionTool_ && visibleVisual(i)))
            region+=textRect(i).adjusted(-6,-6,6,6).toAlignedRect();
        if(editing_>=0 && !visibleText(editing_)) finishEditing();
        if(editing_>=0 && canvas_.width()>0 && frame_.width()>0) {
            const auto& text=layers_[editing_].text;
            const QSignalBlocker block(editor_);
            const QString content=QString::fromStdString(text.content);
            if(editor_->toPlainText()!=content) editor_->setPlainText(content);
            QFont font=framescale::textLayerFont(text);
            const double sy=frame_.height()/canvas_.height()*text.verticalScale/100.;
            const double sx=frame_.width()/canvas_.width()*text.horizontalScale/100.;
            // Preserve fractional preview scale so cursor positions do not
            // drift from the glyphs when the viewer is fitted or zoomed.
            font.setPointSizeF(std::max(.1,qRound(text.fontSize)*sy*72./logicalDpiY()));
            font.setStretch(std::clamp(qRound(100*sx/std::max(.001,sy)),1,4000));
            font.setLetterSpacing(QFont::AbsoluteSpacing,text.letterSpacing*sx);font.setUnderline(text.underline);
            const QColor ink(QString::fromStdString(text.fill?text.fillColor:text.strokeColor));
            auto style=framescale::textLayerKey(text);style.remove("content");
            style.insert("viewScaleX",sx);style.insert("viewScaleY",sy);
            if(style!=editorStyle_) {
                editorStyle_=style;editor_->setFont(font);
                // The shared path painter owns glyphs, fill and stroke even
                // while editing; the native text control supplies selection,
                // Unicode/IME input and cursor placement over that geometry.
                editor_->caretColor=ink;
                editor_->setStyleSheet("QPlainTextEdit {background:transparent;color:transparent;border:0;padding:0;selection-background-color:#007aff;selection-color:white;}");
                QTextCursor all(editor_->document());all.select(QTextCursor::Document);
                QTextBlockFormat format;format.setAlignment(text.alignment==0?Qt::AlignLeft:text.alignment==2?Qt::AlignRight:Qt::AlignHCenter);
                const QFontMetricsF metrics(font);
                format.setLineHeight(text.lineHeight>0?text.lineHeight*sy:metrics.lineSpacing(),QTextBlockFormat::FixedHeight);
                all.mergeBlockFormat(format);
            }
            const QRect box=textRect(editing_).adjusted(-2,0,2,4).toAlignedRect();
            editor_->setGeometry(box);editor_->horizontalScrollBar()->setValue(0);editor_->verticalScrollBar()->setValue(0);
            region+=box;
        }
        if(videoSnapshotReady() || creationTool_) region+=frame_.toAlignedRect();
        // The mask leaves the surrounding preview available for pan and zoom.
        if(dragIndex_>=0 && selectionTool_) region=QRegion(rect());
        region&=QRegion(rect());if(mask()!=region) setMask(region);
        setVisible(!region.isEmpty());raise();update();
    }
    std::vector<framescale::MediaLayer> layers_;
    int selection_=-1,editing_=-1;
    double seconds_=0;
    QSize canvas_;
    QRectF frame_;
    CanvasTextEdit* editor_;
    QJsonObject editorStyle_;
    QHash<int,QImage> videoSnapshots_;
    QList<QPointer<QProcess>> snapshotReaders_;
    quint64 snapshotGeneration_=0;bool snapshotActive_=false;
    int dragIndex_=-1;bool dragMoved_=false;QPointF dragOrigin_;
    framescale::LayerTransform dragTransform_;
    bool selectionTool_=false,creationTool_=false;
};
