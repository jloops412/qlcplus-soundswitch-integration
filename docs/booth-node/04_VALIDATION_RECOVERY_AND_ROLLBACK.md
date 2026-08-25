# LLE Booth Node — Validation, Recovery, and Rollback

## Claim language

Use only the strongest claim actually supported by evidence:

- **Configured:** settings/files are present.
- **Structurally validated:** hashes, XML, references, and package checks pass.
- **Software-tested:** the named software behavior passes without claiming live hardware output.
- **Physical-output-tested:** the named device/port/fixture visibly responds.
- **Headless-qualified:** cold boot and normal operation succeed without a booth display.
- **Combined-soak-qualified:** the exact DJ/lighting/network workload passes the defined duration.
- **Gig-qualified:** all required gates, fault drills, and a representative rehearsal/pilot pass with a tested rollback.

Do not compress these into “working” when the distinction matters.

## Evidence folder

Create one private qualification folder per booth computer:

```text
C:\LLE\Qualification\YYYY-MM-DD-booth-node
```

Store:

- timestamped Booth/DJ inventory captures plus completed manual observations;
- Windows/driver versions;
- ReadyNet model/firmware and private config backup location;
- QLC+ core/workspace/plug-in hashes;
- V24 package-test output;
- selected QLC+ I/O mappings;
- VirtualDJ OS2L target and mapper evidence;
- ping/port test output;
- fault-drill notes;
- soak start/end time and observations;
- recording test hashes;
- promotion decision.

Do not commit credentials, SIM details, IMEI, serial numbers, client recordings, or private network exports to Git.

## Gate A — Exact package and machine baseline

Canonical V24 identity:

- commit `ed50f76001866d5e0279dc14011e380d68646104`;
- tag `v24`;
- package `releases/qlcplus-control-one/v24/`;
- workspace SHA-256 `DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF`;
- plug-in SHA-256 `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC`;
- `qlcplus5.exe` SHA-256 `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`.


Required:

- [ ] Booth Node and DJ-laptop rollback inventories captured with `Get-LLEBoothInventory.ps1`;
- [ ] exact ReadyNet label model/hardware revision/firmware recorded privately;
- [ ] storage health/free space acceptable;
- [ ] Windows update is not pending a forced restart;
- [ ] wired Ethernet adapter is identified;
- [ ] Windows connection profile for ReadyNet Ethernet is Private;
- [ ] pinned QLC+ folder is complete and coherent;
- [ ] `qlcplus5.exe` hash matches V24;
- [ ] V24 workspace hash matches the release;
- [ ] installed `soundswitch.dll` hash matches V24;
- [ ] `Test-V24Package.ps1` passes;
- [ ] V23 release rollback and the known-good DJ-laptop rollback remain available;
- [ ] no unapproved automatic QLC+/driver update is enabled.

Failure rule: stop and restore a coherent package. Never solve a hash/runtime failure by mixing files.

## Gate B — Local manual QLC+ reproduction

Run on the booth computer with a temporary display before adding network/headless behavior.

Required:

- [ ] V24 opens without errors or corrupted text;
- [ ] exactly one QLC+ process is running;
- [ ] Universe 3 remains internal/un-routed;
- [ ] one chosen physical Universe 1 output reaches a fixture;
- [ ] on-screen Banks 1–4 are selectable with the mouse while Control One is unplugged;
- [ ] same-pad stop and replacement behavior work;
- [ ] Start Bank and Start All latch, advance, and stop only when deliberately released;
- [ ] the live current-loop highlight follows manual and automatic playback;
- [ ] Autoloops/Priority mode switching works in both directions;
- [ ] Autoplay dwell selects `1 / 2 / 4 / 8 / 16 measures` without changing chase multiplier;
- [ ] chase multiplier selects `0.25x / 0.5x / 1x / 2x / 4x` independently;
- [ ] still and moving Priority Looks take sole authority and release cleanly;
- [ ] color override remains sparse;
- [ ] Global, IR-4, and tube intensity work;
- [ ] Control One MIDI input and known LED feedback work;
- [ ] essential mouse controls work without Control One.

Failure rule: fix the local QLC+/workspace/hardware path first. Do not blame or modify the network.

## Gate C — Stable private LAN

Required with venue/LTE Internet disconnected:

- [ ] ReadyNet remains reachable at its LLE gateway address;
- [ ] DJ laptop receives its reserved address;
- [ ] booth node receives its reserved address;
- [ ] DHCP server/default gateway remain the ReadyNet;
- [ ] continuous ping has zero loss for 30 minutes;
- [ ] typical latency remains steady and no repeated spikes exceed the documented warning threshold;
- [ ] QLC+ OS2L TCP 9996 is reachable from the DJ laptop;
- [ ] QLC+ web TCP 9999 is reachable from the DJ laptop;
- [ ] removing all Internet upstreams does not interrupt local traffic.

Failure rule: reject bridge/repeater mode that hands DHCP authority to the venue or depends on upstream availability.

## Gate D — Authenticated headless operation

Required:

- [ ] QLC+ starts from the at-logon task;
- [ ] the exact V24 workspace loads;
- [ ] QLC+ web access is enabled on 9999;
- [ ] an administrator account exists;
- [ ] a separate Virtual-Console-only operator account exists;
- [ ] no QLC+ web port is forwarded to WAN/LTE;
- [ ] the DJ laptop can operate the complete V24 performance console in a browser;
- [ ] a cold boot reaches the browser console without attaching a monitor;
- [ ] a warm reboot repeats successfully;
- [ ] Windows does not sleep on AC power;
- [ ] Ethernet and USB power management do not suspend critical devices;
- [ ] QLC+ does not start twice after login/retry.

Failure rule: remain in attended/manual mode. Do not declare the box headless-ready.

## Gate E — Network OS2L

Required:

- [ ] VirtualDJ uses the reserved booth-node address and port 9996;
- [ ] `os2l = auto`;
- [ ] five-second keepalive/reconnect mapper is active;
- [ ] `QLC KEEPALIVE` is not mapped to a lighting Function;
- [ ] VirtualDJ restart reconnects within the expected retry interval;
- [ ] QLC+ restart reconnects without restarting VirtualDJ;
- [ ] beat/BPM behavior is correct on representative slow, medium, and fast tracks;
- [ ] `os2lBeatOffset` remains zero unless a repeatable measured correction is required;
- [ ] browser operation and Control One remain responsive while OS2L is active.

Failure rule: restore localhost/DJ-laptop QLC+ for production until the direct path is stable.

## Gate F — Hardware recovery matrix

Run each row deliberately and record the observed result.

| Fault/action | Expected safe behavior | Pass evidence |
|---|---|---|
| Start QLC+ before Control One | workspace/browser/mouse remain usable; Control One appears after connection | MIDI and known LEDs recover |
| Start Control One before QLC+ | normal startup | MIDI, feedback, selected output work |
| Unplug/replug Control One once | QLC+ Functions remain authoritative; mouse fallback remains; MIDI/LED recover | no QLC restart required |
| Repeat Control One hot-plug five times | no stale handle/error storm | every reconnect restores operation |
| Unplug/replug Micro | QLC+ remains responsive | output resumes after reconnect |
| Test Control One DMX 1 | visible output | port named in evidence |
| Test Control One DMX 2 | visible output | port named in evidence |
| Test intended simultaneous outputs | no flicker, cross-port corruption, MIDI loss, or USB storm | two-hour gate includes exact topology |
| Disconnect DJ Ethernet | QLC+/Control One continue locally; OS2L unavailable | no QLC crash or unsafe output |
| Reconnect DJ Ethernet | keepalive restores OS2L | no manual QLC restart |
| Reboot ReadyNet | QLC+/Control One continue locally while LAN is absent | browser/OS2L return after LAN recovery |
| Remove WAN/venue Internet | no local impact | LAN/lighting uninterrupted |
| Remove LTE | no local impact | LAN/lighting uninterrupted |
| Switch WAN -> LTE -> no Internet | only Internet state changes | local addresses stay fixed |
| Close/reopen browser | QLC+ continues independently | state reappears correctly |
| Restart QLC+ in a safe bench moment | task/manual launch reloads V24 | hardware and OS2L recover |

Do not fault-inject power or DMX during a real event.

## Gate G — Combined two-hour soak

Use the exact intended show topology:

- VirtualDJ playing representative files;
- normal controller operation and scratching/transitions;
- OS2L active over wired LAN;
- QLC+ V24 running;
- Control One MIDI and LED feedback active;
- intended Micro/Control One DMX output active;
- browser console open on the DJ laptop;
- upstream Internet switching at least once;
- Windows logging/monitoring active but no heavy development tools.

Every 15 minutes record:

- DJ audio dropout/crackle: yes/no;
- QLC+ responsiveness;
- visible DMX flicker/freeze;
- Control One input/LED status;
- current ping loss/latency;
- booth CPU/RAM/disk utilization;
- USB/device errors;
- OS2L connection state.

Pass requirements:

- no audible dropout attributable to the booth architecture;
- no QLC+ crash or UI starvation;
- no visible DMX corruption;
- no repeating USB error storm;
- no frozen Control One MIDI/feedback;
- zero LAN packet loss;
- automatic OS2L recovery after the planned disconnect;
- no forced Windows restart/update;
- no thermal or storage warning.

A single unexplained interruption fails the soak until reproduced and resolved.

## Gate H — Recording validation

### Primary capture

- [ ] REV7 `USB 5/6 Mixer Output` is `MIX(REC OUT)`;
- [ ] VirtualDJ records 24-bit FLAC or WAV to local SSD;
- [ ] music is present in both channels;
- [ ] DJ microphone is present at usable level;
- [ ] every other intended mixer source is present;
- [ ] worst-case test has no clipping;
- [ ] six-hour-capacity storage gate passes;
- [ ] recording continues if ReadyNet is rebooted;
- [ ] recording continues if booth node is shut down;
- [ ] completed file opens and has plausible duration/size.

### Verified booth copy

- [ ] source file is closed before copy;
- [ ] destination share is accessible only with approved credentials;
- [ ] copy completes over wired LAN;
- [ ] source/destination SHA-256 match;
- [ ] JSON verification manifest exists;
- [ ] source remains intact;
- [ ] beginning/microphone/end spot checks pass on the destination copy.

### Optional second USB recorder

Not required for initial promotion. If pursued, it must pass its own two-hour simultaneous USB A/B test without affecting the DJ-laptop controller/audio path.

## Recovery levels

### Level 1 — Browser/control problem only

Symptoms: QLC+ lighting still runs and Control One works, but the browser page is unavailable.

1. Keep operating from Control One.
2. Confirm DJ-to-booth ping.
3. confirm TCP 9999 with `Test-LLEBoothConnection.ps1`;
4. close/reopen the browser tab and reauthenticate;
5. do not restart QLC+ during a critical song merely to restore the web UI;
6. investigate after the program moment.

### Level 2 — OS2L lost, QLC+ still operating

1. Keep lighting in a safe manual Autoloop/Priority Look through Control One or browser.
2. Confirm Ethernet link lights and booth ping.
3. confirm TCP 9996;
4. send/rely on the five-second keepalive;
5. restart VirtualDJ only if operationally safe and the audio path permits it;
6. do not restart QLC+ unless manual lighting is no longer adequate.

### Level 3 — Control One unavailable, QLC+ operating

1. Use the browser V24 console from the DJ laptop.
2. Keep the current safe lighting owner active.
3. reseat one USB connection only when safe;
4. allow plug-in rescan/reconnect;
5. if MIDI/LED do not recover, finish the event with browser/mouse control and diagnose afterward.

### Level 4 — Booth QLC+ failed, booth Windows still reachable

1. Put fixtures in the safest available state before restarting.
2. verify no second QLC+ instance exists;
3. launch the prepared `Start-LLEBoothQLC.ps1` once;
4. confirm V24, output, MIDI, browser, and OS2L;
5. if it does not recover quickly, use the DJ-laptop rollback instead of repeatedly experimenting.

### Level 5 — Booth node failed completely

Prepared DJ-laptop rollback:

1. stop/blackout at an appropriate program moment;
2. move the selected SoundSwitch USB/DMX device and Control One USB to their labeled DJ-laptop ports;
3. open the pinned local QLC+ build and known-good workspace;
4. restore VirtualDJ `os2lDirectIp` to `127.0.0.1:9996`;
5. confirm the local keepalive, Control One, and DMX output;
6. resume with a safe static look/Autoloop before adding complexity.

This procedure must be rehearsed. A theoretical fallback is not a rollback.

## Plug-in rollback

If the V24 plug-in itself must be restored:

1. close QLC+;
2. locate the installer-created receipt/backup recorded during deployment;
3. from the V24 package folder run:

```powershell
.\Rollback-SoundSwitchPlugin.ps1 `
  -QlcRoot 'C:\LLE\QLC\5.3.0-GIT-a124abe'
```

4. verify the restored DLL hash against the receipt;
5. reopen the prior known-good workspace;
6. rerun the fast regression before output.

Do not overwrite/delete the backup until a later release has been separately qualified.

## Router rollback

Before changing ReadyNet mode, LAN subnet, DHCP, repeater, WAN, LTE, firewall, or firmware:

- capture the exact model/firmware;
- export the configuration if available;
- record existing settings privately;
- preserve the current admin path;
- have a factory-reset/reprovision procedure;
- keep one already-working ReadyNet untouched when possible.

Never upgrade every ReadyNet at once. Qualify one booth unit, preserve one rollback unit, then decide whether to standardize.

## First-event promotion

The first real deployment should be a controlled pilot, not the first time the topology is assembled.

Required before departure:

- all Gates A–H required for the chosen scope pass;
- complete rollback kit is in the vehicle;
- DJ-laptop local QLC+ remains installed;
- all cables are labeled;
- router, booth node, and Control One power sequence is known;
- browser bookmark and credentials are available offline;
- arrival/setup plan includes extra qualification time;
- recording is treated as an additional feature, not a reason to delay core show readiness.

After the pilot, capture what actually happened before changing configuration. Promote to gig-qualified only when the representative event and recovery posture support that claim.