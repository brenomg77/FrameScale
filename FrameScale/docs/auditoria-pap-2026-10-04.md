**FrameScale — auditoria técnica e preparação para a PAP**

> **Atualização de 05/10/2026:** esta auditoria foi conservada como registo do estado anterior. As correções, limpeza, testes aprovados e identificação do instalador atualizado estão no [relatório de fecho](correcoes-finais-2026-10-05.md). O parecer abaixo descreve a revisão de 04/10, não o estado corrigido.

Data: 4 de outubro de 2026. Versão declarada: 0.2.0. Objeto da análise: os ficheiros atuais da pasta de trabalho, incluindo alterações ainda não guardadas no Git.

**Parecer: o projeto tem uma base funcional substancial e conteúdo técnico adequado para uma PAP, mas esta revisão ainda não deve ser declarada finalizada.** Foram reproduzidos quatro erros funcionais, a suíte completa de interface não termina, existe um problema de segurança de memória identificado no código e há pendências na distribuição e nos documentos da entrega. A prioridade é corrigir e consolidar o que já existe. Não é necessário acrescentar funcionalidades para justificar o valor técnico do projeto.

Este parecer avalia o produto e as evidências disponíveis. A admissão e a classificação da PAP pertencem à escola e ao júri.

**1. Trabalho realizado e identificação da revisão**

Foi inventariada a árvore do projeto e revista a implementação por áreas: opções e catálogo, execução dos motores, verificação de resultados, cancelamento, fila, preview/comparação, interface, localização, download, compilação, instalação, dependências e documentação. A análise combinou leitura do código, testes existentes, novas reproduções isoladas e documentação oficial dos componentes.

A compilação foi feita de raiz, em `tmp/audit-2026-10-04/build`, sem reutilizar os objetos dos builds anteriores. Ambiente: Windows 10 Pro 10.0.19045, Qt 6.11.2, MinGW/GCC 13.1, Ninja e NVIDIA GeForce GTX 1070. Foram produzidos o executável, os cinco testes CTest e o teste separado do descodificador. A configuração e as 65 etapas de compilação terminaram com sucesso. O aviso sobre cabeçalhos Vulkan ausentes não impediu a compilação nem a execução dos motores Vulkan externos.

O Git já apresentava numerosas modificações, eliminações e ficheiros novos antes da auditoria. Não foram revertidas alterações nem modificados os ficheiros da aplicação ou dos testes originais. Foram criados este relatório, scripts de auditoria, ficheiros sintéticos e resultados de execução. Os hashes de 91 ficheiros de código, testes, recursos e configuração estão no [inventário da revisão](../../tmp/audit-2026-10-04/source-inventory.json). O identificador do último commit, isoladamente, não identifica esta revisão local.

**2. Resultados efetivamente obtidos nesta auditoria**

| Verificação | Resultado atual | Evidência |
| --- | --- | --- |
| Compilação Release de raiz | Aprovada | [build.log](../../tmp/audit-2026-10-04/build.log) |
| CTest completo | **4/5 aprovados; interface geral excedeu 180 s** | [ctest.log](../../tmp/audit-2026-10-04/ctest.log) |
| Motores reais: upscale, RIFE, combinação, VFR, rotação e áudio | **20/20 aprovados** | [media-pipelines.log](../../tmp/audit-2026-10-04/media-pipelines.log) |
| Todos os 13 modelos de upscale expostos no catálogo, nas escalas nativas e níveis de ruído admitidos | **34/34 combinações aprovadas** em imagem sintética | [catalog/summary.json](../../tmp/audit-2026-10-04/catalog/summary.json) |
| Módulos de saída: AVI, MP3/WAV, sequências, GIF, MOV, escala fracionária, bitrate e proteção do destino | **16/16 aprovados** | [resumo de saídas](../../tmp/audit-2026-10-04/build/output-module-validation/20261004-201041/summary.json) |
| ProRes: seis perfis e compatibilidade do preset HQ antigo | **7/7 aprovados** | [prores.log](../../tmp/audit-2026-10-04/prores.log) |
| Orientação real de vídeo, áudio, corte, GIF e sequência PNG | **10 verificações aprovadas** | [orientation.log](../../tmp/audit-2026-10-04/orientation.log) |
| Enhancement: filtros, restauro, bypass, áudio, imagens e vídeo | **18/18 aprovados** | [validate-enhancements.stdout](../../tmp/audit-2026-10-04/validate-enhancements.stdout) |
| Qualidade controlada dos filtros, transparência e cadência | **9/9 aprovados** | [enhancement-quality.log](../../tmp/audit-2026-10-04/enhancement-quality.log) |
| Restauro combinado com ampliação, incluindo 8×/10× | **6/6 aprovados** | [validate-combined-processing.stdout](../../tmp/audit-2026-10-04/validate-combined-processing.stdout) |
| Redução de ruído e dependência dos fotogramas anteriores | Aprovada; erro máximo medido: 0 | [validate-denoise-motion.stdout](../../tmp/audit-2026-10-04/validate-denoise-motion.stdout) |
| Cancelamento com recuperação de vídeo e preservação de destinos existentes | Aprovado nos cenários do script | [resultados adicionais](../../tmp/audit-2026-10-04/remaining-results.json) |
| Descodificador da preview: 24 e 24000/1001 FPS | Aprovado: pixels, cache, buscas e miniaturas | [cfr24-decoder.stdout](../../tmp/audit-2026-10-04/cfr24-decoder.stdout), [fractional-decoder.stdout](../../tmp/audit-2026-10-04/fractional-decoder.stdout) |
| Deploy CMake para uma pasta nova | Aprovado | [install.log](../../tmp/audit-2026-10-04/install.log) |
| Pacote com PATH restrito ao Windows: interface de orientação e exportação real | Ambos aprovados, usando as DLLs do pacote | [package-orientation-native.stdout](../../tmp/audit-2026-10-04/package-orientation-native.stdout), [package-processing.stdout](../../tmp/audit-2026-10-04/package-processing.stdout) |

Os resultados acima são grupos de testes com sobreposição; não representam uma percentagem de cobertura do código. Os testes de qualidade usam referências sintéticas, não provam superioridade visual em qualquer conteúdo. Os 34 casos do catálogo verificam execução, dimensões e descodificação; não comparam a qualidade percetiva dos modelos.

Os quatro testes CTest aprovados foram `FrameScaleCoreTests`, `FrameScaleOrientationUi`, `FrameScaleProcessingSmoke` e `FrameScaleExportQueueSmoke`. A suite de processamento com ferramentas simuladas também passou; as reproduções abaixo demonstram situações que essa suite ainda não cobre.

**3. Erros funcionais reproduzidos**

**F01 — Alta prioridade: a fila fica bloqueada quando é iniciada durante uma cópia de vídeo.**

Cenário: abrir um vídeo, iniciar a cópia através da preview e, antes de terminar, adicionar/iniciar uma exportação. A fila passa para “Renderizando”, mas não inicia o trabalho. Depois de a cópia terminar, continua bloqueada e o botão de exportar permanece desativado.

Causa: `ExportDialog::startNext`, linhas 547–565, marca o item como ativo e fica à espera da libertação do job. `MainWindow::beginExport`, linhas 1566–1569, retorna quando já existe processamento, sem comunicar a recusa à fila. `MainWindow::processingStopped`, linhas 1671–1680, não chama `jobReleased()` no ramo da cópia. O teste isolado encontrou `copy_finished=1`, `queue_row_state=Renderizando` e `queue_start_enabled=0`.

Correção necessária: coordenar a ocupação do motor com a fila; recusar ou adiar a transição de forma explícita e garantir a libertação do estado. Adicionar regressão que inicie uma exportação durante a cópia. Evidência: [harness UI](../../tmp/audit-ui/audit_ui.cpp) e [execução](../../tmp/audit-ui/audit_ui.log). Código: [MainWindow.cpp](../src/MainWindow.cpp) e [ExportDialog.cpp](../src/ui/ExportDialog.cpp).

**F02 — Prioridade média: a comparação reutiliza o resultado anterior depois de rodar ou espelhar o vídeo.**

Cenário: gerar uma comparação, fechá-la, rodar o vídeo 90° e voltar a abrir a comparação. O resultado antigo é reutilizado, apesar da nova orientação. O teste obteve `orientation_changed_cache_hit=1` e `same_output=1`.

Causa: a chave do cache em [ComparisonDialog.cpp](../src/ui/ComparisonDialog.cpp), linhas 303–314, considera entrada, modelo, escala, corte e filtros, mas não inclui `options.orientation`. A exportação de vídeo aplica a orientação, pelo que a comparação pode mostrar algo diferente do resultado exportado.

Correção necessária: incluir rotação e os dois espelhamentos na identidade do cache, com regressões para cada transformação e para a reposição da orientação. Evidência: o mesmo [teste UI](../../tmp/audit-ui/audit_ui.log).

**F03 — Prioridade média: exportar um vídeo com capítulos pode falhar na validação final.**

Foi gerado um MP4 válido de dois segundos com dois capítulos. A mesma origem sem capítulos exportou normalmente. A origem com capítulos falhou com: `O ficheiro final contém 1 streams inesperadas.` A reprodução foi repetida com o build novo.

O FFmpeg copia capítulos por omissão quando não recebe uma política explícita de `-map_chapters`; no MP4 testado isso introduz uma stream de dados. O validador em [ProcessingJob.cpp](../src/processing/ProcessingJob.cpp), linhas 1339–1347, rejeita qualquer stream diferente de vídeo/áudio. A montagem do comando não define o tratamento dos capítulos. O comportamento por omissão está documentado no [FFmpeg](https://ffmpeg.org/ffmpeg.html#Advanced-options).

Correção necessária: definir uma política coerente de capítulos e aplicá-la no comando e no contrato de validação. Se forem preservados, ajustar também a sua temporização ao corte. Se não forem suportados, desativar a cópia explicitamente. Não basta eliminar a verificação de streams inesperadas.

**F04 — Prioridade média: um MKV com áudio mais longo do que o vídeo falha por calcular mal a cadência.**

Entrada sintética válida: vídeo de dois segundos, quatro FPS, oito fotogramas; áudio de três segundos. A exportação falhou com: `O vídeo final contém 5 fotogramas; eram esperados 8.` A reprodução foi repetida com o build novo.

Causa: a duração do contentor é usada como alternativa quando não existe duração numérica da stream de vídeo (`ProcessingJob.cpp`, linhas 1506–1553). Mais tarde, a cadência normalizada é calculada como `sourceFrameCount_ / durationSeconds_`, linhas 791 e 841. Neste ficheiro, a duração do contentor acompanha o áudio; não é a duração da imagem. A amostragem produz menos fotogramas do que o contrato exige.

Correção necessária: obter a duração efetiva da stream de vídeo, incluindo alternativas baseadas nos seus timestamps; definir separadamente como tratar áudio mais longo. Acrescentar casos com diferenças entre durações de áudio/vídeo. Evidências dos erros F03/F04: [script de reprodução](../../tmp/audit-processing/run_cases.py), [resultados e comandos](../../tmp/audit-processing/results.json), [execução no build novo](../../tmp/audit-2026-10-04/processing-regressions.log).

Nestes dois erros a validação rejeitou o resultado; não se observou uma exportação incorreta ser apresentada como concluída. Ainda assim, são falhas de compatibilidade com entradas válidas e podem surgir numa demonstração.

**4. Segurança de memória e estado dos testes**

**F05 — Alta prioridade técnica: um callback de verificação em segundo plano pode sobreviver ao objeto que utiliza.**

Em `ProcessingJob.cpp`, linha 754, a verificação da imagem final captura `this` e lê `outputBuffer_`. Nas linhas 772–773, todas as verificações do pipeline de imagem são marcadas para execução em segundo plano. Nas linhas 1652–1661, o callback é enviado ao pool global; mantém vivos o resultado e a pasta temporária, mas não o `ProcessingJob`. O cancelamento e o destrutor, linhas 306–317 e 1842–1859, não esperam nem invalidam esse acesso ao objeto. A interface destrói o job após receber o cancelamento.

Se a tarefa estiver pendente no pool ou coincidir com a destruição/reutilização do job, pode ler memória já libertada ou alterada. Parar o timer que consulta o resultado não cancela a tarefa. O comentário “never this” na linha 1655 contradiz a captura da linha 754. A documentação de [QThreadPool](https://doc.qt.io/qt-6/qthreadpool.html) confirma que os callables podem ficar em fila para execução posterior.

**Estado da evidência:** o problema de duração de vida é identificável no código; não foi reproduzido um crash nem foi executado um sanitizador de memória. Não o contar como um quinto erro reproduzido dinamicamente. A correção deve remover a dependência do objeto nos callbacks assíncronos, passando dados imutáveis por valor ou garantindo uma duração de vida segura. É importante testar cancelamento/destruição com uma verificação ainda pendente.

**F06 — Bloqueio de finalização: a suite geral de interface está desatualizada e não termina.**

`FrameScaleUiSmoke` emite sete mensagens de falha e fica num diálogo até ao timeout de 180 segundos imposto pela auditoria. A causa do bloqueio foi localizada: em `tests/ui_smoke.cpp`, perto da linha 552, o teste espera um `QMessageBox` para abrir outro projeto. A implementação atual em `MainWindow.cpp`, linhas 1219–1220, apresenta um `QDialog` com o nome `openProjectDialog`. O timer do teste não o reconhece nem responde; a execução não avança.

Há outras expectativas antigas: o teste exige um timecode sempre completo, mas `TrimTimeline::timecode`, linha 58, passou a abreviá-lo; procura menus `selectionPopup`, mas `SelectionComboBox::showPopup`, linha 222, usa o popup normal do Qt no tema Studio. Também falham expectativas sobre o fundo do logótipo e as dimensões do botão Preview. Estas mensagens não devem ser apresentadas como sete defeitos independentes do produto: parte delas está comprovadamente desajustada da implementação atual.

Correção necessária: rever cada expectativa face ao comportamento pretendido, atualizar a interação com os diálogos e garantir timeout próprio para a suite geral no CMake. O [CMakeLists.txt](../CMakeLists.txt), linhas 214–222, define timeout para orientação, mas não para a suite UI completa. É necessário obter **5/5 testes concluídos e aprovados**, sem simplesmente retirar verificações para tornar o resultado verde. A versão atual não tem uma validação completa da interface.

Observação secundária: a API de processamento ignora orientação ao exportar imagens; foi verificado com PNG. Os menus normais de rotação estão restringidos a vídeo, pelo que não foi classificado como defeito da utilização normal da interface. Convém explicitar esse contrato ou rejeitar a opção quando a origem é imagem.

**5. Distribuição, dependências e reprodutibilidade**

**F07 — Bloqueio de distribuição: fontes SF Pro embutidas sem autorização demonstrada.**

[brand.qrc](../assets/brand.qrc), linhas 4–13, incorpora dez ficheiros SF Pro. [Typography.h](../src/ui/Typography.h), linha 10, carrega-os como fontes da aplicação Windows. Não foi encontrada autorização específica para esta incorporação. A licença pública da Apple consultada restringe o uso e proíbe embutir a fonte em programas. Antes de distribuir, deve ser demonstrado um direito de uso aplicável ou substituídas estas fontes por uma opção com direitos adequados, por exemplo a fonte já instalada no sistema. [Licença publicada pela Apple](https://developer.apple.com/fonts/).

Isto é uma correção de distribuição, não uma nova funcionalidade. A presença de outros textos de licença na pasta não resolve os direitos destas fontes.

**F08 — O instalador existente não corresponde ao build auditado.**

O setup em `FrameScale/dist` contém `bin/FrameScale.exe` com 66 709 595 bytes e data interna de 01/10/2026. O executável compilado de raiz nesta auditoria tem 66 726 737 bytes. A listagem do setup foi obtida sem o instalar: [installer-contents.txt](../../tmp/audit-2026-10-04/installer-contents.txt).

O SHA-256 do setup existente coincide com o ficheiro `.sha256`: `4854af023eb67c9a562024ab67ff772cc3e0c7044e3b1a7aced37f9dc433df25`. Esta coincidência comprova integridade face àquele hash, não atualidade ou correção do software.

Foi criado e ensaiado um novo pacote CMake isolado. A interface de orientação correu com o plugin Windows num ambiente de trabalho separado; uma exportação real também passou. O PATH estava restrito ao Windows e foram removidos os overrides de plugins Qt. Uma tentativa inicial de executar o pacote com `QT_QPA_PLATFORM=offscreen` falhou porque o deploy normal só inclui `qwindows.dll`; isso não foi classificado como falha do produto. O teste nativo posterior passou.

Não foi regenerado o setup público nem executado nesta auditoria o ciclo completo de instalação/atualização/desinstalação numa máquina limpa. Esse ciclo continua necessário para o artefacto final, após as correções. O teste das DLLs no computador de desenvolvimento é útil, mas não substitui esse ensaio.

**F09 — A proveniência e a entrega de componentes de terceiros precisam de ser fechadas.**

O [manifesto](../runtime/manifest.json) é datado de 10/09/2026, lista cinco pacotes e omite `yt-dlp.exe` e `deno.exe`, embora ambos sejam distribuídos e descritos nos avisos. Os hashes existentes referem-se a arquivos de distribuição, não a uma lista verificável de todos os binários instalados; os arquivos de download foram removidos. O endereço de FFmpeg usa `latest`, pelo que não basta para recuperar indefinidamente aquele mesmo arquivo. O manifesto deve identificar os componentes realmente entregues e permitir verificar as suas versões e integridade.

Os [avisos de terceiros](../THIRD_PARTY_NOTICES.md), linha 20, reconhecem que os links e os binários não demonstram, por si só, o cumprimento das obrigações de código-fonte correspondente. Não foi localizado nesta entrega um conjunto completo desse código nem uma oferta concreta para o pacote final. É necessário fechar essa documentação antes de redistribuir. Esta observação não significa que o código da aplicação tenha automaticamente de adotar a licença de todos os executáveis que invoca. As obrigações dependem do componente e da forma de integração/distribuição. [Obrigações descritas pelo Qt](https://www.qt.io/development/open-source-lgpl-obligations), [licenciamento do FFmpeg](https://ffmpeg.org/legal.html).

**F10 — A documentação e o Git ainda não descrevem uma entrega única e estável.**

O README mistura o estado atual com notas sucessivas de revisões. Por exemplo, a descrição inicial afirma que só existe Studio e que temas/transparência foram removidos; `MainWindow::showSettings` ainda oferece três temas e transparência. O relatório PAP refere quatro testes CTest; o CMake atual regista cinco. Algumas instruções e scripts dependem de diretórios/versões locais de Qt e builds antigos. O CMake não declara a versão mínima de Qt, apesar de usar APIs de deploy introduzidas depois do primeiro Qt 6; convém declarar e documentar uma versão efetivamente suportada. [Referência de deploy Qt](https://doc.qt.io/qt-6/qt-generate-deploy-app-script.html).

Há ficheiros essenciais novos que ainda não estão guardados no Git, incluindo partes da comparação, plataforma e download. Uma entrega feita apenas a partir dos commits atuais não representa o estado analisado. Deve ser preparada uma revisão identificável, com os fontes necessários, instruções reproduzíveis, os testes e um pacote construído dessa mesma revisão. Não foi escolhido nem aplicado um novo regime de licença ao código próprio nesta auditoria.

**6. Avaliação para a PAP**

Existe um relatório real e substancial em [DOCX](../../output/relatorio-pap/Relatorio_PAP_FrameScale_Breno_Amorim_2026.docx) e [PDF](../../output/relatorio-pap/Relatorio_PAP_FrameScale_Breno_Amorim_2026.pdf). O PDF tem 38 páginas; o texto extraído do DOCX contém cerca de 8 525 palavras. Inclui contexto, objetivos, requisitos, arquitetura, tecnologias, testes, limitações, referências, autoavaliação, roteiro e anexos. A análise verificou o texto e as páginas do regulamento reproduzidas no final; não corresponde a uma revisão gráfica de todas as páginas.

O relatório distingue corretamente a contribuição própria — integração, interface, execução, validação e organização do fluxo — dos modelos pré-treinados de terceiros. Também reconhece falhas de interface, limites das medições e ausência de uma nova execução integral durante a sua redação. Estes são pontos positivos para uma defesa tecnicamente honesta.

Existe, contudo, uma pendência documental concreta: o próprio relatório informa que não inclui os registos autênticos de avaliações intermédias da orientadora. Na reprodução do regulamento, artigo 8.º, n.º 2, alínea d), esses registos são indicados entre os anexos do relatório final. As páginas 37–38 do PDF foram consultadas visualmente. A autoavaliação retrospetiva não substitui as avaliações docentes. É necessário obter e anexar os registos efetivos, com a orientação da escola.

Antes da entrega, devem ainda ser atualizados os resultados para esta revisão, a contagem dos testes, o balanço dos objetivos e a identificação do instalador. Não foram encontrados slides `.pptx`/`.odp` nesta pasta. Isso não demonstra que não existam noutro local. O roteiro de utilização já existente é uma boa base para a demonstração.

Para a apresentação, convém preparar exemplos curtos e locais, com originais e resultados pré-calculados, que mostrem fotografia, animação e movimento. Medir o tempo no computador que será usado. Demonstrar a escolha de modelo, o corte, a comparação e uma exportação com áudio; explicar artefactos, custos de processamento e limitações. Os testes pequenos não permitem prever o tempo de um vídeo longo ou afirmar que a interpolação preserva sempre os detalhes. A avaliação da qualidade deve usar amostras visuais representativas, para além dos testes de metadados e pixels sintéticos.

**7. O que está bem construído**

O núcleo do projeto funciona com ferramentas reais. Todos os modelos de upscale expostos foram executados nas combinações nativas verificadas. Os pipelines centrais de vídeo, RIFE, áudio, formatos, filtros e escalas passaram em muitas situações concretas. Existem validação dos parâmetros, pares de modelos verificados, ficheiros temporários, controlo de substituição do destino e descodificação final. Os testes de proteção de ficheiros e recuperação de cancelamento que foram executados passaram.

O uso de QProcess com argumentos separados, em vez de construir comandos de shell com os caminhos da origem, é um ponto positivo. O download usa opções para ignorar configurações/plugins externos, recusa credenciais no URL e verifica que o resultado está dentro da pasta temporária. O desinstalador usa uma lista de ficheiros fornecidos pelo pacote e remoção não recursiva das pastas, preservando a intenção de não eliminar ficheiros alheios. Estas medidas não constituem uma certificação geral de segurança, mas revelam preocupação com falhas reais.

A organização entre catálogo/opções, motor, preview e fila é compreensível para apresentação. Existe documentação suficiente para explicar o projeto. Como dívida de manutenção, alguns ficheiros acumulam responsabilidades e são extensos: `ProcessingJob.cpp` tem 2 081 linhas, `MediaPreviewWidget.cpp` 1 833 e `MainWindow.cpp` 1 751. Isto merece melhoria gradual; uma reestruturação ampla perto da entrega aumentaria o trabalho de revalidação e não é necessária para resolver os erros identificados.

**8. O que esta auditoria ainda não demonstra**

Não foi feito um ensaio de duração longa, esgotamento real do disco, queda de energia, falha forçada do driver, matriz de GPUs Intel/AMD, outros sistemas operativos ou uma revisão com sanitizadores. Não foi concluído o percurso inteiro da suite de interface, devido ao bloqueio explicado. O ciclo completo do setup final numa máquina limpa permanece por executar. Os serviços externos de vídeo não foram testados ao vivo nesta revisão; leitura do código e existência dos executáveis não provam compatibilidade atual com todos os sites.

Também não foi feita uma certificação de ausência de vulnerabilidades nos binários de terceiros. A existência de um componente antigo, isoladamente, não foi tratada como prova de vulnerabilidade. A review identifica limites concretos, em vez de declarar “sem erros” a partir da compilação ou dos testes aprovados.

**9. Ordem recomendada para fechar o projeto**

1. Corrigir a coordenação entre cópia e fila e o acesso ao job nas verificações assíncronas.
2. Corrigir a chave de comparação, o tratamento de capítulos e o cálculo da duração de vídeo nos casos MKV demonstrados.
3. Atualizar a suite UI para o comportamento desejado e acrescentar regressões dos erros encontrados; obter os cinco testes aprovados e repetir os cenários reais afetados.
4. Resolver os direitos das fontes e completar proveniência, avisos e entrega aplicável de código-fonte dos terceiros.
5. Consolidar código/documentação numa revisão identificável, gerar um novo setup e testar instalação, arranque, processamento, atualização e desinstalação no ambiente de entrega.
6. Atualizar o relatório PAP com os resultados finais, integrar os registos docentes em falta e ensaiar a defesa/demonstração no equipamento escolhido.

Não foram acrescentadas funcionalidades. Não proponho alargar o âmbito nesta fase: os passos acima são correções e conclusão da entrega já existente. Eventuais funcionalidades novas devem ser discutidas separadamente com o autor antes de implementação.

**10. Reproduzir e conservar as evidências**

Os caminhos abaixo partem da raiz `FrameScale_v0.2.0_source`. Usar uma nova pasta para repetir a auditoria e conservar os resultados desta execução.

```powershell
$env:PATH = 'C:/Qt/Tools/mingw1310_64/bin;C:/Qt/Tools/Ninja;C:/Qt/6.11.2/mingw_64/bin;' + $env:PATH
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' -S FrameScale -B tmp/audit-recheck -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe
& 'C:/Qt/Tools/CMake_64/bin/cmake.exe' --build tmp/audit-recheck --target FrameScale framescale-tests FrameScalePreviewDecodeCheck --parallel 2
& 'C:/Qt/Tools/CMake_64/bin/ctest.exe' --test-dir tmp/audit-recheck --output-on-failure --timeout 180
& FrameScale/tests/validate-media-pipelines.ps1 -BuildDir tmp/audit-recheck -QtBin C:/Qt/6.11.2/mingw_64/bin
python FrameScale/tests/validate-output-modules.py --build tmp/audit-recheck
python FrameScale/tests/validate-prores.py --build tmp/audit-recheck
python FrameScale/tests/validate-orientation.py --build tmp/audit-recheck
python FrameScale/tests/validate-enhancement-quality.py --build-dir tmp/audit-recheck
```

Os scripts de testes existentes sem parâmetro de build foram copiados para a pasta de auditoria e ajustados apenas para apontar ao executável novo, mantendo a origem identificada. Os ficheiros originais não foram editados. O [runner adicional](../../tmp/audit-2026-10-04/run_remaining.py) regista essa adaptação. O [ensaio do catálogo](../../tmp/audit-2026-10-04/check_catalog.py) usa apenas imagens sintéticas. As reproduções de bugs estão em `tmp/audit-processing` e `tmp/audit-ui`; os scripts e os logs devem ser conservados juntamente com este relatório. Um script de reprodução pode terminar com sucesso precisamente porque confirmou o erro procurado: consultar os resultados individuais, não apenas o exit code do script.
