# Correção das alças de corte — 05/10/2026

O utilizador encontrou um erro que escapou à auditoria: ao reduzir o trecho para um fotograma, as duas alças sobrepunham-se e não era possível voltar a expandir o fim, especialmente no início do vídeo. O teste anterior verificava o encaixe com Shift, mas não a sobreposição.

A reprodução isolada com a seleção de alça antiga confirmou o defeito: a 60 fps, arrastar 120 pixels para a direita conservou o fim em `0,0166667 s` (`expanded=0`). Evidência: `tmp/finalization-2026-10-05/trim-before.log`.

## Causa e correção

`TrimTimeline::mousePressEvent` dava sempre prioridade à alça inicial dentro dos 12 pixels de tolerância. Com ambas no mesmo local, a alça final não podia ser escolhida. A restrição que conserva pelo menos um fotograma impedia mover o início para a direita, produzindo o bloqueio observado.

Quando as alças se sobrepõem visualmente, o clique agora aguarda a direção do arrasto: direita seleciona o fim e esquerda seleciona o início. Um simples clique não altera o trecho nem a posição do vídeo. Quando as alças estão visualmente separadas, o clique escolhe a mais próxima. Mantêm-se o mínimo de um fotograma, o alinhamento aos fotogramas, Shift e a navegação pela régua.

## Validação

Foram acrescentados 12 cenários de arrasto em `tests/ui_smoke.cpp`: seleção de um fotograma no início, no meio e no fim de um vídeo de 38 segundos, a 24, 60 e 120 fps. Nos pontos interiores são verificadas as duas direções. Os testes confirmam que a extremidade oposta fica intacta, que não ocorre seek acidental e que uma atualização do modelo durante o arrasto não desfaz a edição.

Os testes `FrameScaleUiSmoke` e `FrameScaleOrientationUi` passaram, incluindo a verificação existente de Shift. A suite geral também passou com o plugin Windows nativo num desktop isolado. Evidências em `tmp/finalization-2026-10-05/trim-fix-tests.log` e `trim-fix-native.log`, relativos à raiz do workspace.

Os mesmos 12 casos integram também o arranque dos testes do pacote. Os modos de orientação e processamento real passaram no payload atualizado com PATH restrito ao Windows, sem Qt/MinGW de desenvolvimento. Evidência: `tmp/finalization-2026-10-05/trim-package-tests.log`.

O executável corrigido é `build/final-2026-10-05/FrameScale.exe`. O instalador atualizado fica em `dist`, mantendo ali apenas o `.exe`; os registos de construção e validação ficam em `build/release-records`. A instalação existente não é atualizada automaticamente: é necessário fechar a aplicação e executar o novo setup, ou recompilar o projeto no ambiente de desenvolvimento utilizado.

Esta correção resolve o caso comunicado e acrescenta cobertura específica. Não constitui uma garantia de que todos os gestos possíveis ou todas as condições de utilização estejam livres de erros.

Instalador desta corre??o: SHA-256 `807896f15511cb5887e2c37ec0e8ba2df3f366790a36cdd553bd90297f1dfdbc`. Confirmada a igualdade dos fontes inventariados e do execut?vel entre build e pacote. O ciclo completo de instala??o/desinstala??o da auditoria anterior n?o foi repetido nesta altera??o exclusiva da interface.
