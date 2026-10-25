#pragma once

#include "core/ModelCatalog.h"

#include <filesystem>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace framescale {

enum class Operation { Copy,
    Upscale,
    Interpolation,
    UpscaleAndInterpolation };

struct EnhancementOptions {
    bool enabled = false;
    // 0 filters only, 1 Real-ESRGAN Plus, 2 Plus Anime, 3 Real-CUGAN SE.
    int model = 0;
    int denoise = 0, sharpen = 0, deblock = 0, deband = 0, grain = 0;
    // Creative finishing controls; neutral values preserve existing projects.
    int clarity = 0; // 0..100, local luma contrast.
    int contrast = 0, vibrance = 0; // -100..100.
    // 0 off, 1 automatic field order, 2 top field first, 3 bottom field first.
    int deinterlace = 0;
};

struct VideoOrientation {
    int quarterTurns = 0; // Clockwise, normalized to 0..3.
    bool horizontalFlip = false;
    bool verticalFlip = false;
    void rotate(int turns) {
        quarterTurns = ((quarterTurns + turns) % 4 + 4) % 4;
        if (turns % 2 != 0) std::swap(horizontalFlip, verticalFlip);
    }
    bool isIdentity() const { return quarterTurns == 0 && !horizontalFlip && !verticalFlip; }
};
std::string orientationFilter(const VideoOrientation& orientation);

enum class LayerKind { Video, Audio, Text };
struct LayerTransform {
    double x=50, y=50, scaleX=100, scaleY=100, rotation=0, opacity=100;
    bool operator==(const LayerTransform& r) const {return x==r.x && y==r.y && scaleX==r.scaleX && scaleY==r.scaleY && rotation==r.rotation && opacity==r.opacity;}
    bool isIdentity() const {return *this==LayerTransform{};}
    bool valid() const {return std::isfinite(x) && std::isfinite(y) && std::isfinite(rotation) && std::isfinite(scaleX) && std::isfinite(scaleY) && std::isfinite(opacity)
        && x>=-1000 && x<=1000 && y>=-1000 && y<=1000 && scaleX>=1 && scaleX<=500 && scaleY>=1 && scaleY<=500 && rotation>=-36000 && rotation<=36000 && opacity>=0 && opacity<=100; }
};
struct TextLayerOptions {
    std::string content = "Texto", fontFamily = "Inter", fontStyle = "Regular";
    double fontSize = 72, lineHeight = 0, letterSpacing = 0;
    double x = 50, y = 50, horizontalScale = 100, verticalScale = 100;
    int alignment = 1; // Left, centre, right; position anchors the text block.
    bool fill = true, stroke = false, underline = false;
    std::string fillColor = "#ffffffff", strokeColor = "#ff000000";
    double strokeWidth = 2;
    double rotation = 0, opacity = 100;
};
struct MediaLayer {
    std::filesystem::path path;
    LayerKind kind = LayerKind::Video;
    double sourceDuration = 0, sourceFps = 25;
    double audioTimeScale = 1; // Sparse HE-AAC packet clock / decoded sample clock.
    int width = 0, height = 0;
    double start = 0, in = 0, out = 0, speed = 1;
    int link = 0;
    bool enabled = true;
    TextLayerOptions text;
    LayerTransform transform;
    double duration() const { return (out - in) / speed; }
};

inline LayerTransform layerTransform(const MediaLayer& l) {
    if(l.kind==LayerKind::Text) return {l.text.x,l.text.y,l.text.horizontalScale,l.text.verticalScale,l.text.rotation,l.text.opacity};
    return l.transform;
}
inline void setLayerTransform(MediaLayer& l,const LayerTransform& t) {
    if(l.kind==LayerKind::Text) {l.text.x=t.x;l.text.y=t.y;l.text.horizontalScale=t.scaleX;l.text.verticalScale=t.scaleY;l.text.rotation=t.rotation;l.text.opacity=t.opacity;}
    else l.transform=t;
}
struct ProcessingOptions {
    bool layered = false;
    std::vector<MediaLayer> layers;
    VideoOrientation orientation;
    EnhancementOptions enhancement;
    std::filesystem::path inputPath;
    std::filesystem::path outputPath;
    MediaType mediaType = MediaType::Image;
    Operation operation = Operation::Upscale;
    UpscaleEngine upscaleEngine = UpscaleEngine::RealESRGAN;
    std::string upscaleModelId = "realesr-animevideov3";
    std::string rifeModelId = "rife-v4.6";
    DenoiseLevel denoise = DenoiseLevel::None;
    double scaleFactor = 2;
    int targetFps = 60;
    int audioOffsetMs = 0; // Positive delays audio relative to video.
    bool audioVideoLinked = true;
    int videoTimelineStartMs = 0; // Editor placement; export uses the selected video interval.
    double playbackSpeed = 1.0; // Independent of output FPS.
    int reductionFps = 0; // Zero preserves the original cadence.
    std::string videoCodec = "libx264";
    int proresProfile = 3; // Proxy, LT, 422, HQ, 4444, 4444 XQ; legacy exports use HQ.
    std::string encoderPreset = "medium";
    std::string pixelFormat = "yuv420p";
    int crf = 18;
    bool keepAudio = true;
    std::string audioCodec = "copy";
    int audioBitRate = 192000;
    int audioSampleRate = 0;
    int audioChannels = 0;
    std::string rateControl = "crf";
    std::string videoProfile = "auto";
    std::string videoLevel = "auto";
    bool keepMetadata = true;
    std::string outputSuffix = ".mp4";
    int bitRate = 0, bufferSize = 0, minRate = 0, maxRate = 0;
    int qmin = -1, qmax = -1;
    std::vector<std::pair<std::string, std::string>> customEncoderOptions;
    double trimStartSeconds = 0.0;
    double trimEndSeconds = 0.0; // Zero means the end of the source.
};

std::string enhancementCleanupFilter(const EnhancementOptions& options, bool video);
std::string enhancementFinishFilter(const EnhancementOptions& options);
std::string enhancementDeinterlaceFilter(const EnhancementOptions& options);
// Ordered AI passes. A restoration pass returns to source size before a
// different upscale model runs; matching models share one inference.
struct UpscalePass {
    UpscaleEngine engine;
    std::string modelId;
    DenoiseLevel denoise;
    double scaleFactor;
    bool restoreOriginalSize = false;
};
std::vector<UpscalePass> upscalePasses(const ProcessingOptions& options);

bool audioOnly(const ProcessingOptions& options);
bool imageSequence(const ProcessingOptions& options);
std::string proresProfileName(int profile);
std::string proresPixelFormat(int profile);
int scaledDimension(int source, double scale, bool even = false);

bool needsUpscale(const ProcessingOptions& options);
bool usesUpscale(Operation operation);
bool usesInterpolation(Operation operation);

std::vector<std::string> validate(const ProcessingOptions& options);

std::filesystem::path suggestOutputPath(const std::filesystem::path& inputPath,
    MediaType mediaType,
    Operation operation, double scaleFactor,
    int targetFps);

} // namespace framescale
