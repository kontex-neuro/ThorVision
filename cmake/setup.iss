#define AppName "Thor Vision"
#define AppExeName "Thor Vision.exe"

[Setup]
ChangesEnvironment=yes
ChangesAssociations=yes
AppCopyright=Copyright (C) 2025, KonteX Neuroscience
UninstallDisplayIcon={app}\{#AppExeName}

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};C:\gstreamer\1.0\msvc_x86_64\bin"; \
Check: NeedsAddPath('C:\gstreamer\1.0\msvc_x86_64\bin');

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: install_dependency; Description: "Install GStreamer Runtime Library"; GroupDescription: "Required Dependencies";

[Dirs]
Name: "{commonappdata}\{#AppName}"; Permissions: users-modify; Flags: uninsneveruninstall;

[Files]
Source: "gstreamer-1.0-msvc-x86_64-1.24.10.msi"; DestDir: "{tmp}"; Flags: deleteafterinstall;

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}";
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}";
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "msiexec.exe"; \
StatusMsg: "Installing GStreamer Runtime Library..."; \
Parameters: "/i ""{tmp}\gstreamer-1.0-msvc-x86_64-1.24.10.msi"" /qb"; \
Tasks: install_dependency; \
Flags: skipifsilent;
; Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
  ParamExpanded: string;
begin
  ParamExpanded := ExpandConstant(Param);
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(ParamExpanded) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;