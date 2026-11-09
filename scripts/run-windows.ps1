param([switch]$Launch)
$ErrorActionPreference = 'Stop'
$receipt = Get-Content -Raw (Join-Path $PSScriptRoot 'build-receipt.json') | ConvertFrom-Json
$env:PATH = (Join-Path $receipt.qtRoot 'bin') + ';' + (Join-Path $receipt.gstreamerRoot 'bin') + ';' + $env:PATH
$env:GST_PLUGIN_PATH = (Join-Path $PSScriptRoot 'gstreamer-plugins') + ';' + (Join-Path $receipt.gstreamerRoot 'lib/gstreamer-1.0')
$env:THORVISION_NODE = Join-Path $PSScriptRoot 'node.exe'
$env:THORVISION_AGENT_BRIDGE = Join-Path $PSScriptRoot 'agent-bridge/dist/main.js'
if (-not $receipt.bridgeIncluded) { throw 'This build omitted the bridge; rerun build-windows.ps1 without -SkipBridge' }
if (-not (Test-Path -LiteralPath $env:THORVISION_AGENT_BRIDGE)) { throw 'The colocated bridge is missing' }
$nodeVersion = (& $env:THORVISION_NODE --version) -replace '^v', ''
if ($nodeVersion -ne $receipt.tools.node) { throw 'The colocated Node version differs from the build receipt' }
foreach ($entry in @(
    @{ File = $env:THORVISION_NODE; Hash = $receipt.nodeSha256 },
    @{ File = (Join-Path $PSScriptRoot 'ThorVision.exe'); Hash = $receipt.executableSha256 },
    @{ File = (Join-Path $PSScriptRoot 'xdaqmetadata.dll'); Hash = $receipt.metadataDllSha256 }
)) {
    if ((Get-FileHash -LiteralPath $entry.File -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Hash) {
        throw "Runtime file differs from build receipt: $($entry.File)"
    }
}
$sink = & (Join-Path $receipt.gstreamerRoot 'bin/gst-inspect-1.0.exe') qml6d3d11sink | Out-String
if ($LASTEXITCODE -ne 0) { throw 'The pinned GStreamer Qt sink cannot load' }
if ($sink -notmatch '(?m)^\s*Filename\s+(.+?)\s*$') { throw 'The GStreamer sink did not report its loaded library' }
if ((Get-FileHash -LiteralPath $Matches[1].Trim() -Algorithm SHA256).Hash.ToLowerInvariant() -ne $receipt.gstreamerQtPluginSha256) {
    throw 'GStreamer selected a different Qt plugin from the build receipt'
}
if (-not $Launch) { Write-Output 'Native runtime, pinned Node, and colocated bridge are available. Pass -Launch to open ThorVision.'; exit 0 }
& (Join-Path $PSScriptRoot 'ThorVision.exe')
