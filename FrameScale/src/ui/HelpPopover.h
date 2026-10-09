#pragma once
#include "GlassMaterial.h"
#include <QAbstractItemView>
#include <QAction>
#include <QAbstractButton>
#include <QComboBox>
#include <QLabel>
#include <QApplication>
#include <QHelpEvent>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTextDocument>
#include <QTimer>
#include <QToolTip>

// One application-owned tooltip surface, shared by fields and menu choices.
// Drawing the translucent window ourselves avoids the native rectangular
// Windows tooltip frame and keeps the material consistent with selection menus.
class HelpPopover final : public QWidget {
public:
    HelpPopover() : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint
        | Qt::NoDropShadowWindowHint | Qt::WindowTransparentForInput) {
        setObjectName("helpPopover");
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setProperty("appearanceManaged", true);
        document_.setDocumentMargin(0);
    }
    void present(const QString& tip, QWidget* owner, const QRect& anchor) {
        hide();
        const QString theme = QSettings().value("appearance", "macos-light").toString();
        light_ = theme == "macos-light";
        studio_ = theme == "studio";
        QFont font = QApplication::font();
        font.setPixelSize(13);
        font.setWeight(QFont::Normal);
        document_.setDefaultFont(font);
        QTextDocument parsed;
        if (Qt::mightBeRichText(tip)) parsed.setHtml(tip);
        else parsed.setPlainText(tip);
        const auto lines = parsed.toPlainText().split('\n', Qt::SkipEmptyParts);
        const bool titled = tip.contains("<b>") && lines.size() > 1;
        const QString title = titled ? lines.first() : QString();
        const QString description = titled ? lines.mid(1).join('\n') : lines.join('\n');
        const QString ink = light_ ? "#29282d" : "#eeeeee";
        const QString secondary = light_ ? "#45444a" : "#dedede";
        const QString html = (title.isEmpty() ? QString() :
            QString("<p style='margin:0 0 4px 0;color:%1;font-weight:600;'>%2</p>")
                .arg(ink, title.toHtmlEscaped()))
            + QString("<p style='margin:0;color:%1;'>%2</p>").arg(secondary, description.toHtmlEscaped().replace('\n', "<br>"));
        document_.setHtml(html);
        QScreen* screen = QGuiApplication::screenAt(anchor.center());
        if (!screen) screen = owner->screen();
        const QRect available = screen->availableGeometry().adjusted(6,6,-6,-6);
        const int maxTextWidth = qMax(80, qMin(330, available.width() - 40));
        const QFontMetrics metrics(font);
        const int textWidth = qMin(maxTextWidth, qMax(130,
            qMax(metrics.horizontalAdvance(title), metrics.horizontalAdvance(description))));
        document_.setTextWidth(textWidth);
        resize(textWidth + 40, qCeil(document_.size().height()) + 34);
        int x = anchor.left() - 8;
        int y = anchor.bottom() + 5;
        if (y + height() > available.bottom() + 1) y = anchor.top() - height() - 5;
        x = qBound(available.left(), x, qMax(available.left(), available.right() - width() + 1));
        y = qBound(available.top(), y, qMax(available.top(), available.bottom() - height() + 1));
        move(x,y);
        backdrop_ = {};
        if (!studio_ && !GlassMaterial::reduced()) {
            QWidget* window = owner->window();
            const QRect region(window->mapFromGlobal(pos()), size());
            const QRect visible = region.intersected(window->rect());
            if (!visible.isEmpty()) backdrop_ = GlassMaterial::blur(window->grab(visible).toImage(), 100);
        }
        setAccessibleName(title.isEmpty() ? description : title);
        setAccessibleDescription(description);
        show();
        raise();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF card = QRectF(rect()).adjusted(8,6,-8,-8);
        painter.setPen(Qt::NoPen);
        for (int spread=6; spread>0; --spread) {
            painter.setBrush(QColor(0,0,0,studio_ ? 3 : 4));
            painter.drawRoundedRect(card.adjusted(-spread, -spread/2., spread, spread), 10+spread,10+spread);
        }
        if (studio_) {
            painter.setBrush(QColor("#303030"));
            painter.drawRoundedRect(card,8,8);
        } else {
            GlassMaterial::paint(painter,card,backdrop_,light_,light_ ? 225 : 230,10,true);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(studio_ ? QColor("#555555") : light_ ? QColor(255,255,255,230) : QColor(255,255,255,55),1));
        painter.drawRoundedRect(card.adjusted(.5,.5,-.5,-.5),studio_?8:10,studio_?8:10);
        painter.translate(20,16);
        document_.drawContents(&painter);
    }
private:
    QTextDocument document_;
    QImage backdrop_;
    bool light_=true,studio_=false;
};

class HelpPopoverFilter final : public QObject {
public:
    explicit HelpPopoverFilter(QObject* parent) : QObject(parent) {
        timeout_.setSingleShot(true);
        connect(&timeout_,&QTimer::timeout,this,[this]{dismiss();});
        tracking_.setInterval(100);
        connect(&tracking_,&QTimer::timeout,this,[this]{
            if (!owner_ || !owner_->isVisible() || !anchor_.contains(QCursor::pos())) dismiss();
        });
    }
    ~HelpPopoverFilter() override { delete bubble_; }
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type()==QEvent::ToolTip) {
            auto* widget=qobject_cast<QWidget*>(object);
            if (!widget || widget==bubble_) return false;
            const auto* help=static_cast<QHelpEvent*>(event);
            // Editors inside spin boxes inherit their parent's help. Keep one
            // owner and anchor while the pointer moves between its children.
            while (widget->toolTip().isEmpty() && widget->parentWidget()
                && !widget->isWindow() && !qobject_cast<QAbstractItemView*>(widget->parentWidget()))
                widget = widget->parentWidget();
            QString tip=widget->toolTip();
            QRect local=widget->rect();
            QString label;
            if (auto* button=qobject_cast<QAbstractButton*>(widget)) label=button->text();
            else if (auto* caption=qobject_cast<QLabel*>(widget)) label=caption->text();
            else if (auto* combo=qobject_cast<QComboBox*>(widget)) label=combo->currentText();
            if (auto* menu=qobject_cast<QMenu*>(widget)) {
                auto* action=menu->actionAt(help->pos());
                if (!action || !menu->toolTipsVisible()) return false;
                tip=action->toolTip(); label=action->text(); local=menu->actionGeometry(action);
            } else {
                auto* view=qobject_cast<QAbstractItemView*>(widget->parentWidget());
                if (view && view->viewport()==widget) {
                    const auto index=view->indexAt(help->pos());
                    const QString itemTip=index.data(Qt::ToolTipRole).toString();
                    if (!itemTip.isEmpty()) { tip=itemTip;label=index.data().toString();local=view->visualRect(index); }
                }
            }
            if (tip.isEmpty()) { dismiss();return false; }
            // QAction::toolTip() falls back to its text even when no help was
            // supplied. Never turn that fallback into an explanatory balloon.
            QTextDocument text;
            text.setHtml(tip);
            auto normalized=[](QString value) {
                value=value.section('\t',0,0);value.remove('&');value.remove(QChar(0x2026));
                return value.simplified().toCaseFolded();
            };
            if (!label.isEmpty() && normalized(text.toPlainText())==normalized(label)) {
                dismiss();QToolTip::hideText();event->accept();return true;
            }
            if (!bubble_) bubble_=new HelpPopover;
            QToolTip::hideText();
            const QRect anchor(widget->mapToGlobal(local.topLeft()),local.size());
            if (bubble_->isVisible() && owner_ == widget && anchor_ == anchor && tip_ == tip) {
                event->accept();
                return true;
            }
            owner_=widget;
            anchor_=anchor;
            tip_=tip;
            bubble_->present(tip,widget,anchor_);
            timeout_.start(widget->toolTipDuration()>0 ? widget->toolTipDuration() : 15000);
            tracking_.start();
            event->accept();
            return true;
        }
        if (bubble_ && bubble_->isVisible()) {
            switch(event->type()) {
            case QEvent::MouseButtonPress:
            case QEvent::KeyPress:
            case QEvent::Wheel:
            case QEvent::ApplicationDeactivate:
                dismiss(); break;
            case QEvent::Hide:
            case QEvent::Close:
            case QEvent::WindowDeactivate:
                if (owner_ && (object==owner_ || object==owner_->window())) dismiss();
                break;
            default:break;
            }
        }
        return false;
    }
private:
    void dismiss() { if(bubble_)bubble_->hide();timeout_.stop();tracking_.stop();owner_.clear(); }
    QPointer<HelpPopover> bubble_;
    QPointer<QWidget> owner_;
    QRect anchor_;
    QString tip_;
    QTimer timeout_,tracking_;
};
inline void installHelpPopovers() {
    static auto* filter = new HelpPopoverFilter(qApp);
    qApp->installEventFilter(filter);
}
