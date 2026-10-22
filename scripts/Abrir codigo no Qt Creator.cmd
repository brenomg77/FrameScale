@echo off
set "FS_ROOT=%~dp0.."
set "PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;%FS_ROOT%\.tools\MinGit\cmd;%PATH%"
if exist "C:\Qt\Tools\QtCreator\bin\qtcreator.exe" (
  start "" "C:\Qt\Tools\QtCreator\bin\qtcreator.exe" "%FS_ROOT%\FrameScale\CMakeLists.txt"
) else (
  echo Qt Creator nao encontrado em C:\Qt. Verifique a instalacao.
  pause
  exit /b 1
)
