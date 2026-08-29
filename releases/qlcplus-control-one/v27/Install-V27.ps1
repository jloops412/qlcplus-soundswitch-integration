[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,
    [string]$QlcUserDataRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$expectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$packageName = 'QLC+ SoundSwitch V27 Full Rig'
$workspaceName = 'IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw'
$profileName = 'SoundSwitch-Control-One-Performance.qxi'
$fixtureName = 'American-DJ-Focus-Spot-Two.qxf'
$manifestName = 'SHA256SUMS.txt'

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
        if ($name -eq $manifestName) {
            throw 'SHA256SUMS.txt must not hash itself.'
        }
        if ($hashes.ContainsKey($name)) {
            throw "Duplicate package checksum entry: $name"
        }
        $hashes[$name] = $hash
    }
    return $hashes
}

if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close every QLC+ window before installing V27.'
}

$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$coreExecutable = Join-Path $resolvedRoot 'qlcplus5.exe'
$pluginDirectory = Join-Path $resolvedRoot 'Plugins'
if (-not (Test-Path -LiteralPath $coreExecutable -PathType Leaf)) {
    throw "QLC+ executable not found: $coreExecutable"
}
if (-not (Test-Path -LiteralPath $pluginDirectory -PathType Container)) {
    throw "QLC+ plug-in directory not found: $pluginDirectory"
}

$coreSha256 = (Get-FileHash -LiteralPath $coreExecutable -Algorithm SHA256).Hash
if ($coreSha256 -ne $expectedCoreSha256) {
    throw "V27 requires the pinned complete QLC+ 5.3.0 GIT a124abe installation. Core SHA-256 was $coreSha256. Install another QLC+ version side-by-side instead of mixing binaries."
}

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
$resolvedUserDataRoot = [System.IO.Path]::GetFullPath($QlcUserDataRoot)
$userFixtureDirectory = Join-Path $resolvedUserDataRoot 'Fixtures'
$userInputProfileDirectory = Join-Path $resolvedUserDataRoot 'InputProfiles'

$manifestPath = Join-Path $PSScriptRoot $manifestName
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Package checksum manifest not found: $manifestPath"
}

# Snapshot the authoritative package manifest before reading or using it.  The
# receipt binds this exact copy to the install, so a later package-directory
# change cannot make rollback validate against different hashes.
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$backupRoot = Join-Path $resolvedRoot 'PluginBackups\QLC-SoundSwitch-V27'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$backupLeaf = "$timestamp-$([Guid]::NewGuid().ToString('N').Substring(0, 8))"
$backupDirectory = Join-Path $backupRoot $backupLeaf
New-Item -ItemType Directory -Path $backupDirectory -ErrorAction Stop | Out-Null
$backupManifest = Join-Path $backupDirectory 'package-SHA256SUMS.txt'
Copy-Item -LiteralPath $manifestPath -Destination $backupManifest
$manifestSha256 = (Get-FileHash -LiteralPath $backupManifest -Algorithm SHA256).Hash
$packageHashes = Read-PackageHashes -Path $backupManifest
$packageAssetNames = @(
    'soundswitch.dll',
    'os2l.dll',
    $profileName,
    $fixtureName,
    $workspaceName
)
foreach ($name in $packageAssetNames) {
    if (-not $packageHashes.ContainsKey($name)) {
        throw "Package checksum entry missing: $name"
    }
    $source = Join-Path $PSScriptRoot $name
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Packaged asset not found: $source"
    }
    $actual = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ($actual -ne $packageHashes[$name]) {
        throw "Package hash mismatch for $name. Expected $($packageHashes[$name]); found $actual."
    }
}

New-Item -ItemType Directory -Path $userFixtureDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $userInputProfileDirectory -Force | Out-Null

$targets = @(
    [ordered]@{
        Name = 'soundswitch.dll'
        Source = Join-Path $PSScriptRoot 'soundswitch.dll'
        Target = Join-Path $pluginDirectory 'soundswitch.dll'
    },
    [ordered]@{
        Name = 'os2l.dll'
        Source = Join-Path $PSScriptRoot 'os2l.dll'
        Target = Join-Path $pluginDirectory 'os2l.dll'
    },
    [ordered]@{
        Name = $profileName
        Source = Join-Path $PSScriptRoot $profileName
        Target = Join-Path $userInputProfileDirectory $profileName
    },
    [ordered]@{
        Name = $fixtureName
        Source = Join-Path $PSScriptRoot $fixtureName
        Target = Join-Path $userFixtureDirectory $fixtureName
    }
)

foreach ($item in $targets) {
    if (Test-Path -LiteralPath $item.Target -PathType Container) {
        throw "Install target is a directory, not a file: $($item.Target)"
    }
}

$receiptFiles = [System.Collections.Generic.List[object]]::new()
foreach ($item in $targets) {
    $hadPrevious = Test-Path -LiteralPath $item.Target -PathType Leaf
    $previousHash = $null
    if ($hadPrevious) {
        $backupPath = Join-Path $backupDirectory $item.Name
        Copy-Item -LiteralPath $item.Target -Destination $backupPath -Force
        $previousHash = (Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash
    }
    $receiptFiles.Add([ordered]@{
        Name = $item.Name
        Target = $item.Target
        HadPrevious = $hadPrevious
        PreviousSha256 = $previousHash
        InstalledSha256 = $packageHashes[$item.Name]
    })
}

$receipt = [ordered]@{
    Package = $packageName
    InstalledAt = (Get-Date).ToString('o')
    QlcRoot = $resolvedRoot
    QlcUserDataRoot = $resolvedUserDataRoot
    CoreSha256 = $coreSha256
    ManifestSha256 = $manifestSha256
    Workspace = [ordered]@{
        Name = $workspaceName
        PackagePath = Join-Path $PSScriptRoot $workspaceName
        Sha256 = $packageHashes[$workspaceName]
    }
    Files = $receiptFiles
}
$receiptPath = Join-Path $backupDirectory 'install-receipt.json'
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath -Encoding UTF8

foreach ($item in $targets) {
    Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force
    $installedHash = (Get-FileHash -LiteralPath $item.Target -Algorithm SHA256).Hash
    if ($installedHash -ne $packageHashes[$item.Name]) {
        throw "Installation verification failed for $($item.Name). Roll back from $backupDirectory."
    }
}

Write-Host 'Installed V27 into the pinned stock QLC+ core and current user profile.'
Write-Host "  SoundSwitch USB output: $(Join-Path $pluginDirectory 'soundswitch.dll')"
Write-Host "  Stable direct VirtualDJ timing: $(Join-Path $pluginDirectory 'os2l.dll')"
Write-Host "  Control One input profile: $(Join-Path $userInputProfileDirectory $profileName)"
Write-Host "  Focus Spot Two fixture definition: $(Join-Path $userFixtureDirectory $fixtureName)"
Write-Host "  Open the packaged workspace: $(Join-Path $PSScriptRoot $workspaceName)"
Write-Host "Rollback backup: $backupDirectory"
