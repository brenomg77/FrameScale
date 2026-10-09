#include "core/ProcessingOptions.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <locale>
#include <sstream>
#include <system_error>

namespace framescale {

std::string orientationFilter(const VideoOrientation& orientation)
{
    std::string filter;
    auto append = [&](const char* value) {
        if (!filter.empty()) filter += ",";
        filter += value;
    };
    switch ((orientation.quarterTurns % 4 + 4) % 4) {
    case 1: append("transpose=clock"); break;
    case 2: append("hflip"); append("vflip"); break;
    case 3: append("transpose=cclock"); break;
    }
    if (orientation.horizontalFlip) append("hflip");
    if (orientation.verticalFlip) append("vflip");
    return filter;
}


namespace {
    std::string filterNumber(double value)
    {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << value;
        return text.str();
    }
    void appendFilter(std::string& chain, const std::string& filter)
    {
        if (!chain.empty())
            chain += ",";
        chain += filter;
    }
}
std::string enhancementDeinterlaceFilter(const EnhancementOptions& o)
{
    if (!o.enabled || !o.deinterlace)
        return { };
    return std::string("bwdif=mode=send_frame:parity=") + (o.deinterlace == 2 ? "tff" : o.deinterlace == 3 ? "bff"
                                                                                                           : "auto")
        + ":deint=all";
}
std::string enhancementCleanupFilter(const EnhancementOptions& o, bool /*video*/)
{
    if (!o.enabled)
        return { };
    std::string result;
    if (o.deblock) {
        appendFilter(result, "deblock=filter=weak:block=8:alpha=" + filterNumber(.12 * o.deblock / 100.) + ":beta=" + filterNumber(.08 * o.deblock / 100.) + ":gamma=" + filterNumber(.08 * o.deblock / 100.) + ":delta=" + filterNumber(.08 * o.deblock / 100.));
    }
    if (o.denoise) {
        // Spatial cleanup only: temporal averaging leaves trails on moving edges.
        // hqdn3d treats zero as its default temporal strength, so use a
        // positive epsilon whose temporal coefficients round to zero.
        const double strength = o.denoise * .24;
        appendFilter(result, "hqdn3d=" + filterNumber(strength) + ":" + filterNumber(strength * .75) + ":" + std::string("0.000001:0.000001"));
    }
    if (o.deband) {
        const auto amount = filterNumber(.04 * o.deband / 100.);
        appendFilter(result, "deband=1thr=" + amount + ":2thr=" + amount + ":3thr=" + amount + ":4thr=" + amount + ":range=16");
    }
    return result;
}
std::string enhancementFinishFilter(const EnhancementOptions& o)
{
    if (!o.enabled)
        return { };
    std::string result;
    // CAS still sharpens at strength=0, so switching from 0 to 1 caused a
    // large contrast/noise jump. Use a linear luma-only amount instead.
    if (o.sharpen)
        appendFilter(result, "unsharp=5:5:" + filterNumber(o.sharpen / 100.) + ":5:5:0");
    if (o.contrast || o.vibrance)
        appendFilter(result, "format=gbrp16le");
    if (o.contrast) {
        // Monotonic S curve with fixed black/white endpoints. Unlike linear
        // contrast, it does not clip whole shadow/highlight intervals.
        const double amount = o.contrast * .0008;
        appendFilter(result, "curves=master='0/0 0.25/" + filterNumber(.25 - amount)
            + " 0.5/0.5 0.75/" + filterNumber(.75 + amount) + " 1/1':interp=pchip");
    }
    if (o.vibrance)
        appendFilter(result, "vibrance=intensity=" + filterNumber(o.vibrance * .005));
    if (o.grain)
        appendFilter(result, "noise=alls=" + filterNumber(o.grain * .2) + ":allf=t+u:all_seed=7");
    return result;
}

bool audioOnly(const ProcessingOptions& o) { return o.mediaType == MediaType::Video && (o.outputSuffix == ".mp3" || o.outputSuffix == ".wav"); }
bool imageSequence(const ProcessingOptions& o) { return o.mediaType == MediaType::Video && (o.outputSuffix == ".png" || o.outputSuffix == ".jpg" || o.outputSuffix == ".jpeg"); }
std::string proresProfileName(int profile)
{
    static const char* names[] = { "ProRes 422 Proxy", "ProRes 422 LT", "ProRes 422", "ProRes 422 HQ", "ProRes 4444", "ProRes 4444 XQ" };
    return profile >= 0 && profile <= 5 ? names[profile] : "";
}
std::string proresPixelFormat(int profile)
{
    return profile >= 4 ? "yuv444p10le" : "yuv422p10le";
}
int scaledDimension(int source, double scale, bool even)
{
    const int pixels = static_cast<int>(std::lround(source * scale));
    return even ? std::max(2, (pixels + 1) / 2 * 2) : std::max(1, pixels);
}

std::vector<UpscalePass> upscalePasses(const ProcessingOptions& o)
{
    if (audioOnly(o))
        return {};
    const bool upscale = usesUpscale(o.operation)
        && (o.scaleFactor != 1 || (o.upscaleEngine == UpscaleEngine::RealCUGAN
            && o.denoise != DenoiseLevel::None));
    const bool restore = o.enhancement.enabled && o.enhancement.model > 0
        && o.enhancement.model <= 3;
    std::vector<UpscalePass> passes;
    if (restore) {
        UpscalePass restoration {
            o.enhancement.model == 3 ? UpscaleEngine::RealCUGAN : UpscaleEngine::RealESRGAN,
            o.enhancement.model == 3 ? "realcugan-se" : o.enhancement.model == 2
                ? "realesrgan-plus-anime-x4" : "realesrgan-plus-x4",
            o.enhancement.model == 3 ? DenoiseLevel::Conservative : DenoiseLevel::None,
            1, upscale
        };
        // The same network already restores while enlarging. Running it twice
        // needlessly removes detail and magnifies reconstruction artifacts.
        if (upscale && restoration.modelId == o.upscaleModelId
            && restoration.engine == o.upscaleEngine && restoration.denoise == o.denoise)
            return {{ o.upscaleEngine, o.upscaleModelId, o.denoise, o.scaleFactor, false }};
        passes.push_back(std::move(restoration));
    }
    if (upscale)
        passes.push_back({ o.upscaleEngine, o.upscaleModelId, o.denoise, o.scaleFactor, false });
    return passes;
}
bool needsUpscale(const ProcessingOptions& o)
{
    return !upscalePasses(o).empty();
}
bool usesUpscale(const Operation operation)
{
    return operation == Operation::Upscale || operation == Operation::UpscaleAndInterpolation;
}

bool usesInterpolation(const Operation operation)
{
    return operation == Operation::Interpolation || operation == Operation::UpscaleAndInterpolation;
}

std::vector<std::string> validate(const ProcessingOptions& requested)
{
    const auto& options = requested;
    std::vector<std::string> errors;
    if (options.mediaType == MediaType::Image && !options.orientation.isIdentity())
        errors.emplace_back("A rotação e o espelhamento só podem ser aplicados a vídeo.");
    const auto& enhancement = options.enhancement;
    if (enhancement.model < 0 || enhancement.model > 3)
        errors.emplace_back("O modelo de Enhancement não é válido.");
    for (int value : { enhancement.denoise, enhancement.sharpen, enhancement.deblock, enhancement.deband, enhancement.grain, enhancement.clarity })
        if (value < 0 || value > 100) {
            errors.emplace_back("Os ajustes de Enhancement devem estar entre 0 e 100.");
            break;
        }
    for (int value : { enhancement.contrast, enhancement.vibrance })
        if (value < -100 || value > 100) {
            errors.emplace_back("Cor e contraste devem estar entre -100 e 100.");
            break;
        }
    if (enhancement.deinterlace < 0 || enhancement.deinterlace > 3)
        errors.emplace_back("A opção de desentrelaçamento não é válida.");

    if (options.layered && options.layers.empty())
        errors.emplace_back("Adicione uma camada ao projeto antes de exportar.");
    for (const auto& layer : options.layers) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(layer.path, ec) || ec)
            errors.emplace_back("O ficheiro de uma camada não existe.");
        if (layer.path == options.outputPath || std::filesystem::equivalent(layer.path, options.outputPath, ec))
            errors.emplace_back("A saída não pode substituir o ficheiro de uma camada.");
        if (!std::isfinite(layer.speed) || layer.speed < .25 || layer.speed > 4
            || !std::isfinite(layer.in) || !std::isfinite(layer.out) || layer.in < 0 || layer.out <= layer.in
            || !std::isfinite(layer.sourceDuration) || layer.out > layer.sourceDuration + .001
            || !std::isfinite(layer.start) || layer.start < 0)
            errors.emplace_back("Uma camada tem velocidade ou intervalo inválido.");
    }
    if (options.inputPath.empty()) {
        errors.emplace_back("Selecione um ficheiro de entrada.");
    } else {
        std::error_code error;
        const bool isFile = std::filesystem::is_regular_file(options.inputPath, error);
        if (error || !isFile) {
            errors.emplace_back("O ficheiro de entrada não existe ou não é válido.");
        }
    }

    if (options.outputPath.empty()) {
        errors.emplace_back("Defina o ficheiro de saída.");
    } else {
        std::string extension = options.outputPath.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (options.mediaType == MediaType::Video && !imageSequence(options) && extension != ".mp4" && extension != ".gif" && extension != ".mkv" && extension != ".mov" && extension != ".avi" && extension != ".mp3" && extension != ".wav") {
            errors.emplace_back(
                "Escolha MP4, MKV, MOV, AVI, GIF, MP3, WAV ou uma sequência PNG/JPEG.");
        } else if (options.mediaType == MediaType::Image && extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp" && extension != ".webp") {
            errors.emplace_back(
                "A saída de imagem tem de usar PNG, JPG, BMP ou WebP.");
        }
        if (audioOnly(options) && extension != options.outputSuffix)
            errors.emplace_back("A extensão de saída não corresponde ao formato de áudio selecionado.");
        if (extension == ".gif" && ((usesInterpolation(options.operation) && options.targetFps > 100) || options.reductionFps > 100)) {
            errors.emplace_back(
                "GIF suporta até 100 FPS. Escolha MP4 para taxas superiores.");
        }
    }

    if (!options.inputPath.empty() && !options.outputPath.empty()) {
        std::error_code equivalentError;
        bool sameFile = std::filesystem::equivalent(
            options.inputPath, options.outputPath, equivalentError);
        if (equivalentError) {
            std::error_code inputError;
            std::error_code outputError;
            const std::filesystem::path normalizedInput = std::filesystem::weakly_canonical(options.inputPath, inputError);
            const std::filesystem::path normalizedOutput = std::filesystem::weakly_canonical(options.outputPath, outputError);
            if (!inputError && !outputError) {
#ifdef _WIN32
                std::wstring input = normalizedInput.native();
                std::wstring output = normalizedOutput.native();
                std::transform(input.begin(), input.end(), input.begin(),
                    [](const wchar_t character) {
                        return static_cast<wchar_t>(std::towlower(character));
                    });
                std::transform(output.begin(), output.end(), output.begin(),
                    [](const wchar_t character) {
                        return static_cast<wchar_t>(std::towlower(character));
                    });
                sameFile = input == output;
#else
                sameFile = normalizedInput == normalizedOutput;
#endif
            } else {
                sameFile = options.inputPath == options.outputPath;
            }
        }
        if (imageSequence(options)) {
            std::error_code ec;
            const auto target = std::filesystem::weakly_canonical(options.outputPath, ec);
            const auto input = std::filesystem::weakly_canonical(options.inputPath, ec);
            if (!ec) {
                auto relative = input.lexically_relative(target);
                if (!relative.empty() && *relative.begin() != "..")
                    sameFile = true;
            }
        }
        if (sameFile) {
            errors.emplace_back(
                "O ficheiro de saída tem de ser diferente do ficheiro de entrada.");
        }
    }

    if (options.mediaType == MediaType::Video) {
        if (options.videoCodec == "prores_ks" && (options.proresProfile < 0 || options.proresProfile > 5))
            errors.emplace_back("O perfil ProRes tem de estar entre Proxy e 4444 XQ.");
        if (options.audioBitRate < 32000 || options.audioBitRate > 320000 || (options.audioSampleRate != 0 && options.audioSampleRate != 44100 && options.audioSampleRate != 48000) || options.audioChannels < 0 || options.audioChannels > 2)
            errors.emplace_back("As opções de áudio não são válidas.");
        if (options.audioCodec != "copy" && options.audioCodec != "aac" && options.audioCodec != "pcm_s16le" && options.audioCodec != "libmp3lame")
            errors.emplace_back("O codec de áudio não é válido.");
        if (options.rateControl != "crf" && options.rateControl != "vbr" && options.rateControl != "cbr")
            errors.emplace_back("O modo de qualidade não é válido.");
        if (!audioOnly(options) && !imageSequence(options) && options.rateControl != "crf" && options.bitRate <= 0)
            errors.emplace_back("Defina uma taxa de bits maior que zero.");
        const std::vector<std::string> profiles { "auto", "baseline", "main", "high", "high444" };
        const std::vector<std::string> levels { "auto", "3.1", "4.0", "4.1", "4.2", "5.0", "5.1", "5.2", "6.0", "6.1", "6.2" };
        if (std::find(profiles.begin(), profiles.end(), options.videoProfile) == profiles.end() || std::find(levels.begin(), levels.end(), options.videoLevel) == levels.end())
            errors.emplace_back("O perfil ou nível de vídeo não é válido.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec == "libx265" && options.videoProfile != "auto")
            errors.emplace_back("Use o perfil automático para H.265.");
        if (options.bitRate < 0 || options.bufferSize < 0 || options.minRate < 0 || options.maxRate < 0 || options.qmin < -1 || options.qmax < -1 || options.qmin > 69 || options.qmax > 69 || (options.maxRate > 0 && options.minRate > options.maxRate))
            errors.emplace_back("Os limites do encoder não são válidos.");
        for (const auto& item : options.customEncoderOptions) {
            const std::vector<std::string> allowed { "tune", "profile:v", "level:v",
                "threads", "g", "bf" };
            if ((options.videoCodec == "prores_ks" && item.first == "profile:v") || std::find(allowed.begin(), allowed.end(), item.first) == allowed.end() || item.second.empty())
                errors.emplace_back(
                    "Opção personalizada inválida: " + item.first + ". Use tune, profile:v, level:v, threads, g ou bf.");
        }
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec != "libx264" && options.videoCodec != "libx265" && options.videoCodec != "prores_ks" && options.videoCodec != "mjpeg" && options.videoCodec != "gif") {
            errors.emplace_back("Selecione um codec de vídeo compatível com o formato de saída.");
        }
        auto container = options.outputPath.extension().string();
        std::transform(container.begin(), container.end(), container.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (options.keepAudio && container == ".mp4" && options.audioCodec != "copy" && options.audioCodec != "aac")
            errors.emplace_back("MP4 suporta áudio original compatível ou AAC nesta aplicação.");
        if (options.keepAudio && container == ".avi" && options.audioCodec != "copy" && options.audioCodec != "pcm_s16le" && options.audioCodec != "libmp3lame")
            errors.emplace_back("AVI suporta áudio PCM ou MP3 nesta aplicação.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec == "gif" && container != ".gif")
            errors.emplace_back("O codec GIF requer o formato GIF.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec == "libx264" && options.pixelFormat == "yuv444p" && options.videoProfile != "auto" && options.videoProfile != "high444")
            errors.emplace_back("H.264 com yuv444p requer perfil automático ou high444.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec == "prores_ks" && (options.pixelFormat != proresPixelFormat(options.proresProfile) || container != ".mov"))
            errors.emplace_back("ProRes requer QuickTime (MOV), yuv422p10le nos perfis 422 ou yuv444p10le nos perfis 4444.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec != "prores_ks" && (options.pixelFormat == "yuv422p10le" || options.pixelFormat == "yuv444p10le"))
            errors.emplace_back("Os formatos de píxel de 10 bits requerem ProRes nesta aplicação.");
        if (!audioOnly(options) && !imageSequence(options) && container == ".avi" && options.videoCodec != "mjpeg" && options.videoCodec != "libx264")
            errors.emplace_back("AVI suporta Motion JPEG ou H.264 nesta aplicação.");
        if (!audioOnly(options) && !imageSequence(options) && options.videoCodec == "mjpeg" && (container != ".avi" || options.pixelFormat != "yuvj420p"))
            errors.emplace_back("Motion JPEG requer AVI e yuvj420p.");
        static const std::vector<std::string> presets {
            "ultrafast", "superfast", "veryfast", "faster", "fast",
            "medium", "slow", "slower", "veryslow"
        };
        if (std::find(presets.begin(), presets.end(), options.encoderPreset) == presets.end()) {
            errors.emplace_back("O preset do encoder não é válido.");
        }
        if (options.crf < 0 || options.crf > 51) {
            errors.emplace_back("O CRF tem de estar entre 0 e 51.");
        }
        if (options.pixelFormat != "yuvj420p" && options.pixelFormat != "auto" && options.pixelFormat != "yuv420p" && options.pixelFormat != "yuv444p" && options.pixelFormat != "yuv422p10le" && options.pixelFormat != "yuv444p10le") {
            errors.emplace_back("O formato de píxel tem de ser yuv420p ou yuv444p.");
        }
        if (!std::isfinite(options.trimStartSeconds) || !std::isfinite(options.trimEndSeconds) || options.trimStartSeconds < 0.0 || options.trimEndSeconds < 0.0 || (options.trimEndSeconds > 0.0 && options.trimEndSeconds <= options.trimStartSeconds)) {
            errors.emplace_back("O intervalo de corte não é válido: o fim tem de ser "
                                "maior que o início.");
        }
    }

    if (!audioOnly(options) && usesUpscale(options.operation)) {
        if (!std::isfinite(options.scaleFactor) || options.scaleFactor < 1 || options.scaleFactor > 10) {
            errors.emplace_back("A escala de upscale tem de estar entre 1x e 10x.");
        }

        const UpscaleModel* model = findUpscaleModel(options.upscaleModelId);
        if (model == nullptr) {
            errors.emplace_back(
                "O modelo de upscale selecionado não existe no catálogo local.");
        } else {
            if (model->engine != options.upscaleEngine) {
                errors.emplace_back(
                    "O modelo de upscale não pertence ao motor selecionado.");
            }

            if (!supportsDenoise(*model, nativeScale(*model, options.scaleFactor),
                    options.denoise)) {
                errors.emplace_back("O modelo de upscale não suporta o nível de "
                                    "redução de ruído selecionado nesta escala.");
            }
            if (!supportsMediaType(*model, options.mediaType)) {
                errors.emplace_back("O modelo de upscale não suporta o tipo de "
                                    "multimédia selecionado.");
            }
        }
    }

    if (!audioOnly(options) && usesInterpolation(options.operation)) {
        if (options.mediaType != MediaType::Video) {
            errors.emplace_back("A interpolação RIFE só pode ser aplicada a vídeo.");
        }
        if (findRifeModel(options.rifeModelId) == nullptr) {
            errors.emplace_back(
                "O modelo RIFE selecionado não existe no catálogo local.");
        }
        if (options.targetFps < 2 || options.targetFps > 240) {
            errors.emplace_back("O FPS pretendido tem de estar entre 2 e 240.");
        }
    }

    if (options.reductionFps != 0 && (options.reductionFps < 1 || options.reductionFps > 240
        || options.mediaType != MediaType::Video || usesInterpolation(options.operation)))
        errors.emplace_back("A redu\u00e7\u00e3o de FPS exige v\u00eddeo, 1 a 240 FPS e interpola\u00e7\u00e3o desativada.");
    if (!std::isfinite(options.playbackSpeed) || options.playbackSpeed < .25 || options.playbackSpeed > 4.)
        errors.emplace_back("Playback speed must be between 0.25 and 4.");
    if (options.playbackSpeed != 1. && imageSequence(options))
        errors.emplace_back("Image sequences do not store playback duration. Select a video format to change speed.");
    if (options.audioOffsetMs < -600000 || options.audioOffsetMs > 600000)
        errors.emplace_back("Audio offset must be within +/- 600000 ms.");
    return errors;
}

std::filesystem::path suggestOutputPath(const std::filesystem::path& inputPath,
    const MediaType mediaType,
    const Operation operation,
    const double scaleFactor,
    const int targetFps)
{
    if (inputPath.empty()) {
        return { };
    }

    std::string suffix = "_framescale";
    if (usesUpscale(operation)) {
        std::ostringstream scale;
        scale.imbue(std::locale::classic());
        scale << scaleFactor;
        suffix += "_" + scale.str() + "x";
    }
    if (usesInterpolation(operation)) {
        suffix += "_" + std::to_string(targetFps) + "fps";
    }

    std::string sourceExtension = inputPath.extension().string();
    std::transform(
        sourceExtension.begin(), sourceExtension.end(), sourceExtension.begin(),
        [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::filesystem::path extension = mediaType == MediaType::Video
        ? std::filesystem::path(sourceExtension == ".gif" ? ".gif" : ".mp4")
        : inputPath.extension();

    std::filesystem::path fileName = inputPath.stem();
    fileName += suffix;
    fileName += extension;
    return inputPath.parent_path() / fileName;
}

} // namespace framescale
