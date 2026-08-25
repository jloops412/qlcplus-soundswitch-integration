[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot
)

$ErrorActionPreference = 'Stop'

$expectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$soundSwitchSha256 = '2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC'
$os2lSha256 = 'EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513'

$resolvedRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$coreExecutable = Join-Path $resolvedRoot 'qlcplus5.exe'
$pluginDirectory = Join-Path $resolvedRoot 'Plugins'
$sourceSoundSwitch = Join-Path $PSScriptRoot 'soundswitch.dll'
$sourceOs2l = Join-Path $PSScriptRoot 'os2l.dll'
$targetSoundSwitch = Join-Path $pluginDirectory 'soundswitch.dll'
$targetOs2l = Join-Path $pluginDirectory 'os2l.dll'

if (Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue) {
    throw 'Close every QLC+ window before installing V26.'
}
if (-not (Test-Path -LiteralPath $coreExecutable -PathType Leaf)) {
    throw "QLC+ executable not found: $coreExecutable"
}
if (-not (Test-Path -LiteralPath $pluginDirectory -PathType Container)) {
    throw "QLC+ plug-in directory not found: $pluginDirectory"
}
foreach ($path in @($sourceSoundSwitch, $sourceOs2l)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Packaged plug-in not found: $path"
    }
}

$coreSha256 = (Get-FileHash -LiteralPath $coreExecutable -Algorithm SHA256).Hash
if ($coreSha256 -ne $expectedCoreSha256) {
    throw "V26 requires the pinned complete QLC+ 5.3.0 GIT a124abe installation. Core SHA-256 was $coreSha256. Install another QLC+ version side-by-side instead of mixing binaries."
}

$packageHashes = [ordered]@{
    $sourceSoundSwitch = $soundSwitchSha256
    $sourceOs2l = $os2lSha256
}
foreach ($entry in $packageHashes.GetEnumerator()) {
    $actual = (Get-FileHash -LiteralPath $entry.Key -Algorithm SHA256).Hash
    if ($actual -ne $entry.Value) {
        throw "Package hash mismatch for $($entry.Key). Expected $($entry.Value); found $actual."
    }
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backupRoot = Join-Path $resolvedRoot 'PluginBackups\QLC-SoundSwitch-V26'
$backupDirectory = Join-Path $backupRoot $timestamp
New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null

$targets = @(
    [ordered]@{ Name = 'soundswitch.dll'; Target = $targetSoundSwitch; Source = $sourceSoundSwitch; Expected = $soundSwitchSha256 },
    [ordered]@{ Name = 'os2l.dll'; Target = $targetOs2l; Source = $sourceOs2l; Expected = $os2lSha256 }
)

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
        InstalledSha256 = $item.Expected
    })
}

$receipt = [ordered]@{
    Package = 'QLC+ SoundSwitch V26 Autoplay Clarity'
    InstalledAt = (Get-Date).ToString('o')
    QlcRoot = $resolvedRoot
    CoreSha256 = $coreSha256
    Files = $receiptFiles
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (
    Join-Path $backupDirectory 'install-receipt.json') -Encoding UTF8

foreach ($item in $targets) {
    Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force
    $installedHash = (Get-FileHash -LiteralPath $item.Target -Algorithm SHA256).Hash
    if ($installedHash -ne $item.Expected) {
        throw "Installation verification failed for $($item.Name). Roll back from $backupDirectory."
    }
}

Write-Host 'Installed V26 into the pinned stock QLC+ core.'
Write-Host '  SoundSwitch USB output: soundswitch.dll'
Write-Host '  Stable direct VirtualDJ timing: os2l.dll'
Write-Host "Rollback backup: $backupDirectory"
