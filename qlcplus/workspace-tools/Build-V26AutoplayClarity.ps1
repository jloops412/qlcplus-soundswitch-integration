[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw'),
    [string]$OutputWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw')
)

$ErrorActionPreference = 'Stop'
$expectedSourceHash = '2EE9FEDEDA8CEDE6F4D2659C296035B9BF1340952DE8C46E50E401DDEF28BC9B'
$expectedAutoplayParents = 788..797
$expectedRawLoops = 532..659
$dwellButtonIds = 1368..1392

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-Widget {
    param([System.Xml.XmlDocument]$Document, [int]$Id)
    return $Document.SelectSingleNode(
        "//*[local-name()='VirtualConsole']//*[@ID='$Id'][*[local-name()='WindowState']]")
}

function Get-BindingSignature {
    param([System.Xml.XmlElement]$Widget)
    $parts = [System.Collections.Generic.List[string]]::new()
    foreach ($function in @($Widget.SelectNodes('./*[local-name()="Function"]'))) {
        $parts.Add("F:$($function.OuterXml)")
    }
    foreach ($input in @($Widget.SelectNodes('.//*[local-name()="Input"]'))) {
        $parts.Add("I:$($input.OuterXml)")
    }
    return $parts -join '|'
}

function Set-WindowGeometry {
    param(
        [System.Xml.XmlElement]$Widget,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height,
        [int]$Z
    )
    $window = $Widget.SelectSingleNode('./*[local-name()="WindowState"]')
    Assert-Condition ($null -ne $window) "Widget $($Widget.GetAttribute('ID')) has no WindowState."
    $window.SetAttribute('Visible', 'True')
    $window.SetAttribute('X', $X.ToString())
    $window.SetAttribute('Y', $Y.ToString())
    $window.SetAttribute('Width', $Width.ToString())
    $window.SetAttribute('Height', $Height.ToString())
    if ($Z -ge 0) { $window.SetAttribute('Z', $Z.ToString()) }
}

$source = (Resolve-Path -LiteralPath $SourceWorkspace).Path
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Assert-Condition ($sourceHash -eq $expectedSourceHash) `
    "V26 must be built from the reviewed V25 candidate; found $sourceHash."

$backupDirectory = Join-Path $PSScriptRoot 'backups'
if (-not (Test-Path -LiteralPath $backupDirectory)) {
    New-Item -ItemType Directory -Path $backupDirectory | Out-Null
}
$backupPath = Join-Path $backupDirectory 'IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw'
if (-not (Test-Path -LiteralPath $backupPath)) {
    Copy-Item -LiteralPath $source -Destination $backupPath
}
Assert-Condition ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -eq $expectedSourceHash) `
    'The protected V25 backup does not match the reviewed source.'

$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $false
$document.Load($source)

$engine = $document.SelectSingleNode('//*[local-name()="Engine"]')
$engineBefore = $engine.OuterXml
$virtualConsole = $document.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$dwellFrame = Get-Widget $document 1367
$header = Get-Widget $document 1000
$autoloopFrame = Get-Widget $document 1045
$autoloopPages = Get-Widget $document 1046
$tracker = Get-Widget $document 1393

Assert-Condition ($null -ne $dwellFrame -and $dwellFrame.LocalName -eq 'Frame') `
    'The V25 Autoplay dwell frame was not found.'
Assert-Condition ($null -ne $header -and $null -ne $autoloopFrame -and
    $null -ne $autoloopPages -and $null -ne $tracker) `
    'A required V25 console surface is missing.'

$widgetBindingsBefore = @{}
$widgetIdsBefore = [System.Collections.Generic.HashSet[int]]::new()
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    [void]$widgetIdsBefore.Add($id)
    $widgetBindingsBefore[$id] = Get-BindingSignature $widget
}

# QLC+ Multipage children are visible only on their assigned page. Keep the
# five small button copies per page so all dwell choices remain visible while
# the frame's native page state colors the selected value green. This costs
# only 20 tiny UI widgets and avoids any polling/tracker process.
$dwellFrame.SetAttribute('Caption', 'AUTO DWELL  •  MEASURES (4 BEATS EACH)')
$dwellFrame.SelectSingleNode('./*[local-name()="ShowHeader"]').InnerText = 'True'
$dwellValues = @(1, 2, 4, 8, 16)
for ($page = 0; $page -lt $dwellValues.Count; $page++) {
    $shortcut = $dwellFrame.SelectSingleNode("./*[local-name()='Shortcut' and @Page='$page']")
    Assert-Condition ($null -ne $shortcut) "Dwell shortcut page $page is missing."
    $measureWord = if ($dwellValues[$page] -eq 1) { 'MEASURE' } else { 'MEASURES' }
    $shortcut.SetAttribute('Name', "$($dwellValues[$page]) $measureWord")

    for ($slot = 0; $slot -lt $dwellValues.Count; $slot++) {
        $id = 1368 + ($page * 5) + $slot
        $button = Get-Widget $document $id
        Assert-Condition ($null -ne $button) "Dwell button $id is missing."
        $button.SetAttribute('Page', $page.ToString())
        $button.SetAttribute('Caption', "$($dwellValues[$slot])M")
        $background = if ($page -eq $slot) { '4279208543' } else { '4280694348' }
        $button.SelectSingleNode('./*[local-name()="Appearance"]/*[local-name()="BackgroundColor"]').InnerText = $background
    }
}

# Turn each tiny four-bank rail into a full-width strip at the bottom of its
# matching pad. The four segments retain their original raw-Chaser bindings;
# QLC+'s native monitor state moves the visible highlight as Autoplay advances.
for ($index = 0; $index -lt 32; $index++) {
    $frame = Get-Widget $document (1542 + $index)
    Assert-Condition ($null -ne $frame) "Active-loop rail $index is missing."
    $window = $frame.SelectSingleNode('./*[local-name()="WindowState"]')
    $x = [int]$window.GetAttribute('X') + 20
    $y = [int]$window.GetAttribute('Y')
    Set-WindowGeometry $frame $x $y 246 12 50

    $buttons = @($frame.SelectNodes('./*[local-name()="Button"]')) | Sort-Object {
        [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
    }
    Assert-Condition ($buttons.Count -eq 4) "Active-loop rail $index must contain four bank monitors."
    for ($bank = 0; $bank -lt 4; $bank++) {
        $width = if ($bank -eq 3) { 63 } else { 60 }
        Set-WindowGeometry $buttons[$bank] (61 * $bank) 0 $width 12 1
        # Keep inactive segments quiet so QLC+'s stock amber Monitoring border
        # is unmistakable even on the formerly yellow/red bank indicators.
        $buttons[$bank].SelectSingleNode(
            './*[local-name()="Appearance"]/*[local-name()="BackgroundColor"]').InnerText = '4278915616'
    }
}

$pageLabels = @(
    @(1047, 'BANK 1 • MEDIUM'),
    @(1090, 'BANK 2 • COLORFUL'),
    @(1133, 'BANK 3 • SLOW DANCE'),
    @(1176, 'BANK 4 • FLASHY')
)
foreach ($entry in $pageLabels) {
    $label = Get-Widget $document $entry[0]
    Assert-Condition ($null -ne $label) "Bank heading $($entry[0]) is missing."
    $label.SetAttribute('Caption', "$($entry[1])  •  ACTIVE STRIP BELOW EACH PAD  •  AMBER = PLAYING")
}

$header.SetAttribute('Caption',
    "CONTROL ONE  •  LIVE PERFORMANCE`nAUTOLOOPS  •  PRIORITY LOOKS  •  NATIVE ACTIVE-LOOP FEEDBACK")
$autoloopFrame.SetAttribute('Caption', 'PERFORMANCE PADS')
$autoloopPages.SetAttribute('Caption', 'AUTOLOOPS  •  4 BANKS × 32  •  ONE ACTIVE LOOP')

Assert-Condition ($engine.OuterXml -ceq $engineBefore) `
    'Engine XML changed during this UI/performance-only workspace pass.'

$widgetIdsAfter = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') | ForEach-Object {
    [int]$_.GetAttribute('ID')
})
Assert-Condition (@($widgetIdsAfter | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'
Assert-Condition (@($dwellButtonIds | Where-Object { $_ -notin $widgetIdsAfter }).Count -eq 0) `
    'A required multipage dwell button is missing.'
Assert-Condition ($widgetIdsAfter.Count -eq $widgetIdsBefore.Count) `
    'V26 must retain the reviewed V25 widget set.'

foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    if ($widgetBindingsBefore.ContainsKey($id)) {
        Assert-Condition ((Get-BindingSignature $widget) -ceq $widgetBindingsBefore[$id]) `
            "Function/Input binding changed on retained widget $id."
    }
}

$cueLists = @($tracker.SelectNodes('./*[local-name()="CueList"]'))
Assert-Condition ($cueLists.Count -eq 10) 'V26 must retain ten Autoplay seek CueLists.'
$parentIds = @($cueLists | ForEach-Object {
    [int]$_.SelectSingleNode('./*[local-name()="Chaser"]').InnerText
})
Assert-Condition ((Compare-Object $expectedAutoplayParents $parentIds).Count -eq 0) `
    'Autoplay seek no longer covers parents 788-797 exactly once.'
Assert-Condition (@($document.SelectNodes(
    '//*[local-name()="Input" and @Universe="1" and @Channel="632"]')).Count -eq 10) `
    'V26 must retain all ten absolute-seek bindings.'

$monitorButtons = @($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    $function = $_.SelectSingleNode('./*[local-name()="Function"]')
    $null -ne $function -and [uint64]$function.GetAttribute('ID') -in $expectedRawLoops -and
        @($_.SelectNodes('./*[local-name()="Input"]')).Count -eq 0
})
Assert-Condition ($monitorButtons.Count -eq 128) `
    'V26 must retain all 128 native raw-Chaser indicators.'
Assert-Condition (@($virtualConsole.SelectNodes(
    './/*[local-name()="Frame" and number(@ID) >= 1542 and number(@ID) <= 1573]')).Count -eq 32) `
    'V26 must retain all 32 active pad/bank rails.'
foreach ($channel in 804..808) {
    Assert-Condition (@($virtualConsole.SelectNodes(
        ".//*[local-name()='Input' and @Universe='1' and @Channel='$channel']")).Count -eq 5) `
        "UI dwell channel $channel must appear once on each of five pages."
}

$outputParent = Split-Path -Parent $OutputWorkspace
if (-not (Test-Path -LiteralPath $outputParent)) {
    New-Item -ItemType Directory -Path $outputParent | Out-Null
}

$settings = [System.Xml.XmlWriterSettings]::new()
$settings.Indent = $true
$settings.IndentChars = '  '
$settings.NewLineChars = "`r`n"
$settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace
$settings.Encoding = [System.Text.UTF8Encoding]::new($false)
$settings.CloseOutput = $true
$writer = [System.Xml.XmlWriter]::Create($OutputWorkspace, $settings)
try { $document.Save($writer) } finally { $writer.Dispose() }

[xml]$roundTrip = Get-Content -LiteralPath $OutputWorkspace -Raw
Assert-Condition ($roundTrip.SelectNodes(
    '//*[local-name()="Engine"]/*[local-name()="Function"]').Count -eq 2090) `
    'V26 Function count changed.'
Assert-Condition (@($roundTrip.SelectNodes(
    '//*[local-name()="Frame" and @ID="1367"]/*[local-name()="Button"]')).Count -eq 25) `
    'Saved V26 dwell panel does not keep all five values visible on every page.'

$outputHash = (Get-FileHash -LiteralPath $OutputWorkspace -Algorithm SHA256).Hash
Write-Host 'PASS: V26 Autoplay-clarity workspace build'
Write-Host '  Lighting Functions / fixtures / I-O: unchanged from V25'
Write-Host '  Autoplay seek: ten clipped native CueLists retained'
Write-Host '  Active feedback: 32 full-width rails / 128 raw Chasers retained'
Write-Host '  Dwell UI: five values remain visible on every native QLC+ multipage state'
Write-Host '  Dwell values: 1/2/4/8/16 measures = 4/8/16/32/64 beats'
Write-Host "  Output SHA-256: $outputHash"
