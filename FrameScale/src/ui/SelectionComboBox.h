#pragma once
#include "Appearance.h"
#include "GlassMaterial.h"
#include <QShowEvent>
#include <QComboBox>
#include <QEvent>
#include <QFontMetricsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QCursor>
#include <functional>
#include <QPainter>
#include <QPointer>
#include <QScreen>

// Keep Qt's menu navigation/accessibility while drawing the rounded material
// ourselves; the native Windows combobox container is always rectangular.
class SelectionMenu final : public QMenu {
public:
    explicit SelectionMenu(QWidget* parent) : QMenu(parent)
    {
        setObjectName("selectionPopup");
        setFont(parent->font());
        setToolTipsVisible(true);
        setToolTipDuration(15000);
        setProperty("appearanceManaged", true);
        // Windows only composites per-pixel alpha for frameless top-levels.
        // Without this flag the transparent shadow gutter is rendered black.
        setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        refreshMaterialStyle();
        setGraphicsEffect(new RoundedWindowEffect(this));
    }
    void refreshMaterialStyle()
    {
        const QString ink = Appearance::light() ? "#252329" : "#f2f1f5";
        QString style = QString(R"(
QMenu#selectionPopup {background:transparent;border:0;padding:14px;color:%1;font-size:14px;font-weight:500;font-family:"%2";}
QMenu#selectionPopup::item {background:transparent;border:0;border-radius:7px;padding:4px 14px 4px 34px;min-height:20px;}
QMenu#selectionPopup::item:checked {background:#007aff;color:white;}
QMenu#selectionPopup::item:selected {background:#0064d2;color:white;}
QMenu#selectionPopup::item:disabled {color:#96939e;}
QMenu#selectionPopup::icon {width:18px;height:18px;left:10px;}
QMenu#selectionPopup::indicator {width:0px;height:0px;}
QMenu#selectionPopup::separator {height:1px;background:rgba(128,128,140,55);margin:5px 12px;}
)").arg(ink, parentWidget()->font().family());
        const bool compactMenuBar = qobject_cast<QMenuBar*>(parentWidget()) != nullptr;
        if (compactMenuBar)
            style += "QMenu#selectionPopup {padding:11px 12px 14px;} QMenu#selectionPopup::item {padding:4px 10px;min-height:18px;min-width:0;}";
        setStyleSheet(style);
        if (compactMenuBar) {
            int textWidth = 0;
            for (auto* action : actions())
                if (!action->isSeparator()) textWidth = qMax(textWidth, fontMetrics().horizontalAdvance(action->text().remove('&')));
            setFixedWidth(textWidth + 48);
        }
    }
    std::function<void(quint64)> outsidePress;
    void setBackdrop(const QImage& image, const QPoint& origin) { source_ = image; sourceOrigin_ = origin; }
protected:
    void showEvent(QShowEvent* event) override
    {
        refreshMaterialStyle();
        if (!outsidePress && parentWidget()) {
            if (auto* bar=qobject_cast<QMenuBar*>(parentWidget())) {
                const QRect anchor=bar->actionGeometry(menuAction());
                QMenu::showEvent(event);
                adjustSize();
                setFixedWidth(qMax(width(), anchor.width() + 24));
                int left = bar->mapToGlobal(QPoint(anchor.left() - 12, bar->height() - 1)).x();
                if (auto* screen = bar->screen())
                    left = qBound(screen->availableGeometry().left(), left,
                        screen->availableGeometry().right() - width() + 1);
                move(left, bar->mapToGlobal(QPoint(0, bar->height() - 1)).y());
            } else {
                QMenu::showEvent(event);
            }
        } else {
            QMenu::showEvent(event);
        }
        backdrop_ = QImage();
        setProperty("frostedBackdropReady",false);
        if (source_.isNull() && parentWidget() && !GlassMaterial::reduced()) {
            auto* owner = parentWidget()->window();
            setBackdrop(owner->grab().toImage(),owner->mapToGlobal(QPoint()));
        }
        openingPress_ = QApplication::mouseButtons().testFlag(Qt::LeftButton);
        openingPosition_ = QCursor::pos();
        if (!source_.isNull() && !GlassMaterial::reduced()) {
            const qreal dpr = source_.devicePixelRatio();
            const QRect logical(mapToGlobal(QPoint())-sourceOrigin_, size());
            const QRect pixels(qRound(logical.x()*dpr),qRound(logical.y()*dpr),qRound(logical.width()*dpr),qRound(logical.height()*dpr));
            if (source_.rect().contains(pixels)) {
                backdrop_=GlassMaterial::blur(source_.copy(pixels),160);
                setProperty("frostedBackdropReady",!backdrop_.isNull());
            }
        }
        source_=QImage();
    }
    bool event(QEvent* event) override
    {
        // Menu-bar commands keep QMenu's own mouse forwarding and release
        // bookkeeping. Only a combo's overlapping popup needs interception.
        if (!outsidePress) return QMenu::event(event);
        // Qt clears this attribute for each new mouse event. Re-arm it here,
        // before QMenu handles a dismissing press: replaying that press onto
        // the combo underneath would immediately open a second popup.
        if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick)
            setAttribute(Qt::WA_NoMouseReplay);
        // A combo opens on mouse-down. Its selected row overlaps the field,
        // so QMenu would otherwise treat that same gesture's mouse-up as a
        // selection and instantly close again. A fresh click still selects;
        // holding and dragging into another row also keeps working.
        if (event->type() == QEvent::MouseMove && openingPress_) {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if ((mouse->globalPosition().toPoint()-openingPosition_).manhattanLength()
                > QApplication::startDragDistance()) openingPress_ = false;
        }
        if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick) {
            openingPress_ = false;
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (outsidePress && !rect().contains(mouse->position().toPoint())) {
                if (outsidePress) outsidePress(mouse->timestamp());
                // Dismiss directly before QMenu can forward the outside press
                // through its parent/caused-popup chain.
                setAttribute(Qt::WA_NoMouseReplay);
                close();
                event->accept();
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease && openingPress_) {
            openingPress_ = false;
            event->accept();
            return true;
        }
        return QMenu::event(event);
    }
    void paintEvent(QPaintEvent* event) override
    {
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            const QRectF body = QRectF(rect()).adjusted(8, 7, -8, -10);
            p.setPen(Qt::NoPen);
            for (int i = 8; i > 0; --i) {
                p.setBrush(QColor(0, 0, 0, 3 + (8-i)/2));
                p.drawRoundedRect(body.adjusted(-i, -i+3, i, i+3), 8+i, 8+i);
            }
            GlassMaterial::paint(p,body,backdrop_,Appearance::light(),Appearance::light()?170:185,8,true);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(Appearance::light() ? QColor(255,255,255,235) : QColor(255,255,255,60), 1));
            p.drawRoundedRect(body, 8, 8);
        }
        if (qobject_cast<QMenuBar*>(parentWidget())) {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            for (auto* action : actions()) {
                QRect row = actionGeometry(action);
                row.setLeft(12);
                row.setRight(width() - 13);
                // Keep the highlight inside the painted card, including its
                // asymmetric shadow gutter, independently of Qt menu metrics.
                row = row.intersected(QRect(12, 11, width() - 24, height() - 25));
                if (action->isSeparator()) {
                    p.setPen(QColor(128,128,128,55));
                    p.drawLine(row.left(), row.center().y(), row.right(), row.center().y());
                    continue;
                }
                if (action == activeAction()) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor("#0064d2"));
                    p.drawRoundedRect(row, 5, 5);
                }
                p.setPen(action == activeAction() ? Qt::white : palette().color(QPalette::WindowText));
                p.drawText(row.adjusted(6,0,-6,0), Qt::AlignLeft | Qt::AlignVCenter,
                    action->text().remove('&'));
            }
            return;
        }
        QMenu::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (auto* action : actions()) {
            if (!action->isChecked()) continue;
            const QRect row = actionGeometry(action);
            const QFontMetricsF metrics(font());
            // Match the optical centre of the capital letters, not the
            // rectangle's integer midpoint (which puts the tick too high).
            const qreal baseline = QRectF(row).center().y()
                + (metrics.ascent() - metrics.descent()) / 2.0;
            const QPointF centre(row.left() + 15.0, baseline - metrics.capHeight() / 2.0);
            p.setPen(QPen(QColor(Qt::white), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawLine(centre + QPointF(-4.5, -0.2), centre + QPointF(-1.5, 3.5));
            p.drawLine(centre + QPointF(-1.5, 3.5), centre + QPointF(4.5, -3.5));
        }
    }
private:
    QImage source_, backdrop_;
    QPoint sourceOrigin_, openingPosition_;
    bool openingPress_ = false;
};
class SelectionComboBox : public QComboBox {
public:
    explicit SelectionComboBox(QWidget* parent=nullptr) : QComboBox(parent) {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        connect(this, &QComboBox::currentTextChanged, this, [this] { updateGeometry(); });
    }
    QSize sizeHint() const override {
        QStyleOptionComboBox option;
        initStyleOption(&option);
        const QSize text(fontMetrics().horizontalAdvance(currentText()), fontMetrics().height());
        return style()->sizeFromContents(QStyle::CT_ComboBox, &option, text, this).expandedTo(QSize(58,24));
    }
    QSize minimumSizeHint() const override { return QSize(qMin(96,sizeHint().width()),sizeHint().height()); }
    void showPopup() override
    {
        if (Appearance::theme() == "studio") { QComboBox::showPopup(); return; }
        if (popup_ || !isEnabled() || count() == 0) return;
        auto* menu = new SelectionMenu(this);
        popup_ = menu;
        menu->outsidePress = [this](quint64 timestamp) { dismissedPress_ = timestamp; };
        menu->setMinimumWidth(width()+16);
        QAction* selected = nullptr;
        for (int i=0; i<count(); ++i) {
            auto* action = menu->addAction(itemText(i).replace('&', "&&"));
            action->setCheckable(true);
            action->setChecked(i == currentIndex());
            action->setEnabled(model()->flags(model()->index(i,modelColumn(),rootModelIndex())).testFlag(Qt::ItemIsEnabled));
            action->setToolTip(itemData(i,Qt::ToolTipRole).toString());
            action->setData(i);
            if (i == currentIndex()) selected = action;
            connect(action, &QAction::triggered, this, [this,i] {
                if (i >= count()) return;
                setCurrentIndex(i);
                emit activated(i);
                emit textActivated(currentText());
            });
        }
        connect(menu, &QMenu::aboutToHide, this, [this,menu] {
            if (popup_ != menu) return;
            popup_ = nullptr;
            // Exactly one reset for every custom popup. The menu may emit
            // aboutToHide from Esc, selection, outside click or hidePopup().
            QComboBox::hidePopup();
            menu->setObjectName(QString());
            menu->deleteLater();
        });
        // Position the selected row over its field, like macOS pickers.
        menu->ensurePolished();
        if (!GlassMaterial::reduced()) {
            auto* owner = window();
            menu->setBackdrop(owner->grab().toImage(),owner->mapToGlobal(QPoint()));
        }
        menu->popup(mapToGlobal(QPoint(-8, height()/2-16)), selected);
        menu->setActiveAction(selected);
    }
    void hidePopup() override
    {
        if (popup_) {
            popup_->close(); // aboutToHide performs the base-state reset.
        } else {
            QComboBox::hidePopup();
        }
    }
protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (Appearance::theme()=="studio" || event->button()!=Qt::LeftButton) {
            QComboBox::mousePressEvent(event);
            return;
        }
        // Match only the replay of the closing gesture, never block the next
        // real click with a debounce timer.
        if (event->timestamp() && event->timestamp()==dismissedPress_) {
            event->accept();
            return;
        }
        // Do not start the native combo container's release timer: this menu
        // owns its gesture and the container is never shown.
        if (popup_) hidePopup(); else showPopup();
        event->accept();
    }
private:
    quint64 dismissedPress_ = 0;
    QPointer<SelectionMenu> popup_;
};
