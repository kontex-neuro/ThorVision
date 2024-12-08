[Files]
Source: "gstreamer-1.0-msvc-x86_64-1.24.10.msi"; \
DestDir: "{app}"; \
Flags: deleteafterinstall;

[Run]
Filename: "msiexec.exe"; \
Parameters: "/i ""{app}\gstreamer-1.0-msvc-x86_64-1.24.10.msi"" /qb"; \
WorkingDir: {app};