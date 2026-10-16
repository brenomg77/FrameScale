# Tour do programa

O tour aparece automaticamente uma vez por perfil de utilizador. A preferência
`onboarding/tourSeen` é gravada ao apresentar o convite: concluir, ignorar ou
interromper não provoca outro convite no próximo arranque. Atualizações e
reinstalações que preservam as preferências também preservam esta escolha.

Pode ser repetido em **Definições → Geral → Tour**. O percurso tem sete etapas,
com destaques das áreas da aplicação, progresso, voltar, avançar e confirmação
de saída. Os atalhos vêm das preferências atuais. Sem um vídeo aberto, a etapa
da timeline mostra um exemplo identificado como tal.

O tour não abre arquivos, não muda opções de processamento e não inicia
exportações. Pausa a reprodução e restaura a aba do painel ao terminar.

Os cartões usam `GlassMaterial`, respeitando o tema e a preferência de
transparência. O material recebe uma região desfocada do programa por baixo
do cartão. Os destaques acompanham o layout e respeitam os limites visíveis;
o da timeline exclui a margem da sombra. A abertura usa a logo de fundo preto
e a logo escrita. As instruções incluem copiar/colar arquivos e cortar o
intervalo de exportação; o atalho de ajustar à janela não aparece no tour.

Implementação: `src/ui/ProgramTour.h` e `MainWindow::startTour()`.
`FrameScaleProgramTourTests` verifica as decisões de primeira abertura,
persistência, repetição, navegação e saída, sem mostrar janelas ou capturar pixels.
