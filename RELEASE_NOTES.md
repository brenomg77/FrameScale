# FrameScale 0.1.0 Alpha 1

Atualizada em 16 de outubro de 2026, mantendo a mesma pr?-release.

## Instalador Windows

- Interface personalizada no tema Liquid Glass Claro, com janela compacta de 480 ? 310.
- Logo de fundo preto e logo escrita acima das a??es. Cada a??o abre diretamente sua tela.
- Instalar, reparar e desinstalar, com escolha da pasta e atalho opcional.
- Cantos do vidro ajustados e bot?o de fechar com destaque centralizado.
- Apenas um setup Windows, mantendo o nome habitual do download.

## Processamento de v?deo

- A prepara??o para IA converte fontes de 10 bits para PNG RGB de 8 bits por canal, compat?vel com os motores, evitando intermedi?rios RGB48 desnecess?rios. O formato de sa?da escolhido, incluindo ProRes de 10 bits, continua respeitado.
- Concorr?ncia da grava??o dos quadros limitada para reduzir picos de mem?ria.
- Falta de espa?o tempor?rio gera uma mensagem espec?fica; o erro fica vis?vel na fila e os diagn?sticos s?o guardados em log.
- Testado um trecho curto de uma fonte 2160 ? 3840 a 120 fps, ampliado com Real-CUGAN para 4320 ? 7680, em ProRes 422 HQ. Isso n?o representa uma valida??o de exporta??es longas em 8K; o fluxo ainda usa sequ?ncias tempor?rias e precisa de espa?o em disco.

Continuam inclu?das as corre??es anteriores de Preview de 10 bits, timeline, fila de exporta??o e caminhos dos motores no Linux.

## Downloads

- **Windows x64:** execute `FrameScale-0.1.0-Windows-x64-Setup.exe`.
- **Linux x86_64:** baixe `FrameScale-0.1.0-Linux-x86_64.AppImage`, marque como execut?vel e execute. Requer glibc 2.39 ou mais recente. [Instru??es para Linux](https://github.com/brenomg77/FrameScale/blob/main/FrameScale/docs/linux-package.md).

Os pacotes incluem Qt, ferramentas de m?dia, motores e modelos. As fun??es de IA precisam de driver Vulkan compat?vel. O AppImage ? experimental; o vidro depende do compositor Linux.

## Verifica??es

- Windows: cliques e navega??o das tr?s a??es do instalador; instala??o, atualiza??o e remo??o silenciosas; integridade dos modelos e bibliotecas; regress?es de processamento.
- Linux: compila??o e verifica??es pelo GitHub Actions. O pacote ainda precisa de testes em desktops e GPUs Linux reais.
- Arquivos `.sha256` acompanham os downloads.

Vers?o alpha, sem atualiza??o autom?tica. Instalador Windows sem assinatura digital. Os arquivos ?Source code? s?o para desenvolvimento.

[Avisos de terceiros](https://github.com/brenomg77/FrameScale/blob/main/FrameScale/THIRD_PARTY_NOTICES.md).
