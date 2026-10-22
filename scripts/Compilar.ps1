param([switch]$Testar, [switch]$Reconfigurar)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Ambiente.ps1')
# Windows PowerShell represents native stderr as error records when logging.
# Use the executable exit status below to distinguish warnings from failures.
$ErrorActionPreference = 'Continue'
$source = Join-Path $frameScaleRoot 'FrameScale'
$build = Join-Path $source 'build\Release'
$configureOptions = @()
if ($Reconfigurar) { $configureOptions += '--fresh' }
$resourceCompiler = (Join-Path $frameScaleQtRoot 'Tools\mingw1310_64\bin\windres.exe').Replace('\', '/')
& cmake @configureOptions -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$env:CMAKE_PREFIX_PATH" `
    "-DCMAKE_CXX_COMPILER=$frameScaleQtRoot\Tools\mingw1310_64\bin\g++.exe" `
    "-DCMAKE_RC_COMPILER:FILEPATH=$resourceCompiler" `
    "-DCMAKE_MAKE_PROGRAM=$frameScaleQtRoot\Tools\Ninja\ninja.exe"
if ($LASTEXITCODE -ne 0) { throw 'Falha na configuracao do CMake.' }
$targets = @('FrameScale')
if ($Testar) {
    $targets += @('framescale-tests', 'FrameScaleUrlImageTests',
        'FrameScaleProgramTourTests', 'FrameScaleSetupActionsTests',
        'FrameScalePreviewDecodeCheck')
}
& cmake --build $build --target @targets --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Falha na compilacao.' }
if ($Testar) {
    & ctest --test-dir $build --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Um ou mais testes falharam. Ver logs do CTest.' }
}
Write-Output "Executavel compilado: $build\FrameScale.exe"
