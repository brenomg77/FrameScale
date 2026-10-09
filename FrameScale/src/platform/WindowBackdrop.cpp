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
    const bool rounded = widget->property("nativeGlassActive").toBool()
        && !widget->isMaximized() && !widget->isFullScreen();
    const qreal scale = widget->devicePixelRatioF();
    const QSize pixels(qRound(widget->width()*scale), qRound(widget->height()*scale));
    if (widget->property("glassRegionSize").toSize() == pixels
        && widget->property("glassRegionRounded").isValid()
        && widget->property("glassRegionRounded").toBool() == rounded) return;
    // Accent blur otherwise paints an opaque rectangle behind Qt's alpha corners.
    // Windows owns the region after a successful SetWindowRgn call.
    HRGN region = rounded ? CreateRoundRectRgn(0, 0, pixels.width()+1, pixels.height()+1,
        qRound(24*scale), qRound(24*scale)) : nullptr;
    if (SetWindowRgn(hwnd, region, TRUE)) {
        widget->setProperty("glassRegionSize", pixels);
        widget->setProperty("glassRegionRounded", rounded);
    } else if (region) DeleteObject(region);
#else
    Q_UNUSED(widget);
#endif
}

bool applyGlass(QWidget* widget, bool enabled, bool dark)
{
#ifdef _WIN32
    if (!widget || QApplication::platformName() != "windows") return false;
    // Win10 compatibility path. Resolve at runtime because this entry point
    // and accent policy are not an SDK contract; fall back if unavailable.
    struct AccentPolicy { int state; DWORD flags; DWORD color; int animation; };
    struct CompositionData { int attribute; void* data; SIZE_T size; };
    using SetComposition = BOOL (WINAPI*)(HWND, CompositionData*);
    static auto setComposition = reinterpret_cast<SetComposition>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!setComposition) return false;
    BOOL composited = FALSE;
    if (FAILED(DwmIsCompositionEnabled(&composited)) || !composited) return false;
    const auto hwnd = reinterpret_cast<HWND>(widget->winId());
    // Layered dialogs use our cached backdrop and alpha edge. Applying the
    // accent compositor behind them creates a second, jagged native outline.
    enabled = enabled && widget->inherits("QMainWindow");
    AccentPolicy policy { enabled ? 3 : 0, 0, 0, 0 };
    CompositionData data {19, &policy, sizeof(policy)};
    const bool applied = setComposition(hwnd, &data);
    const BOOL darkCaption = dark;
    DwmSetWindowAttribute(hwnd, immersiveDarkModeAttribute, &darkCaption, sizeof(darkCaption));
    // Qt draws the single smooth outline; do not add a second DWM border.
    const DWORD noBorder = 0xfffffffe;
    DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
    widget->setProperty("nativeGlassActive", enabled && applied);
    syncGlassRegion(widget);
    return enabled && applied;
#else
    Q_UNUSED(widget); Q_UNUSED(enabled); Q_UNUSED(dark);
    return false;
#endif
}

const char* styleName(Mode mode)
{
    return mode == Mode::Mica ? "mica" : "fallback";
}

} // namespace framescale::window_backdrop
