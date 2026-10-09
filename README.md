# FrameScale 0.1.0

Aplicação Windows para edição, ampliação e interpolação de vídeos e imagens, desenvolvida em C++ e Qt.

**Estado: alpha, em desenvolvimento.** Ainda não existe uma versão estável.

## Baixar e instalar

Abra a [pré-release 0.1.0-alpha.1](https://github.com/brenomg77/FrameScale/releases/tag/v0.1.0-alpha.1) e baixe **FrameScale-0.1.0-Windows-x64-Setup.exe** na lista de arquivos.

O instalador inclui a aplicação, Qt, ferramentas de processamento e modelos. **Não precisa de copiar pastas, instalar Python ou compilar o código para usar o programa.** Requer Windows x64; as funções de IA precisam de GPU e driver com suporte a Vulkan.

Esta é uma versão experimental. O instalador ainda não possui assinatura digital de editor. Os arquivos “Source code” disponibilizados pelo GitHub destinam-se a desenvolvimento, não à instalação.

## Código e desenvolvimento

O código está em [`FrameScale/`](FrameScale/). Consulte as [instruções de compilação](FrameScale/README.md) e os [avisos de terceiros](FrameScale/THIRD_PARTY_NOTICES.md).

Este repositório começa com uma cópia do código atual. O histórico anterior, que inclui grandes binários e arquivos locais, permanece no computador original.

As ferramentas de processamento e os modelos estão no instalador da release, mas não no histórico Git, para evitar centenas de megabytes de binários em cada cópia do código. Os arquivos e hashes necessários estão em [`runtime/manifest.json`](FrameScale/runtime/manifest.json). É possível compilar a interface sem esses arquivos, mas o processamento e a criação de instaladores exigem as dependências completas.

**Apenas para desenvolvimento:** depois de instalar o FrameScale, copie as pastas `bin/runtime/bin` e `bin/models` da instalação para `FrameScale/runtime/bin` e `FrameScale/models` neste repositório. Também pode usar as pastas correspondentes da máquina de desenvolvimento original. Valide-as com:

```powershell
cd FrameScale
powershell -NoProfile -File scripts/verify-runtime.ps1
```

Essas pastas são ignoradas pelo Git. Não envie executáveis, vídeos pessoais, resultados de testes ou instaladores como código-fonte.

## Versões

- Versão atual: **0.1.0**, em desenvolvimento.
- Os commits guardam alterações sem criar uma versão para download.
- Primeira versão de teste: `v0.1.0-alpha.1`.
- A publicação automática de instaladores e a atualização dentro do programa ainda não estão configuradas.

## Licenças

As dependências e modelos conservam as respetivas licenças; consulte os [avisos de terceiros](FrameScale/THIRD_PARTY_NOTICES.md), incluindo os limites documentados de proveniência. A disponibilização do código próprio para consulta não concede uma licença de reutilização: ainda não foi escolhida uma licença para o FrameScale.
