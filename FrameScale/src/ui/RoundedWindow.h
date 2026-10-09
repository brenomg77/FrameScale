#pragma once
#include <QGraphicsEffect>
#include <QVariantAnimation>
#include "GlassMaterial.h"
#include "platform/WindowBackdrop.h"
#include <QPainter>
#include <QPainterPath>
#include <QDialog>
#include <QEvent>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QWindow>
#include <QStyleOption>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

class RoundedWindowEffect final : public QGraphicsEffect {
public:
    explicit RoundedWindowEffect(QWidget* window):QGraphicsEffect(window),window_(window) {
        window_->installEventFilter(this);
        reveal_.setDuration(150);
        reveal_.setEasingCurve(QEasingCurve::OutCubic);
        connect(&reveal_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {opacity_=value.toReal();update();});
    }
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window_) {
            if (event->type()==QEvent::Show && window_->isWindow() && !window_->inherits("QMainWindow")
                && !QSettings().value("reduceMotion",false).toBool()) {
                reveal_.stop(); reveal_.setStartValue(0.); reveal_.setEndValue(1.); reveal_.start();
            }
            switch (event->type()) {
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::StyleChange:
            case QEvent::PaletteChange:
            case QEvent::WindowStateChange:
            case QEvent::DevicePixelRatioChange:
                if (window_->isWindow()) framescale::window_backdrop::syncGlassRegion(window_);
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
        if (window_->property("glassWindowSurface").toBool()) {
            const bool light = QSettings().value("appearance", "macos-light").toString() != "macos-dark";
            const bool native = window_->property("nativeGlassActive").toBool();
            if (window_->property("stableSurface").toBool()) {
                // An opaque neutral base keeps the welcome surface independent
                // of activation, desktop colors and partially repainted children.
                mask.fillRect(window_->rect(), light ? QColor(242,242,244) : QColor(40,40,42));
            } else if (native) {
                mask.fillRect(window_->rect(), light ? QColor(255,255,255,160) : QColor(35,35,35,166));
            } else {
                QImage backdrop = window_->property("glassBackdrop").value<QImage>();
                if (backdrop.isNull() && window_->parentWidget())
                    backdrop = window_->parentWidget()->window()->property("glassBackdrop").value<QImage>();
                GlassMaterial::paint(mask, window_->rect(), backdrop, light, light ? 140 : 150, 0, true);
            }
        } else {
            // A solid base also covers areas whose child styles are transparent
            // after switching from a glass theme to Studio.
            if (qobject_cast<QDialog*>(window_) || window_->inherits("QMainWindow"))
                mask.fillRect(window_->rect(), window_->palette().color(QPalette::Window));
            QStyleOption background; background.initFrom(window_);
            window_->style()->drawPrimitive(QStyle::PE_Widget,&background,&mask,window_);
        }
        mask.restore();
        mask.drawPixmap(QPoint(),source);
        mask.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        if(alpha_.size()!=pixmap.size() || alpha_.devicePixelRatio()!=pixmap.devicePixelRatio() || offset_!=offset || rounded_!=rounded) {
            alpha_=QImage(pixmap.size(),QImage::Format_ARGB32_Premultiplied);
            alpha_.setDevicePixelRatio(pixmap.devicePixelRatio());alpha_.fill(Qt::transparent);
            QPainterPath path;
            path.addRoundedRect(QRectF(QPointF(-offset),QSizeF(window_->size())),rounded ? 12 : 0,rounded ? 12 : 0);
            QPainter shape(&alpha_);shape.setRenderHint(QPainter::Antialiasing);shape.fillPath(path,Qt::white);
            offset_=offset; rounded_=rounded;
        }
        mask.drawImage(QPoint(),alpha_);
        if (window_->property("glassWindowSurface").toBool() && rounded) {
            mask.setCompositionMode(QPainter::CompositionMode_SourceOver);
            mask.setBrush(Qt::NoBrush);
            mask.setPen(QPen(QColor(128,128,128,35),1));
            mask.drawRoundedRect(QRectF(QPointF(-offset),QSizeF(window_->size())).adjusted(.5,.5,-.5,-.5),11.5,11.5);
        }
        mask.end();
        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setOpacity(opacity_);
        painter->drawPixmap(offset,pixmap);
        painter->restore();
    }
private:
    QWidget* window_;
    QVariantAnimation reveal_;
    qreal opacity_ = 1.;
    QImage alpha_;
    QPoint offset_;
    bool rounded_ = true;
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
        setFixedHeight(30);
        auto* row=new QHBoxLayout(this);row->setContentsMargins(6,0,2,0);
        auto* title=new QLabel(dialog->windowTitle(),this);
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        title->setStyleSheet("background:transparent;font-weight:500;");row->addWidget(title,1);
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
        if (child && qobject_cast<QAbstractButton*>(child)) return false;
        // Welcome uses client dragging: native caption dragging enables Aero
        // Shake, which can minimize background windows during rapid movement.
        *result = window()->property("stableSurface").toBool() ? HTCLIENT : HTCAPTION;
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
