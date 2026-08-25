# Project Instructions for Build Agents

## Current owner directive

EmberLights as a standalone application is archived. The active project is a QLC+ workspace, creative show programming, and the smallest necessary SoundSwitch Micro/Control One hardware integration.

Do not resume the native EmberLights application, its custom lighting engine/UI, old Preview installer line, or its historical issue critical path unless the owner explicitly reopens that work.

At show time, QLC+ is the only lighting application:

```text
VirtualDJ -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- SoundSwitch Hardware plug-in --> Micro or Control One DMX
```

## Read first

1. `docs/00_START_HERE.md`
2. `docs/qlcplus-control-one/PROJECT_STATUS_AND_ROADMAP.md`
3. `docs/qlcplus-control-one/CONTROL_ONE_WORKFLOW_SPEC.md`
4. `docs/qlcplus-control-one/STATE_MODEL_AND_ARCHITECTURE.md`
5. `docs/qlcplus-control-one/VALIDATION_AND_MAINTENANCE.md`
6. For ReadyNet, separate-computer, headless, network OS2L, recording, or Booth hardware work: `docs/booth-node/00_START_HERE.md`, `07_OWNER_SYSTEM_GUIDE.md`, and the ordered documents they link.

Historical handoffs and issues remain useful provenance, but these current QLC+ documents supersede their standalone-app sequencing whenever they differ.

## Current rig

- Four Both Lighting IR-4 fixtures, 10-channel mode, Universe 1 addresses 1, 11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, Universe 1 addresses 175, 215, 255, and 295.
- Each BO-TUBE192 40-channel fixture has eight RGBWY zones and no master dimmer/effect channel.
- Private duplicate fixtures on QLC+ Universe 3 support full-frame Priority Looks.

## Workspace rules

- The current alpha-candidate release is `releases/qlcplus-control-one/v24/IR4-TUBES-CONTROL-ONE-V24-RUNTIME-FEEDBACK.qxw`. V23 is the Live Console rollback, V22 the unified creative rollback, V21 the reliability rollback, and V20 the protected creative baseline.
- Preserve public Function IDs and logical channels used by Control One, OS2L, and the Virtual Console.
- Resolve Scene fixture IDs from the workspace; do not infer them from names or addresses.
- Back up before edits. Validate XML, fixture patch/modes, Function references, and ID uniqueness afterward.
- For creative-only work, leave the Virtual Console/control layer unchanged.
- For UI/control-only work, prove creative Functions are unchanged.
- Prefer small, physically testable creative passes over bulk replacement.
- Do not put personal paths, usernames, hardware serials, tokens, or secrets in published files.

## Plug-in rules

- Keep custom code limited to SoundSwitch USB transport, Control One MIDI translation/feedback, reconnect, and behavior QLC+ cannot cleanly express.
- Do not add a bridge, daemon, second runtime application, firmware replacement, or new lighting engine.
- Treat the current DLL as build-matched, not ABI-stable across QLC+/Qt versions.
- V24 contains the unified Surface/Priority feedback plug-in built against QLC+ commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`. Preserve and update the complete compatibility tuple, package hashes, installer receipt, rollback path, and release validator for later releases.
- Before calling a release gig-qualified, qualify Micro, Control One DMX 1/2 together, simultaneous MIDI/feedback, repeated hot-plug, and the combined DJ workload.
- Move the current hard-coded rig intensity ranges into workspace/configuration before calling the plug-in general-purpose.

## Pinned V24 continuity contract

- Published commit: `ed50f76001866d5e0279dc14011e380d68646104`
- Release tag: `v24`
- Canonical package: `releases/qlcplus-control-one/v24/`
- Workspace SHA-256: `DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF`
- Plug-in DLL SHA-256: `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC`
- Pinned `qlcplus5.exe` SHA-256: `16DFC419BF878AC4802D88684253D12602DBAAAB94579E88FD55519A1FB09533`

V24 fixes on-screen Bank 1–4 selection, two-way Autoloops/Priority mode switching, latched Start Bank/Start All, live current-loop highlighting for manual and automatic playback, mouse operation while Control One is unplugged, and trailing-zero double-dispatch. Chase multiplier is independently `0.25x / 0.5x / 1x / 2x / 4x`; Autoplay dwell is `1 / 2 / 4 / 8 / 16 measures`.

VirtualDJ/OS2L remains the only beat source. Never add a synthetic or second beat generator. QLC+ remains the sole lighting runtime; standalone EmberLights stays archived. Software/runtime validation has passed. Physical lights and Booth-machine qualification remain pending.

## Booth-node deployment rules

The ReadyNet/private-LAN and separate Windows Booth Node work is a deployment layer around V24, not a change in lighting ownership.

- The DJ laptop and Booth Node use wired Ethernet for show control.
- The Booth Node runs QLC+; Control One and the selected SoundSwitch DMX hardware connect directly to it.
- Normal operation is headless and uses QLC+ authenticated built-in web interface from the DJ laptop.
- Keep the show LAN routed/NAT, stable, private, and independent of venue DHCP or Internet availability. Reject a repeater/bridge mode that renumbers local devices.
- Never expose QLC+ web control to WAN/LTE or place guests on the show segment.
- Prepared scripts under `tools/booth-node/` are one-shot inventory, startup, preflight, and verified-copy helpers. Do not turn them into a second runtime controller or watchdog without explicit owner approval.
- The primary event recording writes to the DJ laptop local SSD through the REV7 record return; a completed file may be copied to the Booth Node only after SHA-256 verification. Do not make the only recording depend on SMB or live network audio.
- Preserve the DJ-laptop QLC+ setup as a rehearsed rollback until the Booth Node passes all documented headless, fault-recovery, combined-soak, and pilot gates.
- Do not call the Booth system gig-qualified before `docs/booth-node/04_VALIDATION_RECOVERY_AND_ROLLBACK.md` passes for the exact hardware topology.

## Booth scope boundary

The Booth project includes only the dedicated lighting appliance, private ReadyNet show LAN, wired OS2L/browser control, Control One/Micro integration, recording/verified-copy workflow, recovery, later simple emergency playback, and minimal Booth health evidence.

Do not add, stage, document as Booth backlog, or make the Booth Node depend on:

- Twenty, CRM, Portal, or business-management infrastructure;
- event cache/synchronization or client-data authority;
- surveys, quizzes, forms, requests, or guest-facing application flows;
- Guestbook Hotline, telephony, voice-request processing, EmberShare, guest Wi-Fi, or captive portals;
- staff chat/PTT/voice systems;
- feedback-suppression DSP;
- cross-system orchestration;
- general-purpose containers, databases, or service hosting.

If the owner reopens one of those ideas, route it to its own project/plan/hardware decision. Do not let it enter this repository branch through “future idea” documentation.

## Evidence and continuity rules

- Run `tools/booth-node/Get-LLEBoothInventory.ps1` before physical-machine changes and at documented checkpoints.
- Generated machine inventories, router exports, label photographs, MAC addresses, device identifiers, credentials, client recordings, and qualification folders remain private; never commit them.
- Update the relevant runbook, `05_BACKLOG_AND_DECISIONS.md`, validation gates, compatibility tuple/hashes, PR description/status, and rollback record in the same change whenever behavior or scope changes.
- A future agent must distinguish repository evidence from physical evidence and must not infer hardware success from scripts or documentation.
- Keep PR #100 draft until the exact machine/topology passes the documented gates.

## Claim boundaries

Use precise labels:

- **Configured:** settings/files are present.
- **Structurally validated:** XML/references/IDs pass automated checks.
- **Software-tested:** deterministic plug-in/workspace tests passed.
- **Physical-output-tested:** the named device/port/fixture visibly responded.
- **Headless-qualified:** cold/warm boot and normal browser operation passed without a Booth display.
- **Combined-soak-qualified:** the exact intended DJ/network/USB/DMX workload passed the defined duration.
- **Gig-qualified:** soak, fault recovery, audio/OS2L/MIDI/DMX, and operator workflow passed.

V24 is structurally validated and software-tested against its pinned QLC+ build. Its isolated runtime check covers Bank selection, both mode directions, chase speed, latched Start Bank/All, parent progression, and the live indicator following the current raw Chaser. Preceding baselines have physical evidence for Micro, each Control One DMX port independently, Control One MIDI/feedback, OS2L, and core pad/Priority Look behavior. V24 still needs the short fixture observation. Repeated hot-plug/LED restoration, simultaneous ports, and the combined two-hour workload remain pending.

The old `.github/workflows/native-core.yml` workflow is manual-only and its release job is disabled. Do not re-enable it for QLC+ tags. `.github/workflows/qlcplus-v21.yml` is historical and manual-only because this repository failed Actions jobs before runner allocation. V24 is validated locally and published directly; do not add or trigger a V24 GitHub Actions workflow unless the owner explicitly changes this rule.
