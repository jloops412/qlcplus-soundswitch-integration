[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$QlcRoot,

    [Parameter(Mandatory)]
    [string]$WorkspacePath,

    [string]$AuthFile = "$env:ProgramData\LLEBooth\qlc-web-auth",

    [ValidateRange(1, 65535)]
    [int]$WebPort = 9999,

    [ValidateRange(1, 65535)]
    [int]$Os2lPort = 9996,

    [string]$TaskName = 'LLE Booth - QLC+ V23',

    [string]$RunAsUser = "$env:USERDOMAIN\$env:USERNAME",

    [switch]$ConfigureFirewall,
    [switch]$PreventSleep,
    [switch]$Kiosk,
    [switch]$StartNow,
    [switch]$AllowModifiedWorkspace,
    [switch]$AllowCompatibleCore
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExpectedCoreSha256 = '16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533'
$ExpectedPluginSha256 = 'AC6BE24B6B8FA252E0C426D68248F99326B43EC1E2569C7B7EDB15511F2ED54D'
$ExpectedWorkspaceSha256 = 'E953C3483EB09D2E600D32495887B27A03021DC47EF7BA5797552C4F5A21547B'

function Assert-WindowsAdministrator {
    if ($env:OS -ne 'Windows_NT') {
        throw 'This helper supports Windows only.'
    }

    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    $adminRole = [Security.Principal.WindowsBuiltInRole]::Administrator
    if (-not $principal.IsInRole($adminRole)) {
        throw 'Run this script from an elevated PowerShell window.'
    }
}

function Get-NormalizedSha256 {
    param([Parameter(Mandatory)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Escape-SingleQuotedPowerShellString {
    param([Parameter(Mandatory)][string]$Value)
    return $Value.Replace("'", "''")
}

Assert-WindowsAdministrator

$resolvedQlcRoot = (Resolve-Path -LiteralPath $QlcRoot).Path
$resolvedWorkspace = (Resolve-Path -LiteralPath $WorkspacePath).Path
$qlcExecutable = Join-Path $resolvedQlcRoot 'qlcplus5.exe'
$pluginPath = Join-Path $resolvedQlcRoot 'Plugins\soundswitch.dll'

if (-not (Test-Path -LiteralPath $qlcExecutable -PathType Leaf)) {
    throw "Pinned QLC+ executable not found: $qlcExecutable"
}
if (-not (Test-Path -LiteralPath $pluginPath -PathType Leaf)) {
    throw "SoundSwitch plug-in not found: $pluginPath. Run the V23 installer first."
}

$coreHash = Get-NormalizedSha256 -Path $qlcExecutable
if ($coreHash -ne $ExpectedCoreSha256 -and -not $AllowCompatibleCore) {
    throw "QLC+ core hash mismatch. Expected $ExpectedCoreSha256; found $coreHash. Do not bypass this unless the different core and plug-in were deliberately rebuilt and qualified together."
}

$pluginHash = Get-NormalizedSha256 -Path $pluginPath
if ($pluginHash -ne $ExpectedPluginSha256) {
    throw "SoundSwitch plug-in hash mismatch. Expected $ExpectedPluginSha256; found $pluginHash."
}

$workspaceHash = Get-NormalizedSha256 -Path $resolvedWorkspace
if ($workspaceHash -ne $ExpectedWorkspaceSha256 -and -not $AllowModifiedWorkspace) {
    throw "V23 workspace hash mismatch. Expected $ExpectedWorkspaceSha256; found $workspaceHash. Preserve the exact release and run structural validation before using -AllowModifiedWorkspace for an intentional machine-specific copy."
}

$boothRoot = Join-Path $env:ProgramData 'LLEBooth'
$logRoot = Join-Path $boothRoot 'Logs'
$launcherPath = Join-Path $boothRoot 'Start-LLEBoothQLC.ps1'
$authParent = Split-Path -Parent $AuthFile

foreach ($path in @($boothRoot, $logRoot, $authParent)) {
    if ($path -and -not (Test-Path -LiteralPath $path -PathType Container)) {
        if ($PSCmdlet.ShouldProcess($path, 'Create directory')) {
            New-Item -ItemType Directory -Path $path -Force | Out-Null
        }
    }
}

$escapedQlc = Escape-SingleQuotedPowerShellString -Value $qlcExecutable
$escapedWorkspace = Escape-SingleQuotedPowerShellString -Value $resolvedWorkspace
$escapedAuth = Escape-SingleQuotedPowerShellString -Value ([IO.Path]::GetFullPath($AuthFile))
$escapedLogRoot = Escape-SingleQuotedPowerShellString -Value $logRoot
$kioskLiteral = if ($Kiosk) { '$true' } else { '$false' }

$launcherTemplate = @'
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$QlcExecutable = '__QLC_EXECUTABLE__'
$WorkspacePath = '__WORKSPACE_PATH__'
$AuthFile = '__AUTH_FILE__'
$LogRoot = '__LOG_ROOT__'
$WebPort = __WEB_PORT__
$UseKiosk = __USE_KIOSK__

New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null
$LogPath = Join-Path $LogRoot ('qlc-launch-' + (Get-Date -Format 'yyyyMMdd') + '.log')

function Write-LaunchLog {
    param([string]$Message)
    ('{0:o} {1}' -f (Get-Date), $Message) | Add-Content -LiteralPath $LogPath -Encoding UTF8
}

try {
    Start-Sleep -Seconds 15

    $existing = Get-Process -Name 'qlcplus5' -ErrorAction SilentlyContinue
    if ($existing) {
        Write-LaunchLog ('QLC+ already running; no duplicate launch. PID(s): ' + (($existing.Id | Sort-Object) -join ','))
        exit 0
    }

    $quote = [char]34
    $arguments = @(
        '-3',
        '-w',
        '-wp', [string]$WebPort,
        '-wa',
        '-a', ($quote + $AuthFile + $quote),
        '-o', ($quote + $WorkspacePath + $quote)
    )
    if ($UseKiosk) {
        $arguments += '-k'
    }

    Write-LaunchLog ('Starting QLC+: ' + $QlcExecutable)
    $process = Start-Process `
        -FilePath $QlcExecutable `
        -ArgumentList ($arguments -join ' ') `
        -WorkingDirectory (Split-Path -Parent $WorkspacePath) `
        -WindowStyle Minimized `
        -PassThru

    Write-LaunchLog ('QLC+ started with PID ' + $process.Id)
    exit 0
}
catch {
    Write-LaunchLog ('FAILED: ' + $_.Exception.Message)
    throw
}
'@

$launcherContent = $launcherTemplate
$launcherContent = $launcherContent.Replace('__QLC_EXECUTABLE__', $escapedQlc)
$launcherContent = $launcherContent.Replace('__WORKSPACE_PATH__', $escapedWorkspace)
$launcherContent = $launcherContent.Replace('__AUTH_FILE__', $escapedAuth)
$launcherContent = $launcherContent.Replace('__LOG_ROOT__', $escapedLogRoot)
$launcherContent = $launcherContent.Replace('__WEB_PORT__', [string]$WebPort)
$launcherContent = $launcherContent.Replace('__USE_KIOSK__', $kioskLiteral)

if ($PSCmdlet.ShouldProcess($launcherPath, 'Write guarded QLC+ launcher')) {
    Set-Content -LiteralPath $launcherPath -Value $launcherContent -Encoding UTF8
}

if ($ConfigureFirewall) {
    $firewallRules = @(
        @{
            Name = 'LLE Booth - QLC OS2L'
            Port = $Os2lPort
            Description = 'Allow VirtualDJ OS2L to the booth QLC+ listener from the private local subnet.'
        },
        @{
            Name = 'LLE Booth - QLC Web'
            Port = $WebPort
            Description = 'Allow authenticated QLC+ browser control from the private local subnet.'
        }
    )

    foreach ($rule in $firewallRules) {
        $existingRule = Get-NetFirewallRule -DisplayName $rule.Name -ErrorAction SilentlyContinue
        if ($existingRule -and $PSCmdlet.ShouldProcess($rule.Name, 'Replace Windows Firewall rule')) {
            $existingRule | Remove-NetFirewallRule
        }

        if ($PSCmdlet.ShouldProcess($rule.Name, "Create inbound TCP rule on port $($rule.Port)")) {
            New-NetFirewallRule `
                -DisplayName $rule.Name `
                -Description $rule.Description `
                -Direction Inbound `
                -Action Allow `
                -Protocol TCP `
                -LocalPort $rule.Port `
                -RemoteAddress LocalSubnet `
                -Profile Private | Out-Null
        }
    }
}

if ($PreventSleep) {
    if ($PSCmdlet.ShouldProcess('AC power plan', 'Disable system sleep and hibernate timeouts')) {
        & powercfg.exe /change standby-timeout-ac 0
        if ($LASTEXITCODE -ne 0) {
            throw 'powercfg failed while disabling AC standby timeout.'
        }

        & powercfg.exe /change hibernate-timeout-ac 0
        if ($LASTEXITCODE -ne 0) {
            throw 'powercfg failed while disabling AC hibernate timeout.'
        }
    }
}

$powerShellExe = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$taskAction = New-ScheduledTaskAction `
    -Execute $powerShellExe `
    -Argument "-NoLogo -NoProfile -ExecutionPolicy Bypass -File `"$launcherPath`""
$taskTrigger = New-ScheduledTaskTrigger -AtLogOn -User $RunAsUser
$taskPrincipal = New-ScheduledTaskPrincipal `
    -UserId $RunAsUser `
    -LogonType Interactive `
    -RunLevel Highest
$taskSettings = New-ScheduledTaskSettingsSet `
    -StartWhenAvailable `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries

if ($PSCmdlet.ShouldProcess($TaskName, "Register at-logon task for $RunAsUser")) {
    Register-ScheduledTask `
        -TaskName $TaskName `
        -Action $taskAction `
        -Trigger $taskTrigger `
        -Principal $taskPrincipal `
        -Settings $taskSettings `
        -Description 'Starts the pinned LLE QLC+ V23 workspace with authenticated web control after interactive logon.' `
        -Force | Out-Null
}

if ($StartNow -and $PSCmdlet.ShouldProcess($TaskName, 'Start scheduled task now')) {
    Start-ScheduledTask -TaskName $TaskName
}

Write-Host ''
Write-Host 'LLE booth-node startup installed.'
Write-Host "QLC+ core hash:     $coreHash"
Write-Host "SoundSwitch hash:   $pluginHash"
Write-Host "Workspace hash:     $workspaceHash"
Write-Host "Launcher:           $launcherPath"
Write-Host "Scheduled task:     $TaskName"
Write-Host "Run-as user:        $RunAsUser"
Write-Host "QLC+ web port:      $WebPort"
Write-Host "OS2L port:          $Os2lPort"
Write-Host ''
Write-Host 'Next steps:'
Write-Host '1. Confirm the ReadyNet Ethernet profile is Private.'
Write-Host '2. Log off/on or run the task manually.'
Write-Host '3. Create an administrator and a Virtual-Console-only QLC+ web user.'
Write-Host '4. Test from the DJ laptop with Test-LLEBoothConnection.ps1.'
Write-Host '5. Do not enable unattended automatic Windows sign-in until cold-boot qualification passes.'
