# V32 independent live-control audit

Audit scope: V32 source checkpoint `1f37441`, workspace
`IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw`, SHA-256
`16c676c531aa9aefa354eb2052e0f643be3c27d9e5fa71953deebf99257478c0`.
This report does not inspect or qualify V33. No product code or workspace was
changed for this audit.

The findings below follow the actual workspace, plug-in and exact pinned QLC+
source. They are source-confirmed defects or limitations with operator
reproduction procedures; physical execution remains pending.

## P1: STOP leaves Flash overrides active

The visible STOP button is widget `1001`, `Action=StopAll` (workspace line
41034). Color latches, color holds, White/Black/UV and Focus positions use
`Action=Flash`, with ForceLTP enabled.

In the pinned core, [VCButton::requestStateChange](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/qmlui/virtualconsole/vcbutton.cpp#L452)
calls only `MasterTimer::stopAllFunctions`. That method stops the running
Function list; it does not clear the separate DMX-source list.
[Scene::flash](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/scene.cpp#L650)
registers the Scene in that separate list. The
[DMX-source loop](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/mastertimer.cpp#L340)
continues writing it until `unFlash`.

**Impact:** STOP can stop Autoplay, Priority Looks, MOVE and STROBE while
latched Flash output remains lit. A color latch alone can keep tubes and Wash
lit because their emitter values are their intensity. The documented
"emergency global stop" claim is too broad.

**Reproduce:** Start a loop; Shift-tap WHITE; release both keys; click STOP.
White should remain active under the current implementation. Repeat with a
RED latch and inspect Wash/tubes. Repeat over Priority and STROBE. Use QLC+'s
native global blackout for the actual output kill; separately release latches
before removing blackout.

**Required repair:** A bounded stop route must release all Flash owners and
stop ordinary Functions, with real native-host verification. The MIDI test
`latchesFollowFunctionState` manually supplies zero feedback labelled "actual
QLC Stop"; it does not execute StopAll and cannot validate this contract.

## P1: BLACK is not an unconditional output blackout

BLACK widgets `1008`/`1019` target Scene `0` as ordinary ForceLTP Flash.
All color and White/UV overrides use the same priority. Pinned
[Universe::requestFader](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/universe.cpp#L217)
inserts a new equal-priority fader after existing ones, and
[GenericFader::write](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/genericfader.cpp#L291)
force-writes Flash channel values. A later override can therefore overwrite
BLACK on shared channels.

**Reproduce:** Shift-tap BLACK; then tap RED. Wash/tubes can illuminate while
the BLACK Scene remains flashed. A later WHITE hold can illuminate the full
rig. Run the same sequence over a Priority Look.

**Operator boundary:** BLACK is a performance Scene, not QLC+'s native global
blackout. The workspace's separate native `Action=Blackout` widget `1015` is
only 1 by 1 pixels; use the QLC+ toolbar global blackout and verify it with
all overlays active. A subsequent version should expose an unmistakable
native blackout control and define performance-effect precedence explicitly.

## P2: White/Black/UV hold and latch share one Flash state

Normal WHITE and Shift-WHITE use widgets `1009` and `1018`, channels `45` and
`173`, both referencing Scene `1`. BLACK shares Scene `0`; UV shares Scene `2`.
See workspace lines 41133–41177 and 41268–41309.

The pinned [Function::flash/unFlash](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/engine/src/function.cpp#L999)
uses one boolean, without per-input ownership. All widgets referencing that
Scene receive its flashing state via
[VCButton::slotFunctionFlashing](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/qmlui/virtualconsole/vcbutton.cpp#L293).
An ordinary release calls `unFlash` and clears the shifted latch too.

**Reproduce:** Shift-tap WHITE to latch it; release Shift; press and release
ordinary WHITE. The latch is lost after release. Repeat for BLACK and UV, and
with a mouse hold of the corresponding visible button. A disconnect while
the ordinary button is held has the same release interaction.

**Required repair:** Separate momentary/latch Scene ownership or merge the
two input states into one explicitly reference-counted control. Preserve
feedback and test mouse, MIDI, release ordering and disconnect together.

## P2: Color overrides flatten Wash and tube intensity patterns

Scene `37` RED writes every Wash zone red to `96` and every tube cell red to
`96`, with other emitters zero (workspace lines 890–914). Scenes `38–45` and
held Scenes `2185–2193` follow the same fixed-emitter approach. Physical and
private Priority fixtures both receive these values.

The pinned [BO-TUBE192 40-channel definition](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/resources/fixtures/Both_Lighting/Both-Lighting-BO-TUBE192.qxf)
and [Wash FX Hex 40-channel definition](https://github.com/mcallegari/qlcplus/blob/a124abebe0b5ad6077727c561a5a0e1f3730810c/resources/fixtures/Chauvet/Chauvet-Wash-FX-Hex.qxf)
have direct zone/cell emitters here, with no independent per-cell dimmer.
ForceLTP therefore replaces brightness patterns as well as hue. The plug-in
applies STROBE and Global/group scaling after this merge but cannot recover
the overwritten underlying per-cell brightness.

**Impact:** The documented color-only promise holds for the separate IR-4
master and Focus dimmer, but not for Wash/tube pixel intensity. Dark cells can
turn on; running rolls/comets become fixed all-on color layouts.

**Reproduce:** Run a visible Wash/tube chase. Latch RED, hold BLUE and release
BLUE. Observe whether the underlying per-cell chase continues through each
color state. Repeat under a moving Priority Look.

**Required repair:** Preserve the selected show's per-zone brightness while
applying replacement hue, or narrow the documented behavior. Do not describe
fixed emitter replacement as complete color-only SoundSwitch parity.

## Other boundaries and targeted checks

- V32 MOVE is a native sweep override, not a movement-freeze button. STROBE is
  a toggle with a fixed one-cycle-per-beat gate independent of chase speed.
  Position latches write the physical, Priority and MOVE mirrors. Confirm aim
  precedence, release to the advancing sweep and removal of the U4 route.
- Priority owner changes clear cached frame data, but independently scheduled
  universes have no generation tag. The existing source correctly documents
  the unresolved handoff window. Observe rapid look-to-look changes and
  release under playback; do not claim complete stale-frame rejection.
- Selected-loop seek, separate chase speed/dwell, mouse pad/color translation,
  held-key release after Shift release and reconnect cleanup have actual
  translator tests. They still need the pinned Windows host and rig checks.
- Scripted state, OLED/custom firmware, Hue/Smoke and broader Pan/Tilt
  functionality are documented reserved/deferred features, not hidden
  SoundSwitch-equivalent implementations. Intentional gesture changes in
  `CONTROL_ONE_WORKFLOW_SPEC.md` remain intentional.

## Evidence run in this audit

`python3 qlcplus/workspace-tools/Test-V32Workspace.py --self-test` passed:
128 raw loops, 32 Priority Looks, 849 widgets, 1,280 speed-preset checks and
eight rejected corruption cases. Two independent builds produced the workspace
hash above. This validator does not execute the pinned core's Flash/StopAll
behavior. No physical-output, Windows pinned-host or gig qualification was
performed by this audit. Existing native-test evidence is separate; this audit
did not rerun the six native targets because its local Qt build tools were
unavailable.
