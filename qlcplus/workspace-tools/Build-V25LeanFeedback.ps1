[CmdletBinding()]
param(
    [string]$SourceWorkspace = (Join-Path $PSScriptRoot '..\..\releases\qlcplus-control-one\v24\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'),
    [string]$OutputWorkspace = (Join-Path $PSScriptRoot 'IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw')
)

$ErrorActionPreference = 'Stop'
$expectedSourceHash = 'DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF'
$expectedAutoplayParents = 788..797
$expectedRawLoops = 532..659

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
    $window.SetAttribute('Z', $Z.ToString())
}

$source = (Resolve-Path -LiteralPath $SourceWorkspace).Path
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
Assert-Condition ($sourceHash -eq $expectedSourceHash) `
    "V25 must be built from published V24; found $sourceHash."

$backupDirectory = Join-Path $PSScriptRoot 'backups'
if (-not (Test-Path -LiteralPath $backupDirectory)) {
    New-Item -ItemType Directory -Path $backupDirectory | Out-Null
}
$backupPath = Join-Path $backupDirectory 'IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'
if (-not (Test-Path -LiteralPath $backupPath)) {
    Copy-Item -LiteralPath $source -Destination $backupPath
}
Assert-Condition ((Get-FileHash -LiteralPath $backupPath -Algorithm SHA256).Hash -eq $expectedSourceHash) `
    'The protected V24 backup does not match the published source.'

$document = [System.Xml.XmlDocument]::new()
$document.PreserveWhitespace = $false
$document.Load($source)

$engine = $document.SelectSingleNode('//*[local-name()="Engine"]')
$engineBefore = $engine.OuterXml
$virtualConsole = $document.SelectSingleNode('//*[local-name()="VirtualConsole"]')
$tracker = Get-Widget $document 1393
$manualTrackerLabel = Get-Widget $document 1394
$header = Get-Widget $document 1000

Assert-Condition ($null -ne $tracker -and
    $tracker.GetAttribute('Caption') -eq 'AUTOPLAY TRACKER • CURRENT LOOP') `
    'The published V24 tracker frame was not found.'
Assert-Condition ($null -ne $manualTrackerLabel -and $manualTrackerLabel.LocalName -eq 'Label') `
    'The V24 manual tracker label was not found.'
Assert-Condition ($null -ne $header -and $header.LocalName -eq 'Label') `
    'The V24 Live-page header was not found.'

$widgetBindingsBefore = @{}
$widgetIdsBefore = [System.Collections.Generic.HashSet[int]]::new()
foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    [void]$widgetIdsBefore.Add($id)
    $widgetBindingsBefore[$id] = Get-BindingSignature $widget
}

$cueLists = @($tracker.SelectNodes('./*[local-name()="CueList"]'))
Assert-Condition ($cueLists.Count -eq 10) 'V24 must contain ten Autoplay seek CueLists.'
$parentIds = @($cueLists | ForEach-Object {
    [int]$_.SelectSingleNode('./*[local-name()="Chaser"]').InnerText
})
Assert-Condition ((Compare-Object $expectedAutoplayParents $parentIds).Count -eq 0) `
    'The ten Autoplay seek CueLists do not cover parents 788-797 exactly once.'

foreach ($cueList in $cueLists) {
    $seekInput = $cueList.SelectSingleNode(
        './*[local-name()="CrossLeft"]/*[local-name()="Input" and @Universe="1" and @Channel="632"]')
    Assert-Condition ($null -ne $seekInput) `
        "CueList $($cueList.GetAttribute('ID')) lost its absolute-seek input."
}

# The large tracker was visual duplication. Keep its ten CueLists only as a
# clipped 1x1 native input engine: QLC+ still owns the channel-632 absolute
# seek behavior, while the Live page no longer paints a 1560x154 step list.
$tracker.SetAttribute('Caption', 'AUTOPLAY SEEK ENGINE • HIDDEN')
Set-WindowGeometry $tracker 1599 899 1 1 -100
$tracker.SelectSingleNode('./*[local-name()="ShowHeader"]').InnerText = 'False'
$tracker.SelectSingleNode('./*[local-name()="ShowEnableButton"]').InnerText = 'False'
$tracker.SelectSingleNode('./*[local-name()="Collapsed"]').InnerText = 'False'
$tracker.SelectSingleNode('./*[local-name()="Disabled"]').InnerText = 'False'

foreach ($cueList in $cueLists) {
    Set-WindowGeometry $cueList 0 0 1 1 -100
}

[void]$manualTrackerLabel.ParentNode.RemoveChild($manualTrackerLabel)
$header.SetAttribute('Caption',
    "CONTROL ONE  •  LIVE PERFORMANCE`n32 PADS  •  4 AUTOLOOP BANKS  •  PRIORITY LOOKS  •  ACTIVE PAD/BANK FEEDBACK")

Assert-Condition ($engine.OuterXml -ceq $engineBefore) `
    'Engine XML changed during this UI/control-only pass.'

$widgetIdsAfter = @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]') | ForEach-Object {
    [int]$_.GetAttribute('ID')
})
Assert-Condition (@($widgetIdsAfter | Group-Object | Where-Object Count -gt 1).Count -eq 0) `
    'Duplicate Virtual Console widget IDs exist.'
Assert-Condition ($null -eq (Get-Widget $document 1394)) `
    'The visible manual tracker label still exists.'
Assert-Condition ($widgetIdsAfter.Count -eq ($widgetIdsBefore.Count - 1)) `
    'V25 must remove exactly one display-only widget.'
Assert-Condition (@($widgetIdsBefore | Where-Object { $_ -ne 1394 -and $_ -notin $widgetIdsAfter }).Count -eq 0) `
    'V25 unexpectedly removed another Virtual Console widget.'

foreach ($widget in @($virtualConsole.SelectNodes('.//*[@ID][*[local-name()="WindowState"]]'))) {
    $id = [int]$widget.GetAttribute('ID')
    if ($widgetBindingsBefore.ContainsKey($id)) {
        Assert-Condition ((Get-BindingSignature $widget) -ceq $widgetBindingsBefore[$id]) `
            "Function/Input binding changed on widget $id."
    }
}

$trackerAfter = Get-Widget $document 1393
$trackerWindow = $trackerAfter.SelectSingleNode('./*[local-name()="WindowState"]')
Assert-Condition ($trackerWindow.GetAttribute('Width') -eq '1' -and
    $trackerWindow.GetAttribute('Height') -eq '1' -and
    $trackerAfter.SelectSingleNode('./*[local-name()="ShowHeader"]').InnerText -eq 'False') `
    'The Autoplay seek engine is not fully clipped.'
Assert-Condition (@($trackerAfter.SelectNodes('./*[local-name()="CueList"]')).Count -eq 10) `
    'V25 lost an Autoplay seek CueList.'
Assert-Condition (@($document.SelectNodes(
    '//*[local-name()="Input" and @Universe="1" and @Channel="632"]')).Count -eq 10) `
    'V25 must retain all ten absolute-seek bindings.'

$monitorButtons = @($virtualConsole.SelectNodes('.//*[local-name()="Button"]') | Where-Object {
    $function = $_.SelectSingleNode('./*[local-name()="Function"]')
    $null -ne $function -and [uint64]$function.GetAttribute('ID') -in $expectedRawLoops -and
        @($_.SelectNodes('./*[local-name()="Input"]')).Count -eq 0
})
Assert-Condition ($monitorButtons.Count -eq 128) `
    'V25 must retain all 128 read-only active-loop indicators.'
Assert-Condition (@($virtualConsole.SelectNodes(
    './/*[local-name()="Frame" and number(@ID) >= 1542 and number(@ID) <= 1573]')).Count -eq 32) `
    'V25 must retain all 32 active pad/bank rail frames.'

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
    'V25 Function count changed.'
$savedTrackerWindow = $roundTrip.SelectSingleNode(
    '//*[local-name()="Frame" and @ID="1393"]/*[local-name()="WindowState"]')
Assert-Condition ($savedTrackerWindow.GetAttribute('Width') -eq '1') `
    'The saved V25 seek engine is visible.'

$outputHash = (Get-FileHash -LiteralPath $OutputWorkspace -Algorithm SHA256).Hash
Write-Host 'PASS: V25 lean-feedback workspace build'
Write-Host '  Lighting Functions / fixtures / I-O: unchanged from V24'
Write-Host '  Visible tracker: removed'
Write-Host '  Native absolute seek: ten clipped CueLists retained on channel 632'
Write-Host '  Active feedback: 32 pad rails / 128 raw Chasers retained'
Write-Host "  Output SHA-256: $outputHash"
