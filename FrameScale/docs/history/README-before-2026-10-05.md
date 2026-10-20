# FrameScale

FrameScale é uma aplicação de ambiente de trabalho em C++ para melhorar imagens e vídeos. O objetivo final é permitir upscale com inteligência artificial e interpolação de fotogramas numa interface simples.

## Estado atual — v0.2.0

A versão 0.2 integra a interface Qt com motores reais de processamento:

- Real-ESRGAN ncnn Vulkan, Real-CUGAN ncnn Vulkan e Anime4K/mpv para upscale;
- RIFE ncnn Vulkan v4.6 para interpolação de vídeo;
- processamento de imagens, vídeos e GIFs animados, inclusive upscale + interpolação e corte;
- escala de 1× a 10×, digitável com até duas casas decimais e passos de 0,5×; o motor usa uma escala nativa e o FFmpeg ajusta as dimensões finais com Lanczos;
- exportação MP4/MKV/MOV, AVI, GIF, MP3, WAV e sequências PNG/JPEG; imagens individuais em PNG, JPEG, WebP e BMP;
- execução assíncrona, progresso e cancelamento;
- catálogo dos modelos compatíveis com os runners empacotados. Os pesos RIFE mais novos permanecem em `models/`, mas ficam ocultos até existir um runner validado para eles.

A interpolação aplica-se somente a vídeos. Os motores ncnn e o mpv requerem uma GPU/driver com Vulkan; os executáveis são procurados em `runtime/bin`.

Antes de confirmar uma imagem exportada, o FFmpeg decodifica o arquivo parcial para PNG e o pipeline verifica as dimensões resultantes. Assim, a exportação WebP funciona mesmo sem o plugin WebP opcional do Qt.

Todo vídeo exportado usa uma cadência constante (CFR). Entradas VFR são amostradas pelos timestamps de apresentação para uma cadência média normalizada antes dos motores processarem a sequência; os timestamps individuais da origem não são preservados. Na interpolação, a contagem usa `ceil(duração × FPS alvo)`, podendo estender o resultado por menos de um fotograma. Antes de substituir o destino, o MP4 parcial passa por validação de codec, formato de píxel, resolução, FPS, contagem de fotogramas e streams de áudio, seguida de uma decodificação integral de vídeo e áudio.

## Interface

A aplicação abre diretamente maximizada, sem tela de carregamento. A logo usa fundo branco com cantos arredondados no topo e no ícone Windows. A janela de boas-vindas aparece sobre o editor vazio, inicia em Home e pode ser fechada. Novo ficheiro usa um botão azul arredondado; Abrir e Home ficam na mesma coluna. O botão Selecionar mantém tamanho suficiente para o texto. A interface usa apenas o tema escuro Studio; os temas Apple Glass e a preferência Transparência foram removidos. Respeita redução de movimento do Windows.

File > Novo (Ctrl+N) e Abrir (Ctrl+O) abrem o seletor de arquivos. Sair aceita Ctrl+Q e Alt+F4. Ao abrir um arquivo, inclusive um que já está aberto, é possível criar uma aba ou substituir o projeto atual; cancelar preserva o projeto. Fechar a aplicação com projetos abertos exige confirmação. Cada aba guarda filtros, encoder e corte. O preview permanece pausado até Reproduzir, inclusive após trocar de aba.

No menu do botão direito da prévia de vídeo ou da aba do projeto, **Rotacionar** abre quatro ícones: girar 90° para a esquerda, girar 90° para a direita, espelhar horizontalmente e espelhar verticalmente. As transformações podem ser combinadas, aparecem na prévia e são aplicadas na exportação e na cópia do vídeo. Cada aba conserva sua orientação; o arquivo original permanece intacto.

Processing Settings reúne Filter (Upscaling) e Frame Interpolation. Escala aceita vírgula ou ponto, por exemplo 1,5 ou 2.25. O resultado usa dimensões inteiras e, quando o formato de píxel exige, arredonda para dimensões pares.

Encoder Options separa o **Formato** do codec. **Opções de formato…** abre uma janela com as opções de vídeo e áudio compatíveis; Cancelar descarta as alterações. MP4 e MKV oferecem H.264/H.265; QuickTime/MOV também oferece ProRes 422 HQ; AVI oferece Motion JPEG/H.264. Presets antigos com ProRes/MKV passam para MOV ao serem carregados. H.264/H.265 oferecem CRF, VBR de uma passagem ou CBR, taxa de bits, velocidade, formato de píxel e nível; H.264 também oferece perfil. ProRes HQ usa yuv422p10le e Motion JPEG usa yuvj420p. Não há um painel Advanced Options separado: os controles relevantes ficam na janela do formato. Presets antigos continuam conservando os parâmetros personalizados permitidos, mas os limites automáticos de VBR/CBR têm precedência sobre os limites antigos.

Áudio oferece cópia do original ou os codecs compatíveis com o contêiner, bitrate, amostragem e canais. MP3 e WAV exportam a primeira faixa de áudio do vídeo, respeitam o corte e dispensam os motores de IA. Entradas sem áudio produzem um erro claro. As sequências PNG/JPEG exportam fotogramas numerados (`frame_00000001.png`, etc.) para uma pasta, sem áudio. A sequência é validada inteira antes de promover a pasta temporária ao destino. Uma pasta com arquivos alheios à sequência não pode ser substituída.

Guardar preset abre o diálogo do Windows para escolher onde criar o arquivo `.fspreset`. Uma cópia gerenciada entra automaticamente na biblioteca do aplicativo (AppData/Presets), como ao importar um preset. O tooltip da lista mostra o caminho usado. Presets antigos nas preferências são migrados; importar também aceita o formato JSON anterior. Remover preset apaga a cópia da biblioteca após confirmação; o arquivo portátil guardado na pasta escolhida é mantido.

A prévia de vídeos grandes usa trechos de até oito segundos em 720p, preparados conforme a navegação, com cache de três trechos. Um primeiro frame aparece enquanto o trecho é preparado. A exportação usa o original. O controle de zoom e a roda do mouse ampliam ou reduzem a prévia. Segurar Espaço e arrastar move a imagem em qualquer direção; duplo clique ou Ctrl+0 volta a ajustar. A barra de reprodução foi removida; Espaço e o menu de contexto controlam play/pausa.

A timeline abaixo do preview permite arrastar as alças de início e fim e posicionar o cursor clicando na régua. A barra numérica Trim/Start foi removida. Espaço alterna reprodução e pausa quando a timeline está em foco; ao terminar, o cursor regressa ao início do trecho. O corte aplica-se ao export, incluindo o áudio; os pontos são alinhados aos frames da cadência normalizada.

Ver fila abre a fila sem adicionar um item. Add to Render Queue guarda uma cópia das opções e do corte. A fila permite selecionar itens, editar destinos, exportar selecionados ou todos, remover pela lixeira de cada linha ou pela tecla Delete. O item em processamento fica protegido; itens pendentes podem ser removidos. Fechar oculta a janela sem parar a exportação; Cancelar interrompe o item atual e deixa o restante pendente. Falhas preservam os outros itens e permitem tentar novamente. Quando o destino já existe, a confirmação oferece Substituir, Manter ambos (arquivo_2, arquivo_3, etc.) e Cancelar.

A barra azul mostra frames concluídos da etapa indicada, por exemplo `13/140 (9%)`. O número e o preenchimento usam a mesma medida. A contagem reinicia quando muda a etapa; preparação sem total conhecido usa progresso indeterminado. Não há contador de Frames/s. O tempo decorrido usa relógio monotônico e para ao terminar. Etapas rápidas podem concluir vários frames entre repinturas.

Metadados compatíveis do contêiner e das streams são preservados quando a opção está ativa. A nova exportação H.264/H.265 em modo CRF grava `crf`, `encoding_crf`, `encoding_preset` e `encoding_codec`; esses valores descrevem o novo encode. ProRes grava o perfil HQ; ProRes, Motion JPEG e os modos VBR/CBR removem as etiquetas CRF antigas. No MediaInfo, use a vista Texto ou Árvore e procure **Vídeo → Configurações de codificação → crf=…**: esse valor vem do codificador H.264/H.265, além das etiquetas do contêiner. ProRes não usa CRF. A verificação opcional `python tests/verify-mediainfo.py resultado.mp4 --crf 23` usa a biblioteca do MediaInfo instalada no Windows para conferir o valor real; `--title` e `--comment` verificam etiquetas da origem. A cópia de metadados depende do formato de destino e não garante todos os blocos EXIF/XMP de imagens.

A validação das sequências, a leitura de frames concluídos e a limpeza das pastas temporárias de vídeo acontecem em segundo plano para manter a interface responsiva. Em 1× sem restauração por IA, denoising do CUGAN ou interpolação, o vídeo segue diretamente ao FFmpeg, sem extração de PNGs ou passagem pelo motor de IA. O CUGAN em 1× continua processando quando o denoising está ativo. A extração usa compressão PNG reduzida. Os motores continuam usando sequências em disco: velocidade e espaço dependem da GPU, resolução, modelo e duração; não há garantia de desempenho equivalente ao Video2X.

GIF é tratado como animação, com exportação para GIF ou MP4. GIF não contém áudio e seus tempos são quantizados em centésimos de segundo; interpolação GIF limita-se a 100 FPS. As opções do encoder MP4 não são aplicadas ao GIF.

Configurações > General contém a seleção de idioma Português/English, aplicada imediatamente ao confirmar. O pacote está em build/FrameScale_UI/bin/FrameScale.exe; mantenha a pasta FrameScale_UI completa. Capturas atuais estão em build/output-module-captures/.

O teste opcional pela interface usa FRAMESCALE_UI_VIDEO com um vídeo curto de 4 FPS; FRAMESCALE_UI_CAPTURES define a pasta das capturas. FRAMESCALE_UI_HEAVY aponta para um vídeo grande; FRAMESCALE_UI_SEGMENTS=1 verifica busca e reprodução entre trechos com a fixture de 20 segundos. A suíte CTest também cobre a fila: fechamento, Delete/lixeira, cancelamento, retry e passagem segura entre jobs. Os testes reais desta revisão estão em build/final-media-check/ (GIF, H.265 e corte com áudio).

## Requisitos de desenvolvimento

- compilador compatível com C++17;
- CMake 3.16 ou superior;
- Qt 6 com os módulos Widgets, Multimedia e MultimediaWidgets.

## Compilar

```bash
cmake -S . -B build
cmake --build build --config Release
```

Quando o Qt não for encontrado automaticamente, indique a instalação através de `CMAKE_PREFIX_PATH`. Exemplo no Windows:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\msvc2022_64
cmake --build build --config Release
```

## Testes

O build normal compila apenas o aplicativo. Os executáveis de teste ficam disponíveis pelo alvo explícito `framescale-tests`. No Ninja/Qt Creator, as compilações C++ são limitadas a duas em paralelo (ajustável com `-DFRAMESCALE_COMPILE_JOBS=2`) e os links a um. As fontes e imagens são embutidas com `qt_add_big_resources`, evitando compilar os dados binários como um enorme arquivo C++.

Depois de configurar e compilar o projeto:

```bash
cmake --build build --target framescale-tests
ctest --test-dir build --output-on-failure
```

Os testes incluem contratos do catálogo/opções, um smoke test offscreen que verifica menus, logos, preview, opções e abertura/validação da janela de exportação e uma suíte determinística dos pipelines com ferramentas externas simuladas. Essa suíte cobre os três motores de upscale em imagem e vídeo, RIFE isolado e combinado na ordem RIFE→upscale, argumentos do encode H.264, commit seguro, validação de contêiner/streams/metadados, decodificação final, fallback AAC seletivo para múltiplas streams, falhas de probe/motor/modelo, cancelamento reentrante nos limites de etapa e de retry, reutilização do mesmo job, destruição durante encode, limpeza de temporários, progresso monotônico e isolamento do runtime. O executável `FrameScaleProcessingSmoke` também preserva o modo CLI para testar os pipelines reais com fixtures pequenas.

No Windows, a etapa `validate-media-pipelines` compila a aplicação e executa os motores reais com fixtures pequenas geradas localmente. Requer PowerShell e uma GPU/driver Vulkan:

```powershell
cmake --build build --target validate-media-pipelines
```

Também pode executar o script diretamente, indicando o diretório que contém `FrameScaleProcessingSmoke.exe`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/validate-media-pipelines.ps1 -BuildDir build/Claude_Debug
```

O script verifica os três motores em imagem/vídeo, RIFE isolado e combinado, escalas e formatos de imagem, VFR, rotação e preservação/conversão de áudio. Cada execução conserva entradas, saídas, logs e `summary.json` num novo subdiretório do build. As saídas passam por verificações independentes com FFprobe e decodificação com FFmpeg; o caso VFR também compara a sequência de cores com uma referência amostrada por timestamps; qualquer falha devolve um código de saída diferente de zero. Estes testes com GPU são opcionais e não integram o `ctest` padrão.

A matriz dos novos módulos de saída pode ser executada com:

```powershell
python tests/validate-output-modules.py --build build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug
```

Ela gera fixtures pequenas e verifica AVI, MP3/WAV com corte, escala fracionada, sequências PNG/JPEG com interpolação, CRF/VBR/CBR, ProRes, GIF, proteção e substituição de pastas. Logs e relatórios ficam em `build/.../output-module-validation/`.

Validação desta revisão em **16/09/2026**: **4/4 suítes CTest, 20/20 cenários com motores reais e 14/14 cenários dos módulos de saída aprovados**. A interface também passou no teste Windows usando somente as DLLs e plugins do pacote, e o MediaInfo confirmou CRF 18, título e comentário numa exportação H.264 com escala 1,5×. Registros locais: `build/output-module-tests.log`, `build/output-module-ui.log`, `build/output-module-package-check.log` e `build/output-module-mediainfo.json`.

Validação de referência concluída em **13/09/2026**, com Qt 6.11.2/MinGW e NVIDIA GeForce GTX 1070: **3/3 testes CTest e 20/20 cenários com motores reais aprovados**. Foram corrigidas a localização dos pares Real-ESRGAN por escala e a validação de WebP sem plugin Qt opcional. O [relatório JSON desta execução](build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug/media-validation/20260913-111154-4391c92466df42ff9ae0b81fc7e9ca58/summary.json) fica no build local; o script recria as fixtures e os resultados. Esta matriz verifica integração com amostras pequenas e presets representativos, sem medir desempenho em vídeos longos.

Para criar um diretório independente com o executável, runtime, modelos, licenças, DLLs e plugins Qt:

```bash
cmake --install build --prefix build/FrameScale-package
```

## Runtime de processamento

A aplicação espera os seguintes arquivos em `runtime/bin`:

- `ffmpeg.exe` e `ffprobe.exe`;
- `realesrgan-ncnn-vulkan.exe`;
- `realcugan-ncnn-vulkan.exe`;
- `rife-ncnn-vulkan.exe`;
- `mpv.exe` e suas DLLs necessárias.

As origens, versões e hashes dos pacotes binários estão em `runtime/manifest.json`. Os downloads e pacotes extraidos sao caches opcionais e podem ser removidos; URLs e hashes permanecem no manifesto. O CMake copia apenas os binários selecionados de `runtime/bin`, modelos, manifesto, avisos e textos de licença para junto do executável; `cmake --install` também executa o deploy das dependências Qt. O manifesto e os arquivos de auditoria não substituem a oferta ou entrega de código-fonte correspondente exigida por licenças GPL/LGPL. Consulte `THIRD_PARTY_NOTICES.md` e `runtime/licenses/` e faça a revisão de conformidade antes de redistribuir a aplicação.

## Estrutura

```text
FrameScale/
├── CMakeLists.txt
├── README.md
├── THIRD_PARTY_NOTICES.md
├── models/
├── runtime/
│   ├── bin/
│   ├── licenses/
│   └── manifest.json
├── src/
│   ├── core/
│   ├── platform/
│   ├── processing/
│   ├── main.cpp
│   └── MainWindow.*
└── tests/
    ├── core_tests.cpp
    ├── processing_smoke.cpp
    ├── ui_smoke.cpp
    ├── fake_processing_tool.cpp
    └── validate-media-pipelines.ps1
```

## Fontes técnicas principais

- [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html)
- [QFileDialog](https://doc.qt.io/qt-6/qfiledialog.html)
- [QProcess](https://doc.qt.io/qt-6/qprocess.html)
- [Qt — primeiros passos com CMake](https://doc.qt.io/qt-6/cmake-get-started.html)

## Arquivos grandes

Imagens são decodificadas para uma miniatura em um processo FFmpeg separado. Para vídeo acima de 1080p, a aplicação prepara uma cópia temporária de prévia em até 1280×720, com progresso e dois threads de codificação; a exportação sempre usa o original. Até três trechos de oito segundos ficam em cache durante a sessão. Fechar ou mudar o projeto cancela a preparação pendente.

Os motores ncnn usam tiles de 128 pixels e um worker por etapa para limitar alocações simultâneas de GPU. PNGs intermediários acima de 16 megapixels são verificados por dimensões e fechamento do arquivo, evitando decodificações completas redundantes; o resultado continua passando pela validação integral do FFmpeg. Modelos Plus têm escala nativa 4×: gerar 8K a partir de 4K exige produzir 16K intermediário antes de reduzir, sendo mais caro que AnimeVideo v3 em 2×.

Validação desta revisão: prévia reduzida de 4K, exportação real de dois frames 3840×2160 para 7680×4320 com tags title/comment preservadas, e testes curtos separados dos modelos Plus e Plus Anime. Isso não constitui um benchmark de vídeos longos.
## Pasta do projeto

A pasta de trabalho atual é C:\Users\Admin\Downloads\FrameScale_v0.2.0_source. Abra essa pasta diretamente com File > Open Folder. Não é necessário um arquivo .code-workspace. Os caminhos do CMake e do editor foram atualizados. A pasta anterior com “Copy” foi preservada porque estava bloqueada pelo Windows durante a migração.

A sondagem inicial usa a contagem declarada pelo contêiner quando disponível; somente formatos sem essa informação exigem uma contagem completa antes do processamento. O teste de entrada MKV também verifica essa segunda passagem e a preservação do áudio.

### Revisão da interface de 16/09/2026

O editor vazio mostra um documento sem título atrás das boas-vindas. Ao escolher o ficheiro, a janela converge para a área de prévia; com redução de movimento a troca é imediata. A barra Composition foi removida. O zoom mostra somente a percentagem atual no canto inferior direito, calculada em relação à resolução original, mesmo usando uma prévia leve.

Avisos de preparação ocupam a linha fixa do zoom e não deslocam a timeline. Cliques feitos durante a preparação substituem a posição pendente; a última posição vence. A revisão seguinte organiza Codificação em Formato e Opções de formato, com presets no rodapé.


### Timeline e comparação de prévias

Novo ficheiro, Abrir e Selecionar usam controles compactos, sem cortar o texto. A timeline mostra miniaturas amostradas do vídeo e um cursor azul. Durante arrastos rápidos, um leitor de fotogramas separado atualiza a imagem enquanto a prévia de reprodução prepara o trecho necessário; pedidos intermediários são agrupados e a última posição prevalece. As miniaturas carregam em segundo plano, com um único processo por vez.

O ícone de volume abre um controle de 0 a 100%, lembrado entre sessões. Isso afeta somente a reprodução, não o áudio exportado.

**Comparar** abre Original e o nome do arquivo lado a lado. Para vídeo, gera até dois segundos dentro do corte atual, perto do cursor, usando as configurações atuais de processamento e encoder, incluindo interpolação real. Original lê diretamente o arquivo de entrada, no instante correspondente ao corte; o resultado usa o formato escolhido quando reproduzível. GIF, áudio e sequências usam temporariamente H.264 para permitir a comparação visual. Imagens usam PNG. Os dois lados têm reprodução/pausa e posição compartilhadas; apenas o original reproduz áudio. Fechar cancela a geração em andamento. Esse processamento pode demorar em resoluções altas; a janela apresenta a etapa e o progresso. Na comparação, os frames são decodificados na resolução nativa. Um codec incompatível com o leitor Qt usa fallback lossless em 8 bits; não há certificação de monitoramento HDR.

Em áudio, **Manter original** copia as streams quando possível. Para precisão do corte, ou quando o áudio original não pertence ao perfil de compatibilidade MP4/MOV do programa, há conversão para AAC. AVI usa PCM/MP3. WAV e MP3 produzem somente áudio; a taxa de bits não é mostrada como qualidade CRF de vídeo.

A pesquisa de motores e filtros de Enhancement, com fontes, limites e uma prova local de sete filtros, está em [docs/enhancements-research.md](docs/enhancements-research.md). A implementação inicial de seis controles está descrita abaixo.

Validação desta revisão: quatro testes CTest; teste nativo Windows com arrasto contínuo, 16 miniaturas, volume, prévia 4K e comparação real com upscale/interpolação; matriz de formatos com fallback de áudio PCM/MKV para AAC/MP4. Para repetir a prova isolada dos filtros, execute `python tests/probe-enhancement-filters.py`.


### Enhancement e controles de prévia — 19/09/2026

**Processamento → Enhancement** ativa um painel com cinco intensidades de 0 a 100: redução de ruído, nitidez, redução de blocos, redução de banding e grão. Para vídeo há também desentrelaçamento desligado/automático/campo superior/campo inferior. Use essa opção somente em fontes entrelaçadas; ela conserva a cadência de saída, sem dobrar o FPS. Repor ajustes zera os filtros. A chave geral desliga os efeitos sem perder os valores.

O painel reúne filtros FFmpeg e os modelos locais Real-ESRGAN Plus, Real-ESRGAN Anime e Real-CUGAN SE. Não inclui os modelos proprietários do Topaz. Denoise, blocos e banding vêm antes dos motores de interpolação/upscale; nitidez e grão são aplicados na resolução final. O desentrelaçamento precede a normalização da cadência. Com Somente filtros e em 1× sem interpolação, o vídeo segue num único encode, sem uma sequência PNG intermediária. Ao selecionar um modelo de restauração, ele é executado mesmo em 1×; o resultado volta à resolução de saída escolhida. A redução de ruído temporal é própria para vídeo; imagens são tratadas individualmente. Saídas somente de áudio ignoram Enhancement.

Os ajustes pertencem a cada projeto e entram no snapshot da fila. **Guardar preset**, em Codificação, salva também Enhancement no `.fspreset`; carregar restaura os dois. Presets antigos sem essa seção carregam com Enhancement desligado. A comparação de até dois segundos usa os efeitos apenas no resultado. Original abre diretamente o arquivo de entrada e alinha sua posição ao início do trecho; ele não passa por ProcessingJob nem pelos ajustes de exportação. Os dois lados decodificam na resolução nativa, preservando detalhes ao dar zoom.

A prévia usa o cursor normal. Com foco na imagem, segure **Espaço + arrasto** para mover livremente, inclusive quando a imagem cabe na janela. Soltar Espaço restaura o cursor; na timeline, Espaço continua reproduzindo/pausando. A roda reduz e amplia a partir do zoom real, também na comparação, sem ficar presa ao menor preset. Duplo clique ou Ctrl+0 recentra e ajusta à janela.

As alças de corte sempre alinham a fotogramas. Segurar **Shift** durante o arrasto acrescenta encaixe magnético no cursor e nos limites do trecho/vídeo, num raio de 10 pixels. O popup de volume fecha ao clicar novamente no seu botão. Comparar tem largura mínima e texto branco legível. **Manter áudio original** oculta taxa de bits, amostragem e canais; desligar **Incluir áudio** oculta todas essas opções sem apagar os valores.

Validação: `python tests/validate-enhancements.py` gera mídia sintética e passa por `ProcessingJob` real, verificando alteração de pixels, bypass, cadência, corte, áudio, metadados/CRF, upscale + interpolação, imagens, GIF e sequências. O teste nativo de interface inclui cursor/arrasto, Shift, zoom da comparação, popup de volume e persistência em projeto/preset. Os fixtures pequenos verificam correção; não estimam desempenho em vídeos longos ou 8K.


### Correção de restauração e comparação — 19/09/2026

A logo usa o PNG transparente fornecido, também no ícone do executável. **Enhancement** oferece cartões para Somente filtros, Real-ESRGAN Plus (filmagem/fotografia), Real-ESRGAN Anime e Real-CUGAN SE (anime/ilustração). O modelo escolhido substitui o motor de upscale numa única passagem; os seletores redundantes ficam desativados. A escala continua definida no Filtro e, com ele desligado, mantém-se a resolução original.

Os modelos e as intensidades são escolhidos manualmente. Ativar o painel conserva os valores existentes; não aplica perfis prontos. Repor ajustes zera as intensidades. A redução de ruído tem uma faixa mais ampla; valores altos suavizam texturas. O botão **Comparar**, junto à prévia, gera o trecho com os ajustes atuais. A prévia principal continua sendo a origem, sem os efeitos.

A restauração por IA usa os modelos já distribuídos e trabalha por fotograma; não equivale aos modelos temporais, de rosto ou de difusão do Topaz. Presets antigos preservam Somente filtros até que outro modelo seja escolhido. `tests/validate-enhancements.py` inclui os três modelos em 1×, tanto em imagem como vídeo, bypass quando desligados e uma medição de redução de ruído contra uma referência sintética limpa.


### Controles e comparação — revisão de 19/09/2026

- Intensidades comportam os três dígitos de 100. A roda do mouse não altera campos numéricos, listas ou sliders; continua ampliando/reduzindo quando usada sobre a imagem. Escala e FPS têm setas visíveis.
- O painel **Melhoria de imagem** é manual, sem os perfis Equilibrado/Manual e sem um segundo botão de comparação. Cada modelo explica o tipo de material que trata. O desentrelaçamento explica o efeito de pente e as opções de ordem dos campos ao passar o ponteiro.
- **Comparar** usa cabeçalhos Original/Resultado com o nome do arquivo e uma timeline com miniaturas. Espaço reproduz/pausa; as setas navegam por fotograma quando a timeline tem foco. O botão Reproduzir foi removido.
- O zoom é vinculado: 100% ajusta cada vídeo ao respectivo painel. É um percentual relativo ao enquadramento da comparação, não uma indicação de pixels nativos. A prévia principal mantém seu zoom em relação à resolução original.
- Fechar cancela em segundo plano. A codificação temporária de vídeo é H.264 lossless ultrafast e sem áudio; o áudio vem da fonte original. Isso compara o processamento, não a compressão final escolhida para exportação.
- As duas últimas comparações concluídas são reaproveitadas enquanto o programa está aberto. Alterar origem, corte, motor, escala, FPS ou efeitos invalida o resultado correspondente. A primeira restauração ainda executa o modelo na resolução solicitada; o cache não reduz a qualidade nem acelera a inferência de um trecho novo.


### Correção de rastros no Enhancement

A redução de ruído agora usa apenas informação do próprio fotograma. O filtro temporal anterior em intensidade 100 misturava movimento de fotogramas anteriores; zerar os parâmetros não era suficiente, pois hqdn3d interpreta zero como valor padrão. A intensidade espacial foi preservada, com coeficientes temporais efetivamente nulos.

Validação: `tests/validate-denoise-motion.py` exporta uma sequência e sua inversa pelo ProcessingJob com redução de ruído 100 e nitidez 40. A diferença máxima dependente do histórico foi 0 (o filtro antigo apresenta 21 níveis no mesmo cenário). Também passaram os 4 testes CTest e os 18 cenários de Enhancement. Isso não valida o vídeo específico do utilizador, que não foi fornecido como arquivo.

A exportação por Real-ESRGAN/Real-CUGAN agora usa duas tarefas de leitura e duas de gravação, mantendo uma tarefa de GPU e tiles de 128. Não foi medido ganho geral de velocidade; os modelos Plus/Anime de 4× continuam fazendo inferência nessa escala nativa mesmo para saída 1×. O formato é apresentado como MP4.


### Preview, idiomas e aparência — 21/09/2026

- Comparar passou a Preview. A janela usa Carregando… e não apresenta botão Fechar nem instrução de Espaço no rodapé; continua acessível por X/Esc e teclado.
- Português (Portugal), Português (Brasil) e English podem ser alternados em Definições/Configurações > Geral sem reiniciar. Ampliação aparece como Upscaling em inglês.
- macOS Claro (padrão), macOS Escuro e Estúdio estão disponíveis em Aparência. Os novos temas usam painéis com tons suaves, superfícies translúcidas locais, controles arredondados e interruptores com transição interrompível; respeitam a preferência de movimento reduzido do Windows. São temas Qt inspirados nas referências, não o compositor Liquid Glass nativo do macOS. A fonte permanece a do sistema/Segoe UI; nenhuma fonte externa foi fornecida.
- O player mantém o último fotograma nas trocas de trecho e o mesmo fundo ao reproduzir. Vídeos pesados usam trechos de quatro segundos e antecipação do trecho seguinte durante reprodução, em vez de preparar oito segundos de cada vez. O FPS da timeline vem do original, não da prévia temporária.
- A timeline distingue código HH:MM:SS:FF da quantidade de fotogramas selecionados: um segundo a 24 FPS contém 24 fotogramas; 00:00:00:12 representa meio segundo nessa taxa.
- Preview prepara apenas o trecho escolhido, com busca antes da decodificação e intermediário sem perda em resolução nativa. Isso elimina a leitura desde o começo de vídeos longos e mantém a referência Original no próprio arquivo. O custo da inferência do modelo continua dependendo da resolução/GPU; os modelos nativos 4× continuam exigentes em saída 1×.

Validação: quatro testes CTest, teste nativo Windows com vídeo 4K, navegação rápida e passagem entre trechos, troca dos três idiomas/temas, comparação com referência independente e contagem de 24 fotogramas em um segundo. Preparação sem perda validada com todos os pixels dos cinco fotogramas de um corte não nulo e taxa fracionária 24000/1001. Logs em `build/macos-preview-tests.log`, `build/macos-preview-packaged.log`; capturas em `build/macos-preview-captures`.


## Instalador Windows

O setup para outros computadores e gerado por `scripts/build-installer.ps1`, com a configuracao em `packaging/windows/FrameScale.nsi`. Veja [como gerar e distribuir o instalador](docs/windows-installer.md).
