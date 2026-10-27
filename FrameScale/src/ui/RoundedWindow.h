#pragma once
#include <QGraphicsEffect>
#include <QVariantAnimation>
#include "GlassMaterial.h"
#include "platform/WindowBackdrop.h"
#include <QPainter>
#include <QPainterPath>
#include <QDialog>
#include <QEvent>
#include <QDynamicPropertyChangeEvent>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QWindow>
#include <QStyleOption>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QTimer>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class RoundedWindowEffect final : public QGraphicsEffect {
public:
    explicit RoundedWindowEffect(QWidget* window):QGraphicsEffect(window),window_(window) {
        // Owned glass windows must compose both blur and tint inside Qt's
        // alpha outline. Native accent blur has a separate rectangular surface.
        const bool ownedGlass=window_->isWindow() && (qobject_cast<QDialog*>(window_)
            || (window_->parentWidget() && window_->property("glassWindowSurface").toBool()));
        if(ownedGlass && (!window_->property("stableSurface").toBool() || window_->property("cachedGlassSurface").toBool())) {
            window_->setProperty("cachedGlassSurface",true);
            window_->setProperty("stableSurface",true);
        }
        backdropTimer_.setInterval(150);
        connect(&backdropTimer_,&QTimer::timeout,this,[this]{refreshBackdrop();});
        window_->installEventFilter(this);
        reveal_.setDuration(150);
        reveal_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&reveal_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {opacity_=value.toReal();update();});
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_) {
            if (event->type()==QEvent::DynamicPropertyChange) {
                const auto property=static_cast<QDynamicPropertyChangeEvent*>(event)->propertyName();
                if (property=="glassBackdrop" || property=="glassWindowSurface") update();
                if(property=="glassWindowSurface") scheduleBackdrop();
            }
            if (event->type()==QEvent::Show && window_->isWindow() && !window_->inherits("QMainWindow")
                && !QSettings().value("reduceMotion",false).toBool()) {
                reveal_.stop(); reveal_.setStartValue(0.); reveal_.setEndValue(1.); reveal_.start();
            }
            if(event->type()==QEvent::Hide) backdropTimer_.stop();
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::WinIdChange:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::StyleChange:
            case QEvent::PaletteChange:
            case QEvent::WindowStateChange:
            case QEvent::DevicePixelRatioChange:
                if (window_->isWindow()) framescale::window_backdrop::syncGlassRegion(window_);
                // Qt/Windows can update the layered window region after the
                // event filter runs. Restore our outline after that update,
                // including moves over other translucent controls.
                if (window_->isWindow() && !regionUpdatePending_) {
                    regionUpdatePending_=true;
                    QTimer::singleShot(0,this,[this] {
                        regionUpdatePending_=false;
                        framescale::window_backdrop::syncGlassRegion(window_);
                        update();
                    });
                }
                scheduleBackdrop();
                alpha_ = {};
                update();
                break;
            default:
                break;
            }
        }
        return QGraphicsEffect::eventFilter(watched, event);
    }
    void draw(QPainter* painter) override {
        const bool rounded = !window_->isMaximized() && !window_->isFullScreen();
        QPoint offset;
        const QPixmap source=sourcePixmap(Qt::LogicalCoordinates,&offset,NoPad);
        if(source.isNull()) return;
        // Never composite into Qt's cached source, or translucent dirty regions
        // can accumulate alpha after child widgets repaint.
        QPixmap pixmap(source.size());
        pixmap.setDevicePixelRatio(source.devicePixelRatio());
        pixmap.fill(Qt::transparent);
        QPainter mask(&pixmap);
        mask.setRenderHint(QPainter::Antialiasing);
        mask.save();
        mask.setCompositionMode(QPainter::CompositionMode_SourceOver);
        mask.translate(-offset);
        if (window_->property("glassWindowSurface").toBool() && !GlassMaterial::reduced()) {
            const bool light=QSettings().value("appearance", "macos-light").toString()!="macos-dark";
            if(window_->property("cachedGlassSurface").toBool()) {
                const auto backdrop=window_->property("glassBackdrop").value<QImage>();
                if(!backdrop.isNull()) mask.drawImage(window_->rect(),backdrop);
            }
            GlassMaterial::paintWindowSurface(mask,window_->rect(),light);
        } else {
            // A solid base also covers areas whose child styles are transparent
            // after switching from a glass theme to Studio.
            if (qobject_cast<QDialog*>(window_) || window_->inherits("QMainWindow"))
                mask.fillRect(window_->rect(), window_->palette().color(QPalette::Window));
            QStyleOption background; background.initFrom(window_);
            window_->style()->drawPrimitive(QStyle::PE_Widget,&background,&mask,window_);
        }
        mask.restore();
        // QWidget's masked source starts at the mask's bounding origin, even
        // though sourcePixmap reports offset (0,0). Restore that origin before
        // composing: otherwise child faces move up/left while native hover and
        // hit rectangles retain their original coordinates (inset tool windows).
        const QPoint maskOrigin=window_->mask().isEmpty()?QPoint():window_->mask().boundingRect().topLeft();
        mask.drawPixmap(offset.isNull()?maskOrigin:QPoint(),source);
        mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        if(alpha_.size()!=pixmap.size() || alpha_.devicePixelRatio()!=pixmap.devicePixelRatio() || offset_!=offset || rounded_!=rounded) {
            alpha_=QImage(pixmap.size(),QImage::Format_ARGB32_Premultiplied);
            alpha_.setDevicePixelRatio(pixmap.devicePixelRatio());alpha_.fill(Qt::transparent);
            QPainterPath path;
            const qreal inset=window_->property("glassEdgeInset").toReal();
            path.addRoundedRect(QRectF(QPointF(-offset),QSizeF(window_->size())).adjusted(inset,inset,-inset,-inset),rounded ? 14 : 0,rounded ? 14 : 0);
            QPainter shape(&alpha_);shape.setRenderHint(QPainter::Antialiasing);shape.fillPath(path,Qt::white);
            offset_=offset; rounded_=rounded;
        }
        if (window_->property("glassWindowSurface").toBool() && rounded) {
            mask.setCompositionMode(QPainter::CompositionMode_SourceOver);
            mask.setBrush(Qt::NoBrush);
            const bool light=QSettings().value("appearance","macos-light").toString()!="macos-dark";
            QLinearGradient rim(0,0,0,window_->height());
            rim.setColorAt(0,light?QColor(255,255,255,100):QColor(255,255,255,45));
            rim.setColorAt(1,QColor(128,128,128,20));
            mask.setPen(QPen(QBrush(rim),1));
            const qreal edge=window_->property("glassEdgeInset").toReal()+.5;
            mask.drawRoundedRect(QRectF(QPointF(-offset),QSizeF(window_->size())).adjusted(edge,edge,-edge,-edge),13.5,13.5);
        }
        // Clip the rim as well as the content so it cannot leave stray pixels
        // outside the smooth, antialiased alpha corners.
        mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        mask.drawImage(QPoint(),alpha_);
        mask.end();
        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(opacity_);
        painter->drawPixmap(offset,pixmap);
        painter->restore();
    }
private:
    void scheduleBackdrop() {
        if(!window_->property("cachedGlassSurface").toBool() || backdropPending_) return;
        backdropPending_=true;QTimer::singleShot(0,this,[this]{
            backdropPending_=false;refreshBackdrop();
            if(window_->isVisible() && window_->parentWidget()
                && window_->property("glassWindowSurface").toBool() && !GlassMaterial::reduced()) backdropTimer_.start();
        });
    }
    void refreshBackdrop() {
        if(!window_->isVisible() || !window_->property("cachedGlassSurface").toBool()) return;
        if(GlassMaterial::reduced() || !window_->property("glassWindowSurface").toBool()) {
            backdropTimer_.stop();backdropSample_={};GlassMaterial::updateBackdrop(window_,{});return;
        }
        if(!window_->parentWidget()) return;
        QWidget* owner=window_->parentWidget()->window();
        if(owner==window_ || !owner->isVisible()) return;
        const QRect requested(owner->mapFromGlobal(window_->mapToGlobal(QPoint())),window_->size());
        const QRect visible=requested.intersected(owner->rect());
        if(visible.isEmpty()) {backdropSample_={};GlassMaterial::updateBackdrop(window_,{});return;}
        // Only render our own application surface. Child tool windows are
        // excluded by QWidget::grab, so the snapshot cannot capture itself.
        const QImage capture=owner->grab(visible).toImage();
        const QSize sampleSize=window_->size().scaled(160,160,Qt::KeepAspectRatio).expandedTo(QSize(1,1));
        QImage sample(sampleSize,QImage::Format_ARGB32_Premultiplied);
        sample.fill(Qt::transparent);
        QPainter p(&sample);p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.scale(sample.width()/double(window_->width()),sample.height()/double(window_->height()));
        p.drawImage(QRect(visible.topLeft()-requested.topLeft(),visible.size()),capture);p.end();
        if(owner->property("glassWindowSurface").toBool()) {
            // The owner's translucent tint is already beneath this window in
            // the compositor. Copying it again makes Welcome almost opaque.
            // Recover only the content above that tint before blurring it.
            QImage tint(sample.size(),QImage::Format_ARGB32_Premultiplied);tint.fill(Qt::transparent);
            QPainter base(&tint);
            base.scale(sample.width()/double(window_->width()),sample.height()/double(window_->height()));
            base.translate(-requested.topLeft());
            GlassMaterial::paintWindowSurface(base,owner->rect(),QSettings().value("appearance","macos-light").toString()!="macos-dark");
            base.end();
            for(int y=0;y<sample.height();++y) {
                auto* row=reinterpret_cast<QRgb*>(sample.scanLine(y));
                const auto* background=reinterpret_cast<const QRgb*>(tint.constScanLine(y));
                for(int x=0;x<sample.width();++x) {
                    const QRgb src=row[x],bg=background[x];
                    const int alpha=qBound(0,(qAlpha(src)-qAlpha(bg))*255/qMax(1,255-qAlpha(bg)),255);
                    row[x]=qRgba(qBound(0,qRed(src)-qRed(bg)*(255-alpha)/255,alpha),
                        qBound(0,qGreen(src)-qGreen(bg)*(255-alpha)/255,alpha),
                        qBound(0,qBlue(src)-qBlue(bg)*(255-alpha)/255,alpha),alpha);
                }
            }
        }
        if(sample==backdropSample_ && !window_->property("glassBackdrop").value<QImage>().isNull()) return;
        backdropSample_=sample;GlassMaterial::updateBackdrop(window_,sample,160);update();
    }
    QTimer backdropTimer_;QImage backdropSample_;bool backdropPending_=false;
    QWidget* window_;
    QVariantAnimation reveal_;
    qreal opacity_ = 1.;
    QImage alpha_;
    QPoint offset_;
    bool rounded_ = true;
    bool regionUpdatePending_ = false;
};
class DialogCloseButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    void paintEvent(QPaintEvent* event) override {
        QPushButton::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::WindowText), 1.4, Qt::SolidLine, Qt::RoundCap));
        const QPointF c(width() / 2., height() / 2.);
        p.drawLine(c + QPointF(-3.5,-3.5), c + QPointF(3.5,3.5));
        p.drawLine(c + QPointF(-3.5,3.5), c + QPointF(3.5,-3.5));
    }
};
class DialogTitle final : public QWidget, public QAbstractNativeEventFilter {
public:
    explicit DialogTitle(QDialog* dialog):QWidget(dialog) {
        setObjectName("dialogTitleBar");
        setFixedHeight(dialog->property("dialogTitleHeight").isValid()?dialog->property("dialogTitleHeight").toInt():30);
        auto* row=new QHBoxLayout(this);row->setContentsMargins(6,0,2,0);
        auto* header=dialog->property("dialogTitleWidget").value<QWidget*>();
        auto* title=new QLabel(dialog->windowTitle(),this);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        title->setStyleSheet("background:transparent;font-weight:500;");if(header) {title->hide();row->addWidget(header);row->addStretch();} else row->addWidget(title,1);
        auto* close=new DialogCloseButton(this);
        close->setAutoDefault(false);
        close->setAccessibleName(tr("Fechar"));close->setFixedSize(26,26);
        close->setStyleSheet("QPushButton {background:transparent;border:none;padding:0;min-width:0;min-height:0;} QPushButton:hover {background:rgba(128,128,128,50);border-radius:7px;}");
        row->addWidget(close);connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
        connect(dialog,&QWidget::windowTitleChanged,title,&QLabel::setText);
        // Native caption hit testing works even when layered-window painting
        // prevents Qt from delivering client mouse moves to a transparent title.
        qApp->installNativeEventFilter(this);
    }
    ~DialogTitle() override {
        if (qApp) qApp->removeNativeEventFilter(this);
    }
protected:
    bool nativeEventFilter(const QByteArray&, void* message, qintptr* result) override {
#ifdef Q_OS_WIN
        const auto* native = static_cast<MSG*>(message);
        if (native->message != WM_NCHITTEST || !isVisible()
            || native->hwnd != reinterpret_cast<HWND>(window()->internalWinId())) return false;
        POINT point { static_cast<short>(LOWORD(native->lParam)), static_cast<short>(HIWORD(native->lParam)) };
        if (!ScreenToClient(native->hwnd, &point)) return false;
        const qreal ratio = window()->devicePixelRatioF();
        const QPoint local = mapFrom(window(), QPoint(qRound(point.x / ratio), qRound(point.y / ratio)));
        // Include the layout's top and side margins in the caption area.
        const QPoint inWindow = mapTo(window(), local);
        const QRect caption(0, 0, window()->width(), mapTo(window(), QPoint(0, height())).y());
        if (!caption.contains(inWindow)) return false;
        QWidget* child = childAt(local);
        for(auto* item=child;item && item!=this;item=item->parentWidget())
            if(qobject_cast<QAbstractButton*>(item) || item->property("captionInteractive").toBool()) return false;
        // Welcome uses client dragging: native caption dragging enables Aero
        // Shake, which can minimize background windows during rapid movement.
        *result = window()->property("clientCaptionDrag").toBool() ? HTCLIENT : HTCAPTION;
        return true;
#else
        Q_UNUSED(message);
        Q_UNUSED(result);
        return false;
#endif
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        // Layered frameless dialogs do not consistently support
        // QWindow::startSystemMove on Windows. Qt's implicit mouse grab lets
        // the title bar move them reliably, even outside the title bounds.
        dragging_ = true;
        dragOffset_ = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging_ && event->buttons().testFlag(Qt::LeftButton)) {
            window()->move(event->globalPosition().toPoint() - dragOffset_);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && dragging_) {
            dragging_ = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }
private:
    QPoint dragOffset_;
    bool dragging_ = false;
};
inline void installRoundedWindow(QWidget* window) {
    if(window->property("roundedWindowInstalled").toBool()) return;
    window->setProperty("roundedWindowInstalled",true);
    window->setWindowFlag(Qt::FramelessWindowHint);
    window->setAttribute(Qt::WA_TranslucentBackground);
    window->setAttribute(Qt::WA_StyledBackground);
    window->clearMask();
    if(auto* dialog=qobject_cast<QDialog*>(window)) {
        dialog->setSizeGripEnabled(false);
        if(auto* layout=qobject_cast<QBoxLayout*>(dialog->layout())) layout->insertWidget(0,new DialogTitle(dialog));
    }
    window->setGraphicsEffect(new RoundedWindowEffect(window));
}
