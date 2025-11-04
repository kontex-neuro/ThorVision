#define AppName "ThorVision"
#define AppExeName "ThorVision.exe"

[Setup]
ChangesEnvironment=yes
ChangesAssociations=yes
AppCopyright=Copyright (C) 2025, KonteX Neuroscience
UninstallDisplayIcon={app}\bin\{#AppExeName}

[Dirs]
Name: "{commonappdata}\{#AppName}"; Permissions: users-modify; Flags: uninsneveruninstall

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\bin\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent