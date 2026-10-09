#pragma once
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabBar>
#include <QTableWidget>
#include <QTranslator>

// Qt does not translate standard message-box buttons with the application
// dictionary. Keep the small set used by FrameScale local to its language choice.
class PortugueseButtonsTranslator final : public QTranslator {
public:
    explicit PortugueseButtonsTranslator(QObject* parent)
        : QTranslator(parent)
    {
    }
    void setBrazilian(bool value) { brazilian_ = value; }
    bool isEmpty() const override { return false; }
    QString translate(const char* context, const char* source, const char* = nullptr, int = -1) const override
    {
        if (qstrcmp(context, "QPlatformTheme") && qstrcmp(context, "QDialogButtonBox") && qstrcmp(context, "QMessageBox"))
            return { };
        QString text = QString::fromUtf8(source);
        text.remove('&');
        if (text == "Save")
            return brazilian_ ? QStringLiteral("Salvar") : QStringLiteral("Guardar");
        if (text == "Discard")
            return brazilian_ ? QStringLiteral("Não salvar") : QStringLiteral("Não guardar");
        static const QMap<QString, QString> buttons { { "Yes", "Sim" }, { "No", "Não" }, { "Cancel", "Cancelar" }, { "Close", "Fechar" }, { "Save", "Guardar" }, { "Discard", "Não guardar" }, { "Apply", "Aplicar" }, { "Retry", "Tentar novamente" }, { "OK", "OK" } };
        return buttons.value(text);
    }
private:
    bool brazilian_ = false;
};

class InterfaceTranslator final : public QTranslator {
public:
    explicit InterfaceTranslator(QObject* parent = nullptr)
        : QTranslator(parent)
    {
        translations_ = dictionary("en");
    }
    bool isEmpty() const override { return translations_.isEmpty(); }
    QString translate(const char*, const char* source, const char* = nullptr, int = -1) const override
    {
        const QString key = QString::fromUtf8(source);
        QString result = translations_.value(key).toString();
        if (result.isEmpty() && !key.endsWith(QChar(0x2026))) {
            result = translations_.value(key + QChar(0x2026)).toString();
            if (result.endsWith(QChar(0x2026))) result.chop(1);
        }
        return result;
    }
    static QString normalizeLanguage(QString language)
    {
        language.replace('_', '-');
        language = language.toLower();
        if (language == "en" || language.startsWith("en-"))
            return "en";
        return language == "pt-br" ? "pt-BR" : "pt-PT";
    }
    static void applyLanguage(bool english) { applyLanguage(QString(english ? "en" : "pt-PT")); }
    static void applyLanguage(const char* language) { applyLanguage(QString::fromUtf8(language)); }
    static void applyLanguage(const QString& requestedLanguage)
    {
        static InterfaceTranslator* translator = new InterfaceTranslator(qApp);
        static auto* portugueseButtons = new PortugueseButtonsTranslator(qApp);
        static QJsonObject previousDictionary;
        const auto oldDictionary = previousDictionary;
        const QString language = normalizeLanguage(requestedLanguage);
        qApp->removeTranslator(portugueseButtons);
        qApp->removeTranslator(translator);
        translator->translations_ = dictionary(language);
        qApp->installTranslator(translator);
        if (language != "en") {
            portugueseButtons->setBrazilian(language == "pt-BR");
            qApp->installTranslator(portugueseButtons);
        }
        const QLocale locale(language == "en" ? "en_US" : language);
        QLocale::setDefault(locale);
        // Resolve through the language actually on screen. Searching every
        // locale ambiguously mapped translated labels back to the wrong source.
        QHash<QString, QString> sources;
        for (auto it = oldDictionary.begin(); it != oldDictionary.end(); ++it)
            sources.insert(it.value().toString(), it.key());
        auto text = [&sources, &oldDictionary](const QString& value) {
            if (value.isEmpty())
                return value;
            const QString source = sources.value(value, value);
            if (translator->translations_.contains(source))
                return translator->translations_.value(source).toString();
            // Preserve values already substituted with .arg(), including names
            // and durations, while translating the surrounding sentence.
            for (auto it = oldDictionary.begin(); it != oldDictionary.end(); ++it) {
                const QString oldTemplate = it.value().toString();
                if (!oldTemplate.contains('%'))
                    continue;
                const QString translated = translateFormatted(value, oldTemplate, translator->translations_.value(it.key()).toString(it.key()));
                if (!translated.isNull())
                    return translated;
            }
            return value;
        };
        for (auto* widget : QApplication::allWidgets()) {
            const QSignalBlocker blocker(widget);
            widget->setLocale(locale);
            widget->setWindowTitle(text(widget->windowTitle()));
            widget->setToolTip(text(widget->toolTip()));
            widget->setAccessibleName(text(widget->accessibleName()));
            if (auto* label = qobject_cast<QLabel*>(widget); label && !label->text().isEmpty())
                label->setText(text(label->text()));
            if (auto* button = qobject_cast<QAbstractButton*>(widget))
                button->setText(text(button->text()));
            if (auto* group = qobject_cast<QGroupBox*>(widget))
                group->setTitle(text(group->title()));
            if (auto* edit = qobject_cast<QLineEdit*>(widget))
                edit->setPlaceholderText(text(edit->placeholderText()));
            if (auto* spin = qobject_cast<QSpinBox*>(widget)) {
                spin->setPrefix(text(spin->prefix()));
                spin->setSuffix(text(spin->suffix()));
                spin->setSpecialValueText(text(spin->specialValueText()));
            }
            if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
                spin->setPrefix(text(spin->prefix()));
                spin->setSuffix(text(spin->suffix()));
                spin->setSpecialValueText(text(spin->specialValueText()));
            }
            if (auto* combo = qobject_cast<QComboBox*>(widget); combo && combo->objectName() != "languageChoice")
                for (int i = 0; i < combo->count(); ++i)
                    if (combo->objectName() != "encoderPresets" || i == 0) {
                        combo->setItemText(i, text(combo->itemText(i)));
                        combo->setItemData(i,text(combo->itemData(i,Qt::ToolTipRole).toString()),Qt::ToolTipRole);
                    }
            if (auto* tabs = qobject_cast<QTabBar*>(widget)) {
                if (tabs->objectName() != "projectTabs")
                    for (int i = 0; i < tabs->count(); ++i)
                        tabs->setTabText(i, text(tabs->tabText(i)));
            }
            if (auto* table = qobject_cast<QTableWidget*>(widget)) {
                for (int i = 0; i < table->columnCount(); ++i)
                    if (auto* item = table->horizontalHeaderItem(i))
                        item->setText(text(item->text()));
                if (table->objectName() == "exportQueue")
                    for (int row = 0; row < table->rowCount(); ++row)
                        if (auto* item = table->item(row, 2))
                            item->setText(text(item->text()));
            }
            if (auto* list = qobject_cast<QListWidget*>(widget))
                for (int i = 0; i < list->count(); ++i)
                    list->item(i)->setText(text(list->item(i)->text()));
            for (auto* action : widget->actions()) {
                action->setText(text(action->text()));
                action->setToolTip(text(action->toolTip()));
            }
            widget->update();
        }
        previousDictionary = translator->translations_;
    }

private:
    static QJsonObject dictionary(const QString& language)
    {
        QFile file(language == "en" ? ":/brand/english.json" : ":/brand/" + language + ".json");
        return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
    }
    static QString translateFormatted(const QString& value, const QString& oldTemplate, const QString& newTemplate)
    {
        static const QRegularExpression argument("%L?([1-9][0-9]?)");
        auto matches = argument.globalMatch(oldTemplate);
        QString pattern("\\A");
        QList<QString> arguments;
        qsizetype end = 0;
        while (matches.hasNext()) {
            const auto match = matches.next();
            pattern += QRegularExpression::escape(oldTemplate.mid(end, match.capturedStart() - end)) + "(.*?)";
            arguments.append(match.captured(1));
            end = match.capturedEnd();
        }
        if (arguments.isEmpty())
            return {};
        pattern += QRegularExpression::escape(oldTemplate.mid(end)) + "\\z";
        const auto match = QRegularExpression(pattern, QRegularExpression::DotMatchesEverythingOption).match(value);
        if (!match.hasMatch())
            return {};
        QHash<QString, QString> values;
        for (qsizetype i = 0; i < arguments.size(); ++i)
            values.insert(arguments[i], match.captured(i + 1));
        QString result;
        matches = argument.globalMatch(newTemplate);
        end = 0;
        while (matches.hasNext()) {
            const auto next = matches.next();
            result += newTemplate.mid(end, next.capturedStart() - end) + values.value(next.captured(1), next.captured());
            end = next.capturedEnd();
        }
        return result + newTemplate.mid(end);
    }
    QJsonObject translations_;
};
