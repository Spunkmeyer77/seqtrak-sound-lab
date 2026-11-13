#ifndef MyVersion
  #define MyVersion "0.0.0"
#endif

[Setup]
AppId={{6B0E1C52-8F3A-4C1D-9B7E-5A2D3C4E5F60}
AppName=SEQTRAK Editor
AppVersion={#MyVersion}
AppPublisher=Transistor Labs
AppPublisherURL=https://ticcoross.gumroad.com/
DefaultDirName={autopf}\Transistor Labs\SEQTRAK Editor
DefaultGroupName=Transistor Labs
OutputBaseFilename=SEQTRAK-Editor-v{#MyVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "{#Src}\SEQTRAK Editor.vst3\*"; DestDir: "{commoncf64}\VST3\SEQTRAK Editor.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Src}\SEQTRAK Editor.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SEQTRAK Editor"; Filename: "{app}\SEQTRAK Editor.exe"
Name: "{group}\Uninstall SEQTRAK Editor"; Filename: "{uninstallexe}"
