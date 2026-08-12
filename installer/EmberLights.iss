#ifndef BuildDir
  #error BuildDir must point to the staged EmberLights files.
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

#define AppName "EmberLights"
#define AppPublisher "EmberLights"
#define AppExeName "EmberLights.exe"

[Setup]
AppId={{5AE71134-902A-4E44-AF80-ADCC47F15DA9}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=EmberLights-{#AppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
UninstallDisplayIcon={app}\{#AppExeName}
ChangesAssociations=yes
MinVersion=10.0.17763
AppMutex=Local\EmberLights-5AE71134-902A-4E44-AF80-ADCC47F15DA9
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#BuildDir}\EmberLights.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BuildDir}\midi_capture.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion
Source: "{#BuildDir}\Tools\emberlights_qualify.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion
Source: "{#BuildDir}\Tools\emberlights_migrate.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion
Source: "{#BuildDir}\Tools\soundswitch_micro_probe.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion
Source: "{#BuildDir}\Tools\soundswitch_control_one_probe.exe"; DestDir: "{app}\Tools"; Flags: ignoreversion
Source: "{#BuildDir}\Templates\EmberLights-2026-V1-Template.emberlights"; DestDir: "{app}\Templates"; Flags: ignoreversion
Source: "{#BuildDir}\docs\README.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\THIRD_PARTY_NOTICES.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\15_WINDOWS_V1_TESTING.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\16_QLC_FIXTURE_IMPORT.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\17_PRODUCTION_RELEASE_GATE.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\18_SOUNDSWITCH_MIGRATION.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\20_CONTROL_ONE_DMX_QUALIFICATION.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "{#BuildDir}\docs\MORNING_HARDWARE_TEST.md"; DestDir: "{app}\docs"; Flags: ignoreversion

[Icons]
Name: "{group}\EmberLights"; Filename: "{app}\{#AppExeName}"
Name: "{group}\EmberLights Hardware Test"; Filename: "{app}\Tools\soundswitch_micro_probe.exe"; Parameters: "--active-test"
Name: "{group}\Control One DMX Test"; Filename: "{cmd}"; Parameters: "/k """"{app}\Tools\soundswitch_control_one_probe.exe"" --help"""
Name: "{autodesktop}\EmberLights"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\EmberLights"; Flags: uninsdeletekeyifempty
Root: HKCU; Subkey: "Software\Classes\.emberlights"; ValueType: string; ValueData: "EmberLights.Project"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\EmberLights.Project"; ValueType: string; ValueData: "EmberLights Project"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\EmberLights.Project\DefaultIcon"; ValueType: string; ValueData: "{app}\{#AppExeName},0"
Root: HKCU; Subkey: "Software\Classes\EmberLights.Project\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExeName}"" ""%1"""

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch EmberLights"; Flags: nowait postinstall skipifsilent
