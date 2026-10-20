# Velocidade independente dos FPS — 07/10/2026

## Utilização

O controlo **Velocidade do vídeo**, no painel Processamento, aceita 0,25× a 4×. 1× é a velocidade original; valores inferiores fazem câmara lenta e valores superiores aceleram. A duração de saída é a duração selecionada dividida pela velocidade, arredondada aos fotogramas de saída. A timeline mostra a duração estimada em segundos quando a velocidade difere de 1×; a régua e os cortes continuam a usar o tempo da origem.

A interpolação continua a escolher quantos fotogramas são gerados por segundo da origem, e a redução continua a escolher a cadência. Alterar apenas os FPS não altera a duração. É possível combinar interpolação para 120/240 FPS com uma velocidade inferior a 1×. Alterar apenas a velocidade não cria detalhes de movimento: sem interpolação, a câmara lenta repete fotogramas.

O áudio acompanha a velocidade, incluindo a alteração do tom, tanto na reprodução como na exportação. Não foi introduzida uma opção de preservação do tom. O desfasamento das pistas é aplicado antes da velocidade, em tempo da origem. Os ficheiros originais permanecem intactos.

## Compatibilidade

A velocidade é conservada nas opções de cada projeto e item da fila, entra na chave da cache de comparação e afeta a prévia. A comparação converte o tempo do resultado para o tempo da origem. Vídeo, GIF e exportação de áudio suportam velocidade. Sequências de imagens não armazenam duração: o programa recusa velocidade diferente de 1× nesse formato em vez de ignorar a escolha.

## Timeline e menu

O menu Separar/Juntar áudio e vídeo passa a usar o estilo partilhado da aplicação. O corte atua nas bordas do clipe de vídeo, com cursor horizontal de redimensionamento. Os marcadores visíveis são retângulos de 2 píxeis; a área de acerto permanece mais larga para facilitar a utilização. Ctrl+Z mantém a reposição do corte.

## Verificação

Fonte sintética de 4 s, 30 FPS, com som conhecido em 1 s. Resultados medidos:

| Caso | Duração |
| --- | --- |
| 0,25× | 16 s |
| 2× | 2 s |
| Corte de 2 s a 0,5× com áudio deslocado | 4 s |
| Reduzir para 15 FPS e 2× | 2 s |
| Interpolar 1 s para 60 FPS e 0,5× | 2 s |
| Ampliar 1 s em 2× e acelerar em 2× | 0,5 s |
| GIF de 1 s a 0,5× | 2 s |
| WAV de 2 s a 0,5× | 4 s |

Os validadores de exportação confirmaram metadados, contagem e descodificação completa. O início do som foi medido em 4,00015 s a 0,25× e 0,50002 s a 2×; com corte/desfasamento a 0,5×, em 2,00008 s. Evidências: `tmp/finalization-2026-10-05/speed-*.log`, `speed-measurements.json` e `speed-audio-onsets.json` na raiz do workspace.

Esta revisão compila o programa e executa testes; não gera instalador.

Validação final: suite nativa completa aprovada, incluindo comparação a 0,5×/2×, conservação da velocidade entre separadores e painel com todas as opções em janela pequena (`speed-full-final-ui.log`). Testes CTest de interface e orientação aprovados (`speed-ctest-final-ui.log`); processamento aprovado em `speed-ctest.log` (nessa execução a interface ainda usava coordenadas antigas dos marcadores, corrigidas depois). Teste dedicado da timeline e relógios de reprodução aprovado (`speed-ui.log`). Reprodução especial do original `a.mp4` a 0,5× aprovada (`speed-sparse-ui.log`). Nenhum instalador gerado.
