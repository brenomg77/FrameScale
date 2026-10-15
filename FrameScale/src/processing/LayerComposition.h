#pragma once
#include "core/ProcessingOptions.h"
#include <QStringList>
#include <QFileInfo>
#include <cmath>
#include <algorithm>

namespace framescale {
inline QString layerPath(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}
inline double compositionDuration(const std::vector<MediaLayer>& layers) {
    double duration = 0;
    for (const auto& layer : layers) duration = std::max(duration, layer.start + layer.duration());
    return duration;
}
struct LayerComposition {
    QStringList arguments;
    QString error;
    double fps = 25, duration = 0;
    int width = 1280, height = 720;
};
// A single shared graph is used by both the editor preview and full quality export.
// The topmost video row is composited last; all audio rows are mixed independently.
inline LayerComposition composeLayers(const std::vector<MediaLayer>& sourceLayers,
    const QString& output, bool preview = false, double rangeStart=0, double rangeEnd=0) {
    auto layers=sourceLayers;
    LayerComposition plan;
    if (layers.empty()) { plan.error = QObject::tr("Adicione uma camada ao projeto."); return plan; }
    for (const auto& layer : layers) {
        if (!QFileInfo::exists(layerPath(layer.path)) || !std::isfinite(layer.speed)
            || layer.speed < .25 || layer.speed > 4 || !std::isfinite(layer.start)
            || layer.start < 0 || !std::isfinite(layer.in) || !std::isfinite(layer.out)
            || layer.in < 0 || layer.out <= layer.in || layer.out > layer.sourceDuration + .001) {
            plan.error = QObject::tr("Uma camada contém um ficheiro ou intervalo inválido."); return plan;
        }
    }
    for (const auto& layer : layers) if (layer.kind == LayerKind::Video) {
        plan.width = std::max(2, layer.width); plan.height = std::max(2, layer.height);
        plan.fps = std::clamp(layer.sourceFps, 1., 240.); break;
    }
    const bool ranged=rangeStart>0 || rangeEnd>0;
    double duration=compositionDuration(layers);
    if(ranged) {
        const double end=rangeEnd>0?std::min(rangeEnd,duration):duration;
        if(!std::isfinite(rangeStart)||!std::isfinite(end)||rangeStart<0||end<=rangeStart) {
            plan.error=QObject::tr("O trecho selecionado é curto demais para a preview.");return plan;
        }
        duration=end-rangeStart;
        for(auto& layer:layers) {
            const double left=std::max(rangeStart,layer.start);
            const double right=std::min(end,layer.start+layer.duration());
            const double originalIn=layer.in;
            layer.in=originalIn+std::max(0.,left-layer.start)*layer.speed;
            layer.out=originalIn+std::max(0.,right-layer.start)*layer.speed;
            layer.start=std::max(0.,left-rangeStart);
        }
        layers.erase(std::remove_if(layers.begin(),layers.end(),[](const MediaLayer& layer){return layer.out<=layer.in;}),layers.end());
    }
    // Keep the project resolution for inspection and zoom. Reducing the long
    // edge to 720 pixels discarded upscale detail even while paused.
    plan.width += plan.width % 2; plan.height += plan.height % 2;
    const qint64 frames = std::max<qint64>(1, std::ceil(duration * plan.fps - 1e-7));
    plan.duration = frames / plan.fps;
    auto number = [](double value) { return QString::number(value, 'f', 12); };
    QStringList args {"-v", "error", "-nostdin", "-y", "-filter_complex_threads", "2"};
    for (const auto& layer : layers) {
        args << "-threads" << "1";
        if(ranged) args << "-ss" << number(layer.in);
        args << "-i" << layerPath(layer.path);
    }
    QStringList graph;
    graph << QString("color=c=black:s=%1x%2:r=%3:d=%4,format=yuv444p[base]")
        .arg(plan.width).arg(plan.height).arg(number(plan.fps), number(plan.duration));
    QString current = "base";
    // Rows are ordered front to back, matching a conventional layer stack.
    for (int i = int(layers.size()) - 1; i >= 0; --i) {
        const auto& layer = layers[i];
        if (layer.kind != LayerKind::Video || !layer.enabled) continue;
        // Some containers start audio at zero but the first video packet later.
        // Trim relative to the visible video, otherwise a one-frame range can
        // discard every picture and leave only the black composition base.
        graph << QString("[%1:v:0]setpts=PTS-STARTPTS,trim=start=%2:end=%3,setpts=(PTS-STARTPTS)/%4,"
            "scale=%5:%6:force_original_aspect_ratio=decrease,setsar=1,format=yuva444p,"
            "tpad=stop_mode=clone:stop_duration=1,trim=duration=%7,setpts=PTS+%8/TB[v%1]")
            .arg(i).arg(number(ranged?0:layer.in), number(ranged?layer.out-layer.in:layer.out), number(layer.speed))
            .arg(plan.width).arg(plan.height).arg(number(layer.duration()), number(layer.start));
        const QString next = QString("mix%1").arg(i);
        graph << QString("[%1][v%2]overlay=x=(W-w)/2:y=(H-h)/2:eof_action=pass:repeatlast=0:"
            "format=yuv444:enable='gte(t,%3)*lt(t,%4)'[%5]")
            .arg(current).arg(i).arg(number(layer.start), number(layer.start + layer.duration()), next);
        current = next;
    }
    graph << QString("[%1]fps=%2,tpad=stop_mode=clone:stop=-1,trim=end_frame=%3,setpts=N/(%2*TB)[video]")
        .arg(current, number(plan.fps)).arg(frames);
    QStringList audio;
    for (int i = 0; i < int(layers.size()); ++i) {
        const auto& layer = layers[i];
        if (layer.kind != LayerKind::Audio || !layer.enabled) continue;
        const QString tag = QString("a%1").arg(i);
        // atempo preserves pitch and supports independent audio playback speed.
        double remaining = layer.speed / std::clamp(layer.audioTimeScale,1.,8.);
        QString tempo;
        while (remaining > 2.) { tempo += "atempo=2,"; remaining /= 2.; }
        while (remaining < .5) { tempo += "atempo=0.5,"; remaining /= .5; }
        tempo += "atempo=" + number(remaining);
        graph << QString("[%1:a:0]atrim=start=%2:end=%3,asetpts=N/SR/TB,aresample=48000,"
            "%4,apad,atrim=duration=%5,adelay=%6:all=1[%7]")
            .arg(i).arg(number(ranged?0:layer.in), number(ranged?layer.out-layer.in:layer.out), tempo, number(layer.duration()))
            .arg(qRound64(layer.start * 1000)).arg(tag);
        audio << "[" + tag + "]";
    }
    if (!audio.isEmpty()) graph << audio.join("") + QString("amix=inputs=%1:duration=longest:normalize=0,"
        "alimiter=limit=0.95:latency=1,apad,atrim=duration=%2[audio]").arg(audio.size()).arg(number(plan.duration));
    args << "-filter_complex" << graph.join(";") << "-map" << "[video]";
    if (!audio.isEmpty()) args << "-map" << "[audio]" << "-c:a" << (preview ? "aac" : "pcm_s16le");
    args << "-threads" << "2" << "-c:v" << (preview ? "libx264" : "ffv1");
    if (preview) args << "-preset" << "ultrafast" << "-crf" << "20" << "-pix_fmt" << "yuv420p";
    else args << "-level" << "3" << "-pix_fmt" << "yuv444p";
    args << "-t" << number(plan.duration) << "-fps_mode" << "cfr" << output;
    plan.arguments = args;
    return plan;
}
}
