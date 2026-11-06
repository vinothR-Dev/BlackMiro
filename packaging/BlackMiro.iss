; Inno Setup script for the BlackMiro Windows installer (.exe).
; Built by scripts\build-installers.bat:
;   ISCC /DAppVer=1.0.0 /DSrcDir=<deploy folder> /DOutDir=<output folder> BlackMiro.iss
#ifndef AppVer
  #define AppVer "1.0.0"
#endif

[Setup]
AppId={{8C1B2F8E-3D5A-4E6B-9F1C-2A7D4B5E6F01}
AppName=BlackMiro
AppVersion={#AppVer}
AppVerName=BlackMiro {#AppVer}
AppPublisher=BlackMiro
DefaultDirName={autopf}\BlackMiro
DefaultGroupName=BlackMiro
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=BlackMiro-{#AppVer}-windows-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installs for all users by default; users without admin rights can choose "just me".
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\BlackMiro.exe
UninstallDisplayName=BlackMiro
WizardStyle=modern
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SrcDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\BlackMiro"; Filename: "{app}\BlackMiro.exe"
Name: "{autodesktop}\BlackMiro"; Filename: "{app}\BlackMiro.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\BlackMiro.exe"; Description: "{cm:LaunchProgram,BlackMiro}"; Flags: nowait postinstall skipifsilent
