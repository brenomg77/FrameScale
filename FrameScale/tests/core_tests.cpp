#include "core/ModelCatalog.h"
#include "core/ProcessingOptions.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>

namespace {

int failures = 0;

void check(const bool condition, const char* expression, const int line)
{
    if (!condition) {
        std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

template <typename T>
bool hasValue(const std::vector<T>& values, const T value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool hasErrorContaining(
    const std::vector<std::string>& errors,
    const std::string& fragment)
{
    return std::any_of(errors.begin(), errors.end(),
        [&fragment](const std::string& error) {
            return error.find(fragment) != std::string::npos;
        });
}

} // namespace

int main()
{
    namespace fs = std::filesystem;
    using framescale::DenoiseLevel;
    using framescale::MediaType;
    using framescale::Operation;
    using framescale::ProcessingOptions;
    using framescale::UpscaleEngine;

    CHECK(static_cast<int>(DenoiseLevel::None) == 0);
    CHECK(static_cast<int>(DenoiseLevel::Conservative) == -1);
    CHECK(framescale::usesUpscale(Operation::Upscale));
    CHECK(!framescale::usesInterpolation(Operation::Upscale));
    CHECK(framescale::usesInterpolation(Operation::Interpolation));
    CHECK(!framescale::usesUpscale(Operation::Interpolation));
    CHECK(framescale::usesUpscale(Operation::UpscaleAndInterpolation));
    CHECK(framescale::usesInterpolation(Operation::UpscaleAndInterpolation));

    const fs::path suggested = framescale::suggestOutputPath(
        fs::path("media") / "video.mp4",
        MediaType::Video,
        Operation::UpscaleAndInterpolation,
        4,
        60);
    CHECK(suggested == fs::path("media") / "video_framescale_4x_60fps.mp4");
    const fs::path remuxedSuggestion = framescale::suggestOutputPath(
        fs::path("media") / "video.mkv",
        MediaType::Video,
        Operation::Upscale,
        2,
        60);
    CHECK(remuxedSuggestion == fs::path("media") / "video_framescale_2x.mp4");
    CHECK(framescale::suggestOutputPath(fs::path("media") / "animation.GIF", MediaType::Video,
              Operation::Upscale, 2, 60)
        == fs::path("media") / "animation_framescale_2x.gif");
#ifdef _WIN32
    const fs::path unicodeSuggestion = framescale::suggestOutputPath(
        fs::path(L"mídia") / L"vídeo ç漢字.mkv",
        MediaType::Video,
        Operation::Upscale,
        2,
        60);
    CHECK(unicodeSuggestion
        == fs::path(L"mídia") / L"vídeo ç漢字_framescale_2x.mp4");
#endif

    const auto& upscaleModels = framescale::upscaleModels();
    const auto& rifeModels = framescale::rifeModels();
    CHECK(upscaleModels.size() == 13);
    CHECK(rifeModels.size() == 1);

    std::set<std::string> ids;
    for (const auto& model : upscaleModels) {
        CHECK(!model.id.empty());
        CHECK(!model.displayName.empty());
        CHECK(!model.relativePath.empty());
        CHECK(!model.scales.empty());
        CHECK(!model.denoiseLevels.empty());
        CHECK(!model.mediaTypes.empty());
        CHECK(ids.insert(model.id).second);
        CHECK(framescale::findUpscaleModel(model.id) == &model);
        for (const int scale : model.scales) {
            CHECK(scale == 2 || scale == 3 || scale == 4);
            CHECK(framescale::supportsScale(model, scale));
        }
    }
    for (const auto& model : rifeModels) {
        CHECK(!model.id.empty());
        CHECK(!model.displayName.empty());
        CHECK(!model.relativePath.empty());
        CHECK(ids.insert(model.id).second);
        CHECK(framescale::findRifeModel(model.id) == &model);
    }
    CHECK(framescale::findUpscaleModel("missing") == nullptr);
    CHECK(framescale::findRifeModel("missing") == nullptr);

    const auto* animeVideo = framescale::findUpscaleModel("realesr-animevideov3");
    CHECK(animeVideo != nullptr);
    CHECK(animeVideo->engine == UpscaleEngine::RealESRGAN);
    CHECK(hasValue(animeVideo->scales, 2));
    CHECK(hasValue(animeVideo->scales, 3));
    CHECK(hasValue(animeVideo->scales, 4));

    const auto* cuganSe = framescale::findUpscaleModel("realcugan-se");
    CHECK(cuganSe != nullptr);
    CHECK(cuganSe->engine == UpscaleEngine::RealCUGAN);
    CHECK(framescale::supportsScale(*cuganSe, 4));
    CHECK(framescale::supportsDenoise(*cuganSe, DenoiseLevel::Level2));
    CHECK(framescale::supportsDenoise(*cuganSe, 2, DenoiseLevel::Level2));
    CHECK(!framescale::supportsDenoise(*cuganSe, 3, DenoiseLevel::Level2));
    CHECK(framescale::supportsDenoise(*cuganSe, 4, DenoiseLevel::Level3));

    const auto* anime4k = framescale::findUpscaleModel("anime4k-v4-a");
    CHECK(anime4k != nullptr);
    CHECK(anime4k->engine == UpscaleEngine::Anime4K);
    CHECK(framescale::supportsMediaType(*anime4k, MediaType::Image));
    CHECK(framescale::supportsMediaType(*anime4k, MediaType::Video));

    const fs::path temporary = fs::temp_directory_path() / "framescale_core_test.png";
    {
        std::ofstream file(temporary);
        file << "test";
    }

    ProcessingOptions valid;
    valid.inputPath = temporary;
    valid.outputPath = temporary.parent_path() / "framescale_core_test_output.png";
    valid.mediaType = MediaType::Image;
    valid.operation = Operation::Upscale;
    valid.upscaleEngine = UpscaleEngine::RealESRGAN;
    valid.upscaleModelId = "realesr-animevideov3";
    valid.scaleFactor = 3;
    CHECK(framescale::validate(valid).empty());

    ProcessingOptions orientedImage = valid;
    orientedImage.orientation.rotate(1);
    CHECK(hasErrorContaining(framescale::validate(orientedImage), "só podem ser aplicados a vídeo"));
    orientedImage.orientation = {};
    orientedImage.orientation.horizontalFlip = true;
    CHECK(hasErrorContaining(framescale::validate(orientedImage), "só podem ser aplicados a vídeo"));
    orientedImage.orientation = {};
    orientedImage.orientation.verticalFlip = true;
    CHECK(hasErrorContaining(framescale::validate(orientedImage), "só podem ser aplicados a vídeo"));

    ProcessingOptions unsupportedImageExtension = valid;
    unsupportedImageExtension.outputPath = temporary.parent_path()
        / "framescale_core_test_output.tiff";
    CHECK(hasErrorContaining(framescale::validate(unsupportedImageExtension),
        "PNG, JPG, BMP ou WebP"));

    ProcessingOptions wrongEngine = valid;
    wrongEngine.upscaleEngine = UpscaleEngine::Anime4K;
    CHECK(hasErrorContaining(framescale::validate(wrongEngine), "motor selecionado"));

    ProcessingOptions wrongScale = valid;
    wrongScale.upscaleModelId = "realesrgan-plus-x4";
    wrongScale.scaleFactor = 3;
    CHECK(framescale::validate(wrongScale).empty());
    CHECK(framescale::nativeScale(*framescale::findUpscaleModel(wrongScale.upscaleModelId), 3) == 4);

    ProcessingOptions wrongDenoise = valid;
    wrongDenoise.denoise = DenoiseLevel::Level1;
    CHECK(hasErrorContaining(framescale::validate(wrongDenoise), "redução de ruído"));

    ProcessingOptions validCugan = valid;
    validCugan.upscaleEngine = UpscaleEngine::RealCUGAN;
    validCugan.upscaleModelId = "realcugan-se";
    validCugan.denoise = DenoiseLevel::Level2;
    validCugan.scaleFactor = 2;
    CHECK(framescale::validate(validCugan).empty());

    ProcessingOptions invalidCuganScaleDenoise = validCugan;
    invalidCuganScaleDenoise.scaleFactor = 3;
    CHECK(hasErrorContaining(framescale::validate(invalidCuganScaleDenoise),
        "nesta escala"));

    ProcessingOptions validInterpolation = valid;
    validInterpolation.mediaType = MediaType::Video;
    validInterpolation.outputPath = temporary.parent_path() / "framescale_core_test_output.mp4";
    validInterpolation.operation = Operation::Interpolation;
    validInterpolation.rifeModelId = "rife-v4.6";
    validInterpolation.targetFps = 120;
    CHECK(framescale::validate(validInterpolation).empty());

    ProcessingOptions uppercaseMp4Video = validInterpolation;
    uppercaseMp4Video.outputPath = temporary.parent_path() / "framescale_core_test_output.Mp4";
    CHECK(framescale::validate(uppercaseMp4Video).empty());

    ProcessingOptions encoder = validInterpolation;
    encoder.videoCodec = "libx265";
    encoder.encoderPreset = "fast";
    encoder.pixelFormat = "yuv444p";
    encoder.crf = 27;
    encoder.keepAudio = false;
    encoder.trimStartSeconds = 0.25;
    encoder.trimEndSeconds = 2.5;
    CHECK(framescale::validate(encoder).empty());
    encoder.videoCodec = "unsupported";
    CHECK(hasErrorContaining(framescale::validate(encoder), "codec de vídeo"));
    encoder.videoCodec = "libx264";
    encoder.encoderPreset = "unrecognized";
    CHECK(hasErrorContaining(framescale::validate(encoder), "preset"));
    encoder.encoderPreset = "ultrafast";
    encoder.crf = 52;
    CHECK(hasErrorContaining(framescale::validate(encoder), "CRF"));
    encoder.crf = 18;
    encoder.pixelFormat = "invalid";
    CHECK(hasErrorContaining(framescale::validate(encoder), "píxel"));
    encoder.pixelFormat = "yuv420p";
    encoder.trimEndSeconds = encoder.trimStartSeconds;
    CHECK(hasErrorContaining(framescale::validate(encoder), "corte"));
    encoder.trimEndSeconds = std::numeric_limits<double>::infinity();
    CHECK(hasErrorContaining(framescale::validate(encoder), "corte"));
    encoder.trimEndSeconds = 0;
    CHECK(framescale::validate(encoder).empty());
    encoder.outputPath.replace_extension(".gif");
    CHECK(hasErrorContaining(framescale::validate(encoder), "100 FPS"));
    encoder.targetFps = 60;
    CHECK(framescale::validate(encoder).empty());

    ProcessingOptions prores = encoder;
    prores.outputPath.replace_extension(".MOV");
    prores.videoCodec = "prores_ks";
    prores.pixelFormat = "yuv422p10le";
    CHECK(framescale::validate(prores).empty());
    for (int profile = 0; profile <= 5; ++profile) {
        prores.proresProfile = profile;
        prores.pixelFormat = framescale::proresPixelFormat(profile);
        CHECK(framescale::validate(prores).empty());
        CHECK(!framescale::proresProfileName(profile).empty());
    }
    prores.proresProfile = -1;
    CHECK(hasErrorContaining(framescale::validate(prores), "perfil ProRes"));
    prores.proresProfile = 6;
    CHECK(hasErrorContaining(framescale::validate(prores), "perfil ProRes"));
    prores.proresProfile = 4;
    prores.pixelFormat = "yuv422p10le";
    CHECK(hasErrorContaining(framescale::validate(prores), "ProRes"));
    prores.proresProfile = 3;
    prores.pixelFormat = "yuv444p10le";
    CHECK(hasErrorContaining(framescale::validate(prores), "ProRes"));
    prores.pixelFormat = "yuv422p10le";
    prores.outputPath.replace_extension(".mkv");
    CHECK(hasErrorContaining(framescale::validate(prores), "ProRes"));
    prores.outputPath.replace_extension(".mp4");
    CHECK(hasErrorContaining(framescale::validate(prores), "ProRes"));
    prores.outputPath.replace_extension(".mov");
    prores.pixelFormat = "yuv420p";
    CHECK(hasErrorContaining(framescale::validate(prores), "ProRes"));

    ProcessingOptions nonMp4Video = validInterpolation;
    nonMp4Video.outputPath = temporary.parent_path() / "framescale_core_test_output.avi";
    CHECK(framescale::validate(nonMp4Video).empty());
    nonMp4Video.audioCodec = "aac";
    CHECK(hasErrorContaining(framescale::validate(nonMp4Video), "AVI suporta áudio"));
    nonMp4Video.outputPath.replace_extension(".mp4");
    nonMp4Video.audioCodec = "pcm_s16le";
    CHECK(hasErrorContaining(framescale::validate(nonMp4Video), "MP4 suporta áudio"));

    ProcessingOptions imageInterpolation = validInterpolation;
    imageInterpolation.mediaType = MediaType::Image;
    CHECK(hasErrorContaining(framescale::validate(imageInterpolation), "só pode ser aplicada a vídeo"));

    ProcessingOptions unknownModels = validInterpolation;
    unknownModels.operation = Operation::UpscaleAndInterpolation;
    unknownModels.upscaleModelId = "unknown-upscale";
    unknownModels.rifeModelId = "unknown-rife";
    const auto unknownErrors = framescale::validate(unknownModels);
    CHECK(hasErrorContaining(unknownErrors, "upscale selecionado não existe"));
    CHECK(hasErrorContaining(unknownErrors, "RIFE selecionado não existe"));

    for (double scale = 1; scale <= 10; scale += .5) {
        auto requested = valid;
        requested.scaleFactor = scale;
        CHECK(framescale::validate(requested).empty());
        CHECK(framescale::supportsScale(*animeVideo, framescale::nativeScale(*animeVideo, scale)));
    }
    auto fractional = valid;
    fractional.scaleFactor = 1.25;
    CHECK(framescale::validate(fractional).empty());
    CHECK(framescale::scaledDimension(101, 1.5) == 152);
    CHECK(framescale::scaledDimension(102, 1.5, true) == 154);
    fractional.scaleFactor = std::numeric_limits<double>::quiet_NaN();
    CHECK(hasErrorContaining(framescale::validate(fractional), "escala"));
    ProcessingOptions invalid = valid;
    invalid.outputPath = invalid.inputPath;
    invalid.scaleFactor = 11;
    const auto invalidErrors = framescale::validate(invalid);
    CHECK(hasErrorContaining(invalidErrors, "diferente"));
    CHECK(hasErrorContaining(invalidErrors, "entre 1x e 10x"));
#ifdef _WIN32
    ProcessingOptions differentlyCasedInput = valid;
    std::wstring differentlyCasedPath = temporary.wstring();
    std::transform(differentlyCasedPath.begin(), differentlyCasedPath.end(),
        differentlyCasedPath.begin(), [](const wchar_t character) {
            return static_cast<wchar_t>(std::towupper(character));
        });
    differentlyCasedInput.outputPath = fs::path(differentlyCasedPath);
    CHECK(hasErrorContaining(framescale::validate(differentlyCasedInput),
        "diferente"));
#endif

    const fs::path hardLink = temporary.parent_path()
        / "framescale_core_test_hardlink.png";
    std::error_code hardLinkError;
    fs::remove(hardLink, hardLinkError);
    hardLinkError.clear();
    fs::create_hard_link(temporary, hardLink, hardLinkError);
    if (!hardLinkError) {
        ProcessingOptions hardLinkedOutput = valid;
        hardLinkedOutput.outputPath = hardLink;
        CHECK(hasErrorContaining(framescale::validate(hardLinkedOutput),
            "diferente"));
    }

    ProcessingOptions enhanced = valid;
    enhanced.enhancement.contrast = -101;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "contraste"));
    enhanced.enhancement.contrast = 100;
    enhanced.enhancement.vibrance = 101;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "contraste"));
    enhanced.enhancement.vibrance = -100;
    enhanced.enhancement.clarity = 101;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "Enhancement"));
    enhanced = valid;
    enhanced.enhancement.denoise = 101;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "Enhancement"));
    enhanced.enhancement.denoise = 50;
    enhanced.enhancement.deinterlace = -1;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "desentrelaçamento"));
    CHECK(framescale::enhancementCleanupFilter(enhanced.enhancement,true).empty());
    CHECK(framescale::enhancementFinishFilter(enhanced.enhancement).empty());

    enhanced = valid;
    enhanced.operation = framescale::Operation::Copy;
    enhanced.scaleFactor = 10;
    enhanced.enhancement.enabled = true;
    enhanced.enhancement.model = 1;
    const auto restored = framescale::upscalePasses(enhanced);
    CHECK(restored.size() == 1);
    CHECK(restored[0].scaleFactor == 1);
    CHECK(restored[0].modelId == "realesrgan-plus-x4");
    CHECK(framescale::needsUpscale(enhanced));
    CHECK(enhanced.scaleFactor == 10); // Saved enlargement remains independent.
    enhanced.operation = framescale::Operation::Upscale;
    enhanced.upscaleEngine = framescale::UpscaleEngine::RealCUGAN;
    enhanced.upscaleModelId = "realcugan-se";
    enhanced.denoise = framescale::DenoiseLevel::Level3;
    const auto combined = framescale::upscalePasses(enhanced);
    CHECK(combined.size() == 2);
    CHECK(combined[0].modelId == "realesrgan-plus-x4");
    CHECK(combined[0].restoreOriginalSize);
    CHECK(combined[1].modelId == "realcugan-se");
    CHECK(combined[1].denoise == framescale::DenoiseLevel::Level3);
    CHECK(combined[1].scaleFactor == 10);
    CHECK(!combined[1].restoreOriginalSize);
    enhanced.upscaleEngine = framescale::UpscaleEngine::RealESRGAN;
    enhanced.upscaleModelId = "realesrgan-plus-x4";
    enhanced.denoise = framescale::DenoiseLevel::None;
    const auto shared = framescale::upscalePasses(enhanced);
    CHECK(shared.size() == 1); // Matching networks must not restore twice.
    CHECK(shared[0].scaleFactor == 10);
    CHECK(!shared[0].restoreOriginalSize);
    CHECK(framescale::scaledDimension(1254, 8) == 10032);
    CHECK(framescale::scaledDimension(1254, 10) == 12540);
    enhanced.operation = framescale::Operation::Copy;
    enhanced.enhancement.enabled = false;
    CHECK(!framescale::needsUpscale(enhanced));
    enhanced.enhancement.model = 4;
    CHECK(hasErrorContaining(framescale::validate(enhanced), "Enhancement"));

    framescale::VideoOrientation orientation;
    CHECK(framescale::orientationFilter(orientation).empty());
    orientation.rotate(-1);
    CHECK(framescale::orientationFilter(orientation) == "transpose=cclock");
    orientation.rotate(1);
    CHECK(orientation.isIdentity());
    orientation.horizontalFlip = true;
    orientation.rotate(1);
    CHECK(!orientation.horizontalFlip && orientation.verticalFlip);
    CHECK(framescale::orientationFilter(orientation) == "transpose=clock,vflip");
    orientation.rotate(3);
    CHECK(orientation.quarterTurns == 0 && orientation.horizontalFlip && !orientation.verticalFlip);

    std::error_code error;
    fs::remove(hardLink, error);
    error.clear();
    fs::remove(temporary, error);

    if (failures != 0) {
        std::cerr << "FrameScale core tests: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "FrameScale core tests: OK\n";
    return 0;
}
