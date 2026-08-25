# LLE Booth Node — Windows Deployment Runbook

This is the hardware-day sequence for turning the second Windows computer into the dedicated QLC+ booth node. Complete it in order. Do not skip directly to autostart or remove lighting from the DJ laptop until the manual tests pass.

## 1. Bench setup

For the first session, place the following together where they can remain powered for several hours:

- ReadyNet LTE520/LTE520S currently intended for the booth;
- DJ laptop;
- booth computer with a temporary monitor, keyboard, and mouse;
- one known-good Ethernet cable from the DJ laptop to a ReadyNet LAN port;
- one known-good Ethernet cable from the booth computer to a ReadyNet LAN port;
- Control One and its normal USB cable;
- SoundSwitch Micro and its normal USB cable;
- one safe test fixture/DMX path;
- the complete working QLC+ folder from the DJ laptop;
- the complete V24 release folder;
- a separate copy of V22 and the current known-good DJ-laptop workspace.

Use direct USB ports during qualification. Do not introduce an unqualified hub, extender, dock, or long USB run yet.

## 2. Capture both computers before changing them

Use the prepared read-only inventory helper rather than maintaining an ad hoc command transcript. It records machine, Windows, storage, wired-network, USB/device, power, QLC+ artifact, task, and listener facts; creates an owner-readable summary and manual worksheet; and changes no Windows or ReadyNet setting.

From an elevated PowerShell window on the Booth Node, run from the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole BoothNode `
  -OutputRoot 'C:\LLE\Inventory'
```

Open the generated timestamped folder and read:

- `INVENTORY_SUMMARY.md`;
- `MANUAL_OBSERVATIONS.md`;
- `inventory.json`;
- `SHA256SUMS.txt`.

Complete the manual worksheet before router firmware, driver, network, or QLC+ changes. In particular, physically identify the ReadyNet model as printed on its label, then confirm hardware revision and firmware in its administration UI. Software cannot safely infer LTE520 versus LTE520S.

Run the same helper on the DJ laptop with `-MachineRole DJLaptop` and its actual QLC+/workspace/package paths. That capture documents the rollback that must remain intact.

After ReadyNet reservations are configured, rerun the Booth capture with the actual expected network facts. For the recommended clean subnet:

```powershell
.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole BoothNode `
  -ExpectedGateway '10.52.0.1' `
  -ExpectedAddressPrefix '10.52.0.'
```

MAC addresses are omitted by default. If a private reservation record requires them, use `-IncludeMacAddresses` and keep that capture out of GitHub and public/shared folders.

Do not publish generated inventories, router exports/credentials, label photographs, hardware serials, IMEI/SIM identifiers, MAC addresses, client content, or personal paths. Full field-by-field instructions are in `06_HARDWARE_INVENTORY_AND_EVIDENCE.md`.

## 3. Prepare durable folders

Create a simple machine-local layout:

```powershell
$Folders = @(
  'C:\LLE\Packages',
  'C:\LLE\QLC',
  'C:\LLE\Projects',
  'C:\LLE\Recordings',
  'C:\LLE\Logs',
  'C:\LLE\Recovery',
  'C:\ProgramData\LLEBooth'
)
$Folders | ForEach-Object {
  New-Item -ItemType Directory -Path $_ -Force | Out-Null
}
```

Copy, do not move:

- the complete pinned QLC+ installation into `C:\LLE\QLC\5.3.0-GIT-a124abe\`;
- the complete V24 release folder into `C:\LLE\Packages\qlcplus-control-one-v24\`;
- the V24 `.qxw` into `C:\LLE\Projects\`;
- V22 and the current known-good fallback package into `C:\LLE\Recovery\`.

**Copy the entire coherent QLC+ folder.** Do not collect `qlcplus5.exe`, Qt DLLs, FFmpeg/runtime DLLs, and plug-ins from different installations.

## 4. Verify the pinned QLC+ core

Run:

```powershell
$QlcRoot = 'C:\LLE\QLC\5.3.0-GIT-a124abe'
$Core = Join-Path $QlcRoot 'qlcplus5.exe'
Get-FileHash -LiteralPath $Core -Algorithm SHA256
```

Required SHA-256:

```text
16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533
```

Stop if it differs. Do not use `-AllowCompatibleCore` merely to get past the check. A different core requires a separately rebuilt and qualified plug-in.

Launch `qlcplus5.exe` manually once. Confirm that the interface text is normal, the application opens without missing-DLL errors, and the version identifies itself as `5.3.0 GIT a124abe`. Close QLC+.

## 5. Install the normal SoundSwitch device support

Install the normal manufacturer driver/software required for the Micro and/or Control One. Then:

1. connect the device;
2. confirm Windows recognizes it;
3. close SoundSwitch completely;
4. confirm no SoundSwitch background process is holding the hardware;
5. do **not** replace the whole Control One composite USB driver with a generic WinUSB driver, because doing so can remove its MIDI interface.

The custom QLC+ plug-in handles the supported USB transport. SoundSwitch itself must not run at show time.

## 6. Validate and install the V24 package

Open PowerShell in:

```text
C:\LLE\Packages\qlcplus-control-one-v24
```

Run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\Test-V24Package.ps1
```

The package test must pass. With QLC+ closed, install the plug-in:

```powershell
.\Install-SoundSwitchPlugin.ps1 `
  -QlcRoot 'C:\LLE\QLC\5.3.0-GIT-a124abe'
```

Record the displayed rollback-backup directory in:

```text
C:\LLE\Recovery\CURRENT_PLUGIN_BACKUP.txt
```

Verify the installed plug-in:

```powershell
Get-FileHash `
  'C:\LLE\QLC\5.3.0-GIT-a124abe\Plugins\soundswitch.dll' `
  -Algorithm SHA256
```

Required SHA-256:

```text
2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC
```

## 7. Open V24 manually before enabling autostart

Start QLC+ and open:

```text
C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw
```

Confirm the project shows the expected V24 Runtime Feedback console. Do not creatively edit the workspace during deployment qualification.

### Output routing

In QLC+ Input/Output:

- route physical Universe 1 to exactly one first-test output:
  - SoundSwitch Micro; or
  - Control One DMX 1; or
  - Control One DMX 2;
- leave the private Priority Look Universe 3 internal and un-routed;
- confirm no unintended universe is mapped to a physical output.

### Control One MIDI

- connect Control One directly to the booth computer;
- select its MIDI input and feedback/output as required by the current workspace;
- associate `SoundSwitch-Control-One-Performance.qxi` if QLC+ did not retain it;
- verify pads, banks, mode, Play/Pause, Stop, intensity, and LED feedback.

### Five-minute V24 observation

Complete the V24 release test before moving on:

1. With Control One disconnected, use the mouse to select Banks 1–4 and confirm each bank changes on screen.
2. Toggle `AUTOLOOPS ⇄ PRIORITY LOOKS` in both directions and confirm the visible mode follows.
3. Latch `Start Bank` and `Start All`; confirm each remains active until deliberately stopped.
4. During manual and automatic playback, confirm the live highlight follows the loop actually playing.
5. Change chase multiplier through `0.25x / 0.5x / 1x / 2x / 4x` without changing Autoplay dwell.
6. Change Autoplay dwell through `1 / 2 / 4 / 8 / 16 measures` without changing chase multiplier.
7. Apply and release one still and one moving Priority Look; confirm the underlying loop continues.
8. Test one color override and Global/IR-4/tube intensity.
9. Confirm the selected DMX output visibly reaches the fixture.

Trailing-zero double-dispatch is covered by the deterministic V24 software test. Do not invent a second beat source to test timing; VirtualDJ/OS2L remains the only beat source.

If this fails locally, stop. Network OS2L and headless startup cannot repair a local QLC+/hardware failure.

## 8. Configure the ReadyNet show LAN

Use `02_NETWORK_HEADLESS_AND_OS2L.md` as the controlling network plan.

Recommended clean subnet for a newly dedicated booth router:

```text
Router       10.52.0.1
DJ laptop    10.52.0.10  DHCP reservation
Booth node   10.52.0.20  DHCP reservation
DHCP pool    10.52.0.100–10.52.0.199
Mask         255.255.255.0
```

Keeping an already stable private subnet is acceptable. The invariants matter more than these exact numbers:

- ReadyNet remains the DHCP authority for the private show LAN;
- DJ and booth addresses remain constant;
- venue upstream changes do not renumber local devices;
- bridge mode is not used for the production show LAN;
- no guest device shares the private show segment.

After applying reservations, reboot the router, DJ laptop, and booth node. Verify their assigned addresses.

## 9. Manually test QLC+ over the network

With QLC+ open on the booth node, enable OS2L input on the intended QLC+ universe and set its listener port to `9996`.

On the booth node, verify the listener:

```powershell
Get-NetTCPConnection -State Listen |
  Where-Object LocalPort -In 9996,9999 |
  Sort-Object LocalPort
```

The web listener will not appear until QLC+ is launched with web access in the next step.

## 10. First authenticated web launch

Close QLC+. Then run this manually from the booth computer:

```powershell
$Qlc = 'C:\LLE\QLC\5.3.0-GIT-a124abe\qlcplus5.exe'
$Workspace = 'C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw'
$AuthFile = 'C:\ProgramData\LLEBooth\qlc-web-auth'

& $Qlc `
  -3 `
  -w `
  -wp 9999 `
  -wa `
  -a $AuthFile `
  -o $Workspace
```

From the DJ laptop, browse to:

```text
http://10.52.0.20:9999
```

Use the actual reserved booth address if different.

On first setup:

1. open **Configuration**;
2. open **Authorized users**;
3. create one administrator account;
4. create a separate daily operator account restricted to **Only Virtual Console**;
5. store the credentials in the user's password manager, not in Git or a plain-text deployment note;
6. sign out and verify both roles.

QLC+ web authentication is basic HTTP authentication without HTTPS. It is acceptable only on the isolated private booth LAN. Never port-forward port 9999, expose it through LTE/WAN remote administration, or place untrusted guests on this segment.

## 11. Install headless startup and local firewall rules

From the cloned repository root on the booth node, open elevated PowerShell and run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\booth-node\Install-LLEBoothNode.ps1 `
  -QlcRoot 'C:\LLE\QLC\5.3.0-GIT-a124abe' `
  -WorkspacePath 'C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw' `
  -AuthFile 'C:\ProgramData\LLEBooth\qlc-web-auth' `
  -ConfigureFirewall `
  -PreventSleep
```

The helper:

- verifies the pinned core, workspace, and plug-in hashes;
- creates inbound TCP rules for OS2L 9996 and QLC+ web 9999, restricted to `LocalSubnet` and the Windows Private profile;
- writes a local launcher under `C:\ProgramData\LLEBooth\`;
- registers an interactive at-logon task for the current user;
- optionally disables AC sleep/hibernate timeouts without changing display timeout.

It does not configure automatic Windows sign-in because no password should be placed in a script. First qualify the system with a normal manual login. Configure a dedicated local booth account and secure interactive autologon only after the complete reboot test passes.

## 12. Point VirtualDJ at the booth node

On the DJ laptop, in VirtualDJ Options:

```text
os2l = auto
os2lDirectIp = 10.52.0.20:9996
os2lBeatOffset = 0
```

Use the actual reserved booth address. Restart VirtualDJ after changing `os2lDirectIp`.

Update the active keyboard mapper so the existing keepalive remains present:

```xml
<map value="ONINIT" action="os2l_button &apos;QLC KEEPALIVE&apos; &amp; repeat_start &apos;qlc_os2l_keepalive&apos; 5000ms &amp; os2l_button &apos;QLC KEEPALIVE&apos;" name="QLC+ OS2L AUTO-RECONNECT" />
```

`QLC KEEPALIVE` must remain intentionally unmapped in QLC+. It exists only to establish and re-establish the direct connection.

Do not tune `os2lBeatOffset` by guesswork. Start at zero, test with a clear four-on-the-floor track, and adjust only if repeatable visual evidence shows a consistent offset.

## 13. Test from the DJ laptop

Run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\tools\booth-node\Test-LLEBoothConnection.ps1 -BoothAddress 10.52.0.20
```

Required result:

- all pings return;
- no packet loss;
- TCP 9996 is reachable;
- TCP 9999 is reachable;
- the browser returns either the QLC+ page or an expected authentication challenge;
- VirtualDJ beat/BPM activity is visible in QLC+;
- no OS2L reconnect action triggers a lighting Function.

## 14. Cold-boot headless test

Only after manual operation passes:

1. close all programs;
2. shut down the booth computer;
3. disconnect its monitor, keyboard, and mouse;
4. leave ReadyNet, Ethernet, Control One/Micro, and DMX connected;
5. power on the ReadyNet and wait for the LAN to stabilize;
6. power on the booth computer;
7. sign in normally for the first qualification session;
8. from the DJ laptop, wait for the QLC+ web page;
9. verify the V24 workspace, Control One, selected DMX output, and OS2L;
10. reboot the booth computer once from a warm state and repeat.

The booth node is not headless-qualified if any required step still depends on attaching a monitor.

## 15. Preserve the rollback kit

Before any production trial, place these on the DJ laptop and a separate USB drive:

- private pre/post inventory captures and completed manual observations;
- complete V24 release folder;
- V22 workspace/package;
- complete pinned QLC+ folder;
- current known-good DJ-laptop QLC+ setup;
- plug-in installer receipt and backup path;
- ReadyNet configuration backup, with secrets stored separately;
- `04_VALIDATION_RECOVERY_AND_ROLLBACK.md`;
- one spare Ethernet cable and one spare USB cable for each critical device type.

Do not uninstall the working lighting setup from the DJ laptop during booth-node qualification.