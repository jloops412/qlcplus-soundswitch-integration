; Offline/Unix packaging path for unsigned testing-preview builds. The staged
; payload must pass windows_package_contract.py before this script is invoked.

Unicode true

!ifndef BuildDir
  !error "BuildDir must point to the verified EmberLights stage directory."
!endif
!ifndef AppVersion
  !define AppVersion "0.1.0-preview.local"
!endif
!ifndef OutputDir
  !define OutputDir "."
!endif

!include "MUI2.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

!define AppName "EmberLights"
!define AppPublisher "EmberLights"
!define AppId "EmberLights-5AE71134-902A-4E44-AF80-ADCC47F15DA9"

Name "${AppName} ${AppVersion}"
OutFile "${OutputDir}/EmberLights-${AppVersion}-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${AppName}"
InstallDirRegKey HKCU "Software\${AppName}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 64
ManifestDPIAware true
ShowInstDetails show
ShowUninstDetails show
BrandingText "EmberLights testing preview"

VIProductVersion "0.1.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "${AppName}"
VIAddVersionKey /LANG=1033 "CompanyName" "${AppPublisher}"
VIAddVersionKey /LANG=1033 "FileDescription" "${AppName} testing-preview installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${AppVersion}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${AppVersion}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "EmberLights"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\EmberLights.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch EmberLights"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP|MB_OK "This EmberLights preview requires 64-bit Windows 10 or newer."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP|MB_OK "This EmberLights preview requires Windows 10 or newer."
    Abort
  ${EndIf}
  FindWindow $0 "EmberLightsMainWindow"
  ${If} $0 != 0
    MessageBox MB_ICONSTOP|MB_OK "Close EmberLights before installing this version."
    Abort
  ${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext current
FunctionEnd

Section "EmberLights" MainSection
  SectionIn RO
  SetOverwrite on
  SetOutPath "$INSTDIR"
  File /r "${BuildDir}\*"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\${AppName}" "InstallDir" "$INSTDIR"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "DisplayName" "${AppName}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "DisplayVersion" "${AppVersion}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "Publisher" "${AppPublisher}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "DisplayIcon" "$INSTDIR\EmberLights.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "NoRepair" 1

  WriteRegStr HKCU "Software\Classes\.emberlights" "" "EmberLights.Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project" "" "EmberLights Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\DefaultIcon" "" '$\"$INSTDIR\EmberLights.exe$\",0'
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\shell\open\command" "" '$\"$INSTDIR\EmberLights.exe$\" $\"%1$\"'

  CreateDirectory "$SMPROGRAMS\${AppName}"
  CreateShortCut "$SMPROGRAMS\${AppName}\${AppName}.lnk" "$INSTDIR\EmberLights.exe"
  CreateShortCut "$SMPROGRAMS\${AppName}\EmberLights Hardware Test.lnk" "$INSTDIR\Tools\soundswitch_micro_probe.exe" "--active-test"
  CreateShortCut "$SMPROGRAMS\${AppName}\Control One DMX Test.lnk" "$SYSDIR\cmd.exe" '/k $\"$INSTDIR\Tools\soundswitch_control_one_probe.exe$\" --help'
  CreateShortCut "$SMPROGRAMS\${AppName}\Uninstall ${AppName}.lnk" "$INSTDIR\Uninstall.exe"

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
  FindWindow $0 "EmberLightsMainWindow"
  ${If} $0 != 0
    MessageBox MB_ICONSTOP|MB_OK "Close EmberLights before uninstalling it."
    Abort
  ${EndIf}

  Delete "$SMPROGRAMS\${AppName}\${AppName}.lnk"
  Delete "$SMPROGRAMS\${AppName}\EmberLights Hardware Test.lnk"
  Delete "$SMPROGRAMS\${AppName}\Control One DMX Test.lnk"
  Delete "$SMPROGRAMS\${AppName}\Uninstall ${AppName}.lnk"
  RMDir "$SMPROGRAMS\${AppName}"

  DeleteRegKey HKCU "Software\Classes\EmberLights.Project"
  DeleteRegKey HKCU "Software\Classes\.emberlights"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}"
  DeleteRegValue HKCU "Software\${AppName}" "InstallDir"
  DeleteRegKey /ifempty HKCU "Software\${AppName}"

  Delete "$INSTDIR\EmberLights.exe"
  Delete "$INSTDIR\EmberLights-Windows-payload-manifest.json"
  Delete "$INSTDIR\Tools\emberlights_migrate.exe"
  Delete "$INSTDIR\Tools\emberlights_qualify.exe"
  Delete "$INSTDIR\Tools\midi_capture.exe"
  Delete "$INSTDIR\Tools\soundswitch_control_one_probe.exe"
  Delete "$INSTDIR\Tools\soundswitch_micro_probe.exe"
  RMDir "$INSTDIR\Tools"
  Delete "$INSTDIR\Templates\EmberLights-2026-V1-Template.emberlights"
  RMDir "$INSTDIR\Templates"
  Delete "$INSTDIR\docs\README.md"
  Delete "$INSTDIR\docs\THIRD_PARTY_NOTICES.md"
  Delete "$INSTDIR\docs\15_WINDOWS_V1_TESTING.md"
  Delete "$INSTDIR\docs\16_QLC_FIXTURE_IMPORT.md"
  Delete "$INSTDIR\docs\17_PRODUCTION_RELEASE_GATE.md"
  Delete "$INSTDIR\docs\18_SOUNDSWITCH_MIGRATION.md"
  Delete "$INSTDIR\docs\20_CONTROL_ONE_DMX_QUALIFICATION.md"
  Delete "$INSTDIR\docs\31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md"
  Delete "$INSTDIR\docs\32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md"
  Delete "$INSTDIR\docs\33_AUTOSCRIPT_STUDIO_E2E_TEST.md"
  Delete "$INSTDIR\docs\MORNING_HARDWARE_TEST.md"
  RMDir "$INSTDIR\docs"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd
