; Inno Setup script for the Miru Windows installer (.exe).
; Built by scripts\build-installers.bat:
;   ISCC /DAppVer=1.0.0 /DSrcDir=<deploy folder> /DOutDir=<output folder> Miru.iss
#ifndef AppVer
  #define AppVer "1.0.0"
#endif

[Setup]
AppId={{8C1B2F8E-3D5A-4E6B-9F1C-2A7D4B5E6F01}
AppName=Miru
AppVersion={#AppVer}
AppVerName=Miru {#AppVer}
AppPublisher=Miru
DefaultDirName={autopf}\Miru
DefaultGroupName=Miru
DisableProgramGroupPage=yes
OutputDir={#OutDir}
OutputBaseFilename=Miru-{#AppVer}-windows-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Installs for all users by default; users without admin rights can choose "just me".
PrivilegesRequiredOverridesAllowed=dialog
UninstallDisplayIcon={app}\Miru.exe
UninstallDisplayName=Miru
WizardStyle=modern
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SrcDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Miru"; Filename: "{app}\Miru.exe"
Name: "{autodesktop}\Miru"; Filename: "{app}\Miru.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Miru.exe"; Description: "{cm:LaunchProgram,Miru}"; Flags: nowait postinstall skipifsilent
