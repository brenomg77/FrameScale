# Materiais de vidro e menus de seleção

Revisão de 21 de setembro de 2026. Referências visuais: menus de seleção e painéis do Icon Composer enviados pelo utilizador; orientação complementar da skill `apple-design`.

## Direção visual e limites

O material combina fundo desfocado, tonalidade translúcida, contorno claro e sombra suave. O objetivo é dar profundidade aos controles e à navegação, preservando a legibilidade e a aparência da mídia. A separação entre material da interface e conteúdo segue a orientação de [Materials, Apple Human Interface Guidelines](https://developer.apple.com/design/human-interface-guidelines/materials). Os menus mantêm a opção atual indicada por uma marca de verificação e aproximam a linha selecionada do campo de origem, tomando como referência os [pop-up buttons da Apple](https://developer.apple.com/design/human-interface-guidelines/pop-up-buttons).

O ambiente de desenvolvimento usa Windows 10, build 19045. A API documentada `DWM_SYSTEMBACKDROP_TYPE`, que disponibiliza Mica e Desktop Acrylic, exige Windows 11 build 22621. Além disso, `DwmEnableBlurBehindWindow` deixou de produzir desfoque a partir do Windows 8. Essas APIs não são uma solução compatível para este ambiente. Fontes: [DWM_SYSTEMBACKDROP_TYPE](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type) e [DwmEnableBlurBehindWindow](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmenableblurbehindwindow).

O efeito implementado é uma composição dentro do aplicativo. Não é o Liquid Glass nativo do macOS nem uma captura translúcida da área de trabalho. Essa distinção também evita reduzir a opacidade de toda a janela, o que prejudicaria textos e o vídeo.

## Implementação

- `GlassMaterial.h` reduz a imagem de apoio e aplica três passagens separáveis de desfoque, guardando um raster pequeno em cache. A pintura reutiliza esse resultado com uma camada de tonalidade.
- Os painéis recebem uma tonalidade inspirada nas cores da mídia carregada. O cache é atualizado somente quando o fotograma de referência muda, com verificação periódica; o efeito não processa cada fotograma da reprodução.
- Ao abrir, o menu captura o conteúdo da própria janela sob a sua posição, desfoca essa região e conserva o resultado durante a abertura. Se não houver uma região de apoio válida, usa um material sólido legível.
- A opção **Transparência dos painéis**, em Configurações, persiste em `glassTransparency` e controla o material do aplicativo. Ela não altera a preferência de transparência do Windows. O alto contraste do sistema prevalece e desativa o efeito.
- O material fica na interface. Não aplica filtros ao vídeo exibido nem altera a exportação.

Os menus usam `WA_TranslucentBackground` e `FramelessWindowHint` para a composição alfa dos cantos e da sombra no Windows, conforme [Creating translucent windows, Qt](https://doc.qt.io/qt-6/qwidget.html#creating-translucent-windows). A preferência de alto contraste é consultada pela API [QAccessibilityHints](https://doc.qt.io/qt-6/qaccessibilityhints.html).

## Correção da interação

Havia dois caminhos distintos para um menu fechar ou reabrir involuntariamente:

1. O release do mesmo clique que abriu o menu podia ativar imediatamente a linha selecionada, posicionada sob o cursor. O comportamento foi confirmado no código de `QMenu::mouseReleaseEvent`: um release sem press anterior no menu ainda pode ativar `currentAction`. A implementação distingue esse gesto de abertura de uma seleção posterior e permite a seleção ao arrastar. [Fonte do Qt 6.11, qmenu.cpp](https://github.com/qt/qtbase/blob/6.11/src/widgets/widgets/qmenu.cpp).
2. O clique usado para dispensar o popup podia ser reenviado ao campo subjacente e abrir outro menu. `WA_NoMouseReplay` é reativado nos eventos de mouse, porque o Qt limpa esse atributo a cada novo evento. O combo também administra a abertura sem iniciar os temporizadores do container nativo oculto e reconhece uma repetição do mesmo evento pelo timestamp, sem atrasar cliques novos. [Qt::WidgetAttribute](https://doc.qt.io/qt-6/qt.html#WidgetAttribute-enum).

Ao fechar o popup personalizado, o estado interno do combo é reposto exatamente uma vez mediante `QComboBox::hidePopup()`, como exige a [documentação do Qt](https://doc.qt.io/qt-6/qcombobox.html#hidePopup). A marca de verificação usa o centro óptico das letras, em vez de depender apenas do centro inteiro da linha.

## Validação

Resultados finais:

- Compilação Qt 6.11.2 / MinGW concluída e executável instalado em `build/FrameScale_UI/bin/FrameScale.exe`.
- CTest: **4/4 aprovados**, incluindo interface, processamento e fila; log `build/glass-refinement-ctest.log`.
- Teste nativo Windows aprovado: oito ciclos de abrir/fechar com `SendInput`, confirmação de que o release inicial mantém o menu aberto, descarte sem reabertura, seleção de outra opção por mouse, navegação por teclado e Escape. Log `build/glass-refinement-native.log`.
- Verificados o cache de fundo dos painéis e do menu, a presença da opção de transparência e a desativação do material pela preferência do aplicativo.
- Capturas revisadas em `build/glass-refinement-captures`: `macos-selection-desktop.png`, `editor-image.png`, `macos-dark.png` e `settings.png`. Marca alinhada ao texto, cantos sem retângulo preto e material tonalizado nos painéis. A comparação visual não implica equivalência exata ao renderizador nativo da Apple.
- As alterações ficam nos componentes de interface; o processamento/exportação continua coberto pelos testes existentes.

Não há medição de desempenho comparativa com outros programas nesta revisão. O uso de cache e imagens pequenas limita o trabalho adicional por decisão de implementação; os resultados de desempenho devem ser registrados separadamente se forem medidos.

## Refinamento de largura, transparência e tipografia

O menu passa a usar a largura do conteúdo, com mínimo de 172 px incluindo a margem de sombra, em vez de herdar toda a largura do campo. A camada de tonalidade clara cai de 154 para 84/255 e o fundo desfocado admite composição alfa com o conteúdo abaixo. A opacidade dos painéis também foi reduzida. O interruptor passa de 40×22 para 40×18 px, com peça branca oval de 22×14 px; a área clicável permanece maior que o desenho.

Anthropic Sans foi incorporada dos arquivos fornecidos pelo utilizador. A variante Text atende controles a 13 px e pesos Regular/Medium/Semibold/Bold; Display atende títulos maiores. O carregamento usa os nomes de família informados pelos próprios arquivos, sem exigir instalação no sistema. Foi removida a regra global que forçava Segoe UI, e os menus herdam a mesma família dos campos.

Validação deste refinamento: compilação e teste nativo Windows aprovados (`build/glass-font-native.log`), incluindo ciclos de abertura/fechamento, seleção por mouse/teclado e família tipográfica efetiva dos campos, menus e título. Capturas revisadas em `build/glass-font-captures`; pacote atualizado em `build/FrameScale_UI`.

## Controles compactos e estabilidade do painel

O painel passa a 286 px, os campos de processamento a 228 px e os seletores de preferências a 256 px. Preferências usa 520×400 px; opções de formato usa 500×550 px. Os grupos superiores mantêm a altura ao revelar melhoria de imagem, com espaço reservado para a barra de rolagem. Os botões de presets usam margens menores para conservar os textos completos.

O ícone de som tem traço de maior contraste e atualiza sua cor ao trocar o tema. Ficheiro e Definições usam a superfície arredondada dos seletores, preservando a navegação nativa da barra de menus. A tonalidade do material adapta-se a fundos escuros para manter a leitura; os painéis usam tons quentes e lavanda inspirados na referência. Continua sendo composição dentro do aplicativo, não o Liquid Glass nativo do macOS.

Validação: 4/4 CTest aprovados (`build/compact-ui-ctest.log`) e teste nativo Windows aprovado (`build/compact-ui-native.log`). Cobertura inclui contraste do ícone, posição do interruptor antes/depois de ativar melhoria de imagem, largura constante, abrir menus por clique e alternar pela barra, além dos ciclos de seletores. Capturas revisadas em `build/compact-ui-captures`, incluindo opções de formato, preferências e menus. Executável instalado em `build/FrameScale_UI/bin/FrameScale.exe`.

### Smooth window corners (2026-09-21)
- Replaced the main window's binary polygon mask with a cached antialiased alpha mask (12 logical pixels), shared by application dialogs including Preview, Settings and format options.
- Frameless dialogs retain a draggable title, close action and resize grip. The close action does not steal the default Enter action. Maximized/fullscreen windows bypass the rounded mask.
- Explicitly draw the themed background behind the composed widget content so translucent windows do not lose their surface.
- Native Windows UI smoke with video passed, including comparison playback/seek and cached processing. All four CTest targets passed.
- Checked PNG corner alpha in Settings, Preview and format options: includes 0, 28, 79, 157, 217, 253 and 255, confirming real antialiasing rather than a binary cutout. Captures: build/rounded-captures.
- Installed the verified build into build/FrameScale_UI.

### Native desktop glass (2026-09-22)
The Liquid Glass themes now expose a single neutral translucent window surface, including the title, inspector, timeline, Settings, format options and Preview. Opaque media canvases and text are preserved. Removed opaque canvas/header layers that prevented desktop composition.

On Windows, a dynamically resolved SetWindowCompositionAttribute accent blur applies to the existing frameless alpha window. This compatibility API is not recommended as a stable SDK contract by Microsoft, so missing/failed composition restores an opaque neutral surface. Studio, high contrast and the in-app transparency switch disable the effect. Qt still provides antialiased corner coverage. Win10 compositor blur has a tiny rectangular footprint at transparent corner pixels; there is no opaque rectangle and no binary window-region cut that would degrade the smooth material edge.

Primary references: https://learn.microsoft.com/en-us/windows/win32/dwm/setwindowcompositionattribute and https://doc.qt.io/qt-6/qwidget.html#creating-translucent-windows . The Win11-only DWM system backdrop API is not used for the verified Windows 10 path.

Validation: native Windows UI smoke passed (build/desktop-glass-final-ui.log); full video/Preview smoke returned exit 0 (build/desktop-glass-final-video.log). Checks cover translucent inspector alpha, opaque media, disabling native blur and restoring opaque panels. Controlled backing-window probe verified real compositor blur, with media pixels unchanged. Desktop captures are in build/desktop-glass-captures; desktop-comparison.png demonstrates the actual blur behind Preview chrome.

### Glass repaint and preview interaction fixes (2026-09-22)
- Compose each window frame into a fresh transparent pixmap, then replace the destination alpha instead of blending translucent dirty rectangles over the previous backing store. Preserve the native blur and antialiased corners.
- Remove the inspector tab base line; use SwitchControl for metadata; center the smaller project close glyph; remove the application close tooltip; compact and round the volume popup.
- Wheel zoom preserves the image point beneath the cursor, including after panning.
- Paused seek rejects approximate QMediaPlayer frames and obsolete thumbnail jobs. Exact FFmpeg results replace the held frame only for the requested position; active dragging may show progressive results moving toward the current target. Decoder timestamps snap to source frame rate with a small pre-boundary tolerance to avoid advancing a frame due to millisecond rounding.
- Native Windows video UI smoke exit 0 (build/glass-fixes-ui.log), including cursor anchor after pan, one-frame backward seeking, superseded seek suppression, partial-repaint alpha stability, metadata switch and removed tab base/close tooltip. Desktop captures reviewed in build/glass-fixes-captures (enhancement, encoding, volume).
