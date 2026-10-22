@echo off
set "PATH=%~dp0..\.tools\MinGit\cmd;%PATH%"
"%~dp0..\.tools\Python\python.exe" -m code_review_graph update --repo "%~dp0.."
pause
