# EmberLights Current Build Execution Handoff — 2026-08-19

**Status:** Binding coordination document until issue #87 Gate A lands one accepted implementation anchor on `main`.

**Primary trackers:** #87 (convergence), #90 (current SoundSwitch 2026 source adapter), #89 (OS2L/VirtualDJ reliability), #52/#79 (fixture and physical evidence), #37/#32/#33 (replacement shell and first product slice), #88 (perceptual fade quality).

**Read this before changing code, opening another implementation branch, expanding a feature, or generating another migration project.**

## Dynamic-head note

`86694f02d89ef856b06d846026fe558e39206f90` is the exact **pre-coordination implementation/evidence baseline** that was at `main` when this planning package began. Once this planning package lands, `main` will advance through documentation-only coordination commits.

Therefore:

- integration agents start from the **latest `main` coordination head**, not by resetting `main` back to `86694f0`;
- they verify that `86694f0` remains an ancestor of that head;
- they reconcile the exact implementation branch head `7efd0b083cc7c42df3b2a5ee3e760687a15e390b` into that latest `main` descendant;
- no planning/documentation commit is treated as a substitute for implementation convergence.

This note controls every later shorthand reference to `main@86694f0` in issues, comments, or historical handoffs.

---

## 1. Executive directive

EmberLights no longer lacks architecture, engine work, models, or ideas. It has substantial implementation distributed across a diverged integration branch and newer SoundSwitch research on the `main` lineage. The dominant risk is now **building on the wrong truth, reimplementing completed work, or extending a temporary path before existing work is converged and proven**.

The next build session proceeds in this order:

1. **Converge repository truth.** One branch must contain the latest `main` coordination/evidence lineage and PR #86 implementation without silently dropping either history.
2. **Build the real SoundSwitch source adapter/import path.** Begin with one exact current-project Autoloop, not another hand-authored approximation.
3. **Finish OS2L P0 through installed VirtualDJ evidence.** Preserve the existing bounded Blackout-feedback software slice, capture the real wire behavior, then finish listener/discovery/reconnect ownership.
4. **Close only physical/fixture gates that affect correctness and support claims.** Exact fixture qualification must not prevent opening, importing, inspecting, simulating, saving, or re-importing an output-disabled project.
5. **Finish the measured replacement-shell decision and activate one product-shaped Fixtures + Static Looks slice.** Do not add more product-facing Win32 forms.
6. **Then improve perceptual fades, broader Autoloop/Live quality, mappings, and full parity.**

No broad feature agent should extend current `main` or PR #86 independently while Gate A is unresolved.

---

## 2. Exact repository truth at this checkpoint

### Protected implementation inputs

| Input | Exact identity | Meaning |
|---|---|---|
| Pre-coordination `main` implementation/evidence baseline | `86694f02d89ef856b06d846026fe558e39206f90` | Current-project SoundSwitch decoder/pilot evidence plus the main-only historical Preview 97 installer upload lineage. It does not contain the broad current integration implementation. |
| PR #86 / `agent/backyard-party-v2` | `7efd0b083cc7c42df3b2a5ee3e760687a15e390b` | Broad implementation line: Autoloops V2, Studio seams, fixture-control work, qualification tools, UI course correction/Slint lab, Preview 101 lineage, OS2L Blackout-feedback software, and independently added decoder research. |
| Last common ancestor | `47bb45d6e84609a7a73358517ed77e492c29f882` | Merge base of the two implementation histories before coordination docs advance `main`. |

At the time of review, GitHub reported PR #86 as:

- 23 commits ahead;
- 7 commits behind the pre-coordination `main` baseline;
- diverged and not mergeable;
- 276 changed files with a very large implementation surface.

### What the main-side divergence represents

The main-side commits are principally:

1. `fdcc8a43e9b8649077e5d872c7cf6cac8095482d` — historical upload of `installer/EmberLights-0.1.0-preview.97.0-Setup.exe` into source history;
2. current SoundSwitch v2.2.2/current-2026 decoder and pilot evidence ending at `86694f0`.

PR #86 independently added copies of the decoder research near its head. Similar filenames do not prove identical blobs, conclusions, or history. Convergence must compare them deliberately.

### CI truth

Recent GitHub Actions jobs failed before runner assignment because of the repository/account Actions payment or spending-limit gate. Empty jobs are infrastructure failures—not source regressions and not source validation.

PR #86 contains extensive reported local build/test/package evidence from its component checkpoints. That evidence is useful, but the final converged tree still requires a fresh complete local/Windows validation pass before merge or installer publication.

### Installer truth

Preview 101 is the latest identified installed legacy-shell test line in the integration documentation. It is a small session-ownership safety/defect preview over the frozen transitional Win32 shell. It is not the replacement-shell product UI and is not the current PR #86 head.

Do not advance the preview number merely for model-only work or rearranged legacy UI. The next installer must be either:

- an explicitly labeled diagnostic/lab build required for evidence; or
- a meaningful product-shaped slice with exact source, package, Windows, and claim boundaries.

---

## 3. Work that already exists and must not be rebuilt

Open issue checkboxes and old sequencing docs are stale in several places. Inspect the converged tree before assuming any of the following is absent.

### Core, Runner, output, and evidence foundations

The integration lineage already contains:

- deterministic project/compiler/Runner boundaries;
- sparse semantic property ownership and fixed two-universe rendering;
- Art-Net, sACN, initial USB-DMX adapters, SoundSwitch Micro session/qualification tooling, output routing, Blackout authority, diagnostics, and frame attribution;
- MIDI/WinMM, mapping, and session-scoped ownership foundations;
- raw hardware test, Runner-frame inspection, evidence joining, and qualification/attestation foundations;
- fixed-capacity/no-model-call Runner rules and non-droppable safety authority.

Do not create another engine, resolver, output router, project model, or hardware-test subsystem.

### Fixture truth and Static Looks

The integration lineage already contains:

- profile-backed fixture capabilities and one fixture-control identity for direct channels and named ranges/functions;
- structured profile/channel authoring services;
- fixture/group target validation and capability-aware assignments;
- Static Look authoring, transactional document mutation, exact output-disabled production preview, and bounded physical-preview foundations;
- renderer-neutral Fixtures + Static Looks shell models and an opt-in Slint product-shaped lab;
- an accepted UI course correction freezing further product-facing Win32 editor/form growth.

Do not add fixture-specific repair buttons, duplicate channel models, or raw-DMX-first product controls.

### Autoloops V2 and AutoScript

The integration lineage already contains substantial Autoloops V2 work:

- signed 64-bit 960-PPQ source/event model;
- source validation, canonical serialization, and deterministic compilation;
- generation-checked authoring, dependency reporting, placement/content separation, Undo/Redo, and populate/reset behavior;
- an original 128-placement EmberLights starter pack with stable management identity;
- rich-source persistence through the existing project compatibility channel;
- deterministic bounded rule-based AutoScript proposal/review/commit;
- Studio AutoScript transaction bridge through `StudioDocumentService`;
- output-disabled V2 preview and project Palette realization;
- compiled package/director/Runner path with explicit legacy/V2 ownership and return behavior.

The current priority is source migration fidelity and product exposure—not another timeline model, generator, persistence model, runtime, or hand-authored default-pack pass.

### UI platform direction

Accepted direction:

- legacy Win32 is a frozen transitional/Safe bridge;
- no new top-level Win32 editor, property form, or product-skin claim;
- one renderer-neutral command/state/capability/component registry;
- Slint/C++ is the first product-shaped candidate;
- WinUI 3 is the bounded Windows comparison;
- Direct2D/Win32 is the Safe baseline;
- raw DMX is Advanced diagnostics;
- reusable controls are built only within the accepted toolkit/runtime and carry keyboard, accessibility, state, ownership, and safety semantics.

Do not restart UI architecture or introduce another framework/runtime.

### Existing OS2L software checkpoint

PR #86 head contains the first bounded outbound Blackout-feedback implementation:

- `Os2lTcpServer` send/partial-send/coalescing state;
- feedback counts/errors/bytes and synchronization status;
- Runner publication of authoritative Blackout feedback state;
- native tests.

This is software-complete only for the first bounded feedback slice. It does not prove installed VirtualDJ packet behavior, launch-order discovery, song-transition lifecycle, application-owned service extraction, or the generic command bridge.

### Existing SoundSwitch decoder evidence

The repository records source-derived evidence for:

- exact current project identity and hashes;
- 112 Autoloop placements across four banks;
- exact placement-array ordering rather than raw entry order;
- A-record normalized intensity timelines;
- B-record timed RGB plus direct-emitter endpoint evidence;
- Static Look intensity/RGBWAUV tables;
- exact current-project Venue target IDs;
- Pilot 01 failure caused by older-project target-map contamination;
- a corrected-map one-loop comparison probe with a deliberately labeled test-only red fallback.

That evidence is sufficient to start issue #90. It is not sufficient to claim exact interpolation, exact unidentified packed-byte semantics, or a faithful full 112-loop import.

---

## 4. Real remaining gaps

These are genuine gaps rather than stale checklist artifacts:

1. one accepted implementation tree;
2. a production current-2026 SoundSwitch source adapter/import path;
3. installed VirtualDJ wire/lifecycle evidence;
4. application-owned OS2L listener/discovery/reconnect lifetime;
5. installed Windows, physical-output, soak, and gig qualification at the proper claim level;
6. a measured production toolkit/renderer decision and trusted Safe path;
7. activation of the first installed product-shaped Fixtures + Static Looks workflow;
8. perceptual fade/transition quality on owned fixtures;
9. broader SoundSwitch parity: track scripts, movement/effects, source versions, fixture corpus, mapping UX, and complete Studio/Live journeys.

---

## 5. Binding critical path

```text
#87 Gate A: repository convergence
        |
        +--> #90 current SoundSwitch source adapter/import vertical slice
        |
        +--> #89 installed OS2L capture + lifecycle/service proof
        |
        +--> #52/#79 owned-rig evidence and truthful qualification
        |
        +--> #37 toolkit decision -> #32 Safe/runtime -> #33 first Default product slice
                     |
                     +--> #88 perceptual fade quality
                     +--> broader Autoloops/Live/mapping/migration parity
```

After Gate A, independent lanes may proceed in parallel only from one exact convergence anchor and with non-overlapping file ownership.

---

## 6. Work Packet A — Gate A repository convergence

**Priority:** P0; blocks broad implementation.

**Owner:** one integration agent/worktree. Other agents may inspect but must not independently merge the shared histories.

### Branch procedure

1. Fetch the latest `main` after this coordination package lands.
2. Verify `86694f02d89ef856b06d846026fe558e39206f90` is an ancestor of that head.
3. Create `integration/convergence-2026-08-19` from that latest `main` head.
4. Merge or replay exact PR #86 head `7efd0b083cc7c42df3b2a5ee3e760687a15e390b` with both histories preserved.
5. Never resolve conflicts with blanket `ours`/`theirs` or a two-parent commit whose tree silently discards one parent.
6. Resolve by domain and record every deliberate choice.
7. Keep the first convergence commit behavior-preserving; do not mix in new features/refactors.
8. Keep `main` and PR #86 otherwise read-only until the convergence PR is validated.

### Mandatory reconciliation areas

#### SoundSwitch research

Compare all current-2026/v2.2.2 research files from both histories. Retain:

- placement-array ordering;
- current source target map `197–201` and `89–156`;
- Pilot 01 invalidation;
- Pilot 02A limitations;
- no unrelated color borrowing in production import;
- format-1 chunking as comparison only;
- V2 semantic source/program model as the production target.

No evidence file may regress to the older `345+` tube mapping as current truth.

#### Preview 97 binary/source history

Handle `installer/EmberLights-0.1.0-preview.97.0-Setup.exe` explicitly:

- preserve identity/checksum and a durable artifact/tag/branch pointer before any removal from ordinary source history;
- never present it as current;
- do not let historical binary placement dictate the converged source tree;
- record the artifact-policy decision.

#### Governance and generated truth

Reconcile at least:

- `AGENTS.md`;
- `docs/00_START_HERE.md`;
- `docs/06_PRIORITIZED_BACKLOG.md`;
- `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`;
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`;
- active handoffs/checkpoints and artifact pointers;
- CMake/Make/workflow/package sources;
- UI/Action registry sources and generated artifacts.

Regenerate contracts from accepted sources. Do not manually merge generated outputs while leaving generators stale.

### Older PR equivalence review

After the convergence tree is green, compare #53, #82, #83, #84, #85, and #86 against it. For each, record unique:

- implementation;
- tests;
- docs/evidence;
- artifact lineage;
- accepted/rejected disposition.

Close/supersede only after content equivalence, not by title or chronology.

### Validation gate

At minimum:

- `git diff --check`;
- clean UI registry/Action regeneration and checks;
- surface-contract gate;
- full warnings-fatal native `all` and `test` suites;
- supported CMake configure/build/CTest;
- migration/parser regressions;
- Autoloops V1/V2 authoring, persistence, preview, AutoScript, runtime, director, and Runner suites;
- fixture/profile/Static Look/physical-preview suites;
- OS2L server/Runner feedback tests;
- package-contract tests;
- supported Windows x64 warnings-as-errors build;
- Slint lab contract/compiler check;
- dry-run Runner smoke and scheduler/resource guard tests.

Because hosted Actions is infrastructure-blocked, attach exact local commands, environment identity, and results. An empty Actions run is not validation.

### Gate A exit

Gate A closes only when:

- one convergence PR is mergeable and green on available evidence;
- its tree transparently preserves/equates both protected implementation inputs;
- `main` becomes the unambiguous implementation truth;
- #90 and #89 can branch from one exact accepted commit;
- no active draft contains unreviewed unique implementation.

---

## 7. Work Packet B — #90 current SoundSwitch source adapter/import

**Priority:** P0 immediately after Gate A.

### Production architecture

```text
read-only source bundle manifest/probe
  -> exact source-version parser
  -> exact source-project Venue hierarchy/map
  -> Autoloop catalog/placement/length parser
  -> .ssfile A/B records + Static Look tables
  -> evidence-bearing candidate IR
  -> explicit destination target/profile reconciliation
  -> Autoloops V2 semantic source + Static Look proposal
  -> output-disabled production preview
  -> reviewed one-transaction commit
  -> stable idempotent re-import
```

Reuse the accepted migration IR, `AutoloopSourceDocument`, authoring/persistence/preview/compiler services, profile-backed fixture controls, and `StudioDocumentService`. Do not create a second project, timeline, color, fixture, preview, or transaction model.

### First vertical slice only

Implement current-project:

- bank: Medium;
- slot: 1;
- source: `SSAutoLoop1.ssfile`;
- identity: `Red - Smooth Pulse`;
- source SHA-256: recorded in #90;
- source targets: uplights `198–201`; tube cells `90–105`, `107–122`, `124–139`, `141–156`.

Acceptance requires exact source identity/placement/length, exact source map, raw and interpreted record evidence, deterministic output-disabled V2 proposal, save/reopen, and idempotent re-import.

### Fail-closed rules

- resolve target IDs from the exact source Venue database;
- never reuse another project/version's IDs;
- use decoded placement arrays, not raw entry order;
- no name-derived choreography;
- no raw source DMX copy into unrelated destination profiles;
- no color borrowed from unrelated targets;
- missing local color context becomes `MissingColorSource` / `NeedsColorContext`;
- unsupported UV on RGBWY tube cells is degraded, not reassigned;
- strobe/movement/wheels/unknown attributes remain off/opaque until decoded and safety-qualified;
- interpolation and unidentified packed bytes remain translated/opaque until controlled-delta evidence promotes them;
- format-1 sampled helper-Look projects remain comparison artifacts only;
- production import targets V2.

### Testability rule

Unqualified fixture/profile/patch status must:

- remain visible and truthful;
- keep output disabled by default;
- require explicit bounded arming for physical tests;
- never block source inspection, mapping review, output-disabled simulation, editing, save/reopen, or migration-report generation.

Do not expand to slots 2–4 or all 112 loops until this vertical slice is source-map-correct and every unresolved semantic is explicit. A mismatch returns to parser/evidence work, not hand-authored replacement content.

---

## 8. Work Packet C — #89 OS2L/VirtualDJ reliability

**Priority:** P0 after convergence; may proceed parallel to #90 with isolated files.

### Preserve and verify the existing feedback slice

PR #86 head already adds bounded Blackout-feedback send support and tests. Preserve it, then verify:

- authoritative Blackout snapshot after connection;
- transitions from every supported surface;
- bounded/coalesced partial-send behavior;
- send-failure diagnostics and recovery;
- no socket work on the DMX scheduler;
- client loss never clears authoritative Blackout.

Do not reimplement it from issue prose before reviewing the converged code.

### Next executable slice: installed raw capture

Package/expose `os2l_capture` and record exact installed VirtualDJ behavior for:

```vdjscript
os2l_button 'blackout'
os2l_button 'blackout' on
os2l_button 'blackout' off
os2l_button 'EmberLights Keepalive' off
```

Also capture load/play/stop/replace transitions and whether VirtualDJ closes TCP or merely stops/reset transport traffic.

This evidence determines whether the next fix belongs in serialization/feedback semantics, discovery policy, listener ownership, or the VirtualDJ mapping assumption.

### Then finish application-owned service lifetime

The normal product path must work in either launch order and across app/song transitions without pressing a lighting pad to wake OS2L. Separate:

- listener state;
- DNS-SD advertisement;
- TCP client state;
- transport/beat-sync state;
- connection/session epoch;
- authoritative feedback state.

Do not begin the generic `os2l_cmd` bridge until P0 feedback/lifecycle is proven with real VirtualDJ.

---

## 9. Work Packet D — fixture, patch, and physical evidence

**Priority:** P0/P1 evidence lane; it must not displace convergence or #90.

### Current destination rig

Use:

- 4 × Both Lighting IR-4, 10CH, U1 `001`, `011`, `021`, `031`;
- 4 × BO-TUBE192/360 Tubes, 80CH, U1 `041`, `121`, `201`, `281`;
- no additional generic Both Lighting uplight destination fixtures.

SoundSwitch source uplight identities describe the source rig. Reconcile their semantic intent to current IR-4 destination capabilities; do not add duplicate destination fixtures or copy raw source channels.

### Qualification versus project testability

Exact qualification remains required for:

- physical support claims;
- evidence-backed attestation;
- automatic output enablement or gig-qualified status;
- claims of exact emitter/channel behavior.

It is not required merely to:

- import/open a project;
- inspect the source map/report;
- simulate output-disabled frames;
- edit mappings/profiles;
- save/reopen;
- run an explicitly bounded user-authorized test with visible unverified status.

Remove/bypass any hard UI gate that blocks these safe tasks solely because qualification is incomplete. Replace it with truthful status, disabled output, and explicit safe arming.

### Remaining evidence session

Use existing tools to settle:

- IR-4 10CH R/G/B/W/A/UV one-hot truth, master, strobe-off, mode/address;
- tube cell order/direction and RGBWY behavior;
- SoundSwitch Micro raw frame versus exact Runner frame;
- blackout, reconnect, and unplug/replug;
- bounded selected-fixture preview timeout/fault/terminal blackout;
- exact evidence/attestation tied to source/profile/mode/address/backend.

Do not create another hardware-test UI or infer fixture response from accepted USB writes.

---

## 10. Work Packet E — replacement shell and first real product slice

**Priority:** P1 after Gate A. #37 evidence may run parallel to #90/#89 with isolated files.

### No more legacy-shell expansion

Win32 receives only safety, defect, accessibility, bridge-containment, and explicitly required diagnostic-evidence changes. No new top-level Win32 editor, inspector, property form, or product skin.

### Finish #37 with one real workflow

Evaluate Slint/C++, WinUI 3, and Direct2D/Win32 Safe through the same Fixtures + Static Looks model, commands, persistence, preview, and fault states. Measure:

- native Windows packaging;
- DPI/scaling and resize;
- keyboard traversal and visible focus;
- UI Automation/Narrator/accessibility;
- startup, memory, repaint, long-list behavior;
- preview timeout/fault behavior;
- scheduler effect;
- dependency/license/redistribution/servicing impact.

Select one production renderer/toolkit and record the decision while keeping public command/state/skin contracts renderer-neutral.

### First installed Default slice

An ordinary operator must be able to:

1. find/import and inspect a fixture profile with provenance;
2. patch/select fixtures and groups;
3. create/edit/duplicate a Static Look;
4. use complete profile-backed visual controls;
5. understand Set/Release/ForceZero;
6. simulate and explicitly arm bounded selected-fixture preview;
7. save/reopen/Undo/Redo;
8. understand output/DJ/controller/safety health;
9. reach raw DMX only through Advanced.

Only after this works should the shell expand aggressively into Autoloops, Live overrides, mapping editor, SoundSwitch Reference, and broad skin customization.

### Reusable controls after toolkit selection

Build one lightweight accessible primitive system for:

- buttons/toggles/pads;
- faders/knobs/rate controls;
- RGB/RGBW/RGBWAUV color controls and swatches;
- XY position pads;
- segmented/tab/navigation controls;
- searchable lists/trees;
- status/validation/ownership/safety states;
- contextual inspector/layout primitives.

Do not bundle a large control framework without license, footprint, maintenance, accessibility, and renderer-fit review.

---

## 11. Work Packet F — perceptual fade and show quality

**Priority:** P1 after convergence; tracker #88.

Physical feedback shows mathematically linear 8-bit transitions may still look stepped/harsh. Solve this at the engine/profile/transition layer rather than hand-editing every loop:

- parameter-family easing/curve profiles;
- dimmer/color interpolation policy;
- deterministic temporal smoothing or appropriate dither where safe;
- fixture calibration/gamma/white-balance hooks;
- exact Cut versus Linear versus eased semantics;
- phase/beat-preserving return from Look/override ownership;
- physical IR-4/tube comparison and repeatable evidence.

Do not hide decoder mistakes with smoothing. Source timing/target correctness and transition quality are separate acceptance dimensions.

---

## 12. Lane ownership

### Convergence owner only until Gate A closes

- `AGENTS.md`;
- `docs/00_START_HERE.md`;
- `docs/06_PRIORITIZED_BACKLOG.md`;
- `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`;
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`;
- top-level CMake/Make/workflow/package manifests;
- generated registries/adapters;
- any unresolved main/PR #86 conflict.

### #90 migration lane

Prefer new/exclusive files such as:

```text
native-core/include/emberlights/soundswitch_2026_source.hpp
native-core/src/soundswitch_2026_source.cpp
native-core/include/emberlights/soundswitch_autoloop_adapter.hpp
native-core/src/soundswitch_autoloop_adapter.cpp
native-core/tests/test_soundswitch_2026_source.cpp
native-core/tests/test_soundswitch_autoloop_adapter.cpp
```

Reserve before touching shared migration IR, Studio document/preview, project I/O, Make/CMake, or UI/view-model files.

### #89 OS2L lane

Own OS2L server/service/capture/diagnostics files. Coordinate any `runner.*`, `windows_app.cpp`, registry, or shared diagnostics edit. Do not edit migration or UI domain behavior from this lane.

### UI evidence lane

Stay inside the existing comparison/lab/runtime and renderer-neutral shell-model files until #37 accepts production activation. Do not edit migration or OS2L domain behavior.

---

## 13. Mandatory agent protocol

Every implementation agent must:

1. read this handoff, #87, and the smallest owning issue/handoff;
2. state exact base commit/branch and owning issue;
3. reserve exact files before changing shared code;
4. deliver one bounded vertical slice;
5. reuse existing authorities/services/contracts before adding a system;
6. keep source evidence, editable Studio data, compiled packages, Runner state, and physical qualification distinct;
7. add behavioral tests with behavioral changes;
8. report exact commands/results, commit SHA, changed files, remaining unknowns, and claim boundary;
9. update the owning issue/checkpoint rather than creating redundant planning;
10. stop when the slice passes—do not absorb the next lane without a new reservation.

Use claim terms accurately:

- **Implemented:** code exists on the stated tree.
- **Software-tested:** deterministic tests passed in the stated environment.
- **Packaged:** exact payload/installer contract was verified.
- **Installed-Windows-tested:** native install/launch/persistence/uninstall was observed.
- **Physical-output-tested:** exact backend/profile/mode/address produced observed fixture response.
- **Gig-qualified:** soak/fault/operator/live-event gates passed.
- **Exact / DeterministicallyTranslated / Approximated / PreservedOpaque / Unsupported / Conflicted / MissingDependency / RejectedUnsafe:** migration evidence statuses, not interchangeable marketing terms.

Never promote a claim by inference.

---

## 14. Explicit stop/defer list

Until Gate A and the first #90 vertical slice are complete, do not spend primary build time on:

- WOLFMIX parsing/emulation;
- broad track-specific scripted-audio decoding unless needed to prove the current source manifest boundary;
- AI/model-driven lighting;
- broad new default packs;
- another Autoloop source/runtime model;
- another fixture/catalog architecture;
- new top-level Win32 UI;
- broad skin designer/marketplace/cloud/plugin work;
- Control One OLED/storage/proprietary-DMX expansion;
- macOS;
- generalized remote/tablet/event-workflow superiority;
- new preview numbers without a meaningful validated operator slice.

These remain future goals, not next work.

---

## 15. Exact next build-session queue

### Agent 1 — Integration owner

**Start:** latest `main` after this coordination package lands; verify `86694f0` is an ancestor.  
**Task:** create the convergence branch, integrate exact `7efd0b0`, resolve only merge/governance/artifact conflicts, run the complete gate, publish one convergence PR, and make it the sole candidate for implementation `main`.

### Agent 2 — SoundSwitch adapter owner

**Start:** analysis/read-only until Agent 1 publishes the convergence anchor; then branch from that exact commit.  
**Task:** implement #90 from bundle probe through the smallest testable A/B parser slice for `Red - Smooth Pulse`: exact Venue map, placement ordering, source locators, raw-field retention, and deterministic evidence. Do not generate broad project content.

### Agent 3 — OS2L evidence owner

**Start:** inspect the existing Blackout-feedback slice; code only from the convergence anchor.  
**Task:** package/run installed `os2l_capture`, capture plain/on/off/keepalive and song transitions, reconcile the result with current send/feedback code, and define the smallest lifecycle fix. Do not start the generic command bridge.

### Agent 4 — UI/toolkit evidence owner (optional parallel lane)

**Start:** convergence anchor.  
**Task:** close one missing #37 measurement/comparison using the existing Fixtures + Static Looks workflow. No new product behavior or Win32 form.

### Owner/operator evidence

Use installed builds only for explicitly named evidence tasks. Do not ask the owner to compare dozens of generated loops. The next migration comparison is exactly one `Red - Smooth Pulse` vertical slice after its source-map report is correct.

---

## 16. Definition of a successful next build session

The next session succeeds only when it produces:

1. one published convergence PR/commit preserving both implementation histories and becoming the obvious base;
2. current `main`/handoffs/issues no longer routing agents back to completed raw-core or Autoloops-foundation work;
3. #90 parser/adapter implementation or a narrowly evidenced blocker at an exact field/record—not another approximate Ember project;
4. #89's existing feedback slice preserved and installed VirtualDJ behavior captured, or the exact capture package ready for one bounded operator run;
5. fixture qualification no longer hard-blocking output-disabled import/project testing while support/safety status remains truthful;
6. no new parallel engine, project model, UI framework/runtime, or speculative vendor migration path.

That is the shortest route from EmberLights' substantial foundation to a coherent, testable SoundSwitch replacement.