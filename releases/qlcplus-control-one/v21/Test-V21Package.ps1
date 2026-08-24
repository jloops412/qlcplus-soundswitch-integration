[CmdletBinding()]
param(
    [string]$ReleaseDirectory = $PSScriptRoot,
    [string]$BaselineWorkspace
)

$ErrorActionPreference = 'Stop'

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
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
    Assert-Condition ($actualHash -eq $expectedHash) `
        "Checksum mismatch: $name"
}

$unlistedFiles = @(Get-ChildItem -LiteralPath $release -File |
    Where-Object {
        $_.Name -ne 'SHA256SUMS.txt' -and -not $listedFiles.Contains($_.Name)
    })
Assert-Condition ($unlistedFiles.Count -eq 0) `
    "Release contains files absent from SHA256SUMS.txt: $($unlistedFiles.Name -join ', ')"

$requiredFiles = @(
    'IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw',
    'SoundSwitch-Control-One-Performance.qxi',
    'soundswitch.dll',
    'Install-SoundSwitchPlugin.ps1',
    'Rollback-SoundSwitchPlugin.ps1',
    'Test-V21Package.ps1',
    'VIRTUALDJ_OS2L_AUTO_RECONNECT.md',
    'RELEASE_NOTES.md',
    'README.md',
    'LICENSE-APACHE-2.0.txt'
)
foreach ($name in $requiredFiles) {
    Assert-Condition $listedFiles.Contains($name) "Required release file is not checksummed: $name"
}

$workspacePath = Join-Path $release 'IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw'
$profilePath = Join-Path $release 'SoundSwitch-Control-One-Performance.qxi'
[xml]$workspace = Get-Content -LiteralPath $workspacePath -Raw
[xml]$profile = Get-Content -LiteralPath $profilePath -Raw

$functions = @($workspace.Workspace.Engine.Function)
$functionIds = @($functions | ForEach-Object { [int]$_.ID })
$duplicateFunctionIds = @($functionIds | Group-Object | Where-Object Count -gt 1)
Assert-Condition ($duplicateFunctionIds.Count -eq 0) `
    "Duplicate Function IDs: $($duplicateFunctionIds.Name -join ', ')"

$widgetIds = @($workspace.SelectNodes('//VirtualConsole//*[@ID]') |
    ForEach-Object { [int]$_.ID })
$duplicateWidgetIds = @($widgetIds | Group-Object | Where-Object Count -gt 1)
Assert-Condition ($duplicateWidgetIds.Count -eq 0) `
    "Duplicate Virtual Console widget IDs: $($duplicateWidgetIds.Name -join ', ')"

$missingReferences = [System.Collections.Generic.List[int]]::new()
foreach ($reference in @($workspace.SelectNodes('//VirtualConsole//Function'))) {
    $raw = if ($reference.HasAttribute('ID')) {
        $reference.GetAttribute('ID')
    } else {
        $reference.InnerText.Trim()
    }
    Assert-Condition ($raw -match '^\d+$') "Malformed Virtual Console Function reference: $raw"
    $id = [int]$raw
    if ($functionIds -notcontains $id) {
        $missingReferences.Add($id)
    }
}
Assert-Condition ($missingReferences.Count -eq 0) `
    "Missing Virtual Console Function references: $($missingReferences -join ', ')"

$channels = @($profile.SelectNodes('//*[local-name()="Channel"]') |
    ForEach-Object { [int]$_.Number })
$duplicateChannels = @($channels | Group-Object | Where-Object Count -gt 1)
Assert-Condition ($duplicateChannels.Count -eq 0) `
    "Duplicate input-profile channels: $($duplicateChannels.Name -join ', ')"

$physicalFixtures = @($workspace.Workspace.Engine.Fixture |
    Where-Object { [int]$_.Universe -eq 0 })
$priorityFixtures = @($workspace.Workspace.Engine.Fixture |
    Where-Object { [int]$_.Universe -eq 2 })
Assert-Condition ($physicalFixtures.Count -eq 8) `
    "Expected 8 physical fixtures; found $($physicalFixtures.Count)"
Assert-Condition ($priorityFixtures.Count -eq 8) `
    "Expected 8 private Priority Layer fixtures; found $($priorityFixtures.Count)"

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
        $matches = @($workspace.Workspace.Engine.Fixture | Where-Object {
            [int]$_.Universe -eq $universe -and
            [int]$_.Address -eq $expected.Address -and
            [int]$_.Channels -eq $expected.Channels -and
            [string]$_.Mode -eq $expected.Mode
        })
        Assert-Condition ($matches.Count -eq 1) `
            "Fixture patch mismatch at Universe $($universe + 1), address $($expected.Address + 1)"
    }
}

$scriptParseErrors = [System.Collections.Generic.List[string]]::new()
foreach ($scriptName in @('Install-SoundSwitchPlugin.ps1', 'Rollback-SoundSwitchPlugin.ps1')) {
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

if (-not [string]::IsNullOrWhiteSpace($BaselineWorkspace)) {
    [xml]$baseline = Get-Content -LiteralPath (
        (Resolve-Path -LiteralPath $BaselineWorkspace).Path) -Raw
    $baselineFunctions = @($baseline.Workspace.Engine.Function)
    $functionsById = @{}
    foreach ($function in $functions) {
        $functionsById[[string]$function.ID] = $function
    }
    $changedFunctions = [System.Collections.Generic.List[int]]::new()
    foreach ($baselineFunction in $baselineFunctions) {
        $id = [string]$baselineFunction.ID
        if (-not $functionsById.ContainsKey($id) -or
            $functionsById[$id].OuterXml -cne $baselineFunction.OuterXml) {
            $changedFunctions.Add([int]$baselineFunction.ID)
        }
    }
    Assert-Condition ($changedFunctions.Count -eq 0) `
        "V20 creative/control Function XML changed in V21: $($changedFunctions -join ', ')"
}

$textFiles = Get-ChildItem -LiteralPath $release -File |
    Where-Object Extension -in @('.md', '.ps1', '.txt', '.qxw', '.qxi')
$personalLeaks = @($textFiles | Select-String -Pattern `
    'C:\\Users\\|\bjloop\b|\bJ:\\' -CaseSensitive:$false)
Assert-Condition ($personalLeaks.Count -eq 0) `
    'A personal username or absolute DJ-PC path appears in the release package.'

Write-Host 'PASS: QLC+ SoundSwitch V21 package integrity'
Write-Host "  Files checksummed: $($listedFiles.Count)"
Write-Host "  Workspace Functions: $($functions.Count)"
Write-Host "  Input-profile channels: $($channels.Count)"
Write-Host '  Fixture patch: 8 physical + 8 private Priority Layer fixtures'
if (-not [string]::IsNullOrWhiteSpace($BaselineWorkspace)) {
    Write-Host '  V20 Function XML: preserved exactly'
}
