[CmdletBinding()]
param(
    [string]$ReleaseDirectory,
    [string]$SourceWorkspace
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ReleaseDirectory)) {
    $ReleaseDirectory = $PSScriptRoot
}

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
    'IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw',
    'SoundSwitch-Control-One-Performance.qxi',
    'soundswitch.dll',
    'Install-SoundSwitchPlugin.ps1',
    'Rollback-SoundSwitchPlugin.ps1',
    'Test-V24Package.ps1',
    'VIRTUALDJ_OS2L_AUTO_RECONNECT.md',
    'RELEASE_NOTES.md',
    'README.md',
    'LICENSE-APACHE-2.0.txt'
)
foreach ($name in $requiredFiles) {
    Assert-Condition $listedFiles.Contains($name) `
        "Required release file is not checksummed: $name"
}

$workspacePath = Join-Path $release 'IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'
$profilePath = Join-Path $release 'SoundSwitch-Control-One-Performance.qxi'
[xml]$workspace = Get-Content -LiteralPath $workspacePath -Raw
[xml]$profile = Get-Content -LiteralPath $profilePath -Raw
$functions = Get-FunctionMap -Workspace $workspace

Assert-Condition ($functions.Count -eq 2090) `
    "Expected 2,090 V24 Functions; found $($functions.Count)."

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

$legacyOutline = $workspace.SelectSingleNode(
    '//*[local-name()="VirtualConsole"]//*[local-name()="Frame" and @ID="1413"]')
Assert-Condition ($null -eq $legacyOutline) `
    'The hidden V23 active-loop layer still exists.'

$monitorFrames = @($workspace.SelectNodes(
    '//*[local-name()="VirtualConsole"]//*[local-name()="Frame" and number(@ID)>=1542 and number(@ID)<=1573]'))
Assert-Condition ($monitorFrames.Count -eq 32) `
    "Expected 32 visible live-loop monitor frames; found $($monitorFrames.Count)."

$monitorButtons = [System.Collections.Generic.List[System.Xml.XmlElement]]::new()
for ($pad = 0; $pad -lt 32; $pad++) {
    $frameId = 1542 + $pad
    $frame = $workspace.SelectSingleNode(
        "//*[local-name()='VirtualConsole']//*[local-name()='Frame' and @ID='$frameId']")
    Assert-Condition ($null -ne $frame) "Live-loop frame $frameId is missing."
    Assert-Condition ((Get-ChildText -Node $frame -Name 'Disabled') -ceq 'True') `
        "Live-loop frame $frameId must remain read-only."
    Assert-Condition (@($frame.SelectNodes('ancestor::*[local-name()="SoloFrame"]')).Count -eq 0) `
        "Live-loop frame $frameId entered the playback-owner SoloFrame."
    Assert-Condition ([int]$frame.WindowState.Width -eq 26 -and
        [int]$frame.WindowState.Height -eq 35 -and
        [int]$frame.WindowState.Z -eq 50) `
        "Live-loop frame $frameId geometry or paint order changed."

    $buttons = @($frame.SelectNodes('./*[local-name()="Button"]'))
    Assert-Condition ($buttons.Count -eq 4) `
        "Pad $($pad + 1) does not contain four bank indicators."
    $expectedIds = @(
        (532 + $pad), (564 + $pad), (596 + $pad), (628 + $pad)
    )
    $actualIds = @($buttons | ForEach-Object {
        [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
    })
    Assert-Condition ((Compare-Object $expectedIds $actualIds).Count -eq 0) `
        "Pad $($pad + 1) does not monitor its four bank Chasers."
    Assert-Condition (@($buttons | Where-Object {
        @($_.SelectNodes('./*[local-name()="Input"]')).Count -ne 0
    }).Count -eq 0) `
        "Pad $($pad + 1) monitor owns an external input."

    $expectedY = @(0, 9, 18, 27)
    $actualY = @($buttons | ForEach-Object { [int]$_.WindowState.Y })
    Assert-Condition ((Compare-Object $expectedY $actualY).Count -eq 0 -and
        @($buttons | Where-Object {
            [int]$_.WindowState.X -ne 0 -or
            [int]$_.WindowState.Width -ne 26 -or
            [int]$_.WindowState.Height -ne 8
        }).Count -eq 0) `
        "Pad $($pad + 1) bank indicators overlap or moved."
    foreach ($button in $buttons) {
        $monitorButtons.Add($button)
    }
}

$monitorRawIds = @($monitorButtons | ForEach-Object {
    [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
} | Sort-Object)
Assert-Condition ($monitorButtons.Count -eq 128 -and
    (Compare-Object $rawLoopIds $monitorRawIds).Count -eq 0) `
    'Live-loop frames do not cover all 128 raw Chasers exactly once.'

$virtualConsole = $workspace.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$livePage = $virtualConsole.SelectSingleNode('./*[local-name()="Frame" and @ID="0"]')
Assert-Condition ([int]$livePage.WindowState.Width -eq 1600 -and
    [int]$livePage.WindowState.Height -eq 900) `
    'The V24 Live Console must remain 1600 by 900.'
$modeButtons = @($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input" and @Universe="1" and @Channel="811"]')).Count -gt 0
})
Assert-Condition ($modeButtons.Count -eq 1) `
    'Exactly one clickable Autoloop/Priority Looks switch must own logical channel 811.'
Assert-Condition ($modeButtons[0].SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID') -eq '1993' -and
    -not $modeButtons[0].HasAttribute('Page')) `
    'The persistent mode switch no longer targets public Function 1993.'
Assert-Condition ($null -eq $virtualConsole.SelectSingleNode('.//*[@ID="1406"][*[local-name()="WindowState"]]')) `
    'The duplicate V22 mode-switch widget still exists.'

$controlUniverse = $workspace.SelectSingleNode(
    '//*[local-name()="InputOutputMap"]/*[local-name()="Universe" and @ID="1"]')
$feedbackPatches = @($controlUniverse.SelectNodes('./*[local-name()="Feedback"]'))
Assert-Condition ($feedbackPatches.Count -eq 1 -and
    $feedbackPatches[0].GetAttribute('UID') -ceq 'soundswitch:controlone:surface') `
    'V24 requires one unified Control One Surface feedback patch.'
Assert-Condition ($null -ne $workspace.SelectSingleNode(
    '//*[local-name()="InputOutputMap"]/*[local-name()="Universe" and @ID="2"]/*[local-name()="Output" and @UID="soundswitch:priority-layer"]')) `
    'The private Priority Looks output layer is missing.'

foreach ($channel in 800..803) {
    $sources = @($virtualConsole.SelectNodes(
        ".//*[local-name()='Input' and @Universe='1' and @Channel='$channel']"))
    Assert-Condition ($sources.Count -eq 4) `
        "Clickable bank channel $channel must exist on all four bank pages."
}

$speedDials = @($virtualConsole.SelectNodes(
    './/*[local-name()="SpeedDial" and number(@ID)>=1022 and number(@ID)<=1029]'))
$speedCoverage = [System.Collections.Generic.List[int]]::new()
foreach ($dial in $speedDials) {
    $baseTime = [int](Get-ChildText -Node $dial -Name 'Time')
    $channels = @($dial.SelectNodes('./*[local-name()="Preset"]') | ForEach-Object {
        [int]$_.SelectSingleNode('./*[local-name()="Input"]').GetAttribute('Channel')
    })
    $values = @($dial.SelectNodes('./*[local-name()="Preset"]') | ForEach-Object {
        [int](Get-ChildText -Node $_ -Name 'Value')
    })
    Assert-Condition (($channels -join ',') -ceq '470,471,472,473,474') `
        "Chase-speed channels changed on SpeedDial $($dial.GetAttribute('ID'))."
    Assert-Condition ($values[0] -eq $baseTime * 4 -and
        $values[1] -eq $baseTime * 2 -and
        $values[2] -eq $baseTime -and
        $values[3] -eq [Math]::Floor($baseTime / 2) -and
        $values[4] -eq [Math]::Floor($baseTime / 4)) `
        "Chase-speed factors changed on SpeedDial $($dial.GetAttribute('ID'))."
    foreach ($function in @($dial.SelectNodes('./*[local-name()="Function"]'))) {
        $speedCoverage.Add([int]$function.InnerText)
    }
}
$speedGroups = @($speedCoverage | Group-Object)
Assert-Condition ($speedCoverage.Count -eq 128 -and
    $speedGroups.Count -eq 128 -and
    @($speedGroups | Where-Object Count -ne 1).Count -eq 0 -and
    (Compare-Object $rawLoopIds @($speedCoverage | Sort-Object)).Count -eq 0) `
    'The 0.25x–4x speed multiplier does not cover all 128 raw Autoloops exactly once.'

$dwellDial = $virtualConsole.SelectSingleNode(
    './/*[local-name()="SpeedDial" and @ID="1021"]')
$dwellFunctions = @($dwellDial.SelectNodes('./*[local-name()="Function"]') | ForEach-Object {
    [int]$_.InnerText
} | Sort-Object)
$dwellPresets = @($dwellDial.SelectNodes('./*[local-name()="Preset"]') | ForEach-Object {
    $input = $_.SelectSingleNode('./*[local-name()="Input"]')
    "$($input.GetAttribute('Channel')):$((Get-ChildText -Node $_ -Name 'Value'))"
})
Assert-Condition (($dwellFunctions -join ',') -ceq '788,789,790,791,792,793,794,795,796,797') `
    'Autoplay dwell no longer covers all ten parent Chasers.'
Assert-Condition (($dwellPresets -join ',') -ceq
    '480:4000,481:8000,482:16000,483:32000,484:64000') `
    'Autoplay dwell presets are no longer 1/2/4/8/16 measures.'

$nowPlaying = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1393"]')
$cueLists = @($nowPlaying.SelectNodes('./*[local-name()="CueList"]'))
Assert-Condition ($cueLists.Count -eq 10) `
    'The native Autoplay tracker must retain all ten parent CueLists.'
Assert-Condition (@($cueLists | Where-Object {
    [int]$_.WindowState.Height -lt 150
}).Count -eq 0) 'One or more native Autoplay trackers are too short to show the selected row.'

$playButton = $virtualConsole.SelectSingleNode('.//*[local-name()="Button" and @ID="1002"]')
$transportState = $virtualConsole.SelectSingleNode('.//*[local-name()="SoloFrame" and @ID="1011"]')
$playRight = [int]$playButton.WindowState.X + [int]$playButton.WindowState.Width
$stateRight = [int]$transportState.WindowState.X + [int]$transportState.WindowState.Width
Assert-Condition ($playRight -le [int]$transportState.WindowState.X -or
    $stateRight -le [int]$playButton.WindowState.X) `
    'The transport status panel obscures the clickable Play/Pause button.'

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
        'Test-V24Package.ps1')) {
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

if (-not [string]::IsNullOrWhiteSpace($SourceWorkspace)) {
    [xml]$sourceDocument = Get-Content -LiteralPath (
        (Resolve-Path -LiteralPath $SourceWorkspace).Path) -Raw
    $sourceFunctions = Get-FunctionMap -Workspace $sourceDocument
    $actualChanges = [System.Collections.Generic.List[int]]::new()
    foreach ($id in $sourceFunctions.Keys) {
        Assert-Condition $functions.ContainsKey($id) "V23 source Function $id was removed."
        if ($sourceFunctions[$id].OuterXml -cne $functions[$id].OuterXml) {
            $actualChanges.Add($id)
        }
    }
    Assert-Condition ($actualChanges.Count -eq 0) `
        "V24 changed V23 lighting Functions: $($actualChanges -join ', ')"

    $sourceFixtures = @($sourceDocument.SelectNodes(
        '//*[local-name()="Engine"]/*[local-name()="Fixture"]') | ForEach-Object OuterXml)
    Assert-Condition (($sourceFixtures -join "`n") -ceq
        (@($fixtures | ForEach-Object OuterXml) -join "`n")) `
        'V23 fixture XML changed in V24.'

    $sourceIo = $sourceDocument.SelectSingleNode(
        '//*[local-name()="Engine"]/*[local-name()="InputOutputMap"]')
    $expectedIoDocument = [System.Xml.XmlDocument]::new()
    $expectedIoDocument.PreserveWhitespace = $false
    $expectedIoDocument.LoadXml($sourceIo.OuterXml)
    $expectedControlUniverse = $expectedIoDocument.SelectSingleNode(
        '//*[local-name()="Universe" and @ID="1"]')
    $duplicatePriorityFeedback = $expectedControlUniverse.SelectSingleNode(
        './*[local-name()="Feedback" and @UID="soundswitch:priority-layer"]')
    Assert-Condition ($null -ne $duplicatePriorityFeedback) `
        'The V23 source does not contain the duplicate feedback line repaired by V24.'
    [void]$expectedControlUniverse.RemoveChild($duplicatePriorityFeedback)
    $targetIo = $workspace.SelectSingleNode(
        '//*[local-name()="Engine"]/*[local-name()="InputOutputMap"]')
    Assert-Condition ($expectedIoDocument.DocumentElement.OuterXml -ceq $targetIo.OuterXml) `
        'V24 Input/Output changed beyond removing the duplicate Priority feedback patch.'
    foreach ($id in $priorityLookIds) {
        Assert-Condition ($sourceFunctions[$id].OuterXml -ceq $functions[$id].OuterXml) `
            "V23 Priority Look $id changed in V24."
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
    '2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC') `
    'V24 plug-in hash does not match the software-tested package binary.'

Write-Host 'PASS: QLC+ SoundSwitch V24 Runtime Feedback package integrity'
Write-Host "  Files checksummed: $($listedFiles.Count)"
Write-Host "  Workspace Functions: $($functions.Count)"
Write-Host '  Creative merge: 22 raw Chasers + 176 Variety Pro Scene steps'
Write-Host '  Live-loop rail: 32 visible frames / 128 raw Chaser monitors'
Write-Host '  Mouse routing: banks, two-way mode switch, dwell, and 0.25x-4x speed'
Write-Host '  Feedback: one unified Surface line plus private Priority output'
Write-Host '  Fixture patch: 8 physical + 8 private Priority Layer fixtures'
if (-not [string]::IsNullOrWhiteSpace($SourceWorkspace)) {
    Write-Host '  V23 source delta: one feedback repair + visible monitors; all lighting Functions unchanged'
}
