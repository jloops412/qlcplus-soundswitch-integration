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

!define AppName "EmberLights"
!define AppPublisher "EmberLights"
!define AppId "EmberLights-5AE71134-902A-4E44-AF80-ADCC47F15DA9"
!define LegacyNsisAppId "EmberLights"
!define LegacyInnoAppId "{5AE71134-902A-4E44-AF80-ADCC47F15DA9}_is1"
!define LegacyInnoAppIdNoBraces "5AE71134-902A-4E44-AF80-ADCC47F15DA9_is1"
!define UninstallRoot "Software\Microsoft\Windows\CurrentVersion\Uninstall"

Name "${AppName} ${AppVersion}"
OutFile "${OutputDir}/EmberLights-${AppVersion}-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\${AppName}"
InstallDirRegKey HKCU "Software\${AppName}" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 64
ManifestSupportedOS all
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
  ; Do not second-guess Windows architecture/version detection in the 32-bit
  ; NSIS bootstrap. The installed payload is contract-checked PE32+ x86-64,
  ; so the Windows loader remains the authoritative compatibility gate.
  FindWindow $0 "EmberLightsMainWindow"
  ${If} $0 != 0
    MessageBox MB_ICONSTOP|MB_OK "Close EmberLights before installing this version."
    Abort
  ${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext current
FunctionEnd

; The testing builds have used two NSIS registry identities. Treat them as
; aliases for one per-user installation, run the registered uninstaller when
; available, and tolerate only a genuinely stale registry record. Projects and
; user settings live outside the program directory and are not touched.
Function RemovePriorNsisInstall
  ReadRegStr $R0 HKCU "${UninstallRoot}\${AppId}" "UninstallString"
  ReadRegStr $R1 HKCU "${UninstallRoot}\${AppId}" "InstallLocation"
  StrCmp $R0 "" 0 prior_nsis_found
  ReadRegStr $R0 HKCU "${UninstallRoot}\${LegacyNsisAppId}" "UninstallString"
  ReadRegStr $R1 HKCU "${UninstallRoot}\${LegacyNsisAppId}" "InstallLocation"
  StrCmp $R0 "" prior_nsis_done

prior_nsis_found:
  StrCmp $R1 "" 0 prior_nsis_retry
  StrCpy $R1 "$LOCALAPPDATA\Programs\${AppName}"

prior_nsis_retry:
  DetailPrint "Removing the earlier EmberLights installation..."
  ClearErrors
  ExecWait '$R0 /S _?=$R1' $R2
  IfErrors prior_nsis_stale
  IntCmp $R2 0 prior_nsis_removed prior_nsis_failed prior_nsis_failed

prior_nsis_failed:
  MessageBox MB_ICONSTOP|MB_RETRYCANCEL|MB_DEFBUTTON1 \
    "The earlier EmberLights uninstaller returned error $R2. Close EmberLights and retry, or cancel Setup." \
    IDRETRY prior_nsis_retry
  Abort

prior_nsis_stale:
  DetailPrint "The earlier NSIS uninstall record was stale; continuing with a repair install."

prior_nsis_removed:
  DeleteRegKey HKCU "${UninstallRoot}\${AppId}"
  DeleteRegKey HKCU "${UninstallRoot}\${LegacyNsisAppId}"

prior_nsis_done:
FunctionEnd

; Hosted builds used the same stable GUID with Inno Setup. Support both the
; canonical braced key and the defensive unbraced spelling so a new Setup can
; replace any earlier testing build without asking the operator to hunt for it.
Function RemovePriorInnoInstall
  ReadRegStr $R0 HKCU "${UninstallRoot}\${LegacyInnoAppId}" "UninstallString"
  StrCmp $R0 "" 0 prior_inno_found
  ReadRegStr $R0 HKCU "${UninstallRoot}\${LegacyInnoAppIdNoBraces}" "UninstallString"
  StrCmp $R0 "" prior_inno_done

prior_inno_found:
prior_inno_retry:
  DetailPrint "Removing the earlier EmberLights Inno Setup installation..."
  ClearErrors
  ExecWait '$R0 /VERYSILENT /SUPPRESSMSGBOXES /NORESTART' $R2
  IfErrors prior_inno_stale
  IntCmp $R2 0 prior_inno_removed prior_inno_failed prior_inno_failed

prior_inno_failed:
  MessageBox MB_ICONSTOP|MB_RETRYCANCEL|MB_DEFBUTTON1 \
    "The earlier EmberLights uninstaller returned error $R2. Close EmberLights and retry, or cancel Setup." \
    IDRETRY prior_inno_retry
  Abort

prior_inno_stale:
  DetailPrint "The earlier Inno Setup uninstall record was stale; continuing with a repair install."

prior_inno_removed:
  DeleteRegKey HKCU "${UninstallRoot}\${LegacyInnoAppId}"
  DeleteRegKey HKCU "${UninstallRoot}\${LegacyInnoAppIdNoBraces}"

prior_inno_done:
FunctionEnd

Section "EmberLights" MainSection
  SectionIn RO
  Call RemovePriorNsisInstall
  Call RemovePriorInnoInstall

  ; Remove obsolete installer metadata and shortcuts after the registered
  ; uninstallers have run. Do not recursively delete the program directory.
  Delete "$DESKTOP\EmberLights.lnk"
  Delete "$INSTDIR\unins???.exe"
  Delete "$INSTDIR\unins???.dat"
  Delete "$INSTDIR\unins???.msg"

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
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}" "NoRepair" 1

  WriteRegStr HKCU "Software\Classes\.emberlights" "" "EmberLights.Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project" "" "EmberLights Project"
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\DefaultIcon" "" '$\"$INSTDIR\EmberLights.exe$\",0'
  WriteRegStr HKCU "Software\Classes\EmberLights.Project\shell\open\command" "" '$\"$INSTDIR\EmberLights.exe$\" $\"%1$\"'

  CreateDirectory "$SMPROGRAMS\${AppName}"
  CreateShortCut "$SMPROGRAMS\${AppName}\${AppName}.lnk" "$INSTDIR\EmberLights.exe"
  CreateShortCut "$SMPROGRAMS\${AppName}\Advanced Manual DMX Test.lnk" "$INSTDIR\Tools\soundswitch_micro_probe.exe" "--manual-dmx"
  CreateShortCut "$SMPROGRAMS\${AppName}\EmberLights Hardware Test.lnk" "$INSTDIR\Tools\soundswitch_micro_probe.exe" "--active-test"
  CreateShortCut "$SMPROGRAMS\${AppName}\Control One DMX Test.lnk" "$SYSDIR\cmd.exe" '/k $\"$INSTDIR\Tools\soundswitch_control_one_probe.exe$\" --help'
  CreateShortCut "$SMPROGRAMS\${AppName}\IR-4 6CH Editable Bench.lnk" "$INSTDIR\EmberLights.exe" '$\"$INSTDIR\Templates\EmberLights-IR4-6CH-Editable-Bench.emberlights$\"'
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
  Delete "$SMPROGRAMS\${AppName}\Advanced Manual DMX Test.lnk"
  Delete "$SMPROGRAMS\${AppName}\EmberLights Hardware Test.lnk"
  Delete "$SMPROGRAMS\${AppName}\Control One DMX Test.lnk"
  Delete "$SMPROGRAMS\${AppName}\IR-4 6CH Editable Bench.lnk"
  Delete "$SMPROGRAMS\${AppName}\Uninstall ${AppName}.lnk"
  RMDir "$SMPROGRAMS\${AppName}"
  Delete "$DESKTOP\EmberLights.lnk"

  DeleteRegKey HKCU "Software\Classes\EmberLights.Project"
  DeleteRegKey HKCU "Software\Classes\.emberlights"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${LegacyNsisAppId}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${LegacyInnoAppId}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${LegacyInnoAppIdNoBraces}"
  DeleteRegValue HKCU "Software\${AppName}" "InstallDir"
  DeleteRegKey /ifempty HKCU "Software\${AppName}"

  Delete "$INSTDIR\EmberLights.exe"
  Delete "$INSTDIR\EmberLights-Windows-payload-manifest.json"
  Delete "$INSTDIR\Tools\emberlights_migrate.exe"
  Delete "$INSTDIR\Tools\emberlights_qualify.exe"
  Delete "$INSTDIR\Tools\midi_capture.exe"
  Delete "$INSTDIR\Tools\os2l_capture.exe"
  Delete "$INSTDIR\Tools\soundswitch_control_one_probe.exe"
  Delete "$INSTDIR\Tools\soundswitch_micro_probe.exe"
  RMDir "$INSTDIR\Tools"
  Delete "$INSTDIR\Templates\EmberLights-2026-V1-Template.emberlights"
  Delete "$INSTDIR\Templates\EmberLights-IR4-6CH-Editable-Bench.emberlights"
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
  Delete "$INSTDIR\docs\47_ADVANCED_MANUAL_DMX_TEST.md"
  Delete "$INSTDIR\docs\MORNING_HARDWARE_TEST.md"
  Delete "$INSTDIR\docs\IR4_6CH_RUNNER_FRAME_TEST.md"
  RMDir "$INSTDIR\docs"
  Delete "$INSTDIR\unins???.exe"
  Delete "$INSTDIR\unins???.dat"
  Delete "$INSTDIR\unins???.msg"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd
