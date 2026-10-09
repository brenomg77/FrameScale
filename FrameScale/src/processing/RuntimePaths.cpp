#include "processing/RuntimePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <utility>

namespace framescale {
namespace {

QString executableName(QString name)
{
#ifdef _WIN32
    if (!name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        name += QStringLiteral(".exe");
    }
#endif
    return name;
}

QString fromPath(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromUtf8(path.string().c_str());
#endif
}

QStringList defaultRoots()
{
    const QDir appDirectory(QCoreApplication::applicationDirPath());
    const QDir current = QDir::current();
    return {
        appDirectory.absolutePath(),
        QDir(appDirectory.filePath(QStringLiteral(".."))).absolutePath(),
        current.absolutePath(),
        QDir(current.filePath(QStringLiteral(".."))).absolutePath()
    };
}

} // namespace

RuntimePaths::RuntimePaths()
    : roots_(defaultRoots())
{
}

RuntimePaths::RuntimePaths(QString root)
    : roots_({ QDir(std::move(root)).absolutePath() })
{
}

QString RuntimePaths::firstExisting(const QStringList& candidates) const
{
    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists()) {
            return info.absoluteFilePath();
        }
    }
    return candidates.isEmpty() ? QString() : QFileInfo(candidates.front()).absoluteFilePath();
}

QString RuntimePaths::tool(const QString& name) const
{
    QStringList candidates;
    const QString executable = executableName(name);
    for (const QString& root : roots_) {
        candidates.append(QDir(root).filePath(QStringLiteral("runtime/bin/") + executable));
    }
    return firstExisting(candidates);
}

QString RuntimePaths::model(const std::filesystem::path& relative) const
{
    QStringList candidates;
    const QString path = fromPath(relative);
    for (const QString& root : roots_) {
        candidates.append(QDir(root).filePath(QStringLiteral("models/") + path));
    }
    return firstExisting(candidates);
}

QString RuntimePaths::ncnnModelBase(const std::filesystem::path& relative) const
{
    std::filesystem::path parameterFile = relative;
    parameterFile += ".param";
    QString base = model(parameterFile);
    base.chop(6);
    return base;
}

QString RuntimePaths::script(const QString& name) const
{
    QStringList candidates;
    for (const QString& root : roots_) {
        candidates.append(QDir(root).filePath(QStringLiteral("runtime/scripts/") + name));
    }
    return firstExisting(candidates);
}

} // namespace framescale
