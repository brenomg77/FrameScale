Unicode true
!include "LogicLib.nsh"
!include "x64.nsh"
!include "WinVer.nsh"
!include "FileFunc.nsh"
Name "FrameScale"
OutFile "${OUTPUT_FILE}"
InstallDir "$LOCALAPPDATA\Programs\FrameScale"
InstallDirRegKey HKCU "Software\FrameScale\Installer" "InstallDir"
RequestExecutionLevel user
SilentInstall silent
SilentUnInstall silent
SetCompressor /SOLID lzma
SetCompressorDictSize 32
Icon "..\..\assets\framescale.ico"
UninstallIcon "..\..\assets\framescale.ico"
VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey "ProductName" "FrameScale"
VIAddVersionKey "FileDescription" "FrameScale installation engine"
VIAddVersionKey "FileVersion" "${APP_VERSION}"
VIAddVersionKey "LegalCopyright" "FrameScale contributors"
Var ProgressFile
Var DesktopChoice
LangString AppInUse 1033 "Close FrameScale before installing or removing the application."
; Opening without sharing detects a running/locked executable before changing
; any installed files. OPEN_EXISTING never creates or truncates the executable.
!macro CheckAppClosed PREFIX
Function ${PREFIX}CheckAppClosed
  IfFileExists "$INSTDIR\bin\FrameScale.exe" 0 app_closed
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\bin\FrameScale.exe", i 0xC0000000, i 0, p 0, i 3, i 0, p 0) p .r0'
  ${If} $0 == -1
    MessageBox MB_OK|MB_ICONSTOP "$(AppInUse)" /SD IDOK
    SetErrorLevel 2
    Abort
  ${EndIf}
  System::Call 'kernel32::CloseHandle(p r0)'
  app_closed:
FunctionEnd
!macroend
!insertmacro CheckAppClosed ""
!insertmacro CheckAppClosed "un."

Function .onInit
  ${IfNot} ${AtLeastWin10}
    SetErrorLevel 4
    Abort
  ${EndIf}
  ${IfNot} ${RunningX64}
    SetErrorLevel 4
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext current
  ${GetParameters} $0
  ${GetOptions} $0 "/STATUS=" $ProgressFile
  ${GetOptions} $0 "/DESKTOP=" $DesktopChoice
FunctionEnd
Function un.onInit
  SetRegView 64
  SetShellVarContext current
  IfSilent un_ready
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\ui"
  File /r "${FRONTEND_DIR}\*"
  ExecWait '"$PLUGINSDIR\ui\FrameScaleSetup.exe" --uninstall' $0
  SetErrorLevel $0
  Quit
  un_ready:
FunctionEnd
Section "Application" ApplicationSection
  SectionIn RO
  Call CheckAppClosed
  SetOutPath "$INSTDIR"
  SetOverwrite on
  ClearErrors
  !include "${INSTALL_FILES}"
  IfErrors 0 +3
    SetErrorLevel 3
    Abort
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
  ${If} $DesktopChoice == "1"
    CreateShortcut "$DESKTOP\FrameScale.lnk" "$INSTDIR\bin\FrameScale.exe"
  ${EndIf}
SectionEnd

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
