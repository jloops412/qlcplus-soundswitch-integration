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
6. For ReadyNet, separate-computer, headless, network OS2L, or recording work: `docs/booth-node/00_START_HERE.md` and the ordered documents it links.

Historical handoffs and issues remain useful provenance, but these current QLC+ documents supersede their standalone-app sequencing whenever they differ.

## Current rig

- Four Both Lighting IR-4 fixtures, 10-channel mode, Universe 1 addresses 1, 11, 21, and 31.
- Four Both Lighting BO-TUBE192 fixtures, 40-channel mode, Universe 1 addresses 175, 215, 255, and 295.
- Each BO-TUBE192 40-channel fixture has eight RGBWY zones and no master dimmer/effect channel.
- Private duplicate fixtures on QLC+ Universe 3 support full-frame Priority Looks.

## Workspace rules

- The current alpha-candidate release is `releases/qlcplus-control-one/v23/IR4-TUBES-CONTROL-ONE-V23-LIVE-CONSOLE.qxw`. V22 is the unified creative rollback, V21 the reliability rollback, and V20 the protected creative baseline.
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
- V23 reuses the exact V21/V22 plug-in built against QLC+ commit `a124abebe0b5ad6077727c561a5a0e1f3730810c`. Preserve and update the complete compatibility tuple, package hashes, installer receipt, rollback path, and release validator for later releases.
- Before calling a release gig-qualified, qualify Micro, Control One DMX 1/2 together, simultaneous MIDI/feedback, repeated hot-plug, and the combined DJ workload.
- Move the current hard-coded rig intensity ranges into workspace/configuration before calling the plug-in general-purpose.

## Booth-node deployment rules

The ReadyNet/private-LAN and separate Windows booth-node work is a deployment layer around V23, not a change in lighting ownership.

- The DJ laptop and booth node use wired Ethernet for show control.
- The booth node runs QLC+; Control One and the selected SoundSwitch DMX hardware connect directly to it.
- Normal operation is headless and uses QLC+'s authenticated built-in web interface from the DJ laptop.
- Keep the show LAN routed/NAT, stable, private, and independent of venue DHCP or Internet availability. Reject a repeater/bridge mode that renumbers local devices.
- Never expose QLC+ web control to WAN/LTE or place guests on the show segment.
- Prepared scripts under `tools/booth-node/` are one-shot startup, preflight, and verified-copy helpers. Do not turn them into a second runtime controller or watchdog without explicit owner approval.
- The primary event recording writes to the DJ laptop's local SSD through the REV7 record return; a completed file may be copied to the booth node only after SHA-256 verification. Do not make the only recording depend on SMB or live network audio.
- Keep real-time feedback-suppression DSP on a separate dedicated audio node during beta work. QLC+ must not depend on it.
- Preserve the DJ-laptop QLC+ setup as a rehearsed rollback until the booth node passes all documented headless, fault-recovery, combined-soak, and pilot gates.
- Do not call the booth system gig-qualified before `docs/booth-node/04_VALIDATION_RECOVERY_AND_ROLLBACK.md` passes for the exact hardware topology.

## Claim boundaries

Use precise labels:

- **Structurally validated:** XML/references/IDs pass automated checks.
- **Software-tested:** deterministic plug-in/workspace tests passed.
- **Physical-output-tested:** the named device/port/fixture visibly responded.
- **Gig-qualified:** soak, fault recovery, audio/OS2L/MIDI/DMX, and operator workflow passed.

V23 is structurally validated and inherits the software-tested V21/V22 runtime against its pinned QLC+ build. The preceding baseline has physical evidence for Micro, each Control One DMX port independently, Control One MIDI/feedback, OS2L, and core pad/Priority Look behavior. The corrected live rails, persistent mode switch, and refreshed console need the short owner observation. Repeated hot-plug/LED restoration, simultaneous ports, and the combined two-hour workload remain pending.

The old `.github/workflows/native-core.yml` workflow is manual-only and its release job is disabled. Do not re-enable it for QLC+ tags. `.github/workflows/qlcplus-v21.yml` is historical and manual-only because this repository failed Actions jobs before runner allocation. V23 is validated locally and published directly; do not add or trigger a V23 GitHub Actions workflow unless the owner explicitly changes this rule.
