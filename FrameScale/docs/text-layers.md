# Camadas de texto

O botão quadrado **Texto (T)**, ao lado de **Seleção (V)**, ativa a ferramenta sem criar uma camada.
Clique num ponto vazio da imagem na preview para criar texto nessa posição e começar a escrever.
Clicar num texto existente edita essa camada. Uma nova camada surge acima das outras, na posição da agulha, com até cinco segundos de duração.
**Adicionar camada… → Arquivos**, no menu com o botão direito, importa vídeo e áudio.

Selecionar a faixa de texto na timeline prepara a edição na preview, mantendo o foco na timeline.
**Delete** exclui a camada selecionada; **Desfazer/Refazer** recupera a operação.
Quando se está a escrever na preview ou num campo de texto, Delete apaga caracteres.
O nome da faixa mostra apenas o conteúdo, sem prefixo T.

O ícone de ajustes no extremo direito da faixa abre as propriedades.
A janela **Editar** apresenta as seções **Texto** e **Transformação** na lateral esquerda.
O conteúdo é editado na prévia. Texto permite alterar fonte, estilo, tamanho, entrelinha, espaçamento, alinhamento, sublinhado, preenchimento e contorno.
As camadas de vídeo mostram apenas **Transformação**. Também é possível abri-lo pelo menu da camada.
Selecionar outra camada atualiza a janela aberta; selecionar áudio desativa os controlos.

Transformação inclui posição X/Y em píxeis do canvas, escala X/Y em percentagem, rotação em graus e opacidade.
Restaurar transformação devolve os valores iniciais. A transformação usa o centro da camada, sem Anchor Point.
Com **Seleção (V)**, arraste texto ou vídeo na preview mantendo o botão esquerdo premido.
Um arrasto corresponde a um único Desfazer/Refazer. As transformações acompanham a exportação e a comparação.

O texto é desenhado diretamente sobre o vídeo: escrever ou mudar o estilo não reinicia a reprodução nem carrega outro vídeo.
Durante o ajuste de vídeo, a preview compõe fotogramas das fontes em memória, com limite de 1920×1080 para interação.
Ao largar, a composição de reprodução é atualizada; a reprodução e a exportação mantêm a resolução do canvas.

`FrameScaleTextLayerTests` verifica criação por clique, edição, exclusão, foco, propriedades e transformações.
`FrameScaleLayerStateTests` verifica Desfazer/Refazer, isolamento entre camadas e composição real com FFmpeg.
