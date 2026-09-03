[CmdletBinding()]
param(
    [string]$ReleaseDirectory = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$invalidFunctionId = [uint64]4294967295
$workspaceName = 'IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw'
$profileName = 'SoundSwitch-Control-One-Performance.qxi'
$fixtureName = 'American-DJ-Focus-Spot-Two.qxf'
$expectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$expectedOs2lSha256 = 'EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513'
$expectedQlcCommit = 'a124abebe0b5ad6077727c561a5a0e1f3730810c'
$focusAPositions = @(
    '46027,8286', '50752,6930', '35663,7231', '20727,65384', '39473,2260',
    '40693,2561', '39626,3314', '45417,15066', '50142,8587'
)
$focusBPositions = @(
    '40693,6629', '53342,3917', '33682,3917', '23013,63878', '47094,1055',
    '45875,1356', '47094,1356', '40997,13107', '49532,6629'
)

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Assert-SameSet {
    param(
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)][string]$Message
    )

    $difference = @(Compare-Object @($Expected | Sort-Object) @($Actual | Sort-Object))
    Assert-Condition ($difference.Count -eq 0) $Message
}

function Get-Steps {
    param(
        [Parameter(Mandatory)][System.Xml.XmlElement]$Function,
        [Parameter(Mandatory)]$Functions
    )

    $references = [System.Collections.Generic.List[int]]::new()
    $seenNumbers = [System.Collections.Generic.HashSet[int]]::new()
    $position = 0
    foreach ($step in @($Function.SelectNodes('./*[local-name()="Step"]'))) {
        $raw = $step.InnerText.Trim()
        Assert-Condition ($raw -match '^\d+$') `
            "Function $($Function.GetAttribute('ID')) has a malformed Step reference."
        $target = [int]$raw
        Assert-Condition ($Functions.ContainsKey($target)) `
            "Function $($Function.GetAttribute('ID')) references missing Function $target."
        if ($step.HasAttribute('Number')) {
            $number = $step.GetAttribute('Number')
            Assert-Condition ($number -match '^\d+$') `
                "Function $($Function.GetAttribute('ID')) has a malformed Step Number."
            $parsedNumber = [int]$number
            Assert-Condition ($seenNumbers.Add($parsedNumber)) `
                "Function $($Function.GetAttribute('ID')) repeats Step Number $parsedNumber."
            Assert-Condition ($parsedNumber -eq $position) `
                "Function $($Function.GetAttribute('ID')) Step numbering is not consecutive."
        }
        $references.Add($target)
        $position++
    }
    return $references.ToArray()
}

function Get-FunctionClosure {
    param(
        [Parameter(Mandatory)][int[]]$Roots,
        [Parameter(Mandatory)]$Functions
    )

    $visited = [System.Collections.Generic.HashSet[int]]::new()
    $pending = [System.Collections.Generic.Stack[int]]::new()
    foreach ($root in $Roots) { $pending.Push($root) }
    while ($pending.Count -gt 0) {
        $functionId = $pending.Pop()
        Assert-Condition ($Functions.ContainsKey($functionId)) `
            "Required Function $functionId is missing."
        if (-not $visited.Add($functionId)) { continue }
        foreach ($target in @(Get-Steps -Function $Functions[$functionId] -Functions $Functions)) {
            $pending.Push($target)
        }
    }
    return @($visited)
}

function Get-FixtureValues {
    param(
        [Parameter(Mandatory)][System.Xml.XmlElement]$Scene,
        [Parameter(Mandatory)]$Fixtures
    )

    $result = @{}
    foreach ($fixtureValue in @($Scene.SelectNodes('./*[local-name()="FixtureVal"]'))) {
        $rawId = $fixtureValue.GetAttribute('ID')
        Assert-Condition ($rawId -match '^\d+$') `
            "Scene $($Scene.GetAttribute('ID')) has a malformed FixtureVal ID."
        $fixtureId = [int]$rawId
        Assert-Condition ($Fixtures.ContainsKey($fixtureId)) `
            "Scene $($Scene.GetAttribute('ID')) targets missing Fixture $fixtureId."
        Assert-Condition (-not $result.ContainsKey($fixtureId)) `
            "Scene $($Scene.GetAttribute('ID')) repeats FixtureVal $fixtureId."
        $rawText = $fixtureValue.InnerText.Trim()
        Assert-Condition (-not [string]::IsNullOrWhiteSpace($rawText)) `
            "Scene $($Scene.GetAttribute('ID')) has an empty FixtureVal $fixtureId."
        Assert-Condition (-not $rawText.StartsWith(',') -and -not $rawText.EndsWith(',')) `
            "Scene $($Scene.GetAttribute('ID')) has a malformed FixtureVal $fixtureId."
        $tokens = @($rawText -split ',' | ForEach-Object { $_.Trim() })
        Assert-Condition ($tokens.Count % 2 -eq 0) `
            "Scene $($Scene.GetAttribute('ID')) has an odd FixtureVal token count."
        $frame = @{}
        $channelCount = [int]$Fixtures[$fixtureId].SelectSingleNode(
            './*[local-name()="Channels"]').InnerText
        for ($offset = 0; $offset -lt $tokens.Count; $offset += 2) {
            Assert-Condition ($tokens[$offset] -match '^\d+$' -and
                $tokens[$offset + 1] -match '^\d+$') `
                "Scene $($Scene.GetAttribute('ID')) FixtureVal $fixtureId contains a non-integer."
            $channel = [int]$tokens[$offset]
            $value = [int]$tokens[$offset + 1]
            Assert-Condition (-not $frame.ContainsKey($channel)) `
                "Scene $($Scene.GetAttribute('ID')) repeats fixture $fixtureId channel $channel."
            Assert-Condition ($channel -ge 0 -and $channel -lt $channelCount) `
                "Scene $($Scene.GetAttribute('ID')) fixture $fixtureId channel $channel is outside its mode."
            Assert-Condition ($value -ge 0 -and $value -le 255) `
                "Scene $($Scene.GetAttribute('ID')) fixture $fixtureId value $value is outside 0..255."
            $frame[$channel] = $value
        }
        $result[$fixtureId] = $frame
    }
    return $result
}

function Assert-CompleteFrame {
    param(
        [Parameter(Mandatory)]$Frame,
        [Parameter(Mandatory)][int]$ChannelCount,
        [Parameter(Mandatory)][string]$Context
    )

    Assert-SameSet -Expected @(0..($ChannelCount - 1)) -Actual @($Frame.Keys) `
        -Message "$Context is not a complete $ChannelCount-channel frame."
}

function Get-FrameSignature {
    param(
        [Parameter(Mandatory)]$Frame,
        [Parameter(Mandatory)][int]$ChannelCount
    )

    $values = for ($channel = 0; $channel -lt $ChannelCount; $channel++) {
        [string]$Frame[$channel]
    }
    return $values -join ','
}

function Test-NewFixtureEmits {
    param(
        [Parameter(Mandatory)][int]$FixtureId,
        [Parameter(Mandatory)]$Frame
    )

    if ($FixtureId -in @(4, 104)) {
        foreach ($channel in 4..39) {
            if ([int]$Frame[$channel] -gt 0) { return $true }
        }
        return $false
    }
    return (([int]$Frame[8] -ne 0 -and [int]$Frame[9] -gt 0) -or
        ([int]$Frame[10] -ne 0 -and [int]$Frame[11] -gt 0))
}

function Assert-SafeNewFixtureValues {
    param(
        [Parameter(Mandatory)][int]$SceneId,
        [Parameter(Mandatory)]$Values,
        [switch]$AllowWashControlChannels
    )

    foreach ($fixtureId in @(4, 104)) {
        if (-not $Values.ContainsKey($fixtureId)) { continue }
        if (-not $AllowWashControlChannels) {
            foreach ($channel in 0..3) {
                if ($Values[$fixtureId].ContainsKey($channel)) {
                    Assert-Condition ([int]$Values[$fixtureId][$channel] -eq 0) `
                        "Scene $SceneId Wash fixture $fixtureId enables an automatic/strobe control channel."
                }
            }
        }
    }
    foreach ($fixtureId in @(9, 10, 109, 110)) {
        if (-not $Values.ContainsKey($fixtureId)) { continue }
        $frame = $Values[$fixtureId]
        foreach ($channel in @(13, 14, 15, 17)) {
            if ($frame.ContainsKey($channel)) {
                Assert-Condition ([int]$frame[$channel] -eq 0) `
                    "Scene $SceneId Focus fixture $fixtureId enables unsafe channel $channel."
            }
        }
        if ($frame.ContainsKey(8)) {
            Assert-Condition ([int]$frame[8] -in @(0, 8)) `
                "Scene $SceneId Focus fixture $fixtureId has an unauthorized main shutter value."
        }
        if ($frame.ContainsKey(10)) {
            Assert-Condition ([int]$frame[10] -eq 0) `
                "Scene $SceneId Focus fixture $fixtureId enables the unbenchmarked UV shutter."
        }
        if ($frame.ContainsKey(11)) {
            Assert-Condition ([int]$frame[11] -eq 0) `
                "Scene $SceneId Focus fixture $fixtureId enables unbenchmarked UV output."
        }
        if ($frame.ContainsKey(16)) {
            Assert-Condition ([int]$frame[16] -in @(40, 110, 150, 210, 220)) `
                "Scene $SceneId Focus fixture $fixtureId has an unreviewed pan/tilt speed."
        }
        if ($frame.ContainsKey(0) -and $frame.ContainsKey(1) -and
            $frame.ContainsKey(2) -and $frame.ContainsKey(3)) {
            $pan = (([int]$frame[0]) -shl 8) -bor ([int]$frame[1])
            $tilt = (([int]$frame[2]) -shl 8) -bor ([int]$frame[3])
            $allowed = if ($fixtureId -in @(9, 109)) {
                $focusAPositions
            } else {
                $focusBPositions
            }
            Assert-Condition ("$pan,$tilt" -in $allowed) `
                "Scene $SceneId Focus fixture $fixtureId uses an unreviewed position."
        }
    }
}

$release = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$expectedFiles = @(
    'American-DJ-Focus-Spot-Two.qxf',
    'FULL_RIG_PATCH_AND_BENCH.md',
    'Install-V27.ps1',
    $workspaceName,
    'LICENSE-APACHE-2.0.txt',
    'os2l.dll',
    'README.md',
    'RELEASE_NOTES.md',
    'Rollback-V27.ps1',
    'SHA256SUMS.txt',
    'SOUNDSWITCH_SOURCE_PROVENANCE.md',
    $profileName,
    'soundswitch-build-evidence.json',
    'soundswitch.dll',
    'Test-V27Package.ps1',
    'VIRTUALDJ_OS2L_AUTO_RECONNECT.md'
)
$actualFiles = @(Get-ChildItem -LiteralPath $release -File | ForEach-Object Name)
Assert-SameSet -Expected $expectedFiles -Actual $actualFiles `
    -Message 'The V27 package must contain exactly the reviewed 16-file release set.'

$manifestPath = Join-Path $release 'SHA256SUMS.txt'
$packageHashes = @{}
foreach ($line in @(Get-Content -LiteralPath $manifestPath)) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    Assert-Condition ($line -match '^([A-F0-9]{64})  ([^\\/]+)$') `
        "Malformed checksum line: $line"
    $hash = $Matches[1]
    $name = $Matches[2]
    Assert-Condition ($name -ne 'SHA256SUMS.txt') 'SHA256SUMS.txt must not hash itself.'
    Assert-Condition (-not $packageHashes.ContainsKey($name)) `
        "Duplicate checksum entry: $name"
    $packageHashes[$name] = $hash
    $path = Join-Path $release $name
    Assert-Condition (Test-Path -LiteralPath $path -PathType Leaf) `
        "Checksummed file is missing: $name"
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    Assert-Condition ($actual -eq $hash) "Checksum mismatch: $name"
}
$expectedChecksummed = @($expectedFiles | Where-Object { $_ -ne 'SHA256SUMS.txt' })
Assert-SameSet -Expected $expectedChecksummed -Actual @($packageHashes.Keys) `
    -Message 'SHA256SUMS.txt must cover every packaged file except itself, with no extras.'
Assert-Condition ($packageHashes['os2l.dll'] -eq $expectedOs2lSha256) `
    'V27 must retain the reviewed V26 os2l.dll binary unchanged.'

$evidencePath = Join-Path $release 'soundswitch-build-evidence.json'
$evidence = Get-Content -LiteralPath $evidencePath -Raw | ConvertFrom-Json
Assert-Condition ([string]$evidence.qlcplusSourceCommit -eq $expectedQlcCommit) `
    'SoundSwitch build evidence does not name the pinned QLC+ source commit.'
Assert-Condition ([string]$evidence.qtVersion -eq '6.8.1') `
    'SoundSwitch build evidence has the wrong Qt version.'
Assert-Condition ([string]$evidence.architecture -eq 'windows-x64-mingw') `
    'SoundSwitch build evidence has the wrong target architecture.'
Assert-Condition ([string]$evidence.protocolTests -eq 'passed' -and
    [string]$evidence.intensityTests -eq 'passed' -and
    [string]$evidence.pluginLoadSmoke -eq 'passed-without-hardware') `
    'SoundSwitch build/test evidence is incomplete.'
Assert-Condition ($evidence.pinnedQlcHostAbiTested -eq $false) `
    'Evidence must preserve the honest boundary that pinned-host ABI was not physically tested.'
Assert-Condition ([string]$evidence.repositoryCommit -match '^[a-f0-9]{40}$' -and
    [string]$evidence.workflowRun -match '^\d+$') `
    'SoundSwitch build provenance is malformed.'
$evidenceSoundSwitchHash = ([string]$evidence.soundswitchSha256).ToUpperInvariant()
Assert-Condition ($evidenceSoundSwitchHash -match '^[A-F0-9]{64}$') `
    'SoundSwitch build evidence contains a malformed plug-in hash.'
Assert-Condition ($packageHashes['soundswitch.dll'] -eq $evidenceSoundSwitchHash) `
    'The packaged soundswitch.dll does not match its CI build evidence.'

$workspacePath = Join-Path $release $workspaceName
$workspace = [System.Xml.XmlDocument]::new()
$workspace.PreserveWhitespace = $false
$workspace.XmlResolver = $null
$workspace.Load($workspacePath)
$engine = $workspace.SelectSingleNode('//*[local-name()="Engine"]')
$virtualConsole = $workspace.SelectSingleNode('//*[local-name()="VirtualConsole"]')
Assert-Condition ($null -ne $engine -and $null -ne $virtualConsole) `
    'Workspace Engine or Virtual Console is missing.'

$functions = @{}
foreach ($function in @($engine.SelectNodes('./*[local-name()="Function"]'))) {
    $rawId = $function.GetAttribute('ID')
    Assert-Condition ($rawId -match '^\d+$') "Malformed Function ID $rawId."
    $id = [int]$rawId
    Assert-Condition (-not $functions.ContainsKey($id)) "Duplicate Function ID $id."
    $functions[$id] = $function
}
Assert-Condition ($functions.Count -eq 2100) 'Expected exactly 2,100 V27 lighting Functions.'
$functionTypes = @($functions.Values | ForEach-Object { $_.GetAttribute('Type') })
Assert-Condition (@($functionTypes | Where-Object { $_ -eq 'Scene' }).Count -eq 1812 -and
    @($functionTypes | Where-Object { $_ -eq 'Chaser' }).Count -eq 150 -and
    @($functionTypes | Where-Object { $_ -eq 'Collection' }).Count -eq 138) `
    'V27 Function type counts must be 1,812 Scenes, 150 Chasers, and 138 Collections.'

foreach ($function in @($functions.Values)) {
    foreach ($unused in @(Get-Steps -Function $function -Functions $functions)) { }
}

$fixtures = @{}
foreach ($fixture in @($engine.SelectNodes('./*[local-name()="Fixture"]'))) {
    $idNode = $fixture.SelectSingleNode('./*[local-name()="ID"]')
    Assert-Condition ($null -ne $idNode -and $idNode.InnerText -match '^\d+$') `
        'A workspace Fixture has a malformed ID.'
    $id = [int]$idNode.InnerText
    Assert-Condition (-not $fixtures.ContainsKey($id)) "Duplicate Fixture ID $id."
    $fixtures[$id] = $fixture
}
Assert-SameSet -Expected @((0..10) + (100..110)) -Actual @($fixtures.Keys) `
    -Message 'V27 must contain exactly 11 physical fixtures and 11 private Priority duplicates.'

$expectedPatch = @{}
$expectedPatch[0] = 'Both Lighting|IR-4 (BOIR4)|10 Channel|0|0|10'
$expectedPatch[1] = 'Both Lighting|IR-4 (BOIR4)|10 Channel|0|10|10'
$expectedPatch[2] = 'Both Lighting|IR-4 (BOIR4)|10 Channel|0|20|10'
$expectedPatch[3] = 'Both Lighting|IR-4 (BOIR4)|10 Channel|0|30|10'
$expectedPatch[4] = 'Chauvet|Wash FX Hex|40 Channel|0|40|40'
$expectedPatch[5] = 'Both Lighting|BO-TUBE192|40 Channel|0|174|40'
$expectedPatch[6] = 'Both Lighting|BO-TUBE192|40 Channel|0|214|40'
$expectedPatch[7] = 'Both Lighting|BO-TUBE192|40 Channel|0|254|40'
$expectedPatch[8] = 'Both Lighting|BO-TUBE192|40 Channel|0|294|40'
$expectedPatch[9] = 'American DJ|Focus Spot Two|18 Channel|0|80|18'
$expectedPatch[10] = 'American DJ|Focus Spot Two|18 Channel|0|98|18'
foreach ($physicalId in 0..10) {
    $parts = $expectedPatch[$physicalId] -split '\|'
    $expectedPatch[$physicalId + 100] =
        "$($parts[0])|$($parts[1])|$($parts[2])|2|$($parts[4])|$($parts[5])"
}

$spansByUniverse = @{}
foreach ($fixtureId in @($fixtures.Keys)) {
    $fixture = $fixtures[$fixtureId]
    $tuple = @(
        $fixture.SelectSingleNode('./*[local-name()="Manufacturer"]').InnerText,
        $fixture.SelectSingleNode('./*[local-name()="Model"]').InnerText,
        $fixture.SelectSingleNode('./*[local-name()="Mode"]').InnerText,
        $fixture.SelectSingleNode('./*[local-name()="Universe"]').InnerText,
        $fixture.SelectSingleNode('./*[local-name()="Address"]').InnerText,
        $fixture.SelectSingleNode('./*[local-name()="Channels"]').InnerText
    ) -join '|'
    Assert-Condition ($tuple -eq $expectedPatch[$fixtureId]) `
        "Fixture $fixtureId has the wrong manufacturer/model/mode/patch tuple."
    $universe = [int]$fixture.SelectSingleNode('./*[local-name()="Universe"]').InnerText
    $address = [int]$fixture.SelectSingleNode('./*[local-name()="Address"]').InnerText
    $channelCount = [int]$fixture.SelectSingleNode('./*[local-name()="Channels"]').InnerText
    Assert-Condition ($address -ge 0 -and $channelCount -gt 0 -and
        ($address + $channelCount) -le 512) "Fixture $fixtureId has an invalid DMX span."
    if (-not $spansByUniverse.ContainsKey($universe)) {
        $spansByUniverse[$universe] = [System.Collections.Generic.List[object]]::new()
    }
    $spansByUniverse[$universe].Add([pscustomobject]@{
        Start = $address
        End = $address + $channelCount
        FixtureId = $fixtureId
    })
}
foreach ($universe in @(0, 2)) {
    $spans = @($spansByUniverse[$universe] | Sort-Object Start)
    for ($index = 1; $index -lt $spans.Count; $index++) {
        Assert-Condition ($spans[$index - 1].End -le $spans[$index].Start) `
            "Universe $($universe + 1) fixtures overlap."
    }
    Assert-Condition (@($spans | Where-Object {
            $_.End -gt 116 -and $_.Start -lt 174
        }).Count -eq 0) "Universe $($universe + 1) reserved channels 117-174 are not clear."
}
foreach ($physicalId in 0..10) {
    $physicalName = $fixtures[$physicalId].SelectSingleNode('./*[local-name()="Name"]').InnerText
    $privateName = $fixtures[$physicalId + 100].SelectSingleNode('./*[local-name()="Name"]').InnerText
    Assert-Condition ($privateName -eq "$physicalName — Priority Layer") `
        "Fixture pair $physicalId/$($physicalId + 100) has the wrong private-layer name."
}

$inputOutputMap = $engine.SelectSingleNode('./*[local-name()="InputOutputMap"]')
foreach ($universeId in @(0, 2)) {
    $universe = $inputOutputMap.SelectSingleNode(
        "./*[local-name()='Universe' and @ID='$universeId']")
    Assert-Condition ($null -ne $universe) "Workspace Universe $($universeId + 1) is missing."
    $label = $universe.GetAttribute('Name').ToUpperInvariant()
    Assert-Condition ($label.Contains('WASH') -and $label.Contains('FOCUS')) `
        "Universe $($universeId + 1) label does not identify the full rig."
    foreach ($output in @($universe.SelectNodes('./*[local-name()="Output"]'))) {
        $parameters = $output.SelectSingleNode('./*[local-name()="PluginParameters"]')
        Assert-Condition ($null -ne $parameters -and
            [int]$parameters.GetAttribute('UniverseChannels') -ge 334) `
            "Universe $($universeId + 1) output does not carry the 334-channel frame."
    }
}
$privateOutputs = @($inputOutputMap.SelectNodes(
    './*[local-name()="Universe" and @ID="2"]/*[local-name()="Output"]'))
Assert-Condition ($privateOutputs.Count -eq 1 -and
    $privateOutputs[0].GetAttribute('UID') -eq 'soundswitch:priority-layer' -and
    $privateOutputs[0].GetAttribute('Line') -eq '4') `
    'Universe 3 must remain isolated on the internal SoundSwitch Priority layer.'

$monitorItems = @($engine.SelectNodes(
    './*[local-name()="Monitor"]/*[local-name()="FxItem"]'))
$monitorIds = @($monitorItems | ForEach-Object { [int]$_.GetAttribute('ID') })
Assert-SameSet -Expected @(0..10) -Actual $monitorIds `
    -Message 'The visual bench must contain every physical fixture exactly once and no private fixture.'
$monitorPositions = @($monitorItems | ForEach-Object {
    "$($_.GetAttribute('XPos')),$($_.GetAttribute('YPos')),$($_.GetAttribute('ZPos'))"
})
Assert-Condition (@($monitorPositions | Select-Object -Unique).Count -eq 11) `
    'Visual-bench fixtures overlap at the same grid position.'

$rawLoopIds = @(532..659)
$priorityRootIds = @(5..36)
$priorityChaserIds = @(12, 20, 21, 29, 30, 32, 33, 34, 35, 36)
$performanceSceneIds = @(0..4)
$overrideSceneIds = @(37..45)

foreach ($id in $rawLoopIds) {
    Assert-Condition ($functions[$id].GetAttribute('Type') -eq 'Chaser') `
        "Raw Autoloop $id is not a Chaser."
    Assert-Condition (@(Get-Steps -Function $functions[$id] -Functions $functions).Count -eq 8) `
        "Raw Autoloop $id does not have eight steps."
}
$rawClosure = @(Get-FunctionClosure -Roots $rawLoopIds -Functions $functions)
$rawSceneIds = @($rawClosure | Where-Object {
    $functions[[int]$_].GetAttribute('Type') -eq 'Scene'
})
Assert-Condition ($rawSceneIds.Count -eq 1024 -and $rawClosure.Count -eq 1152) `
    'Raw Autoloop closure must contain 1,024 Scenes and 128 Chasers.'
Assert-Condition (@($rawClosure | Where-Object {
        $functions[[int]$_].GetAttribute('Type') -eq 'Chaser'
    }).Count -eq 128) 'Raw Autoloop closure contains an unexpected Function type.'

$actualPriorityChasers = @($priorityRootIds | Where-Object {
    $functions[$_].GetAttribute('Type') -eq 'Chaser'
})
Assert-SameSet -Expected $priorityChaserIds -Actual $actualPriorityChasers `
    -Message 'Priority Scene/Chaser root types changed.'
foreach ($id in $priorityChaserIds) {
    Assert-Condition (@(Get-Steps -Function $functions[$id] -Functions $functions).Count -eq 8) `
        "Priority Chaser $id does not have eight steps."
}
$priorityClosure = @(Get-FunctionClosure -Roots $priorityRootIds -Functions $functions)
$prioritySceneIds = @($priorityClosure | Where-Object {
    $functions[[int]$_].GetAttribute('Type') -eq 'Scene'
})
Assert-Condition ($prioritySceneIds.Count -eq 102 -and $priorityClosure.Count -eq 112) `
    'Priority closure must contain 102 Scenes and 10 Chasers.'
Assert-Condition (@($priorityClosure | Where-Object {
        $functions[[int]$_].GetAttribute('Type') -eq 'Chaser'
    }).Count -eq 10) 'Priority closure contains an unexpected Function type.'

$liveSceneIds = @($rawSceneIds + $prioritySceneIds + $performanceSceneIds + $overrideSceneIds |
    Sort-Object -Unique)
Assert-Condition ($liveSceneIds.Count -eq 1140) 'Expected exactly 1,140 live creative Scenes.'
$cachedSceneIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($id in $liveSceneIds) { [void]$cachedSceneIds.Add([int]$id) }
foreach ($id in 2175..2183) { [void]$cachedSceneIds.Add($id) }
$sceneValues = @{}
foreach ($functionId in @($functions.Keys)) {
    $function = $functions[$functionId]
    if ($function.GetAttribute('Type') -ne 'Scene') { continue }
    $values = Get-FixtureValues -Scene $function -Fixtures $fixtures
    # Apply Focus UV and hazardous-control policy to every Scene in the
    # released workspace, not only the 1,140 live-creative closure.
    Assert-SafeNewFixtureValues -SceneId $functionId -Values $values `
        -AllowWashControlChannels:($functionId -in $overrideSceneIds)
    if ($cachedSceneIds.Contains([int]$functionId)) {
        $sceneValues[$functionId] = $values
    }
}

$rawSceneSet = [System.Collections.Generic.HashSet[int]]::new()
foreach ($id in $rawSceneIds) { [void]$rawSceneSet.Add([int]$id) }
$prioritySceneSet = [System.Collections.Generic.HashSet[int]]::new()
foreach ($id in $prioritySceneIds) { [void]$prioritySceneSet.Add([int]$id) }
$rawOwner = @{}
foreach ($rootId in $rawLoopIds) {
    foreach ($sceneId in @(Get-Steps -Function $functions[$rootId] -Functions $functions)) {
        Assert-Condition (-not $rawOwner.ContainsKey($sceneId)) `
            "Raw Scene $sceneId belongs to more than one Autoloop."
        $rawOwner[$sceneId] = $rootId
    }
}

foreach ($sceneId in $rawSceneIds) {
    $values = $sceneValues[[int]$sceneId]
    Assert-SameSet -Expected @(0..10) -Actual @($values.Keys) `
        -Message "Raw Scene $sceneId does not cover exactly the physical rig."
    foreach ($fixtureId in 0..10) {
        $channelCount = [int]$fixtures[$fixtureId].SelectSingleNode(
            './*[local-name()="Channels"]').InnerText
        Assert-CompleteFrame -Frame $values[$fixtureId] -ChannelCount $channelCount `
            -Context "Raw Scene $sceneId Fixture $fixtureId"
    }
    Assert-SafeNewFixtureValues -SceneId $sceneId -Values $values
    $ownerId = [int]$rawOwner[[int]$sceneId]
    $expectedSpeed = if ($ownerId -le 563) { 150 } elseif ($ownerId -le 595) {
        110
    } elseif ($ownerId -le 627) { 210 } else { 40 }
    foreach ($fixtureId in @(9, 10)) {
        Assert-Condition ([int]$values[$fixtureId][16] -eq $expectedSpeed) `
            "Raw Scene $sceneId Focus fixture $fixtureId has the wrong bank pan/tilt speed."
    }
}

foreach ($sceneId in $prioritySceneIds) {
    $values = $sceneValues[[int]$sceneId]
    Assert-SameSet -Expected @(100..110) -Actual @($values.Keys) `
        -Message "Priority Scene $sceneId does not cover exactly the private rig."
    foreach ($fixtureId in 100..110) {
        $channelCount = [int]$fixtures[$fixtureId].SelectSingleNode(
            './*[local-name()="Channels"]').InnerText
        Assert-CompleteFrame -Frame $values[$fixtureId] -ChannelCount $channelCount `
            -Context "Priority Scene $sceneId Fixture $fixtureId"
    }
    Assert-SafeNewFixtureValues -SceneId $sceneId -Values $values
    foreach ($fixtureId in @(109, 110)) {
        Assert-Condition ([int]$values[$fixtureId][16] -eq 220) `
            "Priority Scene $sceneId Focus fixture $fixtureId must use pan/tilt speed 220."
    }
}

foreach ($sceneId in @($performanceSceneIds + $overrideSceneIds)) {
    Assert-Condition ($functions[$sceneId].GetAttribute('Type') -eq 'Scene') `
        "Live root $sceneId is not a Scene."
    $values = $sceneValues[$sceneId]
    Assert-SameSet -Expected @(0..10) -Actual @($values.Keys) `
        -Message "Live Scene $sceneId does not cover the full physical rig."
    if ($sceneId -in $performanceSceneIds) {
        foreach ($fixtureId in 0..10) {
            $channelCount = [int]$fixtures[$fixtureId].SelectSingleNode(
                './*[local-name()="Channels"]').InnerText
            Assert-CompleteFrame -Frame $values[$fixtureId] -ChannelCount $channelCount `
                -Context "Performance Scene $sceneId Fixture $fixtureId"
        }
    }
    Assert-SafeNewFixtureValues -SceneId $sceneId -Values $values `
        -AllowWashControlChannels:($sceneId -in $overrideSceneIds)
}

foreach ($sceneId in $overrideSceneIds) {
    $values = $sceneValues[$sceneId]
    Assert-Condition ($values[4].Count -gt 0 -and
        @($values[4].Keys | Where-Object { $_ -lt 4 -or $_ -gt 39 }).Count -eq 0) `
        "Color override $sceneId drives a non-color Wash channel."
    foreach ($fixtureId in @(9, 10)) {
        Assert-SameSet -Expected @(4) -Actual @($values[$fixtureId].Keys) `
            -Message "Color override $sceneId must target only Focus color-wheel channel 4."
    }
}

$blackout = $sceneValues[0]
foreach ($channel in 4..39) {
    Assert-Condition ($blackout[4].ContainsKey($channel) -and [int]$blackout[4][$channel] -eq 0) `
        "Blackout does not explicitly clear Wash emitter channel $channel."
}
foreach ($fixtureId in @(9, 10)) {
    foreach ($channel in @(8, 9, 10, 11)) {
        Assert-Condition ($blackout[$fixtureId].ContainsKey($channel) -and
            [int]$blackout[$fixtureId][$channel] -eq 0) `
            "Blackout does not explicitly clear Focus fixture $fixtureId channel $channel."
    }
}

foreach ($rootId in $rawLoopIds) {
    $signatures = [System.Collections.Generic.HashSet[string]]::new()
    $active = @{ 4 = $false; 9 = $false; 10 = $false }
    foreach ($sceneId in @(Get-Steps -Function $functions[$rootId] -Functions $functions)) {
        $values = $sceneValues[$sceneId]
        $parts = [System.Collections.Generic.List[string]]::new()
        foreach ($fixtureId in @(4, 9, 10)) {
            $channelCount = [int]$fixtures[$fixtureId].SelectSingleNode(
                './*[local-name()="Channels"]').InnerText
            $parts.Add((Get-FrameSignature -Frame $values[$fixtureId] -ChannelCount $channelCount))
            if (Test-NewFixtureEmits -FixtureId $fixtureId -Frame $values[$fixtureId]) {
                $active[$fixtureId] = $true
            }
        }
        [void]$signatures.Add(($parts -join '|'))
    }
    Assert-Condition ($signatures.Count -ge 2) `
        "Autoloop $rootId gives the Wash/Focus rig one static frame."
    foreach ($fixtureId in @(4, 9, 10)) {
        Assert-Condition ([bool]$active[$fixtureId]) `
            "Autoloop $rootId never activates new fixture $fixtureId."
    }
}

foreach ($rootId in $priorityChaserIds) {
    $signatures = [System.Collections.Generic.HashSet[string]]::new()
    $active = @{ 104 = $false; 109 = $false; 110 = $false }
    foreach ($sceneId in @(Get-Steps -Function $functions[$rootId] -Functions $functions)) {
        $values = $sceneValues[$sceneId]
        $parts = [System.Collections.Generic.List[string]]::new()
        foreach ($fixtureId in @(104, 109, 110)) {
            $channelCount = [int]$fixtures[$fixtureId].SelectSingleNode(
                './*[local-name()="Channels"]').InnerText
            $parts.Add((Get-FrameSignature -Frame $values[$fixtureId] -ChannelCount $channelCount))
            if (Test-NewFixtureEmits -FixtureId $fixtureId -Frame $values[$fixtureId]) {
                $active[$fixtureId] = $true
            }
        }
        [void]$signatures.Add(($parts -join '|'))
    }
    Assert-Condition ($signatures.Count -ge 2) `
        "Priority Chaser $rootId gives the private Wash/Focus rig one static frame."
    foreach ($fixtureId in @(104, 109, 110)) {
        Assert-Condition ([bool]$active[$fixtureId]) `
            "Priority Chaser $rootId never activates private fixture $fixtureId."
    }
}

foreach ($fixtureId in @(4, 9, 10)) {
    $overrideSignatures = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($sceneId in $overrideSceneIds) {
        $pairs = @($sceneValues[$sceneId][$fixtureId].GetEnumerator() |
            Sort-Object Key | ForEach-Object { "$($_.Key)=$($_.Value)" })
        [void]$overrideSignatures.Add(($pairs -join ','))
    }
    Assert-Condition ($overrideSignatures.Count -ge 5) `
        "New fixture $fixtureId has fewer than five distinct color overrides."
}

$positionData = @(
    [pscustomobject]@{ Id = 2175; Name = 'Crossed Out Down'; A = @(46027, 8286); B = @(40693, 6629) },
    [pscustomobject]@{ Id = 2176; Name = 'Crossed In Down'; A = @(50752, 6930); B = @(53342, 3917) },
    [pscustomobject]@{ Id = 2177; Name = 'Stage Right'; A = @(35663, 7231); B = @(33682, 3917) },
    [pscustomobject]@{ Id = 2178; Name = 'Stage Left'; A = @(20727, 65384); B = @(23013, 63878) },
    [pscustomobject]@{ Id = 2179; Name = 'Straight Ahead'; A = @(39473, 2260); B = @(47094, 1055) },
    [pscustomobject]@{ Id = 2180; Name = 'Crossed Out Up'; A = @(40693, 2561); B = @(45875, 1356) },
    [pscustomobject]@{ Id = 2181; Name = 'Crossed In Up'; A = @(39626, 3314); B = @(47094, 1356) },
    [pscustomobject]@{ Id = 2182; Name = 'Up'; A = @(45417, 15066); B = @(40997, 13107) },
    [pscustomobject]@{ Id = 2183; Name = 'Down'; A = @(50142, 8587); B = @(49532, 6629) }
)
foreach ($position in $positionData) {
    $scene = $functions[[int]$position.Id]
    Assert-Condition ($scene.GetAttribute('Type') -eq 'Scene' -and
        $scene.GetAttribute('Name') -eq [string]$position.Name) `
        "Focus position Scene $($position.Id) has the wrong identity."
    $speed = $scene.SelectSingleNode('./*[local-name()="Speed"]')
    Assert-Condition ($speed.GetAttribute('FadeIn') -eq '0' -and
        $speed.GetAttribute('FadeOut') -eq '0' -and
        $speed.GetAttribute('Duration') -eq '0') `
        "Focus position Scene $($position.Id) is not instantaneous."
    $values = $sceneValues[[int]$position.Id]
    Assert-SameSet -Expected @(9, 10) -Actual @($values.Keys) `
        -Message "Focus position Scene $($position.Id) must target only Focus A/B."
    foreach ($side in @(
            [pscustomobject]@{ Fixture = 9; Pair = $position.A },
            [pscustomobject]@{ Fixture = 10; Pair = $position.B }
        )) {
        $pan = [int]$side.Pair[0]
        $tilt = [int]$side.Pair[1]
        $expectedPositionFrame = @{
            0 = $pan -shr 8
            1 = $pan -band 255
            2 = $tilt -shr 8
            3 = $tilt -band 255
        }
        Assert-SameSet -Expected @(0..3) -Actual @($values[[int]$side.Fixture].Keys) `
            -Message "Focus position Scene $($position.Id) fixture $($side.Fixture) is not sparse."
        foreach ($channel in 0..3) {
            Assert-Condition ([int]$values[[int]$side.Fixture][$channel] -eq
                [int]$expectedPositionFrame[$channel]) `
                "Focus position Scene $($position.Id) fixture $($side.Fixture) channel $channel is wrong."
        }
    }
}

$movement = $functions[2184]
Assert-Condition ($movement.GetAttribute('Type') -eq 'Chaser' -and
    $movement.GetAttribute('Name') -eq 'MOVEMENT — FOCUS A/B SWEEP') `
    'Function 2184 is not the reviewed Focus movement Chaser.'
Assert-Condition ($movement.SelectSingleNode('./*[local-name()="Tempo"]').InnerText -eq 'Beats' -and
    $movement.SelectSingleNode('./*[local-name()="Direction"]').InnerText -eq 'Forward' -and
    $movement.SelectSingleNode('./*[local-name()="RunOrder"]').InnerText -eq 'Loop') `
    'Focus movement Chaser tempo/direction/run order is wrong.'
$movementSpeed = $movement.SelectSingleNode('./*[local-name()="Speed"]')
Assert-Condition ($movementSpeed.GetAttribute('FadeIn') -eq '750' -and
    $movementSpeed.GetAttribute('FadeOut') -eq '0' -and
    $movementSpeed.GetAttribute('Duration') -eq '1000') `
    'Focus movement Chaser global timing is wrong.'
$movementSteps = @($movement.SelectNodes('./*[local-name()="Step"]'))
Assert-Condition ($movementSteps.Count -eq 9) 'Focus movement Chaser must have nine steps.'
for ($index = 0; $index -lt 9; $index++) {
    $step = $movementSteps[$index]
    Assert-Condition ($step.GetAttribute('Number') -eq [string]$index -and
        $step.InnerText.Trim() -eq [string](2175 + $index) -and
        $step.GetAttribute('FadeIn') -eq '750' -and
        $step.GetAttribute('Hold') -eq '250' -and
        $step.GetAttribute('FadeOut') -eq '0') `
        "Focus movement Chaser step $index is wrong."
}

$widgetIds = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') |
    ForEach-Object { [uint64]$_.GetAttribute('ID') })
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'
foreach ($reference in @($virtualConsole.SelectNodes('.//*[local-name()="Function"]'))) {
    $raw = if ($reference.HasAttribute('ID')) {
        $reference.GetAttribute('ID')
    } else {
        $reference.InnerText.Trim()
    }
    Assert-Condition ($raw -match '^\d+$') "Malformed Virtual Console Function reference: $raw"
    $id = [uint64]$raw
    Assert-Condition ($id -eq $invalidFunctionId -or $functions.ContainsKey([int]$id)) `
        "Virtual Console references missing Function $id."
}
$moveWidget = $virtualConsole.SelectSingleNode('.//*[@ID="1004"][*[local-name()="WindowState"]]')
$moveFunction = $moveWidget.SelectSingleNode('./*[local-name()="Function"]')
Assert-Condition ($null -ne $moveFunction -and $moveFunction.GetAttribute('ID') -eq '2184') `
    'Existing MOVE widget 1004 must target Focus movement Chaser 2184.'

$positionFrameIds = [System.Collections.Generic.HashSet[string]]::new()
for ($offset = 0; $offset -lt 9; $offset++) {
    $channel = 164 + $offset
    $inputs = @($virtualConsole.SelectNodes(
        ".//*[local-name()='Input' and @Universe='1' and @Channel='$channel']"))
    Assert-Condition ($inputs.Count -eq 1) `
        "Position input channel $channel must occur exactly once in the Virtual Console."
    $widget = $inputs[0].ParentNode
    while ($null -ne $widget -and
        ($null -eq $widget.Attributes['ID'] -or
        $null -eq $widget.SelectSingleNode('./*[local-name()="WindowState"]'))) {
        $widget = $widget.ParentNode
    }
    Assert-Condition ($null -ne $widget -and $widget.LocalName -eq 'Button') `
        "Position input channel $channel is not owned by a Button."
    $functionReference = $widget.SelectSingleNode('./*[local-name()="Function"]')
    Assert-Condition ($null -ne $functionReference -and
        $functionReference.GetAttribute('ID') -eq [string](2175 + $offset)) `
        "Position input channel $channel targets the wrong position Scene."
    $ancestor = $widget.ParentNode
    while ($null -ne $ancestor -and $ancestor.LocalName -ne 'SoloFrame') {
        $ancestor = $ancestor.ParentNode
    }
    Assert-Condition ($null -ne $ancestor) `
        "Position input channel $channel is not inside an exclusive SoloFrame."
    [void]$positionFrameIds.Add($ancestor.GetAttribute('ID'))
}
Assert-Condition ($positionFrameIds.Count -eq 1) `
    'All nine position buttons must share one exclusive SoloFrame.'

for ($id = 788; $id -le 797; $id++) {
    $parent = $functions[$id]
    Assert-Condition ($parent.GetAttribute('Type') -eq 'Chaser') `
        "Autoplay parent $id is missing or not a Chaser."
    Assert-Condition ($parent.SelectSingleNode('./*[local-name()="Tempo"]').InnerText -eq 'Beats') `
        "Autoplay parent $id is not beat-counted."
    Assert-Condition ($parent.SelectSingleNode('./*[local-name()="Speed"]').GetAttribute('Duration') -eq '32000') `
        "Autoplay parent $id does not default to 8 measures / 32 beats."
    $expectedSteps = if ($id -in @(792, 797)) { 128 } else { 32 }
    Assert-Condition (@(Get-Steps -Function $parent -Functions $functions).Count -eq $expectedSteps) `
        "Autoplay parent $id has the wrong number of loops."
}
foreach ($id in 798..807) {
    Assert-Condition ($functions[$id].GetAttribute('Type') -eq 'Collection') `
        "Autoplay owner $id is missing or not a Collection."
}

$dwell = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1367"]')
Assert-Condition ($null -ne $dwell -and $dwell.ShowHeader -eq 'True') `
    'Visible Auto Dwell panel is missing.'
$dwellValues = @(1, 2, 4, 8, 16)
for ($page = 0; $page -lt 5; $page++) {
    $buttons = @($dwell.SelectNodes("./*[local-name()='Button' and @Page='$page']"))
    Assert-Condition ($buttons.Count -eq 5) "Dwell values are not visible on page $page."
    for ($slot = 0; $slot -lt 5; $slot++) {
        $id = 1368 + ($page * 5) + $slot
        $button = $dwell.SelectSingleNode("./*[local-name()='Button' and @ID='$id']")
        Assert-Condition ($button.Caption -eq "$($dwellValues[$slot])M") `
            "Dwell label $id is wrong."
        Assert-Condition ([int]$button.Function.ID -eq (1986 + $slot) -and
            [int]$button.Input.Channel -eq (804 + $slot)) "Dwell binding $id is wrong."
    }
}

$speedDial = $virtualConsole.SelectSingleNode('.//*[local-name()="SpeedDial" and @ID="1021"]')
$expectedPresets = @(4000, 8000, 16000, 32000, 64000)
Assert-Condition (@($speedDial.SelectNodes('./*[local-name()="Function"]')).Count -eq 10) `
    'Dwell no longer controls all ten Autoplay parents.'
for ($index = 0; $index -lt 5; $index++) {
    $preset = $speedDial.SelectSingleNode("./*[local-name()='Preset' and @ID='$index']")
    Assert-Condition ([int]$preset.Value -eq $expectedPresets[$index] -and
        [int]$preset.Input.Channel -eq (480 + $index)) "Dwell preset $index is wrong."
}

$tracker = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1393"]')
Assert-Condition ($null -ne $tracker -and [int]$tracker.WindowState.Width -eq 1 -and
    [int]$tracker.WindowState.Height -eq 1) 'The native seek helper is not clipped.'
Assert-Condition (@($tracker.SelectNodes('./*[local-name()="CueList"]')).Count -eq 10) `
    'Autoplay must retain one native Cue List per playback parent.'
Assert-Condition (@($workspace.SelectNodes(
    '//*[local-name()="Input" and @Universe="1" and @Channel="632"]')).Count -eq 10) `
    'Absolute Autoplay seek coverage is incomplete.'

$rails = @($virtualConsole.SelectNodes(
    './/*[local-name()="Frame" and number(@ID) >= 1542 and number(@ID) <= 1573]'))
Assert-Condition ($rails.Count -eq 32) `
    'Expected one native running-state strip below each Autoloop pad.'
$rawFeedbackIds = [System.Collections.Generic.List[int]]::new()
foreach ($rail in $rails) {
    Assert-Condition ([int]$rail.WindowState.Width -eq 246 -and
        [int]$rail.WindowState.Height -eq 12 -and $rail.Disabled -eq 'True') `
        "Running-state strip $($rail.ID) is not read-only/full-width."
    $segments = @($rail.SelectNodes('./*[local-name()="Button"]'))
    Assert-Condition ($segments.Count -eq 4) `
        "Running-state strip $($rail.ID) does not cover four banks."
    foreach ($segment in $segments) {
        Assert-Condition (@($segment.SelectNodes('./*[local-name()="Input"]')).Count -eq 0) `
            "Read-only running-state segment $($segment.ID) owns an Input."
        Assert-Condition ($segment.Appearance.BackgroundColor -eq '4278915616') `
            "Running-state segment $($segment.ID) has the wrong inactive background."
        $rawFeedbackIds.Add([int]$segment.Function.ID)
    }
}
Assert-SameSet -Expected @(532..659) -Actual @($rawFeedbackIds) `
    -Message 'Native running-state feedback does not cover all 128 raw Autoloops exactly once.'

$profilePath = Join-Path $release $profileName
$profile = [System.Xml.XmlDocument]::new()
$profile.XmlResolver = $null
$profile.Load($profilePath)
$profileRoot = $profile.DocumentElement
Assert-Condition ($profileRoot.SelectSingleNode(
        './*[local-name()="Manufacturer"]').InnerText -eq 'SoundSwitch' -and
    $profileRoot.SelectSingleNode('./*[local-name()="Model"]').InnerText -eq
        'Control One Performance' -and
    $profileRoot.SelectSingleNode('./*[local-name()="Type"]').InnerText -eq 'MIDI') `
    'The packaged Control One input profile has the wrong identity.'
$profileChannels = @($profile.SelectNodes('//*[local-name()="Channel"]'))
$profileNumbers = @($profileChannels | ForEach-Object { [int]$_.GetAttribute('Number') })
Assert-Condition (@($profileNumbers | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Control One input profile contains duplicate channel numbers.'
$expectedPositionLabels = @(
    'Position — Crossed Out Down (Shift + Pad)',
    'Position — Crossed In Down (Shift + Pad)',
    'Position — Stage Right (Shift + Pad)',
    'Position — Stage Left (Shift + Pad)',
    'Position — Straight Ahead (Shift + Pad)',
    'Position — Crossed Out Up (Shift + Pad)',
    'Position — Crossed In Up (Shift + Pad)',
    'Position — Up (Shift + Pad)',
    'Position — Down (Shift + Pad)'
)
for ($offset = 0; $offset -lt 9; $offset++) {
    $number = 164 + $offset
    $channel = $profile.SelectSingleNode(
        "//*[local-name()='Channel' and @Number='$number']")
    Assert-Condition ($null -ne $channel -and
        $channel.SelectSingleNode('./*[local-name()="Name"]').InnerText -eq
            $expectedPositionLabels[$offset] -and
        $channel.SelectSingleNode('./*[local-name()="Type"]').InnerText -eq 'Button') `
        "Control One position channel $number is missing or mislabeled."
}

$definition = [System.Xml.XmlDocument]::new()
$definition.XmlResolver = $null
$definition.Load((Join-Path $release $fixtureName))
$definitionRoot = $definition.DocumentElement
Assert-Condition ($definitionRoot.LocalName -eq 'FixtureDefinition' -and
    $definitionRoot.SelectSingleNode('./*[local-name()="Manufacturer"]').InnerText -eq 'American DJ' -and
    $definitionRoot.SelectSingleNode('./*[local-name()="Model"]').InnerText -eq 'Focus Spot Two') `
    'The custom Focus Spot Two definition has the wrong identity.'
$definitionChannels = @($definitionRoot.SelectNodes('./*[local-name()="Channel"]'))
$expectedFocusChannels = @(
    'Pan Movement', 'Pan Fine', 'Tilt Movement', 'Tilt Fine', 'Color Wheel',
    'Gobo Wheel', 'Gobo Rotation', 'Prism', 'Main Strobe/Shutter',
    'Main Master Dimmer', 'UV Strobe/Shutter', 'UV Master Dimmer', 'Focus',
    'Show', 'Show Speed', 'Dimmer Modes', 'Pan/Tilt Speed', 'Function'
)
Assert-Condition ($definitionChannels.Count -eq 18) `
    'The custom Focus Spot Two definition must declare exactly 18 channels.'
Assert-SameSet -Expected $expectedFocusChannels `
    -Actual @($definitionChannels | ForEach-Object { $_.GetAttribute('Name') }) `
    -Message 'The custom Focus Spot Two definition has the wrong channel set.'
foreach ($channel in $definitionChannels) {
    $capabilities = @($channel.SelectNodes('./*[local-name()="Capability"]') |
        Sort-Object { [int]$_.GetAttribute('Min') })
    if ($capabilities.Count -eq 0) { continue }
    $nextValue = 0
    foreach ($capability in $capabilities) {
        $minimum = [int]$capability.GetAttribute('Min')
        $maximum = [int]$capability.GetAttribute('Max')
        Assert-Condition ($minimum -eq $nextValue -and $maximum -ge $minimum -and $maximum -le 255) `
            "Fixture-definition channel $($channel.GetAttribute('Name')) has a gap, overlap, or invalid range."
        $nextValue = $maximum + 1
    }
    Assert-Condition ($nextValue -eq 256) `
        "Fixture-definition channel $($channel.GetAttribute('Name')) does not cover 0..255."
}
$focusMode = $definitionRoot.SelectSingleNode('./*[local-name()="Mode" and @Name="18 Channel"]')
Assert-Condition ($null -ne $focusMode) 'The custom Focus Spot Two definition has no 18 Channel mode.'
$modeChannels = @($focusMode.SelectNodes('./*[local-name()="Channel"]'))
Assert-Condition ($modeChannels.Count -eq 18) 'Focus Spot Two 18 Channel mode is incomplete.'
for ($index = 0; $index -lt 18; $index++) {
    $modeChannel = @($modeChannels | Where-Object {
        $_.GetAttribute('Number') -eq [string]$index
    })
    Assert-Condition ($modeChannel.Count -eq 1 -and
        $modeChannel[0].InnerText.Trim() -eq $expectedFocusChannels[$index]) `
        "Focus Spot Two mode channel $index is wrong."
}
$goboRotation = $focusMode.SelectSingleNode(
    './*[local-name()="Channel" and @Number="6"]')
Assert-Condition ($goboRotation.GetAttribute('ActsOn') -eq '5') `
    'Focus Spot Two gobo-rotation dependency is missing.'
$heads = @($focusMode.SelectNodes('./*[local-name()="Head"]'))
Assert-Condition ($heads.Count -eq 2) 'Focus Spot Two definition must expose separate Main and UV heads.'
$mainHead = @($heads[0].SelectNodes('./*[local-name()="Channel"]') |
    ForEach-Object { [int]$_.InnerText })
$uvHead = @($heads[1].SelectNodes('./*[local-name()="Channel"]') |
    ForEach-Object { [int]$_.InnerText })
Assert-SameSet -Expected @(4, 5, 6, 7, 8, 9, 12) -Actual $mainHead `
    -Message 'Focus Spot Two Main head mapping is wrong.'
Assert-SameSet -Expected @(10, 11) -Actual $uvHead `
    -Message 'Focus Spot Two UV head mapping is wrong.'
$focusPhysical = $definitionRoot.SelectSingleNode('./*[local-name()="Physical"]')
$focusLens = $focusPhysical.SelectSingleNode('./*[local-name()="Lens"]')
$focusMovement = $focusPhysical.SelectSingleNode('./*[local-name()="Focus"]')
Assert-Condition ($focusLens.GetAttribute('PrismFaces') -eq '6' -and
    $focusMovement.GetAttribute('Type') -eq 'Head' -and
    $focusMovement.GetAttribute('PanMax') -eq '540' -and
    $focusMovement.GetAttribute('TiltMax') -eq '230') `
    'Focus Spot Two physical prism or movement metadata is wrong.'

$parseErrors = [System.Collections.Generic.List[string]]::new()
foreach ($scriptName in @('Install-V27.ps1', 'Rollback-V27.ps1', 'Test-V27Package.ps1')) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $release $scriptName), [ref]$tokens, [ref]$errors) | Out-Null
    foreach ($error in @($errors)) { $parseErrors.Add("$scriptName`: $($error.Message)") }
}
Assert-Condition ($parseErrors.Count -eq 0) `
    "PowerShell parser errors: $($parseErrors -join '; ')"

$installText = Get-Content -LiteralPath (Join-Path $release 'Install-V27.ps1') -Raw
Assert-Condition ($installText.Contains($expectedCoreSha256)) `
    'Install-V27.ps1 does not hard-pin the reviewed QLC+ core executable hash.'
Assert-Condition ($installText.Contains("'Fixtures'") -and
    $installText.Contains("'InputProfiles'")) `
    'Install-V27.ps1 does not install the user fixture and input profile.'

$textFiles = Get-ChildItem -LiteralPath $release -File | Where-Object {
    $_.Extension -in @('.json', '.md', '.ps1', '.qxf', '.qxi', '.qxw', '.txt')
}
$personalLeaks = @($textFiles | Select-String -Pattern `
    'C:\\Users\\|(?:^|[\s''"])/Users/|(?:^|[\s''"])/home/|(?:^|[\s''"])/workspace/|\bjloop\b|\bJ:\\' `
    -CaseSensitive:$false)
Assert-Condition ($personalLeaks.Count -eq 0) `
    'A personal path or username appears in the release package.'
Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $release 'qlcplus5.exe'))) `
    'The package must not replace the stock QLC+ core executable.'

Write-Host 'PASS: QLC+ SoundSwitch V27 Full Rig package integrity'
Write-Host '  Exact 16-file package and SHA256SUMS coverage verified'
Write-Host '  Workspace: 2,100 Functions; 22 fixtures; 1,140 live creative Scenes'
Write-Host '  Full-rig closure: 128 Autoloops + 32 Priority roots + performance/overrides'
Write-Host '  Focus movement: nine decoded A/B positions + one beat-counted Chaser'
Write-Host '  Plug-ins: CI-attested SoundSwitch output + unchanged stable OS2L timing'
Write-Host '  Structural/software checks only; physical bench and gig qualification remain required'
Write-Host "  Workspace SHA-256: $($packageHashes[$workspaceName])"
