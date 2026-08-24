[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$SourceFile,

    [Parameter(Mandatory)]
    [string]$DestinationRoot,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$EventId,

    [ValidateRange(2, 300)]
    [int]$StableSeconds = 10,

    [ValidateRange(1, 120)]
    [int]$WaitTimeoutMinutes = 10,

    [switch]$Overwrite
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-NormalizedSha256 {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Test-ExclusiveRead {
    param([Parameter(Mandatory)][string]$Path)

    try {
        $stream = [IO.File]::Open(
            $Path,
            [IO.FileMode]::Open,
            [IO.FileAccess]::Read,
            [IO.FileShare]::None
        )
        $stream.Dispose()
        return $true
    }
    catch {
        return $false
    }
}

function Wait-ForCompletedFile {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][int]$StableForSeconds,
        [Parameter(Mandatory)][datetime]$Deadline
    )

    Write-Host "Waiting for the recorder to release and stabilize: $Path"

    while ((Get-Date) -lt $Deadline) {
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
            Start-Sleep -Seconds 2
            continue
        }

        $before = Get-Item -LiteralPath $Path
        if ($before.Length -le 0) {
            Start-Sleep -Seconds 2
            continue
        }

        if (-not (Test-ExclusiveRead -Path $Path)) {
            Start-Sleep -Seconds 2
            continue
        }

        Start-Sleep -Seconds $StableForSeconds
        $after = Get-Item -LiteralPath $Path

        $unchanged = (
            $before.Length -eq $after.Length -and
            $before.LastWriteTimeUtc -eq $after.LastWriteTimeUtc
        )

        if ($unchanged -and (Test-ExclusiveRead -Path $Path)) {
            return $after
        }
    }

    throw "Timed out after $WaitTimeoutMinutes minute(s) waiting for a closed, stable recording file. Stop/finalize the VirtualDJ recording before copying it."
}

$invalidEventChars = [IO.Path]::GetInvalidFileNameChars()
if ($EventId.IndexOfAny($invalidEventChars) -ge 0) {
    throw 'EventId contains characters that are invalid in a Windows folder name.'
}
if ($EventId -in '.', '..') {
    throw 'EventId must be a real event identifier, not a relative-path token.'
}

$resolvedSource = (Resolve-Path -LiteralPath $SourceFile).Path
$deadline = (Get-Date).AddMinutes($WaitTimeoutMinutes)
$sourceInfo = Wait-ForCompletedFile `
    -Path $resolvedSource `
    -StableForSeconds $StableSeconds `
    -Deadline $deadline

if (-not (Test-Path -LiteralPath $DestinationRoot -PathType Container)) {
    if ($PSCmdlet.ShouldProcess($DestinationRoot, 'Create recording destination root')) {
        New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
    }
}

$destinationDirectory = Join-Path $DestinationRoot $EventId
if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
    if ($PSCmdlet.ShouldProcess($destinationDirectory, 'Create event recording directory')) {
        New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null
    }
}

$sourceDirectory = Split-Path -Parent $resolvedSource
$fileName = Split-Path -Leaf $resolvedSource
$destinationFile = Join-Path $destinationDirectory $fileName
$manifestFile = "$destinationFile.sha256.json"
$hashTextFile = "$destinationFile.sha256"

$sourceHash = Get-NormalizedSha256 -Path $resolvedSource

if (Test-Path -LiteralPath $destinationFile -PathType Leaf) {
    $existingDestinationHash = Get-NormalizedSha256 -Path $destinationFile
    if ($existingDestinationHash -eq $sourceHash) {
        Write-Host 'Destination already exists and matches the source SHA-256. No audio copy is required.'
    }
    elseif (-not $Overwrite) {
        throw "Destination exists with a different SHA-256: $destinationFile. Use -Overwrite only after verifying that replacement is intentional."
    }
    elseif ($PSCmdlet.ShouldProcess($destinationFile, 'Remove mismatched destination before verified replacement')) {
        Remove-Item -LiteralPath $destinationFile -Force
    }
}

if (-not (Test-Path -LiteralPath $destinationFile -PathType Leaf)) {
    $stagingDirectory = Join-Path $destinationDirectory ('.copy-' + [guid]::NewGuid().ToString('N'))
    $stagedFile = Join-Path $stagingDirectory $fileName

    if ($PSCmdlet.ShouldProcess($destinationFile, 'Copy completed recording without transcoding')) {
        New-Item -ItemType Directory -Path $stagingDirectory -Force | Out-Null

        try {
            & robocopy.exe `
                $sourceDirectory `
                $stagingDirectory `
                $fileName `
                /COPY:DAT `
                /DCOPY:T `
                /Z `
                /J `
                /R:3 `
                /W:5 `
                /NP `
                /NFL `
                /NDL

            $robocopyExitCode = $LASTEXITCODE
            if ($robocopyExitCode -gt 7) {
                throw "Robocopy failed with exit code $robocopyExitCode. The partial staging directory is $stagingDirectory."
            }

            if (-not (Test-Path -LiteralPath $stagedFile -PathType Leaf)) {
                throw "Robocopy reported success but the staged file was not found: $stagedFile"
            }

            $stagedHash = Get-NormalizedSha256 -Path $stagedFile
            if ($stagedHash -ne $sourceHash) {
                throw "SHA-256 mismatch after copy. Source: $sourceHash; staged destination: $stagedHash"
            }

            Move-Item -LiteralPath $stagedFile -Destination $destinationFile -Force
        }
        finally {
            if (Test-Path -LiteralPath $stagingDirectory -PathType Container) {
                Remove-Item -LiteralPath $stagingDirectory -Recurse -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

if (-not (Test-Path -LiteralPath $destinationFile -PathType Leaf)) {
    throw 'Destination file does not exist after the copy operation.'
}

$destinationInfo = Get-Item -LiteralPath $destinationFile
$destinationHash = Get-NormalizedSha256 -Path $destinationFile
$verified = ($sourceHash -eq $destinationHash -and $sourceInfo.Length -eq $destinationInfo.Length)

if (-not $verified) {
    throw "Final verification failed. Source SHA-256: $sourceHash; destination SHA-256: $destinationHash; source bytes: $($sourceInfo.Length); destination bytes: $($destinationInfo.Length)."
}

$manifest = [ordered]@{
    SchemaVersion = 1
    Verified = $true
    VerifiedAt = (Get-Date).ToString('o')
    EventId = $EventId
    SourceComputer = $env:COMPUTERNAME
    SourcePath = $resolvedSource
    DestinationPath = $destinationFile
    FileName = $fileName
    Bytes = $sourceInfo.Length
    SourceLastWriteTimeUtc = $sourceInfo.LastWriteTimeUtc.ToString('o')
    HashAlgorithm = 'SHA256'
    SourceSha256 = $sourceHash
    DestinationSha256 = $destinationHash
    CopyMethod = 'robocopy /COPY:DAT /DCOPY:T /Z /J with staged promotion'
    SourceDeleted = $false
}

if ($PSCmdlet.ShouldProcess($manifestFile, 'Write recording verification manifest')) {
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestFile -Encoding UTF8
    ("{0}  {1}" -f $destinationHash, $fileName) | Set-Content -LiteralPath $hashTextFile -Encoding ASCII
}

Write-Host ''
Write-Host 'PASS: recording copied and verified bit-for-bit.'
Write-Host "Source:       $resolvedSource"
Write-Host "Destination:  $destinationFile"
Write-Host "Bytes:        $($sourceInfo.Length)"
Write-Host "SHA-256:      $sourceHash"
Write-Host "Manifest:     $manifestFile"
Write-Host ''
Write-Host 'The source file was not deleted.'

$manifest
