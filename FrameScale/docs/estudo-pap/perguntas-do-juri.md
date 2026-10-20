# Perguntas do júri — respostas curtas para treinar

Atualizado em **09/10/2026**. [Plano](plano-de-estudo.md) · [Explicações e exemplos](guia-tecnico.md)

**Começa pelas perguntas 1–15.** Não precisas de aprender as 75 de uma vez. As respostas são pontos de partida: usa as tuas palavras e conta apenas experiências que realmente tiveste.

Depois de cada resposta, tenta acrescentar um exemplo do programa. Se não entenderes uma palavra, volta ao guia antes de decorar a frase. Não é possível prever todas as perguntas; o objetivo é perceber os assuntos para responder também a uma formulação diferente.

## Primeiro: as 15 essenciais

### 1. Para que serve o FrameScale?

Para facilitar a edição, melhoria e exportação de imagens e vídeos numa aplicação gráfica.

### 2. Quem pode usar o programa?

Por exemplo, estudantes e criadores de conteúdos. São públicos possíveis; não realizei um estudo de mercado só por os identificar.

### 3. O que foi desenvolvido no projeto?

A interface, a edição, a integração das ferramentas, a fila e as verificações. Os modelos e ferramentas externas não foram criados neste projeto.

### 4. Usaste IA para ajudar a programar?

Explica o apoio que realmente recebeste e como verificaste as sugestões. Não inventes autoria individual nem uma percentagem de código teu.

### 5. Qt e Qt Creator são a mesma coisa?

Não. Qt fornece bibliotecas usadas pela aplicação. Qt Creator é um ambiente para desenvolver o programa; VS Code é outra ferramenta de desenvolvimento.

### 6. O utilizador precisa do Qt Creator?

Não. Precisa do executável e das dependências de execução. Compilar o programa é diferente de abrir um programa já compilado.

### 7. Qual a diferença entre FPS e velocidade?

FPS é a quantidade de imagens por segundo. Velocidade muda quanto tempo o movimento demora: 10 segundos a 2× passam a 5 segundos.

### 8. O que fazem interpolação e redução de FPS?

Interpolação estima imagens intermédias para aumentar a cadência. Redução usa menos imagens por segundo. Ambas procuram manter a duração, dentro dos arredondamentos necessários.

### 9. Criaste os modelos de IA?

Não. A aplicação usa modelos pré-treinados de terceiros. Usar esses modelos numa nova entrada chama-se inferência.

### 10. O que acontece quando exportas?

A aplicação guarda as opções da tarefa, verifica o pedido, executa as operações, verifica a saída e coloca o ficheiro no destino.

### 11. O que são camadas?

São dados que dizem que trecho de cada ficheiro aparece, onde começa e a que velocidade é apresentado. Editá-las não corta fisicamente o original.

### 12. O que muda quando áudio e vídeo estão ligados?

Velocidade, corte, movimento e exclusão respeitam o grupo ligado. Silenciar áudio e ocultar vídeo são individuais no código atual.

### 13. Qual a diferença entre pausar e cancelar?

Pausar tenta suspender o processo ativo para continuar depois. Cancelar termina o trabalho; pode haver recuperação de um trecho, mas não é garantida.

### 14. Como sabes que funciona?

Comparo resultados esperados com resultados obtidos, usando testes de regras, erros simulados e exportações reais. Ver e ouvir a saída também é necessário.

### 15. O programa está sem erros?

Não posso garantir isso. Posso explicar os cenários testados, as correções feitas e os limites conhecidos. A versão atual está identificada como alpha.

## Depois: compreender o funcionamento — 16 a 35

### 16. Porque C++ e Qt?

São adequados a uma aplicação desktop com interface, ficheiros e processos externos. A escolha também exige gerir compilação e dependências; não explica sozinha a rapidez dos motores de IA.

### 17. Para que servem CMake e Ninja?

CMake prepara como construir o projeto. Ninja executa essas tarefas. O compilador transforma o código C++ em programa executável.

### 18. Porque o exe funciona mas o IDE mostra um erro Qt?

O exe pode já estar compilado. O erro pode estar na configuração para fazer uma nova compilação, como caminhos, compilador ou arquitetura incompatíveis.

### 19. Quais são as partes principais do código?

MainWindow liga as ações da janela; ProcessingOptions guarda o pedido; LayerEditor trata a edição; ExportDialog organiza a fila; ProcessingJob coordena a exportação.

### 20. Porque guardar opções numa estrutura?

Para ter um pedido claro que pode ser validado e guardado na fila. Assim, o trabalho não depende continuamente dos valores que aparecem nos controlos.

### 21. Porque a janela não bloqueia durante a exportação?

O trabalho pesado é executado em programas externos, acompanhados por eventos. A interface pode continuar a receber cliques e a mostrar progresso.

### 22. O que é QProcess?

É a classe Qt usada para iniciar e acompanhar programas externos, como FFmpeg. Permite receber saída, erros e conclusão.

### 23. Um processo e uma thread são iguais?

Não. Um processo é uma execução de programa com recursos próprios. Threads são percursos de execução dentro de um processo e partilham memória.

### 24. O que faz RIFE?

Estima fotogramas intermédios. Pode aumentar a fluidez, mas pode criar deformações ou outros erros visuais.

### 25. É igual a filmar a 120 FPS?

Não. A câmara capta instantes reais; a interpolação estima-os. Mais FPS gerados não garantem a mesma informação de uma captura real.

### 26. Aumentar resolução garante qualidade?

Não. A imagem fica maior, mas o detalhe estimado pode estar errado. A qualidade depende da origem, do modelo e da compressão.

### 27. Como se calcula a duração de uma camada?

Divide-se o trecho da origem pela velocidade: (out − in) / speed. Um trecho de 6 segundos a 2× ocupa 3 segundos.

### 28. Mover uma camada é o mesmo que a cortar?

Não. Mover altera quando aparece no projeto. Cortar altera o trecho da origem que é utilizado.

### 29. Cortar a camada é o mesmo que mudar a área de exportação?

Não. A borda da camada altera esse conteúdo. Os marcadores de exportação escolhem o intervalo do projeto que será exportado.

### 30. Silenciar áudio ligado também esconde o vídeo?

Não no código atual. O som e a visibilidade são controlados individualmente, mesmo quando outras edições atuam sobre o grupo.

### 31. Como funciona Ctrl+Z?

Guarda-se o estado anterior dos dados das camadas e recupera-se esse estado ao desfazer. Não se guarda uma cópia inteira do vídeo por ação.

### 32. Pausar permite fechar o programa e continuar amanhã?

Não. A pausa atual suspende o processo em execução. Não existe aqui uma promessa de guardar uma exportação para a retomar depois de fechar a aplicação.

### 33. Cancelar guarda sempre uma parte do vídeo?

Não. A aplicação tenta recuperar um trecho em percursos compatíveis, se houver dados utilizáveis. A mensagem indica se conseguiu guardar alguma coisa.

### 34. Porque a prévia pode ter menos qualidade?

A composição da prévia usa resolução reduzida para facilitar a interação. A exportação utiliza os originais; a qualidade final deve ser avaliada no ficheiro exportado.

### 35. O relatório já descreve tudo isto?

Não completamente. Algumas partes descrevem versões anteriores. É necessário alinhar relatório, slides e executável antes da apresentação.

## Treino prático — 36 a 55

### 36. Como se lê 00:38:19?

Na timeline curta: 0 minutos, 38 segundos e 19 fotogramas. A 60 FPS, corresponde aproximadamente a 38,317 segundos.

### 37. Como se calcula a duração do projeto?

Pelo fim da camada que termina mais tarde, com ajuste à grelha de fotogramas. Não se somam faixas que decorrem ao mesmo tempo.

### 38. Porque acelerar o vídeo pode não encurtar o projeto?

Se outra camada, como o áudio separado, continuar até mais tarde, ela ainda determina o fim da composição.

### 39. A velocidade muda o tom da voz?

Na composição atual de camadas usa-se atempo para alterar o tempo procurando preservar o tom. Notas antigas descrevem outro percurso e não devem ser generalizadas.

### 40. Como corrigir um áudio desfasado?

Se o atraso é constante, deslocar a camada pode resolver. Se o atraso cresce, é necessário investigar velocidade e tempos da origem.

### 41. Como investigas um áudio que falha só na prévia?

Comparo o mesmo trecho na origem, na prévia e na exportação. Depois analiso os tempos e mensagens para localizar a falha, sem alterar o original para a esconder.

### 42. MP4 é um codec?

Não. É um contentor que organiza os fluxos. H.264 é um codec de vídeo e AAC é um codec de áudio.

### 43. O que é CRF?

Um controlo de qualidade de certos codificadores. Em x264, menor costuma significar mais qualidade e ficheiro maior, sem recuperar informação perdida.

### 44. O preset rápido acelera o vídeo?

Não. Altera o esforço e o tempo de codificação. A velocidade de reprodução é outro controlo.

### 45. Os FPS mostrados durante a exportação são os FPS finais?

O ritmo de processamento indica quantos frames estão a ser tratados por segundo na etapa. Não é necessariamente a cadência escolhida para o vídeo final.

### 46. Porque algumas combinações de formato são recusadas?

Porque formatos e codecs têm capacidades diferentes. A aplicação valida as combinações que suporta.

### 47. A fila acompanha alterações feitas depois nos controlos?

Cada pedido conserva as opções com que foi preparado. Isto evita mudar silenciosamente uma tarefa já adicionada.

### 48. Uma percentagem de progresso diz o tempo exato que falta?

Não. As etapas têm custos diferentes. É uma indicação de avanço, não uma garantia de tempo restante.

### 49. Porque o processo terminar não basta?

Pode terminar e produzir uma saída incorreta. É necessário verificar propriedades e se o conteúdo pode ser descodificado.

### 50. O que é verificado na saída?

Conforme o formato, dimensões, duração, cadência, frames, fluxos e descodificação. Isso não prova sozinho que a imagem ficou visualmente melhor.

### 51. Como proteges o original?

Há validações para não usar a origem como destino, temporários e confirmação da saída. Ao substituir um destino existem mecanismos de proteção/restauro.

### 52. Como funciona o download?

A aplicação chama yt-dlp e ferramentas auxiliares com um endereço validado. O resultado também depende do site e dos conteúdos que este disponibiliza.

### 53. Funciona sem internet?

O processamento analisado usa ferramentas locais. O download por URL precisa de internet; não devo prometer funcionamento offline de todas as funções.

### 54. Porque precisa de GPU?

Os motores de IA distribuídos usam GPU através de Vulkan. Isso exige suporte e driver adequados; nem toda a aplicação trabalha na GPU.

### 55. O que farias se a demonstração falhasse?

Mostraria uma gravação ou resultado preparado, identificando-o como tal. Guardaria o erro para investigar, sem inventar a causa no momento.

## Se sobrar tempo: aprofundamento — 56 a 75

### 56. O que é um teste de regressão?

É um teste que ajuda a detetar se um comportamento volta a falhar depois de uma alteração. Exemplo atual: silenciar áudio ligado não deve esconder o vídeo.

### 57. Quantos testes existem?

O CMake regista seis testes CTest e há scripts adicionais. A contagem não diz quantos cenários existem nem prova que foram executados nesta revisão.

### 58. Porque usar ferramentas falsas em testes?

Permitem provocar erros e cancelamentos de forma repetível. Não substituem testar os motores e codecs reais.

### 59. Porque usar vídeos sintéticos?

Porque conhecemos os tempos, cores e sons esperados. Isso facilita medir falhas; vídeos reais continuam necessários.

### 60. Muitos testes aprovados significam cobertura total?

Não. Mostram os cenários exercitados. Medir cobertura exige um critério próprio e nem isso prova ausência de erros.

### 61. O que são timestamps?

São indicações do momento em que imagens ou áudio devem ser apresentados. São importantes para duração e sincronização.

### 62. O que são CFR e VFR?

Cadência constante e variável. O processamento normaliza a saída; ignorar os intervalos reais da origem pode produzir erros.

### 63. Porque pode haver diferença de um fotograma?

Porque a saída precisa de um número inteiro de imagens. É necessário escolher e verificar o arredondamento dos tempos.

### 64. Porque a cache pode causar uma prévia errada?

Se não distinguir todas as opções relevantes, pode reutilizar uma imagem antiga. A orientação foi um exemplo documentado desse problema.

### 65. Como evitas apresentar uma prévia antiga?

O editor cancela trabalho anterior e verifica se os resultados ainda pertencem ao pedido atual. Só terminar sem erro não torna uma resposta antiga válida.

### 66. Desfazer fica guardado para sempre?

Não. O histórico das camadas é limitado e é limpo ao definir o projeto no editor. Não é um histórico universal de toda a aplicação.

### 67. Guardar projeto significa ter um ficheiro de montagem?

Neste código, saveProject conserva opções em memória. Não devo confundir isso com guardar a montagem completa num documento em disco.

### 68. Porque mais resolução e FPS exigem recursos?

Há mais imagens e mais píxeis a tratar. O tempo real também depende do modelo, do disco, da memória e do equipamento.

### 69. O que mudarias para vídeos longos?

Primeiro mediria consumo de memória, disco e tempo por etapa. Depois escolheria a melhoria com base no ponto que realmente limita o trabalho.

### 70. Um ficheiro temporário protege contra tudo?

Não. Ajuda a evitar entregar uma saída incompleta, mas não garante recuperação perante qualquer falha de energia ou avaria.

### 71. O que prova um hash SHA-256?

Permite comparar os bytes com uma referência. Não prova ausência de bugs, autoria nem licenças resolvidas.

### 72. Podes distribuir tudo comercialmente já?

Não devo afirmar isso: os avisos do projeto ainda registam pendências de distribuição. Também faltaria demonstrar a maturidade adequada ao uso prometido.

### 73. É melhor que um programa comercial?

Não tenho comparação controlada que prove superioridade. Posso explicar a proposta e como faria uma comparação justa.

### 74. Como sabes que o exe corresponde ao código?

Identifico a compilação, as fontes e o ambiente, e ensaio esse executável. O instalador antigo não acompanha automaticamente as alterações.

### 75. O que aprendeste e o que melhorarias?

Escolhe uma aprendizagem verdadeira e mostra um exemplo. Propõe uma melhoria ligada a um problema observado, com uma forma clara de verificar o resultado.

## Como verificar se já sabes

- **Ainda não sei:** preciso de ler a resposta.
- **Já entendi:** explico sem ler e dou um exemplo.
- **Consigo defender:** respondo também a “porquê?” e “e se acontecer outra coisa?”.

Não saber uma resposta não obriga a inventar: “Não testei esse cenário. Começaria por verificar…” e indica um passo concreto.

As respostas foram confrontadas com [ProcessingOptions](../../src/core/ProcessingOptions.h), [LayerEditor](../../src/ui/LayerEditor.h), [LayerComposition](../../src/processing/LayerComposition.h), [ProcessingJob](../../src/processing/ProcessingJob.cpp), [ExportDialog](../../src/ui/ExportDialog.cpp), [CMake](../../CMakeLists.txt), [testes](../../tests/) e [avisos de terceiros](../../THIRD_PARTY_NOTICES.md). O [guia](guia-tecnico.md) identifica as mudanças de 09/10. Foi feita leitura, não uma nova execução dos testes.
