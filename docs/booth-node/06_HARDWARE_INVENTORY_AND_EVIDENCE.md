# LLE Booth Node — Hardware Inventory and Evidence Guide

Status: **ready to run when the DJ laptop, Booth Node, and exact ReadyNet are physically together**

## Why this exists

The Booth Node cannot be safely standardized from guesses about the computer, Windows build, network adapter, USB layout, storage, router model, or QLC+ folder. This milestone captures what is actually present before any setting changes.

The prepared inventory helper turns a large collection of Windows commands into one repeatable evidence package. It is deliberately observational:

- it creates an evidence folder;
- it reads Windows, hardware, network, power, device, and QLC+ file facts;
- it computes file hashes where relevant;
- it creates an owner-readable summary and manual worksheet;
- it does **not** change networking, drivers, firmware, firewall rules, power settings, scheduled tasks, QLC+, VirtualDJ, or the ReadyNet.

An inventory result never means “ready for a gig.” It supports only the claim **inventory captured**.

## The tool

Repository file:

```text
tools\booth-node\Get-LLEBoothInventory.ps1
```

Default Booth Node paths checked by the tool:

```text
C:\LLE\QLC\5.3.0-GIT-a124abe
C:\LLE\Projects\IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw
C:\LLE\Packages\qlcplus-control-one-v23
```

The tool knows the pinned V23 hashes for:

- `qlcplus5.exe`;
- installed and packaged `soundswitch.dll`;
- the V23 `.qxw` workspace.

It checks presence only for the profile, package test, installer, and rollback helper because those files do not form the runtime compatibility tuple by themselves.

## What the tool records

### Computer and Windows

- manufacturer and model;
- system type;
- Windows product, display version, build, and architecture;
- last boot time;
- pending-restart evidence;
- CPU name, physical cores, and logical processors;
- installed RAM.

Why it matters: Windows edition determines later remote-administration choices, a pending restart can invalidate a soak test, and CPU/RAM facts establish whether the machine has reasonable headroom.

### Storage

- fixed drive letters;
- file system;
- total and free capacity;
- free-space percentage;
- physical drive model, media/interface type, size, and Windows status.

Hardware serial numbers are intentionally omitted. The default free-space floor is 25 GB, matching the initial recording-capacity gate. Windows “Status OK” is not a substitute for a real health review; the manual worksheet records the tool and result used for that check.

### Wired network

- physical adapter name and model;
- link state and negotiated speed;
- IPv4 address;
- DHCP state and DHCP server;
- default gateway and DNS servers;
- Windows network profile;
- whether the adapter appears to be a wired candidate.

Why it matters: the ReadyNet must remain DHCP authority for the private show LAN, the production Ethernet profile must be Private, and venue/WAN changes must not renumber local equipment.

MAC addresses are omitted by default. They can be included only for a private DHCP-reservation record with the explicit switch described below.

### USB and show devices

- USB controller names/manufacturers/status;
- present USB, audio, MIDI, Control One, SoundSwitch, and REV7-friendly device names/status.

Device instance IDs are intentionally omitted because they can contain serial-like identifiers. Software cannot prove which physical side/port a cable used, so the generated manual worksheet includes the port map.

### Power and Booth services

- `powercfg /a` sleep-state capabilities;
- whether QLC+ is running and whether more than one process exists;
- whether the expected at-logon task exists;
- whether TCP 9996/9999 listeners are present;
- pinned QLC+ file presence and hashes.

These observations are snapshots. They do not prove headless startup, hot-plug recovery, physical DMX, Control One LED restoration, OS2L timing, router failover, recording, or gig readiness.

## Before the first run

Have these together on the bench:

- Booth Node with temporary monitor, keyboard, and mouse;
- DJ laptop;
- exact ReadyNet intended for the Booth;
- two tested Ethernet cables;
- Control One and its normal USB cable;
- SoundSwitch Micro and its normal USB cable;
- power supplies;
- repository or copied `tools\booth-node` folder.

Do not update router firmware, replace USB drivers, uninstall the DJ-laptop QLC+ setup, or change the ReadyNet mode before the pre-change capture.

## First run on the Booth Node

Open an elevated Windows PowerShell window. Elevation is recommended because some device/process information may otherwise be incomplete; the script still does not alter system settings.

From the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole BoothNode `
  -OutputRoot 'C:\LLE\Inventory'
```

Expected output folder:

```text
C:\LLE\Inventory\YYYYMMDD-HHMMSS-BoothNode-COMPUTERNAME\
```

The folder name is timestamped so a later capture never silently replaces the earlier baseline.

## Run on the DJ laptop

The DJ-laptop capture proves what must remain available for rollback. Supply its real working QLC+ paths when they are known:

```powershell
.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole DJLaptop `
  -OutputRoot 'C:\LLE\Inventory' `
  -QlcRoot 'D:\ACTUAL\PATH\TO\PINNED-QLC' `
  -WorkspacePath 'D:\ACTUAL\PATH\TO\KNOWN-GOOD.qxw' `
  -PackageRoot 'D:\ACTUAL\PATH\TO\V23-PACKAGE'
```

Do not copy those example paths literally. Browse to the actual files first. A DJ-laptop workspace can intentionally differ from the exact V23 release; record that as rollback evidence rather than using a bypass to call it V23.

## Private MAC-address capture

ReadyNet DHCP reservations require the wired adapter MAC address. Prefer recording it directly in the private ReadyNet administration record. If a private machine report is more convenient:

```powershell
.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole BoothNode `
  -IncludeMacAddresses
```

That run is deliberately marked with a warning. Keep it out of GitHub, public file shares, screenshots, support posts, and client folders.

## Post-network run

After the ReadyNet is confirmed in routed/NAT mode and the reservations are applied, rerun the capture with the expected network facts:

```powershell
.\tools\booth-node\Get-LLEBoothInventory.ps1 `
  -MachineRole BoothNode `
  -ExpectedGateway '10.52.0.1' `
  -ExpectedAddressPrefix '10.52.0.'
```

For the recommended layout, the Booth Node should report `10.52.0.20`, the ReadyNet as DHCP server/default gateway, and a Private Windows network profile. The prefix check is only a warning aid; it does not prove DHCP reservations, guest isolation, NAT behavior, or WAN/LTE failover.

If the owner retains another stable private subnet, use its actual gateway and prefix consistently. Do not force `10.52.0.0/24` merely to make the example pass.

## Every generated file

### `inventory.json`

Machine-readable evidence used for comparisons and future tooling. It contains structured computer, Windows, storage, network, USB/device, power, QLC+, task, listener, warning, and collection-limitation data.

It is private operational evidence, not a repository fixture. Even with serials omitted, it contains the computer name, internal paths, and local topology.

### `INVENTORY_SUMMARY.md`

Owner-readable explanation of the same capture. Start here at the Booth machine. It highlights:

- machine/Windows identity;
- fixed-drive capacity;
- network adapters and current addressing;
- pinned QLC+ file presence/hash state;
- warnings;
- collection limitations;
- the next action.

### `MANUAL_OBSERVATIONS.md`

The worksheet for facts Windows cannot safely infer:

- exact ReadyNet label, hardware revision, firmware, mode, and private config-backup location;
- address reservations;
- power supply and boot-after-power-loss behavior;
- physical USB-port/cable map;
- Device Manager and driver observations;
- rollback locations;
- cable evidence;
- owner sign-off for the inventory milestone only.

Complete every applicable field. “Unknown” is better than an invented value.

### `SHA256SUMS.txt`

Hashes the JSON, summary, and manual worksheet at capture time. After completing the manual worksheet, preserve the original hash file as baseline evidence and create a new qualification-level manifest or note that the worksheet was intentionally edited.

## How to read QLC+ artifact results

| Result | Meaning | Action |
|---|---|---|
| Exists `False` | File was not at the supplied path | Locate/copy the coherent package; do not invent a path |
| Exists `True`, HashMatches `True` | File matches the pinned V23 value | Preserve it and continue to package validation |
| Exists `True`, HashMatches `False` | File differs from pinned V23 | Stop; do not install/promote it as V23 |
| HashMatches blank/not pinned | Presence/hash recorded, but no release hash is asserted by this tool | Use the official package test and release manifest |

The helper does not run the V23 package validator or install the plug-in. Those are separate deliberate steps in the deployment runbook.

## What must stay private

Never commit or publish:

- the generated evidence folder;
- router configuration exports or credentials;
- label photographs;
- IMEI, SIM, ICCID, MAC, or serial numbers;
- Windows product keys;
- QLC+ web credentials;
- client names, recordings, timelines, or event content;
- personal user-profile paths.

Repository documentation may contain sanitized findings such as “LTE520S firmware X passed the routed failover test on YYYY-MM-DD,” but not the private identifiers used to reach that conclusion.

## Comparison workflow

Keep at least these captures:

1. Booth Node before changes;
2. DJ laptop rollback baseline;
3. Booth Node after ReadyNet reservations/private profile;
4. Booth Node after the pinned QLC+ package is installed;
5. Booth Node immediately before the combined soak;
6. Booth Node after any later Windows, driver, QLC+, plug-in, or hardware change.

Compare:

- Windows build/restart state;
- wired adapter, DHCP server, gateway, address, and profile;
- QLC+ hashes;
- disk free space;
- show-device presence;
- scheduled-task/listener state;
- warnings.

The captures explain *what changed*. The qualification log explains whether that change passed physical testing.

## Troubleshooting

### PowerShell says script execution is disabled

Use only the process-scoped bypass shown above. It ends when that PowerShell window closes and does not permanently weaken the machine policy.

### Some fields say unavailable

Rerun from an elevated Windows PowerShell window. If the field remains unavailable, preserve the collection note and record the fact manually; do not fabricate it.

### No wired adapter is detected

Connect a known-good Ethernet cable from the computer directly to a ReadyNet LAN port. Confirm link lights, then rerun. Do not qualify OS2L over Wi-Fi.

### Gateway or prefix warning appears

Check the active adapter, `ipconfig /all`, ReadyNet DHCP authority, reservation, subnet, and Windows network profile. If venue equipment supplied the address, stop—the show LAN is in an unapproved bridged state.

### QLC+ hash mismatch appears

Do not use a compatibility bypass. Restore the complete coherent `5.3.0-GIT-a124abe` folder and exact V23 package, then rerun the package validator and inventory.

### ReadyNet model is still uncertain

Stop router firmware work. Read the physical label and administration page. Similar appearance or a support page for another variant is not evidence.

## Completion gate

Milestone 0 is complete only when:

- both-machine baseline captures exist privately;
- the manual worksheet is complete;
- exact ReadyNet model/hardware revision/firmware are known;
- wired NIC and USB topology are known;
- QLC+ source/rollback locations are known;
- sensitive identifiers are absent from Git;
- the DJ-laptop rollback remains untouched;
- the next claim is still only **inventory captured**.

Then continue with the ordered Windows deployment runbook. Do not skip to headless startup, network OS2L, or production promotion.
