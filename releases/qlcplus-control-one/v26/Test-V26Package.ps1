[CmdletBinding()]
param(
    [string]$ReleaseDirectory = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'
$invalidFunctionId = [uint64]4294967295

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

$release = (Resolve-Path -LiteralPath $ReleaseDirectory).Path
$expectedFiles = @(
    'Install-V26.ps1',
    'IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw',
    'LICENSE-APACHE-2.0.txt',
    'os2l.dll',
    'README.md',
    'RELEASE_NOTES.md',
    'Rollback-V26.ps1',
    'SHA256SUMS.txt',
    'SoundSwitch-Control-One-Performance.qxi',
    'soundswitch.dll',
    'Test-V26Package.ps1',
    'VIRTUALDJ_OS2L_AUTO_RECONNECT.md'
)
$actualFiles = @(Get-ChildItem -LiteralPath $release -File | ForEach-Object Name | Sort-Object)
Assert-Condition ((Compare-Object ($expectedFiles | Sort-Object) $actualFiles).Count -eq 0) `
    'The V26 package file set is incomplete or contains an unexpected file.'

$expectedHashes = @{
    'IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw' = 'ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570'
    'soundswitch.dll' = '2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC'
    'os2l.dll' = 'EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513'
}
foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actual = (Get-FileHash -LiteralPath (Join-Path $release $entry.Key) -Algorithm SHA256).Hash
    Assert-Condition ($actual -eq $entry.Value) "Hash mismatch for $($entry.Key)."
}

$checksumLines = @(Get-Content -LiteralPath (Join-Path $release 'SHA256SUMS.txt') |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$listedFiles = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in $checksumLines) {
    Assert-Condition ($line -match '^([A-F0-9]{64})  (.+)$') "Malformed checksum line: $line"
    $expected = $Matches[1]
    $name = $Matches[2]
    Assert-Condition ($name -ne 'SHA256SUMS.txt') 'The checksum file must not hash itself.'
    Assert-Condition ($listedFiles.Add($name)) "Duplicate checksum entry: $name"
    $path = Join-Path $release $name
    Assert-Condition (Test-Path -LiteralPath $path -PathType Leaf) "Checksummed file missing: $name"
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    Assert-Condition ($actual -eq $expected) "Checksum mismatch: $name"
}
$expectedChecksummed = @($expectedFiles | Where-Object { $_ -ne 'SHA256SUMS.txt' })
Assert-Condition ((Compare-Object ($expectedChecksummed | Sort-Object) @($listedFiles | Sort-Object)).Count -eq 0) `
    'SHA256SUMS.txt does not cover the complete package.'

$workspacePath = Join-Path $release 'IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw'
$workspace = [System.Xml.XmlDocument]::new()
$workspace.PreserveWhitespace = $false
$workspace.Load($workspacePath)
$engine = $workspace.SelectSingleNode('//*[local-name()="Engine"]')
$virtualConsole = $workspace.SelectSingleNode('//*[local-name()="VirtualConsole"]')
Assert-Condition ($null -ne $engine -and $null -ne $virtualConsole) 'Workspace Engine or Virtual Console is missing.'

$functions = @{}
foreach ($function in @($engine.SelectNodes('./*[local-name()="Function"]'))) {
    $id = [int]$function.GetAttribute('ID')
    Assert-Condition (-not $functions.ContainsKey($id)) "Duplicate Function ID $id."
    $functions[$id] = $function
}
Assert-Condition ($functions.Count -eq 2090) 'Expected exactly 2,090 lighting Functions.'

$fixtureTuples = @($engine.SelectNodes('./*[local-name()="Fixture"]') | ForEach-Object {
    "$($_.Universe):$($_.Address):$($_.Model)"
})
foreach ($tuple in @(
        '0:0:IR-4 (BOIR4)', '0:10:IR-4 (BOIR4)', '0:20:IR-4 (BOIR4)', '0:30:IR-4 (BOIR4)',
        '0:174:BO-TUBE192', '0:214:BO-TUBE192', '0:254:BO-TUBE192', '0:294:BO-TUBE192',
        '2:0:IR-4 (BOIR4)', '2:10:IR-4 (BOIR4)', '2:20:IR-4 (BOIR4)', '2:30:IR-4 (BOIR4)',
        '2:174:BO-TUBE192', '2:214:BO-TUBE192', '2:254:BO-TUBE192', '2:294:BO-TUBE192')) {
    Assert-Condition ($tuple -in $fixtureTuples) "Fixture patch tuple missing: $tuple"
}

for ($id = 788; $id -le 797; $id++) {
    $parent = $functions[$id]
    Assert-Condition ($null -ne $parent -and $parent.GetAttribute('Type') -eq 'Chaser') `
        "Autoplay parent $id is missing or not a Chaser."
    Assert-Condition ($parent.SelectSingleNode('./*[local-name()="Tempo"]').InnerText -eq 'Beats') `
        "Autoplay parent $id is not beat-counted."
    Assert-Condition ($parent.SelectSingleNode('./*[local-name()="Speed"]').GetAttribute('Duration') -eq '32000') `
        "Autoplay parent $id does not default to 8 measures / 32 beats."
    $expectedSteps = if ($id -in @(792, 797)) { 128 } else { 32 }
    Assert-Condition (@($parent.SelectNodes('./*[local-name()="Step"]')).Count -eq $expectedSteps) `
        "Autoplay parent $id has the wrong number of loops."
}

foreach ($id in 798..807) {
    $owner = $functions[$id]
    Assert-Condition ($null -ne $owner -and $owner.GetAttribute('Type') -eq 'Collection') `
        "Autoplay owner $id is missing or not a Collection."
}

$widgetIds = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') |
    ForEach-Object { [uint64]$_.GetAttribute('ID') })
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'

$missingReferences = [System.Collections.Generic.List[uint64]]::new()
foreach ($reference in @($virtualConsole.SelectNodes('.//*[local-name()="Function"]'))) {
    $raw = if ($reference.HasAttribute('ID')) { $reference.GetAttribute('ID') } else { $reference.InnerText.Trim() }
    Assert-Condition ($raw -match '^\d+$') "Malformed Function reference: $raw"
    $id = [uint64]$raw
    if ($id -ne $invalidFunctionId -and -not $functions.ContainsKey([int]$id)) {
        $missingReferences.Add($id)
    }
}
Assert-Condition ($missingReferences.Count -eq 0) `
    "Missing Virtual Console Function references: $($missingReferences -join ', ')."

$dwell = $virtualConsole.SelectSingleNode('.//*[local-name()="Frame" and @ID="1367"]')
Assert-Condition ($null -ne $dwell -and $dwell.ShowHeader -eq 'True') 'Visible Auto Dwell panel is missing.'
$dwellValues = @(1, 2, 4, 8, 16)
for ($page = 0; $page -lt 5; $page++) {
    $buttons = @($dwell.SelectNodes("./*[local-name()='Button' and @Page='$page']"))
    Assert-Condition ($buttons.Count -eq 5) "Dwell values are not visible on page $page."
    for ($slot = 0; $slot -lt 5; $slot++) {
        $id = 1368 + ($page * 5) + $slot
        $button = $dwell.SelectSingleNode("./*[local-name()='Button' and @ID='$id']")
        Assert-Condition ($button.Caption -eq "$($dwellValues[$slot])M") "Dwell label $id is wrong."
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
Assert-Condition ($rails.Count -eq 32) 'Expected one native running-state strip below each Autoloop pad.'
$rawIds = [System.Collections.Generic.List[int]]::new()
foreach ($rail in $rails) {
    Assert-Condition ([int]$rail.WindowState.Width -eq 246 -and [int]$rail.WindowState.Height -eq 12 -and
        $rail.Disabled -eq 'True') "Running-state strip $($rail.ID) is not read-only/full-width."
    $segments = @($rail.SelectNodes('./*[local-name()="Button"]'))
    Assert-Condition ($segments.Count -eq 4) "Running-state strip $($rail.ID) does not cover four banks."
    foreach ($segment in $segments) {
        Assert-Condition (@($segment.SelectNodes('./*[local-name()="Input"]')).Count -eq 0) `
            "Read-only running-state segment $($segment.ID) owns an Input."
        Assert-Condition ($segment.Appearance.BackgroundColor -eq '4278915616') `
            "Running-state segment $($segment.ID) is not using the quiet inactive background."
        $rawIds.Add([int]$segment.Function.ID)
    }
}
Assert-Condition ((Compare-Object @(532..659) @($rawIds | Sort-Object)).Count -eq 0) `
    'Native running-state feedback does not cover all 128 raw Autoloops exactly once.'

$profilePath = Join-Path $release 'SoundSwitch-Control-One-Performance.qxi'
[xml]$profile = Get-Content -LiteralPath $profilePath -Raw
$channels = @($profile.SelectNodes('//*[local-name()="Channel"]') | ForEach-Object { [int]$_.Number })
Assert-Condition (@($channels | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Control One input profile contains duplicate channel numbers.'

$parseErrors = [System.Collections.Generic.List[string]]::new()
foreach ($scriptName in @('Install-V26.ps1', 'Rollback-V26.ps1', 'Test-V26Package.ps1')) {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $release $scriptName), [ref]$tokens, [ref]$errors) | Out-Null
    foreach ($error in @($errors)) { $parseErrors.Add("$scriptName`: $($error.Message)") }
}
Assert-Condition ($parseErrors.Count -eq 0) "PowerShell parser errors: $($parseErrors -join '; ')"

$textFiles = Get-ChildItem -LiteralPath $release -File | Where-Object {
    $_.Extension -in @('.md', '.ps1', '.txt', '.qxw', '.qxi')
}
$personalLeaks = @($textFiles | Select-String -Pattern 'C:\\Users\\|\bjloop\b|\bJ:\\' -CaseSensitive:$false)
Assert-Condition ($personalLeaks.Count -eq 0) 'A personal path or username appears in the release package.'
Assert-Condition (-not (Test-Path -LiteralPath (Join-Path $release 'qlcplus5.exe'))) `
    'The package must not replace the stock QLC+ core executable.'

Write-Host 'PASS: QLC+ SoundSwitch V26 package integrity'
Write-Host '  Stock QLC+ core retained; no tracker service or second runtime application'
Write-Host '  Workspace: 2,090 Functions; 10 native Autoplay parents; 128 native monitors'
Write-Host '  Dwell: visible 1/2/4/8/16-measure controls on every state'
Write-Host '  Plug-ins: SoundSwitch USB output + stable direct OS2L BPM clock'
Write-Host "  Workspace SHA-256: $($expectedHashes['IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw'])"
