#pragma once
#include "SelectionComboBox.h"
#include <QCheckBox>
#include "Appearance.h"
#include <QVariantAnimation>
#include <QApplication>
#include <QAbstractSlider>
#include <QComboBox>
#include <QWheelEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QDoubleSpinBox>
#include <QFocusEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QTabBar>

class SwitchControl final : public QCheckBox {
public:
    explicit SwitchControl(const QString& label, QWidget* parent = nullptr)
        : QCheckBox(label, parent)
    {
        setCursor(Qt::ArrowCursor);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(28);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        motion_ = new QVariantAnimation(this);
        motion_->setDuration(140);
        motion_->setEasingCurve(QEasingCurve::OutCubic);
        connect(motion_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) { thumb_ = value.toReal(); update(); });
        connect(this, &QCheckBox::toggled, this, [this](bool checked) {
            motion_->stop();
            if (!isVisible() || Appearance::reduceMotion()) { thumb_ = checked ? 1. : 0.; update(); return; }
            motion_->setStartValue(thumb_);
            motion_->setEndValue(checked ? 1. : 0.);
            motion_->start();
        });
    }
    QSize sizeHint() const override { return { fontMetrics().horizontalAdvance(text()) + 88, 28 }; }

protected:
    void focusInEvent(QFocusEvent* event) override
    {
        keyboardFocus_ = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
        QCheckBox::focusInEvent(event);
        update();
    }
    void mousePressEvent(QMouseEvent* event) override
    {
        keyboardFocus_ = false;
        QCheckBox::mousePressEvent(event);
        update();
    }
    QRectF switchRect() const { return QRectF(width() - 42, (height() - 18) / 2., 40, 18); }
    bool hitButton(const QPoint& pos) const override { return switchRect().contains(pos); }
    void mouseMoveEvent(QMouseEvent* event) override {
        setCursor(isEnabled() && hitButton(event->position().toPoint()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
        QCheckBox::mouseMoveEvent(event);
        update();
    }
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        const auto symbol = ControlSymbols::forControl(objectName());
        const int textInset = symbol == ControlSymbols::Symbol::None ? 0 : 26;
        if (textInset) ControlSymbols::paint(p, QRectF(1,(height()-16)/2.,16,16), symbol, p.pen().color());
        p.drawText(rect().adjusted(textInset, 0, -52, 0), Qt::AlignLeft | Qt::AlignVCenter, text());
        const QRectF track(width() - 42, (height() - 18) / 2., 40, 18);
        p.setPen(Qt::NoPen);
        QColor trackColor = isEnabled() && isChecked() ? palette().color(QPalette::Highlight) : palette().color(QPalette::Mid);
        if (isEnabled() && underMouse() && hitButton(mapFromGlobal(QCursor::pos()))) trackColor = trackColor.darker(120);
        p.setBrush(trackColor);
        p.drawRoundedRect(track, 9, 9);
        p.setBrush(isEnabled() ? QColor("#ffffff") : QColor("#a0a0a0"));
        p.drawRoundedRect(QRectF(track.x() + 2 + 14 * thumb_, track.y() + 2, 22, 14), 7, 7);
        if (hasFocus() && keyboardFocus_) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(palette().color(QPalette::Highlight), 1, Qt::DotLine));
            p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 5, 5);
        }
    }

private:
    bool keyboardFocus_ = false;
    qreal thumb_ = 0.;
    QVariantAnimation* motion_ = nullptr;
};

class DocumentCloseButton final : public QPushButton {
public:
    explicit DocumentCloseButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(18, 18);
        setObjectName("documentCloseButton");
        setAccessibleName(tr("Fechar projeto"));
        setToolTip(QString());
        setCursor(Qt::PointingHandCursor);
        setStyleSheet("QPushButton {border:none;background:transparent;padding:0;}"
                      "QPushButton:hover {background:rgba(128,128,128,45);border-radius:6px;}");
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        QPushButton::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::ButtonText), 1.4, Qt::SolidLine, Qt::RoundCap));
        // Center on the widget's pixel bounds so the tab close X is not shifted right.
        const QPointF center((width() - 1) / 2., (height() - 1) / 2.);
        p.drawLine(center+QPointF(-3.5,-3.5),center+QPointF(3.5,3.5));
        p.drawLine(center+QPointF(3.5,-3.5),center+QPointF(-3.5,3.5));
    }
};

class ProjectTabBar final : public QTabBar {
public:
    using QTabBar::QTabBar;

protected:
    void tabLayoutChange() override
    {
        QTabBar::tabLayoutChange();
        for (int i = 0; i < count(); ++i)
            if (auto* close = tabButton(i, QTabBar::RightSide)) {
                const QRect tab = tabRect(i);
                close->move(tab.right() - close->width() - 8,
                    tab.top() + (tab.height() - close->height()) / 2);
            }
    }
    QSize tabSizeHint(int index) const override
    {
        auto size = QTabBar::tabSizeHint(index);
        return { qBound(110, size.width(), 340), fontMetrics().height() + 6 };
    }
};

class ScaleControl final : public SelectionComboBox {
public:
    explicit ScaleControl(QWidget* parent=nullptr) : SelectionComboBox(parent) {
        for (int step=2; step<=20; ++step) addScale(step/2.);
        setValue(2.);
    }
    double value() const { return currentData().toDouble(); }
    void setValue(double value) {
        value = std::clamp(value, 1., 10.);
        int index = findData(value);
        // Preserve custom fractional scales in existing projects/presets.
        if (index < 0) { addScale(value); index = count()-1; }
        setCurrentIndex(index);
    }
private:
    void addScale(double value) {
        addItem(locale().toString(value, 'f', 2).remove(QRegularExpression("0+$")).remove(QRegularExpression("[.,]$")) + QChar(0x00d7), value);
    }
};

// Prevent accidental edits while scrolling through settings, including focused fields.
class SafeValueInput final : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Wheel && (qobject_cast<QAbstractSpinBox*>(object) || qobject_cast<QComboBox*>(object) || qobject_cast<QAbstractSlider*>(object))) {
            if (qobject_cast<QScrollBar*>(object)) return false;
            auto* widget = qobject_cast<QWidget*>(object);
            for (auto* parent = widget ? widget->parentWidget() : nullptr; parent; parent = parent->parentWidget()) {
                if (auto* area = qobject_cast<QScrollArea*>(parent)) {
                    auto* wheel = static_cast<QWheelEvent*>(event);
                    QWheelEvent forwarded(area->viewport()->mapFromGlobal(wheel->globalPosition().toPoint()),
                        wheel->globalPosition(), wheel->pixelDelta(), wheel->angleDelta(),
                        wheel->buttons(), wheel->modifiers(), wheel->phase(), wheel->inverted());
                    QApplication::sendEvent(area->viewport(), &forwarded);
                    event->accept();
                    return true;
                }
            }
            event->ignore();
            return true;
        }
        return QObject::eventFilter(object,event);
    }
};
inline void installSafeValueInput() {
    static auto* filter = new SafeValueInput(qApp);
    qApp->installEventFilter(filter);
}
