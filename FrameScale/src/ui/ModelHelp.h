#pragma once

#include "HelpToolTip.h"
#include <QCoreApplication>
#include <QString>

inline QString upscaleEngineHelp(int engine)
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("ModelHelp", text); };
    if (engine == 0)
        return helpToolTip("Real-ESRGAN", tr("Amplia fotografias, filmagens e animação. Use Plus para texturas reais e Anime ou AnimeVideo para desenhos."));
    if (engine == 1)
        return helpToolTip("Real-CUGAN", tr("Indicado para anime e ilustrações. A redução de ruído ajuda em material comprimido, mas pode apagar traços finos se estiver forte."));
    return helpToolTip("Anime4K", tr("Amplia anime com processamento rápido. Use A para desfoque forte, B para desfoque leve e C para desenhos já nítidos."));
}

inline QString upscaleDenoiseHelp(int level)
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("ModelHelp", text); };
    const QString title = tr("Redução de ruído");
    if (level == 0)
        return helpToolTip(title, tr("Mantém o ruído e a textura da imagem original.\nUse em imagens limpas ou para manter o grão."));
    if (level == -1)
        return helpToolTip(title, tr("Aplica uma restauração suave.\nUse quando quiser limpar a imagem sem suavizar muito os detalhes."));
    if (level == 1)
        return helpToolTip(title, tr("Remove ruído leve.\nUse em imagens com poucos pontos ou manchas indesejados."));
    if (level == 2)
        return helpToolTip(title, tr("Remove ruído moderado.\nDiminua este nível se a pele ou o cabelo perderem textura."));
    return helpToolTip(title, tr("Remove ruído forte.\nUse apenas quando houver muito ruído, pois pode apagar detalhes finos."));
}

inline QString upscaleModelHelp(const QString& id)
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("ModelHelp", text); };
    if (id == "realesr-animevideov3")
        return helpToolTip("RealESRGAN AnimeVideo v3", tr("Indicado para episódios de anime e animação 2D. É mais leve que Anime x4; confira olhos, cabelo e legendas na preview."));
    if (id == "realesrgan-plus-anime-x4")
        return helpToolTip("RealESRGAN Anime", tr("Restaura contornos de ilustrações e anime. Pode reinterpretar linhas finas e exige mais processamento que AnimeVideo v3."));
    if (id == "realesrgan-plus-x4")
        return helpToolTip("RealESRGAN Plus", tr("Indicado para fotografias e filmagens. Reconstrói texturas, mas pode alterar a aparência de pele, grão e detalhes pequenos."));
    if (id == "realcugan-nose")
        return helpToolTip("RealCUGAN Nose", tr("Para desenhos e anime já limpos.\nAmplia sem escolher um nível de ruído."));
    if (id == "realcugan-pro")
        return helpToolTip("RealCUGAN Pro", tr("Alternativa ao SE para anime e desenho. Compare um trecho com movimento: Pro não significa melhor resultado em todas as cenas."));
    if (id == "realcugan-se")
        return helpToolTip("RealCUGAN SE", tr("Amplia anime com ruído ou compressão e oferece vários níveis de limpeza. Comece com pouca redução para preservar os detalhes."));
    if (id == "anime4k-v4-a")
        return helpToolTip("Anime4K A", tr("Use em anime com contornos desfocados ou marcas de compressão.\nEste modo reforça as linhas para deixá-las mais definidas."));
    if (id == "anime4k-v4-a+a")
        return helpToolTip("Anime4K A+A", tr("Aplica uma restauração mais intensa que o modo A.\nO processamento demora mais e pode deixar as linhas fortes demais."));
    if (id == "anime4k-v4-b")
        return helpToolTip("Anime4K B", tr("Use em anime cujas linhas estejam apenas um pouco desfocadas.\nEste modo reforça os contornos de forma mais suave que o modo A."));
    if (id == "anime4k-v4-b+b")
        return helpToolTip("Anime4K B+B", tr("Reforça a restauração suave do modo B.\nUse se as linhas continuarem desfocadas depois de experimentar B."));
    if (id == "anime4k-v4-c")
        return helpToolTip("Anime4K C", tr("Use para ampliar anime ou ilustrações que já estejam nítidos.\nEste modo evita reforçar demais os contornos existentes."));
    if (id == "anime4k-v4-c+a")
        return helpToolTip("Anime4K C+A", tr("Amplia com C e restaura detalhes com A.\nCompare com C para evitar contornos fortes."));
    if (id == "anime4k-v4.1-gan")
        return helpToolTip("Anime4K GAN", tr("Gera detalhes novos para dar mais definição ao anime.\nConfira a preview: os detalhes gerados podem diferir do original."));
    return {};
}
