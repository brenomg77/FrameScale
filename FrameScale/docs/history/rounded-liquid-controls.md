# Controles e tipografia — 29/09/2026

Atualiza??o mais recente: SF Pro Rounded reativada nos textos e t?tulos, com os quatro pesos incorporados ao execut?vel. Bot?es de presets e fila compactados para 24 px de altura e largura pelo conte?do. Espa?amento entre s?mbolos e legendas aumentado; r?tulos de codifica??o continuam sem s?mbolos. O acabamento anterior permanece.

Atualização: a pedido do usuário, o acabamento descrito abaixo foi revertido.
Foram restaurados as cores, os contornos, os menus, os interruptores e as fontes
SF Pro Text/Display anteriores. Somente os símbolos dos controles foram mantidos,
além da correção das descrições de Contraste e Intensidade das cores. A SF Pro
Rounded não faz mais parte dos recursos do executável. O restante deste documento
registra a tentativa anterior, não a aparência atualmente aplicada.

Referência: as imagens de controles Liquid Glass enviadas pelo usuário, com
superfícies neutras, contornos claros, símbolos monocromáticos e cantos suaves.

- SF Pro Rounded Regular, Medium, Semibold e Bold incorporadas aos recursos.
  Os arquivos correspondem por SHA-256 aos quatro pesos da pasta de fontes
  indicada pelo usuário. A família interna é `SF Pro Rounded`. Texto e títulos
  usam a família Rounded, com fallback para as fontes já incluídas.
- Estilo compartilhado em `src/ui/LiquidControls.h`: botões e seletores neutros,
  azul nas seleções e ações principais, estados de foco, pressionado e
  desativado definidos para temas claro e escuro.
- Símbolos vetoriais desenhados com QPainter, independentes de fontes de emoji:
  controles de processamento, filtros, codificação, Preview, fila e exportação.
  Os textos e nomes acessíveis continuam disponíveis.
- Menus com cantos e seleção mais arredondados. Superfícies existentes recebem
  um contorno fino e iluminação suave, sem acrescentar outro painel interno.
- Preview conserva a largura limitada aos controles e o alinhamento com a
  timeline. O modo de comparação mantém apenas o seletor de zoom.
- O tema Estúdio continua opaco; não recebe as novas superfícies translúcidas.
- Removida uma descrição residual da Clareza que deslocava as explicações de
  Contraste e Intensidade das cores.

Verificação: compilação Release e conferência dos nomes/hashes das fontes em
`build/rounded-font-verification.json`. Não foram realizados testes de interface.
O instalador anterior não foi recompilado nesta alteração.
