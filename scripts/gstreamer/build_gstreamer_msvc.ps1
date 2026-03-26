# build_gstreamer_msvc.ps1

$vs_path = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if (-Not (Test-Path $vs_path)) {
    Write-Error "Visual Studio vcvarsall.bat not found at $vs_path"
    exit 1
}

Write-Host "--- Cleaning builddir ---" -ForegroundColor Yellow
Get-Process ninja -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 2
if (Test-Path "builddir") { Remove-Item -Recurse -Force "builddir" -ErrorAction SilentlyContinue }

Write-Host "--- Configuring and Building GStreamer (MSVC) ---" -ForegroundColor Cyan

# 1. Capture a CLEAN path that only has Windows basics, Python, Git and Qt6
# This removes Strawberry Perl, MinGW, etc. from the session
$qt6_path = "C:\Qt\6.8.0\msvc2022_64\bin"
$clean_path = "C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312;C:\Users\$env:USERNAME\AppData\Local\Programs\Python\Python312\Scripts;C:\Program Files\Git\usr\bin;$qt6_path"

# 2. Define the Meson command with aggressive disabling of introspection
$meson_cmd = "meson setup builddir --wipe " +
             "--buildtype=debug " +
             "-Db_vscrt=md " +
             "--default-library=shared " +
             "-Dbase=enabled -Dgood=enabled -Dbad=enabled -Dugly=disabled " +
             "-Dqt6=enabled " +
             "-Dgst-plugins-bad:qt6=enabled -Dgst-plugins-bad:d3d11=enabled " +
             "-Dgst-plugins-bad:d3d12=disabled -Dgst-plugins-bad:vulkan=disabled " +
             "-Dgst-plugins-bad:webrtc=disabled -Dwebrtc=disabled " +
             "-Dlibav=disabled -Ddevtools=disabled -Dges=disabled -Drtsp_server=disabled " +
             "-Dvaapi=disabled -Dsharp=disabled -Drs=disabled " +
             "-Ddoc=disabled -Dintrospection=disabled " +
             "-Dglib:introspection=disabled " + # Target GLib specifically
             "-Dglib:tests=false " +
             "-Dpython=disabled -Dlibxml2:python=false"

# 3. Execute with a RETRY logic for the race condition
Write-Host "--- Starting Build (Attempt 1) ---" -ForegroundColor Cyan
cmd /c "set `"PATH=$clean_path`" && `"$vs_path`" amd64 && set `"CC=cl`" && set `"CXX=cl`" && $meson_cmd && meson compile -C builddir"
if ($LASTEXITCODE -ne 0) {
    Write-Host "--- Build failed (likely a race condition), retrying... ---" -ForegroundColor Yellow
    # Run ONLY the compile command without --wipe. This resumes where it left off.
    cmd /c "set `"PATH=$clean_path`" && `"$vs_path`" amd64 && meson compile -C builddir"
}
if ($LASTEXITCODE -eq 0) {
    Write-Host "--- Build Finished successfully! ---" -ForegroundColor Green
}