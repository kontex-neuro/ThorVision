[Files]
Source: "XDAQ-VC\gstreamer-1.0-msvc-x86_64-1.24.9.msi"; \
DestDir: "{app}"; \
Flags: deleteafterinstall;

[Run]
Filename: "msiexec.exe"; \
Parameters: "/i ""{app}\XDAQ-VC\gstreamer-1.0-msvc-x86_64-1.24.9.msi"" /qb"; \
WorkingDir: {app};