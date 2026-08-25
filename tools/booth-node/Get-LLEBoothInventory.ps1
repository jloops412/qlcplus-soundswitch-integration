[CmdletBinding()]
param(
    [ValidateSet('BoothNode', 'DJLaptop')]
    [string]$MachineRole = 'BoothNode',

    [string]$OutputRoot = 'C:\LLE\Inventory',

    [string]$QlcRoot = 'C:\LLE\QLC\5.3.0-GIT-a124abe',

    [string]$WorkspacePath = 'C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw',

    [string]$PackageRoot = 'C:\LLE\Packages\qlcplus-control-one-v24',

    [ValidateRange(1, 4096)]
    [int]$MinimumFreeGB = 25,

    [string]$ExpectedGateway,

    [string]$ExpectedAddressPrefix,

    [switch]$IncludeMacAddresses
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$ExpectedPluginSha256 = '2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC'
$ExpectedWorkspaceSha256 = 'DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF'
$ExpectedTaskName = 'LLE Booth - QLC+ V24'

$warnings = [Collections.Generic.List[string]]::new()
$collectionNotes = [Collections.Generic.List[string]]::new()

function Add-InventoryWarning {
    param([Parameter(Mandatory)][string]$Message)

    if (-not $warnings.Contains($Message)) {
        $warnings.Add($Message)
    }
}

function Add-CollectionNote {
    param([Parameter(Mandatory)][string]$Message)

    if (-not $collectionNotes.Contains($Message)) {
        $collectionNotes.Add($Message)
    }
}

function ConvertTo-GB {
    param([AllowNull()][object]$Bytes)

    if ($null -eq $Bytes) {
        return $null
    }

    return [math]::Round(([double]$Bytes / 1GB), 2)
}

function ConvertTo-MarkdownCell {
    param([AllowNull()][object]$Value)

    if ($null -eq $Value) {
        return ''
    }

    return ([string]$Value).Replace('|', '\|').Replace("`r", ' ').Replace("`n", ' ')
}

function Get-OptionalPropertyValue {
    param(
        [AllowNull()][object]$InputObject,
        [Parameter(Mandatory)][string]$PropertyName
    )

    if ($null -eq $InputObject) {
        return $null
    }

    $property = $InputObject.PSObject.Properties[$PropertyName]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Get-FileEvidence {
    param(
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Path,
        [string]$ExpectedSha256
    )

    $exists = Test-Path -LiteralPath $Path -PathType Leaf
    $actualSha256 = $null
    $hashMatches = $null
    $bytes = $null
    $lastWriteUtc = $null

    if ($exists) {
        try {
            $item = Get-Item -LiteralPath $Path
            $bytes = $item.Length
            $lastWriteUtc = $item.LastWriteTimeUtc.ToString('o')
            $actualSha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
            if ($ExpectedSha256) {
                $hashMatches = ($actualSha256 -eq $ExpectedSha256.ToUpperInvariant())
                if (-not $hashMatches) {
                    Add-InventoryWarning "$Label exists but its SHA-256 does not match the pinned V24 value. Do not install or promote this copy."
                }
            }
        }
        catch {
            Add-InventoryWarning "Could not hash $Label at $Path. $($_.Exception.Message)"
        }
    }

    return [pscustomobject]@{
        Label = $Label
        Path = $Path
        Exists = $exists
        Bytes = $bytes
        LastWriteTimeUtc = $lastWriteUtc
        Sha256 = $actualSha256
        ExpectedSha256 = if ($ExpectedSha256) { $ExpectedSha256.ToUpperInvariant() } else { $null }
        HashMatches = $hashMatches
    }
}

function Test-PendingRestart {
    $reasons = [Collections.Generic.List[string]]::new()

    if (Test-Path -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending') {
        $reasons.Add('Component Based Servicing')
    }
    if (Test-Path -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired') {
        $reasons.Add('Windows Update')
    }

    try {
        $sessionManager = Get-ItemProperty `
            -LiteralPath 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager' `
            -Name PendingFileRenameOperations `
            -ErrorAction Stop
        if ($sessionManager.PendingFileRenameOperations) {
            $reasons.Add('Pending file rename operation')
        }
    }
    catch {
        # The value normally does not exist when no rename is pending.
    }

    return [pscustomobject]@{
        Pending = ($reasons.Count -gt 0)
        Reasons = @($reasons)
    }
}

if ($env:OS -ne 'Windows_NT') {
    throw 'This inventory helper supports Windows only.'
}

$isAdministrator = $false
try {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}
catch {
    Add-CollectionNote "Could not determine elevation state. $($_.Exception.Message)"
}

$captureTimestamp = Get-Date
$safeComputerName = if ($env:COMPUTERNAME) { $env:COMPUTERNAME } else { 'UNKNOWN-COMPUTER' }
$safeRole = $MachineRole -replace '[^A-Za-z0-9_-]', '_'
$captureName = '{0}-{1}-{2}' -f $captureTimestamp.ToString('yyyyMMdd-HHmmss'), $safeRole, $safeComputerName

New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$captureRoot = Join-Path $OutputRoot $captureName
if (Test-Path -LiteralPath $captureRoot) {
    $captureRoot = '{0}-{1}' -f $captureRoot, ([guid]::NewGuid().ToString('N').Substring(0, 8))
}
New-Item -ItemType Directory -Path $captureRoot -Force | Out-Null

$computerSystem = $null
$operatingSystem = $null
$processor = $null
$windowsVersion = $null

try {
    $computerSystem = Get-CimInstance Win32_ComputerSystem
}
catch {
    Add-CollectionNote "Computer-system details were unavailable. $($_.Exception.Message)"
}
try {
    $operatingSystem = Get-CimInstance Win32_OperatingSystem
}
catch {
    Add-CollectionNote "Windows details were unavailable. $($_.Exception.Message)"
}
try {
    $processor = Get-CimInstance Win32_Processor | Select-Object -First 1
}
catch {
    Add-CollectionNote "Processor details were unavailable. $($_.Exception.Message)"
}
try {
    $windowsVersion = Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
}
catch {
    Add-CollectionNote "Windows display-version details were unavailable. $($_.Exception.Message)"
}

$windowsProductName = Get-OptionalPropertyValue -InputObject $windowsVersion -PropertyName 'ProductName'
$windowsDisplayVersion = Get-OptionalPropertyValue -InputObject $windowsVersion -PropertyName 'DisplayVersion'
$windowsReleaseId = Get-OptionalPropertyValue -InputObject $windowsVersion -PropertyName 'ReleaseId'
$windowsBuild = $null
if ($windowsVersion) {
    $buildNumber = Get-OptionalPropertyValue -InputObject $windowsVersion -PropertyName 'CurrentBuildNumber'
    $updateBuildRevision = Get-OptionalPropertyValue -InputObject $windowsVersion -PropertyName 'UBR'
    if ($null -ne $updateBuildRevision) {
        $windowsBuild = '{0}.{1}' -f $buildNumber, $updateBuildRevision
    }
    else {
        $windowsBuild = [string]$buildNumber
    }
}
elseif ($operatingSystem) {
    $windowsBuild = $operatingSystem.BuildNumber
}

$logicalDisks = @()
try {
    $logicalDisks = @(
        Get-CimInstance Win32_LogicalDisk -Filter 'DriveType=3' |
            Sort-Object DeviceID |
            ForEach-Object {
                $freeGB = ConvertTo-GB $_.FreeSpace
                [pscustomobject]@{
                    Drive = $_.DeviceID
                    FileSystem = $_.FileSystem
                    SizeGB = ConvertTo-GB $_.Size
                    FreeGB = $freeGB
                    FreePercent = if ($_.Size) { [math]::Round((100 * [double]$_.FreeSpace / [double]$_.Size), 1) } else { $null }
                    MeetsMinimumFreeSpace = if ($null -ne $freeGB) { $freeGB -ge $MinimumFreeGB } else { $false }
                }
            }
    )
}
catch {
    Add-CollectionNote "Logical-disk details were unavailable. $($_.Exception.Message)"
}

if ($logicalDisks.Count -eq 0) {
    Add-InventoryWarning 'No fixed-disk free-space result was collected.'
}
elseif (-not ($logicalDisks | Where-Object MeetsMinimumFreeSpace)) {
    Add-InventoryWarning "No fixed drive currently has the recommended $MinimumFreeGB GB free-space floor."
}

$physicalDisks = @()
try {
    $physicalDisks = @(
        Get-CimInstance Win32_DiskDrive |
            Sort-Object Index |
            ForEach-Object {
                [pscustomobject]@{
                    Index = $_.Index
                    Model = $_.Model
                    MediaType = $_.MediaType
                    InterfaceType = $_.InterfaceType
                    SizeGB = ConvertTo-GB $_.Size
                    Status = $_.Status
                    SerialNumber = '[intentionally omitted]'
                }
            }
    )
}
catch {
    Add-CollectionNote "Physical-disk details were unavailable. $($_.Exception.Message)"
}

$networkAdapters = @()
try {
    $adapters = @(Get-NetAdapter -Physical -ErrorAction Stop | Sort-Object ifIndex)
    foreach ($adapter in $adapters) {
        $ipConfiguration = $null
        $ipInterface = $null
        $profile = $null
        $dnsAddresses = @()
        $dhcpServer = $null

        try {
            $ipConfiguration = Get-NetIPConfiguration -InterfaceIndex $adapter.ifIndex -ErrorAction Stop
        }
        catch {
            Add-CollectionNote "No IP configuration was available for adapter '$($adapter.Name)'."
        }
        try {
            $ipInterface = Get-NetIPInterface -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction Stop
        }
        catch {
            Add-CollectionNote "No IPv4 interface record was available for adapter '$($adapter.Name)'."
        }
        try {
            $profile = Get-NetConnectionProfile -InterfaceIndex $adapter.ifIndex -ErrorAction Stop
        }
        catch {
            # Disconnected adapters often have no active connection profile.
        }
        try {
            $dnsAddresses = @(
                (Get-DnsClientServerAddress -InterfaceIndex $adapter.ifIndex -AddressFamily IPv4 -ErrorAction Stop).ServerAddresses
            )
        }
        catch {
            Add-CollectionNote "DNS details were unavailable for adapter '$($adapter.Name)'."
        }
        try {
            $cimAdapterConfiguration = Get-CimInstance Win32_NetworkAdapterConfiguration |
                Where-Object InterfaceIndex -eq $adapter.ifIndex |
                Select-Object -First 1
            if ($cimAdapterConfiguration) {
                $dhcpServer = $cimAdapterConfiguration.DHCPServer
            }
        }
        catch {
            Add-CollectionNote "DHCP-server details were unavailable for adapter '$($adapter.Name)'."
        }

        $ipv4Addresses = if ($ipConfiguration) { @($ipConfiguration.IPv4Address | ForEach-Object IPAddress) } else { @() }
        $gateways = if ($ipConfiguration) { @($ipConfiguration.IPv4DefaultGateway | ForEach-Object NextHop) } else { @() }
        $networkCategory = if ($profile) { [string]$profile.NetworkCategory } else { $null }

        $isWiredCandidate = (
            $adapter.InterfaceDescription -notmatch '(?i)wireless|wi-?fi|bluetooth|wwan|mobile' -and
            $adapter.Name -notmatch '(?i)wireless|wi-?fi|bluetooth|wwan|mobile'
        )

        if ($adapter.Status -eq 'Up' -and $isWiredCandidate -and $networkCategory -and $networkCategory -ne 'Private') {
            Add-InventoryWarning "Active wired adapter '$($adapter.Name)' is using the '$networkCategory' Windows profile; the ReadyNet show LAN must be Private before firewall rules are installed."
        }
        if ($ExpectedGateway -and $adapter.Status -eq 'Up' -and $isWiredCandidate -and ($gateways -notcontains $ExpectedGateway)) {
            Add-InventoryWarning "Active wired adapter '$($adapter.Name)' does not report expected gateway $ExpectedGateway."
        }
        if ($ExpectedAddressPrefix -and $adapter.Status -eq 'Up' -and $isWiredCandidate) {
            $matchingAddress = @($ipv4Addresses | Where-Object { $_.StartsWith($ExpectedAddressPrefix) })
            if ($matchingAddress.Count -eq 0) {
                Add-InventoryWarning "Active wired adapter '$($adapter.Name)' has no IPv4 address beginning with $ExpectedAddressPrefix."
            }
        }

        $networkAdapters += [pscustomobject]@{
            Name = $adapter.Name
            InterfaceDescription = $adapter.InterfaceDescription
            InterfaceIndex = $adapter.ifIndex
            Status = [string]$adapter.Status
            LinkSpeed = [string]$adapter.LinkSpeed
            WiredCandidate = $isWiredCandidate
            MacAddress = if ($IncludeMacAddresses) { $adapter.MacAddress } else { '[omitted; use -IncludeMacAddresses only for private reservation evidence]' }
            IPv4Addresses = $ipv4Addresses
            Dhcp = if ($ipInterface) { [string]$ipInterface.Dhcp } else { $null }
            DhcpServer = $dhcpServer
            DefaultGateways = $gateways
            DnsServers = $dnsAddresses
            NetworkProfile = $networkCategory
        }
    }
}
catch {
    Add-CollectionNote "Network-adapter details were unavailable. $($_.Exception.Message)"
}

$activeWiredAdapters = @($networkAdapters | Where-Object { $_.Status -eq 'Up' -and $_.WiredCandidate })
if ($activeWiredAdapters.Count -eq 0) {
    Add-InventoryWarning 'No active wired Ethernet candidate was detected. Connect the booth computer directly to a ReadyNet LAN port before network qualification.'
}
if ($IncludeMacAddresses) {
    Add-InventoryWarning 'This capture includes MAC addresses. Keep it in private qualification storage and never commit it to the public repository.'
}

$usbControllers = @()
try {
    $usbControllers = @(
        Get-CimInstance Win32_USBController |
            Sort-Object Name |
            ForEach-Object {
                [pscustomobject]@{
                    Name = $_.Name
                    Manufacturer = $_.Manufacturer
                    Status = $_.Status
                    DeviceId = '[intentionally omitted]'
                }
            }
    )
}
catch {
    Add-CollectionNote "USB-controller details were unavailable. $($_.Exception.Message)"
}

$relevantDevices = @()
try {
    $relevantDevices = @(
        Get-PnpDevice -PresentOnly -ErrorAction Stop |
            Where-Object {
                $_.FriendlyName -match '(?i)Control One|SoundSwitch|DDJ-REV7|REV7|MIDI' -or
                $_.Class -in @('USB', 'Media', 'AudioEndpoint')
            } |
            Sort-Object Class, FriendlyName |
            ForEach-Object {
                [pscustomobject]@{
                    Status = [string]$_.Status
                    Class = $_.Class
                    FriendlyName = $_.FriendlyName
                    InstanceId = '[intentionally omitted]'
                }
            }
    )
}
catch {
    Add-CollectionNote "Present USB/audio/MIDI device details were unavailable. $($_.Exception.Message)"
}

$powerCapabilities = @()
try {
    $powerCapabilities = @(& powercfg.exe /a 2>&1 | ForEach-Object { [string]$_ })
}
catch {
    Add-CollectionNote "Power-capability details were unavailable. $($_.Exception.Message)"
}

$pendingRestart = Test-PendingRestart
if ($pendingRestart.Pending) {
    Add-InventoryWarning "Windows reports a pending restart: $($pendingRestart.Reasons -join ', '). Resolve this before the combined soak."
}

$qlcExecutable = Join-Path $QlcRoot 'qlcplus5.exe'
$installedPlugin = Join-Path $QlcRoot 'Plugins\soundswitch.dll'
$packageWorkspace = Join-Path $PackageRoot 'IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'
$packageProfile = Join-Path $PackageRoot 'SoundSwitch-Control-One-Performance.qxi'
$packagePlugin = Join-Path $PackageRoot 'soundswitch.dll'
$packageValidator = Join-Path $PackageRoot 'Test-V24Package.ps1'
$packageInstaller = Join-Path $PackageRoot 'Install-SoundSwitchPlugin.ps1'
$packageRollback = Join-Path $PackageRoot 'Rollback-SoundSwitchPlugin.ps1'

$qlcArtifacts = @(
    Get-FileEvidence -Label 'Pinned QLC+ core' -Path $qlcExecutable -ExpectedSha256 $ExpectedCoreSha256
    Get-FileEvidence -Label 'Installed SoundSwitch plug-in' -Path $installedPlugin -ExpectedSha256 $ExpectedPluginSha256
    Get-FileEvidence -Label 'Active V24 workspace' -Path $WorkspacePath -ExpectedSha256 $ExpectedWorkspaceSha256
    Get-FileEvidence -Label 'Packaged V24 workspace' -Path $packageWorkspace -ExpectedSha256 $ExpectedWorkspaceSha256
    Get-FileEvidence -Label 'Packaged Control One profile' -Path $packageProfile
    Get-FileEvidence -Label 'Packaged SoundSwitch plug-in' -Path $packagePlugin -ExpectedSha256 $ExpectedPluginSha256
    Get-FileEvidence -Label 'V24 package validator' -Path $packageValidator
    Get-FileEvidence -Label 'V24 plug-in installer' -Path $packageInstaller
    Get-FileEvidence -Label 'V24 plug-in rollback helper' -Path $packageRollback
)

$qlcProcesses = @()
try {
    $qlcProcesses = @(
        Get-Process -Name qlcplus5 -ErrorAction SilentlyContinue |
            ForEach-Object {
                $processStartTime = $null
                $processPath = $null
                try {
                    $processStartTime = $_.StartTime.ToString('o')
                }
                catch {
                    $processStartTime = '[unavailable without additional access]'
                }
                try {
                    $processPath = $_.Path
                }
                catch {
                    $processPath = '[unavailable without additional access]'
                }

                [pscustomobject]@{
                    ProcessId = $_.Id
                    StartTime = $processStartTime
                    Path = $processPath
                }
            }
    )
}
catch {
    Add-CollectionNote "QLC+ process details were unavailable. $($_.Exception.Message)"
}
if ($qlcProcesses.Count -gt 1) {
    Add-InventoryWarning 'More than one QLC+ process is running. Production startup must guard against duplicate instances.'
}

$scheduledTask = $null
try {
    $task = Get-ScheduledTask -TaskName $ExpectedTaskName -ErrorAction Stop
    $scheduledTask = [pscustomobject]@{
        Present = $true
        TaskName = $task.TaskName
        State = [string]$task.State
    }
}
catch {
    $scheduledTask = [pscustomobject]@{
        Present = $false
        TaskName = $ExpectedTaskName
        State = 'Not installed or unavailable'
    }
}

$listeners = @()
try {
    $listeners = @(
        Get-NetTCPConnection -State Listen -ErrorAction Stop |
            Where-Object LocalPort -in 9996, 9999 |
            Sort-Object LocalPort |
            Select-Object LocalAddress, LocalPort, OwningProcess
    )
}
catch {
    Add-CollectionNote "QLC+ listener details were unavailable. $($_.Exception.Message)"
}

$report = [ordered]@{
    SchemaVersion = 1
    QualificationClaim = 'Inventory captured; no operational qualification is implied'
    CapturedAtLocal = $captureTimestamp.ToString('o')
    CapturedAtUtc = $captureTimestamp.ToUniversalTime().ToString('o')
    CaptureRoot = $captureRoot
    MachineRole = $MachineRole
    ComputerName = $safeComputerName
    RanAsAdministrator = $isAdministrator
    Privacy = [ordered]@{
        HardwareSerialNumbersIncluded = $false
        DeviceInstanceIdsIncluded = $false
        MacAddressesIncluded = [bool]$IncludeMacAddresses
        PublicRepositorySafe = $false
        Handling = 'Private qualification evidence. Review before sharing; never commit credentials, router exports, SIM/IMEI data, serials, MAC addresses, or client content.'
    }
    Computer = [ordered]@{
        Manufacturer = if ($computerSystem) { $computerSystem.Manufacturer } else { $null }
        Model = if ($computerSystem) { $computerSystem.Model } else { $null }
        SystemType = if ($computerSystem) { $computerSystem.SystemType } else { $null }
        InstalledRamGB = if ($computerSystem) { ConvertTo-GB $computerSystem.TotalPhysicalMemory } else { $null }
    }
    Windows = [ordered]@{
        ProductName = $windowsProductName
        DisplayVersion = $windowsDisplayVersion
        ReleaseId = $windowsReleaseId
        Build = $windowsBuild
        Architecture = if ($operatingSystem) { $operatingSystem.OSArchitecture } else { $null }
        LastBootTime = if ($operatingSystem) { $operatingSystem.LastBootUpTime.ToString('o') } else { $null }
        PendingRestart = $pendingRestart
    }
    Processor = [ordered]@{
        Name = if ($processor) { ([string]$processor.Name).Trim() } else { $null }
        Cores = if ($processor) { $processor.NumberOfCores } else { $null }
        LogicalProcessors = if ($processor) { $processor.NumberOfLogicalProcessors } else { $null }
    }
    Storage = [ordered]@{
        MinimumFreeGB = $MinimumFreeGB
        LogicalDisks = $logicalDisks
        PhysicalDisks = $physicalDisks
    }
    Network = [ordered]@{
        ExpectedGateway = $ExpectedGateway
        ExpectedAddressPrefix = $ExpectedAddressPrefix
        Adapters = $networkAdapters
    }
    UsbControllers = $usbControllers
    RelevantPresentDevices = $relevantDevices
    PowerCapabilities = $powerCapabilities
    Qlc = [ordered]@{
        ExpectedVersion = '5.3.0 GIT a124abe'
        QlcRoot = $QlcRoot
        WorkspacePath = $WorkspacePath
        PackageRoot = $PackageRoot
        Artifacts = $qlcArtifacts
        RunningProcesses = $qlcProcesses
        ScheduledTask = $scheduledTask
        Listeners = $listeners
    }
    ManualFactsRequired = @(
        'ReadyNet model exactly as printed on its label (LTE520 versus LTE520S)',
        'ReadyNet hardware revision and current firmware version from its administration UI',
        'ReadyNet current LAN subnet, DHCP range, and private reservation plan',
        'Physical USB-port map for Control One, Micro, REV7 experiment, and spare ports',
        'Power-supply rating and boot-after-power-loss behavior',
        'Control One/Micro driver and Device Manager observations',
        'Location of the untouched DJ-laptop QLC+ rollback',
        'Operator observation of storage health beyond the Windows status field'
    )
    Warnings = @($warnings)
    CollectionNotes = @($collectionNotes)
}

$jsonPath = Join-Path $captureRoot 'inventory.json'
$summaryPath = Join-Path $captureRoot 'INVENTORY_SUMMARY.md'
$manualPath = Join-Path $captureRoot 'MANUAL_OBSERVATIONS.md'
$hashPath = Join-Path $captureRoot 'SHA256SUMS.txt'

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$summaryLines = [Collections.Generic.List[string]]::new()
$summaryLines.Add('# LLE Booth Machine Inventory Summary')
$summaryLines.Add('')
$summaryLines.Add('Status: **inventory captured only — this does not qualify the machine for a gig**')
$summaryLines.Add('')
$summaryLines.Add(('- Role: `{0}`' -f (ConvertTo-MarkdownCell $MachineRole)))
$summaryLines.Add(('- Computer: `{0}`' -f (ConvertTo-MarkdownCell $safeComputerName)))
$summaryLines.Add(('- Captured: `{0}`' -f (ConvertTo-MarkdownCell ($captureTimestamp.ToString('o')))))
$summaryLines.Add(('- Ran elevated: `{0}`' -f (ConvertTo-MarkdownCell $isAdministrator)))
$summaryLines.Add('- JSON record: `inventory.json`')
$summaryLines.Add('- Manual facts still required: `MANUAL_OBSERVATIONS.md`')
$summaryLines.Add('')
$summaryLines.Add('## What this capture is for')
$summaryLines.Add('')
$summaryLines.Add('This is the pre-change evidence record for the DJ laptop or dedicated Booth Node. It records machine, Windows, storage, wired-network, USB/device, power-capability, and pinned QLC+ file facts without changing Windows, networking, drivers, firmware, firewall rules, power settings, tasks, QLC+, or the ReadyNet.')
$summaryLines.Add('')
$summaryLines.Add('It intentionally omits hardware serial numbers and device instance IDs. MAC addresses are omitted unless the private-evidence switch was explicitly used.')
$summaryLines.Add('')
$summaryLines.Add('## Machine')
$summaryLines.Add('')
$summaryLines.Add('| Item | Observed value |')
$summaryLines.Add('|---|---|')
$summaryLines.Add("| Manufacturer/model | $(ConvertTo-MarkdownCell $report.Computer.Manufacturer) / $(ConvertTo-MarkdownCell $report.Computer.Model) |")
$summaryLines.Add("| Windows | $(ConvertTo-MarkdownCell $report.Windows.ProductName) $(ConvertTo-MarkdownCell $report.Windows.DisplayVersion), build $(ConvertTo-MarkdownCell $report.Windows.Build) |")
$summaryLines.Add("| Processor | $(ConvertTo-MarkdownCell $report.Processor.Name) |")
$summaryLines.Add("| RAM | $(ConvertTo-MarkdownCell $report.Computer.InstalledRamGB) GB |")
$summaryLines.Add("| Pending restart | $(ConvertTo-MarkdownCell $report.Windows.PendingRestart.Pending) |")
$summaryLines.Add('')
$summaryLines.Add('## Fixed-disk capacity')
$summaryLines.Add('')
$summaryLines.Add('| Drive | File system | Size GB | Free GB | Free % | Meets floor |')
$summaryLines.Add('|---|---:|---:|---:|---:|---|')
foreach ($disk in $logicalDisks) {
    $summaryLines.Add("| $(ConvertTo-MarkdownCell $disk.Drive) | $(ConvertTo-MarkdownCell $disk.FileSystem) | $(ConvertTo-MarkdownCell $disk.SizeGB) | $(ConvertTo-MarkdownCell $disk.FreeGB) | $(ConvertTo-MarkdownCell $disk.FreePercent) | $(ConvertTo-MarkdownCell $disk.MeetsMinimumFreeSpace) |")
}
if ($logicalDisks.Count -eq 0) {
    $summaryLines.Add('| — | — | — | — | — | No data collected |')
}
$summaryLines.Add('')
$summaryLines.Add('## Network adapters')
$summaryLines.Add('')
$summaryLines.Add('| Adapter | Status | Wired candidate | IPv4 | DHCP server | Gateway | Profile | Link |')
$summaryLines.Add('|---|---|---|---|---|---|---|---|')
foreach ($adapter in $networkAdapters) {
    $summaryLines.Add("| $(ConvertTo-MarkdownCell $adapter.Name) | $(ConvertTo-MarkdownCell $adapter.Status) | $(ConvertTo-MarkdownCell $adapter.WiredCandidate) | $(ConvertTo-MarkdownCell ($adapter.IPv4Addresses -join ', ')) | $(ConvertTo-MarkdownCell $adapter.DhcpServer) | $(ConvertTo-MarkdownCell ($adapter.DefaultGateways -join ', ')) | $(ConvertTo-MarkdownCell $adapter.NetworkProfile) | $(ConvertTo-MarkdownCell $adapter.LinkSpeed) |")
}
if ($networkAdapters.Count -eq 0) {
    $summaryLines.Add('| — | — | — | — | — | — | — | No data collected |')
}
$summaryLines.Add('')
$summaryLines.Add('## Pinned QLC+ evidence')
$summaryLines.Add('')
$summaryLines.Add('| Artifact | Exists | Hash matches pinned value | Path |')
$summaryLines.Add('|---|---|---|---|')
foreach ($artifact in $qlcArtifacts) {
    $hashState = if ($null -eq $artifact.HashMatches) { 'not pinned/check presence only' } else { [string]$artifact.HashMatches }
    $summaryLines.Add(('| {0} | {1} | {2} | `{3}` |' -f (ConvertTo-MarkdownCell $artifact.Label), (ConvertTo-MarkdownCell $artifact.Exists), (ConvertTo-MarkdownCell $hashState), (ConvertTo-MarkdownCell $artifact.Path)))
}
$summaryLines.Add('')
$summaryLines.Add('## Warnings to resolve')
$summaryLines.Add('')
if ($warnings.Count -eq 0) {
    $summaryLines.Add('- No automated warning was raised. Manual observations and every physical qualification gate are still required.')
}
else {
    foreach ($warning in $warnings) {
        $summaryLines.Add("- $warning")
    }
}
$summaryLines.Add('')
$summaryLines.Add('## Collection limitations')
$summaryLines.Add('')
if ($collectionNotes.Count -eq 0) {
    $summaryLines.Add('- The requested Windows data sources responded. This still does not prove physical DMX, MIDI feedback, OS2L timing, router routing, headless startup, recording, or recovery.')
}
else {
    foreach ($note in $collectionNotes) {
        $summaryLines.Add("- $note")
    }
}
$summaryLines.Add('')
$summaryLines.Add('## Next action')
$summaryLines.Add('')
$summaryLines.Add('1. Complete `MANUAL_OBSERVATIONS.md`, especially the exact ReadyNet label and firmware fields.')
$summaryLines.Add('2. Keep this folder in the private qualification record; do not commit it to GitHub.')
$summaryLines.Add('3. Follow `docs/booth-node/01_WINDOWS_DEPLOYMENT_RUNBOOK.md` in order.')
$summaryLines.Add('4. After ReadyNet reservations are configured, rerun this tool with the expected gateway and address prefix and compare the two captures.')
$summaryLines.Add('5. Do not promote the Booth Node until every required gate in `04_VALIDATION_RECOVERY_AND_ROLLBACK.md` passes.')

$summaryLines | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$manualTemplate = @'
# LLE Booth Hardware — Manual Observations

Status: **must be completed by the owner/technician; automated inventory cannot establish these facts**

Keep this file in private qualification storage. Do not commit router credentials, configuration exports, photographs of device labels, IMEI/SIM data, hardware serial numbers, MAC addresses, or client information to GitHub.

## ReadyNet identity — stop before firmware work until complete

- Exact model printed on bottom label (LTE520 or LTE520S):
- Hardware revision printed on label/admin page:
- Current firmware version from administration UI:
- Current router LAN address:
- Current subnet mask:
- Current DHCP range:
- Current operating mode (routed/NAT, repeater, bridge, other):
- Private location of configuration backup/export:
- Private location of label photograph:
- Venue Ethernet WAN tested (date/result):
- LTE tested (date/result):
- No-Internet local-LAN test (date/result):
- Wi-Fi-as-WAN routing test (leave deferred until proven):

Do not flash firmware based on appearance or a similar model name. Match the exact model and hardware revision first.

## Address reservations

Record MAC addresses only in the private router record, not in Git.

| Device | Intended address | Reservation created | Reboot verified | Notes |
|---|---:|---|---|---|
| ReadyNet gateway | 10.52.0.1 | n/a | [ ] | |
| DJ laptop | 10.52.0.10 | [ ] | [ ] | |
| backup DJ laptop | 10.52.0.11 | [ ] | [ ] | future |
| Booth Node | 10.52.0.20 | [ ] | [ ] | |

If a different stable private subnet is retained, replace every example consistently.

## Computer and power

- Computer make/model confirmed against automated result:
- Windows edition/version/build confirmed with `winver`:
- Power-adapter voltage/amperage/wattage:
- Boot after AC-power loss behavior:
- Intended UPS model/runtime, if present:
- Ventilation/fan condition:
- Physical storage-health check performed and tool/result:
- Windows Update pending/restart state resolved:

## Physical USB-port map

Qualify direct ports before considering a hub, dock, extender, or long cable.

| Device | Computer port/side/label | Cable label | Direct connection proven | Hot-plug proven | Notes |
|---|---|---|---|---|---|
| Control One | | | [ ] | [ ] | |
| SoundSwitch Micro | | | [ ] | [ ] | |
| REV7 secondary USB experiment | | | [ ] | [ ] | deferred |
| Spare critical USB port | | | [ ] | n/a | |

## Driver/device observations

- SoundSwitch manufacturer software/driver version:
- SoundSwitch application fully closed during QLC+ use:
- Control One present in Device Manager without warning:
- Control One MIDI interface remains present:
- Micro present in Device Manager without warning:
- No whole-device generic WinUSB replacement performed on Control One:

## QLC+ and rollback locations

- Source location of complete coherent DJ-laptop QLC+ folder:
- Private location of untouched DJ-laptop rollback:
- Private location of V22 rollback:
- Private location of V24 package:
- Private location of plug-in installer receipt/backup:
- Recovery USB drive prepared and labeled:

## Cable and connection record

- DJ-to-ReadyNet Ethernet cable label/length/test result:
- Booth-to-ReadyNet Ethernet cable label/length/test result:
- Spare Ethernet cable tested:
- Control One USB cable and spare tested:
- Micro USB cable and spare tested:
- DMX path/test fixture used:

## Owner sign-off for inventory milestone only

- [ ] Automated inventory reviewed.
- [ ] Manual observations completed.
- [ ] Sensitive identifiers are stored privately and not in Git.
- [ ] Exact ReadyNet model/firmware is known.
- [ ] Current DJ-laptop lighting rollback remains untouched.
- [ ] No claim beyond “inventory captured” is being made yet.

Completed by:

Completed at:

Notes:
'@

$manualTemplate | Set-Content -LiteralPath $manualPath -Encoding UTF8

$hashLines = foreach ($filePath in @($jsonPath, $summaryPath, $manualPath)) {
    $fileHash = (Get-FileHash -LiteralPath $filePath -Algorithm SHA256).Hash.ToUpperInvariant()
    '{0}  {1}' -f $fileHash, (Split-Path -Leaf $filePath)
}
$hashLines | Set-Content -LiteralPath $hashPath -Encoding ASCII

Write-Host ''
Write-Host 'LLE Booth inventory captured.'
Write-Host "Evidence folder: $captureRoot"
Write-Host "Summary:         $summaryPath"
Write-Host "Manual worksheet: $manualPath"
Write-Host ''
Write-Host 'This script did not change networking, drivers, firmware, firewall rules, power settings, scheduled tasks, QLC+, or the ReadyNet.'
Write-Host 'The result is private evidence and does not qualify the system for a gig.'

if ($warnings.Count -gt 0) {
    Write-Host ''
    Write-Warning "$($warnings.Count) warning(s) were recorded. Review INVENTORY_SUMMARY.md before continuing."
}

[pscustomobject]@{
    CaptureRoot = $captureRoot
    SummaryPath = $summaryPath
    JsonPath = $jsonPath
    ManualObservationsPath = $manualPath
    HashPath = $hashPath
    WarningCount = $warnings.Count
}
