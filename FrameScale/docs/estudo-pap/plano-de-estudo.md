# Por onde começar — estudo da PAP FrameScale

Atualizado em **9 de outubro de 2026**. Tens **1–2 horas por dia** e já conheces algumas classes. O plano começa hoje e termina a 31 de outubro: **23 dias**. Não precisas de recuperar uma sessão de ontem.

## O teu objetivo

Conseguir explicar estas cinco coisas com as tuas palavras:

1. Para que serve o FrameScale e quem o pode usar.
2. O que a aplicação faz e o que usa de outras ferramentas.
3. O que acontece entre abrir um vídeo e receber o resultado.
4. Como funcionam FPS, velocidade, áudio e camadas.
5. Como verificas o resultado e o que fazes quando há um erro.

Não precisas de decorar o código todo. Para o trabalho como programador, interessa conseguires **entender um pedido, explicar a solução, encontrar um erro e verificar uma correção**. É isso que vais praticar.

## Usa os documentos nesta ordem

- **[Guia técnico explicado](guia-tecnico.md):** aprende uma lição por dia. Começa aqui depois de leres esta página.
- **[Perguntas do júri](perguntas-do-juri.md):** começa apenas pelas primeiras 15. As restantes ficam para depois.
- **[Roteiro e simulados](roteiro-e-simulados.md):** usa quando já conseguires explicar o funcionamento básico.

Os nomes de funções e ficheiros ficam no fim do guia. Primeiro entende a ideia; depois procura onde ela está no código.

## Uma sessão de 60 minutos

| Tempo | O que fazer |
|---|---|
| 10 minutos | Responder sem notas a duas perguntas de ontem. |
| 20 minutos | Ler a lição de hoje e observar o exemplo. |
| 20 minutos | Experimentar no programa ou seguir a função indicada. |
| 10 minutos | Explicar em voz alta o que aprendeste. |

Se tiveres duas horas, acrescenta uma experiência e um treino de perguntas. Não precisas de passar duas horas a ler.

**Sabes um tema quando consegues explicá-lo sem ler e dar um exemplo.** Se só reconheces as palavras, volta ao exemplo antes de abrir mais matéria.

## Calendário

A coluna “terminaste quando…” é o teu pequeno objetivo diário.

| Dia | Estudar | Terminaste quando… |
|---|---|---|
| 09/10 | Guia, lição 1: objetivo e contributo. | Explicas o projeto em 1 minuto. |
| 10/10 | Lição 2: ferramentas. | Distingues Qt, Qt Creator, VS Code e compilador. |
| 11/10 | Lição 3: resolução, FPS e velocidade. | Explicas por que 120 FPS não significa automaticamente câmara lenta. |
| 12/10 | Lição 4: IA e modelos. | Dizes o que RIFE faz e o que não foi criado por ti. |
| 13/10 | Rever lições 1–4 e perguntas 1–8. | Respondes sem ler a pelo menos seis dessas perguntas. |
| 14/10 | Lição 5: caminho de uma exportação. | Desenhas abrir → escolher → processar → verificar → guardar. |
| 15/10 | Lição 6: camadas e cortes. | Mostras corte, movimento e desfazer num vídeo curto. |
| 16/10 | Lição 6: vínculo, som e visibilidade. | Explicas por que silenciar áudio não esconde o vídeo ligado. |
| 17/10 | Lição 7: fila, pausa e cancelamento. | Distingues pausar, cancelar e guardar um trecho parcial. |
| 18/10 | Lição 8: testes e erros. | Explicas um teste e aquilo que ele não prova. |
| 19/10 | Lição 9: código, percurso A. | Localizas os dados de uma camada e calculas a duração. |
| 20/10 | Lição 9: código, percurso B. | Segues uma exportação sem tentar ler o ficheiro inteiro. |
| 21/10 | Lição 9: código, percurso C. | Mostras como uma alteração pode ser desfeita. |
| 22/10 | Rever as primeiras 15 perguntas. | Todas têm uma resposta tua, com um exemplo. |
| 23/10 | Lição 10: temas mais difíceis. | Explicas prévia, processo externo e limites do programa. |
| 24/10 | Perguntas 16–35: escolhe as dez mais difíceis para ti. | Corriges as respostas em que dependias das notas. |
| 25/10 | Perguntas 36–75, apenas como simulado. | Identificas cinco assuntos a rever, sem tentar decorar tudo. |
| 26/10 | Montar apresentação com o roteiro. | Tens uma sequência de slides e uma demonstração curta. |
| 27/10 | Ensaiar o programa e escolher o executável certo. | Repetes a demonstração duas vezes e tens uma gravação de reserva. |
| 28/10 | Simulado simples e simulado técnico. | Explicas com clareza para alguém que não conhece o projeto. |
| 29/10 | Apresentação completa com cronómetro. | Respeitas o tempo indicado pela escola. |
| 30/10 | Corrigir as três maiores dúvidas. | Relatório, slides e demonstração contam a mesma história. |
| 31/10 | Revisão leve das 15 essenciais. | Explicas o projeto sem depender de um texto decorado. |

31 de outubro é o teu prazo de estudo; não estou a assumir que é a data da apresentação. Confirma já com a orientadora o tempo disponível, os critérios e o prazo de entrega do relatório.

## Se perderes um dia

Junta duas sessões de revisão e deixa as perguntas avançadas para depois. Mantém o treino da demonstração e das 15 essenciais. É melhor explicar bem o funcionamento principal do que decorar muitos termos sem os compreender.

## O que foi atualizado no material

O código consultado em 09/10 declara **FrameScale 0.1.0 alpha**, ou seja, em desenvolvimento. A exportação tem Pausar/Retomar. Silenciar áudio e ocultar vídeo são ações individuais; velocidade, corte, movimento e exclusão continuam a respeitar o grupo ligado.

Estas informações foram conferidas no código e nos testes existentes. Não executei novamente a aplicação nem os testes para escrever esta revisão. Os detalhes e as fontes estão no fim do [guia](guia-tecnico.md).
