# Download por URL

Menu Ficheiro/Arquivo → **Abrir por URL**, também disponível na janela de boas-vindas. O utilizador indica um link HTTP/HTTPS de imagem ou vídeo e a pasta de destino. Depois de baixar, **Editar agora** abre o ficheiro como projeto; **Concluir** mantém o ficheiro guardado sem o abrir.

A aplicação tenta primeiro reconhecer uma imagem através de Qt Network e `QImageReader`. O link pode não ter extensão e pode redirecionar para a imagem. O percurso aceita imagens raster reconhecidas pelo Qt, exclui SVG/SVGZ e limita a resposta a 128 MiB. A imagem é guardada como `imagem.<extensão>`; se o nome existir, é acrescentado um sufixo numérico. Se a resposta não for reconhecida como imagem, a aplicação tenta o percurso de vídeo.

O download de vídeo usa yt-dlp, Deno e o FFmpeg distribuídos, sem configurações externas, plugins ou cookies automáticos. Limita playlists ao primeiro item e exclui transmissões em direto ou agendadas. Ficheiros intermédios ficam numa subpasta temporária no destino, sem substituir ficheiros existentes. O cancelamento interrompe o pedido de imagem ou o processo de vídeo; no Windows, encerra também a árvore do processo. É escolhida a melhor combinação de vídeo/áudio disponível, sem recodificação; streams separados são unidos em MKV.

As versões dos binários e os respetivos hashes estão em `runtime/manifest.json`, verificados por `scripts/verify-runtime.ps1` antes do empacotamento. `FrameScaleUrlImageTests` usa um servidor HTTP local para verificar uma imagem sem extensão, redirecionamento e preservação de nomes existentes. Os ensaios anteriores de vídeo também utilizaram um servidor local, vídeo sintético, progresso, comparação SHA-256 e erro HTTP 404. Esses cenários não demonstram compatibilidade com todos os serviços externos: os sites podem exigir autenticação ou alterar os extratores.

Referência do componente: [projeto yt-dlp](https://github.com/yt-dlp/yt-dlp).

O limite de 45 segundos aplica-se apenas à preparação do vídeo. O sinal
`before_dl:FS_READY:%(id)s` ou qualquer mensagem `FS_PROGRESS:` desativa esse
limite quando a transferência começa. Não há um limite total de 45 segundos
para baixar ou unir vídeo e áudio; o timeout de rede e as tentativas do yt-dlp
continuam ativos. Um literal sem campo de template em `--print` era interpretado
como nome de campo pelo yt-dlp e produzia `NA`, deixando o temporizador de
preparação ativo durante o download.

`FrameScaleUrlVideoTests` usa o yt-dlp distribuído e um servidor HTTP local:
baixa um MP4 sintético com uma hora de duração em uma transferência de mais de
45 segundos e verifica o conteúdo completo do ficheiro. O teste não baixa
vídeos externos e não usa ficheiros do utilizador.
