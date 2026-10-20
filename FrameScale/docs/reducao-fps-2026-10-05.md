# Redução de FPS e limites da interpolação

Na secção Processamento, com Interpolação desativada, Reduzir FPS permite escolher um inteiro abaixo da taxa original. Original (valor interno zero) conserva a cadência. A redução seleciona fotogramas através do FFmpeg, sem RIFE e sem alterar a velocidade do áudio. É compatível com corte e ampliação. A duração de saída é quantizada aos fotogramas da taxa pretendida, podendo diferir por menos de um fotograma.

Com Interpolação ativa, o mínimo é o primeiro inteiro estritamente superior ao FPS original (60 → mínimo 61; 59,94 → mínimo 60). O máximo continua a ser 240. Fontes de 240 FPS ou mais não permitem ativar a interpolação. Os campos aguardam os metadados; existe validação adicional antes de adicionar a interpolação à fila e no processamento. A taxa média efetiva de um corte VFR é novamente validada pelo processamento.

As opções de cada projeto e item da fila incluem reductionFps; a comparação também distingue essa opção no cache. O nome sugerido da exportação inclui a taxa reduzida. GIF mantém o limite de 100 FPS.

Validação: fonte sintética de 60 FPS/2 s com áudio, exportação para 24 FPS/48 fotogramas; corte de 1 s/24 fotogramas; ampliação Real-ESRGAN 2× com corte de 0,5 s/12 fotogramas. Os três resultados passaram a validação de metadados e descodificação completa. O teste de interface confirmou mínimo 61 para interpolação e máximo 59 para redução. CTest: interface, orientação e processamento aprovados.

Evidências: tmp/finalization-2026-10-05/fps-controls-run.log, fps-trim.log e fps-upscale.log, relativos à raiz do workspace.

Também passaram as saídas GIF e sequência PNG a 24 FPS com corte de 0,5 s. A taxa igual à origem foi rejeitada no modo de redução. O MP4 de 2 s foi confirmado com 48 fotogramas e áudio de 2 s. A suite nativa e os testes de interface do pacote passaram.
