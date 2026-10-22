# FrameScale 0.1.0 Alpha 1

Atualizada em 22 de outubro de 2026, mantendo a mesma pré-release.

## Atualização de 22 de outubro

- Instalador Windows com fundo claro estável, sem interferência das cores das janelas por trás.
- Unificados os estados dos botões, campos e caixas de seleção; removida a camada animada que se sobrepunha ao desenho dos botões.
- Verificados foco, hover, pressão, navegação entre ações e limpeza da barra de progresso em diferentes escalas de ecrã.
- Corrigidos os contornos da barra de progresso do instalador.
- Corrigidas as ações dos presets de codificação.
- Corrigida a tradução da interface ao alternar entre idiomas.
- Recuperada documentação técnica da PAP e organizados os recursos gráficos e os scripts de desenvolvimento.

## Atualização de 19 de outubro

- Verificação de espaço livre no destino e nos temporários também durante a codificação final. A exportação é interrompida quando o espaço disponível fica abaixo de 512 MB; isso não é uma previsão do espaço total necessário para o trabalho.
- Falhas do FFmpeg com “No space left on device” passam a apresentar uma mensagem específica de falta de espaço.
- Barra do instalador desenhada pelo próprio programa para evitar resíduos da animação nativa nas superfícies transparentes.
- Dimensões e FPS da origem reposicionados junto da prévia, respeitando a área do painel lateral, com fundo discreto para manter o contraste.
- Balões de ajuda explicam que AnimeVideo v3 é recomendado para vídeos de anime e que Plus Anime é indicado principalmente para imagens e ilustrações, podendo ser usado em vídeo com processamento mais lento.
- Os balões recomendam comparar um trecho na Preview, pois a qualidade depende da origem; tradução inglesa atualizada.

## Correções mais recentes

- Removidas as opções Novo/Novo arquivo e o atalho Ctrl+N.
- Recorte das janelas sincronizado com o Qt para evitar cantos pretos nas atualizações das janelas translúcidas do Windows. A aparência ainda precisa de confirmação no computador que apresentou o problema.
- Reprodução de vídeos 4K/8K aceita o comando Play durante a preparação; Pausar cancela esse pedido.
- Prévia temporária limitada a 30 FPS e apresentação acelerada quando aplicável. Exportação e inspeção de fotogramas usam a origem.
- Testes locais com ProRes 2160×3840 e 4320×7680 a 120 FPS passaram na reprodução, troca de trecho e retorno na timeline. A preparação inicial levou cerca de 16 e 46 segundos, respetivamente; ainda podem existir esperas entre trechos.
- Documentação das ferramentas e módulos Qt utilizados, incluindo os kits Windows e Linux.

## Tour e navegação entre projetos

- Tour de sete etapas, apresentado uma vez na primeira abertura e disponível novamente em Definições → Geral → Tour.
- Cartões com Liquid Glass, logo de fundo preto e logo escrita; destaques ajustados às áreas visíveis dos controles.
- Explicações de abertura, copiar/colar arquivos, timeline, corte, melhorias, comparação e exportação. Atalhos acompanham as preferências do usuário.
- Textos separados por assunto e cartões dimensionados para exibir os parágrafos completos.
- Cada aba mantém a agulha, o zoom e o deslocamento. A última visualização fica na tela durante o carregamento, com transição curta quando a prévia está pronta.
- Cache limitado de miniaturas e formas de onda evita reconstruções desnecessárias ao alternar projetos.
- X das abas centralizados; removido o status duplicado do cabeçalho da fila de exportação.

## Instalador Windows

- Interface personalizada clara, com fundo estável e cantos arredondados, em janela compacta de 480 × 310.
- Logo de fundo preto e logo escrita acima das ações; cada ação abre diretamente sua tela.
- Instalar, reparar e desinstalar, com escolha da pasta e atalho opcional.
- Apenas um setup Windows, mantendo o nome habitual do download.

## Processamento de vídeo

- Preparação para IA com PNG RGB de 8 bits por canal, evitando intermediários RGB48 desnecessários em fontes de 10 bits. O formato de saída selecionado, incluindo ProRes de 10 bits, continua respeitado.
- Concorrência da gravação dos quadros limitada para reduzir picos de memória.
- Falta de espaço temporário gera uma mensagem específica; erros permanecem visíveis na fila e são registrados em log.
- Testado um trecho curto de uma fonte 2160 × 3840 a 120 fps, ampliado com Real-CUGAN para 4320 × 7680 em ProRes 422 HQ. Isso não representa uma validação de exportações longas em 8K; o fluxo ainda usa sequências temporárias e precisa de espaço em disco.

Continuam incluídas as correções anteriores de Preview de 10 bits, timeline e caminhos dos motores no Linux.

## Downloads

- **Windows x64:** execute `FrameScale-0.1.0-Windows-x64-Setup.exe`.
- **Linux x86_64:** baixe `FrameScale-0.1.0-Linux-x86_64.AppImage`, marque como executável e execute. Requer glibc 2.39 ou mais recente. [Instruções para Linux](https://github.com/brenomg77/FrameScale/blob/main/FrameScale/docs/linux-package.md).

Os pacotes incluem Qt, ferramentas de mídia, motores e modelos. As funções de IA precisam de driver Vulkan compatível. O AppImage é experimental; o vidro depende do compositor Linux.

## Verificações

- Testes de persistência, navegação e saída do tour; dimensões dos parágrafos e destaques; transição e estado dos projetos.
- Windows: integridade dos modelos e bibliotecas e verificação silenciosa da instalação, atualização e remoção.
- Linux: compilação e verificações pelo GitHub Actions. O pacote ainda precisa de testes em desktops e GPUs Linux reais.
- Arquivos `.sha256` acompanham os downloads.

Versão alpha, sem atualização automática. Instalador Windows sem assinatura digital. Os arquivos “Source code” são para desenvolvimento.

[Avisos de terceiros](https://github.com/brenomg77/FrameScale/blob/main/FrameScale/THIRD_PARTY_NOTICES.md).
