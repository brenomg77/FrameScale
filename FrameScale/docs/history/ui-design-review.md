# Revisão da interface — setembro de 2026

Direção: manter o tema Studio e oferecer Apple Glass como alternativa. O vídeo é o centro da interface; materiais translúcidos ficam nos controles e janelas auxiliares. A aproximação em Qt/Windows não é o material nativo do macOS.

## Antes de implementar

- A prévia convertia o vídeo inteiro antes de ficar disponível. Mostrar o primeiro quadro cedo e preparar pequenos trechos navegáveis evita bloquear a abertura pelo tamanho do vídeo.
- A barra misturava progresso ponderado do pipeline com frames de uma etapa. Mostrar frames concluídos da etapa identificada, com denominador conhecido; preparação sem contagem usa estado indeterminado.
- Presets tinham dois caminhos de gravação. Unificar em arquivos `.fspreset`, com migração dos presets existentes e importação na lista.
- Controles de codificação tinham nomes internos e campos principais escondidos na tabela. Destacar formato, codec, CRF e velocidade; recolher os limites avançados.

## Tokens e composição

Espaçamento 4/8/12/16/24 px; controles de pelo menos 28 px; fonte Segoe UI 10 pt; títulos 16–24 px. Foco azul de 2 px, cantos 6 px no Studio e 10 px no Glass. Sem animações decorativas ou blur sobre os frames.

| Papel | Studio / Glass escuro | Glass claro |
|---|---|---|
| Conteúdo | #181818 | #181818 (vídeo) |
| Superfície de controle | #292929 | #F3F5F9 |
| Texto principal | #F5F5F7 | #20242C |
| Texto secundário | #B8BEC9 | #525B69 |
| Ação | #0968CF com branco | #0968CF com branco |

Contrastes WCAG calculados em sRGB: texto principal escuro 13.36:1; texto secundário escuro 7.79:1; principal claro 14.25:1; secundário claro 6.29:1; branco sobre ação 5.39:1. O vídeo permanece neutro nos dois modos. Superfícies translúcidas recebem base opaca para preservar a leitura.

Estrutura: menu compacto → abas de projeto → preview e inspector → timeline. Boas-vindas é uma janela sobre o editor vazio. Inspector: Processing Settings / Encoder Options; rodapé com Ver fila e Add to Render Queue. General agrupa idioma e aparência; presets ficam no rodapé da codificação.

## Referências aplicadas

Skill: [apple-design](C:/Users/Admin/.agents/skills/apple-design/SKILL.md). Referências locais em `references/hig/`: accessibility.md (Vision: contraste, legibilidade e teclado), layout.md (hierarquia e redimensionamento), typography.md e color.md (papéis semânticos), designing-for-macos.md (convenções de desktop), liquid-glass.md e materials.md (vidro na camada funcional), windows.md (janelas auxiliares), settings.md (General conciso), loading.md (mostrar conteúdo cedo), progress-indicators.md (progresso mensurável) e pointing-devices.md (cursores de mão e redimensionamento).


## Verificação final

A diretriz `loading.md › Best practices` diz: “Show something as soon as possible.” A prévia agora apresenta um frame inicial e prepara segmentos de até oito segundos. `progress-indicators.md › Best practices` orienta progresso mensurável; a barra e o rótulo usam os mesmos frames concluídos da etapa.

Revisão visual por capturas em 1440 × 900 nos temas Studio, Glass claro e Glass escuro. Corrigidos contraste da régua, ícones na aparência clara e seleção de linhas da fila. Não foi realizada uma auditoria com leitor de tela. O tema evita animação e desfoque contínuo, mantendo o conteúdo do vídeo opaco.

Validação: quatro suites CTest, fluxo nativo de projetos/presets/fila, Space, zoom e navegação entre segmentos de um vídeo 4K de 20 segundos. Exportações reais H.264/MP4, H.265/MKV e ProRes/MOV verificadas com FFprobe: oito frames, metadados de origem preservados, CRF novo 23 nos codecs que usam CRF; ProRes usa perfil HQ e remove CRF antigo.


## Revisão solicitada em 16 de setembro — plano antes dos ajustes

Problemas observados: Glass era apenas uma troca de cores; as abas competiam com o título; aviso de proxy deslocava a timeline; CRF e opções avançadas tinham hierarquia confusa; boas-vindas animava inclusive quando já estava oculta.

Direção: uma moldura discreta para o vídeo e painéis funcionais com material fosco, luminosidade controlada e borda de luz. O desfoque usa apenas uma miniatura estática, nunca frames contínuos. Conteúdo de vídeo permanece opaco. Studio mantém sua aparência.

Tokens: espaçamento 4/8/12/16/24; cantos 16 em painéis, 8 em controles; fonte de sistema 10 pt; campos 28 px e ações 32 px. Claro: fundo #EDF0F5, painel #F7F8FB, texto #20242C, secundário #525B69. Escuro: fundo #171A20, painel #272D38, texto #F5F5F7, secundário #D0D6E0. Ação #0968CF/branco. Foco de teclado sólido 2 px; clique não desenha retângulo pontilhado.

Regular: [abas de ficheiro                         ][Processamento | Codificação]
         [                 vídeo                 ][saída de vídeo              ]
         [                              62%       ][opções avançadas            ]
         [timeline                                ][presets / ações             ]
Compacto: mesma divisão, painel 310 px; títulos elidem no meio; opções têm uma única rolagem e presets ficam no rodapé.

Assinatura: janela de boas-vindas converge para a área do documento em 180 ms, somente ao abrir o primeiro ficheiro. Redução de movimento remove a transição; redução de transparência/alto contraste usa material opaco. Zoom fica na borda inferior direita, mostrando só a percentagem.

Referências: `materials.md › Liquid Glass`: “Use Liquid Glass effects sparingly.”; `motion.md › Best practices`: “Make motion optional.”; `loading.md › Best practices`: “Show something as soon as possible.” Aplicação: vidro na camada funcional, transição com propósito e prévia navegável durante a preparação.


## Verificação desta revisão

- Material fosco gerado de uma miniatura 24×24, com difusão e dessaturação. Sem leitura de frames contínua e sem desfoque do vídeo. Preferência Transparência em Geral; por padrão acompanha o Windows. Alto contraste força superfície opaca.
- Contraste calculado contra extremos conservadores do material: texto claro sobre painel escuro ≥6,57:1; texto secundário escuro atualizado para #D0D6E0; texto principal sobre material claro ≥10,70:1, secundário ≥4,72:1; ação azul/branco 5,39:1.
- Timeline conserva duração e posição pendente durante recriação do player. A última solicitação de trecho prevalece; avisos não alteram a geometria. Zoom usa dimensões originais, incluindo rotação de 90°.
- Quatro suítes CTest aprovadas. Fluxo nativo aprovado: janela inicial, presets portáteis, projetos, idioma sem reinício, alternância sem autoplay, zoom/Space, carregamento de trechos e layouts 1440×900 e 900×620. Capturas em `build/review-followup-captures` e `build/review-final-captures`.
- Matriz real dos motores: 20/20 cenários aprovados em 16/09/2026, incluindo imagens, vídeos, upscale combinado com interpolação, VFR, áudio WMA→AAC e rotação. Relatório em `build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug/media-validation/20260916-080759-dbbef6049ab14fcfb3bae0e0fb71943f/summary.json`.
- Biblioteca do MediaInfo instalada: H.264 e H.265 expõem `crf=23.0` nas configurações do codificador; título/comentário da origem preservados. ProRes HQ preserva esses campos e remove o CRF anterior. Relatórios e arquivos em `build/final-media-check/final-*`.

Limites: o material é desenhado por Qt no Windows, não é o Liquid Glass nativo do macOS. Não se realizou auditoria com leitor de tela. A cópia de metadados depende do contêiner e não garante EXIF/XMP completos em todos os formatos de imagem. Não há afirmação de desempenho geral baseada nas pequenas fixtures de teste.
