#pragma once
#include <QString>

// Keep one border-box for the normal, hover and pressed surfaces. Text padding
// belongs to the contents, never to a second native button painted on the right.
inline QString controlSurfaceGeometry() {
    return QStringLiteral(R"(
QPushButton, QToolButton, QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
    background-origin:border; background-clip:border;
}
/* Editors inside compound fields must not paint a smaller second face. */
QSpinBox QLineEdit, QDoubleSpinBox QLineEdit, QComboBox QLineEdit {
    background:transparent;border:0;border-radius:0;padding:0;margin:0;
}
QSpinBox QLineEdit:hover, QDoubleSpinBox QLineEdit:hover, QComboBox QLineEdit:hover {
    background:transparent;border:0;
}
/* The whole rounded field owns hover, including the arrow column. */
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background:transparent;
}
QToolButton {margin:0;padding:0;border:1px solid transparent;border-radius:6px;}
QComboBox {padding-right:26px;}
QComboBox::drop-down, QComboBox::drop-down:hover, QComboBox::drop-down:pressed {
    subcontrol-origin:border;subcontrol-position:center right;
    width:24px;margin:0;padding:0;border:0;background:transparent;
}
)");
}
