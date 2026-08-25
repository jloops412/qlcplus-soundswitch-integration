[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw'),
    [string]$OutputWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw')
)

$ErrorActionPreference = 'Stop'
$namespaceUri = 'http://www.qlcplus.org/Workspace'
$expectedSourceHash = 'E953C3483EB09D2E600D32495887B27A03021DC47EF7BA5797552C4F5A21547B'

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

function Get-ColorValue {
    param([string]$Argb)
    return [Convert]::ToUInt32($Argb.TrimStart('#'), 16).ToString()
}

function Add-TextElement {
    param(
        [System.Xml.XmlElement]$Parent,
        [string]$Name,
        [string]$Value
    )
    $node = $Parent.OwnerDocument.CreateElement($Name, $namespaceUri)
    $node.InnerText = $Value
    [void]$Parent.AppendChild($node)
    return $node
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
        [int]$Height,
        [int]$Z = 0
    )
    $window = $Widget.SelectSingleNode('./*[local-name()="WindowState"]')
    Assert-Condition ($null -ne $window) "Widget $($Widget.GetAttribute('ID')) has no WindowState."
    $window.SetAttribute('X', $X.ToString())
    $window.SetAttribute('Y', $Y.ToString())
    $window.SetAttribute('Width', $Width.ToString())
    $window.SetAttribute('Height', $Height.ToString())
    $window.SetAttribute('Z', $Z.ToString())
    $window.SetAttribute('Visible', 'True')
}

function Set-Appearance {
    param(
        [System.Xml.XmlElement]$Widget,
        [string]$Foreground,
        [string]$Background,
        [string]$Font = 'Roboto Condensed,11,-1,5,700,0,0,0,0,0,0,0,0,0,0,1'
    )
    $appearance = $Widget.SelectSingleNode('./*[local-name()="Appearance"]')
    Assert-Condition ($null -ne $appearance) "Widget $($Widget.GetAttribute('ID')) has no Appearance."
    $appearance.SelectSingleNode('./*[local-name()="FrameStyle"]').InnerText = 'None'
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

function New-MonitorFrame {
    param(
        [System.Xml.XmlDocument]$Document,
        [int]$Id,
        [int]$Pad,
        [int]$X,
        [int]$Y
    )
    $frame = $Document.CreateElement('Frame', $namespaceUri)
    $frame.SetAttribute('Caption', "LIVE LOOP $($Pad + 1)")
    $frame.SetAttribute('ID', $Id.ToString())

    $window = $Document.CreateElement('WindowState', $namespaceUri)
    $window.SetAttribute('Visible', 'True')
    $window.SetAttribute('X', $X.ToString())
    $window.SetAttribute('Y', $Y.ToString())
    $window.SetAttribute('Width', '26')
    $window.SetAttribute('Height', '35')
    $window.SetAttribute('Z', '50')
    [void]$frame.AppendChild($window)

    [void](Add-TextElement $frame 'AllowResize' 'False')
    [void](Add-TextElement $frame 'ShowHeader' 'False')
    [void](Add-TextElement $frame 'ShowEnableButton' 'False')
    [void](Add-TextElement $frame 'Collapsed' 'False')
    [void](Add-TextElement $frame 'Disabled' 'True')

    $appearance = $Document.CreateElement('Appearance', $namespaceUri)
    foreach ($entry in @(
        @('FrameStyle', 'None'),
        @('ForegroundColor', (Get-ColorValue '#FFFFFFFF')),
        @('BackgroundColor', (Get-ColorValue '#FF05080D')),
        @('BackgroundImage', 'None'),
        @('Font', 'Roboto Condensed,8,-1,5,700,0,0,0,0,0,0,0,0,0,0,1')
    )) {
        [void](Add-TextElement $appearance $entry[0] $entry[1])
    }
    [void]$frame.AppendChild($appearance)
    return $frame
}

$source = (Resolve-Path -LiteralPath $SourceWorkspace).Path
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Assert-Condition ($sourceHash -eq $expectedSourceHash) `
    "V24 must be built from published V23; found $sourceHash."

$backupDirectory = Join-Path $PSScriptRoot 'backups'
if (-not (Test-Path -LiteralPath $backupDirectory)) {
    New-Item -ItemType Directory -Path $backupDirectory | Out-Null
}
$backupPath = Join-Path $backupDirectory 'IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw'
if (-not (Test-Path -LiteralPath $backupPath)) {
    Copy-Item -LiteralPath $source -Destination $backupPath
}
Assert-Condition ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -eq $expectedSourceHash) `
    'The V23 safety backup does not match the published source.'

$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $false
$document.Load($source)

$engine = $document.SelectSingleNode('//*[local-name()="Engine"]')
$engineBefore = $engine.OuterXml
$controlUniverse = $engine.SelectSingleNode(
    './/*[local-name()="InputOutputMap"]/*[local-name()="Universe" and @ID="1"]')
Assert-Condition ($null -ne $controlUniverse) 'Control One universe 1 is missing.'
$feedbackPatches = @($controlUniverse.SelectNodes('./*[local-name()="Feedback"]'))
Assert-Condition ($feedbackPatches.Count -eq 2) `
    'Published V23 must contain the two feedback declarations being repaired.'
$surfaceFeedback = $feedbackPatches | Where-Object {
    $_.GetAttribute('UID') -eq 'soundswitch:controlone:surface'
}
$priorityFeedback = $feedbackPatches | Where-Object {
    $_.GetAttribute('UID') -eq 'soundswitch:priority-layer'
}
Assert-Condition ($null -ne $surfaceFeedback -and $null -ne $priorityFeedback) `
    'The expected Surface and Priority feedback declarations are missing.'

# QLC+ supports one feedback destination per universe. V23 declared Surface
# commands and Priority ownership as two separate patches on Universe 1, so
# the later Priority declaration silently replaced Surface and broke every
# mouse-driven console command. V24 uses one unified Surface feedback line;
# the plug-in consumes Priority channels 600-631 on that same line while the
# separate Priority output continues to buffer the overlay DMX frame.
[void]$controlUniverse.RemoveChild($priorityFeedback)
$engineAfterFeedbackFix = $engine.OuterXml
Assert-Condition ($engineAfterFeedbackFix -cne $engineBefore) `
    'The duplicate feedback repair did not change the Engine XML.'
$virtualConsole = $document.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$performanceFrame = Get-Widget $document 1045
$oldMonitorFrame = Get-Widget $document 1413
Assert-Condition ($null -ne $performanceFrame) 'Performance-pad frame 1045 is missing.'
Assert-Condition ($null -ne $oldMonitorFrame) 'V23 monitor frame 1413 is missing.'

$bindingBefore = @{}
$sourceWidgetIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    [void]$sourceWidgetIds.Add($id)
    $bindingBefore[$id] = Get-BindingSignature $widget
}

$monitorButtons = @($oldMonitorFrame.SelectNodes('./*[local-name()="Button"]'))
Assert-Condition ($monitorButtons.Count -eq 128) 'V23 does not contain 128 raw-Chaser monitors.'
$byFunction = @{}
foreach ($button in $monitorButtons) {
    $functionId = [int]$button.SelectSingleNode('./*[local-name()="Function"]').GetAttribute('ID')
    Assert-Condition ($functionId -ge 532 -and $functionId -le 659) `
        "Unexpected monitor Function $functionId."
    Assert-Condition (-not $byFunction.ContainsKey($functionId)) `
        "Raw Chaser $functionId is monitored more than once."
    $byFunction[$functionId] = $button
}

# V23 placed one large disabled monitor frame behind opaque bank frames. QLC+
# correctly tracked Monitoring state, but Windows could not paint the rails.
# V24 uses 32 small disabled frames above the pad backgrounds. Each contains
# four non-overlapping, read-only bank indicators and remains visible in both
# Autoloop and Priority Look modes while the underlying owner keeps advancing.
$railColors = @('#FF2563EB', '#FF8B5CF6', '#FFF59E0B', '#FFEF4444')
for ($pad = 0; $pad -lt 32; $pad++) {
    $column = $pad % 4
    $row = [Math]::Floor($pad / 4)
    $frame = New-MonitorFrame $document (1542 + $pad) $pad `
        (20 + $column * 278) (170 + $row * 39)

    for ($bank = 0; $bank -lt 4; $bank++) {
        $functionId = 532 + $bank * 32 + $pad
        $button = $byFunction[$functionId]
        $button.RemoveAttribute('Page')
        Set-Window $button 0 ($bank * 9) 26 8 1
        Set-Appearance $button '#FFFFFFFF' $railColors[$bank]
        [void]$frame.AppendChild($button)
    }
    [void]$performanceFrame.AppendChild($frame)
}
[void]$oldMonitorFrame.ParentNode.RemoveChild($oldMonitorFrame)

# Keep the correction explicit to the operator without changing the public
# Function/Input contract used by Control One and the plug-in.
$modeButton = Get-Widget $document 1405
Assert-Condition ($null -ne $modeButton) 'Persistent mode button 1405 is missing.'
$modeButton.SetAttribute('Caption', 'AUTOLOOPS  ⇄  PRIORITY LOOKS')

Assert-Condition ($engine.OuterXml -ceq $engineAfterFeedbackFix) `
    'Engine XML changed beyond the single unified-feedback repair.'
Assert-Condition (@($controlUniverse.SelectNodes('./*[local-name()="Feedback"]')).Count -eq 1) `
    'Control One universe must contain exactly one feedback patch.'
Assert-Condition ($controlUniverse.SelectSingleNode(
    './*[local-name()="Feedback" and @UID="soundswitch:controlone:surface"]') -ne $null) `
    'The unified Surface feedback patch is missing.'
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    if ($sourceWidgetIds.Contains($id)) {
        Assert-Condition ((Get-BindingSignature $widget) -ceq $bindingBefore[$id]) `
            "Function/Input binding changed on existing widget $id."
    } else {
        Assert-Condition ($id -ge 1542 -and $id -le 1573 -and $widget.LocalName -eq 'Frame') `
            "Unexpected new widget $($widget.LocalName) $id."
        Assert-Condition ((Get-BindingSignature $widget) -eq '') `
            "New monitor frame $id unexpectedly owns a Function/Input."
    }
}

$widgetIds = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') | ForEach-Object {
    [int]$_.GetAttribute('ID')
})
Assert-Condition (@($widgetIds | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'
Assert-Condition ($null -eq (Get-Widget $document 1413)) 'The hidden V23 monitor frame still exists.'
Assert-Condition (@($widgetIds | Where-Object { $_ -ge 1542 -and $_ -le 1573 }).Count -eq 32) `
    'V24 must contain exactly 32 visible mini monitor frames.'
Assert-Condition (@($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    $function = $_.SelectSingleNode('./*[local-name()="Function"]')
    $null -ne $function -and [uint64]$function.GetAttribute('ID') -ge 532 -and
        [uint64]$function.GetAttribute('ID') -le 659 -and
        @($_.SelectNodes('./*[local-name()="Input"]')).Count -eq 0
}).Count -eq 128) 'V24 raw-Chaser monitor coverage is incomplete.'
Assert-Condition (@($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    @($_.SelectNodes('./*[local-name()="Input" and @Universe="1" and @Channel="811"]')).Count -gt 0
}).Count -eq 1) 'V24 must retain one mode command source on logical channel 811.'

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
    'V24 Function count changed.'

$outputHash = (Get-FileHash -LiteralPath $OutputWorkspace -Algorithm SHA256).Hash
Write-Host 'PASS: V24 runtime-feedback workspace build'
Write-Host '  Fixtures/functions/I-O: preserved; duplicate feedback patch unified'
Write-Host '  Active loop: 32 visible read-only B1/B2/B3/B4 mini rails'
Write-Host '  Mode command: Function 1993 / logical channel 811 preserved'
Write-Host "  Output SHA-256: $outputHash"
