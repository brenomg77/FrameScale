#pragma once

#include <QString>
#include <QStringList>

#include <filesystem>

namespace framescale {

class RuntimePaths final {
public:
    RuntimePaths();
    explicit RuntimePaths(QString root);

    QString tool(const QString& name) const;
    QString model(const std::filesystem::path& relative) const;
    QString ncnnModelBase(const std::filesystem::path& relative) const;
    QString script(const QString& name) const;

private:
    QString firstExisting(const QStringList& candidates) const;

    QStringList roots_;
};

} // namespace framescale
