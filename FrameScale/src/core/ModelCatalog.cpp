#include "core/ModelCatalog.h"

#include <algorithm>

namespace framescale {
namespace {

using D = DenoiseLevel;
using E = UpscaleEngine;
using M = MediaType;

const std::vector<UpscaleModel> kUpscaleModels {
    { "realesr-animevideov3", "RealESRGAN AnimeVideo v3", E::RealESRGAN,
      "realesrgan/realesr-animevideov3", { 2, 3, 4 }, { D::None }, { M::Image, M::Video } },
    { "realesrgan-plus-anime-x4", "RealESRGAN Plus Anime", E::RealESRGAN,
      "realesrgan/realesrgan-x4plus-anime", { 4 }, { D::None }, { M::Image, M::Video } },
    { "realesrgan-plus-x4", "RealESRGAN Plus", E::RealESRGAN,
      "realesrgan/realesrgan-x4plus", { 4 }, { D::None }, { M::Image, M::Video } },

    { "realcugan-nose", "RealCUGAN Nose", E::RealCUGAN,
      "realcugan/models-nose", { 2 }, { D::None }, { M::Image, M::Video } },
    { "realcugan-pro", "RealCUGAN Pro", E::RealCUGAN,
      "realcugan/models-pro", { 2, 3 }, { D::None, D::Conservative, D::Level3 },
      { M::Image, M::Video } },
    { "realcugan-se", "RealCUGAN SE", E::RealCUGAN,
      "realcugan/models-se", { 2, 3, 4 },
      { D::None, D::Conservative, D::Level1, D::Level2, D::Level3 },
      { M::Image, M::Video } },

    { "anime4k-v4-a", "Anime4K v4 Mode A", E::Anime4K,
      "libplacebo/anime4k-v4-a.glsl", { 2 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4-a+a", "Anime4K v4 Mode A+A", E::Anime4K,
      "libplacebo/anime4k-v4-a+a.glsl", { 2, 4 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4-b", "Anime4K v4 Mode B", E::Anime4K,
      "libplacebo/anime4k-v4-b.glsl", { 2 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4-b+b", "Anime4K v4 Mode B+B", E::Anime4K,
      "libplacebo/anime4k-v4-b+b.glsl", { 2, 4 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4-c", "Anime4K v4 Mode C", E::Anime4K,
      "libplacebo/anime4k-v4-c.glsl", { 2 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4-c+a", "Anime4K v4 Mode C+A", E::Anime4K,
      "libplacebo/anime4k-v4-c+a.glsl", { 2, 4 }, { D::None }, { M::Image, M::Video } },
    { "anime4k-v4.1-gan", "Anime4K v4.1 GAN", E::Anime4K,
      "libplacebo/anime4k-v4.1-gan.glsl", { 2, 4 }, { D::None }, { M::Image, M::Video } }
};

const std::vector<RifeModel> kRifeModels {
    // The packaged 20221029 runner documents compatibility through v4.6.
    { "rife-v4.6", "RIFE v4.6", "rife/rife-v4.6" }
};

template <typename T>
bool contains(const std::vector<T>& values, const T value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

} // namespace

const std::vector<UpscaleModel>& upscaleModels()
{
    return kUpscaleModels;
}

const std::vector<RifeModel>& rifeModels()
{
    return kRifeModels;
}

const UpscaleModel* findUpscaleModel(const std::string& id)
{
    const auto it = std::find_if(kUpscaleModels.begin(), kUpscaleModels.end(),
        [&id](const UpscaleModel& model) { return model.id == id; });
    return it == kUpscaleModels.end() ? nullptr : &*it;
}

const RifeModel* findRifeModel(const std::string& id)
{
    const auto it = std::find_if(kRifeModels.begin(), kRifeModels.end(),
        [&id](const RifeModel& model) { return model.id == id; });
    return it == kRifeModels.end() ? nullptr : &*it;
}

int nativeScale(const UpscaleModel& model, double requested)
{
    for (int scale : model.scales)
        if (scale >= requested) return scale;
    return model.scales.back();
}

bool supportsScale(const UpscaleModel& model, const int scaleFactor)
{
    return contains(model.scales, scaleFactor);
}

bool supportsDenoise(const UpscaleModel& model, const DenoiseLevel denoise)
{
    return contains(model.denoiseLevels, denoise);
}

bool supportsDenoise(
    const UpscaleModel& model,
    const int scaleFactor,
    const DenoiseLevel denoise)
{
    if (!supportsScale(model, scaleFactor) || !supportsDenoise(model, denoise)) {
        return false;
    }
    if (model.engine != UpscaleEngine::RealCUGAN) {
        return true;
    }
    if (model.id == "realcugan-se"
        && (denoise == DenoiseLevel::Level1 || denoise == DenoiseLevel::Level2)) {
        return scaleFactor == 2;
    }
    return true;
}

bool supportsMediaType(const UpscaleModel& model, const MediaType mediaType)
{
    return contains(model.mediaTypes, mediaType);
}

} // namespace framescale
