# Revisão de design: prévia, timeline e Output

## Resumo

Avaliação: **boa**, dentro do escopo verificado. Aplicativo Windows/Qt Widgets para processamento de mídia; a direção visual é o editor escuro solicitado, inspirado nas referências do After Effects. A filmstrip e a comparação em movimento tornam visível o conteúdo que está sendo editado, enquanto o azul identifica ações e seleção.

Foram revistos código, dimensões dos controles e capturas nativas em `build/preview-revision-captures`. Esta revisão não certifica a acessibilidade de toda a aplicação. A skill apple-design foi usada como referência de interação e legibilidade; os temas Apple/Transparency continuam removidos conforme solicitado.

## Correções e padrões preservados

- Botões Novo/Selecionar com 30 px de altura e largura pelo conteúdo; Abrir como ação textual. Há estados de foco, hover e pressão. `buttons.md › Style`: “Keep the number of prominent buttons to one or two per view.” Referência local: `C:/Users/Admin/.agents/skills/apple-design/references/hig/buttons.md`.
- O branco sobre o azul `#1473e6` tem contraste calculado de **4,54:1**. `#dedede` sobre `#202020` tem **12,11:1**; sobre `#292929`, **10,81:1**. São cores do código, não estimativas da captura.
- Volume com alvo de 30×28 px, nome acessível, tooltip, porcentagem e slider operável pelo teclado. `sliders.md › Best practices`: “Use familiar slider directions.” O mínimo fica à esquerda. Referência: `references/hig/sliders.md` na mesma skill.
- O popup fica junto ao botão, limitado à área da tela, e preserva o valor ao fechar. `popovers.md › Best practices`: “Always save work when automatically closing a nonmodal popover.” Referência: `references/hig/popovers.md`.
- A timeline mantém geometria durante carregamentos, com miniaturas progressivas e atualização durante arrasto. A comparação identifica as duas fontes e oferece reprodução/posição compartilhadas e cancelamento durante a geração.
- Output apresenta primeiro o formato; codec, qualidade e áudio ficam nas opções desse formato. A explicação de áudio muda conforme a escolha, evitando uma lista de termos técnicos sem contexto.

## Limites da revisão

- **Médio:** leitura completa por NVDA e escalas de texto/DPI extremas ainda precisam de uma auditoria específica. Foram verificados controles Qt, nomes acessíveis e atalhos, não um percurso integral com leitor de tela.
- **Médio:** a comparação passou a decodificar as fontes em resolução nativa. Isso exige mais memória em 4K/8K; não houve certificação de monitoramento HDR. O fallback para codecs incompatíveis com Qt usa vídeo lossless de 8 bits.
- A avaliação de fidelidade espacial e temporal dos futuros modelos de Enhancement permanece separada, em `enhancements-research.md`.


## Complemento de 19/09/2026

A revisão implementa cursor normal com mão temporária ao segurar Espaço, zoom contínuo na comparação, encaixe de corte com Shift e fechamento do popup sem repetir o clique de abertura. O áudio usa revelação progressiva: campos incompatíveis ficam ocultos e seus valores são preservados. Comparar usa texto branco e largura mínima estável.

Enhancement mantém controles homogêneos, slider e valor numérico de 0 a 100, labels associados, tooltips e restauração dos ajustes. Desentrelaçamento aparece apenas para vídeo. Os efeitos começam desligados; os presets guardam explicitamente estado e intensidade. A área de processamento continua rolável para preservar a prévia e os botões da fila em janelas menores.

Verificação final: os quatro testes CTest e os 13 cenários de `validate-enhancements.py` passaram. O teste nativo também passou executado com as DLLs e plugins do pacote Windows, incluindo comparação processada, prévia 4K, cursor/arrasto, encaixe com Shift, zoom e áudio. Capturas revisadas em `build/enhancement-captures`: `enhancement-controls.png`, `comparison.png` e `timeline-filmstrip.png`. Executável atualizado em `build/FrameScale_UI/bin/FrameScale.exe`.


## Revisão de logo e restauração

Os cartões mostram os nomes dos motores disponíveis; o estado selecionado tem contorno claro e permanece acessível pelo teclado. Restauração e intensidade estão separados, e os perfis não são apresentados como detecção automática. O painel informa onde o efeito aparece e oferece acesso direto a Comparar. A logo vem do arquivo transparente fornecido, sem aplicar uma placa escura.

Referências adicionais: `icons.md › Best practices` recomenda “Maintain visual consistency across all interface icons in your app.” A decisão de manter a marca enviada e o editor escuro é específica deste produto. `machine-learning.md` orienta comunicar o funcionamento e as limitações do recurso; não foram usados nomes de modelos proprietários para filtros locais.

Verificação desta revisão: 4/4 testes CTest; 18/18 cenários reais em `build/enhancement-validation-lr3f7ryc/summary.json`; teste nativo com o pacote Windows em `build/restoration-packaged.log`. Original é confrontado com uma decodificação independente dos pixels do arquivo no início de um corte não nulo, e seu player precisa abrir o próprio arquivo de entrada. Os três modelos passam por exportação real de imagem e vídeo em 1×, com upscale desativado. A medição sintética de denoise passou de MSE 80,82 para 50,55 (37,45% menor); não se trata de uma nota de qualidade geral nem de comparação com Topaz. Capturas revisadas: `build/restoration-captures/enhancement-controls.png` e `comparison.png`.


## Revisão dos controles e da janela Comparar

O campo de intensidade passa de 38 para pelo menos 64 px, com padding lateral de 6 px, alinhamento central e confirmação da edição antes de atualizar o slider. Setas de escala/FPS usam SVGs claros. A revisão remove perfis internos, botão duplicado e tooltip sobre a imagem. O desentrelaçamento possui ajuda geral e por opção. Os títulos Original/Resultado ficam acima do nome do arquivo, alinhados à esquerda. A timeline da comparação navega sem editar o corte do projeto; o zoom relativo ao enquadramento é sincronizado.

As capturas desta revisão ficam em `build/controls-captures`. A análise de contraste e a direção visual da revisão anterior continuam aplicáveis. O fechamento libera a interface antes de aguardar cancelamento/limpeza; duas comparações concluídas podem ser reutilizadas. A primeira execução de um novo trecho permanece dependente da resolução e do modelo de IA.

Validação final: 4/4 testes CTest e teste nativo do pacote Windows aprovados. Foram verificados intensidade 100, tradução de retorno ao português, timeline com miniaturas, zoom vinculado, reutilização da comparação e fechamento durante geração em menos de 250 ms. O teste também concluiu o encerramento do aplicativo sem falha. Log nativo: build/controls-packaged.log; capturas: build/controls-captures.


## Temas e Preview — 21/09/2026

Referência aplicada: skill apple-design de emilkowalski, instalada em ~/.codex/skills/apple-design. Materiais distinguem painéis e controles; tipografia mantém a fonte do sistema; interruptores respondem sem bloquear a interação e respeitam redução de movimento. O tema claro usa tons quentes/lavanda discretos; o escuro usa neutros azulados. Nenhuma refração sobre pixels do vídeo ou mudança de exportação. Os controles de janela mantêm as convenções do Windows.

Capturas revisadas em build/macos-preview-captures: editor-combined.png, macos-dark.png, settings.png e comparison.png. Foi corrigido o recorte vertical dos títulos nas abas após revisão visual. A Preview não repete a seleção de arquivo nem o rodapé removido. Testes cobrem carregamento, idiomas, retomada entre proxies, contagem e cancelamento; não representam uma comparação de desempenho com Topaz.


## Queue and Preview - 2026-09-23

Export progress is visible only during an active job; completed, failed and cancelled jobs retain their individual row status. Export all was removed; Export uses checked rows. Queue headings are left aligned. Inspector and document borders are aligned, and combo boxes have reduced vertical padding. Preview panes now zoom independently; media, headers and timeline use rounded clipping.

Validation: core, processing and queue smoke tests passed; updated UI smoke passed. Visual captures: build/queue-preview-captures.
