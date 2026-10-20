# Timeline integrada de vídeo e áudio — 06/10/2026

## Utilização atual

Vídeo e forma de onda partilham o mesmo painel, régua, cursor e navegação. As pistas começam ligadas, mas sem seleção. O botão direito sobre uma pista abre **Separar áudio e vídeo**; depois de separar, é necessário selecionar uma pista e manter Ctrl ao clicar na outra. Só com as duas selecionadas o menu apresenta **Juntar áudio e vídeo**. Ctrl também permite retirar uma pista da seleção. O botão direito conserva a seleção múltipla. O menu contém apenas a ação de separar/juntar; desfazer e refazer continuam disponíveis pelos atalhos.

Clicar numa pista separada seleciona-a e destaca toda a linha. Arrastar horizontalmente move essa pista em relação à outra. As extremidades do vídeo continuam a definir o corte; clicar na régua posiciona a reprodução. Ctrl + roda amplia a timeline. As pistas ligadas são selecionadas em conjunto.

**Ctrl+Z** desfaz separação, ligação, arrasto de áudio, arrasto de vídeo e cortes pelas alças. **Ctrl+Shift+Z** refaz. Um arrasto corresponde a uma ação, não a cada movimento do rato. Uma edição nova elimina o ramo de refazer. O histórico, limitado a 100 ações, é limpo ao mudar/fechar a origem; não é um histórico global das definições de exportação.

Foram removidos os controlos visíveis Ligar áudio e vídeo, Desfasamento do áudio e Repor áudio. A posição relativa continua guardada nas opções do projeto e nos itens da fila. Juntar conserva a sincronização escolhida.

## Semântica e limites

O FrameScale continua a exportar o intervalo selecionado do vídeo. A posição das pistas serve para ajustar a sincronização: deslocar o vídeo para a direita adianta o áudio relativamente ao vídeo; deslocar o áudio para a direita atrasa-o. A exportação começa no corte do vídeo, sem acrescentar um trecho preto correspondente ao espaço anterior na área de edição. A prévia também reproduz o intervalo do vídeo. Não se trata de uma sequência de vários clipes com espaços vazios exportáveis.

A forma de onda representa a primeira faixa de áudio; o mesmo desfasamento é aplicado às faixas mantidas na exportação. O ajuste é constante e não muda a velocidade nem o tom da voz. O vídeo original não é modificado nem copiado para esta edição.

## Implementação

`AudioTrack` é um filho de `TrimTimeline`, dentro do painel comum. As posições na régua são convertidas para o tempo da origem para procurar fotogramas e cortar. A reprodução só recebe a alteração quando o arrasto termina, evitando recarregar o descodificador a cada movimento. Os estados de desfazer incluem posição do vídeo, desfasamento do áudio, ligação e corte.

O cálculo de forma de onda usa picos em memória, leitura incremental do FFmpeg e 2400 intervalos. Fechar ou mudar de origem cancela o processo. Sem áudio, a pista fica escondida.

## Validação

O teste nativo `--audio-tracks` verifica o painel comum, ausência dos antigos controlos, menu real de contexto, seleção independente/conjunta, arrasto de ambas as pistas, desfazer/refazer, cortes e limpeza do histórico. Também compara os relógios de áudio e vídeo para ajustes de +500 e −500 ms, com tolerância de 200 ms.

Os testes anteriores de exportação mediram o início de um som conhecido: +500 ms em 1,50004 s; −500 ms em 0,50004 s; +500 ms com corte em 1,00004 s; −500 ms com ampliação em 0,50004 s; áudio WAV com corte em 1,00004 s. O processamento mantém esse mesmo filtro de desfasamento.

Evidências desta revisão: `tmp/finalization-2026-10-05/timeline-unified-*.log`, testes nativos e captura `audio-track-captures/audio-tracks.png` na raiz do workspace. Os registos do instalador ficam em `FrameScale/build/release-records`.

Resultado final: 3/3 testes CTest aprovados (44,84 s), teste nativo de interação aprovado com eventos de teclado Ctrl+Z/Ctrl+Shift+Z e 3/3 verificações do pacote aprovadas com PATH limitado ao Windows. A captura final foi inspecionada com a paleta da aplicação. O teste de limpeza temporária foi corrigido para aguardar a remoção assíncrona, em vez de exigir a inexistência imediata da pasta. Uma execução intermédia da suite geral usou indevidamente a fonte curta do teste de áudio; a execução final utilizou as fixtures próprias da suite.

Revisão de seleção: cursor padrão na timeline, sem seleção automática ao abrir/restaurar ligação; nenhuma geração de instalador nesta revisão, conforme pedido do utilizador.

Polimento da timeline: clicar fora das pistas (incluindo régua, rodapé, prévia e painel lateral) remove a seleção. O menu usa cores explícitas do tema claro/escuro. Shift durante o arrasto do áudio encaixa o seu início no início do vídeo quando fica a até 12 píxeis desse ponto; a tolerância acompanha o zoom e o resultado é um desfasamento exato de 0 ms. As alças de corte têm geometria idêntica. Mover áudio não estende a régua além do fim do vídeo, e a última miniatura preenche o intervalo até à alça final.
