# FrameScale 0.1.0

Versão em desenvolvimento (alpha). Ainda não é uma versão estável.

Aplicação de ambiente de trabalho em C++/Qt para melhorar imagens e vídeos com motores reais de upscale, restauração e interpolação. Desenvolvida para a Prova de Aptidão Profissional (PAP). O trabalho próprio inclui a interface, integração dos motores, processamento assíncrono, fila, prévias, validação e proteção dos ficheiros; os modelos de IA são de terceiros.

## Funcionalidades existentes

- Upscale de imagens, vídeos e GIFs com Real-ESRGAN, Real-CUGAN e Anime4K/mpv; escala final de 1× a 10×, incluindo valores fracionários.
- Interpolação de vídeo com RIFE v4.6, combinável com upscale, corte e ajustes de imagem.
- Filtros manuais de redução de ruído, nitidez, deblock, deband e grão; restauração com os modelos compatíveis do catálogo.
- Projetos em abas, presets `.fspreset`, fila de exportação, progresso, cancelamento e recuperação de um resultado parcial quando disponível.
- Prévia da origem, navegação por fotogramas, zoom e comparação de um trecho processado. Rotação e espelhamento de **vídeo** aparecem na prévia, comparação, exportação e cópia.
- MP4/MKV/MOV, AVI, GIF, MP3, WAV e sequências PNG/JPEG; imagens individuais PNG/JPEG/WebP/BMP. Codecs e opções dependem do formato selecionado.
- Download por URL HTTP/HTTPS através de yt-dlp e Deno; [comportamento e limites](docs/url-downloads.md).
- Português de Portugal, português do Brasil e inglês; temas macOS Claro, macOS Escuro e Estúdio. Os nomes dos temas descrevem a aparência Qt, não suporte validado para macOS. Fontes do sistema, incluindo Segoe UI no Windows.

## Requisitos e utilização

A entrega validada é Windows x64, com Qt 6.11.2 e MinGW 13.1. Os motores de IA exigem uma GPU e um driver com Vulkan. O instalador inclui as dependências de execução; outro computador não precisa de Qt Creator, CMake ou Python. A execução sem IA continua a usar o FFmpeg distribuído.

Abrir um ficheiro, escolher o trecho e os ajustes, usar **Preview** para comparar e adicionar à fila. A fila guarda as opções de cada item. A cópia de vídeo e a exportação partilham o processamento: enquanto a cópia termina ou é cancelada, o início da fila fica indisponível. Itens pendentes continuam editáveis. Os destinos existentes exigem uma escolha entre substituir, manter ambos ou cancelar.

As prévias processadas usam intermediários sem perda e não representam a compressão final escolhida. O processamento exporta vídeo CFR: entradas VFR são amostradas pelos timestamps para uma cadência normalizada. A duração considera o vídeo selecionado, não o prolongamento de outra faixa de áudio. Na interpolação, o arredondamento pode acrescentar menos de um fotograma. Capítulos não são copiados nem remapeados; metadados comuns compatíveis continuam opcionais.

Resultados são validados antes da promoção ao destino, incluindo dimensões, contagem/cadência, streams e descodificação. Isso não garante ausência de artefactos de IA, melhoria visual universal ou desempenho em vídeos longos. A qualidade depende da origem, modelo, ajustes e GPU. A restauração trabalha por fotograma; não equivale a modelos temporais ou de difusão de outros produtos.

## Compilar e testar

Requisitos de desenvolvimento: C++17, CMake ≥ 3.16, Ninja e Qt ≥ 6.5 com Widgets, Multimedia e MultimediaWidgets. A combinação efetivamente testada é Qt 6.11.2 / MinGW 13.1; outras versões e sistemas precisam de validação própria. Os comandos seguintes partem desta pasta `FrameScale`; adapte os caminhos à instalação local.

```powershell
$env:PATH = 'C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;C:/Qt/6.11.2/mingw_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' --build build/Release --target FrameScale framescale-tests FrameScalePreviewDecodeCheck --parallel 2
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir build/Release --output-on-failure
.\build\Release\FrameScale.exe
```

CTest contém cinco testes: núcleo, interface, orientação na interface, processamento com ferramentas simuladas e fila. As simulações verificam falhas/cancelamento de forma determinística; os testes reais abaixo exercitam FFmpeg e os motores distribuídos. Requerem Python 3 e, quando usam IA, Vulkan. Criam media sintética e resultados em diretórios próprios.

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

O instalador é `dist/FrameScale-0.1.0-Windows-x64-Setup.exe`; o SHA-256 e o registo de construção são gerados ao lado. [Instruções de empacotamento](docs/windows-installer.md). Para uma entrega reproduzível, incluir os ficheiros novos de código/testes/recursos: apenas o último commit antigo não representa esta árvore de trabalho.

A [auditoria de 04/10/2026](docs/auditoria-pap-2026-10-04.md) regista o estado anterior às correções. O [fecho das correções de 05/10/2026](docs/correcoes-finais-2026-10-05.md) identifica os resultados atuais, o instalador e as pendências para PAP/distribuição. Os documentos de desenvolvimento em `docs/history` são evidências históricas, não a especificação atual.


### Edição por camadas

Projetos de vídeo têm uma camada de vídeo e outra de áudio quando a origem contém ambas. **Adicionar camada**, no menu do botão direito da timeline, importa vídeos ou ficheiros de áudio no mesmo projeto; também é possível arrastá-los para a timeline. Novas camadas visuais ficam acima das existentes. O menu do botão direito permite alterar a ordem; as camadas de áudio são misturadas na exportação.

**Velocidade** edita apenas a camada selecionada, mesmo quando áudio e vídeo estão vinculados. As bordas recortam cada camada, ocultando a parte removida; arrastar o corpo altera sua posição. O vínculo sincroniza o movimento das camadas da mesma origem, e **Separar áudio e vídeo** permite movê-las individualmente. **Del** exclui a camada selecionada com a timeline em foco; Ctrl+Z e Ctrl+Shift+Z desfazem/refazem as alterações. Cada aba preserva sua composição. A duração do projeto acompanha a camada que termina por último; intervalos sem vídeo ficam pretos e intervalos sem áudio ficam silenciosos.

A prévia das camadas é atualizada em segundo plano, com no máximo 720 pixels no lado maior; uma nova edição cancela a preparação anterior. A exportação compõe os ficheiros originais antes dos filtros de processamento. A alteração de velocidade em 2× mantém a contagem exata de fotogramas, inclusive com cadências fracionárias.

Validações sem inspeção visual: `FrameScaleLayerStateTests` verifica velocidade individual, exclusão, desfazer/refazer e isolamento entre projetos. `python tests/validate-layers.py` verifica exportações reais, conteúdo de vídeo/áudio, sobreposição, corte, exclusão e velocidade.
