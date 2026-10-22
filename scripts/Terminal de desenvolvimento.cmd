@echo off
powershell.exe -NoExit -ExecutionPolicy Bypass -Command ". '%~dp0Ambiente.ps1'; Set-Location -LiteralPath '%~dp0..\FrameScale'"
