# FrameScale — fecho das correções e limpeza

Data: **5 de outubro de 2026**. Versão: **0.2.0**, Windows x64. Complementa a [auditoria de 04/10](auditoria-pap-2026-10-04.md), preservada como evidência histórica.

> **Correção posterior comunicada pelo utilizador:** a seleção de um único fotograma podia prender as alças de corte sobrepostas. Esse caso não estava coberto pelos testes desta revisão; a conclusão técnica abaixo não demonstrava a sua ausência. Ver [correção e regressões da timeline](correcao-alcas-timeline-2026-10-05.md). O setup em `dist` foi substituído pela versão corrigida; os hashes abaixo identificam o pacote anterior.

## Conclusão

**A implementação das correções técnicas desta revisão está concluída e a aplicação está apta para uma demonstração local da PAP no ambiente testado.** Os defeitos funcionais reproduzidos na auditoria foram corrigidos, a suite deixou de bloquear e o instalador foi reconstruído e verificado. Não foram acrescentadas funcionalidades ao produto.

**A entrega global da PAP ainda não pode ser declarada encerrada:** faltam os registos autênticos das avaliações intermédias mencionados pelo próprio relatório académico, a integração desta atualização técnica no dossier e o ensaio no computador da defesa. A distribuição pública dos binários também mantém pendências de fontes/licenças de terceiros, descritas abaixo. Não se afirma que o programa seja isento de qualquer erro possível.

## Correções realizadas

| Auditoria | Correção e evidência atual |
| --- | --- |
| F01 — fila bloqueada durante cópia de vídeo | A fila recebe a disponibilidade do processamento partilhado. Iniciar fica indisponível durante a cópia; a libertação efetiva do job permite continuar. Cópia concluída e cancelada, edição de itens pendentes e exportação posterior verificadas com FFmpeg real e teste da fila. |
| F02 — comparação com orientação antiga | A chave de cache inclui rotação e ambos os espelhamentos. Testados rotação, flip horizontal/vertical, reset e reutilização apenas quando as opções coincidem. |
| F03 — capítulos causam stream inesperada | O FFmpeg recebe `-map_chapters -1` explicitamente na exportação de vídeo/áudio. Capítulos não são copiados; metadados comuns compatíveis mantêm o comportamento existente. Verificados vídeo com capítulos, corte, metadados desligados e saída só de áudio. |
| F04 — duração do áudio altera contagem de vídeo | O cálculo usa o primeiro stream de vídeo e, quando faltam dados, os timestamps dos fotogramas, consumidos incrementalmente. MKV com áudio maior/menor, corte, silêncio, um fotograma e VFR aprovados. O áudio longo é limitado à duração visual. |
| F05 — verificação assíncrona captura job destruído | Diagnósticos do processo são verificados na thread proprietária. Os callbacks enviados ao pool conservam apenas caminhos/dimensões imutáveis e o workspace necessário. Testes determinísticos enfileiram a verificação final e cancelam/destroem o job antes da execução do callback. |
| F06 — testes de interface desatualizados | Atualizados diálogos, timecode compacto, logo, dimensões, fontes, default de processamento e os popups específicos de cada tema. A orientação tem um CTest dedicado para evitar interferência de eventos de ativação entre janelas nativas. Os cinco CTest e a suite geral no Windows nativo passaram. |
| F07 — fontes externas embutidas | Retiradas as dez fontes SF Pro e as referências do QRC. A aplicação utiliza a fonte instalada no sistema/Segoe UI. |
| F08 — setup antigo | Novo setup construído a partir do Release corrigido; executable SHA-256 coincide entre build, payload e instalação. Instalador testado conforme a secção seguinte. |
| F09 — inventário incompleto | Manifesto schema 2 com 65 ficheiros e hashes, yt-dlp/Deno identificados, versão real do FFmpeg e avisos corrigidos. Verificador rejeita ficheiros ausentes, alterados ou não inventariados. A entrega das fontes correspondentes dos terceiros permanece pendente. |
| F10 — documentação e identificação | README consolidado, notas antigas arquivadas, Qt mínimo declarado e scripts de regressão com caminhos de build configuráveis. Os registos de build e validação identificam os fontes por hash, incluindo ficheiros novos que ainda não constam do último commit. |

O caso VFR adicional revelou outra falha: FPS normalizado escrito com muitas casas decimais podia produzir uma base de tempo que o muxer MP4 recusava. Os argumentos de FPS passaram a usar uma fração reduzida, com precisão de micro-FPS e limites inteiros. O caso real que falhava passou após a correção. Também se passou a rejeitar explicitamente orientação de imagens no núcleo, pois a função existente é exclusiva de vídeo e seria ignorada silenciosamente.

## Limpeza efetiva

Foram retirados da árvore ativa `assets/symbol.png` (o ficheiro mostrado na imagem), `framescale.png`, `title-mark.svg`, `check.svg` e `chevron-light.svg`, todos sem referências de utilização. O QRC conserva apenas os recursos ativos.

Retirados três diretórios RIFE fora do catálogo (`v4.25`, `v4.25-lite`, `v4.26`), quatro aliases Real-ESRGAN `.bin/.param` que tinham hashes idênticos aos ficheiros canónicos utilizados, o script Anime4K antigo sem uso e dois inventários antigos de limpeza. Estes 18 ficheiros somavam **77 349 414 bytes**. As dez fontes somavam outros **63 565 296 bytes**. O executável passou de cerca de **66,7 MB para 2,9 MB**.

As revisões de desenvolvimento ficaram em [docs/history](history/README.md); a arte original editável foi preservada. Os diretórios abandonados `build` e `Testing` da raiz do workspace foram retirados; a compilação válida está dentro de `FrameScale/build`. Builds/evidências citados pela PAP foram conservados. Caches locais passaram a ser ignorados pelo Git.

A recuperação local está em `tmp/finalization-2026-10-05/retired` e, para as fontes, em `tmp/audit-fixes-2026-10-04/backup/SF-Pro-fonts`, fora da árvore ativa da aplicação. O [inventário de retirada](../../tmp/finalization-2026-10-05/retired/inventory.json) regista os hashes. Estes backups não entram no instalador.

## Validação executada

Ambiente: Windows 10 Pro 19045, Qt 6.11.2, MinGW/GCC 13.1, Ninja e NVIDIA GTX 1070. Build inicial limpo em `build/final-2026-10-05`, seguido da recompilação das correções encontradas pelos testes. Todos os resultados abaixo referem-se ao executável corrigido.

| Verificação | Resultado |
| --- | --- |
| CTest: núcleo, interface, orientação, processamento determinístico e fila | **5/5 aprovados**; interface/orientação novamente aprovadas após o ajuste final dos testes nativos |
| Regressões reais novas: duração, VFR e capítulos | **10/10** |
| Motores reais: Real-ESRGAN, Real-CUGAN, Anime4K, RIFE, VFR, áudio e rotação | **20/20** |
| Módulos de saída, escala fracionária, áudio e proteção do destino | **16/16** |
| ProRes, incluindo compatibilidade do preset antigo | **7/7** |
| Orientação, pixels, áudio e corte | **10/10** |
| Enhancement e bypass | **18/18** |
| Medições sintéticas de filtros e três modelos de restauração | **12/12** |
| Restauração/ampliação combinadas | **6/6** |
| Denoise e movimento | Erro máximo dependente do histórico: **0** |
| Cancelamento real com preservação parcial e colisões de nomes | Aprovado |
| Suite geral UI com plugin Windows, em desktop isolado | Aprovada |
| Pacote com PATH limitado ao Windows: orientação e regressões de cópia/fila/comparação | Ambos aprovados |
| Integridade do runtime/modelos e presença de licenças | **65 artefactos / 19 textos** verificados |
| Setup: instalação, bloqueio, reinstalação e desinstalação | Aprovado; todos os **151 ficheiros** comparados com o payload e ficheiro alheio preservado |

As primeiras tentativas com falhas ficaram nos logs para rastreabilidade. Um teste novo tinha o destino com o mesmo nome do fixture de capítulos; foi corrigido. A matriz WMA ainda esperava a duração do contentor com atraso de áudio, precisamente o comportamento corrigido por F04; passou a verificar o segundo exato de vídeo gerado pelo fixture. A suite nativa tinha uma expectativa de alfa anterior à composição atual do painel e repetia a orientação após o fecho de outras janelas; os contratos vigentes continuam cobertos. O script de verificação do instalador passou a capturar diretamente as saídas do `reg.exe`: a mensagem de sucesso em stderr interrompia a restauração com a política Stop do PowerShell. Os backups foram repostos e o ciclo foi repetido com essa correção.

Evidências: [CTest](../../tmp/finalization-2026-10-05/ctest-final.log), [UI final](../../tmp/finalization-2026-10-05/ui-ctest-complete.log), [10 regressões](../../tmp/finalization-2026-10-05/processing-real-final.log), [matrizes](../../tmp/finalization-2026-10-05/matrices-summary.json), [motores](../../tmp/finalization-2026-10-05/media-pipelines-complete.log), [pacote nativo](../../tmp/finalization-2026-10-05/package-native-results.json) e [instalador](../../tmp/finalization-2026-10-05/installer-verification-complete.log).

## Entrega identificada

- [Instalador da revis?o anterior](../build/release-records/archive/5508a43abb2d468892000fa24a7e79b6/FrameScale-0.2.0-Windows-x64-Setup.exe): **294 328 064 bytes**.
- SHA-256: `2b5815275fef27801105bf9cbf5bd71dcc5b43ab63d40b2e77095280a3347563`.
- SHA-256 do `FrameScale.exe`: `942cd3c5ea13c9f4f39d0c34db945ff61756523f32e61cc406e044489efaf6b3`.
- [Registo da construção](../build/release-records/archive/5508a43abb2d468892000fa24a7e79b6/FrameScale-0.2.0-Windows-x64-Setup.exe.build.json) e [registo da validação final](../build/release-records/archive/5508a43abb2d468892000fa24a7e79b6/FrameScale-0.2.0-Windows-x64-Setup.exe.validation.json). Dois ficheiros de testes/verificação foram ajustados depois da construção do setup; o segundo registo documenta esses hashes finais. Os fontes da aplicação coincidem com os usados no setup.

O teste do instalador utilizou uma pasta temporária e restaurou as entradas de registo/atalhos existentes. A reinstalação testada foi do setup novo sobre a mesma pasta, não uma matriz de migração de todas as versões antigas. O código e os ficheiros novos não foram automaticamente incluídos num commit: para entregar via Git, é necessário conservar esta árvore final, e não apenas os commits anteriores.

## Pendências reais e limites

1. **Dossier PAP:** anexar os registos docentes autênticos em falta e incorporar estes resultados/identificação do setup no relatório de 03/10. Esta atualização não substitui avaliações da orientadora nem a decisão da escola/júri.
2. **Computador da defesa:** instalar e ensaiar os exemplos no equipamento que será utilizado. Os ensaios locais e o PATH restrito não equivalem a uma máquina limpa ou a uma matriz de GPUs.
3. **Distribuição pública:** completar a entrega aplicável de fontes correspondentes e os registos de dependências/termos de redistribuição indicados em [THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md). Inventários e links de upstream não resolvem, por si sós, essa entrega. Nenhum compromisso de fornecimento de fontes foi inventado em nome do autor.

Não foram revalidados todos os serviços de download externos, vídeos longos, esgotamento de disco, falhas de energia ou outras GPUs. O fallback de duração pode exigir leitura dos fotogramas e acrescentar tempo à análise de ficheiros longos. As medições sintéticas aprovadas não prometem qualidade visual universal. Estas limitações são distintas dos defeitos corrigidos e devem ser explicadas na defesa.

Organiza??o posterior da entrega: a pedido do autor, `dist` conserva apenas o `.exe`. Os tr?s registos t?cnicos foram movidos sem altera??o para `build/release-records`; os scripts e as liga??es da documenta??o foram ajustados. Os invent?rios continuam a identificar a revis?o testada na data da sua cria??o, anterior a esta reorganiza??o. O instalador n?o foi alterado.
