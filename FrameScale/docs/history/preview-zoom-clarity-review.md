# Revisão de Preview, zoom e Clareza

Correções realizadas em 28/09/2026, após o relato de faixas pretas na comparação e desfoque ao ampliar.

- A comparação não pinta mais o retângulo escuro de toda a área do componente. O fundo escuro fica na imagem; metadados e controles seguem o fundo da janela. As margens extras de cima e de baixo do modo compacto foram retiradas.
- A busca de um frame pausado usa a fonte, sem substituir o frame por uma cópia de reprodução reduzida. A resolução solicitada acompanha o zoom, com limite de 8192 por eixo e aproximadamente 32 megapixels.
- O decodificador mantém o detalhe já carregado ao reduzir o zoom. Frames grandes são lidos individualmente para evitar oito decodificações 8K a cada ajuste; o cache é limitado a 128 MiB por leitor.
- Ao ampliar além dos pixels disponíveis, o desenho não aplica outra suavização bilinear. Os pixels podem ficar visíveis em ampliações extremas: isso não é recuperação de detalhe novo.
- Arrastar os filtros encaixa em múltiplos de 25 quando estiver a até dois pontos da marca, incluindo valores negativos nos controles que os aceitam. Campo numérico e teclado continuam permitindo valores intermediários.
- Clareza continua sendo contraste local de luminância, com uma área maior que Nitidez. O coeficiente máximo passou de 0,45 para 0,9, sem alterar o ponto neutro. Em regiões planas de anime há pouca textura para realçar; valores altos podem evidenciar halos ou ruído.

Validação sem testes de interface:

- Nove casos de processamento passaram: `build/clarity-check.log`, incluindo transparência, neutralidade dos filtros e estabilidade de frames iguais.
- A decodificação do vídeo de teste 8K foi comparada pixel a pixel com uma leitura independente do mesmo frame. A redução de zoom reutilizou o frame nativo de 7680×4320 sem iniciar outro processo: `build/zoom-8k-native.log`.
- Na amostra de Demon Slayer em 1920×1080, Clareza 25, 50 e 75 produziram alterações médias absolutas crescentes de 0,608, 0,867 e 1,146 níveis por canal em 8 bits. Isso comprova efeito progressivo, não uma nota de qualidade perceptual. Evidência em `build/clarity-real-check/report.json`.

Esta revisão amplia os limites de exibição descritos na pesquisa anterior. O processamento de exportação não é limitado pela resolução da Preview.
