[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$HostWorkspace,

    [Parameter(Mandatory)]
    [string]$CreativeWorkspace,

    [Parameter(Mandatory)]
    [string]$OutputWorkspace
)

$ErrorActionPreference = 'Stop'

$namespaceUri = 'http://www.qlcplus.org/Workspace'
$rawLoopIds = 532..659
$priorityLookIds = 5..36
$expectedVarietyRoots = @(
    568, 572, 573, 574, 575, 576, 580, 581, 583, 587, 589,
    590, 593, 632, 633, 635, 636, 637, 645, 647, 657, 658
)

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Open-Workspace {
    param([string]$Path)

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $document = [System.Xml.XmlDocument]::new()
    $document.PreserveWhitespace = $false
    $document.Load($resolved)
    $namespaces = [System.Xml.XmlNamespaceManager]::new($document.NameTable)
    $namespaces.AddNamespace('q', $namespaceUri)
    return @{
        Path = $resolved
        Document = $document
        Namespaces = $namespaces
    }
}

function Get-FunctionMap {
    param($Workspace)

    $map = @{}
    foreach ($function in $Workspace.Document.SelectNodes(
            '//q:Engine/q:Function', $Workspace.Namespaces)) {
        $id = [int]$function.GetAttribute('ID')
        Assert-Condition (-not $map.ContainsKey($id)) "Duplicate Function ID $id"
        $map[$id] = $function
    }
    return $map
}

function New-WorkspaceElement {
    param(
        [System.Xml.XmlDocument]$Document,
        [string]$Name,
        [hashtable]$Attributes = @{},
        [AllowNull()][string]$Text
    )

    $element = $Document.CreateElement($Name, $namespaceUri)
    foreach ($key in $Attributes.Keys) {
        $element.SetAttribute([string]$key, [string]$Attributes[$key])
    }
    if ($PSBoundParameters.ContainsKey('Text')) {
        $element.InnerText = $Text
    }
    return $element
}

function Add-TextElement {
    param(
        [System.Xml.XmlElement]$Parent,
        [string]$Name,
        [string]$Text,
        [hashtable]$Attributes = @{}
    )

    $child = New-WorkspaceElement -Document $Parent.OwnerDocument `
        -Name $Name -Attributes $Attributes -Text $Text
    [void]$Parent.AppendChild($child)
    return $child
}

function Add-Appearance {
    param(
        [System.Xml.XmlElement]$Parent,
        [string]$Background = '0',
        [string]$Foreground = '4294967295',
        [int]$FontSize = 12
    )

    $appearance = New-WorkspaceElement -Document $Parent.OwnerDocument `
        -Name 'Appearance'
    [void]$Parent.AppendChild($appearance)
    [void](Add-TextElement -Parent $appearance -Name 'FrameStyle' -Text 'None')
    [void](Add-TextElement -Parent $appearance -Name 'ForegroundColor' -Text $Foreground)
    [void](Add-TextElement -Parent $appearance -Name 'BackgroundColor' -Text $Background)
    [void](Add-TextElement -Parent $appearance -Name 'BackgroundImage' -Text 'None')
    [void](Add-TextElement -Parent $appearance -Name 'Font' -Text `
        "Roboto Condensed,$FontSize,-1,5,700,0,0,0,0,0,0,0,0,0,0,1")
}

function Get-NextWidgetId {
    param($Workspace)

    $ids = [System.Collections.Generic.List[uint64]]::new()
    foreach ($node in $Workspace.Document.SelectNodes(
            '//q:VirtualConsole//*[@ID]', $Workspace.Namespaces)) {
        if ($null -ne $node.SelectSingleNode('q:WindowState', $Workspace.Namespaces)) {
            $ids.Add([uint64]$node.GetAttribute('ID'))
        }
    }
    Assert-Condition ($ids.Count -gt 0) 'No Virtual Console widget IDs found.'
    return ([uint64](($ids | Measure-Object -Maximum).Maximum) + 1)
}

function Save-Workspace {
    param(
        [System.Xml.XmlDocument]$Document,
        [string]$Path
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $directory = [System.IO.Path]::GetDirectoryName($fullPath)
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        [void](New-Item -ItemType Directory -Path $directory -Force)
    }

    $settings = [System.Xml.XmlWriterSettings]::new()
    $settings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $settings.Indent = $true
    $settings.IndentChars = '  '
    $settings.NewLineChars = "`n"
    $settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace
    $settings.OmitXmlDeclaration = $false

    $writer = [System.Xml.XmlWriter]::Create($fullPath, $settings)
    try {
        $Document.Save($writer)
    } finally {
        $writer.Dispose()
    }
}

$destination = Open-Workspace -Path $HostWorkspace
$creative = Open-Workspace -Path $CreativeWorkspace
$hostFunctions = Get-FunctionMap -Workspace $destination
$creativeFunctions = Get-FunctionMap -Workspace $creative

foreach ($id in $rawLoopIds + $priorityLookIds) {
    Assert-Condition $hostFunctions.ContainsKey($id) "Host Function $id is missing."
    Assert-Condition $creativeFunctions.ContainsKey($id) "Creative Function $id is missing."
}

$actualChangedRoots = @($rawLoopIds | Where-Object {
    $hostFunctions[$_].OuterXml -cne $creativeFunctions[$_].OuterXml
})
Assert-Condition (
    (Compare-Object $expectedVarietyRoots $actualChangedRoots).Count -eq 0
) "Unexpected creative root delta: $($actualChangedRoots -join ', ')"

foreach ($id in $priorityLookIds) {
    Assert-Condition (
        $hostFunctions[$id].GetAttribute('Name') -ceq
            $creativeFunctions[$id].GetAttribute('Name')
    ) "Priority Look $id changed name in the creative donor."
    Assert-Condition (
        $hostFunctions[$id].GetAttribute('Type') -ceq
            $creativeFunctions[$id].GetAttribute('Type')
    ) "Priority Look $id changed type in the creative donor."
}

$creativeHelperIds = [System.Collections.Generic.HashSet[int]]::new()
foreach ($rootId in $expectedVarietyRoots) {
    $root = $creativeFunctions[$rootId]
    Assert-Condition ($root.GetAttribute('Type') -ceq 'Chaser') `
        "Creative root $rootId is not a Chaser."
    $steps = @($root.SelectNodes('q:Step', $creative.Namespaces))
    Assert-Condition ($steps.Count -eq 8) `
        "Creative root $rootId does not contain eight steps."
    foreach ($step in $steps) {
        $helperId = [int]$step.InnerText.Trim()
        Assert-Condition $creativeFunctions.ContainsKey($helperId) `
            "Creative root $rootId references missing helper $helperId."
        $helper = $creativeFunctions[$helperId]
        Assert-Condition ($helper.GetAttribute('Type') -ceq 'Scene') `
            "Creative helper $helperId is not a Scene."
        Assert-Condition ($helper.GetAttribute('Name').StartsWith(
            'HELPER - VARIETY PRO ', [System.StringComparison]::Ordinal)) `
            "Creative helper $helperId is not part of Variety Pro."
        [void]$creativeHelperIds.Add($helperId)
    }
}
Assert-Condition ($creativeHelperIds.Count -eq 176) `
    "Expected 176 Variety Pro helper Scenes; found $($creativeHelperIds.Count)."

$allReservedIds = @($hostFunctions.Keys + $creativeHelperIds | Sort-Object -Unique)
$nextFunctionId = [int](($allReservedIds | Measure-Object -Maximum).Maximum) + 1
$helperIdMap = @{}
$collisionMap = @{}
foreach ($sourceId in @($creativeHelperIds | Sort-Object)) {
    if (-not $hostFunctions.ContainsKey($sourceId)) {
        $helperIdMap[$sourceId] = $sourceId
        continue
    }

    while ($hostFunctions.ContainsKey($nextFunctionId) -or
           $creativeHelperIds.Contains($nextFunctionId) -or
           $helperIdMap.Values -contains $nextFunctionId) {
        ++$nextFunctionId
    }
    $helperIdMap[$sourceId] = $nextFunctionId
    $collisionMap[$sourceId] = $nextFunctionId
    ++$nextFunctionId
}

$engine = $destination.Document.SelectSingleNode('//q:Engine', $destination.Namespaces)
Assert-Condition ($null -ne $engine) 'Host Engine is missing.'

foreach ($rootId in $expectedVarietyRoots) {
    $replacement = $destination.Document.ImportNode($creativeFunctions[$rootId], $true)
    foreach ($step in $replacement.SelectNodes('q:Step', $destination.Namespaces)) {
        $sourceHelperId = [int]$step.InnerText.Trim()
        $step.InnerText = [string]$helperIdMap[$sourceHelperId]
    }
    [void]$engine.ReplaceChild($replacement, $hostFunctions[$rootId])
    $hostFunctions[$rootId] = $replacement
}

$lastFunction = @($engine.SelectNodes('q:Function', $destination.Namespaces))[-1]
$insertBefore = $lastFunction.NextSibling
foreach ($sourceId in @($creativeHelperIds | Sort-Object)) {
    $copy = $destination.Document.ImportNode($creativeFunctions[$sourceId], $true)
    $copy.SetAttribute('ID', [string]$helperIdMap[$sourceId])
    if ($null -eq $insertBefore) {
        [void]$engine.AppendChild($copy)
    } else {
        [void]$engine.InsertBefore($copy, $insertBefore)
    }
    $hostFunctions[[int]$helperIdMap[$sourceId]] = $copy
}

$ownerToRaw = @{}
foreach ($function in $engine.SelectNodes('q:Function', $destination.Namespaces)) {
    if (-not $function.GetAttribute('Name').StartsWith(
            'MANUAL LOOP [', [System.StringComparison]::Ordinal)) {
        continue
    }
    $steps = @($function.SelectNodes('q:Step', $destination.Namespaces))
    Assert-Condition ($steps.Count -eq 1) `
        "Manual owner $($function.GetAttribute('ID')) is not one-child."
    $ownerId = [int]$function.GetAttribute('ID')
    $rawId = [int]$steps[0].InnerText.Trim()
    Assert-Condition ($rawLoopIds -contains $rawId) `
        "Manual owner $ownerId references unexpected raw Function $rawId."
    $function.SetAttribute('Name', $hostFunctions[$rawId].GetAttribute('Name').Replace(
        'AUTOLOOP', 'MANUAL LOOP'))
    $ownerToRaw[$ownerId] = $rawId
}
Assert-Condition ($ownerToRaw.Count -eq 128) `
    "Expected 128 manual owners; found $($ownerToRaw.Count)."

$manualButtons = [System.Collections.Generic.List[System.Xml.XmlElement]]::new()
foreach ($button in $destination.Document.SelectNodes(
        '//q:VirtualConsole//q:Button', $destination.Namespaces)) {
    $target = $button.SelectSingleNode('q:Function', $destination.Namespaces)
    if ($null -eq $target) {
        continue
    }
    $targetId = [uint64]$target.GetAttribute('ID')
    if ($targetId -le [uint64][int]::MaxValue -and
        $ownerToRaw.ContainsKey([int]$targetId)) {
        $manualButtons.Add($button)
    }
}
Assert-Condition ($manualButtons.Count -eq 128) `
    "Expected 128 manual pad buttons; found $($manualButtons.Count)."

foreach ($button in $manualButtons) {
    $ownerId = [int]$button.SelectSingleNode(
        'q:Function', $destination.Namespaces).GetAttribute('ID')
    $rawId = $ownerToRaw[$ownerId]
    $rawName = $hostFunctions[$rawId].GetAttribute('Name')
    Assert-Condition ($rawName -match '^AUTOLOOP \[[^]]+\] - (.+)$') `
        "Unexpected raw Autoloop name: $rawName"
    $pad = (($rawId - 532) % 32) + 1
    $button.SetAttribute('Caption', ('{0:d2}  {1}' -f $pad, $Matches[1]))
}

$virtualConsole = $destination.Document.SelectSingleNode(
    '//q:VirtualConsole', $destination.Namespaces)
$modeFrame = $null
foreach ($frame in $virtualConsole.SelectNodes('.//q:Frame', $destination.Namespaces)) {
    if ($frame.GetAttribute('Caption') -ceq
        'CONTROL ONE • 32 PERFORMANCE PADS • CURRENT MODE') {
        $modeFrame = $frame
        break
    }
}
Assert-Condition ($null -ne $modeFrame) 'Control One performance mode frame is missing.'
Assert-Condition ($null -eq $modeFrame.SelectSingleNode(
    'q:Frame[starts-with(@Caption,"ACTIVE LOOP OUTLINE")]', $destination.Namespaces)) `
    'An active-loop outline layer already exists.'

$autoloopFrame = $null
foreach ($frame in $modeFrame.SelectNodes('q:SoloFrame', $destination.Namespaces)) {
    if ($frame.GetAttribute('Page') -ceq '0' -and
        $frame.GetAttribute('Caption').StartsWith(
            'AUTO LOOP MODE', [System.StringComparison]::Ordinal)) {
        $autoloopFrame = $frame
        break
    }
}
Assert-Condition ($null -ne $autoloopFrame) 'Autoloop owner SoloFrame is missing.'

$nextWidgetId = Get-NextWidgetId -Workspace $destination
$outlineFrame = New-WorkspaceElement -Document $destination.Document -Name 'Frame' `
    -Attributes @{
        Caption = 'ACTIVE LOOP OUTLINE • MANUAL + AUTO BANK + AUTO ALL'
        ID = [string]$nextWidgetId
        Page = '0'
    }
++ $nextWidgetId
[void]$outlineFrame.AppendChild((New-WorkspaceElement -Document $destination.Document `
    -Name 'WindowState' -Attributes @{
        Visible = 'True'; X = '12'; Y = '152'; Width = '1071'; Height = '341'; Z = '-20'
    }))
[void](Add-TextElement -Parent $outlineFrame -Name 'AllowResize' -Text 'False')
[void](Add-TextElement -Parent $outlineFrame -Name 'ShowHeader' -Text 'False')
[void](Add-TextElement -Parent $outlineFrame -Name 'ShowEnableButton' -Text 'False')
[void](Add-TextElement -Parent $outlineFrame -Name 'Collapsed' -Text 'False')
[void](Add-TextElement -Parent $outlineFrame -Name 'Disabled' -Text 'True')
Add-Appearance -Parent $outlineFrame -Background '0'

$buttonsByRaw = @{}
foreach ($button in $manualButtons) {
    $ownerId = [int]$button.SelectSingleNode(
        'q:Function', $destination.Namespaces).GetAttribute('ID')
    $rawId = $ownerToRaw[$ownerId]
    $position = $button.SelectSingleNode('q:WindowState', $destination.Namespaces)
    Assert-Condition ($null -ne $position) "Manual button for raw $rawId has no WindowState."
    $monitor = New-WorkspaceElement -Document $destination.Document -Name 'Button' `
        -Attributes @{ Caption = ''; ID = [string]$nextWidgetId }
    ++$nextWidgetId
    [void]$monitor.AppendChild((New-WorkspaceElement -Document $destination.Document `
        -Name 'Function' -Attributes @{ ID = [string]$rawId }))
    [void](Add-TextElement -Parent $monitor -Name 'Action' -Text 'Toggle')
    [void](Add-TextElement -Parent $monitor -Name 'Intensity' -Text '100' `
        -Attributes @{ Adjust = 'False' })
    [void]$monitor.AppendChild((New-WorkspaceElement -Document $destination.Document `
        -Name 'WindowState' -Attributes @{
            Visible = 'True'
            X = [string][int]$position.GetAttribute('X')
            Y = [string][int]$position.GetAttribute('Y')
            Width = [string]([int]$position.GetAttribute('Width') + 8)
            Height = [string]([int]$position.GetAttribute('Height') + 8)
        }))
    Add-Appearance -Parent $monitor -Background '0'
    [void]$outlineFrame.AppendChild($monitor)
    $buttonsByRaw[$rawId] = $monitor
}
Assert-Condition ($buttonsByRaw.Count -eq 128) `
    "Expected one active-loop monitor per raw Chaser; found $($buttonsByRaw.Count)."

$insertIndex = 0
for ($index = 0; $index -lt $modeFrame.ChildNodes.Count; ++$index) {
    if ($modeFrame.ChildNodes[$index] -eq $autoloopFrame) {
        $insertIndex = $index
        break
    }
}
[void]$modeFrame.InsertBefore($outlineFrame, $autoloopFrame)

$hostFixtureXml = @($destination.Document.SelectNodes(
    '//q:Engine/q:Fixture', $destination.Namespaces) | ForEach-Object OuterXml)
$hostIoXml = $destination.Document.SelectSingleNode(
    '//q:Engine/q:InputOutputMap', $destination.Namespaces).OuterXml
$hostPriorityXml = @($priorityLookIds | ForEach-Object {
    $hostFunctions[$_].OuterXml
})

Save-Workspace -Document $destination.Document -Path $OutputWorkspace

$result = Open-Workspace -Path $OutputWorkspace
$resultFunctions = Get-FunctionMap -Workspace $result
$resultFixtures = @($result.Document.SelectNodes(
    '//q:Engine/q:Fixture', $result.Namespaces))

Assert-Condition ($resultFunctions.Count -eq ($hostFunctions.Count)) `
    "Unexpected Function count after save: $($resultFunctions.Count)."
Assert-Condition (@($result.Document.SelectNodes(
    '//q:Engine/q:Fixture', $result.Namespaces) | ForEach-Object OuterXml) -join "`n" -ceq
    ($hostFixtureXml -join "`n")) 'The host fixture patch changed during the merge.'
Assert-Condition ($result.Document.SelectSingleNode(
    '//q:Engine/q:InputOutputMap', $result.Namespaces).OuterXml -ceq $hostIoXml) `
    'The host Input/Output map changed during the merge.'
Assert-Condition ((@($priorityLookIds | ForEach-Object {
    $resultFunctions[$_].OuterXml
}) -join "`n") -ceq ($hostPriorityXml -join "`n")) `
    'The V21 private Priority Looks changed during the merge.'

$fixtureIds = @($resultFixtures | ForEach-Object {
    [int]$_.SelectSingleNode('q:ID', $result.Namespaces).InnerText
})
$fixtureIdSet = [System.Collections.Generic.HashSet[int]]::new()
foreach ($fixtureId in $fixtureIds) {
    Assert-Condition $fixtureIdSet.Add($fixtureId) "Duplicate Fixture ID $fixtureId"
}
Assert-Condition ($fixtureIdSet.Count -eq 16) `
    "Expected 16 physical/private fixtures; found $($fixtureIdSet.Count)."

foreach ($function in $resultFunctions.Values) {
    foreach ($step in $function.SelectNodes('q:Step', $result.Namespaces)) {
        if (-not [string]::IsNullOrWhiteSpace($step.InnerText)) {
            $reference = [int]$step.InnerText.Trim()
            Assert-Condition $resultFunctions.ContainsKey($reference) `
                "Function $($function.GetAttribute('ID')) references missing Function $reference."
        }
    }
    foreach ($value in $function.SelectNodes('q:FixtureVal', $result.Namespaces)) {
        $fixtureId = [int]$value.GetAttribute('ID')
        Assert-Condition $fixtureIdSet.Contains($fixtureId) `
            "Function $($function.GetAttribute('ID')) targets missing Fixture $fixtureId."
    }
}

$widgetIds = [System.Collections.Generic.HashSet[uint64]]::new()
foreach ($node in $result.Document.SelectNodes(
        '//q:VirtualConsole//*[@ID]', $result.Namespaces)) {
    if ($null -ne $node.SelectSingleNode('q:WindowState', $result.Namespaces)) {
        $id = [uint64]$node.GetAttribute('ID')
        Assert-Condition $widgetIds.Add($id) "Duplicate Virtual Console widget ID $id"
    }
}

$resultOutline = $result.Document.SelectSingleNode(
    '//q:VirtualConsole//q:Frame[starts-with(@Caption,"ACTIVE LOOP OUTLINE")]',
    $result.Namespaces)
Assert-Condition ($null -ne $resultOutline) 'Active-loop outline layer is missing.'
Assert-Condition ($resultOutline.SelectSingleNode(
    'q:Disabled', $result.Namespaces).InnerText -ceq 'True') `
    'Active-loop outline layer must remain read-only.'
$resultMonitorButtons = @($resultOutline.SelectNodes('q:Button', $result.Namespaces))
Assert-Condition ($resultMonitorButtons.Count -eq 128) `
    "Expected 128 active-loop monitor buttons; found $($resultMonitorButtons.Count)."
Assert-Condition (@($resultOutline.SelectNodes(
    'ancestor::q:SoloFrame', $result.Namespaces)).Count -eq 0) `
    'Active-loop monitors must remain outside the playback-owner SoloFrame.'
$monitorRawIds = @($resultMonitorButtons | ForEach-Object {
    [int]$_.SelectSingleNode('q:Function', $result.Namespaces).GetAttribute('ID')
} | Sort-Object)
Assert-Condition ((Compare-Object $rawLoopIds $monitorRawIds).Count -eq 0) `
    'Active-loop monitors do not cover all 128 raw Autoloops exactly once.'

$outputHash = (Get-FileHash -LiteralPath (
    (Resolve-Path -LiteralPath $OutputWorkspace).Path) -Algorithm SHA256).Hash
Write-Host 'PASS: V22 Unified Pro workspace merge'
Write-Host "  Variety Pro roots replaced: $($expectedVarietyRoots.Count)"
Write-Host "  Variety Pro helper Scenes imported: $($creativeHelperIds.Count)"
Write-Host "  Helper-ID collisions remapped: $($collisionMap.Count)"
Write-Host '  V21 Priority Looks, fixtures, I/O, ownership, and controls: preserved'
Write-Host '  Active-loop outline: 128 raw Chasers, manual + Auto Bank + Auto All'
Write-Host "  Output SHA-256: $outputHash"
