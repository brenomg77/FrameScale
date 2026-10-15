#include "WindowBackdrop.h"

#include <QWindow>
#include <QWidget>
#include <QApplication>

#ifdef _WIN32
#include <QDateTime>
#include <QDebug>
#include <dwmapi.h>
#include <windows.h>
#endif

namespace framescale::window_backdrop {

#ifdef _WIN32
namespace {

constexpr DWORD immersiveDarkModeAttribute = 20;
constexpr DWORD windowCornerPreferenceAttribute = 33;
constexpr DWORD systemBackdropTypeAttribute = 38;
constexpr int roundCornerPreference = 2;
constexpr int mainWindowBackdrop = 2;

} // namespace
#endif

Mode applyLightBackdrop(QWindow* window)
{
#ifdef _WIN32
    if (window == nullptr) {
        return Mode::Fallback;
    }

    BOOL compositionEnabled = FALSE;
    if (FAILED(DwmIsCompositionEnabled(&compositionEnabled)) || !compositionEnabled) {
        return Mode::Fallback;
    }

    qDebug() << "[FrameScale] applying light backdrop at" << QDateTime::currentDateTime().toString();
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (hwnd == nullptr) {
        return Mode::Fallback;
    }

    const BOOL useDarkCaption = FALSE;
    DwmSetWindowAttribute(
        hwnd,
        immersiveDarkModeAttribute,
        &useDarkCaption,
        sizeof(useDarkCaption));

    const int cornerPreference = roundCornerPreference;
    DwmSetWindowAttribute(
        hwnd,
        windowCornerPreferenceAttribute,
        &cornerPreference,
        sizeof(cornerPreference));

    const int backdropType = mainWindowBackdrop;
    if (SUCCEEDED(DwmSetWindowAttribute(
            hwnd,
            systemBackdropTypeAttribute,
            &backdropType,
            sizeof(backdropType)))) {
        const MARGINS margins { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        return Mode::Mica;
    }
#else
    Q_UNUSED(window);
#endif

    return Mode::Fallback;
}

void syncGlassRegion(QWidget* widget)
{
#ifdef _WIN32
    if (!widget || !widget->isWindow() || !widget->testAttribute(Qt::WA_WState_Created)
        || QApplication::platformName() != "windows") return;
    const auto hwnd = reinterpret_cast<HWND>(widget->winId());
    const bool rounded = widget->property("roundedWindowInstalled").toBool()
        && !widget->isMaximized() && !widget->isFullScreen();
    if (!rounded) {
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    // Accent blur is composed by Windows outside Qt's alpha mask. Bound the
    // native window too, otherwise its rectangular blur leaves bright square
    // tips behind the rounded content (especially at the bottom corners).
    // Use physical pixels so the native and Qt outlines agree at any DPI.
    const qreal scale = widget->devicePixelRatioF();
    const int width = qRound(widget->width() * scale);
    const int height = qRound(widget->height() * scale);
    const int diameter = qRound(28 * scale);
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, diameter, diameter);
    // Windows owns the region only after a successful SetWindowRgn.
    if (region && !SetWindowRgn(hwnd, region, TRUE)) DeleteObject(region);
#else
    Q_UNUSED(widget);
#endif
}

void configureGlassWindow(QWidget* widget, bool enabled, bool dark)
{
    if (!widget) return;
    widget->setProperty("glassWindowSurface",enabled);
    widget->setProperty("nativeGlassActive",false);
#ifdef _WIN32
    if (QApplication::platformName() != "windows") return;
    // Request blur behind the live window, without an acrylic colour layer.
    // Qt's per-pixel alpha remains transparent even when native blur is absent.
    struct AccentPolicy { int state; DWORD flags; DWORD color; int animation; };
    struct CompositionData { int attribute; void* data; SIZE_T size; };
    using SetComposition = BOOL (WINAPI*)(HWND, CompositionData*);
    static auto setComposition = reinterpret_cast<SetComposition>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    const auto hwnd = reinterpret_cast<HWND>(widget->winId());
    const bool blur = enabled && !widget->property("stableSurface").toBool();
    AccentPolicy policy { blur ? 3 : 0, 0, 0, 0 };
    CompositionData data {19, &policy, sizeof(policy)};
    if (setComposition)
        widget->setProperty("nativeGlassActive",setComposition(hwnd, &data) && blur);
    const BOOL darkCaption = dark;
    DwmSetWindowAttribute(hwnd, immersiveDarkModeAttribute, &darkCaption, sizeof(darkCaption));
    // Qt draws the single smooth outline; do not add a second DWM border.
    const DWORD noBorder = 0xfffffffe;
    DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
    syncGlassRegion(widget);
#else
    Q_UNUSED(dark);
#endif
}

const char* styleName(Mode mode)
{
    return mode == Mode::Mica ? "mica" : "fallback";
}

} // namespace framescale::window_backdrop
