#pragma once
#include "RoundedWindow.h"
#include "ControlSymbols.h"
#include <QAbstractButton>
#include <QPushButton>
#include "platform/WindowBackdrop.h"
#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QWidget>
#include <QMessageBox>
#include <QEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QToolTip>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
namespace Appearance {
inline bool reduceMotion() { return QSettings().value("reduceMotion", false).toBool(); }
inline void revealSection(QWidget* widget, bool visible) {
    auto* motion = widget->findChild<QVariantAnimation*>("sectionMotion", Qt::FindDirectChildrenOnly);
    if (motion) { motion->stop(); motion->setObjectName(QString()); motion->deleteLater(); widget->setMaximumHeight(QWIDGETSIZE_MAX); }
    if (!visible || !widget->window()->isVisible() || reduceMotion()) {
        widget->setMaximumHeight(QWIDGETSIZE_MAX);
        widget->setVisible(visible);
        return;
    }
    if (!widget->isHidden()) return;
    widget->setVisible(true);
    const int fullHeight = widget->sizeHint().height();
    widget->setMaximumHeight(0);
    motion = new QVariantAnimation(widget);
    motion->setObjectName("sectionMotion");
    motion->setDuration(160);
    motion->setEasingCurve(QEasingCurve::OutCubic);
    motion->setStartValue(0);
    motion->setEndValue(fullHeight);
    QObject::connect(motion, &QVariantAnimation::valueChanged, widget, [widget](const QVariant& value) {widget->setMaximumHeight(value.toInt());});
    QObject::connect(motion, &QVariantAnimation::finished, widget, [widget,motion] {widget->setMaximumHeight(QWIDGETSIZE_MAX);motion->deleteLater();});
    motion->start();
}
inline QString theme()
{
    const auto value = QSettings().value("appearance", "macos-light").toString();
    return value == "studio" || value == "macos-dark" ? value : QStringLiteral("macos-light");
}
inline bool light() { return theme() == "macos-light"; }
inline QString adapted(QString source)
{
    if (theme() == "studio") return source;
    // Map the legacy neutral colors, never the media pixels or semantic accents.
    const QRegularExpression color("#[0-9a-fA-F]{6}\\b|#[0-9a-fA-F]{3}\\b");
    auto matches = color.globalMatch(source);
    struct Replacement { int start, length; QString text; };
    QList<Replacement> replacements;
    while (matches.hasNext()) {
        const auto match = matches.next();
        QColor value(match.captured());
        if (value.hsvSaturation() < 28) {
            const int v = value.value();
            if (light()) {
                const int level = qBound(24, 262 - v, 250);
                value = QColor(level, level, level);
            } else if (v < 100) {
                value = QColor(qMax(24, v), qMax(24, v), qMax(24, v));
            }
        } else if (light() && value.lightness() > 165) {
            value = value.darker(155);
        }
        replacements.push_back({int(match.capturedStart()), int(match.capturedLength()), value.name()});
    }
    for (auto it = replacements.crbegin(); it != replacements.crend(); ++it)
        source.replace(it->start, it->length, it->text);
    return source;
}
inline void adaptLocal(QWidget* widget)
{
    if (widget->property("appearanceManaged").toBool()) return;
    if (!widget->property("appearanceOriginalStyle").isValid()) {
        if (widget->styleSheet().isEmpty()) return;
        widget->setProperty("appearanceOriginalStyle", widget->styleSheet());
    }
    QString style = adapted(widget->property("appearanceOriginalStyle").toString());
    if (light()) style += " QLabel {color:#29282d;} QToolButton:checked {color:#222126;} QPushButton#comparePreviewButton {color:white;} QComboBox::down-arrow,QSpinBox::down-arrow,QDoubleSpinBox::down-arrow {image:url(:/brand/chevron-ink.svg);} QSpinBox::up-arrow,QDoubleSpinBox::up-arrow {image:url(:/brand/chevron-up-ink.svg);}";
    if (theme() != "studio")
        style += QString(" QWidget#mediaPreview, QWidget#previewToolbar, QWidget#previewTrimBar, QWidget#comparisonHeader, QLabel#previewPlaceholder {background:%1;}").arg(theme() != "studio" && !GlassMaterial::reduced() ? "transparent" : (light() ? "#fafafa" : "#292929"));
    if (widget->styleSheet() != style) widget->setStyleSheet(style);
}
class SurfaceFilter final : public QObject {
public:
    explicit SurfaceFilter(QObject* parent) : QObject(parent) { }
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show)
            if (auto* widget = qobject_cast<QWidget*>(watched)) {
                adaptLocal(widget);
                if (qobject_cast<QMessageBox*>(widget)) {
                    const QString bg = light() ? "#fafafa" : "#292929";
                    const QString fg = light() ? "#292929" : "#eeeeee";
                    widget->setStyleSheet(QString("QMessageBox {background:%1;color:%2;} QMessageBox QLabel {background:transparent;color:%2;} QMessageBox QPushButton {background:%1;color:%2;border:1px solid #888;border-radius:6px;padding:3px 12px;min-height:20px;}").arg(bg, fg));
                }
                if (widget->isWindow() && widget->property("roundedWindowInstalled").toBool()) {
                    widget->setProperty("nativeGlassActive", framescale::window_backdrop::applyGlass(widget, widget->property("glassWindowSurface").toBool(), !light()));
                    widget->update();
                }
            }
        return false;
    }
};
inline void palette()
{
    static auto* filter = new SurfaceFilter(qApp);
    qApp->installEventFilter(filter);
    QPalette p;
    p.setColor(QPalette::Window, QColor("#202020"));
    p.setColor(QPalette::WindowText, QColor("#dedede"));
    p.setColor(QPalette::Text, QColor("#dedede"));
    p.setColor(QPalette::Base, QColor("#171717"));
    p.setColor(QPalette::AlternateBase, QColor("#292929"));
    p.setColor(QPalette::Button, QColor("#303030"));
    p.setColor(QPalette::ButtonText, QColor("#dedede"));
    p.setColor(QPalette::Highlight, QColor("#1473e6"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    for (auto role : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText })
        p.setColor(QPalette::Disabled, role, QColor("#8b8b8b"));
    if (theme() != "studio") {
        const bool pale = light();
        p.setColor(QPalette::Window, QColor(pale ? "#fafafa" : "#292929"));
        p.setColor(QPalette::WindowText, QColor(pale ? "#242424" : "#f0f0f0"));
        p.setColor(QPalette::Text, p.color(QPalette::WindowText));
        p.setColor(QPalette::Base, QColor(pale ? "#fafafa" : "#202020"));
        p.setColor(QPalette::AlternateBase, QColor(pale ? "#eeeeee" : "#303030"));
        p.setColor(QPalette::Button, QColor(pale ? "#ffffff" : "#3a3a3a"));
        p.setColor(QPalette::ButtonText, p.color(QPalette::WindowText));
        p.setColor(QPalette::Highlight, QColor("#007aff"));
        p.setColor(QPalette::Mid, QColor(pale ? "#c7c7c7" : "#505050"));
        for (auto role : { QPalette::Text, QPalette::WindowText, QPalette::ButtonText })
            p.setColor(QPalette::Disabled, role, QColor(pale ? "#898989" : "#8b8b8b"));
    }
    qApp->setPalette(p);
}
inline void apply(QWidget* widget, const QString& base = { })
{
    QPalette helpPalette;
    helpPalette.setColor(QPalette::ToolTipBase, light() ? QColor("#fafafa") : QColor("#292929"));
    helpPalette.setColor(QPalette::ToolTipText, light() ? QColor("#202020") : QColor("#f2f2f2"));
    QToolTip::setPalette(helpPalette);
    QFont helpFont = QApplication::font(); helpFont.setPixelSize(12); QToolTip::setFont(helpFont);
    if (!widget->property("studioStyle").isValid())
        widget->setProperty("studioStyle", base.isEmpty() ? widget->styleSheet() : base);
    QString style = widget->property("studioStyle").toString() + R"(
QDialog, QMessageBox {background:#202020;color:#dedede;}
QDialog QLabel, QMessageBox QLabel {background:transparent;color:#dedede;}
QPushButton:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QLineEdit:focus {border:1px solid #70b6ff;}
QGroupBox {border:1px solid #3a3a3a;border-radius:4px;margin-top:12px;padding:12px 8px 8px;}
QGroupBox::title {subcontrol-origin:margin;left:12px;padding:0 5px;color:#ccc;}
QDialog QDialogButtonBox QPushButton,QMessageBox QPushButton {background:#252525;color:#ddd;border:2px solid #555;border-radius:13px;padding:3px 16px;min-height:20px;min-width:52px;}
QDialog QDialogButtonBox QPushButton:default,QMessageBox QPushButton:default {background:#414141;border-color:#ddd;}
QDialog QDialogButtonBox QPushButton:focus,QMessageBox QPushButton:focus {border-color:#69b4ff;}
QDialog QDialogButtonBox QPushButton:hover,QMessageBox QPushButton:hover {background:#444;border-color:#aaa;}
QListWidget#settingsSections {background:#242424;border:none;}
QListWidget#settingsSections::item {padding:12px;}
QListWidget#settingsSections::item:selected {background:#343434;}
QComboBox::down-arrow {image:url(:/brand/chevron-dark.svg);width:12px;height:8px;}
QProgressBar {color:white;background:#292d34;border:1px solid #697383;border-radius:6px;text-align:center;}
QProgressBar::chunk {background:#0968cf;border-radius:5px;}
)";
    widget->setProperty("appearanceManaged", true);
    if (widget->isWindow() && (qobject_cast<QDialog*>(widget) || widget->inherits("QMainWindow"))) installRoundedWindow(widget);
    if (widget->isWindow() && widget->property("roundedWindowInstalled").toBool()) {
        const bool glass = theme() != "studio" && !GlassMaterial::reduced();
        widget->setProperty("glassWindowSurface", glass);
        widget->setProperty("nativeGlassActive", framescale::window_backdrop::applyGlass(widget, glass, !light()));
        if (theme() == "studio") widget->setProperty("glassBackdrop", QImage());
    }
    if (theme() == "studio") {
        // The controls now have compact measured geometry in every theme.
        // Retaining the old Studio padding clips fixed-height tabs/buttons.
        style += R"(
QWidget#dialogTitleBar {background:transparent;border:0;}
QFrame#inspectorCard {background:transparent;border:0;border-radius:14px;}
QWidget#inspectorContent, QWidget#encoderOptions {background:#292929;border:0;}
QTabWidget#inspectorTabs {background:transparent;border:0;}
QTabWidget#inspectorTabs::pane {border:0;top:0;margin:0;}
QTabWidget#inspectorTabs QTabBar::tab {padding:4px 10px;margin:4px 3px 0;font-size:12px;border-radius:12px;}
QTabBar#projectTabs::tab {margin:2px 4px;padding:0 8px;font-weight:500;font-size:14px;}
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {background:#202020;color:#ededed;border:1px solid #505050;border-radius:4px;padding:1px 8px;min-height:20px;}
QSpinBox, QDoubleSpinBox {padding:1px 25px 1px 8px;max-height:24px;}
QSpinBox::up-button, QDoubleSpinBox::up-button {subcontrol-origin:border;subcontrol-position:top right;width:18px;height:10px;margin:2px 3px 0 0;border:none;background:#303030;}
QSpinBox::down-button, QDoubleSpinBox::down-button {subcontrol-origin:border;subcontrol-position:bottom right;width:18px;height:10px;margin:0 3px 2px 0;border:none;background:#303030;}
QSpinBox::up-arrow,QDoubleSpinBox::up-arrow,QSpinBox::down-arrow,QDoubleSpinBox::down-arrow {width:9px;height:6px;}
QPushButton {padding:1px 8px;min-height:18px;font-weight:500;}
QPushButton[variant="primary"] {padding:1px 8px;border-radius:5px;}
QDialog QDialogButtonBox QPushButton {background:#363636;color:#eeeeee;border:1px solid #565656;border-radius:5px;padding:1px 8px;min-width:0;min-height:18px;}
QDialog QDialogButtonBox QPushButton:default {border-color:#70b6ff;}
QDialog QDialogButtonBox QPushButton[variant="primary"] {background:#1473e6;border-color:#1473e6;color:white;}
QListWidget#settingsSections {background:transparent;border:none;}
QListWidget#settingsSections::item {padding:4px 8px;margin:0 0 4px;border-radius:4px;}
QListWidget#settingsSections::item:selected {background:#1473e6;color:white;}
QScrollArea#formatOptionsScroll, QWidget#formatOptionsViewport, QTabWidget#formatDetails, QTabWidget#formatDetails QStackedWidget, QWidget#formatVideoPage, QWidget#formatAudioPage {background:transparent;border:0;}
QTabWidget#formatDetails::pane {background:transparent;border:0;}
QTabWidget#formatDetails QTabBar::tab {padding:4px 10px;}
QGroupBox {background:#292929;border:1px solid #454545;border-radius:5px;margin-top:22px;padding:12px 10px 10px;}
QGroupBox::title {subcontrol-origin:margin;subcontrol-position:top left;left:10px;top:0;padding:0;color:#dedede;background:transparent;}
QFrame#previewDock {background:transparent;border:0;}
QWidget#previewZoomBar {background:#333333;border:1px solid #454545;border-radius:10px;margin:0;}
QToolButton#previewVolumeButton {background:#333333;border:1px solid #505050;border-radius:5px;padding:0;margin:0;}
QPushButton#comparePreviewButton {background:#1473e6;color:white;border:1px solid #1473e6;border-radius:5px;padding:1px 8px;}
QPushButton#saveEncoderPreset, QPushButton#removeEncoderPreset, QPushButton#importEncoderPreset, QPushButton#viewQueueButton, QPushButton#addToQueueButton {font-size:14px;font-weight:500;padding:1px 8px;}
QSpinBox#targetFps {padding:1px 22px 1px 8px;min-width:58px;}
QToolTip {background:#303030;color:#eeeeee;border:1px solid #606060;padding:6px;border-radius:6px;font-size:12px;}
)";
    }
    if (theme() != "studio") {
        style = adapted(style);
        const QString surface = light() ? "rgba(255,255,255,88)" : "rgba(64,64,64,100)";
        const QString border = light() ? "rgba(60,60,60,38)" : "rgba(255,255,255,24)";
        const QString ink = light() ? "#292929" : "#eeeeee";
        const QString panel = light() ? "#fafafa" : "#292929";
        style += QString(R"(
QMainWindow, QDialog, QWidget#appRoot, QWidget#welcomePage, QWidget#welcomeBody {background:%1; color:%4;}
QMenuBar {background:transparent;color:%4;spacing:3px;}
QMenuBar::item {background:transparent;padding:2px 6px;border-radius:5px;}
QMenuBar::item:selected {background:%2;}
QMenuBar::item:pressed {background:#007aff;color:white;}
QToolButton#previewVolumeButton {background:%2;border:1px solid %3;border-radius:7px;padding:0;margin:0;}
QToolButton#previewVolumeButton:hover {background:%3;}
QWidget#titleBar {background:transparent;border:0;}
QWidget#documentBar {background:transparent;border:0;}
QWidget#appRoot, QWidget#editorPage {background:transparent;}
QWidget#inspectorPanel {background:#181818;}
QFrame#inspectorCard {background:transparent;border:0;border-radius:14px;}
QTabWidget#inspectorTabs {background:transparent;border:0;}
QWidget#inspectorContent, QWidget#encoderOptions {background:transparent;}
QWidget#welcomeRail {background:%2;}
QWidget#inspectorContent, QWidget#encoderOptions {border:0;}
QTabWidget#inspectorTabs QTabBar::tab {padding:4px 10px;margin:4px 3px 0;font-size:12px;border-radius:12px;}
QTabWidget#inspectorTabs::pane {border:0;top:0;margin:0;}
QTabWidget#inspectorTabs QTabBar {border:0;}
QTabBar::tab {border:0;border-radius:7px;background:transparent;color:%4;margin:3px;padding:4px 10px;}
QTabBar::tab:selected {background:%2;color:%4;border:1px solid %3;}
QTabBar#projectTabs::tab {margin:2px 4px;padding:0px 8px;font-weight:500;font-size:14px;}
QSpinBox, QDoubleSpinBox {padding:1px 25px 1px 8px;min-height:20px;max-height:24px;}
QSpinBox::up-button, QDoubleSpinBox::up-button {subcontrol-origin:border;subcontrol-position:top right;width:18px;height:10px;margin:2px 3px 0 0;border:none;background:transparent;border-top-right-radius:5px;}
QSpinBox::down-button, QDoubleSpinBox::down-button {subcontrol-origin:border;subcontrol-position:bottom right;width:18px;height:10px;margin:0 3px 2px 0;border:none;background:transparent;border-bottom-right-radius:5px;}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,QSpinBox::down-button:hover,QDoubleSpinBox::down-button:hover {background:%3;}
QSpinBox::up-arrow,QDoubleSpinBox::up-arrow,QSpinBox::down-arrow,QDoubleSpinBox::down-arrow {width:9px;height:6px;}
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {background:%2;}
QComboBox {padding:1px 8px;min-height:20px;}
QPushButton, QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {border:1px solid %3;border-radius:8px;}
QGroupBox {background:rgba(255,255,255,18);}
QPushButton {background:%2;color:%4;padding:1px 8px;min-height:18px;min-width:0;}
QPushButton:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover, QToolButton:hover {background:rgba(0,0,0,45);}
QTabBar::tab:hover {background:rgba(0,0,0,45);}
QSlider::handle:horizontal:hover, QScrollBar::handle:vertical:hover {background:#0064d2;}
QCheckBox::indicator:hover {background:rgba(0,0,0,45);border:1px solid %3;border-radius:3px;}
QPushButton:pressed {background:%3;}
QPushButton[variant="primary"], QPushButton#comparePreviewButton {background:#007aff;color:white;border:1px solid #1686ff;border-radius:7px;padding:1px 8px;}
QPushButton[variant="primary"]:hover, QPushButton#comparePreviewButton:hover {background:#0064d2;}
QPushButton[variant="text"] {background:transparent;}
QComboBox QAbstractItemView, QMenu {background:%2;border:1px solid %3;border-radius:9px;padding:6px;color:%4;selection-background-color:#007aff;selection-color:white;}
QMenu::item {padding:3px 20px 3px 12px;border-radius:5px;}
QLabel#welcomeTitle, QLabel#dropTitle {color:%4;}
QFrame#mediaDropZone {background:%2;border:1px solid transparent;border-radius:18px;}
QGroupBox {border:1px solid %3;border-radius:10px;margin-top:22px;padding:12px 10px 10px;}
QGroupBox::title {subcontrol-origin:margin;subcontrol-position:top left;left:10px;top:0;padding:0;color:%4;background:transparent;}
QScrollArea#formatOptionsScroll, QWidget#formatOptionsViewport, QTabWidget#formatDetails, QTabWidget#formatDetails QStackedWidget, QWidget#formatVideoPage, QWidget#formatAudioPage {background:transparent;border:0;}
QTabWidget#formatDetails::pane {background:transparent;border:0;}
QTabWidget#formatDetails QTabBar {background:transparent;border:0;}
QListWidget#settingsSections {background:transparent;}
QListWidget#settingsSections::item {padding:4px 8px;margin:0 0 4px 0;border-radius:7px;}
QListWidget#settingsSections::item:hover {background:#0064d2;color:white;}
QListWidget#settingsSections::item:selected {background:#007aff;color:white;}
QListWidget#settingsSections::item:selected:active {background:#007aff;}
QDialog QDialogButtonBox QPushButton {border:1px solid %3;background:%2;color:%4;border-radius:7px;padding:1px 8px;min-width:0;min-height:18px;}
QDialog QDialogButtonBox QPushButton:hover {background:rgba(0,0,0,45);}
QDialog QDialogButtonBox QPushButton:default:hover {background:#0064d2;}
QDialog QDialogButtonBox QPushButton:default {background:#007aff;border-color:#1686ff;color:white;}
QDialog#exitConfirmation QDialogButtonBox QPushButton:default {background:%2;color:%4;border-color:#70b6ff;}
QDialog#exitConfirmation QDialogButtonBox QPushButton[variant="primary"] {background:#007aff;color:white;border-color:#1686ff;}
QFrame#previewDock {background:transparent;border:0;}
QWidget#previewZoomBar {background:transparent;border:0;margin:0;}
QPushButton#saveEncoderPreset, QPushButton#removeEncoderPreset, QPushButton#importEncoderPreset, QPushButton#viewQueueButton, QPushButton#addToQueueButton {font-size:14px;font-weight:500;padding:1px 8px;}
QSpinBox#targetFps {padding:1px 22px 1px 8px;min-width:58px;}

QDialog#exportDialog QFrame#exportPanel, QDialog#exportDialog QFrame#exportRenderPanel {background:%2;border:1px solid %3;border-radius:12px;}
QDialog#exportDialog QTableWidget QWidget {background:transparent;}
QDialog#exportDialog QTableWidget {background:transparent;color:%4;border:0;border-radius:8px;selection-background-color:#0064d2;selection-color:white;}
QDialog#exportDialog QHeaderView {background:transparent;}
QDialog#exportDialog QHeaderView::section {background:transparent;color:%4;border:0;padding:6px;}
QDialog#exportDialog QTableWidget::item {padding:0 6px;border-bottom:1px solid %3;}
QDialog#exportDialog QTableWidget::item:hover {background:rgba(0,0,0,35);}
QDialog#exportDialog QPushButton {background:%2;color:%4;border:1px solid %3;border-radius:7px;padding:1px 8px;min-width:0;min-height:18px;font-weight:400;}
QDialog#exportDialog QPushButton:hover {background:rgba(0,0,0,45);}
QDialog#exportDialog QPushButton#exportStartButton {background:#007aff;color:white;border-color:#1686ff;}
QDialog#exportDialog QPushButton#exportStartButton:hover {background:#0064d2;}
QDialog#exportDialog QLineEdit {background:%2;color:%4;border:1px solid %3;}
QDialog#exportDialog QLabel#exportTab {color:%4;font-size:15px;padding:0;font-weight:600;}
QScrollBar:vertical {background:transparent;width:9px;}
QScrollBar::handle:vertical {background:%3;min-height:30px;border-radius:4px;}
QScrollBar::handle:vertical:disabled {background:transparent;}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {height:0;border:0;background:transparent;}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {background:transparent;}
QSplitter::handle {background:%3;width:1px;}
QProgressBar {color:%4;}
QToolTip {background:%2;color:%4;border:1px solid %3;padding:6px;border-radius:6px;font-size:12px;}
)").arg(panel, surface, border, ink);
        if (light()) style += "QComboBox::down-arrow,QSpinBox::down-arrow,QDoubleSpinBox::down-arrow {image:url(:/brand/chevron-ink.svg);} QSpinBox::up-arrow,QDoubleSpinBox::up-arrow {image:url(:/brand/chevron-up-ink.svg);}";
    }
    if (theme() != "studio")
        style += QString(" QWidget#mediaPreview, QWidget#previewToolbar, QWidget#previewTrimBar, QWidget#comparisonHeader, QLabel#previewPlaceholder {background:%1;}").arg(theme() != "studio" && !GlassMaterial::reduced() ? "transparent" : (light() ? "#fafafa" : "#292929"));
    if (widget->property("glassWindowSurface").toBool())
        style += "QMainWindow, QDialog, QDialog#comparisonDialog, QFrame#canvasPanel, QWidget#welcomePage, QWidget#welcomeBody, QWidget#welcomeRail {background:transparent;}";
    style += QString(R"(
QPushButton#welcomeOpenButton, QPushButton#welcomeUrlButton, QPushButton#welcomeNewButton {font-size:13px;font-weight:500;padding:1px 8px;min-height:18px;border-radius:6px;}
QPushButton#welcomeNewButton, QPushButton#welcomeOpenButton, QPushButton#welcomeUrlButton {background:transparent;color:%1;border:1px solid %3;}
QPushButton#welcomeNewButton:hover, QPushButton#welcomeOpenButton:hover, QPushButton#welcomeUrlButton:hover {background:%2;border:1px solid %3;}
QDialog#urlDownloadDialog QLabel {color:%1;background:transparent;font-size:14px;font-weight:500;}
QDialog#urlDownloadDialog QLineEdit {font-size:14px;font-weight:500;padding:1px 8px;}
QDialog#urlDownloadDialog QPushButton {font-size:13px;font-weight:500;padding:1px 8px;min-height:18px;}
QDialog#urlDownloadDialog QProgressBar {border:0;border-radius:3px;background:%2;}
QDialog#urlDownloadDialog QProgressBar::chunk {background:#007aff;border-radius:3px;}
)").arg(light()?"#292929":"#eeeeee",light()?"#dedee2":"#404044",light()?"#b5b5ba":"#66666b");
    style += R"(
QFrame#mediaDropZone, QFrame#mediaDropZone[dragActive="true"] {background:transparent;border:0;border-radius:18px;}
QFrame#mediaDropZone QLabel#dropTitle, QFrame#mediaDropZone QLabel#dropSubtitle, QFrame#mediaDropZone QLabel#dropHint {color:#292929;background:transparent;}
)";
    if (widget->styleSheet() != style) widget->setStyleSheet(style);
    for (auto* button : widget->findChildren<QAbstractButton*>()) {
        if (button->property("presetIconOnly").toBool()) continue;
        if (!qobject_cast<QPushButton*>(button)) continue;
        const auto symbol = ControlSymbols::forControl(button->objectName());
        if (symbol == ControlSymbols::Symbol::None) continue;
        button->setIcon(ControlSymbols::icon(symbol, button->property("variant").toString()=="primary"
            || button->objectName()=="exportStartButton" || button->objectName()=="comparePreviewButton"));
        button->setIconSize(QSize(19,15));
    }
    for (auto* child : widget->findChildren<QWidget*>()) adaptLocal(child);
}
inline void refresh()
{
    palette();
    for (auto* window : QApplication::topLevelWidgets())
        if (window->property("appearanceManaged").toBool()) apply(window);
        else { adaptLocal(window); for (auto* child : window->findChildren<QWidget*>()) adaptLocal(child); }
}
}
