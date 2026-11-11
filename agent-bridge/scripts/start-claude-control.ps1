param(
    [Parameter(Mandatory = $true)][string]$DeviceUrl,
    [Parameter(Mandatory = $true)][string]$Workspace,
    [string]$Camera = '',
    [string]$ClaudeExecutable = 'claude'
)
$ErrorActionPreference = 'Stop'
$bridgeRoot = Split-Path -Parent $PSScriptRoot
$pluginRoot = Join-Path $bridgeRoot 'plugins/thorvision-framelab'
if (-not (Test-Path -LiteralPath (Join-Path $bridgeRoot 'dist/tools/server.js'))) {
    throw 'Build agent-bridge first: npm ci, then npm run build.'
}
$versionOutput = & $ClaudeExecutable --version
if ($LASTEXITCODE -ne 0 -or "$versionOutput" -notmatch '(\d+\.\d+\.\d+)') {
    throw 'Claude Code was not found. Install it from Anthropic, sign in, then run this launcher again.'
}
if ([version]$Matches[1] -lt [version]'2.1.312') {
    throw "This mod requires Claude Code 2.1.312 or newer. Found: $versionOutput. Pass -ClaudeExecutable to use a newer installation."
}
# Validate through the same URL policy as every ThorVision client before launching.
$envBefore = @{}
foreach ($name in @('THORVISION_WORKSPACE','THORVISION_FRAMELAB_URL','THORVISION_EXTENSIONS_URL','THORVISION_CAMERA','THORVISION_SDK_SNAPSHOT')) {
    $envBefore[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    $env:THORVISION_EXTENSIONS_URL = $DeviceUrl.TrimEnd('/')
    $env:THORVISION_FRAMELAB_URL = $env:THORVISION_EXTENSIONS_URL
    & node (Join-Path $PSScriptRoot 'validate-device-url.mjs') $env:THORVISION_FRAMELAB_URL
    if ($LASTEXITCODE -ne 0) { throw 'Use a valid local FrameLab device API URL.' }
    New-Item -ItemType Directory -Force -Path $Workspace | Out-Null
    $env:THORVISION_WORKSPACE = (Resolve-Path -LiteralPath $Workspace).Path
    $env:THORVISION_CAMERA = $Camera
    $env:THORVISION_SDK_SNAPSHOT = Join-Path $bridgeRoot 'sdk-snapshot'
    Write-Host 'ThorVision FrameLab: describe your idea in Claude. Open /framelab for camera controls and live video.'
    Write-Host 'Sign-in and billing use the account configured in Claude Code.'
    Push-Location $Workspace
    try {
        # Authoring uses only ThorVision MCP and Read. Shell/HTTP tools are not
        # supplied to the model. Human controls remain in the trusted mod.
        & $ClaudeExecutable --plugin-dir $pluginRoot --tools Read --allowedTools 'mcp__plugin_thorvision-framelab_framelab__*'
    } finally { Pop-Location }
} finally {
    foreach ($name in $envBefore.Keys) { [Environment]::SetEnvironmentVariable($name, $envBefore[$name], 'Process') }
}
