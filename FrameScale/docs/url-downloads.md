# Download por URL

Menu Ficheiro/Arquivo > Baixar vídeo por URL. O utilizador indica o link e a pasta. Depois de baixar, **Editar agora** abre o ficheiro como projeto; **Concluir** mantém o vídeo guardado sem o abrir.

O download usa yt-dlp 2026.08.19, Deno 2.9.7 e o FFmpeg distribuídos, sem configurações externas, plugins ou cookies automáticos. Aceita URLs HTTP/HTTPS e limita playlists ao primeiro item. Ficheiros intermédios ficam numa subpasta temporária no destino, sem substituir ficheiros existentes. O cancelamento encerra a árvore do processo no Windows e limpa os temporários. É escolhida a melhor combinação de vídeo/áudio disponível, sem recodificação; streams separados são unidos em MKV.

Os binários e os respetivos hashes estão em `runtime/manifest.json`, verificados por `scripts/verify-runtime.ps1` antes do empacotamento. O teste anterior usou um servidor HTTP local, vídeo sintético, progresso, comparação SHA-256 e erro HTTP 404. Não demonstra compatibilidade atual com todos os serviços externos: os sites podem exigir autenticação ou alterar os extratores. Não se deve prometer suporte universal na apresentação.

Referência do componente: [projeto yt-dlp](https://github.com/yt-dlp/yt-dlp).
