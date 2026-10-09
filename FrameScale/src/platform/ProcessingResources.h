#pragma once

#include <QThread>
#include <QLibrary>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_4.h>
#endif

namespace framescale {

inline QString processingFilterThreads()
{
    // Bound filter concurrency instead of forcing every CPU to use two threads.
    return QString::number(std::clamp(QThread::idealThreadCount() / 2, 1, 8));
}

inline QString processingRifeThreads(int width, int height)
{
    // Parallelize lossless PNG loading/saving, keeping the two GPU workers and
    // the model unchanged. Limit queued image buffers for very large sources.
    return QThread::idealThreadCount() >= 8 && qint64(width) * height <= 3840LL * 2160
        ? QStringLiteral("2:2:4") : QStringLiteral("1:2:2");
}

inline QString processingTileSize()
{
#ifdef Q_OS_WIN
    // NCNN chooses the GPU. Only enlarge tiles when every hardware adapter has
    // ample dedicated memory and current local budget. Unknown/low-memory
    // systems keep the conservative tile size. Recheck for each job.
    QLibrary dxgi(QStringLiteral("dxgi"));
    using CreateFactory = HRESULT(WINAPI*)(REFIID, void**);
    const auto create = reinterpret_cast<CreateFactory>(dxgi.resolve("CreateDXGIFactory1"));
    IDXGIFactory1* factory = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
        return QStringLiteral("128");
    bool found = false;
    bool enough = true;
    constexpr quint64 gib = quint64(1024) * 1024 * 1024;
    for (UINT index = 0; ; ++index) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(index, &adapter) != S_OK)
            break;
        DXGI_ADAPTER_DESC1 description {};
        if (FAILED(adapter->GetDesc1(&description))) {
            enough = false;
        } else if (!(description.Flags & (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE))) {
            found = true;
            IDXGIAdapter3* budgetAdapter = nullptr;
            DXGI_QUERY_VIDEO_MEMORY_INFO memory {};
            const bool queried = SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3),
                reinterpret_cast<void**>(&budgetAdapter)))
                && SUCCEEDED(budgetAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory));
            enough = enough && description.DedicatedVideoMemory >= 4 * gib && queried
                && memory.Budget > memory.CurrentUsage && memory.Budget - memory.CurrentUsage >= 3 * gib;
            if (budgetAdapter) budgetAdapter->Release();
        }
        adapter->Release();
    }
    factory->Release();
    if (found && enough) return QStringLiteral("256");
#endif
    return QStringLiteral("128");
}

} // namespace framescale
