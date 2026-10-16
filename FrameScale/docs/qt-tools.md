# Ferramentas e módulos Qt utilizados

Inventário conferido no código e na configuração de compilação em 16/10/2026.

## Ambiente de desenvolvimento

- **Qt Creator**: IDE instalada para abrir o projeto CMake, editar, compilar e depurar.
- **Qt 6.11.2 com MinGW 13.1 (64 bits)**: kit usado na compilação Windows desta entrega.
- **C++17, CMake e Ninja**: linguagem, configuração e execução da compilação.
- **moc**: geração do código do sistema de metaobjetos, sinais e slots, integrada ao CMake.
- **rcc**: incorporação de ícones, logótipos e traduções JSON nos recursos `.qrc`.
- **windeployqt / Qt CMake deployment**: cópia das bibliotecas e plugins necessários para executar no Windows sem instalar o ambiente de desenvolvimento. O projeto usa `qt_generate_deploy_app_script` e `cmake --install`.

O workflow Linux usa Qt **6.8.3**, GCC, CMake e Ninja. A versão mínima declarada no CMake é Qt 6.5; os ambientes efetivamente utilizados são os indicados acima.

## Módulos usados pela aplicação

| Módulo | Utilização no FrameScale |
| --- | --- |
| Qt Core | Eventos, sinais e slots, processos externos (`QProcess`), preferências (`QSettings`), JSON, temporizadores, ficheiros e tarefas em segundo plano. |
| Qt Gui | Imagens, pixmaps, ícones, fontes, desenho com `QPainter` e caminhos usados nos controles e superfícies Liquid Glass. |
| Qt Widgets | Janelas, diálogos, layouts, menus, botões, abas, timeline e interface do instalador. |
| Qt Multimedia | Reprodução e controlo de vídeo/áudio com `QMediaPlayer`, `QVideoSink`, `QAudioOutput` e `QAudioSink`. |
| Qt MultimediaWidgets | Apresentação de vídeo através de `QVideoWidget`. |
| Qt Network | Pedidos HTTP/HTTPS e respostas de rede no fluxo de abertura por URL. |

Widgets, Multimedia, MultimediaWidgets e Network são dependências diretas no `CMakeLists.txt`; Core e Gui são também utilizadas no código e vêm pelas dependências desses módulos. O pacote inclui Qt SVG e plugins de imagem para carregar os recursos gráficos, além dos plugins de plataforma, multimédia e TLS necessários.

## Como a interface é construída

A interface é escrita em **C++ com Qt Widgets**, layouts e desenho personalizado. Os temas combinam folhas de estilo Qt, `QPainter`, efeitos e animações. As janelas de vidro integram ainda APIs nativas do Windows para a composição e o recorte.

Esta implementação não utiliza QML/Qt Quick nem formulários `.ui` do Qt Designer. As traduções da aplicação são ficheiros JSON carregados pelo tradutor do programa; não são catálogos `.ts` do Qt Linguist. As traduções do próprio Qt podem acompanhar o pacote.

FFmpeg, Real-ESRGAN, Real-CUGAN, RIFE, mpv/Anime4K, yt-dlp e Deno são ferramentas externas integradas pelo FrameScale, não módulos Qt. Os avisos e licenças estão em [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
