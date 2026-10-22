$frameScaleRoot = Split-Path $PSScriptRoot -Parent
$frameScaleQtRoot = 'C:\Qt'
$env:PATH = @(
    (Join-Path $frameScaleRoot '.tools\MinGit\cmd'),
    (Join-Path $frameScaleRoot '.tools\Python'),
    (Join-Path $frameScaleQtRoot '6.11.2\mingw_64\bin'),
    (Join-Path $frameScaleQtRoot 'Tools\mingw1310_64\bin'),
    (Join-Path $frameScaleQtRoot 'Tools\CMake_64\bin'),
    (Join-Path $frameScaleQtRoot 'Tools\Ninja'),
    $env:PATH
) -join ';'
$env:CMAKE_PREFIX_PATH = Join-Path $frameScaleQtRoot '6.11.2\mingw_64'
