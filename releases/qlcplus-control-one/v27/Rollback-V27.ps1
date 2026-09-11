[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,
    [string]$BackupDirectory,
    [string]$QlcUserDataRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$packageName = 'QLC+ SoundSwitch V27 Full Rig'
$profileName = 'SoundSwitch-Control-One-Performance.qxi'
$fixtureName = 'American-DJ-Focus-Spot-Two.qxf'
$manifestName = 'package-SHA256SUMS.txt'

function Read-PackageHashes {
    param([Parameter(Mandatory)][string]$Path)

    $hashes = @{}
    foreach ($line in @(Get-Content -LiteralPath $Path)) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line -notmatch '^([A-Fa-f0-9]{64})  ([^\\/]+)$') {
            throw "Malformed package checksum line: $line"
        }
        $hash = $Matches[1].ToUpperInvariant()
        $name = $Matches[2]
        if ($name -eq 'SHA256SUMS.txt') {
            throw 'The package checksum manifest must not hash itself.'
        }
        if ($hashes.ContainsKey($name)) {
            throw "Duplicate package checksum entry: $name"
        }
        $hashes[$name] = $hash
    }
    return $hashes
}

if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close every QLC+ window before rolling back V27.'
}

$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$backupRoot = Join-Path $resolvedRoot 'PluginBackups\QLC-SoundSwitch-V27'

if ([string]::IsNullOrWhiteSpace($QlcUserDataRoot)) {
    $userProfile = $env:USERPROFILE
    if ([string]::IsNullOrWhiteSpace($userProfile)) {
        $userProfile = [Environment]::GetFolderPath('UserProfile')
    }
    if ([string]::IsNullOrWhiteSpace($userProfile)) {
        throw 'The Windows user profile directory could not be resolved. Supply -QlcUserDataRoot explicitly.'
    }
    $QlcUserDataRoot = Join-Path $userProfile 'QLC+'
}
$confirmedUserDataRoot = [System.IO.Path]::GetFullPath($QlcUserDataRoot)

if ([string]::IsNullOrWhiteSpace($BackupDirectory)) {
    if (-not (Test-Path -LiteralPath $backupRoot -PathType Container)) {
        throw "No V27 backup exists under $backupRoot"
    }
    $candidates = [System.Collections.Generic.List[object]]::new()
    foreach ($directory in @(Get-ChildItem -LiteralPath $backupRoot -Directory)) {
        $candidateReceipt = Join-Path $directory.FullName 'install-receipt.json'
        if (-not (Test-Path -LiteralPath $candidateReceipt -PathType Leaf)) { continue }
        try {
            $candidate = Get-Content -LiteralPath $candidateReceipt -Raw | ConvertFrom-Json
            $candidateRoot = [System.IO.Path]::GetFullPath([string]$candidate.QlcRoot)
            $candidateUserDataRoot =
                [System.IO.Path]::GetFullPath([string]$candidate.QlcUserDataRoot)
            if ([string]$candidate.Package -eq $packageName -and
                $candidateRoot.Equals(
                    $resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
                $candidateUserDataRoot.Equals(
                    $confirmedUserDataRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
                $candidates.Add($directory)
            }
        } catch {
            # A damaged or unrelated receipt is not a rollback candidate.  An
            # explicitly selected directory still produces a detailed error.
        }
    }
    $latest = $candidates | Sort-Object Name -Descending | Select-Object -First 1
    if ($null -eq $latest) {
        throw "No valid V27 install receipt exists under $backupRoot"
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
if ($receipt.Package -ne $packageName) {
    throw 'The selected receipt is not a V27 Full Rig install receipt.'
}
$receiptRoot = [System.IO.Path]::GetFullPath([string]$receipt.QlcRoot)
if (-not $receiptRoot.Equals($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The selected receipt belongs to a different QLC+ root: $receiptRoot"
}
if ([string]::IsNullOrWhiteSpace([string]$receipt.QlcUserDataRoot)) {
    throw 'The selected receipt has no QLC+ user-data root.'
}
$receiptUserDataRoot = [System.IO.Path]::GetFullPath([string]$receipt.QlcUserDataRoot)
if (-not $receiptUserDataRoot.Equals(
        $confirmedUserDataRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The selected receipt belongs to QLC+ user data at $receiptUserDataRoot. Re-run rollback with -QlcUserDataRoot set to that exact path if it is intentional."
}
$userDataRoot = $confirmedUserDataRoot

$backupManifest = Join-Path $resolvedBackup $manifestName
if (-not (Test-Path -LiteralPath $backupManifest -PathType Leaf)) {
    throw "Install-time package checksum manifest not found: $backupManifest"
}
$manifestSha256 = (Get-FileHash -LiteralPath $backupManifest -Algorithm SHA256).Hash
if ($manifestSha256 -ne [string]$receipt.ManifestSha256) {
    throw 'The install-time package checksum manifest does not match its receipt.'
}
$packageHashes = Read-PackageHashes -Path $backupManifest

$expectedTargets = @{}
$expectedTargets['soundswitch.dll'] =
    Join-Path (Join-Path $resolvedRoot 'Plugins') 'soundswitch.dll'
$expectedTargets['os2l.dll'] =
    Join-Path (Join-Path $resolvedRoot 'Plugins') 'os2l.dll'
$expectedTargets[$profileName] =
    Join-Path (Join-Path $userDataRoot 'InputProfiles') $profileName
$expectedTargets[$fixtureName] =
    Join-Path (Join-Path $userDataRoot 'Fixtures') $fixtureName

$receiptFiles = @($receipt.Files)
if ($receiptFiles.Count -ne $expectedTargets.Count) {
    throw 'The V27 receipt does not contain the exact installed file set.'
}
$seenNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($file in $receiptFiles) {
    $name = [string]$file.Name
    if (-not $seenNames.Add($name)) {
        throw "Duplicate file in V27 receipt: $name"
    }
    if (-not $expectedTargets.ContainsKey($name)) {
        throw "Unexpected file in V27 receipt: $name"
    }
    if (-not $packageHashes.ContainsKey($name)) {
        throw "Install-time package checksum missing: $name"
    }
    if ([string]$file.InstalledSha256 -ne $packageHashes[$name]) {
        throw "Installed hash in receipt does not match the install-time manifest: $name"
    }
    $receiptTarget = [System.IO.Path]::GetFullPath([string]$file.Target)
    $expectedTarget = [System.IO.Path]::GetFullPath([string]$expectedTargets[$name])
    if (-not $receiptTarget.Equals(
            $expectedTarget, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Unsafe or unexpected rollback target for $name`: $receiptTarget"
    }
    if (Test-Path -LiteralPath $receiptTarget -PathType Container) {
        throw "Rollback target is a directory, not a file: $receiptTarget"
    }
    if ([bool]$file.HadPrevious) {
        $backupFile = Join-Path $resolvedBackup $name
        if (-not (Test-Path -LiteralPath $backupFile -PathType Leaf)) {
            throw "Backed-up file not found: $backupFile"
        }
        $backupHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash
        if ($backupHash -ne [string]$file.PreviousSha256) {
            throw "Backed-up $name does not match its receipt."
        }
    }
}

$rollbackTimestamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$rollbackUnique = "$rollbackTimestamp-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
$displacedDirectory = $null
$displacedFiles = [System.Collections.Generic.List[object]]::new()
foreach ($file in $receiptFiles) {
    $target = [System.IO.Path]::GetFullPath([string]$file.Target)
    if (-not (Test-Path -LiteralPath $target -PathType Leaf)) { continue }
    $currentHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($currentHash -eq [string]$file.InstalledSha256) { continue }
    if ($null -eq $displacedDirectory) {
        $displacedDirectory = Join-Path $resolvedBackup "rollback-displaced-$rollbackUnique"
        New-Item -ItemType Directory -Path $displacedDirectory -ErrorAction Stop | Out-Null
    }
    $displacedPath = Join-Path $displacedDirectory ([string]$file.Name)
    Copy-Item -LiteralPath $target -Destination $displacedPath -Force
    $displacedFiles.Add([ordered]@{
        Name = [string]$file.Name
        OriginalTarget = $target
        PreservedAt = $displacedPath
        Sha256 = $currentHash
    })
}

foreach ($file in $receiptFiles) {
    $name = [string]$file.Name
    $target = [System.IO.Path]::GetFullPath([string]$file.Target)
    if ([bool]$file.HadPrevious) {
        $backupFile = Join-Path $resolvedBackup $name
        $targetDirectory = Split-Path -Parent $target
        New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
        Copy-Item -LiteralPath $backupFile -Destination $target -Force
        $restoredHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
        if ($restoredHash -ne [string]$file.PreviousSha256) {
            throw "Rollback verification failed for $name."
        }
        Write-Host "Restored $name"
    } elseif (Test-Path -LiteralPath $target -PathType Leaf) {
        Remove-Item -LiteralPath $target
        Write-Host "Removed $name; no prior copy existed."
    }
}

$rollbackRecord = [ordered]@{
    Package = $packageName
    RolledBackAt = (Get-Date).ToString('o')
    BackupDirectory = $resolvedBackup
    PreservedChangedFiles = $displacedFiles
}
$rollbackRecordPath = Join-Path $resolvedBackup "rollback-receipt-$rollbackUnique.json"
$rollbackRecord | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $rollbackRecordPath -Encoding UTF8

Write-Host "Rollback complete from: $resolvedBackup"
if ($null -ne $displacedDirectory) {
    Write-Host "Post-install files that differed from V27 were preserved at: $displacedDirectory"
}
Write-Host "Rollback receipt: $rollbackRecordPath"
