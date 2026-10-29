# Arquivos da área de transferência

As imagens e os vídeos colados no editor são guardados na pasta `Clipboard`
dos dados locais da aplicação. No Windows, o caminho padrão é
`%LOCALAPPDATA%\PAP\FrameScale\Clipboard`.

Imagens recebidas diretamente da área de transferência são guardadas em PNG.
Ficheiros copiados pelo Explorador são duplicados sem alterar os originais,
preservando nome, extensão e conteúdo. Cada colagem recebe uma subpasta
`Paste-...`, evitando conflitos entre nomes iguais. Colagens de ficheiros que
já estão em `Clipboard` reutilizam o ficheiro existente. A cópia é feita em
segundo plano para manter a interface disponível durante vídeos grandes.

A opção **Copiar** na preview de um vídeo gera um ficheiro do mesmo formato original (MP4, MOV, MKV, AVI, WebM ou GIF) do trecho selecionado,
com os cortes e as camadas aplicados, para colar em outro programa. O resultado
fica em `Clipboard\Copy-...\selection.<extensao original>`. Não usa os modelos de melhoria
de imagem da exportação. Copiar uma imagem coloca uma referência ao ficheiro
na área de transferência.

Os ficheiros aceites são mantidos ao fechar o programa e ao fazer novas
cópias, porque outras aplicações, abas ou tarefas de exportação podem usá-los.
Importações rejeitadas e cópias de vídeo falhadas ou canceladas removem somente
a sua subpasta de trabalho. Não se faz limpeza automática de cópias anteriores.

Os downloads por URL continuam na pasta escolhida em **Salvar em**, por padrão
a pasta Downloads do utilizador. Não são ficheiros da área de transferência.

O formato do contêiner é preservado; os trechos são recodificados para aplicar
os cortes e as camadas. Isso preserva o tipo de arquivo, mas não necessariamente
o codec original nem os bytes do vídeo. WebM usa VP9/Opus, AVI usa Motion
JPEG/PCM e GIF permanece animado sem áudio.

Validação dos seis formatos com FFmpeg e ffprobe reais:
`python FrameScale/tests/validate-copy-formats.py FrameScale/build/Release`.
