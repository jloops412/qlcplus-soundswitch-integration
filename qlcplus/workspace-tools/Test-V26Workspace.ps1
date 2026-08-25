[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw'),
    [string]$CandidateWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw')
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
$sourceEngine = $source.SelectSingleNode('//*[local-name()="Engine"]')
$candidateEngine = $candidate.SelectSingleNode('//*[local-name()="Engine"]')
Assert-Condition ($sourceEngine.OuterXml -ceq $candidateEngine.OuterXml) `
    'Engine/fixture/I-O XML changed during the V26 UI and performance pass.'

$functions = Get-FunctionMap $candidate
Assert-Condition ($functions.Count -eq 2090) 'Expected exactly 2,090 Functions.'
foreach ($id in 788..797) {
    $function = $functions[$id]
    Assert-Condition ($null -ne $function -and $function.GetAttribute('Type') -eq 'Chaser') `
        "Autoplay parent $id is missing or not a Chaser."
    Assert-Condition ($function.SelectSingleNode('./*[local-name()="Speed"]').GetAttribute('Duration') -eq '32000') `
        "Autoplay parent $id no longer defaults to 8 measures / 32 beats."
    Assert-Condition ($function.SelectSingleNode('./*[local-name()="Tempo"]').InnerText -eq 'Beats') `
        "Autoplay parent $id is no longer beat-counted."
}

$virtualConsole = $candidate.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$widgetIds = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') |
    ForEach-Object { [uint64]$_.GetAttribute('ID') })
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'

$missingReferences = [System.Collections.Generic.List[uint64]]::new()
foreach ($reference in @($virtualConsole.SelectNodes('.//*[local-name()="Function"]'))) {
    $raw = if ($reference.HasAttribute('ID')) {
        $reference.GetAttribute('ID')
    } else {
        $reference.InnerText.Trim()
    }
    Assert-Condition ($raw -match '^\d+$') "Malformed Function reference $raw."
    $id = [uint64]$raw
    if ($id -ne $invalidFunctionId -and -not $functions.ContainsKey([int]$id)) {
        $missingReferences.Add($id)
    }
}
Assert-Condition ($missingReferences.Count -eq 0) `
    "Missing Virtual Console Function references: $($missingReferences -join ', ')."

$dwell = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1367"]')
Assert-Condition ($null -ne $dwell -and $dwell.ShowHeader -eq 'True') `
    'The visible Auto Dwell panel is missing.'
Assert-Condition (@($dwell.SelectNodes('./*[local-name()="Button"]')).Count -eq 25) `
    'Auto Dwell must show five controls on each of its five native pages.'
$dwellValues = @(1, 2, 4, 8, 16)
for ($page = 0; $page -lt 5; $page++) {
    $shortcut = $dwell.SelectSingleNode("./*[local-name()='Shortcut' and @Page='$page']")
    Assert-Condition ($null -ne $shortcut -and
        [int]$shortcut.Input.Channel -eq (480 + $page)) `
        "Dwell page $page lost its native SpeedDial feedback."
    $pageButtons = @($dwell.SelectNodes("./*[local-name()='Button' and @Page='$page']"))
    Assert-Condition ($pageButtons.Count -eq 5) "Dwell page $page does not show all values."
    for ($slot = 0; $slot -lt 5; $slot++) {
        $widgetId = 1368 + ($page * 5) + $slot
        $button = $dwell.SelectSingleNode("./*[local-name()='Button' and @ID='$widgetId']")
        Assert-Condition ($null -ne $button -and [int]$button.Page -eq $page -and
            $button.Caption -eq "$($dwellValues[$slot])M") `
            "Dwell button $widgetId is not visible/labeled correctly."
        Assert-Condition ([int]$button.Function.ID -eq (1986 + $slot) -and
            $button.Input.Universe -eq '1' -and [int]$button.Input.Channel -eq (804 + $slot)) `
            "Dwell button $widgetId lost its public binding."
        $expectedColor = if ($page -eq $slot) { '4279208543' } else { '4280694348' }
        Assert-Condition ($button.Appearance.BackgroundColor -eq $expectedColor) `
            "Dwell page $page does not mark its selected value green."
    }
}

$speedDial = $virtualConsole.SelectSingleNode('.//*[local-name()="SpeedDial" and @ID="1021"]')
$expectedPresets = @(4000, 8000, 16000, 32000, 64000)
Assert-Condition (@($speedDial.SelectNodes('./*[local-name()="Function"]')).Count -eq 10) `
    'The dwell SpeedDial must still control all ten Autoplay parents.'
for ($index = 0; $index -lt $expectedPresets.Count; $index++) {
    $preset = $speedDial.SelectSingleNode("./*[local-name()='Preset' and @ID='$index']")
    Assert-Condition ([int]$preset.Value -eq $expectedPresets[$index] -and
        [int]$preset.Input.Channel -eq (480 + $index)) `
        "Dwell preset $index is incorrect."
}

$tracker = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1393"]')
Assert-Condition ($null -ne $tracker -and [int]$tracker.WindowState.Width -eq 1 -and
    [int]$tracker.WindowState.Height -eq 1) 'The native seek engine is visible.'
$cueLists = @($tracker.SelectNodes('./*[local-name()="CueList"]'))
Assert-Condition ($cueLists.Count -eq 10) 'Expected ten native Autoplay seek CueLists.'
Assert-Condition (@($candidate.SelectNodes(
    '//*[local-name()="Input" and @Universe="1" and @Channel="632"]')).Count -eq 10) `
    'Absolute seek channel 632 is incomplete.'

$rails = @($virtualConsole.SelectNodes(
    './/*[local-name()="Frame" and number(@ID) >= 1542 and number(@ID) <= 1573]'))
Assert-Condition ($rails.Count -eq 32) 'Expected one active strip below every pad.'
$rawIds = [System.Collections.Generic.List[int]]::new()
foreach ($rail in $rails) {
    Assert-Condition ([int]$rail.WindowState.Width -eq 246 -and
        [int]$rail.WindowState.Height -eq 12 -and $rail.Disabled -eq 'True') `
        "Rail $($rail.ID) is not a lean read-only pad strip."
    $segments = @($rail.SelectNodes('./*[local-name()="Button"]'))
    Assert-Condition ($segments.Count -eq 4) "Rail $($rail.ID) does not show all four banks."
    $totalWidth = 0
    foreach ($segment in $segments) {
        Assert-Condition (@($segment.SelectNodes('./*[local-name()="Input"]')).Count -eq 0) `
            "Read-only rail segment $($segment.ID) owns an Input."
        Assert-Condition ($segment.Appearance.BackgroundColor -eq '4278915616') `
            "Rail segment $($segment.ID) is not using the quiet inactive background."
        $rawIds.Add([int]$segment.Function.ID)
        $totalWidth += [int]$segment.WindowState.Width
    }
    Assert-Condition ($totalWidth -eq 243) `
        "Rail $($rail.ID) has an unexpected active-segment footprint."
}
Assert-Condition ((Compare-Object @(532..659) @($rawIds | Sort-Object)).Count -eq 0) `
    'Native active-loop feedback does not cover raw Chasers 532-659 exactly once.'

$workspaceText = Get-Content -LiteralPath $CandidateWorkspace -Raw
Assert-Condition ($workspaceText -notmatch 'C:\\Users\\|\bjloop\b|\bJ:\\') `
    'A personal path or username leaked into the workspace.'

Write-Host 'PASS: V26 workspace regression'
Write-Host '  Engine / fixtures / Functions / I-O: byte-identical to V25'
Write-Host '  Autoplay: 10 parents, 10 native seek lanes, 128 active raw-loop monitors'
Write-Host '  Auto Dwell: 1/2/4/8/16 measures = 4/8/16/32/64 beats'
Write-Host '  UI: 32 full-width active strips; dwell values visible on every page'
Write-Host "  SHA-256: $((Get-FileHash -LiteralPath $CandidateWorkspace -Algorithm SHA256).Hash)"
