# FrameScale 0.1.0

Aplicação Windows para edição, ampliação e interpolação de vídeos e imagens, desenvolvida em C++ e Qt.

**Estado: alpha, em desenvolvimento.** Ainda não existe uma versão estável.

## Código e desenvolvimento

O código está em [`FrameScale/`](FrameScale/). Consulte as [instruções de compilação](FrameScale/README.md) e os [avisos de terceiros](FrameScale/THIRD_PARTY_NOTICES.md).

Este repositório começa com uma cópia do código atual. O histórico anterior, que inclui grandes binários e arquivos locais, permanece no computador original.

As ferramentas de processamento e os modelos não estão incluídos no Git. Os arquivos e hashes necessários estão em [`runtime/manifest.json`](FrameScale/runtime/manifest.json). É possível compilar a interface sem esses arquivos, mas o processamento e a criação de instaladores exigem as dependências completas.

Na máquina de desenvolvimento original, copie as pastas `FrameScale/runtime/bin` e `FrameScale/models` para os mesmos caminhos neste repositório. Valide-as com:

```powershell
cd FrameScale
powershell -NoProfile -File scripts/verify-runtime.ps1
```

Essas pastas são ignoradas pelo Git. Não envie executáveis, vídeos pessoais, resultados de testes ou instaladores como código-fonte.

## Versões

- Versão atual: **0.1.0**, em desenvolvimento.
- Os commits guardam alterações sem criar uma versão para download.
- Quando houver uma versão de teste pronta, poderá ser publicada uma pré-release como `v0.1.0-alpha.1`.
- A publicação automática de instaladores e a atualização dentro do programa ainda não estão configuradas.

## Licenças

As dependências e modelos conservam as respetivas licenças. Este repositório privado não concede uma licença pública sobre o código próprio. A distribuição pública será preparada separadamente, incluindo as obrigações aplicáveis às dependências.
