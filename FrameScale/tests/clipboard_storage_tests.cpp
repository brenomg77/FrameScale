#include "ui/ClipboardStorage.h"
#include <QGuiApplication>
#include <iostream>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir fixture;
    if (!fixture.isValid()) return 1;
    framescale::ClipboardStorage storage(fixture.filePath("application/Clipboard"));
    const QString original = fixture.filePath(QString::fromUtf8("Vídeo original.mp4"));
    const QByteArray bytes = QByteArrayLiteral("Synthetic clipboard payload\0with binary data");
    QFile source(original);
    if (!source.open(QIODevice::WriteOnly) || source.write(bytes) != bytes.size()) return 2;
    source.close();
    QString firstPath, secondPath;
    {
        auto first = storage.storeFile(original);
        auto second = storage.storeFile(original);
        if (!first.error.isEmpty() || !second.error.isEmpty() || first.path == second.path) return 3;
        firstPath = first.path; secondPath = second.path;
        if (!storage.contains(first.path) || QFileInfo(first.path).fileName() != QFileInfo(original).fileName()) return 4;
        first.keep(); second.keep();
    }
    if (!QFileInfo::exists(firstPath) || !QFileInfo::exists(secondPath)) return 5;
    QFile saved(firstPath);
    if (!saved.open(QIODevice::ReadOnly) || saved.readAll() != bytes) return 6;
    saved.close();
    // Pasting an application-owned copy reuses it without a second large copy.
    const auto reused = storage.storeFile(firstPath);
    if (reused.path != firstPath || reused.pendingDirectory) return 7;
    QString rejected;
    {
        const auto cancelled = storage.storeFile(original);
        rejected = cancelled.path;
    }
    if (QFileInfo::exists(rejected) || !QFileInfo::exists(original)) return 8;
    if (!storage.storeFile(fixture.filePath("missing.mp4")).path.isEmpty()) return 9;
    QImage image(16, 12, QImage::Format_ARGB32); image.fill(QColor(40, 80, 120, 127));
    QString pastedImage;
    {
        auto entry = storage.storeImage(image);
        if (!entry.error.isEmpty() || QImage(entry.path) != image) return 10;
        pastedImage = entry.path; entry.keep();
    }
    if (QImage(pastedImage) != image) return 11;
    QString successfulCopy, cancelledCopy;
    {
        auto directory = storage.videoCopyDirectory();
        if (!directory || !directory->isValid()) return 12;
        successfulCopy = directory->filePath("selection.mp4");
        if (!QFile::copy(original, successfulCopy)) return 13;
        directory->setAutoRemove(false);
    }
    {
        auto directory = storage.videoCopyDirectory();
        if (!directory || !directory->isValid()) return 14;
        cancelledCopy = directory->filePath("selection.mp4");
        if (!QFile::copy(original, cancelledCopy)) return 15;
    }
    if (QFileInfo::exists(cancelledCopy) || !QFileInfo::exists(successfulCopy)
        || !QFileInfo::exists(firstPath)) return 16;
    framescale::ClipboardStorage unwritable(original); // A file cannot be the cache directory.
    if (unwritable.storeFile(original).error.isEmpty() || unwritable.videoCopyDirectory()) return 17;
    source.open(QIODevice::ReadOnly);
    if (source.readAll() != bytes) return 18;
    std::cout << "Clipboard: persistent copies, original preservation, unique names, image pixels, reuse and rollback OK\n";
}
