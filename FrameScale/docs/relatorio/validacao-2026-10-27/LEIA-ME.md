# Registos de validação de 27 de outubro de 2026

Resultados da árvore local, com alterações de código e testes já existentes antes da revisão documental. `revision.txt` identifica o commit de base e a lista de alterações; `source-test-sha256.json` identifica os ficheiros por SHA-256. Os registos devem ser lidos em conjunto, sem atribuir estes resultados apenas ao commit ou ao instalador publicado anteriormente.

- `build-tests.log`: compilação incremental dos alvos de teste; sucesso, com avisos de retorno de `QFile::open` ignorado no auxiliar de processamento.
- `ctest.log`: resumo dos **11 CTest aprovados**, em 40,86 s.
- `ctest-full.log`: registo completo do CTest.
- `tour.log`: aprovação do executável de testes do tour.
- `setup-actions.log`: aprovação das ações e navegação do instalador com operações simuladas, sem instalação real.
- `text-captures.log`: execução adicional dos testes de texto para gerar capturas.
- `test-results.json`: códigos de saída; zero indica sucesso.

Ambiente: Windows x64, Qt 6.11.2, MinGW 13.1 e backend `offscreen`. Para reproduzir o âmbito da verificação a partir da raiz do repositório, com o ambiente local configurado:

```powershell
. ./scripts/Ambiente.ps1
$env:QT_QPA_PLATFORM = 'offscreen'
cmake --build FrameScale/build/Release --target framescale-tests FrameScaleUrlImageTests FrameScaleProgramTourTests FrameScaleSetupActionsTests --parallel 2
ctest --test-dir FrameScale/build/Release --output-on-failure
./FrameScale/build/Release/FrameScaleProgramTourTests.exe
./FrameScale/build/Release/FrameScaleSetupActionsTests.exe
```

Os testes configuram preferências e ficheiros temporários próprios. Para as capturas, `FrameScaleTextLayerTests` recebe a pasta de saída como primeiro argumento e `FRAMESCALE_CAPTURE_LIGHT=1` seleciona o tema claro. Os resultados não cobrem as matrizes extensas de IA, o ambiente gráfico nativo, o empacotamento Linux ou a instalação real numa máquina limpa.
