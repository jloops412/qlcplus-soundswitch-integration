[CmdletBinding()]
param(
    [string]$ReleaseDirectory = $PSScriptRoot,
    [string]$HostWorkspace
)

$ErrorActionPreference = 'Stop'

$varietyRootIds = @(
    568, 572, 573, 574, 575, 576, 580, 581, 583, 587, 589,
    590, 593, 632, 633, 635, 636, 637, 645, 647, 657, 658
)
$rawLoopIds = 532..659
$priorityLookIds = 5..36
$invalidFunctionId = [uint64]4294967295

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Get-FunctionMap {
    param([xml]$Workspace)

    $map = @{}
    foreach ($function in @($Workspace.SelectNodes(
            '//*[local-name()="Engine"]/*[local-name()="Function"]'))) {
        $id = [int]$function.GetAttribute('ID')
        Assert-Condition (-not $map.ContainsKey($id)) "Duplicate Function ID $id"
        $map[$id] = $function
    }
    return $map
}

function Get-ChildText {
    param([System.Xml.XmlElement]$Node, [string]$Name)
    $child = $Node.SelectSingleNode("./*[local-name()='$Name']")
    if ($null -eq $child) {
        return $null
    }
    return $child.InnerText
}

$release = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$releasePrefix = $release.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
$checksumPath = Join-Path $release 'SHA256SUMS.txt'
Assert-Condition (Test-Path -LiteralPath $checksumPath -PathType Leaf) `
    "Checksum manifest not found: $checksumPath"

$listedFiles = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in Get-Content -LiteralPath $checksumPath) {
    Assert-Condition ($line -match '^([0-9A-Fa-f]{64})  ([^/\\]+)$') `
        "Malformed checksum line: $line"
    $expectedHash = $Matches[1].ToUpperInvariant()
    $name = $Matches[2]
    Assert-Condition $listedFiles.Add($name) "Duplicate checksum entry: $name"

    $target = [System.IO.Path]::GetFullPath((Join-Path $release $name))
    Assert-Condition $target.StartsWith(
        $releasePrefix, [System.StringComparison]::OrdinalIgnoreCase) `
        "Checksum target escapes the release directory: $name"
    Assert-Condition (Test-Path -LiteralPath $target -PathType Leaf) `
        "Checksum target is missing: $name"
    $actualHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    Assert-Condition ($actualHash -eq $expectedHash) "Checksum mismatch: $name"
}

$unlistedFiles = @(Get-ChildItem -LiteralPath $release -File | Where-Object {
    $_.Name -ne 'SHA256SUMS.txt' -and -not $listedFiles.Contains($_.Name)
})
Assert-Condition ($unlistedFiles.Count -eq 0) `
    "Release contains files absent from SHA256SUMS.txt: $($unlistedFiles.Name -join ', ')"

$requiredFiles = @(
    'IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw',
    'SoundSwitch-Control-One-Performance.qxi',
    'soundswitch.dll',
    'Install-SoundSwitchPlugin.ps1',
    'Rollback-SoundSwitchPlugin.ps1',
    'Test-V22Package.ps1',
    'VIRTUALDJ_OS2L_AUTO_RECONNECT.md',
    'RELEASE_NOTES.md',
    'README.md',
    'LICENSE-APACHE-2.0.txt'
)
foreach ($name in $requiredFiles) {
    Assert-Condition $listedFiles.Contains($name) `
        "Required release file is not checksummed: $name"
}

$workspacePath = Join-Path $release 'IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw'
$profilePath = Join-Path $release 'SoundSwitch-Control-One-Performance.qxi'
[xml]$workspace = Get-Content -LiteralPath $workspacePath -Raw
[xml]$profile = Get-Content -LiteralPath $profilePath -Raw
$functions = Get-FunctionMap -Workspace $workspace

Assert-Condition ($functions.Count -eq 2090) `
    "Expected 2,090 V22 Functions; found $($functions.Count)."

$widgetIds = [System.Collections.Generic.HashSet[uint64]]::new()
foreach ($widget in @($workspace.SelectNodes(
        '//*[local-name()="VirtualConsole"]//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [uint64]$widget.GetAttribute('ID')
    Assert-Condition $widgetIds.Add($id) "Duplicate Virtual Console widget ID $id"
}

$missingVcReferences = [System.Collections.Generic.List[uint64]]::new()
foreach ($reference in @($workspace.SelectNodes(
        '//*[local-name()="VirtualConsole"]//*[local-name()="Function"]'))) {
    $raw = if ($reference.HasAttribute('ID')) {
        $reference.GetAttribute('ID')
    } else {
        $reference.InnerText.Trim()
    }
    Assert-Condition ($raw -match '^\d+$') `
        "Malformed Virtual Console Function reference: $raw"
    $id = [uint64]$raw
    if ($id -ne $invalidFunctionId -and -not $functions.ContainsKey([int]$id)) {
        $missingVcReferences.Add($id)
    }
}
Assert-Condition ($missingVcReferences.Count -eq 0) `
    "Missing Virtual Console Function references: $($missingVcReferences -join ', ')"

$fixtures = @($workspace.SelectNodes(
    '//*[local-name()="Engine"]/*[local-name()="Fixture"]'))
$fixtureIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($fixture in $fixtures) {
    $id = [int](Get-ChildText -Node $fixture -Name 'ID')
    Assert-Condition $fixtureIds.Add($id) "Duplicate Fixture ID $id"
}
Assert-Condition ($fixtures.Count -eq 16) `
    "Expected 8 physical and 8 private fixtures; found $($fixtures.Count)."

foreach ($function in $functions.Values) {
    foreach ($step in @($function.SelectNodes('./*[local-name()="Step"]'))) {
        if ([string]::IsNullOrWhiteSpace($step.InnerText)) {
            continue
        }
        $reference = [int]$step.InnerText.Trim()
        Assert-Condition $functions.ContainsKey($reference) `
            "Function $($function.GetAttribute('ID')) references missing Function $reference."
    }
    foreach ($value in @($function.SelectNodes('./*[local-name()="FixtureVal"]'))) {
        $fixtureId = [int]$value.GetAttribute('ID')
        Assert-Condition $fixtureIds.Contains($fixtureId) `
            "Function $($function.GetAttribute('ID')) targets missing Fixture $fixtureId."
    }
}

$expectedPatch = @(
    @{ Address = 0;   Channels = 10; Mode = '10 Channel' },
    @{ Address = 10;  Channels = 10; Mode = '10 Channel' },
    @{ Address = 20;  Channels = 10; Mode = '10 Channel' },
    @{ Address = 30;  Channels = 10; Mode = '10 Channel' },
    @{ Address = 174; Channels = 40; Mode = '40 Channel' },
    @{ Address = 214; Channels = 40; Mode = '40 Channel' },
    @{ Address = 254; Channels = 40; Mode = '40 Channel' },
    @{ Address = 294; Channels = 40; Mode = '40 Channel' }
)
foreach ($expected in $expectedPatch) {
    foreach ($universe in @(0, 2)) {
        $matches = @($fixtures | Where-Object {
            [int](Get-ChildText -Node $_ -Name 'Universe') -eq $universe -and
            [int](Get-ChildText -Node $_ -Name 'Address') -eq $expected.Address -and
            [int](Get-ChildText -Node $_ -Name 'Channels') -eq $expected.Channels -and
            [string](Get-ChildText -Node $_ -Name 'Mode') -ceq $expected.Mode
        })
        Assert-Condition ($matches.Count -eq 1) `
            "Fixture patch mismatch at Universe $($universe + 1), address $($expected.Address + 1)."
    }
}

$rawFunctions = @($rawLoopIds | ForEach-Object { $functions[$_] })
Assert-Condition ($rawFunctions.Count -eq 128 -and
    @($rawFunctions | Where-Object { $_.GetAttribute('Type') -cne 'Chaser' }).Count -eq 0) `
    'Raw Autoloop contract is incomplete.'

$manualOwners = @($functions.Values | Where-Object {
    $_.GetAttribute('Name').StartsWith('MANUAL LOOP [', [System.StringComparison]::Ordinal)
})
$autoplayControls = @($functions.Values | Where-Object {
    $_.GetAttribute('Name').StartsWith('AUTOPLAY CONTROL ', [System.StringComparison]::Ordinal)
})
$autoplayParents = @($functions.Values | Where-Object {
    $_.GetAttribute('Type') -ceq 'Chaser' -and (
        $_.GetAttribute('Name').StartsWith('AUTOPLAY BANK [', [System.StringComparison]::Ordinal) -or
        $_.GetAttribute('Name').StartsWith('AUTOPLAY ALL BANKS -', [System.StringComparison]::Ordinal))
})
Assert-Condition ($manualOwners.Count -eq 128) `
    "Expected 128 manual owners; found $($manualOwners.Count)."
Assert-Condition ($autoplayControls.Count -eq 10) `
    "Expected 10 Autoplay controls; found $($autoplayControls.Count)."
Assert-Condition ($autoplayParents.Count -eq 10) `
    "Expected 10 Autoplay parent Chasers; found $($autoplayParents.Count)."

$varietyHelperIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($rootId in $varietyRootIds) {
    $root = $functions[$rootId]
    $steps = @($root.SelectNodes('./*[local-name()="Step"]'))
    Assert-Condition ($steps.Count -eq 8) `
        "Variety Pro Autoloop $rootId does not contain eight steps."
    foreach ($step in $steps) {
        $helperId = [int]$step.InnerText.Trim()
        $helper = $functions[$helperId]
        Assert-Condition ($helper.GetAttribute('Type') -ceq 'Scene') `
            "Variety Pro helper $helperId is not a Scene."
        Assert-Condition ($helper.GetAttribute('Name').StartsWith(
            'HELPER - VARIETY PRO ', [System.StringComparison]::Ordinal)) `
            "Autoloop $rootId references a non-Variety helper $helperId."
        [void]$varietyHelperIds.Add($helperId)
    }
}
Assert-Condition ($varietyHelperIds.Count -eq 176) `
    "Expected 176 imported Variety Pro Scenes; found $($varietyHelperIds.Count)."

$priorityFixtureIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($id in @(100, 101, 102, 103, 105, 106, 107, 108)) {
    [void]$priorityFixtureIds.Add($id)
}
$priorityClosure = [System.Collections.Generic.HashSet[int]]::new()
$priorityQueue = [System.Collections.Generic.Queue[int]]::new()
foreach ($id in $priorityLookIds) {
    [void]$priorityClosure.Add($id)
    $priorityQueue.Enqueue($id)
}
while ($priorityQueue.Count -gt 0) {
    $id = $priorityQueue.Dequeue()
    foreach ($step in @($functions[$id].SelectNodes('./*[local-name()="Step"]'))) {
        $dependency = [int]$step.InnerText.Trim()
        if ($priorityClosure.Add($dependency)) {
            $priorityQueue.Enqueue($dependency)
        }
    }
}
foreach ($id in $priorityClosure) {
    foreach ($value in @($functions[$id].SelectNodes('./*[local-name()="FixtureVal"]'))) {
        Assert-Condition $priorityFixtureIds.Contains([int]$value.GetAttribute('ID')) `
            "Priority Function $id escaped the private fixture layer."
    }
}

$outlineFrames = @($workspace.SelectNodes(
    '//*[local-name()="VirtualConsole"]//*[local-name()="Frame" and starts-with(@Caption,"ACTIVE LOOP OUTLINE")]'))
Assert-Condition ($outlineFrames.Count -eq 1) `
    "Expected one active-loop outline layer; found $($outlineFrames.Count)."
$outline = $outlineFrames[0]
Assert-Condition ((Get-ChildText -Node $outline -Name 'Disabled') -ceq 'True') `
    'Active-loop outline must remain read-only.'
Assert-Condition (@($outline.SelectNodes('ancestor::*[local-name()="SoloFrame"]')).Count -eq 0) `
    'Active-loop outline must remain outside the playback-owner SoloFrame.'
$outlineButtons = @($outline.SelectNodes('./*[local-name()="Button"]'))
Assert-Condition ($outlineButtons.Count -eq 128) `
    "Expected 128 active-loop monitors; found $($outlineButtons.Count)."
Assert-Condition (@($outlineButtons | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input"]')).Count -ne 0
}).Count -eq 0) 'Active-loop monitors must not own external inputs.'
$outlineRawIds = @($outlineButtons | ForEach-Object {
    [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
} | Sort-Object)
Assert-Condition ((Compare-Object $rawLoopIds $outlineRawIds).Count -eq 0) `
    'Active-loop outline does not cover all 128 raw Chasers exactly once.'

$channels = @($profile.SelectNodes('//*[local-name()="Channel"]') | ForEach-Object {
    [int]$_.GetAttribute('Number')
})
$duplicateChannels = @($channels | Group-Object | Where-Object Count -gt 1)
Assert-Condition ($duplicateChannels.Count -eq 0) `
    "Duplicate input-profile channels: $($duplicateChannels.Name -join ', ')"

$scriptParseErrors = [System.Collections.Generic.List[string]]::new()
foreach ($scriptName in @(
        'Install-SoundSwitchPlugin.ps1',
        'Rollback-SoundSwitchPlugin.ps1',
        'Test-V22Package.ps1')) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $release $scriptName), [ref]$tokens, [ref]$errors) | Out-Null
    foreach ($error in @($errors)) {
        $scriptParseErrors.Add("$scriptName`: $($error.Message)")
    }
}
Assert-Condition ($scriptParseErrors.Count -eq 0) `
    "PowerShell parser errors: $($scriptParseErrors -join '; ')"

if (-not [string]::IsNullOrWhiteSpace($HostWorkspace)) {
    [xml]$hostDocument = Get-Content -LiteralPath (
        (Resolve-Path -LiteralPath $HostWorkspace).Path) -Raw
    $hostFunctions = Get-FunctionMap -Workspace $hostDocument
    $actualChanges = [System.Collections.Generic.List[int]]::new()
    foreach ($id in $hostFunctions.Keys) {
        Assert-Condition $functions.ContainsKey($id) "V21 host Function $id was removed."
        if ($hostFunctions[$id].OuterXml -cne $functions[$id].OuterXml) {
            $actualChanges.Add($id)
        }
    }
    Assert-Condition ((Compare-Object $varietyRootIds @($actualChanges)).Count -eq 0) `
        "Unexpected V21 Function changes: $($actualChanges -join ', ')"

    $hostFixtures = @($hostDocument.SelectNodes(
        '//*[local-name()="Engine"]/*[local-name()="Fixture"]') | ForEach-Object OuterXml)
    Assert-Condition (($hostFixtures -join "`n") -ceq
        (@($fixtures | ForEach-Object OuterXml) -join "`n")) `
        'V21 fixture XML changed in V22.'
    Assert-Condition ($hostDocument.SelectSingleNode(
        '//*[local-name()="Engine"]/*[local-name()="InputOutputMap"]').OuterXml -ceq
        $workspace.SelectSingleNode(
        '//*[local-name()="Engine"]/*[local-name()="InputOutputMap"]').OuterXml) `
        'V21 Input/Output map changed in V22.'
    foreach ($id in $priorityLookIds) {
        Assert-Condition ($hostFunctions[$id].OuterXml -ceq $functions[$id].OuterXml) `
            "V21 Priority Look $id changed in V22."
    }
}

$textFiles = Get-ChildItem -LiteralPath $release -File | Where-Object {
    $_.Extension -in @('.md', '.ps1', '.txt', '.qxw', '.qxi')
}
$personalLeaks = @($textFiles | Select-String -Pattern `
    'C:\\Users\\|\bjloop\b|\bJ:\\' -CaseSensitive:$false)
Assert-Condition ($personalLeaks.Count -eq 0) `
    'A personal username or absolute DJ-PC path appears in the release package.'

$pluginHash = (Get-FileHash -LiteralPath (
    Join-Path $release 'soundswitch.dll') -Algorithm SHA256).Hash
Assert-Condition ($pluginHash -eq
    'AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D') `
    'V22 must reuse the exact software-tested V21 plug-in binary.'

Write-Host 'PASS: QLC+ SoundSwitch V22 Unified Pro package integrity'
Write-Host "  Files checksummed: $($listedFiles.Count)"
Write-Host "  Workspace Functions: $($functions.Count)"
Write-Host '  Creative merge: 22 raw Chasers + 176 Variety Pro Scene steps'
Write-Host '  Active-loop outline: 128 raw Chasers, manual/Auto Bank/Auto All'
Write-Host '  Fixture patch: 8 physical + 8 private Priority Layer fixtures'
if (-not [string]::IsNullOrWhiteSpace($HostWorkspace)) {
    Write-Host '  V21 host delta: exactly 22 raw Chasers; every existing owner unchanged'
}
