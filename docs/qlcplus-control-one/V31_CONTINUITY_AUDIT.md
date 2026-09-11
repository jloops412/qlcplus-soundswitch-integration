# V31 continuity and reliability audit

## Scope and authority

This audit starts from V30 repair branch commit
`f842153acb8edee790b8c47650c81b0285dcd33d` (PR 112) and records the state found
before the V31 reliability edits. Findings below describe that input, not a
claim that every finding remains open after the repair pass.

The owner's September 10, 2026 direction defines three separate passes:

1. Review the accumulated work, recover missing behavior, fix regressions,
   check the result, and prepare a coherent reliability candidate.
2. Redesign the Autoloops and static/Priority Scenes for professional lighting
   quality after the control foundation is dependable.
3. Publish the resulting files to GitHub when the owner requests that pass.

This first pass does not authorize publication or wholesale creative
replacement. Existing show content remains the creative reference. Specific
timing or fixture corrections needed to restore a control contract must be
identified explicitly.

The mission remains one lighting runtime: QLC+. VirtualDJ supplies direct
OS2L timing; Control One supplies MIDI; native QLC+ plug-ins handle the narrow
hardware, feedback, Priority-frame, and timing gaps. QLC+ owns fixture
definitions, patching, Functions, playback, Virtual Console, persistence, and
output routing. A second application, daemon, custom firmware, or replacement
lighting engine is outside this work.

## Recovered history

| Version | Role and preserved contribution | Available provenance | Boundary at audit entry |
|---|---|---|---|
| V20 | Protected creative baseline; mouse bank/dwell; actual-widget ID allocation; preserved V19 creative/control Functions | Release folder and `V20_RELEASE_NOTES.md` | Workspace and profile do not match their recorded package hashes; retain as historical evidence pending exact-byte recovery |
| V21 | Reliability host; stale MIDI-handle recovery; LED retry/restore; direct OS2L keepalive; mouse fallback; install receipts | Release folder and its release notes | Workspace and profile hash mismatches prevent calling the checked-out folder a verified rollback |
| V22 | Merged 22 reviewed Variety Pro Chasers and 176 supporting Scenes into V21; preserved public ownership/control paths | Release folder, merge builder, `V22_UNIFIED_MERGE_PROVENANCE.md` | Workspace manifest expects LF while checkout forces CRLF; profile hash also differs |
| V23 | Virtual Console layout and nonoverlapping monitor indicators; one persistent mode switch; unchanged 2,090 Functions | Release folder, builder/validator, `V23_LIVE_CONSOLE_PROVENANCE.md` | Workspace matches its hash; packaged profile does not |
| V24 | One Surface feedback destination; positive-edge mouse commands; hardware-independent feedback; native runtime monitors | Release folder, builder, `V24_RUNTIME_FEEDBACK_PROVENANCE.md` | Workspace matches its hash; packaged profile does not; recorded isolated runtime evidence is historical |
| V25 | Reduced tracker footprint while preserving the V24 Engine; reviewed source for V26 | Source workspace and deterministic builder | Source workspace hash matches the V26 provenance; no public release folder is expected |
| V26 | Native four-bank monitor strips and all dwell options on each page; focused reported-BPM OS2L correction | Complete release folder, builder/validators, `V26_AUTOPLAY_CLARITY_PROVENANCE.md` | All manifest entries match; protected generation source and earlier-rig rollback |
| V27 | Adds Wash and Focus pair without moving existing fixtures; complete physical/private creative coverage; Focus positions; four-group intensity | Complete release folder, builders/validators, source/bench/build evidence | All manifest entries match; latest complete full-rig package; physical qualification remains unproved |
| V28 | No tracked workspace, release folder, documentation entry, or matching local-history subject recovered | Owner/previous-agent versions may exist elsewhere | Unknown provenance; do not infer that it never existed or that its changes are preserved |
| V29 | Owner reported slow/ineffective speed, lost Priority behavior, desired selected-loop continuation and color holds | Owner report; no corresponding tracked version recovered | Treat the reported symptoms as required acceptance cases, not a reviewed source file |
| V30 | Common raw-loop timing; deterministic randomized seek; Priority ownership/feedback; color latch/hold; Focus shortcuts moved to shifted performance pads | Generated workspace, Python builder/validator, source/profile, workflow, `V30_PERFORMANCE_RECOVERY.md` | Source candidate in PR 112; no complete versioned release package in the reviewed tree |
| V31 | Current reliability integration and continuity repair | This audit plus the bounded code/workspace fixes and recorded validation | Candidate only; software and physical evidence must be stated separately |

V30 is generated directly from the reviewed V27 full-rig workspace. It is not
an established merge of an identified V28 or V29 file. The prior owner reports
are important requirements; a missing intermediate artifact must never be
replaced by an invented history.

## Hash and rollback findings

The initial read-only check compared every entry in the seven available
release `SHA256SUMS.txt` manifests. V26 and V27 passed completely. V20 through
V24 had the mismatches listed below; other entries in those manifests passed.

| Package | Mismatched artifacts | Finding |
|---|---|---|
| V20 | Workspace and Control One profile | Neither LF/CRLF nor UTF-8 BOM conversion recovers the recorded hashes |
| V21 | Workspace and Control One profile | Neither LF/CRLF nor UTF-8 BOM conversion recovers the recorded hashes |
| V22 | Workspace and Control One profile | Workspace matches the recorded hash when converted to LF; profile does not match with newline/BOM conversion |
| V23 | Control One profile | Newline/BOM conversion does not recover the recorded hash |
| V24 | Control One profile | Newline/BOM conversion does not recover the recorded hash |

The repository's `*.qxw text eol=crlf` rule is relevant to V22's workspace
mismatch. It does not explain the other mismatches. Do not rewrite an older
manifest to bless different bytes. Preserve the folders, record the mismatch,
and recover exact original bytes from a verified archive or source before
describing those older packages as hash-verified rollbacks.

History distinguishes an initial publication problem from a later regression:
all of the affected V20–V24 workspace/profile Git blobs are byte-identical to
their first publishing commits. The V20/V21 workspace and old-profile
mismatches therefore already existed in the initial checked-in packages; they
are not evidence that a later agent rewrote those tracked artifacts.

| Artifact | First publishing commit | Historical recovery result |
|---|---|---|
| V20 workspace/profile | `1ed7b5d` | First-published blobs do not satisfy the recorded hashes |
| V21 workspace/profile | `7912a30` | First-published blobs do not satisfy the recorded hashes |
| V22 workspace | `4b6ba0f` | Exact original LF bytes are recoverable from blob `d69e644e7a0936fca4eec70afd4352a172649fa7`; SHA-256 `7ac6ed5413e2c4593b79d414c58c5adadbab2c3474a665f7e8ff3631fba012e7` |
| V22 profile | `4b6ba0f` | Same profile blob as V21; recorded hash does not match |
| V23 profile | `69c8887` | Same profile blob as V21; recorded hash does not match |
| V24 profile | `ed50f76` | Same profile blob as V21; recorded hash does not match |

The V22 mismatch is recoverable without rewriting the protected Git blob or
manifest: preserve LF for that specific file rather than applying the blanket
CRLF checkout rule. The blanket rule entered the merged contributor cleanup in
`a097228` after the file was published. V23–V27 workspaces use their currently
matching CRLF form and must not be globally converted to LF.

That V22 recovery is applied in this first pass: `.gitattributes` has a single
file-specific LF exception, and the working copy was restored directly from
the original blob. Its SHA-256 now matches `7ac6ed5413e2c4593b79d414c58c5adadbab2c3474a665f7e8ff3631fba012e7`.
The restored file hashes to the same Git blob as `HEAD`; there is no tracked
release-content delta and no manifest edit. The V22 profile mismatch remains
unresolved, so the complete V22 package is still unverified historical material.

A scan of all twelve unique historical `.qxw`/`.qxi` blobs reachable across
the 591 locally available commits found no exact match for the four remaining
expected hashes. LF/CRLF, BOM, and final-newline variants did not recover them.
The exact missing manifest identities are:

| Missing exact artifact | Recorded SHA-256 |
|---|---|
| V20 workspace | `b75cc331274ec616d94193fb9fbc9cf5e1615bcb95029778db3421f32c2fed0d` |
| V20 profile | `355e22535b551e409b8aea3a333cc5006b37ab19fe3aba293b760f24b539b9fd` |
| V21 workspace | `87445a94a40768a045a667f8efeec03025b9c1aac2dc4c78b913c8299c4587cf` |
| V21–V24 shared profile | `808023f28d56b8ad40bc522e131f47881026baf4c1dc66e9b2d8841e22cb9b8f` |

Recover those identities from a separately verified original archive if one
is available. Until then, keep the discrepancy explicit and use the complete
verified V26/V27 packages for the documented rollback route.

Verified source identities at entry:

| Artifact | SHA-256 |
|---|---|
| V25 source workspace | `2ee9fededa8cede6f4d2659c296035b9bf1340952de8c46e50e401ddef28bc9b` |
| V26 packaged workspace | `ed97e3ebaea120bc6ff5ff9747485da54e1808479f64a02ab4bc044744fab570` |
| V27 packaged/generated workspace | `a4f7559930e93485f3ea2815a0b44ca8e40acdfcc43fb3b0430e863c64b2dc4b` |
| V30 generated workspace at audit entry | `530e6d4166bc150442660bfbbff790bd1c1e4803f0c6d02c99302ebb30d79331` |

V27 is the immediate complete full-rig rollback; V26 is the verified protected
earlier-rig source. Neither package inherits a physical/gig-qualified label
from historical observations of a different workspace or plug-in.

## Active-document contradictions found

| Record at audit entry | Conflicting or incomplete statement | Required correction |
|---|---|---|
| `AGENTS.md` | V26 is current; only IR-4/tube rig; required validator/CI described as V26 | Name the current source candidate separately from the latest complete package; require its applicable validation while preserving V26 |
| Root `README.md`, `docs/00_START_HERE.md` | V27 described as current without a V30 source-candidate route | Present one consistent candidate, package, and rollback map |
| `PROJECT_STATUS_AND_ROADMAP.md` | Opens with V30, then calls V27 active and offers only V27 qualification/promotion steps | Distinguish V30/V31 source state, V27 rollback package, and current qualification route |
| `CONTROL_ONE_WORKFLOW_SPEC.md` | Groups 2 and 4 reserved | Group 2 is Wash zone emitters; Group 4 is Focus dimmers; only Scripted remains reserved |
| `VALIDATION_AND_MAINTENANCE.md` | Reserved groups must have no effect; V26-only validation and fixture list | Add all four live groups, eleven physical/private fixture pairs, seek, holds, and current candidate checks |
| `STATE_MODEL_AND_ARCHITECTURE.md` | One parameter overlay; only IR-4/tube hard-coded intensity ranges | Describe separate latched and transient held colors; document all current group spans and retained rig specificity |
| `COMMUNITY_MIGRATION_GUIDE.md` | Installs V26; troubleshooting and fixture reuse assume the older rig | Keep a clearly labeled complete-package route and prevent mixing old DLL/profile with the newer workspace |
| `CONTRIBUTING.md`, `docs/DEVELOPMENT.md` | Candidate instructions stop at V26/V27 | Give the current bounded build/validation route without weakening protected-package checks |
| `docs/qlcplus-control-one/README.md` | Calls V26 package current alongside V30 source description | Match the authoritative candidate/package/rollback map |
| `POST_TEST_PROMOTION_AND_CLEANUP.md` | Promotes V26 and archives `Legacy-Pre-V26` | Make this historical guidance explicit; do not execute cleanup against the active candidate |
| `MAPPING_REFERENCE.md` | Position controls remain in a generic reserved/partial list | Name nine implemented Focus A/B positions and MOVE sweep; separate genuinely unimplemented controls |

Historical files inside protected release folders should retain their original
version descriptions. Correct the active entrypoints and current engineering
records; do not erase the provenance by globally replacing version numbers.

## Build and artifact continuity gaps

- The reviewed V30 tree has a generated workspace, updated input profile and
  plug-in source, but no complete `releases/qlcplus-control-one/v30/` package.
  A workspace alone cannot deliver the changed native behavior.
- A runnable candidate requires the matching `soundswitch.dll`, pinned
  `os2l.dll`, Control One `.qxi`, required custom fixture `.qxf` files,
  workspace, exact source and build evidence, manifest, manual install steps,
  and rollback instructions.
  Never substitute a V27 DLL for a changed V30/V31 native implementation.
- The workspace references `Both Lighting / IR-4 (BOIR4) / 10 Channel`, but
  the original IR-4 fixture definition was not recovered in the repository,
  any release folder, or the exact pinned QLC+ fixture resources. At audit
  entry this blocked a reproducible clean install. V31 resolves that dependency
  with an independently authored exact-identity definition, checked against
  the manufacturer's manual rather than a generic ten-channel substitute.
  `qlcplus/fixture-definitions/IR4_DEFINITION_PROVENANCE.md` records the
  downloaded manual, channel mapping, and deliberate limits. The definition's
  SHA-256 is `082552a1d28ea9777dd8f3601ab654a34d4d61886aafbc76e60ff2931cee4cff`.
  It passes the pinned fixture schema and workspace checks. Exact-host loading
  and isolated physical channel observations remain pending; this file is not
  falsely represented as the owner's recovered original.
- V30's workflow path filter omits direct edits to its generated workspace and
  the input profile. Native-source changes trigger their own workflow but are
  not an integrated workspace/profile/package check.
- V30's workflow builds twice and compares those two outputs. At audit entry
  it does not first compare the checked-in candidate against the deterministic
  output, so that check alone cannot detect a stale or hand-edited candidate.
- Older package/branch checks remain useful provenance, but a green V26/V27
  check does not validate V30/V31 runtime contracts.
- Windows plug-in-load smoke evidence does not automatically prove loading
  against the exact pinned Windows QLC+ host. Preserve that distinction in
  build evidence and install instructions.

## Confirmed first-pass defects and repair scope

The parallel console, native plug-in, and fixture audits confirmed these
behavioral gaps. The table records the repair contract; use the final candidate
validation evidence for which repaired paths have actually passed. It is not
a claim that an unbuilt Windows binary or the physical rig has been tested.

| Confirmed gap | V31 reliability repair contract |
|---|---|
| Mouse loop pads replaced Autoplay with a manual owner | Mouse pad selections seek the active parent while Autoplay remains running, matching hardware behavior |
| Mouse colors did not follow the native latch state | Mouse and MIDI latch selection use consistent ownership and feedback |
| Focus position controls had Flash/latch behavior and Live-page access gaps | Preserve the nine position IDs/decoded values, provide usable latch/mouse control, and make them reachable from Live |
| A momentary color could survive disconnect or release in the wrong Shift order | Emit the held channel's release on disconnect and on physical release even after Shift is released first |
| A held effect could miss its release after leaving the Live page | Route momentary color/White/Black/UV release through Live first; this can return the operator to Live, while persistent latch release keeps the current page |
| Mouse-started Auto All could fail the native pause path | Preserve the actual automatic owner across mouse start and Play/Pause |
| Browsing another bank could redirect an order change | Keep the running owner's bank separate from the browsed bank when toggling order |
| Reconnect restored the browsed bank as the running Bank scope | Restore the actual owner's scope while preserving the browsed page and without restarting playback |
| Hardware scope changes retained an explicit seek from the old scope | Clear seek memory before changing scope; retain selected-start memory for order/resume within the same scope |
| MIDI latch and LED restoration could drift from QLC+ | Preserve authoritative Function/latch state and restore known feedback coherently |
| Autoplay pad LEDs did not observe the actual advancing raw Chaser | Carry native raw-Chaser feedback on dedicated channels 1100–1227, protect those channels from playback dispatch, and restore current-pad LEDs after reconnect or mode changes |
| Buffered Priority frame ownership was not an independent snapshot | Store a deeply owned frame snapshot so later producer-buffer mutation cannot alter the selected output |
| Removing the private output could leave its last frame buffered | Clear buffered frame data on private-output close while retaining the current Look's ownership for a clean reopen |
| Performance Scenes 0–4 and Focus positions 2175–2183 omitted private mirrors | Add corresponding private-layer fixture values so those controls can affect an active Priority Look |

Two broad effects remain explicitly deferred to the second pass: MOVE
(`2184`) and STROBE (`808`) are ordinary Chasers and do not guarantee override
precedence. Inspection of the pinned QLC+ behavior shows that applying the
Scene-style Toggle Override setting to a Chaser is ignored, and Chaser Flash
does not produce the intended output. A checkbox or copied attribute cannot
repair that contract; the effects need an actual design that establishes
correct parameter/output ownership. Their current behavior must not be
described as a guaranteed override over Priority Looks.

Priority-frame clearing at a look transition, and a deep frame copy, do not
prove generation-based rejection of late frames from independent universe
threads. Avoid claiming “all stale-frame races eliminated” without a specific
cross-thread generation/ordering test or an explicit synchronization design.

This pass prepares source/workspace repairs and evidence. A matched Windows
DLL build and complete runnable package remain separate work; no prior DLL is
silently relabeled as the repaired implementation. GitHub writes remain
deferred to the owner's third publication pass.

## First-pass acceptance checklist

The checklist states the concrete behavior that must survive integration.
Structural inspection, deterministic software tests, exact-host observation,
and physical output are different evidence columns; no unchecked physical
claim becomes true because a software test passes.

| Area | Required acceptance result | Appropriate evidence |
|---|---|---|
| Source identity | Candidate has named source hashes; V26/V27 unchanged; versioned output generated reproducibly | Hash comparison and deterministic build |
| Function graph | Unique Function/widget IDs; all references resolve; public IDs and logical channels retained except documented gesture migration | Independent workspace validator |
| Complete rig | IR-4s at 001/011/021/031; Wash at 041; Focus at 081/099; tubes at 175/215/255/295; exact private mirrors | Workspace/fixture comparison; named fixture bench |
| Priority isolation | Universe 3 stays on the private buffer, never a physical/network output; only one Surface feedback destination | Routing assertions and exact-host inspection |
| Native ownership | One-child manual owners; manual/Autoplay exclusion; read-only monitors cannot start or stop Functions | Graph checks and runtime transitions |
| Banks and manual pads | All four banks use child channels 0–31; Note Off retains manual latch; same pad toggles off; another replaces it | Mapping checks plus mouse/MIDI runtime cases |
| Autoplay selected start | While 6 runs and 7 is pending, pressing 3 runs 3 then 4/5/6 in sequential mode; Bank and All both work | Deterministic seek tests plus live parent observation |
| Randomized seek | Requested pad starts exactly; cycle is a complete nonrepeating permutation; selecting same pad after advancement still works | All bank/all mappings plus repeated-input runtime cases |
| Order and transport | Order change and resume preserve intended selected start; pause does not silently reset the show | State-transition tests and exact-host observation |
| Dwell | All five values visible on every dwell page; changing dwell retains the running parent | Workspace assertions and runtime observation |
| Chase speed | 0.25x/0.5x/1x/2x/4x changes actual step timing independently of dwell for all 128 raw loops | Timing/controller coverage plus representative exact-host cycles |
| Priority handoff | Every Look takes full-frame ownership, clears stale previous-look output, shows correct LED state, and releases to the continuously advancing base | Native state tests plus still/moving Look transitions |
| Normal color latch | Nine colors toggle exclusively and affect every physical/private fixture class without taking movement/intensity ownership | Scene/channel audit and runtime output observations |
| Shift-held colors | Nine holds release only their transient layer; underlying latched color/show returns; Shift release order, mode changes, Stop and reconnect do not leave a stuck hold | Targeted input/state tests and exact-host MIDI checks |
| Performance controls | White, Black and UV retain intended hold/toggle/release behavior on both layers; existing Strobe stays present with its current precedence limitation explicitly recorded | Function closure comparison and output checks |
| Focus controls | Shift + performance pads 1–9 keeps IDs 2175–2183 and decoded aims; MOVE keeps sweep 2184; no color-channel collision | Profile/widget/function comparison and controlled aim bench |
| Intensity | Global multiplies Groups 1–4 correctly; group changes touch only designated emitter/dimmer channels; Scripted remains inert | Channel-level deterministic tests and fixture-group observation |
| Mouse fallback | Bank, mode, order, dwell, speed, transport and intensity controls work without an attached MIDI-output handle; command trailing zero does not retrigger | Hardware-free native tests and exact-host UI observation |
| Monitoring and feedback | 128 raw-Chaser monitors cover all banks once, outside owner exclusivity; one persistent mode button; LEDs restore known state | Workspace assertions and reconnect observation |
| OS2L | Reported BPM drives timing; packet bursts do not multiply the beat; late start/reconnect and source silence behave as documented | Focused plug-in test or exact-host known-track observation |
| Output/reconnect | Newest frame wins; disconnect does not freeze QLC+; recovery restores output/LEDs and clears transient holds | Native tests and named device/port hot-plug bench |
| Fixture-specific constraints | Focus reset/internal-show values remain disabled; released Focus UV remains off; physical A/B identity and Wash zone orientation recorded | Channel audit and controlled fixture bench |
| Package integrity | Every delivered file has verified hash and source/build compatibility; documented verified IR-4 and custom Focus definitions available; manual Explorer/QLC+ install; backups and receipt route | Package validation and install review |
| Claim accuracy | Report exactly which structural, software, exact-host and physical checks ran; no unobserved gig-ready claim | Evidence record tied to candidate hashes |

Combined two-hour DJ/audio/OS2L/MIDI/LED/DMX soak and simultaneous outputs are
existing gig-qualification gates. Run the intended actual output configuration;
do not multiply hardware work by testing ports the owner will not use. The
first pass can complete its software repair and handoff while transparently
leaving unavailable physical observations pending.

## Handoff to the later creative pass

The owner's dissatisfaction with every Autoloop/static Scene remains open.
Full fixture coverage and a complete XML frame do not prove professional
lighting design. Preserve all 128 loop and 32 Priority public identities while
planning cohesive color, intensity, movement, contrast, phrasing and variation
for the actual small-DJ rig. The second pass should begin from the repaired
control candidate and prove its console/control layer remains unchanged.

Include MOVE and STROBE parameter/output ownership in that creative design.
Their Chaser limitations are known functional work, not merely aesthetic
polish. Any control-layer change genuinely required by that design must be
bounded, documented, and regression-checked separately.

V30 explicitly normalized nine variable-timing raw loops to restore functional
speed: `573, 576, 632, 633, 635, 636, 645, 657, 658`. Revisit their musical
phrasing during the creative pass without restoring the PerStep timing that
bypasses the current speed controls. Do not treat a label such as “Pro” or
“Variety Pro” as evidence that the resulting light show meets the owner's
quality standard.

The third pass must assemble and publish one matching package and current
documentation from the reviewed result. GitHub publication, release creation,
and remote cleanup remain deferred until the owner requests that pass.
