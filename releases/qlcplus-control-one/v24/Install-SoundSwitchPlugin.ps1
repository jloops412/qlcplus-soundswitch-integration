[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,
    [switch]$AllowCompatibleCore
)

$ErrorActionPreference = 'Stop'

$expectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$expectedPluginSha256 = '2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC'
$sourcePlugin = Join-Path $PSScriptRoot 'soundswitch.dll'
$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$coreExecutable = Join-Path $resolvedRoot 'qlcplus5.exe'
$pluginDirectory = Join-Path $resolvedRoot 'Plugins'
$targetPlugin = Join-Path $pluginDirectory 'soundswitch.dll'

if (-not (Test-Path -LiteralPath $coreExecutable -PathType Leaf)) {
    throw "QLC+ executable not found: $coreExecutable"
}
if (-not (Test-Path -LiteralPath $sourcePlugin -PathType Leaf)) {
    throw "Packaged plug-in not found: $sourcePlugin"
}
if (-not (Test-Path -LiteralPath $pluginDirectory -PathType Container)) {
    throw "QLC+ plug-in directory not found: $pluginDirectory"
}
if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close QLC+ before installing the plug-in.'
}

$coreSha256 = (Get-FileHash -LiteralPath $coreExecutable -Algorithm SHA256).Hash
if ($coreSha256 -ne $expectedCoreSha256 -and -not $AllowCompatibleCore) {
    throw "This package is pinned to QLC+ 5.3.0 GIT a124abe. Core SHA-256 was $coreSha256. Use -AllowCompatibleCore only after validating the newer QLC+ build."
}

$sourceSha256 = (Get-FileHash -LiteralPath $sourcePlugin -Algorithm SHA256).Hash
if ($sourceSha256 -ne $expectedPluginSha256) {
    throw "Packaged plug-in hash mismatch. Expected $expectedPluginSha256; found $sourceSha256."
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupDirectory = Join-Path $resolvedRoot "PluginBackups\SoundSwitch\$timestamp"
New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null

$hadPreviousPlugin = Test-Path -LiteralPath $targetPlugin -PathType Leaf
if ($hadPreviousPlugin) {
    Copy-Item -LiteralPath $targetPlugin -Destination (Join-Path $backupDirectory 'soundswitch.dll') -Force
}

$receipt = [ordered]@{
    Package = 'QLC+ SoundSwitch V24 Runtime Feedback'
    InstalledAt = (Get-Date).ToString('o')
    QlcRoot = $resolvedRoot
    CoreSha256 = $coreSha256
    HadPreviousPlugin = $hadPreviousPlugin
    PreviousPluginSha256 = if ($hadPreviousPlugin) {
        (Get-FileHash -LiteralPath (Join-Path $backupDirectory 'soundswitch.dll') -Algorithm SHA256).Hash
    } else {
        $null
    }
    InstalledPluginSha256 = $expectedPluginSha256
}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backupDirectory 'install-receipt.json') -Encoding UTF8

Copy-Item -LiteralPath $sourcePlugin -Destination $targetPlugin -Force
$installedSha256 = (Get-FileHash -LiteralPath $targetPlugin -Algorithm SHA256).Hash
if ($installedSha256 -ne $expectedPluginSha256) {
    throw "Installation verification failed. Roll back from $backupDirectory."
}

Write-Host "Installed SoundSwitch V24 package plug-in: $targetPlugin"
Write-Host "Rollback backup: $backupDirectory"
