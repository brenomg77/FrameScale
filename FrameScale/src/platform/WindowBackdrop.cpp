#include "WindowBackdrop.h"

#include <QWindow>
#include <QWidget>
#include <QApplication>
#include <QPainterPath>
#include <QRegion>
#include <cmath>

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
        if (!widget->mask().isEmpty()) widget->clearMask();
        SetWindowRgn(hwnd, nullptr, TRUE);
        return;
    }
    // Keep Qt's own window mask in sync too. Qt reapplies its stored region
    // when updating layered windows; a GDI-only region can otherwise be lost,
    // exposing the rectangular native blur as black corner wedges on Windows 10.
    const qreal edgeInset = widget->property("glassEdgeInset").toReal();
    QPainterPath outline;
    outline.addRoundedRect(QRectF(widget->rect()).adjusted(edgeInset,edgeInset,-edgeInset,-edgeInset),14,14);
    const QRegion qtRegion(outline.toFillPolygon().toPolygon());
    if (widget->mask() != qtRegion) widget->setMask(qtRegion);
    // Accent blur is composed by Windows outside Qt's alpha mask. Bound the
    // native window too, otherwise its rectangular blur leaves bright square
    // tips behind the rounded content (especially at the bottom corners).
    // Use physical pixels so the native and Qt outlines agree at any DPI.
    const qreal scale = widget->devicePixelRatioF();
    const int inset = qRound(widget->property("glassEdgeInset").toReal() * scale);
    const int width = qRound(widget->width() * scale) - 2*inset;
    const int height = qRound(widget->height() * scale) - 2*inset;
    const int radius = qMin(qRound(14 * scale),qMin(width,height)/2);
    // Build symmetric pixel rows inside Qt's alpha outline. GDI's rounded
    // rectangle uses different right/bottom endpoint rules and can leave tips.
    HRGN region = CreateRectRgn(0,radius,width,height-radius);
    for(int y=0;y<radius && region;++y) {
        const double dy=radius-y-.5;
        const int inset=qCeil(radius-std::sqrt(radius*radius-dy*dy));
        HRGN row=CreateRectRgn(inset,y,width-inset,y+1);
        if(row) {CombineRgn(region,region,row,RGN_OR);DeleteObject(row);}
        row=CreateRectRgn(inset,height-y-1,width-inset,height-y);
        if(row) {CombineRgn(region,region,row,RGN_OR);DeleteObject(row);}
    }
    if(region && inset) OffsetRgn(region,inset,inset);
    // Avoid invalidating the native window when the outline is already right.
    HRGN current=CreateRectRgn(0,0,0,0);
    const bool unchanged=region && current && GetWindowRgn(hwnd,current)!=ERROR && EqualRgn(region,current);
    if(current) DeleteObject(current);
    if(unchanged) {DeleteObject(region);return;}
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
    if(widget->property("roundedWindowInstalled").toBool() && !widget->inherits("QMainWindow")) {
        const LONG_PTR style=GetWindowLongPtr(hwnd,GWL_STYLE);
        const LONG_PTR extended=GetWindowLongPtr(hwnd,GWL_EXSTYLE);
        const LONG_PTR cleanStyle=style & ~(WS_BORDER|WS_DLGFRAME|WS_THICKFRAME);
        const LONG_PTR cleanExtended=extended & ~(WS_EX_WINDOWEDGE|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE|WS_EX_DLGMODALFRAME);
        if(style!=cleanStyle) SetWindowLongPtr(hwnd,GWL_STYLE,cleanStyle);
        if(extended!=cleanExtended) SetWindowLongPtr(hwnd,GWL_EXSTYLE,cleanExtended);
        const MARGINS margins{0,0,0,0};DwmExtendFrameIntoClientArea(hwnd,&margins);
        if(style!=cleanStyle || extended!=cleanExtended) SetWindowPos(hwnd,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
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
