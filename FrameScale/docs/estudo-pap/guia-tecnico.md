# Entender o FrameScale, passo a passo

Atualizado em **09/10/2026**. [Plano diário](plano-de-estudo.md) · [Perguntas](perguntas-do-juri.md) · [Apresentação](roteiro-e-simulados.md)

Lê uma lição de cada vez. Em cada uma encontras uma explicação, um exemplo e uma frase que podes adaptar para a apresentação. As frases são apoio; não são um texto para decorar.

## 1. Para que serve o projeto?

O FrameScale permite editar e melhorar imagens e vídeos através de uma janela com controlos. Junta tarefas como aumentar a resolução, aumentar ou reduzir FPS, alterar a velocidade e exportar o resultado.

**Exemplo:** tens um vídeo pequeno e queres aumentar a resolução. Escolhes o ficheiro, o modelo, a escala e o destino. A aplicação prepara o trabalho, chama o motor necessário e verifica o ficheiro produzido.

**Como explicar:** “O objetivo é facilitar tarefas de processamento de imagem e vídeo. O utilizador escolhe as opções numa interface, sem precisar de preparar os comandos de cada ferramenta.”

O contributo do projeto está na interface, na edição, na organização das tarefas e no controlo do processamento. Qt, FFmpeg e os modelos de IA são de terceiros. Explica honestamente também o apoio que recebeste no desenvolvimento; não precisas de afirmar que escreveste tudo sozinho.

**Confirma se entendeste:** o que seria mais trabalhoso para o utilizador se esta interface não existisse?

## 2. Porque existem Qt, Qt Creator e VS Code?

São coisas diferentes:

| Nome | Explicação simples |
|---|---|
| C++ | A linguagem em que a aplicação foi escrita. |
| Qt | Bibliotecas que ajudam a criar a janela, os botões, os eventos e outras partes do programa. |
| Qt Creator | Um ambiente para escrever, compilar e depurar programas, com integração Qt. |
| VS Code | Outro editor em que podes trabalhar nos mesmos ficheiros. |
| CMake | Organiza como o projeto deve ser construído. |
| Ninja | Executa as tarefas de construção preparadas pelo CMake. |
| MinGW/GCC | Ferramentas que compilam o código C++ para produzir o programa. |

**Compilar** significa transformar o código num programa executável. **Executar** significa abrir esse programa já construído.

**Exemplo:** o `.exe` pode abrir normalmente mesmo que o VS Code esteja com um erro na configuração do Qt. Uma coisa é executar a compilação que já existe; outra é conseguir criar uma compilação nova.

**Como explicar:** “Qt é usado pela aplicação. Qt Creator e VS Code são ferramentas de desenvolvimento. O utilizador final precisa do programa e das dependências de execução, mas não precisa dos editores.”

## 3. Resolução, FPS e velocidade

Pensa num vídeo como uma sequência de imagens apresentada ao longo do tempo. Cada imagem é um **fotograma**, também chamado frame.

| Controlo | O que muda | Exemplo com vídeo de 10 segundos |
|---|---|---|
| Resolução | O tamanho de cada imagem em píxeis. | 1920 × 1080 ampliado 2× torna-se 3840 × 2160. |
| Interpolação | Cria imagens intermédias para aumentar FPS. | 30 → 60 FPS, mantendo aproximadamente 10 segundos. |
| Reduzir FPS | Usa menos imagens por segundo. | 60 → 24 FPS, mantendo aproximadamente 10 segundos. |
| Velocidade | O tempo que o movimento demora. | 2×: 5 segundos; 0,5×: 20 segundos; 0,25×: 40 segundos. |

**FPS** significa fotogramas por segundo. Dez segundos a 30 FPS têm cerca de 300 fotogramas; a 60 FPS, cerca de 600. Há arredondamentos porque não podemos exportar uma fração de fotograma.

**Como explicar:** “FPS controla a cadência das imagens. Velocidade controla a duração do movimento. Exportar a 120 FPS não torna o vídeo lento por si só.”

Na timeline, `00:38:19` significa 0 minutos, 38 segundos e 19 fotogramas. A 60 FPS, isso corresponde a aproximadamente 38,317 segundos. O último campo não representa centésimos. Para durações de uma hora ou mais aparece também o campo das horas.

**Faz sozinho:** quanto dura um trecho de 6 segundos a 2×? Resposta: 3 segundos. E a 0,5×? Resposta: 12 segundos.

## 4. O que a IA faz?

O programa utiliza modelos que já foram treinados por outros projetos. **Inferência** é usar esse modelo para processar uma nova imagem ou vídeo. O FrameScale não treinou esses modelos.

- RIFE estima imagens entre as imagens existentes, para interpolação.
- Real-ESRGAN e Real-CUGAN são usados em operações de ampliação/restauração compatíveis.
- Também existem filtros convencionais. Nem todo o ajuste de nitidez ou contraste é IA.

**Exemplo:** se tens duas imagens de uma pessoa a mover a mão, a interpolação tenta estimar como seria uma imagem entre elas. Pode acertar, mas também pode deformar os dedos.

**Como explicar:** “Uso modelos pré-treinados. A aplicação prepara as entradas, escolhe os parâmetros e integra os resultados. As imagens geradas são estimativas, por isso podem ter artefactos.”

Gravar a 120 FPS capta instantes reais. Interpolar para 120 FPS estima instantes. Não prometas que as duas coisas têm sempre a mesma qualidade.

## 5. O que acontece quando exporto?

```text
Escolhas do utilizador
        ↓
Guardar as opções da tarefa na fila
        ↓
Verificar se o pedido e as ferramentas são válidos
        ↓
Juntar as camadas, se necessário
        ↓
Processar e codificar com as ferramentas externas
        ↓
Verificar o resultado
        ↓
Colocar o ficheiro final no destino
```

**Codificar** é transformar as imagens e o áudio no formato que será guardado. FFmpeg é uma das ferramentas usadas nesse trabalho. `ffprobe` lê informações dos ficheiros.

Os nomes principais no código são:

| Nome | Responsabilidade |
|---|---|
| `MainWindow` | Liga as escolhas da janela às ações da aplicação. |
| `ProcessingOptions` | Guarda o pedido: ficheiros, modelos, FPS, camadas e outras opções. |
| `ExportDialog` | Organiza a fila e mostra o estado da exportação. |
| `ProcessingJob` | Coordena o trabalho, os erros e a verificação da saída. |

**Como explicar:** “A interface recolhe o pedido; o processamento executa as etapas. Antes de entregar o ficheiro, a aplicação verifica se o resultado corresponde ao pedido.”

O programa utiliza ficheiros temporários durante o trabalho. Isso ajuda a não apresentar um ficheiro incompleto como resultado final. Existem validações para não usar o original como destino e mecanismos de proteção ao substituir um ficheiro existente. Não são uma garantia contra qualquer avaria de disco ou falha de energia.

## 6. Camadas: vídeo e áudio na timeline

Uma camada guarda a referência a um ficheiro e as decisões de edição. Arrastar uma camada não altera fisicamente o original.

**Exemplo:** escolhes do segundo 2 ao segundo 8 de um vídeo. São 6 segundos de conteúdo. Se colocares velocidade 2×, esse conteúdo passa a ocupar 3 segundos na timeline.

| Ação | O que acontece no código atual |
|---|---|
| Arrastar a borda | Altera o trecho utilizado pela camada. |
| Arrastar o corpo | Altera a posição na timeline. |
| Marcadores da área de exportação | Escolhem a parte do projeto que será exportada. |
| Separar áudio e vídeo | Permite editar essas camadas independentemente. |
| Alterar velocidade com camadas ligadas | Aplica a alteração ao grupo ligado. |
| Cortar, mover ou excluir com camadas ligadas | Respeita o grupo ligado. |
| Silenciar áudio | Afeta só a camada de áudio, mesmo estando ligada ao vídeo. |
| Ocultar vídeo pelo ícone do olho | Afeta só a camada de vídeo, mesmo estando ligada ao áudio. |
| Ctrl+Z | Recupera um estado anterior das camadas. |

**Esta distinção foi atualizada em 09/10:** o material anterior dizia que ativar/desativar também afetava o grupo. Já não descreve `toggleLayerEnabled`.

A composição junta as camadas visuais e mistura as de áudio. Uma camada colocada mais tarde pode prolongar a duração. Se acelerares apenas o vídeo, outro áudio ainda pode continuar até mais tarde.

Na composição atual, a alteração de velocidade do áudio usa `atempo`, procurando manter o tom da voz. Existe código anterior com outro comportamento; para a apresentação explica o percurso de camadas que estás a demonstrar.

**Como explicar:** “As camadas guardam decisões de edição. O programa usa essas decisões para produzir a composição, mantendo os ficheiros originais separados.”

## 7. Fila, pausa e cancelamento

A fila permite preparar várias exportações. Cada tarefa guarda as suas opções; mudar os controlos depois não deve alterar silenciosamente um pedido já preparado.

A exportação atual tem **Pausar/Retomar**. O código tenta suspender o processo externo que está a trabalhar e retomá-lo depois. Não significa guardar uma exportação para continuar depois de fechar o programa. A pausa pode não estar disponível em instantes em que nenhum processo externo está a executar.

**Cancelar** termina o trabalho. A exportação pode tentar guardar um trecho parcial, se houver dados utilizáveis. A mensagem indica se algum trecho foi guardado. Não prometas que existe sempre um resultado parcial nem que ele equivale à exportação completa.

A velocidade mostrada durante a exportação é o ritmo do processamento, quando há contagem de frames disponível. **Não é o controlo 0,5× ou 2× que muda a duração do vídeo.**

**Como explicar:** “Pausar permite continuar o trabalho atual; cancelar termina-o. Guardar um trecho ao cancelar depende da etapa e dos dados já produzidos.”

## 8. Como sabes que funciona?

Um teste compara o que devia acontecer com o que aconteceu.

**Exemplo:** silencias o áudio de duas camadas ligadas. Esperas que o áudio fique desativado e o vídeo continue visível. O teste verifica esses dois estados. Esse teste existe em `layer_state_tests.cpp`.

Há diferentes formas de verificar o programa:

| Verificação | Para que serve |
|---|---|
| Testar uma regra do código | Confirmar, por exemplo, que opções inválidas são recusadas. |
| Usar uma ferramenta falsa | Provocar erros e cancelamentos de forma controlada. |
| Exportar com ferramentas reais | Confirmar que as partes funcionam juntas. |
| Ver e ouvir o resultado | Procurar problemas visuais e sonoros que os números não mostram. |

O CMake regista **seis testes CTest**, além de scripts adicionais. Isso não significa que existam apenas seis casos nem que todos tenham acabado de passar. Nesta revisão do material, li o código e os testes; não os executei novamente.

**Como explicar:** “Tenho verificações para cenários concretos. Os testes ajudam a encontrar erros e evitar que regressem, mas não provam que nunca haverá outro problema.”

Para investigar um erro: reproduz → recolhe informação → identifica a causa → corrige → repete o teste. Se o áudio está errado, compara primeiro a origem, a prévia e a exportação. Assim evitas corrigir a parte errada.

## 9. Três passeios curtos pelo código

Faz apenas um por sessão. Usa a pesquisa do IDE para encontrar os nomes; não precisas de decorar linhas.

### A — Como uma camada é representada

Abre [ProcessingOptions.h](../../src/core/ProcessingOptions.h) e procura `MediaLayer`.

| Campo | Significado |
|---|---|
| `path` | Ficheiro de origem. |
| `in` e `out` | Início e fim do trecho escolhido na origem. |
| `start` | Momento em que começa no projeto. |
| `speed` | Velocidade. |
| `link` | Grupo a que está ligada. |
| `enabled` | Se está ativa. |

Lê `duration()`: calcula `(out - in) / speed`. Testa mentalmente `in=2`, `out=8`, `speed=2`: duração de 3 segundos. Se `start=5`, a camada termina no segundo 8 do projeto.

### B — Como começa e termina uma exportação

Abre [MainWindow.cpp](../../src/MainWindow.cpp) e procura `beginExport`. Depois abre [ProcessingJob.cpp](../../src/processing/ProcessingJob.cpp) e procura `start`, `verifyFinalVideoProbe` e `copyOrCommitOutput`.

Concentra-te em três perguntas: quem recebe as opções? Quem verifica o resultado? Quem coloca o ficheiro no destino? Só depois explora `prepareVideoSteps`, que organiza as operações do vídeo.

### C — Como funciona desfazer

Abre [LayerEditor.h](../../src/ui/LayerEditor.h) e procura `setSpeed`, `remember`, `undo` e `redo`.

Antes da edição, o programa guarda os dados anteriores das camadas. Desfazer recupera esses dados. Não duplica o vídeo original em cada ação. Uma edição nova elimina a possibilidade de refazer o caminho anterior.

O histórico é limitado e é limpo em `setProject`. Não digas que Ctrl+Z desfaz qualquer ação da aplicação ou que o histórico fica guardado para sempre.

## 10. Só depois: perguntas mais técnicas

**Porque a janela continua a responder?** O trabalho pesado é enviado para programas externos, acompanhados através de `QProcess`. A interface recebe eventos de progresso e conclusão. Algumas verificações usam tarefas internas em threads. Um evento é uma notificação; não é automaticamente uma nova thread.

**Porque a prévia pode ser diferente?** A prévia das camadas é preparada numa resolução reduzida, até cerca de 720 píxeis no lado maior. Ajuda a ver a edição rapidamente. A exportação usa os originais; confirma a qualidade no resultado exportado.

**MP4 e H.264 são a mesma coisa?** Não. MP4 é o contentor: organiza os fluxos no ficheiro. H.264 é um codec: define como o vídeo é codificado. AAC é um exemplo de codec de áudio.

**O que significa CRF?** É um controlo de qualidade de certos codificadores. Em x264, um valor menor costuma conservar mais informação e aumentar o tamanho. Não recupera detalhe perdido na origem.

**Porque precisa de GPU?** Os motores de IA distribuídos utilizam GPU através de Vulkan. O desempenho depende da placa, do driver, do modelo e do vídeo. Outras operações usam caminhos diferentes.

**O programa precisa de internet?** O processamento analisado é local. O download por URL precisa de rede e depende do site. A leitura do código não equivale a uma auditoria de todo o tráfego das dependências.

**O que significa CFR/VFR?** Cadência constante/variável. Na constante, as imagens são apresentadas a intervalos regulares. Na variável, os intervalos podem mudar. Os timestamps indicam o momento de apresentação; ignorá-los pode afetar duração e sincronização.

**Guardar uma aba é guardar um projeto em disco?** Não necessariamente. `MainWindow::saveProject` conserva opções em memória. Um preset guarda configurações; não deve ser apresentado como um documento completo da montagem.

## O que mudou e onde foi confirmado

| Assunto | Estado conferido em 09/10/2026 | Fonte |
|---|---|---|
| Versão | 0.1.0; README identifica alpha, ainda em desenvolvimento. | [CMake](../../CMakeLists.txt), [README](../../README.md) |
| Pausa | `setPaused` suspende/retoma o processo ativo; a fila tem Pausar/Retomar. | [ProcessingJob](../../src/processing/ProcessingJob.cpp), [ExportDialog](../../src/ui/ExportDialog.cpp) |
| Som e visibilidade | A alteração de `enabled` é individual mesmo num grupo ligado. | [LayerEditor](../../src/ui/LayerEditor.h), [teste das camadas](../../tests/layer_state_tests.cpp) |
| Velocidade e vínculo | `setSpeed` continua a aplicar a velocidade ao grupo ligado. | [LayerEditor](../../src/ui/LayerEditor.h) |
| Cancelamento | Há tentativa de recuperar um trecho e mensagem com o resultado. | [ProcessingJob](../../src/processing/ProcessingJob.cpp), [MainWindow](../../src/MainWindow.cpp) |
| Testes novos dentro do executável de testes | Existem casos de pausa/retoma e recuperação parcial. A sua existência não prova uma execução aprovada. | [processing_smoke.cpp](../../tests/processing_smoke.cpp) |
| Composição | Continua a usar `atempo` no áudio e uma prévia reduzida. | [LayerComposition](../../src/processing/LayerComposition.h) |

Algumas descrições no README e nas notas antigas continuam diferentes do código. O relatório também precisa de ser alinhado com a versão que apresentares. Não foi alterado nesta revisão dos materiais de estudo.

**Limites para conhecer, sem decorar agora:** a extensão da composição ainda considera camadas desativadas; a dimensão/cadência base vem da primeira camada visual; compor vem antes de interpolar no percurso de camadas, pelo que slow motion não equivale a captura real a alta cadência. Os avisos de terceiros ainda registam pendências de distribuição. Consulta [THIRD_PARTY_NOTICES](../../THIRD_PARTY_NOTICES.md) antes de afirmar que o projeto está pronto para publicação comercial.
