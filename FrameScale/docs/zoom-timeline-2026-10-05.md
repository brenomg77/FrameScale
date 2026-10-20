# Zoom da timeline — 05/10/2026

Funcionalidade solicitada pelo utilizador para facilitar a seleção de fotogramas, com ampliação e navegação horizontal na timeline.

## Utilização

- `Ctrl + roda do rato`: ampliar à volta do ponteiro.
- Roda do rato, barra horizontal ou arrasto com o botão central: deslocar a zona visível.
- Tecla `0`: mostrar novamente todo o vídeo.
- Com a timeline focada, `+`/`-` alteram o zoom e as setas continuam a avançar/recuar um fotograma.

O zoom máximo mostra oito fotogramas. Ampliar ou deslocar a vista não altera o trecho selecionado nem a posição do vídeo. A régua, o cursor e as alças usam a mesma conversão entre posição e tempo. As miniaturas continuam a ser amostras do vídeo; a imagem do fotograma selecionado é apresentada no preview principal.

A vista mantém-se durante as atualizações do descodificador e é reposta ao carregar outro conteúdo. A timeline da comparação também recebe os controlos de navegação. Mantém-se a correção das alças sobrepostas.

## Verificação

Os testes de interface verificam a preservação da âncora e da seleção ao ampliar, Ctrl + roda, deslocação até ambos os extremos, limite de oito fotogramas, edição de exatamente um fotograma com zoom, seek pela régua ampliada, reposição da vista e zoom muito próximo de 1 sem overflow da barra horizontal. Os 12 cenários anteriores de alças sobrepostas continuam incluídos.

Os testes CTest de interface e orientação passaram. A suite completa de interface passou também com o plugin nativo do Windows num desktop isolado. Foi inspecionada uma captura da timeline ampliada em `tmp/finalization-2026-10-05/zoom-captures/timeline-zoom.png` (caminho relativo à raiz do workspace).

O executável de desenvolvimento é `build/final-2026-10-05/FrameScale.exe`. O instalador fica em `dist`; os registos técnicos ficam em `build/release-records`. Uma instalação existente precisa de ser atualizada executando o novo setup com a aplicação fechada.

Setup SHA-256: `8ab2b16df7bb4b3d161b330a2865f920bd33b9fab2579c488075e67441183152`. Build e payload com o mesmo SHA-256 do executável; inventário de fontes confirmado. Os testes de orientação e processamento do pacote passaram com PATH apenas do Windows, incluindo as regressões da timeline. O ciclo completo de instalação/desinstalação anterior não foi repetido para esta alteração da interface.

A pedido do utilizador, os controlos visuais de zoom e o balão de ajuda foram retirados. A altura voltou a 160 pixels; a barra horizontal só aparece quando a vista está ampliada. Os atalhos de zoom mantêm-se. O SHA acima identifica o instalador anterior a esta simplificação.

Instalador com interface simplificada: SHA-256 `5b807c1d52b7f39aed83fdc209e7e5a139986a3c27feb52af50dd2833f59d16a`. Suite nativa de interface e testes do pacote aprovados; executável e fontes conferidos por hash.
