[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,
    [string]$BackupDirectory
)

$ErrorActionPreference = 'Stop'

$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$pluginDirectory = Join-Path $resolvedRoot 'Plugins'
$targetPlugin = Join-Path $pluginDirectory 'soundswitch.dll'
$backupRoot = Join-Path $resolvedRoot 'PluginBackups\SoundSwitch'

if (-not (Test-Path -LiteralPath $pluginDirectory -PathType Container)) {
    throw "QLC+ plug-in directory not found: $pluginDirectory"
}
if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close QLC+ before rolling back the plug-in.'
}

if ([string]::IsNullOrWhiteSpace($BackupDirectory)) {
    $latest = Get-ChildItem -LiteralPath $backupRoot -Directory -ErrorAction Stop |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $latest) {
        throw "No SoundSwitch plug-in backup exists under $backupRoot"
    }
    $BackupDirectory = $latest.FullName
}

$resolvedBackup = (Resolve-Path -LiteralPath $BackupDirectory).Path
$safeBackupPrefix = $backupRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
if (-not $resolvedBackup.StartsWith(
        $safeBackupPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Backup must be inside $backupRoot"
}

$receiptPath = Join-Path $resolvedBackup 'install-receipt.json'
if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
    throw "Install receipt not found: $receiptPath"
}
$receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json

if ($receipt.HadPreviousPlugin) {
    $backupPlugin = Join-Path $resolvedBackup 'soundswitch.dll'
    if (-not (Test-Path -LiteralPath $backupPlugin -PathType Leaf)) {
        throw "Backed-up plug-in not found: $backupPlugin"
    }
    $backupSha256 = (Get-FileHash -LiteralPath $backupPlugin -Algorithm SHA256).Hash
    if ($backupSha256 -ne $receipt.PreviousPluginSha256) {
        throw 'Backed-up plug-in hash does not match its install receipt.'
    }
    Copy-Item -LiteralPath $backupPlugin -Destination $targetPlugin -Force
    $restoredSha256 = (Get-FileHash -LiteralPath $targetPlugin -Algorithm SHA256).Hash
    if ($restoredSha256 -ne $receipt.PreviousPluginSha256) {
        throw 'Rollback verification failed.'
    }
    Write-Host "Restored previous SoundSwitch plug-in: $targetPlugin"
} elseif (Test-Path -LiteralPath $targetPlugin -PathType Leaf) {
    Remove-Item -LiteralPath $targetPlugin
    Write-Host "Removed the installed SoundSwitch plug-in; no prior plug-in existed."
}

Write-Host "Rollback source: $resolvedBackup"
