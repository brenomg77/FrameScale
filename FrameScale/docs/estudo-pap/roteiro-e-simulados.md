# Como apresentar o FrameScale

Atualizado em **09/10/2026**. [Plano](plano-de-estudo.md) · [Guia](guia-tecnico.md) · [Perguntas](perguntas-do-juri.md)

Primeiro confirma com a orientadora quanto tempo tens. O exemplo abaixo dura **10 minutos**; não é uma regra da escola. O prazo de 31 de outubro é o prazo de estudo que indicaste.

## A história que vais contar

A apresentação deve responder, nesta ordem:

1. Que problema quiseste resolver?
2. O que o programa permite fazer?
3. Como organizaste a solução?
4. Consegues mostrar que funciona?
5. Como testaste e o que aprendeste?

Não precisas de mostrar todos os botões nem todas as classes.

## Sete slides simples

| Slide | Tempo | O que dizer ou mostrar |
|---|---|---|
| 1. O problema | 1 min | Para quem é o programa e que tarefa facilita. |
| 2. O contributo | 1 min | O que pertence à aplicação e o que vem de terceiros. |
| 3. O funcionamento | 1 min | O desenho abrir → opções → processamento → verificação → ficheiro. |
| 4. Um conceito importante | 1 min | Diferença entre FPS e velocidade, com um exemplo de 10 segundos. |
| 5. Demonstração | 3 min | Uma edição curta e um resultado exportado. |
| 6. Testes e uma dificuldade | 2 min | Um problema concreto, como foi investigado e o que o teste verifica. |
| 7. Limites e aprendizagem | 1 min | O que aprendeste e o que ainda precisa de melhorar. |

Se tiveres 5 minutos, reduz a demonstração e explica só uma decisão técnica. Se tiveres 15, acrescenta um percurso curto no código e perguntas sobre ele. Não ocupes o tempo extra à espera de uma exportação longa.

## Uma abertura possível

> O FrameScale é uma aplicação local para editar e melhorar imagens e vídeos. O objetivo é reunir estas tarefas numa interface mais acessível. A aplicação usa ferramentas e modelos existentes, e o trabalho do projeto está na edição, integração, organização das exportações e verificação dos resultados.

Depois acrescenta a tua motivação verdadeira. Não inventes clientes, estudos de mercado ou resultados.

## Demonstração de três minutos

Prepara um vídeo local teu ou autorizado, com 3–5 segundos, movimento e som fáceis de reconhecer. Usa o mesmo exemplo nos ensaios.

1. **Abre o vídeo.** Aponta a resolução e os FPS.
2. **Mostra a edição.** Corta uma borda e usa Ctrl+Z. Explica que o original não foi cortado fisicamente.
3. **Mostra áudio e vídeo.** Silencia o áudio e confirma que o vídeo continua visível. Explica que essa ação é individual, mesmo com as camadas ligadas.
4. **Mostra a saída.** Faz uma exportação curta já ensaiada ou mostra um resultado previamente exportado, dizendo claramente que foi preparado antes.

Só demonstra Pausar/Retomar ao vivo se a tarefa durar o suficiente e já tiveres ensaiado. Não precisas de mostrar todas as funções novas. Explica que pausar não permite fechar a aplicação e continuar amanhã.

Para mostrar interpolação, podes usar um resultado preparado: “Este exemplo foi exportado anteriormente com estas opções para não gastar a apresentação à espera.”

Prepara também uma gravação curta. Se algo falhar, diz o que observaste, mostra a alternativa e guarda o erro para investigar. Não inventes uma causa imediata.

## Como responder sem te perderes

Usa três passos: **resposta direta → exemplo → limite**, quando o limite for relevante.

Pergunta: “Aumentar FPS faz câmara lenta?”

> Não por si só. Um vídeo de 10 segundos interpolado de 30 para 60 FPS continua com cerca de 10 segundos. Para o tornar lento, altero a velocidade, por exemplo para 0,5×, ficando com 20 segundos.

Pergunta: “Então o programa não tem erros?”

> Os testes verificam cenários concretos. Por exemplo, há um teste para confirmar que silenciar áudio ligado não esconde o vídeo. Isso não garante que todos os casos possíveis estejam livres de problemas.

Se não souberes: “Não confirmei esse cenário. Para o investigar, começaria por…” e indica o ficheiro, medição ou experiência relevante.

## Simulado 1 — explicar a alguém que não programa

Pede a alguém que faça estas perguntas sem te mostrar a ordem:

- Para que serve o programa?
- Quem o utilizaria?
- O que desenvolveste e o que já existia?
- O que é um fotograma?
- Qual a diferença entre aumentar resolução e aumentar FPS?
- Preciso do Qt Creator para o abrir?
- Qual é uma limitação?

Conseguiste responder sem palavras que a pessoa não compreendeu? Se não, troca a definição por um exemplo.

## Simulado 2 — explicar o código

Abre apenas os ficheiros necessários:

- Mostra `MediaLayer` e explica `in`, `out`, `start` e `speed`.
- Mostra onde começa uma exportação e onde a saída é verificada.
- Mostra `remember` e `undo` e explica que dados são guardados.
- Explica a diferença entre `setPaused` e `cancel`.
- Mostra um teste e diz o que ele não prova.

Os caminhos estão na lição 9 do [guia](guia-tecnico.md). Explica a intenção de cada trecho, sem ler todas as linhas em voz alta.

## Simulado 3 — perguntas difíceis

| Pergunta | O que a tua resposta deve mostrar |
|---|---|
| “É só uma interface para ferramentas dos outros?” | Explica edição, validação, fila, falhas e proteção de ficheiros; reconhece os motores externos. |
| “Usaste IA para programar?” | Conta o apoio real, as decisões que tomaste e o que consegues explicar. |
| “Porque houve erros depois da auditoria?” | Os testes têm alcance limitado; novos casos exigem reprodução e regressão. |
| “O relatório diz outra coisa.” | Reconhece a revisão anterior e apresenta a evidência da versão escolhida. Resolve as diferenças antes da defesa. |
| “É melhor que um produto comercial?” | Não há comparação controlada que permita afirmar isso. Explica como a farias. |
| “Posso fechar enquanto está pausado?” | Pausa não é gravação persistente da exportação. |

## Antes da apresentação

- Identifica o executável que vais mostrar. O instalador antigo pode não conter as alterações atuais.
- Usa a versão correta nos slides: o código consultado declara **0.1.0 alpha**, em desenvolvimento.
- Confere o programa na máquina prevista, com áudio e GPU adequados às funções escolhidas.
- Guarda os exemplos, as opções utilizadas e a gravação de reserva.
- Ensaia com cronómetro pelo menos duas vezes.
- Confirma relatório, anexos e critérios com a orientadora.

## O que precisa de estar coerente no relatório

O material de estudo foi atualizado; **o relatório não foi reescrito nesta tarefa**.

| Assunto | O que conferir antes de entregar |
|---|---|
| Versão | Usar a versão da compilação realmente apresentada. |
| Camadas | Explicar o editor atual, em vez de apenas a edição antiga de uma faixa. |
| Som e visibilidade | São individuais, mesmo com camadas ligadas. |
| Pausa | Existe Pausar/Retomar; não é continuação após fechar o programa. |
| Velocidade | Atua no grupo ligado; o áudio do compositor usa atempo para preservar o tom. |
| Testes | Há seis registos CTest; distinguir existência dos testes de resultados executados. |
| Resultados antigos | Identificar a data e versão das auditorias e ensaios citados. |
| Avaliações docentes | Integrar os registos autênticos indicados como pendentes; não os inventar. |

Fontes: [CMake](../../CMakeLists.txt), [LayerEditor](../../src/ui/LayerEditor.h), [ProcessingJob](../../src/processing/ProcessingJob.cpp), [relatório](../../../output/relatorio-pap/Relatorio_PAP_FrameScale_Breno_Amorim_2026.docx) e [correções documentadas](../correcoes-finais-2026-10-05.md). Esta revisão do material não executou novos testes nem gerou um instalador.

## Quando estás preparado?

Quando consegues explicar o objetivo em um minuto, mostrar o percurso principal, responder às 15 perguntas essenciais e reconhecer uma limitação. Nos últimos dias, trabalha nas dúvidas que ainda te impedem de fazer isso. Os detalhes menores ficam para depois.
