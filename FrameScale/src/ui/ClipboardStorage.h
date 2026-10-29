#pragma once

#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <memory>

namespace framescale {

// All clipboard media belongs to the application's writable data directory,
// independent of its installation directory and of the Windows temp folder.
class ClipboardStorage final {
public:
    struct Entry {
        QString path;
        QString error;
        std::shared_ptr<QTemporaryDir> pendingDirectory;

        // Keep accepted media: open tabs and queued exports may still use it
        // after another copy/paste or after the application closes.
        void keep() const { if (pendingDirectory) pendingDirectory->setAutoRemove(false); }
    };

    static QString defaultDirectory() {
        const auto data = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        return data.isEmpty() ? QString() : QDir(data).filePath("Clipboard");
    }

    explicit ClipboardStorage(QString directory = defaultDirectory()) : directory_(std::move(directory)) {}

    Entry storeFile(const QString& source) const {
        const QFileInfo original(source);
        QFile file(source);
        if (!original.isFile() || !file.open(QIODevice::ReadOnly))
            return { {}, file.errorString(), {} };
        file.close();
        if (contains(source)) return { original.canonicalFilePath(), {}, {} };
        Entry result = newEntry(original.fileName());
        if (!result.error.isEmpty()) return result;
        if (!file.copy(result.path)) { result.error = file.errorString(); result.path.clear(); }
        return result;
    }

    Entry storeImage(const QImage& image) const {
        Entry result = newEntry("image.png");
        if (!result.error.isEmpty()) return result;
        if (image.isNull() || !image.save(result.path, "PNG")) {
            result.error = QCoreApplication::translate("ClipboardStorage", "Não foi possível guardar a imagem copiada.");
            result.path.clear();
        }
        return result;
    }

    std::unique_ptr<QTemporaryDir> videoCopyDirectory() const {
        if (directory_.isEmpty() || !QDir().mkpath(directory_)) return {};
        return std::make_unique<QTemporaryDir>(QDir(directory_).filePath("Copy-XXXXXX"));
    }

    bool contains(const QString& path) const {
        const auto root = QFileInfo(directory_).canonicalFilePath();
        const auto file = QFileInfo(path).canonicalFilePath();
#ifdef Q_OS_WIN
        constexpr auto sensitivity = Qt::CaseInsensitive;
#else
        constexpr auto sensitivity = Qt::CaseSensitive;
#endif
        return !root.isEmpty() && !file.isEmpty() && file.startsWith(root + '/', sensitivity);
    }

private:
    Entry newEntry(const QString& name) const {
        if (directory_.isEmpty() || !QDir().mkpath(directory_))
            return { {}, QCoreApplication::translate("ClipboardStorage", "Não foi possível criar a pasta Clipboard."), {} };
        auto pending = std::make_shared<QTemporaryDir>(QDir(directory_).filePath("Paste-XXXXXX"));
        if (!pending->isValid()) return { {}, pending->errorString(), {} };
        return { pending->filePath(name), {}, std::move(pending) };
    }

    QString directory_;
};

} // namespace framescale
