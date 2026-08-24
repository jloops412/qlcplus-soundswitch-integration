[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot '..\..\releases\qlcplus-control-one\v22\IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw'),
    [string]$OutputWorkspace = (Join-Path $PSScriptRoot '..\..\releases\qlcplus-control-one\v23\IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw'),
    [string]$BackupDirectory = (Join-Path $PSScriptRoot 'backups')
)

$ErrorActionPreference = 'Stop'
$namespaceUri = 'http://www.qlcplus.org/Workspace'
$expectedSourceHash = '7AC6ED5413E2C4593B79D414C58C5ADADBAB2C3474A665F7E8FF3631FBA012E7'

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-ColorValue {
    param([string]$Argb)
    return [Convert]::ToUInt32($Argb.TrimStart('#'), 16).ToString()
}

function Get-Widget {
    param([System.Xml.XmlDocument]$Document, [int]$Id)
    return $Document.SelectSingleNode(
        "//*[local-name()='VirtualConsole']//*[@ID='$Id'][*[local-name()='WindowState']]")
}

function Set-Window {
    param(
        [System.Xml.XmlElement]$Widget,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height
    )
    $window = $Widget.SelectSingleNode('./*[local-name()="WindowState"]')
    Assert-Condition ($null -ne $window) "Widget $($Widget.GetAttribute('ID')) has no WindowState."
    $window.SetAttribute('X', $X.ToString())
    $window.SetAttribute('Y', $Y.ToString())
    $window.SetAttribute('Width', $Width.ToString())
    $window.SetAttribute('Height', $Height.ToString())
    $window.SetAttribute('Visible', 'True')
}

function Set-Appearance {
    param(
        [System.Xml.XmlElement]$Widget,
        [string]$Foreground,
        [string]$Background,
        [string]$Font = 'Roboto Condensed,15,-1,5,700,0,0,0,0,0,0,0,0,0,0,1',
        [string]$FrameStyle = 'None'
    )
    $appearance = $Widget.SelectSingleNode('./*[local-name()="Appearance"]')
    if ($null -eq $appearance) {
        $appearance = $Widget.OwnerDocument.CreateElement('Appearance', $namespaceUri)
        foreach ($entry in @(
            @('FrameStyle', 'None'),
            @('ForegroundColor', '4294967295'),
            @('BackgroundColor', '0'),
            @('BackgroundImage', 'None'),
            @('Font', 'Roboto Condensed,15,-1,5,700,0,0,0,0,0,0,0,0,0,0,1')
        )) {
            $node = $Widget.OwnerDocument.CreateElement($entry[0], $namespaceUri)
            $node.InnerText = $entry[1]
            [void]$appearance.AppendChild($node)
        }
        $firstNestedWidget = $Widget.SelectSingleNode('./*[@ID][*[local-name()="WindowState"]]')
        if ($null -ne $firstNestedWidget) {
            [void]$Widget.InsertBefore($appearance, $firstNestedWidget)
        } else {
            [void]$Widget.AppendChild($appearance)
        }
    }
    $appearance.SelectSingleNode('./*[local-name()="FrameStyle"]').InnerText = $FrameStyle
    $appearance.SelectSingleNode('./*[local-name()="ForegroundColor"]').InnerText = (Get-ColorValue $Foreground)
    $appearance.SelectSingleNode('./*[local-name()="BackgroundColor"]').InnerText = (Get-ColorValue $Background)
    $appearance.SelectSingleNode('./*[local-name()="Font"]').InnerText = $Font
}

function Get-BindingSignature {
    param([System.Xml.XmlElement]$Widget)
    $parts = [System.Collections.Generic.List[string]]::new()
    foreach ($function in @($Widget.SelectNodes('./*[local-name()="Function"]'))) {
        $parts.Add("F:$($function.OuterXml)")
    }
    foreach ($input in @($Widget.SelectNodes('./*[local-name()="Input"]'))) {
        $parts.Add("I:$($input.OuterXml)")
    }
    return $parts -join '|'
}

$source = (Resolve-Path -LiteralPath $SourceWorkspace).Path
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Assert-Condition ($sourceHash -eq $expectedSourceHash) `
    "V23 must be built from the published V22 workspace; found $sourceHash."

if (-not (Test-Path -LiteralPath $BackupDirectory)) {
    New-Item -ItemType Directory -Path $BackupDirectory | Out-Null
}
$backupPath = Join-Path $BackupDirectory 'IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw'
if (-not (Test-Path -LiteralPath $backupPath)) {
    Copy-Item -LiteralPath $source -Destination $backupPath
}
Assert-Condition ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -eq $expectedSourceHash) `
    'The V22 safety backup does not match the published source.'

$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $false
$document.Load($source)

$engine = $document.SelectSingleNode('//*[local-name()="Engine"]')
$engineBefore = $engine.OuterXml
$virtualConsole = $document.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$pageOne = $virtualConsole.SelectSingleNode('./*[local-name()="Frame" and @ID="0"]')
Assert-Condition ($null -ne $pageOne) 'Control One Live page not found.'

$sourceWidgetCount = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]')).Count
$bindingBefore = @{}
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $bindingBefore[[int]$widget.GetAttribute('ID')] = Get-BindingSignature $widget
}

# Repair the V22 raw-Chaser outline. The 128 monitor buttons were all stacked
# on the same 32 positions, so an inactive button from a later bank painted
# over the active Chaser. V23 gives each pad a four-dot live rail (Bank 1–4,
# top-to-bottom). The active raw Chaser gets QLC+'s amber Monitoring border,
# including across Auto All bank transitions. The layer remains disabled,
# read-only, and outside the playback SoloFrame.
$outline = Get-Widget $document 1413
Assert-Condition ($null -ne $outline) 'V22 active-loop outline was not found.'
$outlineButtons = @($outline.SelectNodes('./*[local-name()="Button"]'))
Assert-Condition ($outlineButtons.Count -eq 128) 'V22 active-loop outline does not contain 128 monitors.'
$outline.SetAttribute('Caption', 'LIVE PAD TRACKING • MANUAL + AUTO BANK + AUTO ALL')
Set-Window $outline 20 170 1120 318
$outline.WindowState.SetAttribute('Z', '-20')
Set-Appearance $outline '#FFFFFFFF' '#00000000' `
    'Roboto Condensed,11,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'

$liveRailColors = @('#FF1D4ED8', '#FF7C3AED', '#FFB7791F', '#FFB4233A')
for ($index = 0; $index -lt 128; $index++) {
    $button = $outlineButtons[$index]
    $bank = [Math]::Floor($index / 32)
    $pad = $index % 32
    $column = $pad % 4
    $row = [Math]::Floor($pad / 4)
    $button.RemoveAttribute('Page')
    Set-Window $button ($column * 278) ($row * 39 + $bank * 9) 22 8
    Set-Appearance $button '#FFFFFFFF' $liveRailColors[$bank] `
        'Roboto Condensed,11,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

# A single persistent mouse button must own logical channel 811. The two V22
# page-specific copies could echo the same Function feedback and cancel the
# mode change. Keep the original public control Function/channel, remove only
# the duplicate widget, and move the survivor outside the mode-paged frame.
$modeButton = Get-Widget $document 1405
$duplicateModeButton = Get-Widget $document 1406
Assert-Condition ($null -ne $modeButton -and $null -ne $duplicateModeButton) `
    'V22 mode-switch widgets were not found.'
[void]$duplicateModeButton.ParentNode.RemoveChild($duplicateModeButton)
[void]$modeButton.ParentNode.RemoveChild($modeButton)
$modeButton.RemoveAttribute('Page')
$modeButton.SetAttribute('Caption', 'AUTOLOOPS  ⇄  PRIORITY LOOKS')
Set-Window $modeButton 1200 76 380 52
Set-Appearance $modeButton '#FFF8FAFC' '#FF0F766E' `
    'Roboto Condensed,18,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
[void]$pageOne.AppendChild($modeButton)

# Modern dark performance canvas sized for the DJ-PC's 1920×1080 display.
Set-Window $pageOne 0 0 1600 900
Set-Appearance $pageOne '#FFF8FAFC' '#FF0B1220' `
    'Roboto Condensed,16,-1,5,500,0,0,0,0,0,0,0,0,0,0,1'

foreach ($pageId in @(1, 2)) {
    $page = Get-Widget $document $pageId
    Set-Appearance $page '#FFF8FAFC' '#FF0B1220' `
        'Roboto Condensed,16,-1,5,500,0,0,0,0,0,0,0,0,0,0,1'
}

$title = Get-Widget $document 1000
$title.SetAttribute('Caption', "CONTROL ONE  •  LIVE PERFORMANCE`n32 PADS  •  4 AUTOLOOP BANKS  •  PRIORITY LOOKS  •  OVERRIDES  •  LIVE TRACKING")
Set-Window $title 20 10 1560 50
Set-Appearance $title '#FFF8FAFC' '#FF111C2E' `
    'Roboto Condensed,19,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'

# Transport strip.
$transportColors = @{
    1001 = @('#FFFFFFFF', '#FFB4233A')
    1002 = @('#FFFFFFFF', '#FF0F8A5F')
    1003 = @('#FFFFFFFF', '#FF2855A6')
    1004 = @('#FFCBD5E1', '#FF253247')
    1005 = @('#FFCBD5E1', '#FF253247')
    1006 = @('#FFCBD5E1', '#FF253247')
    1007 = @('#FFCBD5E1', '#FF253247')
    1008 = @('#FFFFFFFF', '#FF111111')
    1009 = @('#FF111827', '#FFF1F5F9')
    1010 = @('#FFFFFFFF', '#FF6D28D9')
}
$transportX = @{
    1001 = 20
    1002 = 98
    1003 = 254
    1004 = 332
    1005 = 410
    1006 = 488
    1007 = 566
    1008 = 644
    1009 = 722
    1010 = 800
}
foreach ($id in 1001..1010) {
    $button = Get-Widget $document $id
    Set-Window $button $transportX[$id] 76 72 64
    Set-Appearance $button $transportColors[$id][0] $transportColors[$id][1] `
        'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}
$transportState = Get-Widget $document 1011
Set-Window $transportState 176 76 72 64
foreach ($label in @($transportState.SelectNodes('./*[local-name()="Label"]'))) {
    Set-Window $label 0 0 72 64
}

# Compact five-choice live dwell selector.
$dwellFrame = Get-Widget $document 1367
$dwellFrame.SetAttribute('Caption', 'AUTOPLAY DWELL • MEASURES')
Set-Window $dwellFrame 900 76 280 64
Set-Appearance $dwellFrame '#FFE2E8F0' '#FF111C2E' `
    'Roboto Condensed,12,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
foreach ($button in @($dwellFrame.SelectNodes('./*[local-name()="Button"]'))) {
    $input = $button.SelectSingleNode('./*[local-name()="Input" and @Universe="1"]')
    $choiceIndex = [int]$input.GetAttribute('Channel') - 804
    $measures = @(1, 2, 4, 8, 16)[$choiceIndex]
    $button.SetAttribute('Caption', $measures.ToString())
    Set-Window $button (6 + $choiceIndex * 53) 28 48 31
    $page = if ($button.HasAttribute('Page')) { [int]$button.GetAttribute('Page') } else { 0 }
    $background = if ($page -eq $choiceIndex) { '#FF0F8A5F' } else { '#FF26364C' }
    Set-Appearance $button '#FFF8FAFC' $background `
        'Roboto Condensed,13,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

# Main Control One pad surface.
$padFrame = Get-Widget $document 1045
$padFrame.SetAttribute('Caption', 'CONTROL ONE • PERFORMANCE PADS')
Set-Window $padFrame 20 155 1160 520
Set-Appearance $padFrame '#FFE2E8F0' '#FF111C2E' `
    'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'

$autoloopOwner = Get-Widget $document 1046
$priorityOwner = Get-Widget $document 1349
Set-Window $autoloopOwner 8 30 1144 482
Set-Window $priorityOwner 8 30 1144 482
Set-Appearance $autoloopOwner '#FFE2E8F0' '#FF101A2A'
Set-Appearance $priorityOwner '#FFE2E8F0' '#FF101A2A'

foreach ($labelId in @(1047, 1090, 1133, 1176)) {
    $label = Get-Widget $document $labelId
    $label.SetAttribute('Caption', "$($label.GetAttribute('Caption'))  •  LIVE RAIL B1/B2/B3/B4 — AMBER = CURRENT")
    Set-Window $label 16 30 1112 26
    Set-Appearance $label '#FFF8FAFC' '#FF1B2940' `
        'Roboto Condensed,16,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

$bankFrame = Get-Widget $document 1350
$bankFrame.SetAttribute('Caption', 'BANK SELECT • MATCHES CONTROL ONE')
Set-Window $bankFrame 24 88 780 36
Set-Appearance $bankFrame '#FFCBD5E1' '#FF111C2E' `
    'Roboto Condensed,11,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
$bankColors = @('#FF2855A6', '#FF7C3AED', '#FFB7791F', '#FFB4233A')
foreach ($button in @($bankFrame.SelectNodes('./*[local-name()="Button"]'))) {
    $functionId = [int]$button.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
    $bankIndex = $functionId - 1982
    Set-Window $button ($bankIndex * 193) 0 185 34
    Set-Appearance $button '#FFFFFFFF' $bankColors[$bankIndex] `
        'Roboto Condensed,13,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

$bankLaneIds = @(1057, 1100, 1143, 1186)
$autoFrameIds = @(1052, 1095, 1138, 1181)
for ($bank = 0; $bank -lt 4; $bank++) {
    $autoFrame = Get-Widget $document $autoFrameIds[$bank]
    Set-Window $autoFrame 805 58 330 82
    Set-Appearance $autoFrame '#FFE2E8F0' '#FF162238' `
        'Roboto Condensed,12,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
    foreach ($button in @($autoFrame.SelectNodes('./*[local-name()="Button"]'))) {
        $isAll = $button.GetAttribute('Caption') -match 'ALL'
        $buttonX = if ($isAll) { 169 } else { 6 }
        $buttonColor = if ($isAll) { '#FF0F8A5F' } else { $bankColors[$bank] }
        Set-Window $button $buttonX 31 155 43
        Set-Appearance $button '#FFFFFFFF' $buttonColor `
            'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
    }

    $lane = Get-Widget $document $bankLaneIds[$bank]
    Set-Window $lane 8 132 1128 338
    Set-Appearance $lane '#FFE2E8F0' '#FF0E1726' `
        'Roboto Condensed,11,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
    $buttons = @($lane.SelectNodes('./*[local-name()="Button"]'))
    Assert-Condition ($buttons.Count -eq 32) "Bank $($bank + 1) does not contain 32 pads."
    for ($index = 0; $index -lt 32; $index++) {
        $column = $index % 4
        $row = [Math]::Floor($index / 4)
        Set-Window $buttons[$index] (32 + $column * 278) (12 + $row * 39) 246 35
        $appearance = $buttons[$index].SelectSingleNode('./*[local-name()="Appearance"]')
        $appearance.SelectSingleNode('./*[local-name()="Font"]').InnerText = `
            'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
    }
}

$priorityButtons = @($priorityOwner.SelectNodes('./*[local-name()="Button"]'))
Assert-Condition ($priorityButtons.Count -eq 32) 'Priority surface does not contain 32 pads.'
for ($index = 0; $index -lt 32; $index++) {
    $column = $index % 4
    $row = [Math]::Floor($index / 4)
    Set-Window $priorityButtons[$index] (8 + $column * 278) (32 + $row * 53) 268 46
    $appearance = $priorityButtons[$index].SelectSingleNode('./*[local-name()="Appearance"]')
    $appearance.SelectSingleNode('./*[local-name()="Font"]').InnerText = `
        'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

# Right-side live control rail.
$speedFrame = Get-Widget $document 1407
$speedFrame.SetAttribute('Caption', 'CHASE SPEED • CLICK TO ADVANCE')
Set-Window $speedFrame 1200 136 380 40
Set-Appearance $speedFrame '#FFE2E8F0' '#FF111C2E'
foreach ($button in @($speedFrame.SelectNodes('./*[local-name()="Button"]'))) {
    Set-Window $button 0 0 380 40
    Set-Appearance $button '#FFFFFFFF' '#FF2855A6' `
        'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

$colorHeader = Get-Widget $document 1252
$colorHeader.SetAttribute('Caption', 'COLOR OVERRIDES • PRESS ON / PRESS OFF')
Set-Window $colorHeader 1200 188 380 34
Set-Appearance $colorHeader '#FFE2E8F0' '#FF162238' `
    'Roboto Condensed,13,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
$overrideColors = @(
    @('#FFFFFFFF', '#FFB4233A'), @('#FF111827', '#FFF97316'), @('#FF111827', '#FFFACC15'),
    @('#FFFFFFFF', '#FF169B62'), @('#FF0F172A', '#FF22D3EE'), @('#FFFFFFFF', '#FF2855A6'),
    @('#FFFFFFFF', '#FF7C3AED'), @('#FFFFFFFF', '#FFDB2777'), @('#FFFFFFFF', '#FF475569')
)
for ($index = 0; $index -lt 9; $index++) {
    $button = Get-Widget $document (1253 + $index)
    $column = $index % 3
    $row = [Math]::Floor($index / 3)
    Set-Window $button (1200 + $column * 131) (232 + $row * 78) 118 66
    Set-Appearance $button $overrideColors[$index][0] $overrideColors[$index][1] `
        'Roboto Condensed,14,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

$autoplayState = Get-Widget $document 1262
Set-Window $autoplayState 1200 466 380 50
foreach ($label in @($autoplayState.SelectNodes('./*[local-name()="Label"]'))) {
    Set-Window $label 0 0 380 50
    $appearance = $label.SelectSingleNode('./*[local-name()="Appearance"]')
    $appearance.SelectSingleNode('./*[local-name()="Font"]').InnerText = `
        'Roboto Condensed,15,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
}

$intensityFrame = Get-Widget $document 1270
$intensityFrame.SetAttribute('Caption', 'INTENSITY TARGET • CLICK HEADER ARROW')
Set-Window $intensityFrame 1200 526 380 149
Set-Appearance $intensityFrame '#FFE2E8F0' '#FF111C2E' `
    'Roboto Condensed,12,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
foreach ($label in @($intensityFrame.SelectNodes('./*[local-name()="Label"]'))) {
    Set-Window $label 8 30 364 30
}
foreach ($slider in @($intensityFrame.SelectNodes('./*[local-name()="Slider"]'))) {
    Set-Window $slider 110 66 160 64
}

# Keep a detailed native CueList below the 4x8 pad grid as a second source of
# truth. The repaired live rail follows the actual pad across all banks; this tracker
# supplies the full loop name and progression for Auto Bank/Auto All.
$nowPlaying = Get-Widget $document 1393
$nowPlaying.SetAttribute('Caption', 'AUTOPLAY TRACKER • CURRENT LOOP')
Set-Window $nowPlaying 20 690 1560 190
Set-Appearance $nowPlaying '#FFE2E8F0' '#FF111C2E' `
    'Roboto Condensed,13,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
$showHeader = $nowPlaying.SelectSingleNode('./*[local-name()="ShowHeader"]')
$showHeader.InnerText = 'True'
$manualLabel = Get-Widget $document 1394
$manualLabel.SetAttribute('Caption', "MANUAL LOOP MODE`nTHE LATCHED PAD USES QLC+'S GREEN ACTIVE BORDER • AUTO BANK / AUTO ALL TRACK BELOW")
Set-Window $manualLabel 0 30 1560 154
Set-Appearance $manualLabel '#FFCBD5E1' '#FF162238' `
    'Roboto Condensed,17,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
foreach ($cueList in @($nowPlaying.SelectNodes('./*[local-name()="CueList"]'))) {
    Set-Window $cueList 0 30 1560 154
}

# Subtle visual modernization on the remaining console pages without moving
# their established manual/map controls.
foreach ($pageId in @(1, 2)) {
    $page = Get-Widget $document $pageId
    foreach ($label in @($page.SelectNodes('./*[local-name()="Label"]'))) {
        $window = $label.SelectSingleNode('./*[local-name()="WindowState"]')
        if ([int]$window.GetAttribute('Y') -le 20 -and [int]$window.GetAttribute('Width') -ge 700) {
            Set-Appearance $label '#FFF8FAFC' '#FF111C2E' `
                'Roboto Condensed,18,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
        }
    }
}

# Preserve all lighting/control contracts.
Assert-Condition ($engine.OuterXml -ceq $engineBefore) 'Engine XML changed during the UI-only pass.'
$removedIds = [System.Collections.Generic.HashSet[int]]::new()
[void]$removedIds.Add(1406)
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    Assert-Condition ($bindingBefore.ContainsKey($id)) "Unexpected new widget ID $id."
    Assert-Condition ((Get-BindingSignature $widget) -ceq $bindingBefore[$id]) `
        "Function/Input binding changed on widget $id."
}
foreach ($id in $removedIds) {
    Assert-Condition ($null -eq (Get-Widget $document $id)) "Removed V22 widget $id still exists."
}
$widgetIds = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') | ForEach-Object {
    [int]$_.GetAttribute('ID')
})
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'
Assert-Condition ($widgetIds.Count -eq $sourceWidgetCount - 1) `
    'Unexpected V23 widget-count delta.'
Assert-Condition (@($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input" and @Universe="1" and @Channel="811"]')).Count -gt 0
}).Count -eq 1) 'V23 must have exactly one mouse mode-switch source on logical channel 811.'
Assert-Condition (@($nowPlaying.SelectNodes('./*[local-name()="CueList"]')).Count -eq 10) `
    'Autoplay tracker must retain all ten native parent CueLists.'
Assert-Condition (@($outline.SelectNodes('./*[local-name()="Button"]')).Count -eq 128) `
    'Active-loop outline must retain all 128 raw Chaser monitors.'
for ($bank = 0; $bank -lt 4; $bank++) {
    $firstId = 532 + $bank * 32
    $lastId = $firstId + 31
    $bankMonitors = @($outline.SelectNodes('./*[local-name()="Button"]') | Where-Object {
        $functionId = [int]$_.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
        $functionId -ge $firstId -and $functionId -le $lastId
    })
    Assert-Condition ($bankMonitors.Count -eq 32) `
        "Active-loop outline Bank $($bank + 1) must expose exactly 32 monitors."
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
Assert-Condition ($roundTrip.SelectNodes('//*[local-name()="Engine"]/*[local-name()="Function"]').Count -eq 2090) `
    'V23 Function count changed.'

$outputHash = (Get-FileHash -LiteralPath $OutputWorkspace -Algorithm SHA256).Hash
Write-Host 'PASS: V23 Live Console corrective UI build'
Write-Host '  Engine/fixtures/functions/I-O: byte-equivalent XML preserved'
Write-Host '  Mode switch: one persistent mouse source on logical channel 811'
Write-Host '  Autoplay feedback: 4-dot live rails across all banks + ten native CueLists'
Write-Host "  Output SHA-256: $outputHash"
