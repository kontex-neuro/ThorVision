param(
    [Parameter(Mandatory)][string]$QtRoot,
    [Parameter(Mandatory)][string]$GStreamerRoot,
    [Parameter(Mandatory)][string]$Python,
    [Parameter(Mandatory)][string]$LibxvcSource,
    [string]$BuildDirectory,
    [string]$VisualStudioRoot,
    [string]$Conan,
    [switch]$SkipBridge,
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$source = Split-Path -Parent $PSScriptRoot
$support = Join-Path $source 'build-support/windows'
$pins = Get-Content -Raw (Join-Path $support 'sources.json') | ConvertFrom-Json
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $source 'build/AgentWorkflow' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$LibxvcSource = (Resolve-Path -LiteralPath $LibxvcSource).Path
$QtRoot = (Resolve-Path -LiteralPath $QtRoot).Path
$GStreamerRoot = (Resolve-Path -LiteralPath $GStreamerRoot).Path
$Python = (Resolve-Path -LiteralPath $Python).Path
if (-not $Conan) { $Conan = Join-Path (Split-Path $Python) 'Scripts/conan.exe' }

function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}
function Require-Version([string]$Name, [string]$Actual, [string]$Expected) {
    if ($Actual.Trim() -ne $Expected) { throw "$Name is '$($Actual.Trim())'; this build requires '$Expected'" }
}
function Git-Value([string]$Repository, [string[]]$Arguments) {
    $value = & git -c "safe.directory=$($Repository.Replace('\','/'))" -C $Repository @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect source repository: $Repository" }
    return ($value -join "`n").Trim()
}

$libCommit = Git-Value $LibxvcSource @('rev-parse', 'HEAD')
Require-Version 'libxvc commit' $libCommit $pins.libxvc.commit
if (Git-Value $LibxvcSource @('status', '--porcelain', '--untracked-files=no')) {
    throw 'libxvc has uncommitted tracked changes; use the pinned clean checkout'
}
Require-Version 'Qt' (& (Join-Path $QtRoot 'bin/qtpaths.exe') --qt-version) $pins.tools.qt
Require-Version 'GStreamer' (& (Join-Path $GStreamerRoot 'bin/pkg-config.exe') --modversion gstreamer-1.0) $pins.tools.gstreamer
Require-Version 'Python' (& $Python -c 'import platform; print(platform.python_version())') $pins.tools.python
Require-Version 'Conan' ((& $Conan --version) -replace '^Conan version ', '') $pins.tools.conan
$node = (Get-Command node.exe -ErrorAction Stop).Source
Require-Version 'Node' ((& $node --version) -replace '^v', '') $pins.tools.node
$npm = (Get-Command npm.cmd -ErrorAction Stop).Source
Require-Version 'npm' (& $npm --version) $pins.tools.npm
$pluginDirectory = Join-Path $source 'third_party/windows/gstreamer'
$plugin = Join-Path $pluginDirectory $pins.gstreamerQtPlugin.file
if ((Get-FileHash -LiteralPath $plugin -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.gstreamerQtPlugin.sha256) {
    throw 'The GStreamer Qt plugin does not match the pinned SHA256'
}

if (-not $VisualStudioRoot) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    $VisualStudioRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
Import-Module (Join-Path $VisualStudioRoot 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $VisualStudioRoot -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64 -vcvars_ver=14.40 -winsdk=10.0.22621.0'
$compilerVersion = (& cl 2>&1 | Out-String)
if ($compilerVersion -notmatch [regex]::Escape($pins.tools.msvc)) { throw "MSVC $($pins.tools.msvc) is required" }
Require-Version 'Windows SDK' $env:WindowsSDKVersion.TrimEnd('\') $pins.tools.windowsSdk
$env:PKG_CONFIG_PATH = Join-Path $GStreamerRoot 'lib/pkgconfig'
$env:PATH = (Join-Path $QtRoot 'bin') + ';' + (Join-Path $GStreamerRoot 'bin') + ';' + $env:PATH
$env:GST_PLUGIN_PATH = $pluginDirectory + ';' + (Join-Path $GStreamerRoot 'lib/gstreamer-1.0')

New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null
$dependencies = Join-Path $BuildDirectory 'dependencies'
$env:CONAN_HOME = Join-Path $BuildDirectory 'conan-cache'
$packages = @(Get-Content -Raw (Join-Path $support 'packages.json') | ConvertFrom-Json)
foreach ($package in $packages | Where-Object download) {
    & $Conan cache path $package.reference 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Invoke-Checked $Conan @('download', $package.reference, '--remote', 'conancenter')
    }
}
$lock = Get-Content -Raw (Join-Path $support 'conan.lock') | ConvertFrom-Json
foreach ($reference in @($lock.requires) + @($lock.build_requires)) {
    $recipeReference = ($reference -split '%')[0]
    if ($recipeReference -like 'xdaqmetadata/*') { continue }
    & $Conan cache path $recipeReference 2>$null | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Invoke-Checked $Conan @('download', $recipeReference, '--only-recipe', '--remote', 'conancenter')
    }
}
Invoke-Checked $Conan @('export', (Join-Path $support 'xdaqmetadata'))
$installArgs = @('install', (Join-Path $support 'dependencies.py'), '--profile:host', (Join-Path $support 'msvc194-release'), '--profile:build', (Join-Path $support 'msvc194-release'), '--lockfile', (Join-Path $support 'conan.lock'), '--output-folder', $dependencies, '--conf', "tools.build:jobs=$Jobs", '--build=missing', '--no-remote', '--format=json')
$graphText = & $Conan @installArgs
if ($LASTEXITCODE -ne 0) { throw 'Conan dependency installation failed' }
$graphText | Set-Content -Encoding utf8 (Join-Path $BuildDirectory 'conan-graph.json')
$graph = $graphText | ConvertFrom-Json
$nodes = @($graph.graph.nodes.PSObject.Properties.Value | Where-Object package_folder | Sort-Object context,ref,package_id,prev -Unique)
foreach ($package in $packages) {
    $expected = $package.reference
    $actual = @($nodes | Where-Object {
        if ($_.context -ne $package.context) { return $false }
        $reference = $_.ref + ':' + $_.package_id
        if ($package.download) { $reference += '#' + $_.prev }
        return $reference -eq $expected
    })
    if ($actual.Count -ne 1) { throw "Resolved package differs from packages.json: $expected" }
}
if (@($nodes | Where-Object package_folder).Count -ne $packages.Count) { throw 'Resolved graph has unexpected native packages' }
$cmake = Join-Path ($nodes | Where-Object name -eq 'cmake' | Select-Object -First 1).package_folder 'bin/cmake.exe'
$ninja = Join-Path ($nodes | Where-Object name -eq 'ninja' | Select-Object -First 1).package_folder 'bin/ninja.exe'
Invoke-Checked $cmake @('-S', $source, '-B', $BuildDirectory, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_TOOLCHAIN_FILE=$dependencies/conan_toolchain.cmake", "-DCMAKE_MAKE_PROGRAM=$ninja", "-DTHORVISION_LIBXVC_SOURCE=$LibxvcSource", "-DCMAKE_PREFIX_PATH=$QtRoot", "-DPython3_EXECUTABLE=$Python", "-DPKG_CONFIG_EXECUTABLE=$GStreamerRoot/bin/pkg-config.exe", "-Dgst_dir=$GStreamerRoot", '-DCMAKE_OBJECT_PATH_MAX=220', '-DTHORVISION_ENABLE_UI_SMOKE=OFF')
Invoke-Checked $cmake @('--build', $BuildDirectory, '--target', 'ThorVision', 'recording-filename-test', 'extension-client-smoke', '--parallel', "$Jobs")
Copy-Item -LiteralPath (Join-Path $dependencies 'xdaqmetadata.dll') -Destination $BuildDirectory -Force
$env:PATH = $BuildDirectory + ';' + $env:PATH
$outputPluginDirectory = Join-Path $BuildDirectory 'gstreamer-plugins'
New-Item -ItemType Directory -Path $outputPluginDirectory -Force | Out-Null
Get-ChildItem -LiteralPath $pluginDirectory | Copy-Item -Destination $outputPluginDirectory -Recurse -Force
Invoke-Checked (Join-Path $BuildDirectory 'thorvision/recording-filename-test.exe') @()
$previousPlatform = $env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM = 'offscreen'
    Invoke-Checked (Join-Path $BuildDirectory 'thorvision/extension-client-smoke.exe') @('--api-transport-test')
} finally { $env:QT_QPA_PLATFORM = $previousPlatform }

if (-not $SkipBridge) {
    Push-Location (Join-Path $source 'agent-bridge')
    try {
        Invoke-Checked $npm @('ci', '--ignore-scripts', '--no-audit', '--no-fund')
        Invoke-Checked $npm @('run', 'build')
        Invoke-Checked $npm @('test')
    } finally { Pop-Location }
    $bridge = Join-Path $BuildDirectory 'agent-bridge'
    New-Item -ItemType Directory -Path $bridge -Force | Out-Null
    foreach ($item in @('dist', 'control-ui', 'sdk-snapshot', 'plugins', 'node_modules', 'package.json', 'package-lock.json')) {
        Copy-Item -LiteralPath (Join-Path $source "agent-bridge/$item") -Destination $bridge -Recurse -Force
    }
    Copy-Item -LiteralPath $node -Destination (Join-Path $BuildDirectory 'node.exe') -Force
}
$executable = Join-Path $BuildDirectory 'ThorVision.exe'
$tests = @('recording-filename-test', 'extension-client-smoke --api-transport-test')
if (-not $SkipBridge) { $tests += 'npm test (agent bridge)' }
$receipt = [ordered]@{
    desktopCommit = Git-Value $source @('rev-parse', 'HEAD')
    desktopTrackedChanges = Git-Value $source @('status', '--porcelain', '--untracked-files=no')
    libxvcCommit = $libCommit
    conanLockSha256 = (Get-FileHash -LiteralPath (Join-Path $support 'conan.lock') -Algorithm SHA256).Hash.ToLowerInvariant()
    packagesManifestSha256 = (Get-FileHash -LiteralPath (Join-Path $support 'packages.json') -Algorithm SHA256).Hash.ToLowerInvariant()
    executableSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
    metadataDllSha256 = (Get-FileHash -LiteralPath (Join-Path $BuildDirectory 'xdaqmetadata.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
    gstreamerQtPluginSha256 = (Get-FileHash -LiteralPath (Join-Path $outputPluginDirectory $pins.gstreamerQtPlugin.file) -Algorithm SHA256).Hash.ToLowerInvariant()
    nodeSha256 = (Get-FileHash -LiteralPath $node -Algorithm SHA256).Hash.ToLowerInvariant()
    qtRoot = $QtRoot
    gstreamerRoot = $GStreamerRoot
    pluginDirectory = $outputPluginDirectory
    bridgeIncluded = -not $SkipBridge
    tests = $tests
    tools = $pins.tools
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $BuildDirectory 'build-receipt.json')
Copy-Item -LiteralPath (Join-Path $source 'scripts/run-windows.ps1') -Destination (Join-Path $BuildDirectory 'run-thorvision.ps1') -Force
Write-Output "Built $executable"
Write-Output "Build receipt: $(Join-Path $BuildDirectory 'build-receipt.json')"
