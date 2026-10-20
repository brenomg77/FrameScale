# Revisão de desempenho — 28/09/2026

## Alterações

- A validação final continua decodificando todo o vídeo e áudio com detecção de erros. A contagem real de frames agora ocorre nessa mesma passagem. O ffprobe anterior lê apenas metadados, evitando uma segunda decodificação completa do arquivo exportado.
- Os filtros deixam de ficar limitados a dois threads em qualquer computador: usam metade dos processadores lógicos disponíveis, com mínimo de um e máximo de oito.
- Real-ESRGAN/Real-CUGAN usam blocos de 256 pixels quando os adaptadores de hardware informam pelo menos 4 GiB de memória dedicada e 3 GiB de orçamento local disponível. O orçamento é consultado por DXGI a cada preparação de trabalho. Memória insuficiente, hardware desconhecido ou falha na consulta mantêm os blocos de 128 pixels.
- No RIFE, computadores com oito ou mais processadores lógicos e entrada de até 3840 × 2160 usam dois leitores e quatro gravadores PNG. A concorrência da inferência continua em dois. Entradas maiores/computadores menores mantêm 1:2:2 para conter o uso de memória.
- Modelos, precisão da inferência, resolução, FPS, parâmetros dos filtros, codec e CRF não foram reduzidos.

## Medições locais

Computador: Ryzen 7 5700X3D e GeForce GTX 1070. A consulta de recursos selecionou `tile=256`, `filters=8`, `rife=2:2:4`.

Entrada: os primeiros seis frames de `Demon Slayer - S4E08 (120).mp4`, 1920 × 1080, 24000/1001 FPS, extraídos para um fixture FFV1. H.264 CRF 18, preset fast, sem áudio. Uma execução isolada antes e outra com a versão final; os números incluem preparação, processamento, codificação e validação. São amostras curtas, não uma previsão para todo vídeo ou computador.

| Operação | Antes | Depois | Redução do tempo |
| --- | ---: | ---: | ---: |
| AnimeVideo v3, 2× | 9,215 s | 8,289 s | 10,0% |
| Real-CUGAN SE, 2× | 11,813 s | 10,622 s | 10,1% |
| RIFE v4.6, 60 FPS | 3,597 s | 3,308 s | 8,0% |
| Somente filtros, Clareza 100 | 0,627 s | 0,446 s | 28,9% |

Medições de etapas isoladas:

- Real-ESRGAN Plus, um frame 1080p → 4K: 30,799 s com blocos 128; 25,748 s com blocos 256 (16,4% menos tempo).
- Redimensionamento Lanczos para 4K + Clareza 100, seis frames, saída framemd5: 0,599 s com dois threads; 0,325 s com oito. Os hashes dos seis frames são idênticos.
- RIFE, 24 frames 1080p → 60 frames: 4,509/4,710 s com 1:2:2; 4,142/4,238 s com 2:2:4. A segunda rodada inverteu a ordem. Os 60 PNGs das duas configurações são idênticos byte a byte.
- No Plus, mudar o tamanho dos blocos produz pequenas diferenças numéricas de inferência: PSNR de 53,37 dB entre as saídas. Isso não significa pixels idênticos nem prova que o modelo recupera detalhes reais ausentes na fonte.
- Blocos automáticos e dois workers de upscale também foram comparados; não foram adotados indiscriminadamente porque o ganho não foi consistente ou justificava memória adicional.

Artefatos e scripts de medição: `build/performance-review/`, `build/measure_processing.py`, `build/measure_tiles_more.py`, `build/measure_threads.py`, `build/measure_rife.py`. `isolated-before.json` e `final.json` contêm os tempos da tabela. `before.json` foi descartado da comparação por ter sobreposição com outro teste de GPU.

## Verificação

- Build Release do aplicativo e do harness de processamento.
- `FrameScaleProcessingSmoke`: aprovado, incluindo metadados incorretos, erros de decodificação, erro reportado com código de saída zero e contagem decodificada diferente da declarada. A exportação anterior permanece protegida em caso de falha.
- Seis casos reais de processamento combinado: restauração, ampliação 8×/10×, transparência e interpolação.
- Dezesseis casos de formatos/saída e doze casos de qualidade dos filtros/restauração: aprovados.
- Exportação Matroska sem contagem de frames no cabeçalho: aprovada pela contagem durante a decodificação.
- Não foram executados testes de interface.

O custo da inferência continua sendo relevante, sobretudo no Plus e ao combinar interpolação com upscale: todos os frames interpolados também precisam receber o processamento selecionado. As otimizações não substituem esses frames por cópias ou por um modelo mais leve.
