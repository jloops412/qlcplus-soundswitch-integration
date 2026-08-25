[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BoothAddress,

    [ValidateRange(1, 65535)]
    [int]$WebPort = 9999,

    [ValidateRange(1, 65535)]
    [int]$Os2lPort = 9996,

    [ValidateRange(1, 100)]
    [int]$PingCount = 10,

    [ValidateRange(1, 10000)]
    [int]$PingTimeoutMs = 1000,

    [ValidateRange(0, 10000)]
    [double]$WarnAverageLatencyMs = 5,

    [ValidateRange(0, 10000)]
    [double]$WarnMaximumLatencyMs = 20,

    [string]$OutputJsonPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($env:OS -ne 'Windows_NT') {
    throw 'This preflight currently supports Windows only.'
}

function Test-TcpPort {
    param(
        [Parameter(Mandatory)][string]$ComputerName,
        [Parameter(Mandatory)][int]$Port
    )

    try {
        $client = [Net.Sockets.TcpClient]::new()
        $async = $client.BeginConnect($ComputerName, $Port, $null, $null)
        $connected = $async.AsyncWaitHandle.WaitOne(3000, $false)
        if (-not $connected) {
            $client.Close()
            return $false
        }

        $client.EndConnect($async)
        $client.Close()
        return $true
    }
    catch {
        return $false
    }
}

function Get-HttpProbe {
    param([Parameter(Mandatory)][uri]$Uri)

    try {
        $response = Invoke-WebRequest `
            -Uri $Uri `
            -UseBasicParsing `
            -TimeoutSec 5 `
            -MaximumRedirection 0 `
            -ErrorAction Stop

        return [pscustomobject]@{
            Reachable = $true
            StatusCode = [int]$response.StatusCode
            Detail = 'QLC+ web endpoint returned a page without an authentication challenge.'
        }
    }
    catch {
        $statusCode = $null
        if ($_.Exception.Response) {
            try {
                $statusCode = [int]$_.Exception.Response.StatusCode
            }
            catch {
                $statusCode = $null
            }
        }

        if ($statusCode -eq 401) {
            return [pscustomobject]@{
                Reachable = $true
                StatusCode = 401
                Detail = 'QLC+ web endpoint is reachable and authentication is enabled.'
            }
        }

        return [pscustomobject]@{
            Reachable = $false
            StatusCode = $statusCode
            Detail = $_.Exception.Message
        }
    }
}

$pingClient = [Net.NetworkInformation.Ping]::new()
$latencies = [Collections.Generic.List[double]]::new()
$pingFailures = 0

for ($index = 1; $index -le $PingCount; $index++) {
    try {
        $reply = $pingClient.Send($BoothAddress, $PingTimeoutMs)
        if ($reply.Status -eq [Net.NetworkInformation.IPStatus]::Success) {
            $latencies.Add([double]$reply.RoundtripTime)
        }
        else {
            $pingFailures++
        }
    }
    catch {
        $pingFailures++
    }

    if ($index -lt $PingCount) {
        Start-Sleep -Milliseconds 200
    }
}
$pingClient.Dispose()

$successfulPings = $latencies.Count
$packetLossPercent = [math]::Round((100 * $pingFailures / $PingCount), 2)
$averageLatency = if ($successfulPings -gt 0) {
    [math]::Round(($latencies | Measure-Object -Average).Average, 2)
} else {
    $null
}
$maximumLatency = if ($successfulPings -gt 0) {
    [math]::Round(($latencies | Measure-Object -Maximum).Maximum, 2)
} else {
    $null
}

$os2lReachable = Test-TcpPort -ComputerName $BoothAddress -Port $Os2lPort
$webPortReachable = Test-TcpPort -ComputerName $BoothAddress -Port $WebPort
$webProbe = Get-HttpProbe -Uri ([uri]"http://${BoothAddress}:$WebPort/")

$pingPass = ($pingFailures -eq 0 -and $successfulPings -eq $PingCount)
$latencyWarning = $false
if ($averageLatency -ne $null -and $averageLatency -gt $WarnAverageLatencyMs) {
    $latencyWarning = $true
}
if ($maximumLatency -ne $null -and $maximumLatency -gt $WarnMaximumLatencyMs) {
    $latencyWarning = $true
}

$overallPass = $pingPass -and $os2lReachable -and $webPortReachable -and $webProbe.Reachable

$result = [pscustomobject]@{
    TestedAt = (Get-Date).ToString('o')
    SourceComputer = $env:COMPUTERNAME
    BoothAddress = $BoothAddress
    PingSent = $PingCount
    PingReceived = $successfulPings
    PacketLossPercent = $packetLossPercent
    AverageLatencyMs = $averageLatency
    MaximumLatencyMs = $maximumLatency
    LatencyWarning = $latencyWarning
    Os2lPort = $Os2lPort
    Os2lReachable = $os2lReachable
    WebPort = $WebPort
    WebPortReachable = $webPortReachable
    WebHttpReachable = $webProbe.Reachable
    WebHttpStatus = $webProbe.StatusCode
    WebHttpDetail = $webProbe.Detail
    Passed = $overallPass
}

Write-Host ''
Write-Host 'LLE Booth Connection Test'
Write-Host '-------------------------'
$result | Format-List

if ($latencyWarning) {
    Write-Warning "Latency exceeded the warning threshold (average > $WarnAverageLatencyMs ms or maximum > $WarnMaximumLatencyMs ms). This is not an automatic failure, but investigate before qualification."
}

if ($OutputJsonPath) {
    $outputParent = Split-Path -Parent $OutputJsonPath
    if ($outputParent -and -not (Test-Path -LiteralPath $outputParent -PathType Container)) {
        New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
    }

    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $OutputJsonPath -Encoding UTF8
    Write-Host "Saved JSON result: $OutputJsonPath"
}

if (-not $overallPass) {
    Write-Error 'Booth connection preflight failed. Do not promote networked QLC+ until ping, OS2L, and web checks all pass.'
    exit 1
}

Write-Host 'PASS: booth address, OS2L, and QLC+ web endpoint are reachable.'
exit 0
