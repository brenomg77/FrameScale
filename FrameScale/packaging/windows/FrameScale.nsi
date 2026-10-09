Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

!ifndef APP_VERSION
  !define APP_VERSION "0.1.0"
!endif
!ifndef PAYLOAD_DIR
  !error "Supply PAYLOAD_DIR (cmake install prefix)"
!endif
!ifndef OUTPUT_FILE
  !error "Supply OUTPUT_FILE"
!endif
!ifndef UNINSTALL_FILES
  !error "Supply UNINSTALL_FILES (generated file list)"
!endif

Name "FrameScale"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\FrameScale"
InstallDirRegKey HKCU "Software\FrameScale\Installer" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
ManifestDPIAware true
BrandingText "FrameScale ${APP_VERSION}"
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "FrameScale"
VIAddVersionKey "FileDescription" "FrameScale Setup"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "ProductVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "FrameScale contributors"

!define MUI_ICON "..\..\assets\framescale.ico"
!define MUI_UNICON "..\..\assets\framescale.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\FrameScale.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "Portuguese"
!insertmacro MUI_LANGUAGE "English"

LangString AppSection ${LANG_PORTUGUESEBR} "Programa e modelos"
LangString AppSection ${LANG_PORTUGUESE} "Programa e modelos"
LangString AppSection ${LANG_ENGLISH} "Application and models"
LangString DesktopSection ${LANG_PORTUGUESEBR} "Atalho na Área de trabalho"
LangString DesktopSection ${LANG_PORTUGUESE} "Atalho no ambiente de trabalho"
LangString DesktopSection ${LANG_ENGLISH} "Desktop shortcut"
LangString UnsupportedWindows ${LANG_PORTUGUESEBR} "O FrameScale requer Windows 10 ou 11 de 64 bits."
LangString UnsupportedWindows ${LANG_PORTUGUESE} "O FrameScale requer Windows 10 ou 11 de 64 bits."
LangString UnsupportedWindows ${LANG_ENGLISH} "FrameScale requires 64-bit Windows 10 or 11."
LangString AppInUse ${LANG_PORTUGUESEBR} "Feche o FrameScale antes de instalar ou remover o programa."
LangString AppInUse ${LANG_PORTUGUESE} "Feche o FrameScale antes de instalar ou remover o programa."
LangString AppInUse ${LANG_ENGLISH} "Close FrameScale before installing or removing the application."

; Opening without sharing detects a running/locked executable before changing
; any installed files. OPEN_EXISTING never creates or truncates the executable.
!macro CheckAppClosed PREFIX
Function ${PREFIX}CheckAppClosed
  IfFileExists "$INSTDIR\bin\FrameScale.exe" 0 app_closed
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\bin\FrameScale.exe", i 0xC0000000, i 0, p 0, i 3, i 0, p 0) p .r0'
  ${If} $0 == -1
    MessageBox MB_OK|MB_ICONSTOP "$(AppInUse)" /SD IDOK
    SetErrorLevel 1
    Abort
  ${EndIf}
  System::Call 'kernel32::CloseHandle(p r0)'
  app_closed:
FunctionEnd
!macroend
!insertmacro CheckAppClosed ""
!insertmacro CheckAppClosed "un."

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "$(UnsupportedWindows)"
    Abort
  ${EndIf}
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "$(UnsupportedWindows)"
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
FunctionEnd

Section "$(AppSection)" ApplicationSection
  SectionIn RO
  Call CheckAppClosed
  SetOutPath "$INSTDIR"
  SetOverwrite on
  File /r "${PAYLOAD_DIR}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\FrameScale\Installer" "InstallDir" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "DisplayName" "FrameScale"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "DisplayIcon" "$INSTDIR\bin\FrameScale.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "NoRepair" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale" "EstimatedSize" ${PAYLOAD_KIB}
  CreateDirectory "$SMPROGRAMS\FrameScale"
  CreateShortcut "$SMPROGRAMS\FrameScale\FrameScale.lnk" "$INSTDIR\bin\FrameScale.exe"
  CreateShortcut "$SMPROGRAMS\FrameScale\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section /o "$(DesktopSection)" DesktopShortcut
  CreateShortcut "$DESKTOP\FrameScale.lnk" "$INSTDIR\bin\FrameScale.exe"
SectionEnd

Function un.onInit
  SetRegView 64
  SetShellVarContext current
FunctionEnd

Section "Uninstall"
  Call un.CheckAppClosed
  ; Delete only files supplied by this package; preserve user-created files.
  !include "${UNINSTALL_FILES}"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\FrameScale\FrameScale.lnk"
  Delete "$SMPROGRAMS\FrameScale\Uninstall.lnk"
  RMDir "$SMPROGRAMS\FrameScale"
  Delete "$DESKTOP\FrameScale.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\FrameScale"
  DeleteRegKey HKCU "Software\FrameScale\Installer"
SectionEnd
