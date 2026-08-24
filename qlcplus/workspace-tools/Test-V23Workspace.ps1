[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot '..\..\releases\qlcplus-control-one\v22\IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw'),
    [string]$CandidateWorkspace = (Join-Path $PSScriptRoot '..\..\releases\qlcplus-control-one\v23\IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw')
)

$ErrorActionPreference = 'Stop'
$invalidFunctionId = [uint64]4294967295

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Import-Workspace {
    param([string]$Path)
    $document = [System.Xml.XmlDocument]::new()
    $document.PreserveWhitespace = $false
    $document.Load((Resolve-Path -LiteralPath $Path).Path)
    return $document
}

function Get-FunctionMap {
    param([System.Xml.XmlDocument]$Document)
    $map = @{}
    foreach ($function in @($Document.SelectNodes(
            '//*[local-name()="Engine"]/*[local-name()="Function"]'))) {
        $id = [int]$function.GetAttribute('ID')
        Assert-Condition (-not $map.ContainsKey($id)) "Duplicate Function ID $id."
        $map[$id] = $function
    }
    return $map
}

$source = Import-Workspace $SourceWorkspace
$candidate = Import-Workspace $CandidateWorkspace
$sourceFunctions = Get-FunctionMap $source
$candidateFunctions = Get-FunctionMap $candidate

Assert-Condition ($sourceFunctions.Count -eq 2090 -and $candidateFunctions.Count -eq 2090) `
    'The source or candidate Function count is not 2,090.'
Assert-Condition ((Compare-Object @($sourceFunctions.Keys | Sort-Object) `
    @($candidateFunctions.Keys | Sort-Object)).Count -eq 0) 'Function IDs changed.'
$changedFunctions = @($sourceFunctions.Keys | Where-Object {
    $sourceFunctions[$_].OuterXml -cne $candidateFunctions[$_].OuterXml
})
Assert-Condition ($changedFunctions.Count -eq 0) `
    "Lighting Functions changed: $($changedFunctions -join ', ')."

foreach ($name in @('Fixture', 'InputOutputMap', 'FixtureGroup', 'Monitor', 'ChannelsGroup')) {
    $before = @($source.SelectNodes(
        "//*[local-name()='Engine']/*[local-name()='$name']") | ForEach-Object OuterXml) -join "`n"
    $after = @($candidate.SelectNodes(
        "//*[local-name()='Engine']/*[local-name()='$name']") | ForEach-Object OuterXml) -join "`n"
    Assert-Condition ($before -ceq $after) "$name XML changed."
}

$fixtures = @($candidate.SelectNodes(
    '//*[local-name()="Engine"]/*[local-name()="Fixture"]'))
Assert-Condition ($fixtures.Count -eq 16) 'Expected 8 physical and 8 private fixtures.'
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
            [int]$_.Universe -eq $universe -and [int]$_.Address -eq $expected.Address -and
            [int]$_.Channels -eq $expected.Channels -and [string]$_.Mode -ceq $expected.Mode
        })
        Assert-Condition ($matches.Count -eq 1) `
            "Fixture patch mismatch at Universe $($universe + 1), address $($expected.Address + 1)."
    }
}

$widgetIds = @($candidate.SelectNodes(
    '//*[local-name()="VirtualConsole"]//*[@ID][*[local-name()="WindowState"]]') |
    ForEach-Object { [uint64]$_.GetAttribute('ID') })
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'

$missingReferences = [System.Collections.Generic.List[uint64]]::new()
foreach ($reference in @($candidate.SelectNodes(
        '//*[local-name()="VirtualConsole"]//*[local-name()="Function"]'))) {
    $raw = if ($reference.HasAttribute('ID')) {
        $reference.GetAttribute('ID')
    } else {
        $reference.InnerText.Trim()
    }
    Assert-Condition ($raw -match '^\d+$') "Malformed Function reference $raw."
    $id = [uint64]$raw
    if ($id -ne $invalidFunctionId -and -not $candidateFunctions.ContainsKey([int]$id)) {
        $missingReferences.Add($id)
    }
}
Assert-Condition ($missingReferences.Count -eq 0) `
    "Missing Virtual Console Function references: $($missingReferences -join ', ')."

$virtualConsole = $candidate.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$modeButtons = @($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input" and @Universe="1" and @Channel="811"]')).Count -gt 0
})
Assert-Condition ($modeButtons.Count -eq 1) 'Exactly one mouse mode switch must own logical channel 811.'
Assert-Condition ($modeButtons[0].SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID') -eq '1993') `
    'The mode switch no longer targets public Function 1993.'
Assert-Condition (-not $modeButtons[0].HasAttribute('Page')) `
    'The mouse mode switch must remain visible in both performance modes.'

$outline = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1413"]')
Assert-Condition ($null -ne $outline -and $outline.Disabled -ceq 'True') `
    'The active-loop monitor is missing or is not read-only.'
Assert-Condition (@($outline.SelectNodes('ancestor::*[local-name()="SoloFrame"]')).Count -eq 0) `
    'The active-loop monitor entered the playback owner SoloFrame.'
$monitors = @($outline.SelectNodes('./*[local-name()="Button"]'))
Assert-Condition ($monitors.Count -eq 128) 'The active-loop monitor must contain 128 buttons.'
Assert-Condition (@($monitors | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input"]')).Count -gt 0
}).Count -eq 0) 'Read-only active-loop monitors must not own external Inputs.'
$rawMonitorIds = @($monitors | ForEach-Object {
    [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
} | Sort-Object)
Assert-Condition ((Compare-Object @(532..659) $rawMonitorIds).Count -eq 0) `
    'The live rail does not cover all 128 raw Autoloops exactly once.'
for ($pad = 0; $pad -lt 32; $pad++) {
    $padMonitors = @($monitors | Where-Object {
        (([int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID') - 532) % 32) -eq $pad
    })
    Assert-Condition ($padMonitors.Count -eq 4) "Pad $($pad + 1) does not have four bank indicators."
    $geometries = @($padMonitors | ForEach-Object {
        "$($_.WindowState.X),$($_.WindowState.Y),$($_.WindowState.Width),$($_.WindowState.Height)"
    } | Sort-Object -Unique)
    Assert-Condition ($geometries.Count -eq 4) `
        "Pad $($pad + 1) has overlapping bank indicators."
}

$nowPlaying = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1393"]')
$cueLists = @($nowPlaying.SelectNodes('./*[local-name()="CueList"]'))
Assert-Condition ($cueLists.Count -eq 10) 'The native Autoplay tracker must contain ten CueLists.'
Assert-Condition (@($cueLists | Where-Object {
    [int]$_.WindowState.Height -lt 150
}).Count -eq 0) 'One or more native Autoplay trackers are too short to show the current row.'

$workspaceText = Get-Content -LiteralPath $CandidateWorkspace -Raw
Assert-Condition ($workspaceText -notmatch 'C:\\Users\\|\bjloop\b|\bJ:\\') `
    'A personal path or username leaked into the workspace.'

Write-Host 'PASS: V23 workspace regression'
Write-Host "  Lighting Functions byte-identical to V22: $($candidateFunctions.Count)"
Write-Host "  Unique Virtual Console widgets: $($widgetIds.Count)"
Write-Host '  Live pad tracking: 128 raw Chasers, four non-overlapping bank indicators per pad'
Write-Host '  Mode switch: Function 1993 / logical channel 811 / persistent'
Write-Host "  SHA-256: $((Get-FileHash -LiteralPath $CandidateWorkspace -Algorithm SHA256).Hash)"
