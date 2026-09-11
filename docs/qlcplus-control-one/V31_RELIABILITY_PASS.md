# V31 reliability pass

Audit date: 2026-09-10. Status: unpublished source/workspace candidate.

V30 contained real control and output-layer defects despite passing its
structural validators. V31 repairs the confirmed first-pass defects, protects
the inherited creative show, and records remaining work explicitly. It remains
QLC+ plus the focused SoundSwitch plug-in and existing OS2L correction.

**This is not an installable Windows release or a gig-qualified build.** The
workspace requires the corresponding V31 SoundSwitch source to be built with
the matched Windows toolchain. Do not pair it with a V26, V27, or V30 DLL.

## Source identity

- Canonical repository: `jloops412/qlcplus-soundswitch-integration`; the former
  `jloops412/EmberLights` URL redirects there.
- Published main at audit entry: `14f2d64958ef0455402fcaeadd9509f3be68fde9`,
  still identifying V26.
- Actual V30 candidate: [PR #112](https://github.com/jloops412/qlcplus-soundswitch-integration/pull/112),
  head `f842153acb8edee790b8c47650c81b0285dcd33d`.
- V30 immutable workspace SHA-256:
  `530e6d4166bc150442660bfbbff790bd1c1e4803f0c6d02c99302ebb30d79331`.
- V31 working branch: `audit/v31-reliability-pass`.
- QLC+ source remains `a124abebe0b5ad6077727c561a5a0e1f3730810c`,
  Windows UI `5.3.0 GIT a124abe`, matched Qt `6.8.1`.

V31 workspace SHA-256:
`c6e03f865cfeda3fb5c578221ade3ef6883d87c033072a41bf5f1511f7f13651`.
It contains 2,255 Functions and 849 widgets.

The associated `V31_VALIDATION_EVIDENCE.json` records final candidate identities,
commands, outcomes, and incomplete deployment gates.

## Confirmed repairs

| Defect found | V31 change |
|---|---|
| Clicking an Autoloop pad could replace Autoplay with a manual owner | Mouse pads enter the same selection/seek translator as Control One; existing native owners and raw Chasers remain authoritative |
| Mouse colors used momentary Flash while hardware colors latched | Mouse color commands share the exclusive latch path; held colors remain an independent layer |
| Focus position Toggle Scenes lost precedence to continuing loop writes | Position latches drive native ForceLTP Scene Flash; matching controls are reachable from Live and Position Bench |
| White/Black/UV and position controls affected only the physical layer | Added corresponding private FixtureVals, so those controls reach an active Priority Look too |
| Held colors/White could remain active after MIDI disconnection or page navigation | Return to the native Live page before known transient releases; release on callback and stale-handle recovery while preserving latches |
| Mouse-started Auto All could not pause/resume | Transport uses the actual owner, even before any Auto Bank has been selected |
| Browsing a different bank could redirect order changes or reconnect scope | Preserve the running owner's bank independently from the browsed bank |
| Hardware scope changes retained a seek belonging to the previous scope | Reset selected-start memory only for a genuinely different scope; retain it for same-scope order/resume |
| Old owner-off feedback could overwrite a replacement owner's state | Match feedback to the current owner before changing transport state |
| Latch and Priority LED caches diverged from QLC state | Synchronize from QLC Function feedback; restore the relevant surface mode on reconnect |
| Autoplay lacked raw-Chaser feedback to its hardware pad LEDs | Native read-only monitors report actual raw Function state through dedicated feedback channels; no polling tracker or second playback owner |
| Priority QByteArray could alias QLC's mutable raw universe buffer | Deep-copy incoming frames into owned storage |
| Removing the private output could retain a frozen frame | Invalidate the cached private frame on route removal and fall back to the advancing base |
| Clean installs lacked the custom IR-4 personality | Added an independently authored, schema-validated definition from the original manufacturer manual |
| V30 CI could rebuild over an altered checked-in workspace and still pass | Compare the checked-in file with deterministic rebuilds; add current-workspace/actual-source regression gates; remove CI auto-committing |

Known momentary release returns to Live before releasing, including after the
operator browses another page. Bank LEDs retain the browsed bank while
Autoloop pads show the actual running pad number. Priority mode shows the
selected Look. Keep each mouse command Scene unique; sharing one Scene among
several command widgets could dispatch multiple unrelated commands.

The IR-4 definition and its source evidence are in
`qlcplus/fixture-definitions/IR4_DEFINITION_PROVENANCE.md`. Its channel facts
match the current show, including White before Amber. Raw internal program and
strobe channels remain zero in authored output.

## Preservation proof

- All 11 physical and 11 private fixture IDs, modes, addresses, and I/O routes
  are unchanged. U3 remains an internal-only Priority buffer.
- All 128 raw Autoloops, their 1,024 creative Scene leaves, the 102 Priority
  Scene leaves, and native playback owners are retained.
- The 14 repaired performance/position Scenes retain all original physical
  values and add 73 private fixture payloads.
- V30 speed/dwell behavior, five stable shuffled cycles, and exact selected-start
  mapping are retained. The mapping covers **512** Bank/All/order selections;
  the earlier V30 prose incorrectly stated 320.
- V30's nine normalized timing exceptions remain documented for pass two:
  `573, 576, 632, 633, 635, 636, 645, 657, 658`.
- Protected release Git contents are unchanged. V22's working bytes were
  recovered with an exact-file LF rule matching its original Git blob and
  checksum. Other old manifest discrepancies remain explicitly unverified;
  they were present at initial publication, not evidence of a new mutation.

## Verification

The new MIDI regression tests execute the actual translator with Qt queued
events and a test-only WinMM boundary. They do not simulate physical hardware.
Against the original V30 sources, five of the initial six MIDI test groups fail,
and the mutable Priority-buffer test fails. Against the repaired sources, the
portable protocol, intensity, performance/seek, Priority, and MIDI targets pass,
including nine MIDI test groups.

Workspace verification checks the checked-in artifact, native ownership,
control routing, fixture completeness, private coverage, preserved creative
data, and deliberate corruption rejection. Two separate builds must be byte
identical to each other and to the checked-in candidate. Existing V26 and V27
package validators and the V27/V30 workspace validators also pass.

Run from the repository root:

```bash
python3 qlcplus/workspace-tools/Test-V31Workspace.py --self-test
cmake -S qlcplus/plugins/soundswitch/tests -B build-v31-tests -DCMAKE_PREFIX_PATH=/path/to/Qt6
cmake --build build-v31-tests --parallel
ctest --test-dir build-v31-tests --output-on-failure
```

The added workflow files are prepared for later publication. No GitHub source,
PR, release, or workflow run was written or triggered during this pass.

## Remaining work

1. **Creative pass:** redesign Autoloops and Priority/static Looks intentionally
   for this rig; improve movement, transitions, contrast, phrasing, and fixture
   roles while preserving the now-checked control contracts. Revisit the nine
   timing exceptions without disabling live speed control.
2. **MOVE/STROBE:** these inherited ordinary Chasers do not provide a dependable
   force-override contract. QLC ignores Flash override attributes on Toggle,
   and generic Chaser Flash is not an output implementation. Give these effects
   a real ownership/programming design in pass two. Release a latched Focus aim
   before expecting MOVE to control pan/tilt.
3. **Matched Windows build:** compile final source with the exact QLC+/Qt/MinGW
   tuple, run the Windows tests and plug-in load smoke, and load it into the
   pinned QLC+ host. Linux software tests do not substitute for this gate.
4. **Physical bench:** test speed/dwell/seek with mouse and hardware; all
   Priority handoffs and releases; latch/hold combinations; Focus positions;
   all fixture classes and intensity groups; reconnect and intended outputs.
5. **Priority handoff boundary:** cached-frame clearing and owned snapshots do
   not identify a late previous-generation frame from independently scheduled
   universes. Observe the actual handoff before claiming clean physical output.
6. **Publication pass:** after the owner requests pass three, assemble the
   complete versioned package with both matched DLLs, definitions/profile,
   hashes, installation/rollback instructions, and evidence. Publish through
   the existing QLC+ repository lane.

See `V31_CONTINUITY_AUDIT.md` for the full history, original manifest
discrepancies, requirements inventory, and acceptance matrix.
