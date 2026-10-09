#include "ui/LayerEditor.h"
#include "ui/HelpPopover.h"
#include "ui/VideoDownloadDialog.h"
#include <QShortcut>
#include <QClipboard>
#include <QDir>
#include <QKeyEvent>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QUuid>
#include "ui/GlassMaterial.h"
#include <QResizeEvent>
#include <QRegion>
#include "ui/Typography.h"
#include "ui/SelectionComboBox.h"
#include "ui/ModelHelp.h"
#include "MainWindow.h"
#include "ui/EnhancementWidget.h"
#include "processing/ProcessingJob.h"
#include "platform/TaskbarProgress.h"
#include "ui/Appearance.h"
#include "ui/ComparisonDialog.h"
#include "ui/EditorControls.h"
#include "ui/EncoderOptionsWidget.h"
#include "ui/ExportDialog.h"
#include "ui/InterfaceTranslator.h"
#include "ui/MediaPreviewWidget.h"
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <functional>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
bool confirmClose(QWidget* parent, const QString& title, const QString& text, const QString& action)
{
    // Taskbar thumbnail Close can arrive while the owner is minimized or behind
    // another app. Restore it before entering the modal confirmation loop.
    if (parent) {
        QWidget* owner = parent->window();
        if (owner->isMinimized())
            owner->setWindowState(owner->windowState() & ~Qt::WindowMinimized);
        owner->show();
        owner->raise();
        owner->activateWindow();
#ifdef Q_OS_WIN
        const auto hwnd = reinterpret_cast<HWND>(owner->winId());
        if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
#endif
    }
    QDialog confirmation(parent);
    confirmation.setObjectName("exitConfirmation");
    confirmation.setWindowTitle(title);
    confirmation.setFixedWidth(430);
    auto* layout = new QVBoxLayout(&confirmation);
    layout->setContentsMargins(20, 16, 20, 20);
    layout->setSpacing(18);
    auto* message = new QLabel(text, &confirmation);
    message->setWordWrap(true);
    layout->addWidget(message);
    auto* buttons = new QDialogButtonBox(&confirmation);
    auto* accept = buttons->addButton(action, QDialogButtonBox::AcceptRole);
    accept->setProperty("variant", "primary");
    auto* cancel = buttons->addButton(QCoreApplication::translate("MainWindow", "Cancelar"), QDialogButtonBox::RejectRole);
    cancel->setDefault(true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &confirmation, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &confirmation, &QDialog::reject);
    layout->addWidget(buttons);
    Appearance::apply(&confirmation);
    QTimer::singleShot(0, &confirmation, [&confirmation] {
        confirmation.raise();
        confirmation.activateWindow();
#ifdef Q_OS_WIN
        SetForegroundWindow(reinterpret_cast<HWND>(confirmation.winId()));
#endif
    });
    return confirmation.exec() == QDialog::Accepted;
}
std::filesystem::path toPath(const QString& s)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(s.toUtf8().constData());
#endif
}
QString fromPath(const std::filesystem::path& p)
{
#ifdef Q_OS_WIN
    return QString::fromStdWString(p.wstring());
#else
    return QString::fromUtf8(p.string().c_str());
#endif
}
bool isImage(const QString& path)
{
    return QSet<QString> { "png", "jpg", "jpeg", "bmp", "webp" }.contains(
        QFileInfo(path).suffix().toLower());
}
bool supportedFile(const QString& path)
{
    return QFileInfo(path).isFile() && (isImage(path) || QSet<QString> { "mp4", "mkv", "avi", "mov", "webm", "gif" }.contains(QFileInfo(path).suffix().toLower()));
}
class DropZone final : public QFrame {
public:
    DropZone(std::function<void(const QString&)> callback, QWidget* parent)
        : QFrame(parent)
        , callback_(std::move(callback))
    {
        setAcceptDrops(true);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (objectName() != "mediaDropZone") {
            QFrame::paintEvent(event);
            return;
        }
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#d6d6da"));
        painter.drawRoundedRect(QRectF(rect()),18,18);
    }
    QString localPath(const QMimeData* data) const
    {
        for (const auto& url : data->urls())
            if (url.isLocalFile() && supportedFile(url.toLocalFile()))
                return url.toLocalFile();
        return { };
    }
    void active(bool value)
    {
        setProperty("dragActive", value);
        style()->unpolish(this);
        style()->polish(this);
    }
    void dragEnterEvent(QDragEnterEvent* e) override
    {
        if (!localPath(e->mimeData()).isEmpty()) {
            e->acceptProposedAction();
            active(true);
        }
    }
    void dragLeaveEvent(QDragLeaveEvent* e) override
    {
        active(false);
        e->accept();
    }
    void dropEvent(QDropEvent* e) override
    {
        active(false);
        const auto path = localPath(e->mimeData());
        if (!path.isEmpty()) {
            e->acceptProposedAction();
            callback_(path);
        }
    }

private:
    std::function<void(const QString&)> callback_;
};
class UploadSymbol final : public QWidget {
public:
    explicit UploadSymbol(QWidget* parent)
        : QWidget(parent)
    {
        setFixedSize(64, 64);
        setAccessibleName(tr("Enviar ficheiro"));
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(
            QPen(QColor("#292929"), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(32, 39), QPointF(32, 12));
        p.drawPolyline(
            QPolygonF { QPointF(22, 22), QPointF(32, 12), QPointF(42, 22) });
        QPainterPath tray;
        tray.moveTo(13, 34);
        tray.lineTo(13, 49);
        tray.quadTo(13, 53, 17, 53);
        tray.lineTo(47, 53);
        tray.quadTo(51, 53, 51, 49);
        tray.lineTo(51, 34);
        p.drawPath(tray);
    }
};
class TitleBar final : public QWidget {
public:
    explicit TitleBar(QWidget* parent)
        : QWidget(parent)
    {
    }

protected:
    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton && window()->windowHandle())
            window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton)
            window()->isMaximized() ? window()->showNormal()
                                    : window()->showMaximized();
    }
};
class CaptionButton final : public QPushButton {
public:
    CaptionButton(int kind, QWidget* parent)
        : QPushButton(parent)
        , kind_(kind)
    {
        setFixedSize(38, 24);
        setObjectName(kind == 2 ? "windowCloseButton" : "windowCaptionButton");
        setAccessibleName(kind == 2 ? tr("Fechar")
                : kind == 0         ? tr("Minimizar")
                                    : tr("Maximizar ou restaurar"));
        if (kind != 2) setToolTip(accessibleName());
    }

protected:
    void paintEvent(QPaintEvent* e) override
    {
        QPushButton::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::WindowText), 1));
        const QPointF c(width() / 2.0, height() / 2.0);
        if (kind_ == 0)
            p.drawLine(c + QPointF(-5, 0), c + QPointF(5, 0));
        else if (kind_ == 2) {
            p.drawLine(c + QPointF(-4, -4), c + QPointF(4, 4));
            p.drawLine(c + QPointF(-4, 4), c + QPointF(4, -4));
        } else
            p.drawRect(QRectF(c.x() - 4, c.y() - 4, 8, 8));
    }

private:
    int kind_;
};
QWidget* field(const QString& text, QWidget* control, QWidget* parent)
{
    auto* wrapper = new QWidget(parent);
    wrapper->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto* v = new QVBoxLayout(wrapper);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(7);
    auto* label = new ControlSymbols::Label(text, ControlSymbols::forControl(control->objectName()), wrapper);
    label->setProperty("role", "fieldLabel");
    label->setBuddy(control);
    v->addWidget(label);
    if (auto* spin=qobject_cast<QAbstractSpinBox*>(control)) {
        spin->ensurePolished();
        spin->setMinimumWidth(90);
        spin->setMaximumWidth(QWIDGETSIZE_MAX);
        spin->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    }
    v->addWidget(control, 0, Qt::AlignLeft);
    return wrapper;
}
QString applicationStyle()
{
    return QStringLiteral(R"(
QWidget { font-size: 14px; font-weight: 500; color: #dedede; }
QMainWindow, QWidget#appRoot, QWidget#welcomePage, QWidget#welcomeBody { background: #1e1e1e; }
QWidget#titleBar { background: #303030; border-bottom: 1px solid #171717; }
QWidget#appHeader { background: #323232; border-bottom: 1px solid #191919; }
QLabel#brandLogo, QLabel#titleLogo { background: transparent; border: none; }
QMenuBar { background: transparent; border: none; font-size: 11px; }
QMenuBar::item { padding: 2px 7px; background: transparent; }
QMenuBar::item:selected { background: #484848; }
QMenu { background: #2d2d2d; border: 1px solid #505050; padding: 5px; }
QMenu::item { padding: 7px 28px 7px 18px; }
QMenu::item:selected { background: #1473e6; color: white; }
QMenu::separator { height: 1px; background: #474747; margin: 4px 8px; }
QPushButton { background: #363636; border: 1px solid #565656; border-radius: 4px; padding: 7px 14px; color: #eee; font-weight: 600; }
QPushButton:hover { background: #414141; border-color: #777; }
QPushButton:pressed { background: #292929; }
QPushButton:disabled { background: #292929; color: #707070; border-color: #363636; }
QPushButton[variant="primary"] { background: #1473e6; color: white; border: none; border-radius: 16px; padding: 7px 19px; }
QPushButton[variant="primary"]:hover { background: #0d66d0; }
QPushButton[variant="primary"]:pressed { background: #095aba; }
QPushButton[variant="primary"]:disabled { background: #304156; color: #919eae; }
QPushButton[variant="text"] { background: transparent; border: none; text-align: left; padding: 8px 16px; }
QPushButton[variant="text"]:hover { background: #303030; }
QPushButton#windowCaptionButton, QPushButton#windowCloseButton { background: transparent; border: none; border-radius: 0; padding: 0; }
QPushButton#windowCaptionButton:hover { background: #454545; }
QPushButton#windowCloseButton:hover { background: #c42b1c; }
QWidget#welcomeRail { background: #1e1e1e; }
QPushButton#chooseMediaButton {font-size:12px;font-weight:700;border:2px solid transparent;border-radius:15px;padding:0 15px;min-width:0;background:#1473e6;color:white;}
QPushButton#chooseMediaButton:hover {background:#0d66d0;}
QPushButton#chooseMediaButton:focus {border-color:#70b6ff;}
QPushButton#welcomeOpenButton, QPushButton#welcomeNewButton {font-size:12px;font-weight:700;background:transparent;border:2px solid transparent;border-radius:4px;padding:0 10px;min-width:0;}
QPushButton#welcomeOpenButton:hover, QPushButton#welcomeNewButton:hover {background:#303030;}
QPushButton#welcomeOpenButton:focus, QPushButton#welcomeNewButton:focus {border-color:#70b6ff;}
QFrame#railDivider { background: #2c2c2c; border: none; }
QLabel#welcomeTitle { color: #dfdfdf; font-size: 23px; font-weight: 400; }
QFrame#mediaDropZone { background: #232323; border: 1px solid transparent; border-radius: 6px; }
QFrame#mediaDropZone[dragActive="true"] { border: 1px solid transparent; background: #253144; }
QLabel#dropTitle { color: white; font-size: 23px; font-weight: 700; }
QLabel#dropSubtitle { color: #67aaff; font-size: 13px; font-weight: 600; }
QLabel#dropHint { color: #b5b5b5; font-size: 13px; }
QScrollArea { border: none; background: transparent; }
QWidget#editorPage, QFrame#canvasPanel { background: #181818; }
QWidget#documentBar { background: #292929; border-bottom: 1px solid #111; }
QLabel#previewFileLabel { background: #343434; color: #f1f1f1; border-right: 1px solid #161616; padding: 11px 20px; }
QLabel#documentHint { color: #aaa; padding: 0 16px; }
QWidget#inspectorPanel, QWidget#inspectorContent, QWidget#encoderOptions { background: #292929; }
QTabBar::tab { background: #222; color: #aaa; border-bottom: 2px solid #222; padding: 13px 17px; font-weight: 600; }
QTabBar::tab:selected { color: #f2f2f2; background: #292929; border-bottom: 2px solid #5c9fff; }
QTabBar::tab:disabled { color: #606060; }
QTabWidget::pane { border: none; }
QTabWidget#inspectorTabs QTabBar::tab { font-size: 11px; padding: 13px 10px; }
QTabBar#projectTabs::tab { padding: 8px 12px; }
QLabel[role="fieldLabel"] { color: #c9c9c9; }
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { background: #202020; color: #ededed; border: 1px solid #454545; border-radius: 3px; padding: 8px 10px; min-height: 17px; }
QSpinBox, QDoubleSpinBox {padding-right:26px;}
QSpinBox::up-button, QDoubleSpinBox::up-button {subcontrol-origin:border;subcontrol-position:top right;width:22px;height:18px;border-left:1px solid #454545;background:#303030;}
QSpinBox::down-button, QDoubleSpinBox::down-button {subcontrol-origin:border;subcontrol-position:bottom right;width:22px;height:18px;border-left:1px solid #454545;background:#303030;}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {background:#454545;}
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {image:url(:/brand/chevron-up.svg);width:10px;height:6px;}
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {image:url(:/brand/chevron-dark.svg);width:10px;height:6px;}
QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color: #6c9cdc; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView { background: #303030; color: #eee; border: 1px solid #555; selection-background-color: #1473e6; padding: 4px; }
QCheckBox { spacing: 8px; padding: 6px 0; }
QCheckBox::indicator { width: 14px; height: 14px; }
QSplitter::handle { background: #171717; width: 2px; }
QLabel#validationMessage { color: #ffbcab; padding: 8px 0; }
QStatusBar { background: #252525; border-top: 1px solid #111; min-height: 23px; }
QStatusBar::item { border: none; }
QLabel#statusLabel { color: #aaa; font-size: 11px; padding: 0 10px; }
QScrollBar:vertical { width: 9px; background: #252525; }
QScrollBar::handle:vertical { background: #525252; min-height: 30px; border-radius: 4px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QDialog { background: #242424; }
)");
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    QApplication::setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#292929"));
    palette.setColor(QPalette::WindowText, QColor("#dedede"));
    palette.setColor(QPalette::Base, QColor("#202020"));
    palette.setColor(QPalette::AlternateBase, QColor("#303030"));
    palette.setColor(QPalette::Text, QColor("#dedede"));
    palette.setColor(QPalette::Button, QColor("#363636"));
    palette.setColor(QPalette::ButtonText, QColor("#eeeeee"));
    palette.setColor(QPalette::Highlight, QColor("#1473e6"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#858585"));
    qApp->setPalette(palette);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(tr("FrameScale"));
    installSafeValueInput();
    qApp->installEventFilter(this);
    installHelpPopovers();
    setWindowIcon(QIcon(":/brand/framescale.ico"));
    setMinimumSize(900, 620);
    resize(1440, 900);
    buildInterface();
    updateModelChoices();
    updateOperationControls();
    pageStack_->setCurrentWidget(editorPage_);
    applyAppearance();
    auto* materialTimer = new QTimer(this);
    materialTimer->setInterval(900);
    auto updateMaterial = [this] {
        if (!isVisible() || isMinimized()) return;
        if (Appearance::theme()=="studio" || GlassMaterial::reduced()) return;
        const QImage source=preview_->currentFrame();
        if (source.isNull()) {
            if (property("glassSourceKey").isValid()) {
                setProperty("glassSourceKey", QVariant());
                setProperty("glassBackdrop", QVariant());
                centralWidget()->update();
            }
            return;
        }
        if (property("glassSourceKey").toLongLong()==source.cacheKey()) return;
        setProperty("glassSourceKey",source.cacheKey());
        setProperty("glassBackdrop",GlassMaterial::blur(source,96));
        centralWidget()->update();
    };
    connect(materialTimer,&QTimer::timeout,this,updateMaterial);
    connect(preview_,&MediaPreviewWidget::mediaReady,this,updateMaterial);
    materialTimer->start();
    exportButton_->setEnabled(false);
    QTimer::singleShot(0, this, [this] { if (inputPath_.isEmpty()) showWelcome(); });

    setWindowState(windowState() | Qt::WindowMaximized);
}
MainWindow::~MainWindow() = default;
void MainWindow::updateWindowCorners()
{
    clearMask();
    if (graphicsEffect()) graphicsEffect()->update();
}
void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateWindowCorners();
}
void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type()==QEvent::WindowStateChange) updateWindowCorners();
}


QWidget* MainWindow::buildTitleBar()
{
    auto* title = new TitleBar(this);
    title->setObjectName("titleBar");
    title->setFixedHeight(24);
    auto* row = new QHBoxLayout(title);
    row->setContentsMargins(7, 0, 0, 0);
    row->setSpacing(4);
    auto* logo = new QLabel(title);
    logo->setObjectName("titleLogo");
    logo->setPixmap(QIcon(":/brand/framescale-symbol.png").pixmap(20, 20));
    logo->setFixedSize(20, 20);
    logo->setAlignment(Qt::AlignCenter);
    row->addWidget(logo);
    auto* menus = new QMenuBar(title);
    menus->setObjectName("mainMenu");
    menus->setNativeMenuBar(false);
    auto* file = new SelectionMenu(menus);
    file->setTitle(tr("Arquivo"));
    menus->addMenu(file);
    file->menuAction()->setObjectName("fileMenuAction");
    newAction_ = file->addAction(tr("Novo"));
    newAction_->setObjectName("newFileAction");
    connect(new QShortcut(QKeySequence::New, this), &QShortcut::activated, newAction_, &QAction::trigger);
    openAction_ = file->addAction(tr("Abrir"));
    connect(new QShortcut(QKeySequence::Open, this), &QShortcut::activated, openAction_, &QAction::trigger);
    auto* download = file->addAction(tr("Abrir por URL"));
    connect(download, &QAction::triggered, this, &MainWindow::openUrl);
    file->addSeparator();
    auto* quit = file->addAction(tr("Sair"));
    quit->setObjectName("quitAction");
    for (const auto& key : { QKeySequence(Qt::CTRL | Qt::Key_Q), QKeySequence(Qt::ALT | Qt::Key_F4) })
        connect(new QShortcut(key, this), &QShortcut::activated, quit, &QAction::trigger);
    auto* settings = new SelectionMenu(menus);
    settings->setTitle(tr("Configurações"));
    menus->addMenu(settings);
    auto* preferences = settings->addAction(tr("Geral"));
    preferences->setObjectName("preferencesAction");
    menus->setFixedWidth(240);
    menus->setContentsMargins(0, 0, 0, 0);
    row->addWidget(menus);
    row->addStretch();
    auto* minimize = new CaptionButton(0, title);
    auto* maximize = new CaptionButton(1, title);
    auto* close = new CaptionButton(2, title);
    row->addWidget(minimize);
    row->addWidget(maximize);
    row->addWidget(close);
    connect(minimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maximize, &QPushButton::clicked, this,
        [this] { isMaximized() ? showNormal() : showMaximized(); });
    connect(close, &QPushButton::clicked, this, &QWidget::close);
    connect(newAction_, &QAction::triggered, this, [this] { chooseInputFile(); });
    connect(openAction_, &QAction::triggered, this,
        [this] { chooseInputFile(); });
    connect(quit, &QAction::triggered, this, &QWidget::close);
    connect(preferences, &QAction::triggered, this, [this] { showSettings(); });
    return title;
}
void MainWindow::buildInterface()
{
    auto* root = new GlassBackdrop(this);
    root->setObjectName("appRoot");
    auto* v = new QVBoxLayout(root);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    v->addWidget(buildTitleBar());
    pageStack_ = new QStackedWidget(root);
    pageStack_->setObjectName("mainPages");
    editorPage_ = buildEditorPage();
    pageStack_->addWidget(editorPage_);
    v->addWidget(pageStack_, 1);
    setCentralWidget(root);
    setStyleSheet(applicationStyle());
    // Keep an editor surface visible behind the non-modal welcome dialog. It
    // makes the first file selection feel like entering the app, not replacing
    // the whole window with a second unrelated screen.
    inspector_->setEnabled(false);
}
QWidget* MainWindow::buildWelcomePage()
{
    auto* page = new QWidget(this);
    page->setObjectName("welcomePage");
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto* rail = new QWidget(page);
    rail->setObjectName("welcomeRail");
    rail->setFixedWidth(204);
    auto* r = new QVBoxLayout(rail);
    r->setContentsMargins(18, 46, 20, 20);
    r->setSpacing(8);
    auto* create = new QPushButton(tr("Novo ficheiro"), rail);
    create->setObjectName("welcomeNewButton");
    create->setProperty("variant", "text");
    create->setAutoDefault(false);
    create->setFixedHeight(24);
    create->setMinimumWidth(0);
    create->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    r->addWidget(create, 0, Qt::AlignLeft);
    auto* open = new QPushButton(tr("Abrir"), rail);
    open->setObjectName("welcomeOpenButton");
    open->setProperty("variant", "text");
    open->setFixedHeight(24);
    r->addWidget(open, 0, Qt::AlignLeft);
    auto* openUrlButton = new QPushButton(tr("Abrir por URL"), rail);
    openUrlButton->setObjectName("welcomeUrlButton");
    openUrlButton->setFixedHeight(24);
    r->addWidget(openUrlButton, 0, Qt::AlignLeft);
    connect(openUrlButton, &QPushButton::clicked, this, &MainWindow::openUrl);
    r->addSpacing(15);
    auto* divider = new QFrame(rail);
    divider->setObjectName("railDivider");
    divider->setFixedHeight(1);
    r->addWidget(divider);
    auto* home = new QPushButton(tr("Início"), rail);
    home->setObjectName("welcomeHomeButton");
    home->setCheckable(true);
    home->setChecked(true);
    home->setFixedHeight(26);
    home->setIcon(QIcon(":/brand/home.svg"));
    home->setIconSize(QSize(16, 16));
    home->setAutoExclusive(true);
    home->setAutoDefault(false);
    home->setProperty("appearanceManaged", true);
    home->setStyleSheet("QPushButton, QPushButton:checked, QPushButton:hover, QPushButton:pressed, QPushButton:focus {text-align:left;padding:2px 10px;background:#007aff;color:white;border:none;border-radius:8px;font-size:14px;font-weight:600;}");
    connect(home, &QPushButton::clicked, home, [home] { home->setChecked(true); });
    r->addSpacing(12);
    r->addWidget(home, 0, Qt::AlignLeft);
    r->addStretch();
    row->addWidget(rail);
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* body = new QWidget(scroll);
    body->setObjectName("welcomeBody");
    auto* b = new QVBoxLayout(body);
    b->setContentsMargins(65, 16, 65, 64);
    b->setSpacing(34);
    b->addStretch(1);
    auto* title = new QLabel(tr("Bem-vindo ao FrameScale"), body);
    title->setObjectName("welcomeTitle");
    title->setFont(Typography::display(23));
    title->setStyleSheet(QString("font-family:\"%1\";").arg(Typography::displayFamily));
    title->setAlignment(Qt::AlignCenter);
    b->addWidget(title);
    auto* drop = new DropZone(
        [this](const QString& path) { trySetInputFile(path); }, body);
    drop->setObjectName("mediaDropZone");
    drop->setAttribute(Qt::WA_StyledBackground, false);
    drop->setAutoFillBackground(false);
    drop->setMinimumHeight(318);
    drop->setMaximumHeight(360);
    auto* d = new QVBoxLayout(drop);
    d->setContentsMargins(28, 28, 28, 28);
    d->setSpacing(13);
    d->addStretch();
    d->addWidget(new UploadSymbol(drop), 0, Qt::AlignHCenter);
    auto label = [drop, d](const QString& text, const char* name) {
        auto* l = new QLabel(text, drop);
        l->setObjectName(name);
        if (QString::fromLatin1(name)=="dropTitle") {
            l->setFont(Typography::display(23));
            l->setStyleSheet(QString("font-family:\"%1\";").arg(Typography::displayFamily));
        }
        l->setProperty("appearanceManaged", true);
        l->setStyleSheet(l->styleSheet() + "color:#292929;background:transparent;");
        l->setAlignment(Qt::AlignCenter);
        l->setWordWrap(true);
        d->addWidget(l);
    };
    label(tr("Arraste e solte um ficheiro"), "dropTitle");
    label(tr("PNG, JPG, BMP, WebP, GIF  ·  MP4, MKV, AVI, MOV, WebM"),
        "dropSubtitle");
    label(tr("Escolha uma imagem ou um vídeo"), "dropHint");
    auto* choose = new QPushButton(tr("Selecionar ficheiro"), drop);
    choose->setObjectName("chooseMediaButton");
    choose->setProperty("variant", "primary");
    choose->setFixedHeight(30);
    choose->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    d->addSpacing(4);
    d->addWidget(choose, 0, Qt::AlignHCenter);
    d->addStretch();
    b->addWidget(drop);
    b->addStretch(1);
    scroll->setWidget(body);
    row->addWidget(scroll, 1);
    connect(create, &QPushButton::clicked, this, [this] { chooseInputFile(); });
    connect(open, &QPushButton::clicked, this, [this] { chooseInputFile(); });
    connect(choose, &QPushButton::clicked, this, [this] { chooseInputFile(); });
    return page;
}
namespace {
class InspectorSurface final : public QFrame {
public:
    explicit InspectorSurface(QWidget* parent) : QFrame(parent) {
        setAttribute(Qt::WA_StyledBackground, false);
        setAutoFillBackground(false);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        GlassMaterial::paint(painter, QRectF(rect()),
            window()->property("glassBackdrop").value<QImage>(),
            Appearance::light(), Appearance::light() ? 190 : 185, 14, true);
    }
};
}

QWidget* MainWindow::buildInspector()
{
    inspector_ = new QWidget(this);
    inspector_->setObjectName("inspectorPanel");
    inspector_->setProperty("appearanceManaged", true);
    inspector_->setStyleSheet("QWidget#inspectorPanel {background:#181818;border:0;}");
    inspector_->setFixedWidth(300);
    auto* surround = new QVBoxLayout(inspector_);
    // Match the 16-pixel document and timeline insets; the canvas supplies the left gap.
    surround->setContentsMargins(0, 16, 16, 16);
    auto* card = new InspectorSurface(inspector_);
    card->setObjectName("inspectorCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 3);
    shadow->setColor(QColor(0,0,0,45));
    card->setGraphicsEffect(shadow);
    surround->addWidget(card);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    auto* scroll = new QScrollArea(inspector_);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto* inspectorScrollBar=scroll->verticalScrollBar();
    inspectorScrollBar->setEnabled(false);
    connect(inspectorScrollBar,&QScrollBar::rangeChanged,inspectorScrollBar,
        [inspectorScrollBar](int minimum,int maximum) {inspectorScrollBar->setEnabled(maximum>minimum);});
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setObjectName("inspectorContent");
    auto* form = new QVBoxLayout(content);
    form->setSizeConstraint(QLayout::SetMinimumSize);
    content->setMinimumWidth(0);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    form->setContentsMargins(8, 8, 8, 12);
    form->setSpacing(12);
    upscaleCheck_ = new SwitchControl(tr("Ampliação"), content);
    upscaleCheck_->setObjectName("enableUpscale");
    upscaleCheck_->setChecked(false);
    form->addWidget(upscaleCheck_);
    connect(upscaleCheck_, &QCheckBox::toggled, this,
        [this] { updateOperationControls(); });
    interpolationFields_ = new QWidget(content);
    interpolationFields_->setObjectName("interpolationFields");
    interpolationFields_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto* interpolation = new QVBoxLayout(interpolationFields_);
    interpolation->setContentsMargins(0, 0, 0, 0);
    interpolation->setSpacing(12);
    fpsSpin_ = new QSpinBox(content);
    fpsSpin_->setObjectName("targetFps");
    fpsSpin_->setRange(2, 240);
    fpsSpin_->setValue(60);
    rifeCombo_ = new SelectionComboBox(content);
    rifeCombo_->setObjectName("rifeModel");
    for (const auto& m : framescale::rifeModels())
        rifeCombo_->addItem(QString::fromStdString(m.displayName),
            QString::fromStdString(m.id));
    interpolation->addWidget(
        field(tr("Fotogramas por segundo"), fpsSpin_, content));
    interpolation->addWidget(
        field(tr("Modelo de interpolação"), rifeCombo_, content));
    // Use the same outer width for both interpolation fields.
    auto alignInterpolationFields = [this] {
        fpsSpin_->ensurePolished();
        rifeCombo_->ensurePolished();
        const int width = qMax(100, rifeCombo_->fontMetrics().horizontalAdvance(rifeCombo_->currentText()) + 40);
        fpsSpin_->setFixedWidth(width);
        rifeCombo_->setFixedWidth(width);
    };
    connect(rifeCombo_, &QComboBox::currentTextChanged, this, [alignInterpolationFields] { alignInterpolationFields(); });
    QTimer::singleShot(0, this, alignInterpolationFields);
    upscaleFields_ = new QWidget(content);
    upscaleFields_->setObjectName("upscaleFields");
    upscaleFields_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    auto* upscale = new QVBoxLayout(upscaleFields_);
    upscale->setContentsMargins(0, 0, 0, 0);
    upscale->setSpacing(12);
    engineCombo_ = new SelectionComboBox(content);
    engineCombo_->setObjectName("upscaleEngine");
    engineCombo_->addItem(
        "Real-ESRGAN", static_cast<int>(framescale::UpscaleEngine::RealESRGAN));
    engineCombo_->addItem("Real-CUGAN",
        static_cast<int>(framescale::UpscaleEngine::RealCUGAN));
    engineCombo_->addItem("Anime4K",
        static_cast<int>(framescale::UpscaleEngine::Anime4K));
    for (int i = 0; i < engineCombo_->count(); ++i)
        engineCombo_->setItemData(i, upscaleEngineHelp(engineCombo_->itemData(i).toInt()), Qt::ToolTipRole);
    engineCombo_->setToolTip(engineCombo_->currentData(Qt::ToolTipRole).toString());
    connect(engineCombo_, &QComboBox::currentIndexChanged, this, [this] {
        engineCombo_->setToolTip(engineCombo_->currentData(Qt::ToolTipRole).toString());
    });
    scaleCombo_ = new ScaleControl(content);
    scaleCombo_->setObjectName("upscaleFactor");
    scaleCombo_->setValue(2.);
    scaleCombo_->setAccessibleName(tr("Escala de 1 a 10"));
    scaleCombo_->setToolTip(helpToolTip(tr("Escala"), tr("Define o tamanho final da imagem ou vídeo.\nAcima da escala do modelo, redimensiona o resultado.")));
    modelCombo_ = new SelectionComboBox(content);
    modelCombo_->setObjectName("upscaleModel");
    upscale->addWidget(field(tr("Motor"), engineCombo_, content));
    upscale->addWidget(field(tr("Escala"), scaleCombo_, content));
    upscale->addWidget(field(tr("Modelo"), modelCombo_, content));
    denoiseCombo_ = new SelectionComboBox(content);
    denoiseCombo_->setObjectName("denoiseLevel");
    denoiseCombo_->addItem(tr("Sem redução"), 0);
    denoiseCombo_->addItem(tr("Conservador"), -1);
    for (int i = 1; i <= 3; ++i)
        denoiseCombo_->addItem(tr("Nível %1").arg(i), i);
    connect(denoiseCombo_, &QComboBox::currentIndexChanged, this, [this] {
        denoiseCombo_->setToolTip(denoiseCombo_->currentData(Qt::ToolTipRole).toString());
    });
    denoiseField_ = field(tr("Redução de ruído"), denoiseCombo_, content);
    upscale->addWidget(denoiseField_);
    form->addWidget(upscaleFields_);
    combinedCheck_ = new SwitchControl(tr("Interpolação"), content);
    combinedCheck_->setObjectName("enableInterpolation");
    form->addWidget(combinedCheck_);
    form->addWidget(interpolationFields_);
    reductionFps_ = new QSpinBox(content);
    reductionFps_->setObjectName("reductionFps");
    reductionFps_->setRange(0, 240);
    reductionFps_->setSpecialValueText(tr("Original"));
    reductionField_ = field(tr("Reduzir FPS"), reductionFps_, content);
    form->addWidget(reductionField_);
    playbackSpeed_ = new QDoubleSpinBox(content);
    playbackSpeed_->setObjectName("playbackSpeed"); playbackSpeed_->setRange(.25,4.);
    playbackSpeed_->setDecimals(2); playbackSpeed_->setSingleStep(.25); playbackSpeed_->setValue(1.);
    playbackSpeed_->setSuffix(" x"); playbackSpeed_->setKeyboardTracking(false);
    speedField_ = field(tr("Velocidade"),playbackSpeed_,content); form->addWidget(speedField_);
    connect(playbackSpeed_, &QDoubleSpinBox::valueChanged, this, [this](double value) { if(layers_ && layers_->active()) layers_->setSpeed(value); else if(preview_) preview_->setPlaybackSpeed(value); });
    enhancement_ = new EnhancementWidget(content);
    form->addWidget(enhancement_);
    form->addStretch();
    scroll->setWidget(content);
    content->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    auto* tabs = new QTabWidget(inspector_);
    tabs->setObjectName("inspectorTabs");
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setExpanding(true);
    tabs->addTab(scroll, tr("Processamento"));
    encoderOptions_ = new EncoderOptionsWidget(tabs);
    encoderOptions_->enhancementPresetValues = [this]{return enhancementValues(enhancement_->options());};
    encoderOptions_->applyEnhancementPresetValues = [this](const QVariantMap& values){enhancement_->setOptions(enhancementFromValues(values));};
    tabs->addTab(encoderOptions_, tr("Codificação"));
    layout->addWidget(tabs, 1);
    auto* footer = new QWidget(inspector_);
    auto* f = new QVBoxLayout(footer);
    f->setContentsMargins(22, 16, 22, 22);
    f->setSpacing(12);
    validationLabel_ = new QLabel(footer);
    validationLabel_->setObjectName("validationMessage");
    validationLabel_->setWordWrap(true);
    validationLabel_->hide();
    f->addWidget(validationLabel_);
    exportButton_ = new QPushButton(tr("Adicionar à fila"), footer);
    exportButton_->setObjectName("addToQueueButton");
    exportButton_->setProperty("variant", "primary");
    exportButton_->setFixedHeight(24);
    exportButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* queueRow = new QHBoxLayout;
    queueRow->setSpacing(8);
    auto* view = new QPushButton(tr("Ver fila"), footer);
    view->setObjectName("viewQueueButton");
    view->setFixedHeight(24);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    queueRow->addWidget(view, 2);
    queueRow->addWidget(exportButton_, 3);

    f->setContentsMargins(12, 12, 12, 16);
    f->addLayout(queueRow);
    connect(view, &QPushButton::clicked, this, [this] { showExportDialog(); });
    layout->addWidget(footer);

    connect(combinedCheck_, &QCheckBox::toggled, this,
        [this] { updateOperationControls(); });
    connect(engineCombo_, &QComboBox::currentIndexChanged, this,
        [this] { updateModelChoices(); });
    connect(scaleCombo_, &QComboBox::currentIndexChanged, this,
        [this] { updateModelChoices(); });
    connect(modelCombo_, &QComboBox::currentIndexChanged, this,
        [this] { updateDenoiseChoices(); });
    connect(exportButton_, &QPushButton::clicked, this, [this] { addToQueue(); });
    return inspector_;
}
QWidget* MainWindow::buildEditorPage()
{
    auto* page = new QWidget(this);
    page->setObjectName("editorPage");
    auto* row = new QHBoxLayout(page);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    auto* splitter = new QSplitter(Qt::Horizontal, page);
    splitter->setProperty("appearanceManaged",true);
    splitter->setHandleWidth(0);
    splitter->setStyleSheet("QSplitter::handle {background:transparent;border:0;width:0px;}");
    auto* canvas = new DropZone(
        [this](const QString& path) { trySetInputFile(path); }, splitter);
    canvas->setObjectName("canvasPanel");
    canvas->setMinimumWidth(0);
    auto* v = new QGridLayout(canvas);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto* document = new QWidget(canvas);
    document->setObjectName("documentBar");
    document->setProperty("appearanceManaged", true);
    document->setStyleSheet("QWidget#documentBar, QTabBar#projectTabs {background:transparent;border:0;}");
    auto* d = new QHBoxLayout(document);
    d->setContentsMargins(14, 14, 16, 0);
    d->setSpacing(0);
    projectTabs_ = new ProjectTabBar(document);
    projectTabs_->setObjectName("projectTabs");
    projectTabs_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(projectTabs_, &QTabBar::customContextMenuRequested, this, [this](const QPoint& point) {
        const int index = projectTabs_->tabAt(point);
        if (index < 0 || isImage(projects_[index].path)) return;
        projectTabs_->setCurrentIndex(index);
        activateProject(index);
        SelectionMenu menu(projectTabs_);
        preview_->addOrientationMenu(&menu);
        menu.exec(projectTabs_->mapToGlobal(point));
    });
    // This palette is set explicitly in applyAppearance. Do not let the
    // generic local-style adapter restore an older transparent tab style.
    projectTabs_->setProperty("appearanceManaged", true);
    projectTabs_->setFixedHeight(projectTabs_->fontMetrics().height() + 6);
    projectTabs_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    projectTabs_->setExpanding(false);
    projectTabs_->setDrawBase(false);
    projectTabs_->setTabsClosable(false);
    projectTabs_->setUsesScrollButtons(true);
    projectTabs_->setElideMode(Qt::ElideMiddle);
    emptyDocument_ = new QLabel(tr("Sem título"), document);
    emptyDocument_->setObjectName("emptyDocumentTitle");
    emptyDocument_->setContentsMargins(16, 0, 16, 0);
    emptyDocument_->setFixedHeight(28);
    d->addWidget(emptyDocument_);
    d->addWidget(projectTabs_, 1);
    connect(projectTabs_, &QTabBar::currentChanged, this,
        [this](int i) { activateProject(i); });
    connect(projectTabs_, &QTabBar::tabCloseRequested, this, [this](int i) {
        if (!confirmClose(this, tr("Fechar projeto"),
                tr("Fechar %1? As opções deste projeto serão descartadas.").arg(projectTabs_->tabText(i)),
                tr("Fechar projeto")))
            return;
        saveProject();
        const QSignalBlocker block(projectTabs_);
        projects_.removeAt(i);
        projectTabs_->removeTab(i);
        activeProject_ = -1;
        if (projects_.isEmpty())
            startNewProject();
        else
            activateProject(projectTabs_->currentIndex());
    });
    // Keep project tabs over the canvas without a full-width title strip.
    auto* pageLayout = new QVBoxLayout;
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    preview_ = new MediaPreviewWidget(canvas);
    connect(preview_, &MediaPreviewWidget::videoMetadataReady, this, [this] { updateOperationControls(); });
    preview_->projectDetails = [this] {
        QString text;
        if(upscaleCheck_->isChecked()) text += tr("Ampliação: %1x\nMotor: %2\nModelo: %3\n").arg(scaleCombo_->value()).arg(engineCombo_->currentText(),modelCombo_->currentText());
        if(combinedCheck_->isChecked()) text += tr("Interpolação: %1 FPS\nModelo: %2\n").arg(fpsSpin_->value()).arg(rifeCombo_->currentText());
        return text;
    };
    preview_->setObjectName("mediaPreview");
    preview_->enableFileSelection();
    connect(preview_, &MediaPreviewWidget::fileSelectionRequested, this, &MainWindow::chooseInputFile);
    connect(preview_, &MediaPreviewWidget::copyRequested, this, &MainWindow::copyToClipboard);
    connect(preview_, &MediaPreviewWidget::comparisonRequested, this, [this]{
        if(inputPath_.isEmpty()) return;
        preview_->pause();
        auto comparisonOptions = processingOptions(inputPath_);
        if (comparisonOptions.layered) {
            if (!layers_->ready() || layers_->layers().empty()) { setStatusText(tr("Aguarde a atualização da prévia das camadas.")); return; }
            comparisonOptions.inputPath = toPath(preview_->mediaPath());
        }
        ComparisonDialog comparison(comparisonOptions, preview_->positionMs()/1000., preview_->durationMs()/1000., preview_->frameRate(), this);
        comparison.exec();
    });
    v->addWidget(preview_, 0, 0);
    v->setRowStretch(0, 1);
    layers_ = new LayerEditor(canvas);
    preview_->setLayerEditorWidget(layers_);
    layers_->selectionChanged = [this] {
        if (!playbackSpeed_) return;
        const QSignalBlocker block(playbackSpeed_);
        playbackSpeed_->setValue(layers_->selectedSpeed());
        playbackSpeed_->setEnabled(!layers_->active() || layers_->hasSelection());
    };
    layers_->changed = [this] { preview_->pause(); saveProject(); };
    layers_->status = [this](const QString& text) { if(!isProcessing()) setStatusText(text); };
    layers_->seekRequested = [this](double seconds) { if(preview_->isPlaying()) preview_->pause(); preview_->seek(qRound64(seconds*1000)); };
    layers_->volumeRequested = [this](const QPoint& point) { preview_->showLayerVolume(point); };
    layers_->playRequested = [this] { if(preview_->isPlaying()) preview_->pause(); else preview_->play(); };
    layers_->previewFailed = [this](const QString& message) { preview_->failLayerPreview(message); };
    layers_->previewInvalidated = [this] { preview_->invalidateLayerPreview(layers_->layers().empty()); };
    layers_->previewReady = [this](const QString& path) {
        if (!layers_->active()) return;
        const auto orientation = preview_->orientation();
        const auto position = preview_->positionMs();
        if (path.isEmpty()) { preview_->invalidateLayerPreview(true); return; }
        else preview_->setMedia(path, position);
        QSize projectSize;
        for(const auto& layer:layers_->layers()) if(layer.kind==framescale::LayerKind::Video) { projectSize=QSize(layer.width,layer.height); break; }
        preview_->setProjectMediaInfo(inputPath_,projectSize);
        preview_->setOrientation(orientation);
        preview_->setLayerEditing(true);
    };
    auto* layerClock = new QTimer(this);
    layerClock->setTimerType(Qt::PreciseTimer);
    layerClock->setInterval(16);
    connect(layerClock, &QTimer::timeout, this, [this] {
        if(layers_->active()) {
            const double seconds=preview_->positionMs()/1000.;
            layers_->setPosition(seconds);
            const auto& layers=layers_->layers();
            const bool video=std::any_of(layers.begin(),layers.end(),[seconds](const auto& layer) {
                return layer.enabled && layer.kind==framescale::LayerKind::Video && seconds>=layer.start && seconds<layer.start+layer.duration();
            });
            preview_->setLayerVideoVisible(video);
        }
    });
    layerClock->start();
    v->addWidget(document, 0, 0, Qt::AlignTop);
    document->raise();
    splitter->addWidget(canvas);
    splitter->addWidget(buildInspector());
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setSizes({ 1050, 286 });
    pageLayout->addWidget(splitter, 1);
    row->addLayout(pageLayout);
    connect(preview_, &MediaPreviewWidget::previewError, this,
        [this](const QString& s) {
            if (!isProcessing())
                setStatusText(s);
        });
    connect(preview_, &MediaPreviewWidget::mediaReady, this, [this] {
        if (!isProcessing())
            setStatusText(tr("Pronto"));
    });
    return page;
}
void MainWindow::applyAppearance()
{
    Appearance::palette();
    Appearance::apply(this, applicationStyle());
    if (projectTabs_) {
        const bool light = Appearance::light();
        projectTabs_->setStyleSheet(QStringLiteral(
            "QTabBar#projectTabs {background:transparent;border:0;}"
            "QTabBar#projectTabs::tab {background:%1;color:%2;border:1px solid %3;"
            "border-radius:6px;margin:2px 4px;padding:0 8px;font-size:14px;font-weight:500;}"
            "QTabBar#projectTabs::tab:selected {background:%4;color:%2;border:1px solid %3;}"
            "QTabBar#projectTabs::tab:hover {background:%4;}"
            "QPushButton#documentCloseButton {color:%2;}")
            .arg(light ? "#d8d8dc" : "#303034", light ? "#202024" : "#f2f2f4",
                light ? "#b8b8be" : "#55555c", light ? "#f0f0f2" : "#424249"));
    }

    updateWindowCorners();
    if (exportDialog_)
        Appearance::apply(exportDialog_);
}
void MainWindow::showWelcome()
{
    setProperty("documentOpen", false);
    preview_->setEmptyPromptVisible(false);
    inspector_->setStyleSheet("QWidget#inspectorPanel {background:transparent;border:0;}");
    preview_->update();
    emptyDocument_->show();
    projectTabs_->hide();
    inspector_->setEnabled(false);
    if (!welcomeDialog_) {
        welcomeDialog_ = new QDialog(this);
        welcomeDialog_->setObjectName("welcomeDialog");
        welcomeDialog_->setProperty("stableSurface", true);
        welcomeDialog_->setWindowTitle(tr("Bem-vindo ao FrameScale"));
        welcomeDialog_->setWindowIcon(windowIcon());
        welcomeDialog_->setWindowModality(Qt::NonModal);
        welcomeDialog_->setFocusPolicy(Qt::StrongFocus);
        connect(welcomeDialog_, &QDialog::finished, this, [this] { preview_->setEmptyPromptVisible(true); });
        auto* layout = new QVBoxLayout(welcomeDialog_);
        layout->setContentsMargins(0, 0, 0, 0);
        welcomePage_ = buildWelcomePage();
        layout->addWidget(welcomePage_);
    }
    // Resolve the inherited font and stylesheet before the first native show.
    Appearance::apply(welcomeDialog_);
    welcomeDialog_->ensurePolished();
    if (auto* button = welcomeDialog_->findChild<QPushButton*>("welcomeNewButton")) {
        button->ensurePolished();
        button->setMinimumWidth(qMax(button->minimumSizeHint().width(), button->fontMetrics().horizontalAdvance(button->text()) + 28));
    }
    welcomeDialog_->layout()->activate();
    welcomeDialog_->resize(qMin(1160, width() - 80), qMin(640, height() - 80));
    welcomeDialog_->move(mapToGlobal(rect().center()) - welcomeDialog_->rect().center());
    welcomeDialog_->show();
    welcomeDialog_->setFocus(Qt::OtherFocusReason);
    welcomeDialog_->setWindowOpacity(1.0);
    welcomeDialog_->raise();
}
void MainWindow::enterDocument()
{
    setProperty("documentOpen", true);
    preview_->setEmptyPromptVisible(true);
    inspector_->setStyleSheet("QWidget#inspectorPanel {background:#181818;border:0;}");
    preview_->update();
    // Close before loading media; never stretch a translucent window snapshot
    // over the editor or keep it visible while the first frame is decoded.
    if (welcomeDialog_)
        welcomeDialog_->hide();
}
void MainWindow::showSettings()
{
    QDialog dialog(this);
    dialog.setObjectName("settingsDialog");
    dialog.setWindowTitle(tr("Configurações"));
    dialog.setWindowIcon(windowIcon());
    dialog.resize(520, 400);
    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(16);
    auto* row = new QHBoxLayout;
    auto* sections = new QPushButton(tr("Geral"), &dialog);
    sections->setObjectName("settingsSections");
    sections->setProperty("variant", "primary");
    sections->setCheckable(true);
    sections->setAutoExclusive(true);
    sections->setChecked(true);
    sections->setAutoDefault(false);
    sections->setFixedHeight(24);
    sections->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    row->addWidget(sections, 0, Qt::AlignTop);
    auto* settingsCard = new QWidget(&dialog);
    auto* content = new QVBoxLayout(settingsCard);
    content->setContentsMargins(12, 4, 12, 12);
    content->setSpacing(10);
    auto* heading = new QLabel(tr("Geral"), &dialog);
    heading->setFont(Typography::display());
    heading->setStyleSheet(QString("font-family:\"%1\";font-size:22px;font-weight:600;background:transparent;").arg(Typography::displayFamily));
    content->addWidget(heading);
    content->addSpacing(12);
    auto* language = new SelectionComboBox(&dialog);
    language->setObjectName("languageChoice");
    language->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    language->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    language->addItem("Português (Portugal)", "pt-PT");
    language->addItem("Português (Brasil)", "pt-BR");
    language->addItem("English", "en");
    language->setCurrentIndex(qMax(0, language->findData(InterfaceTranslator::normalizeLanguage(QSettings().value("language", "pt-PT").toString()))));
    auto* languageLabel = new QLabel(tr("Idioma"), &dialog);
    languageLabel->setBuddy(language);
    content->addWidget(languageLabel);
    content->addWidget(language, 0, Qt::AlignLeft);
    content->addSpacing(12);
    auto* appearance = new SelectionComboBox(&dialog);
    appearance->setObjectName("appearanceChoice");
    appearance->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    appearance->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    appearance->addItem(tr("Liquid Glass Claro"), "macos-light");
    appearance->addItem(tr("Liquid Glass Escuro"), "macos-dark");
    appearance->addItem(tr("Estúdio"), "studio");
    appearance->setCurrentIndex(qMax(0, appearance->findData(Appearance::theme())));
    auto* appearanceLabel = new QLabel(tr("Aparência"), &dialog);
    appearanceLabel->setBuddy(appearance);
    content->addWidget(appearanceLabel);
    content->addWidget(appearance, 0, Qt::AlignLeft);
    content->addSpacing(12);
    auto* transparency = new SwitchControl(tr("Transparência dos painéis"), &dialog);
    transparency->setObjectName("glassTransparencyChoice");
    transparency->setChecked(QSettings().value("glassTransparency", true).toBool());
    transparency->setEnabled(appearance->currentData() != "studio");
    connect(appearance, &QComboBox::currentIndexChanged, &dialog, [appearance,transparency] {
        transparency->setEnabled(appearance->currentData() != "studio");
    });
    content->addWidget(transparency);
    content->addStretch();
    row->addWidget(settingsCard, 1);
    root->addLayout(row, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("OK");
    buttons->button(QDialogButtonBox::Ok)->setProperty("variant", "primary");
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    root->addWidget(buttons);
    Appearance::apply(&dialog);
    language->ensurePolished();
    appearance->ensurePolished();
    int choiceWidth = 0;
    for (auto* choice : {language, appearance})
        for (int i=0; i<choice->count(); ++i)
            choiceWidth = qMax(choiceWidth, choice->fontMetrics().horizontalAdvance(choice->itemText(i)) + 40);
    language->setFixedWidth(choiceWidth);
    appearance->setFixedWidth(choiceWidth);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings().setValue("language", language->currentData());
        QSettings().setValue("appearance", appearance->currentData());
        QSettings().setValue("glassTransparency", transparency->isChecked());
        applyAppearance();
        Appearance::refresh();
        InterfaceTranslator::applyLanguage(language->currentData().toString());
    }
}
bool MainWindow::isProcessing() const
{
    return processingJob_ && processingJob_->isRunning();
}
void MainWindow::openUrl()
{
    VideoDownloadDialog dialog(welcomeDialog_ && welcomeDialog_->isVisible() ? static_cast<QWidget*>(welcomeDialog_.data()) : static_cast<QWidget*>(this));
    dialog.exec();
    const auto path = dialog.fileToEdit();
    if (!path.isEmpty()) openInputFile(path);
}

void MainWindow::chooseInputFile()
{
    const auto directory = inputPath_.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
        : QFileInfo(inputPath_).absolutePath();
    const auto path = QFileDialog::getOpenFileName(
        this, tr("Abrir imagem ou vídeo"), directory,
        tr("Imagens e vídeos (*.png *.jpg *.jpeg *.bmp *.webp *.mp4 *.mkv *.avi "
           "*.mov *.webm *.gif);;"
           "Imagens (*.png *.jpg *.jpeg *.bmp *.webp);;Vídeos (*.mp4 *.mkv *.avi "
           "*.mov *.webm *.gif)"));
    if (!path.isEmpty())
        trySetInputFile(path);
}
bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->matches(QKeySequence::Paste)) {
        auto* active = QApplication::activeWindow();
        const bool editorActive = active == this || (welcomeDialog_ && active == welcomeDialog_);
        bool editing = false;
        for (auto* widget = QApplication::focusWidget(); widget; widget = widget->parentWidget()) {
            if (qobject_cast<QLineEdit*>(widget) || qobject_cast<QTextEdit*>(widget)
                || qobject_cast<QPlainTextEdit*>(widget) || qobject_cast<QAbstractSpinBox*>(widget)) {
                editing = true;
                break;
            }
        }
        if (editorActive && !editing && !QApplication::activePopupWidget()
            && (!QApplication::activeModalWidget() || QApplication::activeModalWidget() == welcomeDialog_)) {
            pasteMedia();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::pasteMedia()
{
    const auto* mime = QApplication::clipboard()->mimeData();
    if (!mime) return;
    QStringList paths;
    for (const auto& url : mime->urls()) {
        if (url.isLocalFile() && supportedFile(url.toLocalFile()))
            paths.append(url.toLocalFile());
    }
    paths.removeDuplicates();
    if (!paths.isEmpty()) {
        for (const auto& path : paths)
            if (!trySetInputFile(path)) break;
        return;
    }
    const QImage image = QApplication::clipboard()->image();
    if (image.isNull()) return;
    // Keep the source beyond this window's lifetime: queued exports may still use it.
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/Clipboard");
    const QString path = directory + QStringLiteral("/Clipboard-%1.png")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!QDir().mkpath(directory) || !image.save(path, "PNG")) {
        QMessageBox::warning(this, tr("Colar imagem"), tr("Não foi possível guardar a imagem copiada."));
        return;
    }
    if (!trySetInputFile(path)) QFile::remove(path);
}

void MainWindow::openInputFile(const QString& path) { trySetInputFile(path); }
bool MainWindow::trySetInputFile(const QString& path)
{
    const auto absolute = QFileInfo(path).absoluteFilePath();
    if (!supportedFile(absolute)) {
        QMessageBox::warning(
            this, tr("Abrir arquivo"),
            tr("O arquivo não existe ou o formato não é suportado."));
        return false;
    }
    bool replace = false;
    if (activeProject_ >= 0) {
        QDialog prompt(this);
        prompt.setObjectName("openProjectDialog");
        prompt.setWindowTitle(tr("Abrir outro arquivo"));
        prompt.setMinimumWidth(440);
        auto* layout = new QVBoxLayout(&prompt);
        layout->setContentsMargins(20, 16, 20, 20);
        layout->setSpacing(12);
        auto* message = new QLabel(tr("Como deseja abrir %1?").arg(QFileInfo(absolute).fileName()), &prompt);
        message->setWordWrap(true);
        layout->addWidget(message);
        auto* tab = new QPushButton(tr("Abrir em nova aba"), &prompt);
        auto* swap = new QPushButton(tr("Substituir projeto atual"), &prompt);
        auto* cancel = new QPushButton(tr("Cancelar"), &prompt);
        tab->setProperty("variant", "primary");
        tab->setDefault(true);
        for (auto* button : {tab, swap, cancel}) {
            button->setMinimumHeight(32);
            layout->addWidget(button);
        }
        connect(tab, &QPushButton::clicked, &prompt, &QDialog::accept);
        connect(swap, &QPushButton::clicked, &prompt, [&prompt, &replace] { replace = true; prompt.accept(); });
        connect(cancel, &QPushButton::clicked, &prompt, &QDialog::reject);
        Appearance::apply(&prompt);
        if (prompt.exec() != QDialog::Accepted)
            return false;
    }
    saveProject();
    auto options = processingOptions({ });
    options.inputPath = toPath(absolute);
    options.orientation = {};
    options.layers.clear(); options.layered = false;
    options.trimStartSeconds = 0;
    options.trimEndSeconds = 0;
    if (QFileInfo(absolute).suffix().compare("gif", Qt::CaseInsensitive) == 0)
        options.outputSuffix = ".gif";
    options.mediaType = isImage(absolute) ? framescale::MediaType::Image
                                          : framescale::MediaType::Video;
    if (isImage(absolute))
        options.operation = framescale::usesUpscale(options.operation)
            ? framescale::Operation::Upscale : framescale::Operation::Copy;
    const QSignalBlocker block(projectTabs_);
    int index;
    if (replace) {
        index = activeProject_;
        projects_[index] = { absolute, options };
        projectTabs_->setTabText(index, QFileInfo(absolute).fileName());
        projectTabs_->setTabToolTip(index, QDir::toNativeSeparators(absolute));
    } else {
        index = projects_.size();
        projects_.append({ absolute, options });
        projectTabs_->addTab(QFileInfo(absolute).fileName());
        projectTabs_->setTabToolTip(index, QDir::toNativeSeparators(absolute));
        auto* close = new DocumentCloseButton(projectTabs_);
        projectTabs_->setTabButton(index, QTabBar::RightSide, close);
        connect(close, &QPushButton::clicked, this, [this, close] {
            for (int i = 0; i < projectTabs_->count(); ++i)
                if (projectTabs_->tabButton(i, QTabBar::RightSide) == close) {
                    emit projectTabs_->tabCloseRequested(i);
                    break;
                }
        });
    }
    activeProject_ = -1;
    projectTabs_->setCurrentIndex(index);
    activateProject(index);
    return true;
}
void MainWindow::saveProject()
{
    if (activeProject_ >= 0 && activeProject_ < projects_.size())
        projects_[activeProject_].options = processingOptions({ });
}
void MainWindow::activateProject(int index)
{
    if (index < 0 || index >= projects_.size())
        return;
    if (activeProject_ == index) {
        pageStack_->setCurrentWidget(editorPage_);
        return;
    }
    saveProject();
    preview_->pause();
    activeProject_ = index;
    enterDocument();
    emptyDocument_->hide();
    projectTabs_->show();
    editorPage_->setEnabled(true);
    inspector_->setEnabled(true);
    exportButton_->setEnabled(true);
    inputPath_ = projects_[index].path;
    const auto o = projects_[index].options;
    engineCombo_->setCurrentIndex(
        engineCombo_->findData(static_cast<int>(o.upscaleEngine)));
    scaleCombo_->setValue(o.scaleFactor);
    modelCombo_->setCurrentIndex(
        modelCombo_->findData(QString::fromStdString(o.upscaleModelId)));
    denoiseCombo_->setCurrentIndex(
        denoiseCombo_->findData(static_cast<int>(o.denoise)));
    rifeCombo_->setCurrentIndex(
        rifeCombo_->findData(QString::fromStdString(o.rifeModelId)));
    fpsSpin_->setRange(2, 240);
    fpsSpin_->setValue(o.targetFps);
    reductionFps_->setRange(0, 240);
    reductionFps_->setValue(o.reductionFps);
    { const QSignalBlocker block(playbackSpeed_); playbackSpeed_->setValue(o.playbackSpeed); }
    {
        const QSignalBlocker interpolationBlock(combinedCheck_),
            upscaleBlock(upscaleCheck_);
        upscaleCheck_->setChecked(framescale::usesUpscale(o.operation));
        combinedCheck_->setChecked(framescale::usesInterpolation(o.operation));
    }
    encoderOptions_->applyOptions(o);
    updateOperationControls();
    pageStack_->setCurrentWidget(editorPage_);
    preview_->setLayerEditing(!isImage(inputPath_));
    preview_->setMedia(inputPath_);
    preview_->setTrimRange(o.trimStartSeconds, o.trimEndSeconds);
    preview_->setOrientation(o.orientation);
    preview_->setVideoTimelineStart(o.videoTimelineStartMs);
    preview_->setAudioOffset(o.audioOffsetMs);
    preview_->setPlaybackSpeed(o.playbackSpeed);
    preview_->setAudioLinked(o.audioVideoLinked);
    layers_->setProject(inputPath_, o, !isImage(inputPath_));
}
void MainWindow::startNewProject()
{
    saveProject();
    activeProject_ = -1;
    inputPath_.clear();
    layers_->setProject({}, {}, false);
    preview_->clear();
    validationLabel_->hide();
    pageStack_->setCurrentWidget(editorPage_);
    exportButton_->setEnabled(false);
    showWelcome();
}
framescale::Operation MainWindow::operation() const
{
    if (combinedCheck_->isChecked() && !isImage(inputPath_))
        return upscaleCheck_->isChecked()
            ? framescale::Operation::UpscaleAndInterpolation
            : framescale::Operation::Interpolation;
    return upscaleCheck_->isChecked() ? framescale::Operation::Upscale
                                      : framescale::Operation::Copy;
}
void MainWindow::updateOperationControls()
{
    const bool video = !isImage(inputPath_);
    const bool known = preview_ && (preview_->mediaPath() == inputPath_ || (layers_ && layers_->active())) && preview_->durationMs() > 0;
    const double sourceFps = known ? preview_->frameRate() : 0.;
    combinedCheck_->setEnabled(video && (!known || sourceFps < 240.));
    if (known && sourceFps >= 240.) combinedCheck_->setChecked(false);
    fpsSpin_->setMinimum(2);
    fpsSpin_->setEnabled(known && sourceFps < 240.);
    reductionFps_->setMaximum(known ? std::max(0, std::min(240, int(std::ceil(sourceFps - .0001)) - 1)) : 240);
    reductionFps_->setEnabled(known);
    reductionField_->setVisible(video && !combinedCheck_->isChecked());
    speedField_->setVisible(video);
    if (!video)
        combinedCheck_->setChecked(false);
    Appearance::revealSection(upscaleFields_, upscaleCheck_->isChecked());
    upscaleFields_->setEnabled(upscaleCheck_->isChecked());
    Appearance::revealSection(interpolationFields_, combinedCheck_->isChecked());
    updateDenoiseChoices();
    validationLabel_->hide();
}
void MainWindow::updateModelChoices()
{
    const auto engine = static_cast<framescale::UpscaleEngine>(
        engineCombo_->currentData().toInt());
    const auto previous = modelCombo_->currentData().toString();
    {
        const QSignalBlocker blocker(modelCombo_);
        modelCombo_->clear();
        for (const auto& m : framescale::upscaleModels())
            if (m.engine == engine) {
                modelCombo_->addItem(QString::fromStdString(m.displayName), QString::fromStdString(m.id));
                modelCombo_->setItemData(modelCombo_->count() - 1,
                    upscaleModelHelp(QString::fromStdString(m.id)), Qt::ToolTipRole);
            }
        const int i = modelCombo_->findData(previous);
        if (i >= 0)
            modelCombo_->setCurrentIndex(i);
        else if (engine == framescale::UpscaleEngine::RealCUGAN)
            modelCombo_->setCurrentIndex(modelCombo_->findData("realcugan-se"));
    }
    modelCombo_->setToolTip(upscaleModelHelp(modelCombo_->currentData().toString()));
    modelCombo_->updateGeometry();
    updateDenoiseChoices();
}
void MainWindow::updateDenoiseChoices()
{
    modelCombo_->setToolTip(upscaleModelHelp(modelCombo_->currentData().toString()));
    const auto* model = framescale::findUpscaleModel(
        modelCombo_->currentData().toString().toStdString());
    const int previous = denoiseCombo_->currentData().toInt();
    denoiseField_->setVisible(model && model->engine == framescale::UpscaleEngine::RealCUGAN);
    const QSignalBlocker blocker(denoiseCombo_);
    denoiseCombo_->clear();
    for (int level : { 0, -1, 1, 2, 3 })
        if (model && framescale::supportsDenoise(*model, framescale::nativeScale(*model, scaleCombo_->value()), static_cast<framescale::DenoiseLevel>(level)))
            denoiseCombo_->addItem(level == 0 ? tr("Sem redução")
                    : level == -1             ? tr("Conservador")
                                              : tr("Nível %1").arg(level),
                level);
    for (int i = 0; i < denoiseCombo_->count(); ++i)
        denoiseCombo_->setItemData(i, upscaleDenoiseHelp(denoiseCombo_->itemData(i).toInt()), Qt::ToolTipRole);
    denoiseCombo_->setCurrentIndex(qMax(0, denoiseCombo_->findData(previous)));
    denoiseCombo_->setToolTip(denoiseCombo_->currentData(Qt::ToolTipRole).toString());
}
framescale::ProcessingOptions
MainWindow::processingOptions(const QString& output) const
{
    framescale::ProcessingOptions o;
    o.inputPath = toPath(inputPath_);
    o.outputPath = toPath(output);
    o.mediaType = isImage(inputPath_) ? framescale::MediaType::Image
                                      : framescale::MediaType::Video;
    o.operation = operation();
    o.upscaleEngine = static_cast<framescale::UpscaleEngine>(
        engineCombo_->currentData().toInt());
    o.upscaleModelId = modelCombo_->currentData().toString().toStdString();
    o.rifeModelId = rifeCombo_->currentData().toString().toStdString();
    o.denoise = static_cast<framescale::DenoiseLevel>(
        denoiseCombo_->currentData().toInt());
    o.scaleFactor = scaleCombo_->value();
    o.targetFps = fpsSpin_->value();
    o.playbackSpeed = isImage(inputPath_) ? 1. : playbackSpeed_->value();
    o.reductionFps = !isImage(inputPath_) && !combinedCheck_->isChecked() ? reductionFps_->value() : 0;
    encoderOptions_->writeOptions(o);
    o.orientation = preview_->orientation();
    o.audioOffsetMs = preview_->audioOffset();
    o.audioVideoLinked = preview_->audioLinked();
    o.videoTimelineStartMs = preview_->videoTimelineStart();
    o.trimStartSeconds = preview_->trimStartSeconds();
    o.trimEndSeconds = preview_->trimEndSeconds();
    if (layers_->active() && layers_->initialized()) {
        o.layered = true; o.layers = layers_->layers();
        o.playbackSpeed = 1.; o.audioOffsetMs = 0;
        o.trimStartSeconds = layers_->exportStart(); o.trimEndSeconds = layers_->exportEnd();
    }
    return o;
}
void MainWindow::showExportDialog()
{
    if (!exportDialog_) {
        exportDialog_ = new ExportDialog(inputPath_, { }, this);
        Appearance::apply(exportDialog_);
        exportDialog_->setWindowIcon(windowIcon());
        connect(exportDialog_, &ExportDialog::exportRequested, this,
            [this](const QString& path) { beginExport(path); });
        connect(exportDialog_, &ExportDialog::cancelRequested, this,
            [this] { cancelProcessing(); });
        connect(exportDialog_,&ExportDialog::pauseRequested,this,[this](bool paused) {
            if(processingJob_ && processingJob_->setPaused(paused)) exportDialog_->setPaused(paused);
            else setStatusText(tr("Nao foi possivel pausar ou retomar nesta etapa. Tente novamente."));
        });
        exportDialog_->setProcessingAvailable(!clipboardCopyPending_);
    }

    exportDialog_->show();
    exportDialog_->raise();
}
void MainWindow::copyToClipboard()
{
    if (inputPath_.isEmpty())
        return;
    if (isImage(inputPath_)) {
        auto* data = new QMimeData;
        data->setUrls({ QUrl::fromLocalFile(inputPath_) });
        QApplication::clipboard()->setMimeData(data);
        return;
    }
    // Completion releases the job on the next event-loop turn. Do not replace
    // it in that gap: its release callback still owns clipboard/queue state.
    if (processingJob_)
        return;

    pendingClipboardDirectory_ = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/FrameScale-copy-XXXXXX");
    if (!pendingClipboardDirectory_->isValid()) {
        setStatusText(tr("Não foi possível copiar o vídeo: %1").arg(tr("Não foi possível criar a pasta temporária.")));
        pendingClipboardDirectory_.reset();
        return;
    }

    // Clipboard copies are ordinary videos, independent of export settings.
    framescale::ProcessingOptions options;
    options.inputPath = toPath(inputPath_);
    options.operation = framescale::Operation::Copy;
    options.mediaType = framescale::MediaType::Video;
    options.orientation = preview_->orientation();
    options.audioOffsetMs = preview_->audioOffset();
    options.playbackSpeed = playbackSpeed_->value();
    options.trimStartSeconds = preview_->trimStartSeconds();
    options.trimEndSeconds = preview_->trimEndSeconds();
    if(layers_->active() && layers_->initialized()) {
        options.layered=true; options.layers=layers_->layers();
        options.playbackSpeed=1.; options.audioOffsetMs=0;
        options.trimStartSeconds=layers_->exportStart(); options.trimEndSeconds=layers_->exportEnd();
    }
    const QString output = pendingClipboardDirectory_->filePath(QStringLiteral("selection.mp4"));
    options.outputPath = toPath(output);
    const auto errors = framescale::validate(options);
    if (!errors.empty()) {
        QStringList messages;
        for (const auto& error : errors)
            messages << QString::fromUtf8(error.c_str());
        setStatusText(messages.join('\n'));
        pendingClipboardDirectory_.reset();
        return;
    }

    cancellationPending_ = false;
    clipboardCopyPending_ = true;
    if (exportDialog_)
        exportDialog_->setProcessingAvailable(false);
    processingJob_ = std::make_unique<framescale::ProcessingJob>(options, this);
    auto* job = processingJob_.get();
    connect(job, &framescale::ProcessingJob::progressChanged, this,
        [this, job](int percent, const QString& stage) {
            if (processingJob_.get() != job)
                return;
            framescale::taskbar::setProgress(this, percent);
            setStatusText(stage);
        });
    connect(job, &framescale::ProcessingJob::finished, this,
        [this, job](const QString& path) {
            if (processingJob_.get() != job)
                return;
            auto* data = new QMimeData;
            data->setUrls({ QUrl::fromLocalFile(path) });
            QApplication::clipboard()->setMimeData(data);
            if (clipboardDirectory_)
                clipboardDirectory_->remove();
            pendingClipboardDirectory_->setAutoRemove(false);
            clipboardDirectory_ = std::move(pendingClipboardDirectory_);
            setStatusText(tr("Vídeo copiado."));
            processingStopped();
        });
    connect(job, &framescale::ProcessingJob::failed, this,
        [this, job](const QString& error) {
            if (processingJob_.get() != job)
                return;
            setStatusText(tr("Não foi possível copiar o vídeo: %1").arg(error));
            processingStopped();
        });
    connect(job, &framescale::ProcessingJob::cancelled, this, [this, job] {
        if (processingJob_.get() != job)
            return;
        setStatusText(tr("Cópia do vídeo cancelada."));
        processingStopped();
    });

    preview_->pause();
    preview_->setComparisonAllowed(false);
    framescale::taskbar::setBusy(this);
    setStatusText(tr("A preparar o vídeo para copiar…"));
    job->start();
}
void MainWindow::addToQueue()
{
    if (inputPath_.isEmpty())
        return;
    if (!exportDialog_)
        showExportDialog();
    auto output = fromPath(framescale::suggestOutputPath(
        toPath(inputPath_),
        isImage(inputPath_) ? framescale::MediaType::Image
                            : framescale::MediaType::Video,
        operation(), scaleCombo_->value(), fpsSpin_->value()));
    {
        const auto opts = processingOptions(output);
        auto path = toPath(output);
        if (opts.reductionFps && !framescale::audioOnly(opts)) {
            const auto extension = path.extension();
            path.replace_extension();
            path += "_" + std::to_string(opts.reductionFps) + "fps";
            path += extension;
        }
        if (framescale::imageSequence(opts)) {
            path.replace_extension();
            path += opts.outputSuffix == ".png" ? "_PNG" : "_JPEG";
        } else
            path.replace_extension(opts.outputSuffix);
        output = fromPath(path);
    }
    const auto queuedOptions = processingOptions(output);
    if (!framescale::audioOnly(queuedOptions) && framescale::usesInterpolation(queuedOptions.operation)
        && (!preview_ || (preview_->mediaPath() != inputPath_ && !layers_->active()) || preview_->durationMs() <= 0 || fpsSpin_->value() <= preview_->frameRate() + .0001)) {
        QMessageBox::warning(this, tr("Interpola\u00e7\u00e3o"), tr("Escolha um FPS superior ao original e aguarde os dados do v\u00eddeo."));
        return;
    }
    QStringList summary;
    if (queuedOptions.audioOffsetMs && queuedOptions.keepAudio) summary << tr("Áudio: %1 ms").arg(queuedOptions.audioOffsetMs);
    if (queuedOptions.layered && !layers_->originalLayers()) summary << tr("%1 camadas").arg(queuedOptions.layers.size());
    if (queuedOptions.playbackSpeed != 1.) summary << tr("Velocidade: %1 x").arg(queuedOptions.playbackSpeed);
    if (queuedOptions.reductionFps) summary << tr("Reduzir para %1 fps").arg(queuedOptions.reductionFps);
    if (framescale::audioOnly(queuedOptions))
        summary << tr("Apenas áudio · %1").arg(QString::fromStdString(queuedOptions.outputSuffix).mid(1).toUpper());
    if (!framescale::audioOnly(queuedOptions) && framescale::usesUpscale(operation()))
        summary << tr("%1 · %2×")
                       .arg(engineCombo_->currentText())
                       .arg(scaleCombo_->value());
    if (!framescale::audioOnly(queuedOptions) && framescale::usesInterpolation(operation()))
        summary << tr("RIFE · %1 fps").arg(fpsSpin_->value());
    exportDialog_->addItem(queuedOptions, summary.join(" / "));
    exportDialog_->show();
    exportDialog_->raise();
}
void MainWindow::beginExport(const QString& output)
{
    if (isProcessing() || !exportDialog_)
        return;
    auto options = exportDialog_->activeOptions();
    options.outputPath = toPath(output);
    const auto errors = framescale::validate(options);
    if (!errors.empty()) {
        QStringList messages;
        for (const auto& e : errors)
            messages << QString::fromUtf8(e.c_str());
        exportDialog_->setFailed(messages.join('\n'));
        exportDialog_->jobReleased();
        return;
    }
    cancellationPending_ = false;
    clipboardCopyPending_ = false;
    validationLabel_->hide();
    processingJob_ = std::make_unique<framescale::ProcessingJob>(options, this);
    auto* job = processingJob_.get();
    job->preservePartialOnCancel = true;
    job->resolveOutputConflict = [this](const QString& path) {
        QMessageBox prompt(QMessageBox::Question, tr("Substituir arquivo?"),
            tr("Já existe um arquivo com este nome:\n%1").arg(path), QMessageBox::Cancel, exportDialog_);
        auto* replace = prompt.addButton(tr("Substituir"), QMessageBox::YesRole);
        auto* keep = prompt.addButton(tr("Manter ambos"), QMessageBox::AcceptRole);
        prompt.setDefaultButton(keep);
        prompt.exec();
        if (prompt.clickedButton() == replace)
            return path;
        if (prompt.clickedButton() != keep)
            return QString();
        const QFileInfo info(path);
        int index = 2;
        QString candidate;
        do {
            candidate = info.dir().filePath(info.completeBaseName() + "_" + QString::number(index++) + "." + info.suffix());
        } while (QFileInfo::exists(candidate));
        return candidate;
    };
    connect(job, &framescale::ProcessingJob::progressChanged, this,
        [this, job](int percent, const QString& stage) {
            if (processingJob_.get() != job)
                return;
            if (exportDialog_)
                exportDialog_->setProgress(percent, stage);
            framescale::taskbar::setProgress(this, percent);
            setStatusText(stage);
        });
    connect(job, &framescale::ProcessingJob::frameProgressChanged, this,
        [this, job](qint64 current, qint64 total) {
            if (processingJob_.get() == job && exportDialog_ && !cancellationPending_)
                exportDialog_->setFrameProgress(current, total);
        });
    connect(
        job, &framescale::ProcessingJob::finished, this,
        [this, job](const QString& path) {
            if (processingJob_.get() != job)
                return;
            if (exportDialog_)
                exportDialog_->setFinished(path);
            setStatusText(
                tr("Exportação concluída · %1").arg(QFileInfo(path).fileName()));
            processingStopped();
        });
    connect(job, &framescale::ProcessingJob::failed, this,
        [this, job](const QString& error) {
            if (processingJob_.get() != job)
                return;
            if (exportDialog_)
                exportDialog_->setFailed(error);
            setStatusText(tr("Não foi possível concluir a exportação."));
            processingStopped();
        });
    connect(job, &framescale::ProcessingJob::cancelled, this, [this, job] {
        if (processingJob_.get() != job)
            return;
        if (exportDialog_)
            exportDialog_->setCancelled(job->savedPartialPath());
        setStatusText(job->savedPartialPath().isEmpty() ? tr("Exportação cancelada. Nenhum trecho completo foi salvo.") : tr("Trecho salvo: %1").arg(job->savedPartialPath()));
        processingStopped();
    });

    exportDialog_->setRunning(true);
    preview_->setComparisonAllowed(false);
    framescale::taskbar::setBusy(this);
    job->start();
}
void MainWindow::cancelProcessing()
{
    if (!isProcessing() || cancellationPending_)
        return;
    cancellationPending_ = true;
    setStatusText(tr("A cancelar exportação…"));
    processingJob_->cancel();
}
void MainWindow::processingStopped()
{
    framescale::taskbar::clear(this);
    cancellationPending_ = false;
    preview_->setComparisonAllowed(true);
    inspector_->setEnabled(true);
    newAction_->setEnabled(true);
    openAction_->setEnabled(true);
    auto* job = processingJob_.get();
    const bool clipboardCopy = clipboardCopyPending_;
    QTimer::singleShot(0, this, [this, job, clipboardCopy] {
        if (processingJob_.get() == job)
            processingJob_.reset();
        if (clipboardCopy) {
            clipboardCopyPending_ = false;
            pendingClipboardDirectory_.reset();
            if (exportDialog_)
                exportDialog_->setProcessingAvailable(true);
        } else if (exportDialog_) {
            exportDialog_->jobReleased();
        }
        if (closeWhenStopped_) {
            closeWhenStopped_ = false;
            close();
        }
    });
}
void MainWindow::setStatusText(const QString& text)
{
    if (statusLabel_)
        statusLabel_->setText(text);
}
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!exitConfirmed_ && (!projects_.isEmpty() || isProcessing())) {
        if (isMinimized())
            setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        raise();
        activateWindow();
        if (!confirmClose(this, tr("Fechar FrameScale?"),
                tr("Há projetos abertos. Deseja fechar o programa? As opções não guardadas serão descartadas."), tr("Sair"))) {
            event->ignore();
            return;
        }
        exitConfirmed_ = true;
    }
    if (isProcessing()) {
        closeWhenStopped_ = true;
        cancelProcessing();
        event->ignore();
        return;
    }
    preview_->pause();
    QMainWindow::closeEvent(event);
}
bool MainWindow::nativeEvent(const QByteArray& type, void* message,
    qintptr* result)
{
#ifdef Q_OS_WIN
    auto* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST && !isMaximized() && !isFullScreen()) {
        RECT r;
        GetWindowRect(msg->hwnd, &r);
        const int x = static_cast<short>(LOWORD(msg->lParam)),
                  y = static_cast<short>(HIWORD(msg->lParam));
        const int b = qRound(6 * devicePixelRatioF());
        const bool l = x < r.left + b, rr = x >= r.right - b, t = y < r.top + b,
                   bt = y >= r.bottom - b;
        if (t && l)
            *result = HTTOPLEFT;
        else if (t && rr)
            *result = HTTOPRIGHT;
        else if (bt && l)
            *result = HTBOTTOMLEFT;
        else if (bt && rr)
            *result = HTBOTTOMRIGHT;
        else if (l)
            *result = HTLEFT;
        else if (rr)
            *result = HTRIGHT;
        else if (t)
            *result = HTTOP;
        else if (bt)
            *result = HTBOTTOM;
        else
            return QMainWindow::nativeEvent(type, message, result);
        return true;
    }
#endif
    return QMainWindow::nativeEvent(type, message, result);
}
