[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory, Position = 0)]
    [string] $DeviceAddress,
    [ValidateSet('release', 'dev')]
    [string] $Channel = 'dev',
    [ValidatePattern('^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')]
    [string] $Version
)

$ErrorActionPreference = 'Stop'
$parsed = [System.Net.IPAddress]::None
if (-not [System.Net.IPAddress]::TryParse($DeviceAddress, [ref] $parsed) -or
    $parsed.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork -or
    $parsed.ToString() -cne $DeviceAddress -or $parsed.GetAddressBytes()[0] -ge 224 -or
    $DeviceAddress -eq '0.0.0.0') {
    throw "Enter the target device's IPv4 address."
}

$binary = Join-Path $PSScriptRoot 'bin\ThorVisionDevUpdater.exe'
if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    throw "The DEV updater executable is missing from this bundle: $binary"
}
if ($PSCmdlet.ShouldProcess($DeviceAddress, 'Review the selected AIO package')) {
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $binary
    $start.WorkingDirectory = Split-Path -Parent $binary
    $start.UseShellExecute = $false
    $start.EnvironmentVariables['THORVISION_DEV_UPDATE_HOST'] = $DeviceAddress
    $start.EnvironmentVariables['THORVISION_DEVICE_UPDATE_CHANNEL'] = $Channel.ToLowerInvariant()
    $start.EnvironmentVariables['THORVISION_DEVICE_UPDATE_VERSION'] = [string]$Version
    [System.Diagnostics.Process]::Start($start)
}
