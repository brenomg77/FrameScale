#pragma once
#include <QApplication>
#include <QKeyEvent>
#include <QSettings>
#include <QShortcut>
#include <QWidget>

namespace KeyboardBindings {
struct Command { const char* id; const char* label; const char* keys; int group; };
inline const Command commands[] = {
    {"open", "Abrir ficheiro", "Ctrl+O", 0},
    {"paste", "Colar imagem ou ficheiro", "Ctrl+V", 0}, {"quit", "Sair do programa", "Ctrl+Q", 0},
    {"undo", "Desfazer", "Ctrl+Z", 1}, {"redo", "Refazer", "Ctrl+Shift+Z", 1},
    {"delete", "Excluir camadas selecionadas", "Del", 1},
    {"selection", "Ferramenta de seleção", "V", 1},
    {"textTool", "Ferramenta de texto", "T", 1},
    {"play", "Reproduzir / pausar", "Space", 2}, {"previous", "Fotograma anterior", "Left", 2},
    {"next", "Fotograma seguinte", "Right", 2}, {"zoomIn", "Aumentar zoom", "+", 2},
    {"zoomOut", "Diminuir zoom", "-", 2}, {"resetView", "Repor vista da timeline", "0", 2},
    {"fit", "Ajustar à janela", "Ctrl+0", 3},
    {"queueDelete", "Remover da fila", "Del", 4}
};
inline QKeySequence defaults(const QString& id) {
    for (const auto& command : commands) if (id == command.id)
        return QKeySequence(QString::fromLatin1(command.keys), QKeySequence::PortableText);
    return {};
}
inline QKeySequence sequence(const QString& id) {
    QSettings settings;
    return QKeySequence(settings.value("keyboardShortcuts/" + id,
        defaults(id).toString(QKeySequence::PortableText)).toString(), QKeySequence::PortableText);
}
inline bool matches(const QKeyEvent* event, const QString& id) {
    const auto keys=sequence(id);
    if(keys.isEmpty()) return false;
    if(id=="zoomIn" && keys==defaults(id) && event->key()==Qt::Key_Plus
        && (event->modifiers() & ~Qt::ShiftModifier)==Qt::NoModifier) return true;
    return QKeySequence(event->keyCombination()) == keys;
}
inline QShortcut* bind(const QString& id, QWidget* parent) {
    auto* shortcut = new QShortcut(sequence(id), parent);
    shortcut->setProperty("keyboardCommand", id);
    return shortcut;
}
inline void refresh() {
    for (auto* widget : QApplication::allWidgets())
        for (auto* shortcut : widget->findChildren<QShortcut*>(QString(), Qt::FindDirectChildrenOnly)) {
            const auto id = shortcut->property("keyboardCommand").toString();
            if (!id.isEmpty()) shortcut->setKey(sequence(id));
        }
}
}
