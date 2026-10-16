Unicode true
!include "LogicLib.nsh"
!include "FileFunc.nsh"
Name "FrameScale"
OutFile "${OUTPUT_FILE}"
RequestExecutionLevel user
InstallDir "$LOCALAPPDATA\Programs\FrameScale"
SilentInstall silent
AutoCloseWindow true
SetCompressor lzma
Icon "..\..\assets\framescale.ico"
ManifestDPIAware true
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "FrameScale"
VIAddVersionKey "FileDescription" "FrameScale Setup"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "FrameScale contributors"
Var Quiet
Var DesktopChoice
Function .onInit
  ; SilentInstall suppresses the bootstrap UI. /S alone decides whether the
  ; custom Qt surface opens; automated deployments use the same task controller.
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/S" $1
  ${If} ${Errors}
    StrCpy $Quiet "0"
  ${Else}
    StrCpy $Quiet "1"
  ${EndIf}
  ${GetOptions} $0 "/DESKTOP=" $DesktopChoice
FunctionEnd
Section
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\ui"
  File /r "${FRONTEND_DIR}\*"
  SetOutPath "$PLUGINSDIR"
  ; This inner package is already compressed; avoid recompressing 281 MB.
  SetCompress off
  File /oname=payload.exe "${BACKEND_FILE}"
  SetCompress auto
  ${If} $Quiet == "1"
    ExecWait '"$PLUGINSDIR\ui\FrameScaleSetup.exe" --silent-install --backend "$PLUGINSDIR\payload.exe" --target "$INSTDIR" --desktop "$DesktopChoice"' $0
  ${Else}
    ExecWait '"$PLUGINSDIR\ui\FrameScaleSetup.exe" --backend "$PLUGINSDIR\payload.exe" --version "${APP_VERSION}"' $0
  ${EndIf}
  SetErrorLevel $0
SectionEnd
