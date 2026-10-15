# Linux x86_64

O pacote Linux é um AppImage para distribuições x86_64 com glibc 2.39 ou mais recente, como Ubuntu 24.04. É uma versão alpha: a compilação e os testes automáticos são feitos no Ubuntu 24.04; outros ambientes gráficos e drivers ainda precisam de validação em máquinas reais.

## Executar

Baixe `FrameScale-0.1.0-Linux-x86_64.AppImage` e o arquivo `.sha256` na release existente.

```bash
sha256sum -c FrameScale-0.1.0-Linux-x86_64.AppImage.sha256
chmod +x FrameScale-0.1.0-Linux-x86_64.AppImage
./FrameScale-0.1.0-Linux-x86_64.AppImage
```

Se FUSE não estiver disponível, extraia o pacote e execute:

```bash
./FrameScale-0.1.0-Linux-x86_64.AppImage --appimage-extract
./squashfs-root/AppRun
```

O pacote inclui Qt 6.8.3, FFmpeg/ffprobe, mpv, Real-ESRGAN, Real-CUGAN, RIFE, shaders Anime4K, modelos e componentes de abertura por URL. As funções de IA precisam de um driver Vulkan compatível. O driver da GPU e componentes básicos do sistema não são incluídos. A aparência translúcida depende do compositor Linux; o desfoque nativo usado no Windows não está disponível no Linux.

## Compilar e empacotar

O workflow [Linux release package](../../.github/workflows/linux-release.yml) instala Qt, prepara os motores Linux, compila o programa e executa testes sem abrir janelas. Depois, usa linuxdeploy e seu plugin Qt para gerar o AppImage. O pacote é enviado como artefato do workflow; anexá-lo à release é uma operação separada, sem criar uma versão nova.

As origens e os hashes dos downloads ficam em `usr/bin/runtime/manifest.json` dentro do AppImage. Os avisos e as licenças ficam em `usr/share/doc/framescale/`. Os motores são ferramentas nativas Linux, não executáveis Windows via Wine.
