#include "platform/TaskbarProgress.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QEvent>
#include <QTimer>
#include <QWidget>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#endif

namespace framescale::taskbar {
namespace {
#ifdef Q_OS_WIN
class Indicator final : public QObject, public QAbstractNativeEventFilter {
public:
    explicit Indicator(QWidget* window)
        : QObject(window), window_(window)
    {
        setObjectName("framescaleTaskbarProgress");
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ownsCom_ = SUCCEEDED(initialized);
        if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE) {
            if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                    IID_ITaskbarList3, reinterpret_cast<void**>(&taskbar_)))) {
                if (FAILED(taskbar_->HrInit())) {
                    taskbar_->Release();
                    taskbar_ = nullptr;
                }
            }
        }
        createdMessage_ = RegisterWindowMessageW(L"TaskbarButtonCreated");
        qApp->installNativeEventFilter(this);
        window_->installEventFilter(this);
    }

    ~Indicator() override
    {
        if (qApp) qApp->removeNativeEventFilter(this);
        if (taskbar_) taskbar_->Release();
        if (ownsCom_) CoUninitialize();
    }

    void update(int percent)
    {
        percent_ = percent;
        apply();
    }

protected:
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override
    {
        const auto* event = static_cast<MSG*>(message);
        if (createdMessage_ && event->message == createdMessage_
            && event->hwnd == reinterpret_cast<HWND>(window_->internalWinId())) {
            // Explorer can recreate the button after a restart or a style change.
            QTimer::singleShot(0, this, [this] { apply(); });
        }
        return false;
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == window_ && (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange))
            QTimer::singleShot(0, this, [this] { apply(); });
        return false;
    }

private:
    void apply()
    {
        if (!taskbar_ || !window_->internalWinId()) return;
        const auto handle = reinterpret_cast<HWND>(window_->internalWinId());
        if (percent_ == -2) {
            taskbar_->SetProgressState(handle, TBPF_NOPROGRESS);
        } else if (percent_ == -1) {
            taskbar_->SetProgressState(handle, TBPF_INDETERMINATE);
        } else {
            taskbar_->SetProgressState(handle, TBPF_NORMAL);
            taskbar_->SetProgressValue(handle, static_cast<ULONGLONG>(percent_), 100);
        }
    }

    QWidget* window_;
    ITaskbarList3* taskbar_ = nullptr;
    bool ownsCom_ = false;
    UINT createdMessage_ = 0;
    int percent_ = -2;
};

Indicator* indicator(QWidget* window, bool create = true)
{
    if (!window) return nullptr;
    while (window->parentWidget()) window = window->parentWidget();
    for (auto* child : window->children())
        if (auto* existing = dynamic_cast<Indicator*>(child)) return existing;
    return create ? new Indicator(window) : nullptr;
}
#endif
}

void setProgress(QWidget* window, int percent)
{
#ifdef Q_OS_WIN
    if (auto* item = indicator(window)) item->update(std::clamp(percent, 0, 100));
#else
    Q_UNUSED(window);
    Q_UNUSED(percent);
#endif
}

void setBusy(QWidget* window)
{
#ifdef Q_OS_WIN
    if (auto* item = indicator(window)) item->update(-1);
#else
    Q_UNUSED(window);
#endif
}

void clear(QWidget* window)
{
#ifdef Q_OS_WIN
    if (auto* item = indicator(window, false)) item->update(-2);
#else
    Q_UNUSED(window);
#endif
}
}
