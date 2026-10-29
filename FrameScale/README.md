# FrameScale 0.1.0

Versão em desenvolvimento (alpha). Ainda não é uma versão estável.

Aplicação de ambiente de trabalho em C++/Qt para melhorar imagens e vídeos com motores reais de upscale, restauração e interpolação. Desenvolvida para a Prova de Aptidão Profissional (PAP). O trabalho próprio inclui a interface, integração dos motores, processamento assíncrono, fila, prévias, validação e proteção dos ficheiros; os modelos de IA são de terceiros.

## Funcionalidades existentes

- Upscale de imagens, vídeos e GIFs com Real-ESRGAN, Real-CUGAN e Anime4K/mpv; escala final de 1× a 10×, incluindo valores fracionários.
- Interpolação de vídeo com RIFE v4.6, combinável com upscale, corte e ajustes de imagem; redução de FPS sem interpolação e velocidade de 0,25× a 4×.
- Filtros manuais de redução de ruído, nitidez, deblock, deband e grão; restauração com os modelos compatíveis do catálogo.
- Projetos em abas, presets `.fspreset`, fila de exportação, progresso, pausa/retoma, cancelamento e recuperação de um resultado parcial quando disponível.
- Camadas de vídeo, áudio e texto, com edição de texto diretamente na prévia. As ferramentas **Texto (T)** e **Seleção (V)** permitem criar texto e mover camadas visuais; as propriedades incluem tipografia, posição, escala, rotação e opacidade. [Edição e transformação de camadas](docs/text-layers.md).
- Prévia da origem, navegação por fotogramas, zoom e comparação de um trecho processado. Rotação e espelhamento de **vídeo** aparecem na prévia, comparação, exportação e cópia.
- MP4/MKV/MOV, AVI, GIF, MP3, WAV e sequências PNG/JPEG; imagens individuais PNG/JPEG/WebP/BMP. Codecs e opções dependem do formato selecionado.
- Abertura de imagens por links diretos HTTP/HTTPS e de vídeos através de yt-dlp e Deno; [comportamento e limites](docs/url-downloads.md).
- Português de Portugal, português do Brasil e inglês; temas Liquid Glass Claro, Liquid Glass Escuro e Estúdio, transparência e atalhos configuráveis. Fontes do sistema, incluindo Segoe UI no Windows.
- [Tour de sete etapas](docs/program-tour.md), apresentado uma vez na primeira abertura e disponível novamente em **Definições → Geral → Tour**. Cada aba conserva a posição da agulha, o zoom e o deslocamento da timeline.

## Requisitos e utilização

A distribuição Windows x64 usa Qt 6.11.2 e MinGW 13.1. Existe também um [AppImage Linux x86_64 experimental](docs/linux-package.md), compilado com Qt 6.8.3 para sistemas com glibc 2.39 ou mais recente; os testes automáticos não substituem a validação em desktops e GPUs Linux reais. Os motores de IA exigem uma GPU e um driver com Vulkan. Os pacotes incluem as dependências de execução; outro computador não precisa de Qt Creator, CMake ou Python. A execução sem IA continua a usar o FFmpeg distribuído.

Abrir um ficheiro, escolher o trecho e os ajustes, usar **Preview** para comparar e adicionar à fila. A fila guarda as opções de cada item. A cópia de vídeo e a exportação partilham o processamento: enquanto a cópia termina ou é cancelada, o início da fila fica indisponível. Itens pendentes continuam editáveis. Os destinos existentes exigem uma escolha entre substituir, manter ambos ou cancelar.

As prévias processadas usam intermediários sem perda e não representam a compressão final escolhida. O processamento exporta vídeo CFR: entradas VFR são amostradas pelos timestamps para uma cadência normalizada. Na origem sem composição, a duração considera o vídeo selecionado, não o prolongamento de outra faixa de áudio; nos projetos com camadas, considera a composição e o intervalo de exportação escolhido. Na interpolação, o arredondamento pode acrescentar menos de um fotograma. Capítulos não são copiados nem remapeados; metadados comuns compatíveis continuam opcionais.

A pausa suspende o processo externo ativo quando possível; não permite retomar uma exportação depois de fechar a aplicação. Durante o processamento, a aplicação verifica o espaço livre no destino e nos temporários e interrompe a tarefa abaixo de 512 MB. Este limite é uma proteção durante a execução, não uma estimativa do espaço total necessário.

Resultados são validados antes da promoção ao destino, incluindo dimensões, contagem/cadência, streams e descodificação. Isso não garante ausência de artefactos de IA, melhoria visual universal ou desempenho em vídeos longos. A qualidade depende da origem, modelo, ajustes e GPU. A restauração trabalha por fotograma; não equivale a modelos temporais ou de difusão de outros produtos.

## Compilar e testar

Requisitos de desenvolvimento: C++17, CMake ≥ 3.16, Ninja e Qt ≥ 6.5 com Widgets, Multimedia, MultimediaWidgets e Network. O ambiente Windows utiliza Qt 6.11.2 / MinGW 13.1; o workflow Linux utiliza Qt 6.8.3. Os comandos seguintes partem desta pasta `FrameScale`; adapte os caminhos à instalação local.

```powershell
$env:PATH = 'C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;C:/Qt/6.11.2/mingw_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/Release --target FrameScale framescale-tests FrameScaleUrlImageTests FrameScalePreviewDecodeCheck --parallel 2
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/Release --output-on-failure
.\build\Release\FrameScale.exe
```

O CMake regista 11 testes CTest: núcleo, interface, orientação na interface, processamento com ferramentas simuladas, fila, estado das camadas, texto e transformações, atalhos, material de vidro, imagens por URL e geometria dos controlos. O comando de compilação inclui explicitamente `FrameScaleUrlImageTests`, que ainda não é dependência de `framescale-tests`. Os alvos `FrameScaleProgramTourTests` e `FrameScaleSetupActionsTests` (este último apenas em Windows) são verificações adicionais, compiladas e executadas separadamente; não integram os 11 CTest.

As simulações verificam falhas/cancelamento de forma determinística; os testes reais abaixo exercitam FFmpeg e os motores distribuídos. Requerem Python 3 e, quando usam IA, Vulkan. Criam media sintética e resultados em diretórios próprios. A existência dos testes não equivale à sua aprovação: os resultados pertencem à revisão e ao ambiente efetivamente ensaiados.

```powershell
.\tests\validate-media-pipelines.ps1 -BuildDir build/Release -QtBin C:/Qt/6.11.2/mingw_64/bin
python tests/validate-audit-processing.py --build build/Release
python tests/validate-output-modules.py --build build/Release
python tests/validate-prores.py --build build/Release
python tests/validate-orientation.py --build build/Release
python tests/validate-enhancements.py --build build/Release
python tests/validate-denoise-motion.py --build build/Release
python tests/validate-combined-processing.py --build build/Release
python tests/validate-partial-export.py --build build/Release
python tests/validate-enhancement-quality.py --build-dir build/Release --with-ai
.\build\Release\FrameScaleUiSmoke.exe --processing-regressions
.\scripts\verify-runtime.ps1
```

O PATH deve incluir Qt/MinGW ao executar os testes de desenvolvimento. Os scripts Python aceitam `--qt-bin` quando necessário. A comparação de métricas sintéticas não substitui avaliação visual.

## Estrutura e entrega

- `src/core`: opções, validação e catálogo de modelos.
- `src/processing`: execução, temporários, validação e descodificação para prévias.
- `src/ui` e `src/platform`: interface e integração Windows.
- `assets`: apenas recursos utilizados pela aplicação.
- `runtime`, `models`: ferramentas, modelos e inventário SHA-256; [proveniência](runtime/README.md) e [avisos de terceiros](THIRD_PARTY_NOTICES.md).
- `tests`: testes determinísticos e regressões reais.
- `scripts`, `packaging`: verificação do runtime e geração do instalador.
- `docs`: documentação vigente, auditoria e [histórico](docs/history/README.md).

O instalador é `dist/FrameScale-0.1.0-Windows-x64-Setup.exe`; oferece instalar, reparar e desinstalar, com escolha da pasta e atalho opcional. O SHA-256 e o registo de construção são gerados ao lado. [Instruções de empacotamento](docs/windows-installer.md). O pacote Linux é descrito em [Linux x86_64](docs/linux-package.md). A versão permanece 0.1.0 alpha, sem atualização automática; o instalador Windows não tem assinatura digital de editor. Para uma entrega reproduzível, incluir os ficheiros novos de código/testes/recursos e verificar o pacote produzido a partir da revisão escolhida.

A [auditoria de 04/10/2026](docs/auditoria-pap-2026-10-04.md) e o [fecho das correções de 05/10/2026](docs/correcoes-finais-2026-10-05.md) registam as revisões e os resultados dessas datas. As [notas da versão](../RELEASE_NOTES.md) descrevem a evolução posterior. Os documentos datados e os ficheiros em `docs/history` conservam evidências históricas; não substituem o estado atual do código.


### Edição por camadas

Projetos de vídeo têm uma camada de vídeo e outra de áudio quando a origem contém ambas. **Adicionar camada**, no menu do botão direito da timeline, importa vídeos ou ficheiros de áudio no mesmo projeto; também é possível arrastá-los para a timeline. Novas camadas visuais ficam acima das existentes. O menu do botão direito permite alterar a ordem; as camadas de áudio são misturadas na exportação.

**Velocidade**, movimento, corte pelas bordas e exclusão atuam sobre o grupo quando áudio e vídeo estão vinculados. **Separar áudio e vídeo** permite editar as camadas individualmente. Silenciar áudio e ocultar vídeo continuam a atuar apenas sobre a pista escolhida. **Del** exclui a camada ou o grupo selecionado; durante a escrita num campo de texto, apaga caracteres. Ctrl+Z e Ctrl+Shift+Z desfazem/refazem as alterações. Cada aba preserva a sua composição. A duração do projeto acompanha a camada que termina por último, incluindo camadas desativadas; intervalos sem conteúdo visual ficam pretos e intervalos sem áudio ficam silenciosos.

A composição da prévia preserva a resolução do canvas e é atualizada em segundo plano; uma nova edição cancela a preparação anterior. Durante o arrasto de vídeo, a composição interativa usa fotogramas limitados a 1920×1080. A reprodução pode usar proxies temporários limitados a 30 FPS; a inspeção de fotogramas e a exportação usam a origem. A exportação compõe os ficheiros originais antes dos filtros de processamento. O áudio composto usa `atempo` para acompanhar a velocidade procurando preservar o tom.

Use **Texto (T)** e clique na prévia para criar uma camada de texto. **Seleção (V)** permite mover texto e vídeo; a janela **Editar** reúne tipografia e transformação. As alterações acompanham a comparação e a exportação. [Percurso completo e limites](docs/text-layers.md).

Validações sem inspeção visual: `FrameScaleLayerStateTests` verifica velocidade individual e de grupos ligados, corte, exclusão, desfazer/refazer, isolamento entre projetos e composição com FFmpeg. `FrameScaleTextLayerTests` cobre criação, edição e transformação de texto. `python tests/validate-layers.py` verifica exportações reais, conteúdo de vídeo/áudio, sobreposição, corte, exclusão e velocidade.
