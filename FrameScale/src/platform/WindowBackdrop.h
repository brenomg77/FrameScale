#pragma once

class QWindow;
class QWidget;

namespace framescale::window_backdrop {

enum class Mode {
    Mica,
    Fallback
};

Mode applyLightBackdrop(QWindow* window);
// Configures the native frame; translucency is composed by Qt, not OS effects.
void configureGlassWindow(QWidget* widget, bool enabled, bool dark);
void syncGlassRegion(QWidget* widget);
const char* styleName(Mode mode);

} // namespace framescale::window_backdrop
