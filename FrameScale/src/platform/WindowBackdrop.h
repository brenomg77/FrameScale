#pragma once

class QWindow;
class QWidget;

namespace framescale::window_backdrop {

enum class Mode {
    Mica,
    Fallback
};

Mode applyLightBackdrop(QWindow* window);
// Refreshes native blur without changing the opacity of controls or media.
bool applyGlass(QWidget* widget, bool enabled, bool dark);
void syncGlassRegion(QWidget* widget);
const char* styleName(Mode mode);

} // namespace framescale::window_backdrop
