# build_debug.ps1 - Build debug version of gstqt6d3d11 QML plugin

$ErrorActionPreference = "Stop"

$script_dir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $script_dir

# Visual Studio environment
$vs_path = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if (-Not (Test-Path $vs_path)) {
    Write-Error "Visual Studio vcvarsall.bat not found at $vs_path"
    exit 1
}

# Qt6 path
$qt6_path = "C:\Qt\6.8.0\msvc2022_64"
$qt6_bin = "$qt6_path\bin"

# GStreamer path
$gst_path = "C:\gstreamer\1.0\msvc_x86_64"

# Set up environment
$env:PATH = "$qt6_bin;$gst_path\bin;$env:PATH"
$env:PKG_CONFIG_PATH = "$gst_path\lib\pkgconfig"
$env:Qt6_DIR = "$qt6_path\lib\cmake\Qt6"

Write-Host "--- Configuring CMake (Debug) ---" -ForegroundColor Cyan

# Clean build directory
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build"
}

# Configure with CMake using MSVC
cmd /c "`"$vs_path`" amd64 && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=`"$qt6_path`""

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    Pop-Location
    exit 1
}

Write-Host "--- Building ---" -ForegroundColor Cyan

cmd /c "`"$vs_path`" amd64 && cmake --build build"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    Pop-Location
    exit 1
}

Write-Host "--- Build Complete ---" -ForegroundColor Green
Write-Host "Debug plugin built and copied to ../gstreamer_qml/org/freedesktop/gstreamer/Qt6D3D11VideoItem/"

Pop-Location
