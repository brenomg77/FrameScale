# 🎬 Demonstrações do FrameScale

Resultados produzidos pelo fluxo de processamento do FrameScale, com os motores e modelos distribuídos no instalador. Clique em **Assistir ao vídeo** para abrir o MP4; as imagens animadas permitem uma comparação rápida.

## 🧪 Como estes exemplos foram feitos

A cena é uma **ilustração sintética criada para esta documentação**, com movimento, contornos, gradientes e pequenos detalhes. A fonte tem 384 × 216 pixels, 8 FPS e 3 segundos. Para ampliação, foi preparada uma versão de 192 × 108 pixels; para restauração e redução de ruído, foi acrescentado ruído à entrada.

As saídas foram geradas pelo `ProcessingJob` do FrameScale, por meio do executável de validação `FrameScaleProcessingSmoke`. O FFmpeg foi usado depois para montar as comparações, legendas e prévias animadas. Os vídeos desta galeria demonstram **resultados do processamento**, não uma gravação dos controles da interface.

Os dois lados são apresentados em áreas de mesmo tamanho para facilitar a comparação. A ampliação visual da entrada usa vizinho mais próximo; a apresentação da saída usa redimensionamento Lanczos. Consulte a legenda para a resolução e os parâmetros reais. Nos exemplos com durações diferentes, a montagem repete os trechos para completar a apresentação.

Estas cenas ajudam a identificar diferenças e reproduzir o fluxo; resultados em filmagens, fotografias ou anime reais podem variar. A galeria não é um benchmark de desempenho.

## 📥 Arquivos de origem

- [Cena original — 384 × 216, 8 FPS](media/source-384x216-8fps.mp4)
- [Entrada reduzida — 192 × 108, 8 FPS](media/source-192x108-8fps.mp4)
- [Entrada com ruído — restauração e redução de ruído](media/source-noisy.mp4)
- [Entrada com faixas de cor — filtro de banding](media/source-bands.mp4)
- [Parâmetros e metadados das demonstrações](media/manifest.json)

<details>
<summary><strong>Reproduzir as demonstrações em um ambiente de desenvolvimento</strong></summary>

Os scripts [build_readme_demos.py](scripts/build_readme_demos.py) e [finish_readme_demos.py](scripts/finish_readme_demos.py) criam a cena e executam o fluxo de processamento. Eles exigem Python com Pillow, a compilação de desenvolvimento `Desktop_Qt_6_11_2_MinGW_64_bit_Debug`, o executável `FrameScaleProcessingSmoke` e os modelos e ferramentas completos. Ajuste os caminhos de Qt conforme seu ambiente.

```powershell
python docs/scripts/build_readme_demos.py
python docs/scripts/finish_readme_demos.py
```

As prévias MP4/GIF são codificações de apresentação. Os logs e os resultados intermediários ficam na pasta de build.

</details>


## 🔍 Ampliação

### Ampliação · Real-ESRGAN

192 × 108 → 384 × 216 · escala 2×

[![Ampliação · Real-ESRGAN](media/upscale-esrgan.gif)](media/upscale-esrgan.mp4)

[▶ Assistir ao vídeo MP4](media/upscale-esrgan.mp4)

### Ampliação · Real-CUGAN

192 × 108 → 384 × 216 · escala 2×

[![Ampliação · Real-CUGAN](media/upscale-cugan.gif)](media/upscale-cugan.mp4)

[▶ Assistir ao vídeo MP4](media/upscale-cugan.mp4)

### Ampliação · Anime4K

192 × 108 → 384 × 216 · escala 2×

[![Ampliação · Anime4K](media/upscale-anime4k.gif)](media/upscale-anime4k.mp4)

[▶ Assistir ao vídeo MP4](media/upscale-anime4k.mp4)

### Ampliação + interpolação

192 × 108 / 8 FPS → 384 × 216 / 16 FPS

[![Ampliação + interpolação](media/combined.gif)](media/combined.mp4)

[▶ Assistir ao vídeo MP4](media/combined.mp4)

## 🎞️ Interpolação

### Interpolação · RIFE

8 FPS → 16 FPS · sem alterar a resolução

[![Interpolação · RIFE](media/interpolation.gif)](media/interpolation.mp4)

[▶ Assistir ao vídeo MP4](media/interpolation.mp4)

## 🧩 Restauração

### Restauração · Real-ESRGAN Plus

Restauração em 1× · modelo Plus · origem com ruído

[![Restauração · Real-ESRGAN Plus](media/restore-plus.gif)](media/restore-plus.mp4)

[▶ Assistir ao vídeo MP4](media/restore-plus.mp4)

### Restauração · Real-ESRGAN Anime

Restauração em 1× · modelo Anime · origem com ruído

[![Restauração · Real-ESRGAN Anime](media/restore-anime.gif)](media/restore-anime.mp4)

[▶ Assistir ao vídeo MP4](media/restore-anime.mp4)

### Restauração · Real-CUGAN SE

Restauração em 1× · modelo SE · origem com ruído

[![Restauração · Real-CUGAN SE](media/restore-cugan.gif)](media/restore-cugan.mp4)

[▶ Assistir ao vídeo MP4](media/restore-cugan.mp4)

## 🎛️ Filtros de imagem

### Filtro · redução de ruído

Somente filtros · redução de ruído: 65

[![Filtro · redução de ruído](media/denoise.gif)](media/denoise.mp4)

[▶ Assistir ao vídeo MP4](media/denoise.mp4)

### Filtro · nitidez

Somente filtros · nitidez: 55

[![Filtro · nitidez](media/sharpen.gif)](media/sharpen.mp4)

[▶ Assistir ao vídeo MP4](media/sharpen.mp4)

### Filtro · reduzir blocos

Somente filtros · reduzir blocos: 70

[![Filtro · reduzir blocos](media/deblock.gif)](media/deblock.mp4)

[▶ Assistir ao vídeo MP4](media/deblock.mp4)

### Filtro · reduzir faixas de cor

Somente filtros · reduzir banding: 70

[![Filtro · reduzir faixas de cor](media/deband.gif)](media/deband.mp4)

[▶ Assistir ao vídeo MP4](media/deband.mp4)

### Filtro · grão

Somente filtros · grão: 40

[![Filtro · grão](media/grain.gif)](media/grain.mp4)

[▶ Assistir ao vídeo MP4](media/grain.mp4)

### Filtro · contraste

Somente filtros · contraste: +45

[![Filtro · contraste](media/contrast.gif)](media/contrast.mp4)

[▶ Assistir ao vídeo MP4](media/contrast.mp4)

### Filtro · intensidade das cores

Somente filtros · intensidade das cores: +65

[![Filtro · intensidade das cores](media/vibrance.gif)](media/vibrance.mp4)

[▶ Assistir ao vídeo MP4](media/vibrance.mp4)

## ✂️ Timeline e movimento

### Corte na timeline

Intervalo: 1 s → 2 s · saída com 8 frames

[![Corte na timeline](media/trim.gif)](media/trim.mp4)

[▶ Assistir ao vídeo MP4](media/trim.mp4)

### Velocidade · movimento lento

Velocidade: 0,5× · saída com 6 segundos

[![Velocidade · movimento lento](media/speed.gif)](media/speed.mp4)

[▶ Assistir ao vídeo MP4](media/speed.mp4)

### Redução de FPS

8 FPS → 4 FPS · mesma duração

[![Redução de FPS](media/fps-reduction.gif)](media/fps-reduction.mp4)

[▶ Assistir ao vídeo MP4](media/fps-reduction.mp4)

### Composição de camadas

Vídeo base + camada espelhada a partir de 1,5 s + áudio

[![Composição de camadas](media/layers.gif)](media/layers.mp4)

[▶ Assistir ao vídeo MP4](media/layers.mp4)

## 🔄 Orientação

### Rotação de vídeo

Rotação: 90° · conteúdo ajustado para esta comparação

[![Rotação de vídeo](media/rotate.gif)](media/rotate.mp4)

[▶ Assistir ao vídeo MP4](media/rotate.mp4)

### Espelhamento de vídeo

Espelhamento horizontal

[![Espelhamento de vídeo](media/flip.gif)](media/flip.mp4)

[▶ Assistir ao vídeo MP4](media/flip.mp4)

## 📦 Exportação

### Exportação de áudio

WAV exportado pelo FrameScale · visualização da onda de áudio

[![Exportação de áudio](media/audio.gif)](media/audio.mp4)

[▶ Assistir ao vídeo MP4](media/audio.mp4)

### Exportação em GIF

Animação GIF exportada pelo FrameScale

[![Exportação em GIF](media/gif.gif)](media/gif.mp4)

[▶ Assistir ao vídeo MP4](media/gif.mp4)

### Sequência de imagens

24 arquivos PNG exportados pelo FrameScale · 8 FPS

[![Sequência de imagens](media/sequence.gif)](media/sequence.mp4)

[▶ Assistir ao vídeo MP4](media/sequence.mp4)
