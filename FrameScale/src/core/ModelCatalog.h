#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace framescale {

enum class MediaType {
    Image,
    Video
};

enum class UpscaleEngine {
    RealESRGAN,
    RealCUGAN,
    Anime4K
};

enum class DenoiseLevel {
    None = 0,
    Conservative = -1,
    Level1 = 1,
    Level2 = 2,
    Level3 = 3
};

struct UpscaleModel {
    std::string id;
    std::string displayName;
    UpscaleEngine engine;
    std::filesystem::path relativePath;
    std::vector<int> scales;
    std::vector<DenoiseLevel> denoiseLevels;
    std::vector<MediaType> mediaTypes;
};

struct RifeModel {
    std::string id;
    std::string displayName;
    std::filesystem::path relativePath;
};

const std::vector<UpscaleModel>& upscaleModels();
const std::vector<RifeModel>& rifeModels();

const UpscaleModel* findUpscaleModel(const std::string& id);
const RifeModel* findRifeModel(const std::string& id);

int nativeScale(const UpscaleModel& model, double requested);
bool supportsScale(const UpscaleModel& model, int scaleFactor);
bool supportsDenoise(const UpscaleModel& model, DenoiseLevel denoise);
bool supportsDenoise(
    const UpscaleModel& model,
    int scaleFactor,
    DenoiseLevel denoise);
bool supportsMediaType(const UpscaleModel& model, MediaType mediaType);

} // namespace framescale
