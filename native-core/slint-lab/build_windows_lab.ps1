[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SlintPrefix,

    [string] $BuildDirectory = ""
)

$ErrorActionPreference = "Stop"

$LabDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$NativeCoreDirectory = Split-Path -Parent $LabDirectory
$RepositoryRoot = Split-Path -Parent $NativeCoreDirectory
$ResolvedSlintPrefix = (Resolve-Path $SlintPrefix).Path

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $RepositoryRoot "build/slint-fixtures-looks-lab"
}

$SlintConfig = Get-ChildItem -Path $ResolvedSlintPrefix -Filter "SlintConfig.cmake" -File -Recurse |
    Select-Object -First 1
if ($null -eq $SlintConfig) {
    throw "SlintConfig.cmake was not found beneath $ResolvedSlintPrefix"
}

$ConfigureArguments = @(
    "-S"
    $NativeCoreDirectory
    "-B"
    $BuildDirectory
    "-A"
    "x64"
    "-DEMBERLIGHTS_BUILD_SLINT_LAB=ON"
    "-DCMAKE_PREFIX_PATH=$ResolvedSlintPrefix"
    "-DEMBERLIGHTS_VERSION=slint-lab-1"
)
& cmake @ConfigureArguments
if ($LASTEXITCODE -ne 0) { throw "Slint lab configure failed" }

$BuildArguments = @(
    "--build"
    $BuildDirectory
    "--config"
    "Release"
    "--target"
    "EmberLightsSlintLab"
    "--parallel"
)
& cmake @BuildArguments
if ($LASTEXITCODE -ne 0) { throw "Slint lab build failed" }

$Executable = Get-ChildItem -Path $BuildDirectory -Filter "EmberLights-Fixtures-Looks-Lab.exe" -File -Recurse |
    Select-Object -First 1
if ($null -eq $Executable) {
    throw "The Slint lab executable was not produced"
}

& $Executable.FullName --model-smoke
if ($LASTEXITCODE -ne 0) { throw "Slint lab model smoke failed" }

$ArtifactDirectory = Join-Path $BuildDirectory "artifact"
if (Test-Path $ArtifactDirectory) {
    Remove-Item -Path $ArtifactDirectory -Recurse -Force
}
New-Item -Path $ArtifactDirectory -ItemType Directory | Out-Null
Copy-Item -Path $Executable.FullName -Destination $ArtifactDirectory

$RuntimeDlls = Get-ChildItem -Path $ResolvedSlintPrefix -Filter "*.dll" -File -Recurse
foreach ($RuntimeDll in $RuntimeDlls) {
    $Destination = Join-Path $ArtifactDirectory $RuntimeDll.Name
    if (-not (Test-Path $Destination)) {
        Copy-Item -Path $RuntimeDll.FullName -Destination $Destination
    }
}

$LicenseDirectory = Join-Path $ResolvedSlintPrefix "licenses"
if (Test-Path $LicenseDirectory) {
    Copy-Item -Path $LicenseDirectory -Destination (Join-Path $ArtifactDirectory "licenses") -Recurse
}

$Readme = @"
EmberLights Fixtures + Static Looks Slint Lab

This is an issue #37 toolkit-evaluation artifact, not a product preview or
installer. It uses an in-memory sample project, opens no DMX output, and does
not persist edits. Run EmberLights-Fixtures-Looks-Lab.exe to evaluate the UI.
"@
Set-Content -Path (Join-Path $ArtifactDirectory "LAB_README.txt") -Value $Readme -Encoding UTF8

$ZipPath = Join-Path $BuildDirectory "EmberLights-Fixtures-Looks-Slint-Lab-win-x64.zip"
if (Test-Path $ZipPath) {
    Remove-Item -Path $ZipPath -Force
}
Compress-Archive -Path (Join-Path $ArtifactDirectory "*") -DestinationPath $ZipPath
Write-Host "Created $ZipPath"
