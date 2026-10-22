# FrameScale — PAP

| Pasta | Conteúdo |
| --- | --- |
| `FrameScale/` | Código C++/Qt, recursos, testes, modelos e compilação |
| `output/` | Relatório Word e PDF |
| `img/` | PSDs e logótipos |
| `docs/` | Documentação e demonstrações |
| `scripts/` | Abrir o editor, compilar, testar e atualizar o grafo |
| `.tools/` | Git, Python e code-review-graph locais, ignorados pelo Git |

O repositório Git está nesta raiz. **Abrir FrameScale.cmd** abre o projeto
no Qt Creator instalado em `C:\Qt`.

## Executar no Qt Creator

1. Abrir `FrameScale/CMakeLists.txt`.
2. Selecionar o kit **Desktop Qt 6.11.2 MinGW 64-bit** ou o preset
   **FrameScale** quando aparecer na configuração.
3. Selecionar o alvo **FrameScale**, compilar e executar com **Ctrl+R**.

O kit prepara as bibliotecas Qt necessárias. Os modelos e motores são
copiados automaticamente para a pasta da compilação.

Compilar: `powershell -ExecutionPolicy Bypass -File .\scripts\Compilar.ps1`.
O executável gerado fica em `FrameScale/build/Release/FrameScale.exe`.
O ambiente utiliza Qt 6.11.2 e MinGW 13.1 instalados em `C:\Qt`.

O projeto usa o seu próprio executável compilado. Git, Python e a instalação
do code-review-graph ficam em `.tools/`. Não é necessária uma cópia separada
da aplicação nem a pasta externa `FrameScaleDevelopment`.

Code-review-graph está configurado para esta raiz e a ligação MCP foi testada.
Reiniciar o Codex para carregar os caminhos atualizados.
Atualização manual: **scripts/Atualizar grafo.cmd**.

Validação em 20/10/2026: `scripts/Compilar.ps1 -Testar` compilou a aplicação
e passou nos **9/9 testes CTest**, incluindo `FrameScaleUiSmoke`.
O teste independente do tour inicial também passou.
