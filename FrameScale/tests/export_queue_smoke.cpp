#include "ui/ExportDialog.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QThread>
#include <QToolButton>

#include <iostream>

namespace {
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void events()
{
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
}

framescale::ProcessingOptions options(const char* input, const char* output, int scale)
{
    framescale::ProcessingOptions value;
    value.inputPath = input;
    value.outputPath = output;
    value.scaleFactor = scale;
    return value;
}
}

int main(int argc, char** argv)
{
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    ExportDialog queue({ }, { });
    queue.addItem(options("a.png", "a-out.png", 10), "10x");
    queue.addItem(options("b.png", "b-out.png", 2), "2x");
    queue.addItem(options("c.png", "c-out.png", 3), "3x");
    auto* table = queue.findChild<QTableWidget*>("exportQueue");
    auto* start = queue.findChild<QPushButton*>("exportStartButton");
    auto* all = queue.findChild<QPushButton*>("exportStartButton");
    auto* close = queue.findChild<QPushButton*>("exportCloseButton");
    auto* cancel = queue.findChild<QPushButton*>("exportCancelButton");
    table->item(1, 0)->setCheckState(Qt::Unchecked);
    int requests = 0;
    int cancels = 0;
    QObject::connect(&queue, &ExportDialog::exportRequested, [&](const QString&) { ++requests; });
    QObject::connect(&queue, &ExportDialog::cancelRequested, [&] { ++cancels; });
    queue.show();
    events();
    close->click();
    check(!queue.isVisible(), "close did not hide idle queue");
    queue.show();
    start->click();
    check(requests == 1 && queue.activeOptions().scaleFactor == 10, "wrong initial snapshot");
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&queue, &escape);
    check(!queue.isVisible() && cancels == 0, "Escape did not hide running queue safely");
    queue.show();
    queue.setProgress(40, "Inference");
    queue.setFrameProgress(2, 8);
    check(queue.findChild<QProgressBar*>("exportProgressBar")->value() == 250,
        "frame bar does not match completed frames");
    queue.setProgress(39, "Inference");
    check(queue.findChild<QProgressBar*>("exportProgressBar")->value() == 250,
        "stage percentage replaced measured frames");
    check(queue.findChild<QLabel*>("exportFrameCount")->text() == "2 / 8 frames",
        "frame count changed worker data");
    queue.setFrameProgress(1, 8);
    check(queue.findChild<QLabel*>("exportFrameCount")->text() == "2 / 8 frames",
        "delayed frame update moved count backwards");
    queue.setFrameProgress(10, 0);
    queue.setFrameProgress(8, 8);
    check(queue.findChild<QLabel*>("exportFrameCount")->text() == "8 / 8 frames",
        "known frame total did not clamp earlier unknown count");
    queue.setProgress(42, "Encoding");
    queue.setFrameProgress(0, 12);
    queue.setFrameProgress(1, 12);
    check(queue.findChild<QLabel*>("exportFrameCount")->text() == "1 / 12 frames",
        "new stage retained previous frame count");
    check(queue.findChild<QLabel*>("exportSpeed")->text().startsWith("Frames/s: "),
        "incorrect speed label");
    QThread::msleep(1100);
    events();
    const QString runningElapsed = queue.findChild<QLabel*>("exportElapsed")->text();
    check(runningElapsed != "00:00:00", "elapsed timer did not advance");
    const QString measuredRate = queue.findChild<QLabel*>("exportSpeed")->text();
    check(!measuredRate.contains(QChar(0x2014)) && !measuredRate.endsWith("0.0"),
        "speed did not use elapsed time and completed frames");
    check(!queue.findChild<QLabel*>("exportPercent"), "removed total percent remains");
    check(!queue.findChild<QPushButton*>("exportAddButton"), "removed queue add remains");
    check(!queue.findChild<QPushButton*>("exportRemoveButton"), "removed queue remove remains");
    check(!table->cellWidget(0, 3)->isEnabled(), "active row can be removed");
    qobject_cast<QToolButton*>(table->cellWidget(2, 3))->click();
    check(queue.queueCount() == 2, "pending item removal failed");
    queue.addItem(options("d.png", "d-out.png", 4), "4x");
    check(queue.activeOptions().scaleFactor == 10, "selection changed active snapshot");
    close->click();
    check(!queue.isVisible() && cancels == 0, "close cancelled active job");
    queue.show();
    queue.setFinished("a-out.png");
    check(!queue.findChild<QProgressBar*>("exportProgressBar")->isVisible(),"completed progress remains visible");
    const QString stoppedTime = queue.findChild<QLabel*>("exportElapsed")->text();
    QThread::msleep(1100);
    events();
    check(queue.findChild<QLabel*>("exportElapsed")->text() == stoppedTime,
        "finished elapsed timer kept advancing");
    queue.setFrameProgress(1, 100);
    check(queue.findChild<QLabel*>("exportFrameCount")->text() == "1 / 12 frames",
        "late worker signal changed completed frame count");
    events();
    check(requests == 1 && !start->isEnabled(), "dispatch raced unreleased job");
    queue.jobReleased();
    events();
    check(requests == 1, "new item or removed pending item auto exported");
    table->item(1,0)->setCheckState(Qt::Checked);
    all->click();
    check(requests == 2 && queue.activeOptions().scaleFactor == 2, "queue index shifted after deletion");
    queue.activateWindow();
    table->setCurrentCell(2, 1);
    table->setFocus();
    events();
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QApplication::sendEvent(table, &key);
    events();
    check(queue.queueCount() == 2, "Delete failed to remove pending row");
    cancel->click();
    cancel->click();
    check(cancels == 1, "cancel signal repeated");
    queue.setCancelled();
    queue.jobReleased();
    events();
    check(requests == 2 && all->isEnabled(), "cancellation did not stop queue or unlock retry");
    all->click();
    check(requests == 3, "cancelled item cannot retry");
    queue.setFailed("test failure");
    queue.jobReleased();
    check(all->isEnabled(), "failed item cannot retry");
    all->click();
    check(requests == 4, "failed retry did not dispatch");
    queue.setFinished("b-out.png");
    queue.jobReleased();
    check(!all->isEnabled(), "finished items are exported again by Export all");
    table->setCurrentCell(0, 1);
    table->item(0, 0)->setCheckState(Qt::Checked);
    start->click();
    check(requests == 5 && queue.activeOptions().scaleFactor == 10, "explicit completed rerender failed");
    queue.setFinished("a-out.png");
    queue.jobReleased();
    queue.close();

    // Synchronous validation failure and completion still dispatch sequentially,
    // with an explicit release and no recursive exportRequested emissions.
    ExportDialog serial({ }, { });
    serial.addItem(options("one.png", "one-out.png", 1), "one");
    serial.addItem(options("two.png", "two-out.png", 2), "two");
    serial.addItem(options("three.png", "three-out.png", 3), "three");
    int serialRequests = 0;
    int depth = 0;
    QObject::connect(&serial, &ExportDialog::exportRequested, [&](const QString& path) {
        ++depth;
        ++serialRequests;
        check(depth == 1, "queue dispatched recursively");
        check(serial.activeOptions().scaleFactor == serialRequests, "snapshot order corrupted");
        if (serialRequests == 1)
            serial.setFailed("validation");
        else
            serial.setFinished(path);
        serial.jobReleased();
        --depth;
    });
    serial.findChild<QPushButton*>("exportStartButton")->click();
    events();
    check(serialRequests == 3, "failure prevented later queued export");

    // Cancelling between jobs must clear pending work even after the next
    // dispatch has been scheduled, with no cancellation sent to a finished job.
    ExportDialog between({ }, { });
    between.addItem(options("first.png", "first-out.png", 1), "first");
    between.addItem(options("second.png", "second-out.png", 2), "second");
    auto* betweenTable = between.findChild<QTableWidget*>("exportQueue");
    auto* betweenOutput = between.findChild<QLineEdit*>("exportOutputPath");
    auto* betweenAll = between.findChild<QPushButton*>("exportStartButton");
    int betweenRequests = 0;
    int betweenCancels = 0;
    QObject::connect(&between, &ExportDialog::exportRequested,
        [&](const QString&) { ++betweenRequests; });
    QObject::connect(&between, &ExportDialog::cancelRequested,
        [&] { ++betweenCancels; });
    betweenTable->setCurrentCell(0, 1);
    betweenOutput->setText("first-custom.png");
    betweenTable->setCurrentCell(1, 1);
    betweenOutput->setText("second-custom.png");
    betweenAll->click();
    check(between.activeOptions().outputPath.filename() == "first-custom.png",
        "changing row lost the first output destination");
    betweenTable->setCurrentCell(1, 1);
    check(!betweenOutput->isEnabled(), "pending output can change after dispatch");
    between.setFinished("first-custom.png");
    between.jobReleased();
    between.findChild<QPushButton*>("exportCancelButton")->click();
    events();
    check(betweenRequests == 1 && betweenCancels == 0 && betweenAll->isEnabled(),
        "cancellation between jobs dispatched another job or did not unlock");
    betweenAll->click();
    check(betweenRequests == 2
            && between.activeOptions().outputPath.filename() == "second-custom.png",
        "retry after inter-job cancellation lost pending output");
    between.setFinished("second-custom.png");
    between.jobReleased();
    check(!betweenAll->isEnabled(), "inter-job cancellation corrupted completed states");

    // Clipboard copying owns the same worker, but pending destinations remain
    // editable. Clicking Start must never create a phantom running export.
    ExportDialog clipboard({}, {});
    clipboard.setProcessingAvailable(false);
    clipboard.addItem(options("copy-source.mp4", "export.mp4", 1), "copy");
    auto* clipboardStart = clipboard.findChild<QPushButton*>("exportStartButton");
    auto* clipboardOutput = clipboard.findChild<QLineEdit*>("exportOutputPath");
    auto* clipboardTable = clipboard.findChild<QTableWidget*>("exportQueue");
    int clipboardRequests = 0;
    QObject::connect(&clipboard, &ExportDialog::exportRequested,
        [&](const QString&) { ++clipboardRequests; });
    check(!clipboardStart->isEnabled() && clipboardOutput->isEnabled(),
        "clipboard preparation did not block dispatch while preserving editing");
    clipboardOutput->setText("edited-during-copy.mp4");
    clipboardStart->click();
    check(clipboardRequests == 0 && clipboardTable->item(0, 2)->text() == "Na fila",
        "clipboard preparation created a phantom running export");
    clipboard.setProcessingAvailable(true);
    clipboardStart->click();
    check(clipboardRequests == 1
            && clipboard.activeOptions().outputPath.filename() == "edited-during-copy.mp4",
        "clipboard release did not restore export with the edited destination");
    clipboard.setFinished("edited-during-copy.mp4");
    clipboard.jobReleased();

    // Copy can also take the worker between completed jobs. A scheduled next
    // item waits without losing its snapshot and resumes exactly once.
    ExportDialog shared({}, {});
    shared.addItem(options("first.mp4", "first-out.mp4", 1), "first");
    shared.addItem(options("second.mp4", "second-out.mp4", 2), "second");
    int sharedRequests = 0;
    QObject::connect(&shared, &ExportDialog::exportRequested,
        [&](const QString&) { ++sharedRequests; });
    shared.findChild<QPushButton*>("exportStartButton")->click();
    shared.setFinished("first-out.mp4");
    shared.jobReleased();
    shared.setProcessingAvailable(false);
    events();
    check(sharedRequests == 1, "queued export raced a clipboard worker between jobs");
    shared.setProcessingAvailable(true);
    shared.setProcessingAvailable(true);
    events();
    check(sharedRequests == 2 && shared.activeOptions().scaleFactor == 2,
        "clipboard release lost or duplicated a scheduled export");
    shared.setFinished("second-out.mp4");
    shared.jobReleased();
    if (!failures)
        std::cout << "Export queue smoke: close, snapshots, delete, cancel, retry, release, progress OK\n";
    return failures ? 1 : 0;
}
