; KNG Studio installer, for Inno Setup 6.
; 1. Run build_release.bat
; 2. Check BuildDir and ExeName below match the files in that folder
; 3. Open this file in Inno Setup and press Compile. The installer lands in the installer folder.

#define AppName "KNG Studio"
#define AppVersion "0.1.0"
#define BuildDir "build\KNGStudio_artefacts\Release"
#define ExeName "KNG Studio.exe"

[Setup]
AppId={{8F3C2A41-6B7E-4D2A-9C55-3E1F0B7A9D21}
AppName={#AppName}
AppVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputDir=installer
OutputBaseFilename=KNG-Studio-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#BuildDir}\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#ExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#ExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#ExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
