[Setup]
AppId={{94DACB03-80DC-4AB6-873B-A3F9D9EF6558}
AppName=Mixish
AppVersion=0.0.2
AppVerName=Mixish 0.0.2
AppPublisher=Mixish
AppPublisherURL=https://github.com/
DefaultDirName={autopf}\Mixish
DefaultGroupName=Mixish
DisableProgramGroupPage=yes
LicenseFile=..\COPYING
OutputDir=..\..\dist
OutputBaseFilename=Mixish-Setup-0.0.2-x64
SetupIconFile=..\frontend\cmake\windows\mixish.ico
UninstallDisplayIcon={app}\bin\64bit\obs64.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
CloseApplications=yes
RestartApplications=no
VersionInfoVersion=0.0.2.0
VersionInfoProductName=Mixish
VersionInfoCompany=Mixish

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build_x64_local\rundir\RelWithDebInfo\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp,*.ilk,config\*"
Source: "..\build_x64_local\rundir\RelWithDebInfo\core\*"; DestDir: "{app}\core"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp,*.ilk"
Source: "..\build_x64_local\rundir\RelWithDebInfo\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.lib,*.exp,*.ilk"
Source: "..\COPYING"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Mixish"; Filename: "{app}\bin\64bit\obs64.exe"; WorkingDir: "{app}\bin\64bit"
Name: "{group}\Uninstall Mixish"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Mixish"; Filename: "{app}\bin\64bit\obs64.exe"; WorkingDir: "{app}\bin\64bit"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\64bit\obs64.exe"; Description: "{cm:LaunchProgram,Mixish}"; Flags: nowait postinstall skipifsilent
