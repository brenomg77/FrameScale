#include "ExportDialog.h"
#include "RoundedWindow.h"

#include <QApplication>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

class QueueTable final : public QTableWidget {
public:
    using QTableWidget::QTableWidget;
protected:
    void resizeEvent(QResizeEvent* event) override {
        QTableWidget::resizeEvent(event);
        // Clip the viewport and header together; a stylesheet radius alone
        // does not clip QAbstractScrollArea's child surfaces.
        QPainterPath edge;
        edge.addRoundedRect(QRectF(rect()), 10, 10);
        setMask(QRegion(edge.toFillPolygon().toPolygon()));
    }
};

class QueueCheckDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        auto* style = item.widget ? item.widget->style() : QApplication::style();
        const auto state = item.checkState;
        item.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
        const int side = style->pixelMetric(QStyle::PM_IndicatorWidth);
        const QRectF box(option.rect.center().x()-side/2., option.rect.center().y()-side/2., side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const bool checked = state == Qt::Checked;
        const QColor ink = checked ? QColor("#0759b8") : option.palette.color(QPalette::Text);
        painter->setPen(QPen(checked ? QColor(Qt::white) : option.palette.color(QPalette::Mid),1));
        painter->setBrush(checked ? QColor(Qt::white) : option.palette.color(QPalette::Base));
        painter->drawRoundedRect(box.adjusted(.5,.5,-.5,-.5),4,4);
        if (checked) {
            painter->setPen(QPen(ink,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            const QPointF c=box.center();
            painter->drawPolyline(QPolygonF{c+QPointF(-3.5,0),c+QPointF(-1,2.5),c+QPointF(3.5,-3)});
        }
        painter->restore();
    }
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton && option.rect.contains(mouse->position().toPoint())
                && (index.flags() & Qt::ItemIsEnabled) && (index.flags() & Qt::ItemIsUserCheckable))
                return model->setData(index, index.data(Qt::CheckStateRole).toInt() == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

QLabel* label(const QString& text, const char* name, QWidget* parent)
{
    auto* result = new QLabel(text, parent);
    result->setObjectName(QString::fromLatin1(name));
    return result;
}

QString pathText(const std::filesystem::path& path)
{
#ifdef Q_OS_WIN
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromUtf8(path.string().c_str());
#endif
}

std::filesystem::path filePath(const QString& path)
{
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(path.toUtf8().constData());
#endif
}

QIcon trashIcon()
{
    QPixmap pixels(36, 36);
    pixels.fill(Qt::transparent);
    pixels.setDevicePixelRatio(2);
    QPainter painter(&pixels);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(
        QPen(QApplication::palette().color(QPalette::WindowText), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(3.5, 5), QPointF(14.5, 5));
    painter.drawPolyline(QPolygonF { QPointF(6.5, 4.5), QPointF(6.5, 2.8),
        QPointF(11.5, 2.8), QPointF(11.5, 4.5) });
    painter.drawPolyline(QPolygonF { QPointF(5, 7), QPointF(5.7, 15.1),
        QPointF(12.3, 15.1), QPointF(13, 7) });
    painter.drawLine(QPointF(7.5, 8), QPointF(7.8, 12.5));
    painter.drawLine(QPointF(10.5, 8), QPointF(10.2, 12.5));
    return QIcon(pixels);
}

} // namespace

ExportDialog::ExportDialog(const QString& inputPath,
    const QString& suggestedOutput, QWidget* parent)
    : QDialog(parent)
{
    setObjectName("exportDialog");
    setWindowTitle(tr("Fila de exportação"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::WindowMinMaxButtonsHint, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumSize(760, 530);
    resize(920, 580);
    setModal(false);
    video_ = QStringList { "mp4", "mkv", "mov", "avi", "webm", "m4v", "gif" }.contains(
        QFileInfo(inputPath).suffix().toLower());

    setStyleSheet(QStringLiteral(R"(
        QDialog#exportDialog { background: #202020; color: #e5e5e5; }
        QDialog#exportDialog QLabel { color: #d6d6d6; background: transparent; font-size: 13px; }
        QDialog#exportDialog QFrame#exportPanel { background: #262626; border: 1px solid #151515; }
        QDialog#exportDialog QFrame#exportRenderPanel { background: #1b1b1b; border: 1px solid #111111; }
        QDialog#exportDialog QLabel#exportTab { font-weight: 600; color: #dedede; border:0; padding: 10px 0px; }
        QDialog#exportDialog QLabel#exportMuted { color: #929292; font-size: 13px; }
        QDialog#exportDialog QLabel#exportStatus { color: #94c3ff; }
        QDialog#exportDialog QLabel#exportStage { color: #eee; font-weight: 600; font-size: 14px; }
        QDialog#exportDialog QLabel#exportFrameCount { color: #f2f2f2; font-size: 16px; font-weight: 600; }
        QDialog#exportDialog QLabel#exportElapsed { color: #eee; font-size: 14px; font-weight: 600; }
        QDialog#exportDialog QLineEdit { background: #191919; color: #dedede; border: 1px solid #454545; border-radius: 7px; padding: 3px 10px; font-size:13px; }
        QDialog#exportDialog QLineEdit:focus { border-color: #5098df; }
        QDialog#exportDialog QLineEdit:disabled { color: #898989; border-color: #383838; }
        QDialog#exportDialog QPushButton { min-height: 20px; min-width: 64px; background: #343434; color: #e3e3e3; border: 1px solid #575757; border-radius: 7px; padding: 3px 12px; font-size: 13px; font-weight: 600; }
        QDialog#exportDialog QPushButton:hover { background: #424242; border-color: #858585; }
        QDialog#exportDialog QPushButton:pressed { background: #292929; }
        QDialog#exportDialog QPushButton:disabled { background: #292929; color: #757575; border-color: #3b3b3b; }
        QDialog#exportDialog QPushButton#exportStartButton { background: #1473e6; border-color: #1473e6; color: white; }
        QDialog#exportDialog QPushButton#exportStartButton:hover { background: #2684f4; border-color: #2684f4; }
        QDialog#exportDialog QPushButton#exportStartButton:disabled { background: #314860; color: #91a2b3; border-color: #314860; }
        QDialog#exportDialog QToolButton#exportTrashButton { background: transparent; border: 0; border-radius: 4px; padding: 5px; }
        QDialog#exportDialog QToolButton#exportTrashButton:hover { background: #584043; }
        QDialog#exportDialog QTableWidget { background: #202020; color: #eee; border: 0; gridline-color: #383838; selection-background-color: #354f70; padding:0; }
        QDialog#exportDialog QTableWidget::item {padding:0 6px;}
        QDialog#exportDialog QHeaderView::section { background: #292929; color: #bbb; border: 0; padding: 5px 8px; }
        QDialog#exportDialog QProgressBar { color:white; background:#292d34; border:1px solid #697383; border-radius:6px; min-height:26px; max-height:26px; text-align:center; }
        QDialog#exportDialog QProgressBar::chunk { background:#0968cf; border:none; border-radius:5px; }
    )"));

    auto* body = new QVBoxLayout(this);
    body->setContentsMargins(20, 16, 20, 20);
    body->setSpacing(16);
    auto* queuePanel = new QFrame(this);
    queuePanel->setObjectName("exportPanel");
    auto* queue = new QVBoxLayout(queuePanel);
    queue->setContentsMargins(16, 14, 16, 16);
    queue->setSpacing(13);
    auto* tabRow = new QHBoxLayout;
    tabRow->addWidget(label(tr("Fila de renderização"), "exportTab", queuePanel));
    tabRow->addStretch();
    statusLabel_ = label({ }, "exportStatus", queuePanel);
    tabRow->addWidget(statusLabel_);
    queue->addLayout(tabRow);

    table_ = new QueueTable(0, 4, queuePanel);
    table_->setObjectName("exportQueue");
    table_->setItemDelegateForColumn(0, new QueueCheckDelegate(table_));
    table_->setHorizontalHeaderLabels(
        { tr("Exportar"), tr("Arquivo"), tr("Estado"), { } });
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table_->horizontalHeader()->resizeSection(0, 76);
    table_->horizontalHeaderItem(0)->setTextAlignment(Qt::AlignCenter);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Fixed);
    table_->horizontalHeader()->resizeSection(2, 104);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table_->horizontalHeader()->resizeSection(3, 40);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(30);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setMinimumHeight(155);
    table_->setMouseTracking(true);
    table_->viewport()->setAutoFillBackground(false);
    table_->setFrameShape(QFrame::NoFrame);
    queue->addWidget(table_, 1);

    auto* settings = new QGridLayout;
    settings->setHorizontalSpacing(12);
    settings->setVerticalSpacing(12);
    settings->setColumnStretch(1, 1);
    settings->setColumnMinimumWidth(0, 76);
    settings->addWidget(label(tr("Definições"), "exportMuted", queuePanel), 0, 0);
    summaryLabel_ = label({ }, "exportSummary", queuePanel);
    summaryLabel_->setWordWrap(true);
    settings->addWidget(summaryLabel_, 0, 1, 1, 2);
    auto* destinationLabel = label(tr("Saída para"), "exportMuted", queuePanel);
    settings->addWidget(destinationLabel, 1, 0);
    outputEdit_ = new QLineEdit(suggestedOutput, queuePanel);
    outputEdit_->setObjectName("exportOutputPath");
    outputEdit_->setPlaceholderText(
        tr("Escolha onde guardar o arquivo exportado"));
    outputEdit_->setAccessibleName(tr("Arquivo de saída"));
    destinationLabel->setBuddy(outputEdit_);
    settings->addWidget(outputEdit_, 1, 1);
    browseButton_ = new QPushButton(tr("Escolher…"), queuePanel);
    browseButton_->setObjectName("exportBrowseButton");
    settings->addWidget(browseButton_, 1, 2);
    queue->addLayout(settings);
    body->addWidget(queuePanel, 1);

    auto* renderPanel = new QFrame(this);
    renderPanel->setObjectName("exportRenderPanel");
    auto* render = new QVBoxLayout(renderPanel);
    render->setContentsMargins(20, 18, 20, 18);
    render->setSpacing(17);
    stageLabel_ = label({ }, "exportStage", renderPanel);
    stageLabel_->setWordWrap(true);
    render->addWidget(stageLabel_);
    progressBar_ = new QProgressBar(renderPanel);
    progressBar_->setObjectName("exportProgressBar");
    progressBar_->setRange(0, 1000);
    progressBar_->setValue(0);
    progressBar_->setTextVisible(true);
    progressBar_->setMinimumHeight(26);
    progressBar_->setAlignment(Qt::AlignCenter);
    progressBar_->setFormat("0%");
    render->addWidget(progressBar_);
    auto* statistics = new QHBoxLayout;
    frameCountLabel_ = label(tr("0 / — frames"), "exportFrameCount", renderPanel);
    frameCountLabel_->hide();
    statistics->addStretch();
    speedLabel_ = label(tr("Frames/s: %1").arg(QLocale().toString(0.0, 'f', 1)),
        "exportSpeed", renderPanel);
    speedLabel_->hide();
    statistics->addStretch();
    auto* time = new QVBoxLayout;
    time->setSpacing(6);
    time->addWidget(label(tr("TEMPO DECORRIDO"), "exportMuted", renderPanel));
    elapsedLabel_ = label("00:00:00", "exportElapsed", renderPanel);
    time->addWidget(elapsedLabel_);
    statistics->addLayout(time);
    render->addLayout(statistics);
    body->addWidget(renderPanel);
    renderPanel->hide();

    auto* actions = new QHBoxLayout;
    folderButton_ = new QPushButton(tr("Abrir pasta"), this);
    folderButton_->setObjectName("exportOpenFolderButton");
    actions->addWidget(folderButton_);
    actions->addStretch();
    pauseButton_=new QPushButton(tr("Pausar"),this);
    pauseButton_->setObjectName("exportPauseButton");
    actions->addWidget(pauseButton_);
    connect(pauseButton_,&QPushButton::clicked,this,[this]{emit pauseRequested(!paused_);});
    cancelButton_ = new QPushButton(tr("Cancelar"), this);
    cancelButton_->setObjectName("exportCancelButton");
    actions->addWidget(cancelButton_);
    closeButton_ = new QPushButton(tr("Fechar"), this);
    closeButton_->setObjectName("exportCloseButton");
    closeButton_->hide();
    startButton_ = new QPushButton(tr("Exportar"), this);
    startButton_->setObjectName("exportStartButton");
    startButton_->setDefault(true);
    actions->addWidget(startButton_);
    actions->setSpacing(8);
    for (auto* button : {folderButton_, pauseButton_, cancelButton_, closeButton_, startButton_, browseButton_})
        button->setFixedHeight(26);
    outputEdit_->setFixedHeight(26);
    body->addLayout(actions);

    elapsedTimer_ = new QTimer(this);
    elapsedTimer_->setInterval(500);
    connect(elapsedTimer_, &QTimer::timeout, this, &ExportDialog::updateElapsed);
    connect(table_, &QTableWidget::currentCellChanged, this,
        [this](int row, int, int, int) { selectItem(row); });
    connect(table_, &QTableWidget::itemChanged, this,
        [this] { updateControls(); });
    connect(outputEdit_, &QLineEdit::textChanged, this, [this] {
        if (!updatingSelection_) {
            saveOutput();
            updateControls();
        }
    });
    connect(browseButton_, &QPushButton::clicked, this,
        &ExportDialog::chooseOutput);
    connect(cancelButton_, &QPushButton::clicked, this,
        &ExportDialog::requestCancel);
    connect(closeButton_, &QPushButton::clicked, this, &QWidget::close);
    connect(startButton_, &QPushButton::clicked, this,
        [this] { startQueue(); });
    connect(folderButton_, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(outputPath()).absolutePath()));
    });
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, table_);
    deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(deleteShortcut, &QShortcut::activated, this,
        &ExportDialog::removeSelectedItems);
    updateControls();
}

int ExportDialog::indexOf(quint64 id) const
{
    for (int row = 0; row < entries_.size(); ++row)
        if (entries_[row].id == id)
            return row;
    return -1;
}

bool ExportDialog::busy() const
{
    return !processingAvailable_ || running_ || awaitingRelease_ || nextScheduled_ || !pending_.isEmpty();
}

void ExportDialog::setProcessingAvailable(bool available)
{
    processingAvailable_ = available;
    if (available && !running_ && !awaitingRelease_ && !pending_.isEmpty() && !nextScheduled_) {
        nextScheduled_ = true;
        QTimer::singleShot(0, this, &ExportDialog::startNext);
    }
    updateControls();
}

bool ExportDialog::editable(quint64 id) const
{
    return id != 0 && indexOf(id) >= 0 && id != active_ && !pending_.contains(id);
}

void ExportDialog::addItem(const framescale::ProcessingOptions& options,
    const QString& summary)
{
    saveOutput();
    const quint64 id = nextId_++;
    const int row = entries_.size();
    {
        const QSignalBlocker blocker(table_);
        entries_.append({ id, options, summary, State::Ready });
        table_->insertRow(row);
        auto* check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        check->setCheckState(Qt::Checked);
        table_->setItem(row, 0, check);
        const QString filename = QFileInfo(pathText(options.inputPath)).fileName();
        table_->setItem(row, 1, new QTableWidgetItem(filename));
        table_->item(row, 1)->setToolTip(pathText(options.inputPath));
        table_->setItem(row, 2, new QTableWidgetItem);
        auto* trash = new QToolButton(table_);
        trash->setObjectName("exportTrashButton");
        trash->setProperty("queueId", QVariant::fromValue(id));
        trash->setIcon(trashIcon());
        trash->setIconSize(QSize(18, 18));
        trash->setToolTip(tr("Remover %1 da fila (Delete)").arg(filename));
        trash->setAccessibleName(tr("Remover %1 da fila").arg(filename));
        connect(trash, &QToolButton::clicked, this, [this, id] { removeItem(id); });
        table_->setCellWidget(row, 3, trash);
        updateRow(row);
    }
    table_->setCurrentCell(row, 1);
    selectItem(row);
    updateControls();
}

void ExportDialog::saveOutput()
{
    if (updatingSelection_ || !editable(selected_))
        return;
    const int row = indexOf(selected_);
    const auto path = filePath(outputPath());
    if (entries_[row].options.outputPath == path)
        return;
    entries_[row].options.outputPath = path;
    // Editing a completed destination makes that snapshot exportable again.
    if (entries_[row].state == State::Finished) {
        entries_[row].state = State::Ready;
        const QSignalBlocker blocker(table_);
        table_->item(row, 0)->setCheckState(Qt::Checked);
        updateRow(row);
    }
}

void ExportDialog::selectItem(int row)
{
    if (updatingSelection_)
        return;
    saveOutput();
    updatingSelection_ = true;
    if (row < 0 || row >= entries_.size()) {
        selected_ = 0;
        outputEdit_->clear();
        summaryLabel_->clear();
    } else {
        const auto& entry = entries_[row];
        selected_ = entry.id;
        outputEdit_->setText(
            QDir::toNativeSeparators(pathText(entry.options.outputPath)));
        outputEdit_->setCursorPosition(0);
        summaryLabel_->setText(entry.summary);
        video_ = entry.options.mediaType == framescale::MediaType::Video;
    }
    updatingSelection_ = false;
    updateControls();
}

void ExportDialog::removeItem(quint64 id)
{
    const int row = indexOf(id);
    if (row < 0 || id == active_)
        return;
    saveOutput();
    pending_.removeAll(id);
    const quint64 previousSelection = selected_;
    selected_ = 0;
    {
        const QSignalBlocker blocker(table_);
        entries_.removeAt(row);
        table_->removeRow(row);
    }
    int next = indexOf(previousSelection);
    if (next < 0 && !entries_.isEmpty())
        next = std::min(row, int(entries_.size()) - 1);
    if (next >= 0)
        table_->setCurrentCell(next, 1);
    selectItem(next);
}

void ExportDialog::removeSelectedItems()
{
    QList<quint64> ids;
    for (const auto& index : table_->selectionModel()->selectedRows())
        if (index.row() >= 0 && index.row() < entries_.size())
            ids.append(entries_[index.row()].id);
    for (quint64 id : ids)
        removeItem(id);
}

void ExportDialog::updateRow(int row, const QString& detail)
{
    if (row < 0 || row >= entries_.size())
        return;
    const QSignalBlocker blocker(table_);
    QString status;
    switch (entries_[row].state) {
    case State::Ready:
        status = tr("Na fila");
        break;
    case State::Queued:
        status = tr("A aguardar");
        break;
    case State::Running:
        status = paused_ ? tr("Pausado") : tr("Renderizando");
        break;
    case State::Finished:
        status = tr("Concluído");
        break;
    case State::Failed:
        status = tr("Falhou");
        break;
    case State::Cancelled:
        status = tr("Cancelado");
        break;
    }
    table_->item(row, 2)->setText(status);
    table_->item(row, 2)->setToolTip(detail);
}

void ExportDialog::updateControls()
{
    if (!outputEdit_)
        return;
    const QSignalBlocker blocker(table_);
    bool hasChecked = false;
    for (int row = 0; row < entries_.size(); ++row) {
        auto* check = table_->item(row, 0);
        if (!check)
            continue;
        const auto& entry = entries_[row];
        hasChecked |= check->checkState() == Qt::Checked;
        check->setFlags(Qt::ItemIsSelectable | (editable(entry.id) ? Qt::ItemIsEnabled | Qt::ItemIsUserCheckable : Qt::NoItemFlags));
        if (auto* trash = table_->cellWidget(row, 3))
            trash->setEnabled(entry.id != active_);
    }
    outputEdit_->setEnabled(editable(selected_));
    browseButton_->setEnabled(editable(selected_));
    startButton_->setEnabled(!busy() && hasChecked);
    pauseButton_->setVisible(running_);
    pauseButton_->setEnabled(running_ && !cancellationPending_);
    pauseButton_->setText(paused_ ? tr("Retomar") : tr("Pausar"));
    cancelButton_->setVisible(running_ || !pending_.isEmpty());
    cancelButton_->setEnabled(!cancellationPending_);
    cancelButton_->setText(cancellationPending_ ? tr("Cancelando…")
                                                : tr("Cancelar"));
    const int selectedRow = indexOf(selected_);
    folderButton_->setVisible(selectedRow >= 0 && entries_[selectedRow].state == State::Finished);
    if (!running_) {
        statusLabel_->clear();
        stageLabel_->clear();
        progressBar_->setRange(0,1000);
        progressBar_->setValue(0);
        progressBar_->setFormat("0%");
    }
}

framescale::ProcessingOptions ExportDialog::activeOptions() const
{
    return activeSnapshot_;
}

QString ExportDialog::outputPath() const
{
    const QString path = outputEdit_->text().trimmed();
    return path.isEmpty() ? QString()
                          : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

void ExportDialog::startQueue()
{
    if (busy())
        return;
    saveOutput();
    for (int row = 0; row < entries_.size(); ++row) {
        auto& entry = entries_[row];
        const bool chosen = table_->item(row, 0)->checkState() == Qt::Checked;
        if (chosen) {
            pending_.append(entry.id);
            entry.state = State::Queued;
            updateRow(row);
        }
    }
    startNext();
}

void ExportDialog::startNext()
{
    nextScheduled_ = false;
    if (!processingAvailable_ || running_ || awaitingRelease_)
        return;
    while (!pending_.isEmpty()) {
        active_ = pending_.takeFirst();
        const int row = indexOf(active_);
        if (row < 0)
            continue;
        activeSnapshot_ = entries_[row].options;
        entries_[row].state = State::Running;
        awaitingRelease_ = true;
        table_->setCurrentCell(row, 1);
        selectItem(row);
        updateRow(row);
        setRunning(true);
        // A snapshot, rather than the selection editor, is the export source.
        emit exportRequested(pathText(activeSnapshot_.outputPath));
        return;
    }
    active_ = 0;
    updateControls();
}

void ExportDialog::jobReleased()
{
    if (running_ || !awaitingRelease_)
        return;
    awaitingRelease_ = false;
    active_ = 0;
    cancellationPending_ = false;
    if (!pending_.isEmpty() && !nextScheduled_) {
        nextScheduled_ = true;
        QTimer::singleShot(0, this, &ExportDialog::startNext);
    }
    updateControls();
}

void ExportDialog::setProcessingSummary(const QString& summary)
{
    const int row = indexOf(selected_);
    if (row >= 0 && editable(selected_))
        entries_[row].summary = summary;
    summaryLabel_->setText(summary);
}

void ExportDialog::setProgress(int percent, const QString& stage)
{
    if (!running_ || paused_)
        return;
    overallPercent_ = std::max(overallPercent_, std::clamp(percent, 0, 100));
    // The label and fill describe the same measured stage, never weighted work.
    if (percent >= 100) {
        progressBar_->setRange(0, 1000);
        progressBar_->setValue(1000);
        progressBar_->setFormat(stageTotal_ > 0 ? QString("%1/%1 (100%)").arg(stageTotal_) : "100%");
    }
    if (!stage.isEmpty() && stage != currentStage_) {
        currentStage_ = stage;
        if (percent < 100) {
            stageFrames_ = 0;
            stageTotal_ = 0;
            progressBar_->setRange(0, 0);
            progressBar_->setFormat(QString());
            stageTimer_.restart();
            rateSamples_.clear();
            rateSamples_.append({ 0, 0 });
            frameCountLabel_->setText(tr("0 / — frames"));
            speedLabel_->setText(tr("Frames/s: —"));
        }
    }
    if (!stage.isEmpty())
        stageLabel_->setText(stage);
}

void ExportDialog::setFrameProgress(qint64 current, qint64 total)
{
    if (!running_ || paused_)
        return;
    current = std::max(qint64(0), current);
    total = std::max(qint64(0), total);
    if (current == 0 && stageFrames_ > 0) {
        stageFrames_ = 0;
        stageTimer_.restart();
        rateSamples_.clear();
        rateSamples_.append({ 0, 0 });
    }
    if (!stageTimer_.isValid())
        stageTimer_.start();
    // Only counts reported by the worker are displayed: timers update elapsed
    // time/rate, never invent intermediate frames or complete a stage early.
    stageFrames_ = std::max(stageFrames_, current);
    if (total > 0)
        stageFrames_ = std::min(stageFrames_, total);
    stageTotal_ = total;
    progressBar_->setRange(0, total > 0 ? 1000 : 0);
    if (total > 0) {
        const int percent = int(stageFrames_ * 100 / total);
        progressBar_->setValue(int(stageFrames_ * 1000 / total));
        progressBar_->setFormat(QString("%1/%2 (%3%)").arg(stageFrames_).arg(total).arg(percent));
    } else
        progressBar_->setFormat(QString());
    frameCountLabel_->setText(
        total > 0 ? tr("%1 / %2 frames").arg(stageFrames_).arg(total)
                  : tr("%1 / — frames").arg(stageFrames_));
    updateSpeed();
}

void ExportDialog::setPaused(bool paused)
{
    if(!running_ || paused_==paused) return;
    if(paused) pauseStarted_=elapsed_.elapsed();
    else pausedTime_+=elapsed_.elapsed()-pauseStarted_;
    paused_=paused;
    if(paused) {
        elapsedTimer_->stop();
        // Busy indicators must not keep animating while the worker is suspended.
        progressBar_->setRange(0,1000);
        progressBar_->setValue(stageTotal_>0 ? int(stageFrames_*1000/stageTotal_) : 0);
        progressBar_->setFormat(stageTotal_>0
            ? tr("Pausado - %1/%2").arg(stageFrames_).arg(stageTotal_) : tr("Pausado"));
        stageLabel_->setText(tr("Pausado - %1").arg(currentStage_));
    } else {
        stageTimer_.restart(); rateSamples_.clear();
        rateSamples_.append({0,stageFrames_});
        setFrameProgress(stageFrames_,stageTotal_);
        stageLabel_->setText(currentStage_);
        elapsedTimer_->start();
    }
    statusLabel_->setText(paused ? tr("Pausado") : tr("A renderizar"));
    updateRow(indexOf(active_));
    updateElapsed(); updateControls();
    // Refresh after Qt has laid out and painted all modified children.
    QTimer::singleShot(0,this,[this] {
        if(graphicsEffect()) graphicsEffect()->update();
        update();
    });
}

void ExportDialog::setRunning(bool running)
{
    if (running_ == running)
        return;
    running_ = running;
    progressBar_->parentWidget()->setVisible(running);
    if (running) {
        paused_=false; pausedTime_=0; pauseStarted_=0;
        cancellationPending_ = false;
        overallPercent_ = 0;
        currentStage_.clear();
        stageFrames_ = 0;
        stageTotal_ = 0;
        statusLabel_->setText(tr("Renderizando"));
        setProgress(0, tr("Preparando exportação…"));
        setFrameProgress(0, 0);
        stoppedElapsed_ = 0;
        elapsed_.start();
        elapsedTimer_->start();
    } else {
        stoppedElapsed_ = elapsed_.isValid() ? (paused_ ? pauseStarted_ : elapsed_.elapsed())-pausedTime_ : 0;
        paused_=false;
        elapsedTimer_->stop();
    }
    updateElapsed();
    updateControls();
}

void ExportDialog::finishItem(State state, const QString& detail)
{
    if (!running_)
        return;
    const int row = indexOf(active_);
    if (row >= 0) {
        entries_[row].state = state;
        updateRow(row, detail);
        if (state == State::Finished) {
            const QSignalBlocker blocker(table_);
            table_->item(row, 0)->setCheckState(Qt::Unchecked);
        }
    }
    setRunning(false);
    // The owner explicitly releases its old job before jobReleased() permits
    // another request. This also covers failures emitted synchronously by
    // start().
}

void ExportDialog::setFinished(const QString& path)
{
    if (!running_)
        return;
    const int row = indexOf(active_);
    if (row >= 0 && !path.isEmpty()) {
        entries_[row].options.outputPath = filePath(path);
        if (selected_ == active_) {
            const QSignalBlocker blocker(outputEdit_);
            outputEdit_->setText(QDir::toNativeSeparators(path));
        }
    }
    setProgress(100, tr("Exportação concluída"));
    statusLabel_->setText(tr("Concluído"));
    finishItem(State::Finished);
}

void ExportDialog::setFailed(const QString& message)
{
    if (!running_)
        return;
    statusLabel_->setText(tr("Falhou"));
    stageLabel_->setText(
        message.isEmpty() ? tr("Não foi possível exportar o arquivo.") : message);
    finishItem(State::Failed, message);
}

void ExportDialog::clearPending()
{
    for (quint64 id : pending_) {
        const int row = indexOf(id);
        if (row >= 0) {
            entries_[row].state = State::Ready;
            updateRow(row);
        }
    }
    pending_.clear();
}

void ExportDialog::setCancelled(const QString& partialPath)
{
    if (!running_)
        return;
    clearPending();
    statusLabel_->setText(tr("Cancelado"));
    const QString detail = partialPath.isEmpty() ? tr("Exportação cancelada. Nenhum trecho completo foi salvo.") : tr("Trecho salvo: %1").arg(partialPath);
    stageLabel_->setText(detail);
    stageLabel_->setWordWrap(true);
    finishItem(State::Cancelled, detail);
}

void ExportDialog::chooseOutput()
{
    if (!editable(selected_))
        return;
    const auto& options = entries_[indexOf(selected_)].options;
    const bool sequence = options.mediaType == framescale::MediaType::Video && (options.outputSuffix == ".png" || options.outputSuffix == ".jpg" || options.outputSuffix == ".jpeg");
    // The queue destination cannot silently change the chosen output module.
    const QString suffix = QString::fromStdString(options.outputSuffix);
    const QString filters = sequence ? tr("Pasta da sequência (*)") : tr("Arquivo de saída (*%1)").arg(suffix);
    const QString selected = QFileDialog::getSaveFileName(this,
        sequence ? tr("Nome da pasta da sequência") : tr("Arquivo de saída"), outputPath(), filters, nullptr, QFileDialog::DontConfirmOverwrite);
    if (!selected.isEmpty())
        outputEdit_->setText(QDir::toNativeSeparators(selected));
}

void ExportDialog::requestCancel()
{
    clearPending();
    if (!running_ || cancellationPending_) {
        updateControls();
        return;
    }
    if(paused_) setPaused(false);
    cancellationPending_ = true;
    statusLabel_->setText(tr("Cancelando"));
    updateControls();
    emit cancelRequested();
}

void ExportDialog::updateSpeed()
{
    if (!running_)
        return;
    const qint64 now = stageTimer_.isValid() ? stageTimer_.elapsed() : 0;
    if (rateSamples_.isEmpty())
        rateSamples_.append({ now, stageFrames_ });
    if (now - rateSamples_.last().first < 200)
        return;
    rateSamples_.append({ now, stageFrames_ });
    while (rateSamples_.size() > 2 && now - rateSamples_[1].first > 2000)
        rateSamples_.removeFirst();
    const auto first = rateSamples_.first();
    const double rate = now > first.first
        ? std::max<qint64>(0, stageFrames_ - first.second) * 1000. / (now - first.first)
        : 0.;
    speedLabel_->setText(
        tr("Frames/s: %1").arg(QLocale().toString(rate, 'f', 1)));
}
void ExportDialog::updateElapsed()
{
    if (running_ && !paused_)
        updateSpeed();
    const qint64 seconds = (running_ && elapsed_.isValid() ? (paused_ ? pauseStarted_ : elapsed_.elapsed())-pausedTime_ : stoppedElapsed_) / 1000;
    elapsedLabel_->setText(QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600, 2, 10, QChar('0'))
            .arg((seconds / 60) % 60, 2, 10, QChar('0'))
            .arg(seconds % 60, 2, 10, QChar('0')));
}

void ExportDialog::closeEvent(QCloseEvent* event)
{
    saveOutput();
    // QDialog::closeEvent calls reject(). Calling close() again from reject()
    // recurses and prevents Fechar from closing. Accepting hides and keeps jobs.
    event->accept();
}

void ExportDialog::reject()
{
    saveOutput();
    hide();
}
