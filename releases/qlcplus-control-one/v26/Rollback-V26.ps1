[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,
    [string]$BackupDirectory
)

$ErrorActionPreference = 'Stop'

$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$backupRoot = Join-Path $resolvedRoot 'PluginBackups\QLC-SoundSwitch-V26'
if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close every QLC+ window before rolling back V26.'
}

if ([string]::IsNullOrWhiteSpace($BackupDirectory)) {
    $latest = Get-ChildItem -LiteralPath $backupRoot -Directory -ErrorAction Stop |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -eq $latest) {
        throw "No V26 backup exists under $backupRoot"
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
if ($receipt.Package -ne 'QLC+ SoundSwitch V26 Autoplay Clarity') {
    throw 'The selected receipt is not a V26 install receipt.'
}

foreach ($file in $receipt.Files) {
    if ($file.HadPrevious) {
        $backupFile = Join-Path $resolvedBackup $file.Name
        if (-not (Test-Path -LiteralPath $backupFile -PathType Leaf)) {
            throw "Backed-up file not found: $backupFile"
        }
        $backupHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash
        if ($backupHash -ne $file.PreviousSha256) {
            throw "Backed-up $($file.Name) does not match its receipt."
        }
        Copy-Item -LiteralPath $backupFile -Destination $file.Target -Force
        $restoredHash = (Get-FileHash -LiteralPath $file.Target -Algorithm SHA256).Hash
        if ($restoredHash -ne $file.PreviousSha256) {
            throw "Rollback verification failed for $($file.Name)."
        }
        Write-Host "Restored $($file.Name)"
    } elseif (Test-Path -LiteralPath $file.Target -PathType Leaf) {
        Remove-Item -LiteralPath $file.Target
        Write-Host "Removed $($file.Name); no prior copy existed."
    }
}

Write-Host "Rollback complete from: $resolvedBackup"
