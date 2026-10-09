#pragma once

#include <QString>

// Limit Qt's rich-text layout so help stays readable beside narrow panels.
inline QString helpToolTip(const QString& title, const QString& description)
{
    if (description.isEmpty()) return {};
    QString text = description.toHtmlEscaped();
    text.replace('\n', "<br>");
    return QString("<qt><p><b>%1</b></p><p>%2</p></qt>")
        .arg(title.toHtmlEscaped(), text);
}
