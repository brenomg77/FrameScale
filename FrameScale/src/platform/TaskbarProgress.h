#pragma once

class QWidget;

namespace framescale::taskbar {

// Calls belong to the GUI thread. Child dialogs share their main window's icon.
void setProgress(QWidget* window, int percent);
void setBusy(QWidget* window);
void clear(QWidget* window);

}
