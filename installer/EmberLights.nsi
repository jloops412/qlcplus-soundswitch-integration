; Offline/Unix packaging fallback for limited beta builds. The GitHub-hosted
; Windows workflow remains authoritative and produces the equivalent Inno
; Setup package from EmberLights.iss.

Unicode true

!ifndef BuildDir
  !error "BuildDir must point to the staged EmberLights files."
!endif
!ifndef AppVersion
  !define AppVersion "0.1.0"
!endif
!ifndef OutputDir
  !define OutputDir "output"
!endif

!define AppName "EmberLights"
!define AppPublisher "EmberLights"
!define AppExeName "EmberLights.exe"
!define AppId "5AE71134-902A-4E44-AF80-ADCC47F15DA9"
!define UninstallKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\EmberLights"

Name "${AppName} ${AppVersion}"
OutFile "${OutputDir}\EmberLights-${AppVersion}-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${AppName}"
InstallDirRegKey HKCU "Software\${AppName}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
ManifestSupportedOS all
ManifestDPIAware true
BrandingText "EmberLights Limited Beta"
ShowInstDetails show
ShowUninstDetails show

VIProductVersion "0.1.0.1"
VIAddVersionKey /LANG=1033 "CompanyName" "${AppPublisher}"
VIAddVersionKey /LANG=1033 "FileDescription" "EmberLights Limited Beta Installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${AppVersion}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright EmberLights contributors"
VIAddVersionKey /LANG=1033 "ProductName" "${AppName}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${AppVersion}"

!include "MUI2.nsh"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${AppExeName}"
!define MUI_FINISHPAGE_RUN_TEXT "Launch EmberLights"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "EmberLights application" SecApplication
  SectionIn RO
  SetShellVarContext current
  SetOverwrite on
  SetOutPath "$INSTDIR"
  File /r "${BuildDir}\*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\${AppName}" "InstallDir" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\${AppName}"
  CreateShortcut "$SMPROGRAMS\${AppName}\EmberLights.lnk" "$INSTDIR\${AppExeName}"
  CreateShortcut "$SMPROGRAMS\${AppName}\EmberLights Hardware Test.lnk" \
    "$INSTDIR\Tools\soundswitch_micro_probe.exe" "--active-test"
  CreateShortcut "$SMPROGRAMS\${AppName}\Control One DMX Test.lnk" \
    "$SYSDIR\cmd.exe" "/k $\"$INSTDIR\Tools\soundswitch_control_one_probe.exe$\" --help"
  CreateShortcut "$SMPROGRAMS\${AppName}\Uninstall EmberLights.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\Classes\.emberlights" "" "EmberLights.Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project" "" "EmberLights Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\DefaultIcon" "" \
    "$INSTDIR\${AppExeName},0"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\shell\open\command" "" \
    "$\"$INSTDIR\${AppExeName}$\" $\"%1$\""

  WriteRegStr HKCU "${UninstallKey}" "DisplayName" "${AppName} ${AppVersion}"
  WriteRegStr HKCU "${UninstallKey}" "DisplayVersion" "${AppVersion}"
  WriteRegStr HKCU "${UninstallKey}" "Publisher" "${AppPublisher}"
  WriteRegStr HKCU "${UninstallKey}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UninstallKey}" "DisplayIcon" "$INSTDIR\${AppExeName}"
  WriteRegStr HKCU "${UninstallKey}" "UninstallString" \
    "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegDWORD HKCU "${UninstallKey}" "NoModify" 1
  WriteRegDWORD HKCU "${UninstallKey}" "NoRepair" 1

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section /o "Desktop shortcut" SecDesktop
  SetShellVarContext current
  CreateShortcut "$DESKTOP\EmberLights.lnk" "$INSTDIR\${AppExeName}"
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\EmberLights.lnk"
  RMDir /r "$SMPROGRAMS\${AppName}"

  DeleteRegKey HKCU "${UninstallKey}"
  DeleteRegKey HKCU "Software\Classes\EmberLights.Project"
  DeleteRegValue HKCU "Software\Classes\.emberlights" ""
  DeleteRegKey /ifempty HKCU "Software\Classes\.emberlights"
  DeleteRegKey HKCU "Software\${AppName}"

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
  RMDir /r "$INSTDIR"
SectionEnd
