# EmberLights Current Build Execution Handoff — 2026-08-19

**Status:** Binding coordination document until issue #87 Gate A lands one accepted convergence anchor on `main`.

**Primary trackers:** #87 (product convergence), #90 (current SoundSwitch 2026 source adapter), #89 (OS2L/VirtualDJ reliability), #52/#79 (fixture and physical evidence), #37/#32/#33 (replacement shell and first product slice), #88 (perceptual fade quality).

**Read this before changing code, opening another implementation branch, expanding a feature, or generating another migration project.**

---

## 1. Executive directive

The immediate problem is no longer a lack of architecture, engines, models, or feature ideas. EmberLights now has a substantial amount of valuable implementation distributed across a diverged integration branch and a newer `main` history. The critical risk is **building on the wrong truth, reimplementing completed work, or extending another temporary path before the existing work is converged and proven**.

The next build session must therefore proceed in this order:

1. **Converge repository truth.** Make one branch contain both the current `main` evidence and PR #86 implementation without silently dropping either history.
2. **Turn the current SoundSwitch research into a real source adapter/import vertical slice.** Begin with one exact current-project Autoloop, not another hand-authored approximation.
3. **Finish OS2L P0 through installed VirtualDJ evidence.** The integration branch already contains the bounded Blackout feedback software slice; the remaining work is capture, lifecycle truth, service ownership, and real launch/reconnect qualification.
4. **Close only the physical/fixture gates that affect correctness and support claims.** Exact fixture qualification must not prevent the user from opening/importing/testing a project with outputs disabled and explicit unverified status.
5. **Finish the measured replacement-shell decision and activate one product-shaped Fixtures + Static Looks slice.** Do not add more product-facing Win32 forms.
6. **Then improve perceptual fades, broader Autoloop content, Live workflow, mappings, and full parity.**

No agent should start a broad feature pass from current `main` or PR #86 independently while Gate A is unresolved.

---

## 2. Exact repository truth at this checkpoint

### Current refs

| Ref | Exact head | Meaning |
|---|---|---|
| `main` | `86694f02d89ef856b06d846026fe558e39206f90` | Newest SoundSwitch 2026 decoder/pilot evidence, plus the main-only Preview 97 installer upload history. It does **not** contain the large current integration implementation. |
| PR #86 / `agent/backyard-party-v2` | `7efd0b083cc7c42df3b2a5ee3e760687a15e390b` | Current broad implementation line: Autoloops V2, Studio seams, fixture-control work, qualification tools, UI course correction/Slint lab, Preview 101 lineage, OS2L Blackout feedback software, and independently added copies of the decoder research. |
| merge base | `47bb45d6e84609a7a73358517ed77e492c29f882` | Last common accepted ancestor. |

GitHub currently reports PR #86 as:

- **23 commits ahead** of the merge base/current comparison;
- **7 commits behind** current `main`;
- **diverged** and **not mergeable**;
- 276 changed files and a very large implementation surface.

### What the seven main-side commits represent

The main-side divergence is not seven unrelated product features. It is principally:

1. `fdcc8a43e9b8649077e5d872c7cf6cac8095482d` — uploaded `installer/EmberLights-0.1.0-preview.97.0-Setup.exe` into source history;
2. a sequence of SoundSwitch v2.2.2/current-2026 decoder and pilot evidence commits ending at `86694f0`.

PR #86 independently added the decoder research files in its latest commit, but the blobs/history are not assumed equivalent merely because the filenames and prose appear similar. Gate A must compare them deliberately.

### CI truth

The current PR head has no valid GitHub-hosted build/test result. Recent workflow runs failed before runner assignment because of the repository/account Actions payment or spending-limit gate. Empty jobs are infrastructure failures, not source regressions and not source validation.

PR #86 contains extensive reported local build/test/package evidence from its component checkpoints. That evidence is useful, but the converged tree still requires a fresh complete local/Windows validation pass before merge or installer publication.

### Installer truth

Preview 101 is the latest identified installed legacy-shell testing line in the integration documentation. It is a small session-ownership safety/defect preview over the frozen transitional Win32 shell. It is **not** the replacement-shell product UI and it does not represent the current PR #86 head.

Do not increment the preview number merely to ship rearranged legacy UI or model-only changes. The next installer must either:

- be an explicitly labeled diagnostic/lab build; or
- activate a meaningful product-shaped slice after convergence and its required validation.

---

## 3. Work that already exists and must not be rebuilt

Agents must inspect the converged implementation before assuming any item below is missing. Open issue checkboxes are stale in several places.

### 3.1 Core/Runner/output foundation

The implementation lineage already contains:

- deterministic project/compiler/Runner boundaries;
- sparse semantic property ownership and two-universe rendering;
- Art-Net, sACN, ENTTEC-style USB output foundations, SoundSwitch Micro session/qualification tooling, output routing, Blackout authority, diagnostics, and frame attribution;
- MIDI/WinMM foundations, mapping contracts, session-scoped ownership cleanup, and controller lifecycle seams;
- raw hardware test, Runner-frame inspection, evidence joining, and qualification/attestation foundations;
- non-droppable safety authority and fixed-capacity/no-model-call Runner rules.

Do not create another engine, resolver, output router, project model, or hardware-test subsystem.

### 3.2 Fixture truth and Static Looks

The integration lineage already contains:

- profile-backed fixture capabilities and parameter catalog;
- direct channels plus named DMX ranges/functions represented through one fixture-control identity;
- structured profile authoring/workbench models;
- fixture/group target validation and capability-aware assignments;
- Static Look authoring, transactional document mutation, exact offline preview through the production compiler/renderer, and bounded physical-preview foundations;
- renderer-neutral Fixtures + Static Looks shell model and an opt-in Slint lab;
- explicit UI course correction freezing further product-facing Win32 editor/form growth.

Do not add fixture-specific repair buttons, duplicate channel models, or raw-DMX-first product controls.

### 3.3 Autoloops V2 and AutoScript

The integration lineage already contains substantial Autoloops V2 work:

- signed 64-bit 960-PPQ source/event model;
- source validation/canonical serialization and deterministic compilation;
- authoring operations, generation checks, dependency reporting, placement/content separation, Undo/Redo, populate/reset semantics;
- original 128-placement EmberLights starter content with stable management identity;
- rich-source persistence through the existing project unknown-record compatibility channel;
- deterministic rule-based AutoScript proposal, immutable review, bounded cancellation/budgets, and explicit commit;
- Studio AutoScript bridge through `StudioDocumentService`;
- output-disabled V2 preview and project palette reference realization;
- compiled package/director/Runner path with explicit legacy/V2 ownership and return behavior.

Before writing new Autoloop model/runtime/generator code, prove the needed behavior is absent from the converged tree. The current priority is source migration fidelity and product exposure—not another source model or another hand-authored default-pack pass.

### 3.4 UI platform direction

The accepted direction is:

- legacy Win32 is a frozen transitional/Safe bridge;
- no new top-level Win32 editor, property form, or product-skin claim;
- one renderer-neutral command/state/capability/component registry;
- Slint/C++ is the first product-shaped candidate, WinUI 3 is the bounded Windows comparison, and Direct2D/Win32 is the Safe baseline;
- product controls are task-facing visual controls over profile semantics; raw DMX is Advanced diagnostics;
- reusable shared primitives/components are desired, but must be built inside the accepted renderer/runtime path rather than becoming a third UI framework.

Do not restart UI architecture, create another skin runtime, or add generic component libraries before the toolkit/runtime gate is decided.

### 3.5 OS2L software checkpoint

PR #86 head includes the first bounded outbound feedback implementation:

- `Os2lTcpServer` send/partial-send/coalescing state for Blackout feedback;
- feedback counts/errors/bytes and synchronization status;
- Runner publication of authoritative Blackout feedback state;
- native tests added in the head commit.

This is **software-complete only for the first bounded feedback slice**. It does not prove the installed VirtualDJ wire behavior, launch-order discovery, song transition lifecycle, application-owned service extraction, or generic command bridge.

### 3.6 SoundSwitch decoder evidence

The repository now records source-derived evidence for:

- exact current project identity and hashes;
- 112 Autoloop placements across four banks;
- exact placement-array ordering rather than raw entry order;
- A-record normalized intensity timelines;
- B-record timed RGB plus direct-emitter endpoint evidence;
- Static Look intensity/RGBWAUV tables;
- exact current-project Venue target IDs;
- the failed Pilot 01 caused by old-project target-map contamination;
- a corrected-map one-loop probe with a deliberately labeled test-only red fallback.

This evidence is enough to begin issue #90. It is not enough to claim exact interpolation, exact unidentified packed-byte semantics, or a faithful full 112-loop conversion.

---

## 4. What remains genuinely unproven or incomplete

The following are real gaps—not stale checklist artifacts:

1. **One accepted repository tree.** There is no current mergeable branch that combines main and PR #86.
2. **Production SoundSwitch source adapter/import.** Research and generated comparison artifacts exist; a bounded version-specific parser/evidence IR/review/re-import path does not yet exist in the product.
3. **OS2L installed behavior.** The exact VirtualDJ packet/release/connection behavior and automatic launch/reconnect matrix remain unproven.
4. **Application-owned OS2L lifetime.** The listener/discovery lifecycle still needs to be cleanly separated from Runner/song transport where current code remains coupled.
5. **Physical/gig qualification.** Software evidence must remain distinct from installed Windows, physical response, soak, and live-event qualification.
6. **Replacement-shell decision.** Slint has a meaningful lab, but the accepted #37 comparison and production toolkit decision are not closed.
7. **Product-shaped activation.** The first Fixtures + Static Looks Default workflow is not yet the installed primary product shell.
8. **Perceptual fade quality.** Physical feedback shows stepped/harsh transitions; issue #88 remains a real rendering/transition-quality lane.
9. **Full SoundSwitch parity.** Track-specific scripting, broad migration coverage, movement/effects, broader fixture corpus, controller UX, and complete Live/Studio journeys remain later work.

---

## 5. Binding critical path

```text
Gate A repository convergence
        |
        +--> #90 current SoundSwitch source adapter/import vertical slice
        |
        +--> #89 installed OS2L capture + lifecycle/service proof
        |
        +--> owned-rig evidence reconciliation
        |
        +--> #37 toolkit decision -> #32 Safe/runtime -> #33 first Default product slice
                     |
                     +--> #88 perceptual fade quality
                     +--> broader Autoloops/Live/mapping/migration parity
```

The lanes after Gate A may proceed in parallel only when they use distinct files and one shared convergence anchor.

---

## 6. Work Packet A — Gate A repository convergence

**Priority:** P0, blocking all broad implementation.

**Owner:** one integration agent/worktree. Other agents may inspect/read, but must not independently merge shared histories.

### 6.1 Required branch procedure

1. Create `integration/convergence-2026-08-19` from exact `main@86694f02d89ef856b06d846026fe558e39206f90`.
2. Merge or replay exact PR #86 head `7efd0b083cc7c42df3b2a5ee3e760687a15e390b` into that branch with both histories preserved.
3. Do not resolve conflicts by blanket `ours`/`theirs` or by constructing a two-parent commit whose tree silently discards one parent.
4. Resolve conflicts by domain and record each deliberate choice.
5. Make the first convergence commit behavior-preserving: no new feature/refactor mixed into the merge.
6. Keep `main` and PR #86 read-only until the convergence PR is validated.

### 6.2 Mandatory reconciliation areas

#### SoundSwitch research

Compare every current-2026/v2.2.2 research file from both histories. Retain the latest accurate conclusions, including:

- current project placement-array ordering;
- current source target map `197–201` and `89–156`;
- invalidation of Pilot 01;
- corrected-map Pilot 02A limitations;
- no unrelated color borrowing in production import;
- format-1 capacity/chunking as comparison only.

No evidence file may regress to the older `345+` tube mapping as current truth.

#### Preview 97 binary/source history

The main-only `installer/EmberLights-0.1.0-preview.97.0-Setup.exe` must be handled explicitly:

- preserve its identity/checksum and a durable artifact/tag/branch pointer before removing it from ordinary source history, if removal is chosen;
- do not accidentally repackage it as current;
- do not let a historical binary dictate the converged source tree;
- record the artifact policy decision in the convergence report.

#### Governance/current-state docs

Reconcile at least:

- `AGENTS.md`;
- `docs/00_START_HERE.md`;
- `docs/06_PRIORITIZED_BACKLOG.md`;
- `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`;
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`;
- active handoffs/checkpoints;
- product/artifact pointers.

This handoff and issue #87 outrank stale “raw Micro proof is the first task” language. Raw/physical proof remains important, but it is not the generic first coding task after the existing evidence lineage.

#### Build systems and generated contracts

Resolve CMake/Make/workflow/registry/generated-file conflicts by regenerating from the accepted source of truth. Do not manually combine generated outputs while leaving generators stale.

#### Installer/package policy

Retain the deterministic package-contract work and uninstall coverage. Do not publish a product installer from the convergence commit until the exact converged payload and version boundary are established.

### 6.3 Required equivalence review before superseding PRs

After the convergence tree is green, compare older drafts #53, #82, #83, #84, #85, and PR #86 itself against the converged tree. For each PR, record:

- unique implementation present/missing/rejected;
- unique tests present/missing/rejected;
- unique docs/evidence present/missing/rejected;
- artifact lineage retained;
- final disposition.

Close/supersede only after content equivalence, not by title or assumed chronology.

### 6.4 Convergence validation gates

At minimum:

- `git diff --check`;
- clean generated UI registry and Ember Action adapter regeneration/check;
- surface-contract gate;
- complete warnings-fatal native `all` and `test` suites;
- CMake configure/build/CTest for supported targets;
- SoundSwitch migration/parser existing regressions;
- Autoloops V1/V2 authoring, persistence, preview, runtime, Runner, and AutoScript suites;
- fixture/profile/Static Look/physical-preview suites;
- OS2L server/Runner feedback tests;
- package-contract tests;
- supported Windows x64 warnings-as-errors build;
- Slint lab contract/compiler check;
- dry-run Runner smoke and scheduler/resource guard tests.

Because GitHub Actions is currently infrastructure-blocked, attach exact local command/output summaries and environment identity. Do not represent an empty Actions run as validation.

### 6.5 Gate A exit condition

Gate A closes only when:

- one convergence PR is mergeable and green on the available evidence path;
- its branch is a descendant of both current `main` and PR #86 histories or otherwise preserves/equates both transparently;
- `main` becomes the unambiguous implementation truth;
- this handoff/current-start routing exists on `main`;
- #90 and #89 can branch from one exact accepted commit;
- no active draft contains unreviewed unique implementation.

---

## 7. Work Packet B — #90 current SoundSwitch source adapter/import

**Priority:** P0 immediately after Gate A anchor.

**Goal:** turn the current decoder evidence into a read-only, version-specific, evidence-carrying Studio migration path. This is the real work required to bring the user's current SoundSwitch project into EmberLights.

### 7.1 Production architecture

```text
source bundle manifest/probe
  -> exact source-version parser
  -> exact current-project Venue hierarchy/map
  -> Autoloop catalog/placement/length parser
  -> .ssfile A/B records + Static Look tables
  -> evidence-bearing candidate IR
  -> explicit destination target/profile reconciliation
  -> Autoloops V2 semantic source + Static Look proposal
  -> output-disabled production preview
  -> reviewed one-transaction commit
  -> stable idempotent re-import
```

Reuse the accepted migration IR, `AutoloopSourceDocument`, authoring/persistence/preview/compiler services, profile-backed fixture properties, and `StudioDocumentService`. Do not create another project/timeline/color/fixture model.

### 7.2 First vertical slice

Implement only current-project Medium Bank Slot 1:

- `SSAutoLoop1.ssfile`;
- `Red - Smooth Pulse`;
- exact source file SHA-256 already recorded in #90;
- uplights `198–201`;
- tube cells `90–105`, `107–122`, `124–139`, `141–156`.

Acceptance requires exact source identity/placement/length, exact current-project source map, raw and interpreted record evidence, output-disabled V2 proposal, save/reopen, and idempotent re-import.

### 7.3 Fail-closed semantic rules

- Target IDs are resolved from the exact source Venue database; never use another project lineage.
- No name-derived choreography.
- No raw DMX copying into unrelated destination profiles.
- No color borrowed from unrelated targets.
- Missing local color context is an explicit blocker/status, not an invitation to fabricate.
- Unsupported UV on tube RGBWY cells is degraded, not reassigned.
- Strobe/movement/wheels/unknown attributes stay off/opaque until decoded and safety-qualified.
- Interpolation and unidentified packed bytes remain translated/opaque until controlled-delta evidence promotes them.
- Format-1 sampled helper-Look projects remain comparison artifacts only; production import targets V2.

### 7.4 User-testability rule

The importer/project must be openable and reviewable without exact fixture qualification. Lack of qualification must:

- mark the patch/output as unverified;
- keep outputs disabled by default;
- require explicit review/arming for physical output;
- never block source inspection, mapping review, preview simulation, save/reopen, or migration-report generation.

This preserves safety without preventing the user from testing migration progress.

### 7.5 Expansion rule

Do not expand to slots 2–4 or all 112 loops until the first vertical slice is source-map-correct and its unresolved semantics are explicit. A mismatch returns to parser/evidence work, not hand-authored replacement content.

---

## 8. Work Packet C — #89 OS2L/VirtualDJ reliability

**Priority:** P0 after convergence; may run parallel to #90 with isolated files.

### 8.1 Integrate the existing software slice first

PR #86 head already adds bounded Blackout feedback send support and tests. Preserve it during convergence, then verify its exact authority path:

- initial authoritative Blackout state after connection;
- state transitions from every supported control surface;
- bounded/coalesced partial-send behavior;
- send failure diagnostics/recovery;
- no socket work on the DMX scheduler;
- connection loss never clears authoritative Blackout.

Do not reimplement this slice from issue prose before reviewing the converged code.

### 8.2 Next executable slice: installed raw capture

Package/expose `os2l_capture` and record exact installed VirtualDJ behavior for:

```vdjscript
os2l_button 'blackout'
os2l_button 'blackout' on
os2l_button 'blackout' off
os2l_button 'EmberLights Keepalive' off
```

Also capture load/play/stop/replace transitions and whether VirtualDJ closes TCP or only stops transport messages.

This evidence decides whether the remaining defect is serialization/feedback semantics, discovery policy, listener ownership, or a VirtualDJ mapping assumption.

### 8.3 Then extract/finish application-owned service lifetime

The normal product path must work in either launch order and across song/app transitions without pressing a lighting pad to wake the connection. Separate:

- listener state;
- DNS-SD advertisement;
- TCP client state;
- transport/beat sync state;
- connection/session epoch;
- authoritative feedback state.

Do not begin the generic `os2l_cmd` bridge until feedback and lifecycle P0 are proven with real VirtualDJ.

### 8.4 Completion evidence

The installed journey must cover both app launch orders, both app restarts, repeated stop/play, load A→B, client reconnect, port conflict recovery, discovery and direct-IP fallback, true Blackout toggle feedback, external Blackout reflection, and stale-session safety.

---

## 9. Work Packet D — fixture, patch, and physical evidence closure

**Priority:** P0/P1 evidence lane; it must not displace Gate A or #90.

### 9.1 Current owned destination rig truth

For the current migration/bench destination, use:

- 4 × Both Lighting IR-4 in 10CH mode at U1 `001`, `011`, `021`, `031`;
- 4 × BO-TUBE192/360 Tubes in 80CH mode at U1 `041`, `121`, `201`, `281`;
- no additional generic “Both Lighting uplight” fixtures in the EmberLights destination project.

The SoundSwitch source Venue's uplight identities describe the source fixture family. Migration reconciles their semantic intent to the current IR-4 destination; it does not add duplicate destination uplights.

### 9.2 Qualification versus testability

Exact profile/patch qualification remains required for:

- physical support claims;
- evidence-backed attestation;
- automatic output enablement or gig-qualified status;
- claiming exact emitter/channel behavior.

It is **not** required merely to:

- import/open a project;
- inspect the source map/migration report;
- simulate output-disabled frames;
- edit mappings/profiles;
- save/reopen;
- explicitly run a bounded user-authorized test with visible unverified status.

Remove or bypass any hard UI gate that prevents source/project testing solely because exact fixture qualification is incomplete. Replace it with truthful status, disabled output by default, and explicit safe arming.

### 9.3 Remaining evidence session

Use existing tools to close only current unknowns:

- IR-4 10CH R/G/B/W/A/UV one-hot truth, master, strobe-off, mode/address;
- tube cell order/direction and RGBWY behavior;
- SoundSwitch Micro raw frame vs exact Runner frame;
- blackout, reconnect, unplug/replug;
- bounded selected-fixture preview timeout/fault/terminal blackout;
- saved exact evidence/attestation tied to source/profile/mode/address/backend.

Do not create another hardware-test UI or infer physical output from accepted USB writes.

---

## 10. Work Packet E — replacement shell and first real product slice

**Priority:** P1 after Gate A; #37 evidence can proceed parallel to #90/#89 if files are isolated.

### 10.1 No more legacy-shell expansion

The Win32 shell may receive only:

- safety fixes;
- defect fixes;
- accessibility fixes;
- bridge removal/containment;
- explicitly labeled diagnostic exposure needed to gather evidence.

No new top-level Win32 editor, inspector, property form, or product skin.

### 10.2 Finish #37 with the same workflow

Evaluate Slint/C++, WinUI 3, and Direct2D/Win32 Safe using the same Fixtures + Static Looks model, commands, queries, persistence, preview, and fault states. Measure native Windows packaging, DPI, accessibility/UI Automation, keyboard navigation, memory, startup, repaint, long lists, resize, preview timeout/fault, and scheduler effect.

Select one product renderer/toolkit and record the decision. Keep public command/state/skin contracts renderer-neutral.

### 10.3 First installed Default slice

The first accepted product slice must let an ordinary operator:

1. find/import and inspect a fixture profile with provenance;
2. patch/select fixtures/groups;
3. create/edit/duplicate a Static Look;
4. control complete profile-backed parameters with appropriate visual controls;
5. understand Set/Release/ForceZero;
6. simulate and explicitly arm bounded selected-fixture preview;
7. save/reopen/Undo/Redo;
8. see output/DJ/controller/safety health;
9. reach raw DMX only through Advanced.

Only after this works should the product shell expand aggressively into Autoloops authoring/performance, Live overrides, mapping editor, SoundSwitch Reference, and broad skin customization.

### 10.4 Reusable UI primitives

After the toolkit/runtime decision, build one lightweight reusable design system for:

- buttons/toggles/pads;
- faders/knobs/rate controls;
- RGB/RGBW/RGBWAUV color controls and swatches;
- XY position pads;
- segmented/tab/navigation controls;
- searchable lists/trees;
- status pills, validation, ownership and safety states;
- contextual inspector/layout primitives;
- accessible focus/keyboard/automation behavior.

Do not source or bundle a large control framework without license, footprint, maintenance, accessibility, and renderer-fit review.

---

## 11. Work Packet F — perceptual fade and show quality

**Priority:** P1 after convergence and alongside product exposure; tracker #88.

Physical feedback indicates that mathematically linear 8-bit transitions can still look stepped or harsh. Address this at the engine/transition layer rather than hand-editing every loop:

- parameter-family easing/curve profiles;
- dimmer/color interpolation policy;
- temporal smoothing or appropriate dither where safe and deterministic;
- fixture calibration/gamma/white balance hooks;
- exact Cut vs Linear vs eased transition semantics;
- phase/beat-preserving return from Look/override ownership;
- physical IR-4/tube comparison and repeatable evidence.

Do not hide decoder mistakes with smoothing. Source timing/target correctness and transition quality are separate acceptance dimensions.

---

## 12. Shared-file and lane coordination map

### Convergence owner only until Gate A closes

- `AGENTS.md`;
- `docs/00_START_HERE.md`;
- `docs/06_PRIORITIZED_BACKLOG.md`;
- `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`;
- `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`;
- top-level CMake/Make/workflow/package manifests;
- generated registries/adapters;
- any file with unresolved main/PR #86 conflict.

### #90 migration lane likely ownership

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

### #89 OS2L lane likely ownership

- `showcore/os2l_server.*`;
- extracted DJ transport/service files;
- Runner/app connection adapters/status only where required;
- OS2L capture/diagnostics tests/tools;
- canonical registry files only after an approved user-visible command/state change.

Coordinate any `runner.*`, `windows_app.cpp`, registry, or shared diagnostics edit explicitly.

### UI lane

Remain inside the Slint lab/comparison/runtime files and renderer-neutral shell model unless Gate C accepts production activation. Do not edit migration or OS2L domain behavior from the UI lane.

---

## 13. Mandatory agent operating protocol

Every implementation agent must:

1. Read this handoff, issue #87, and the smallest owning issue/handoff before editing.
2. State the exact base commit/branch and owning issue in its first repository checkpoint.
3. Reserve exact files before changing shared code.
4. Deliver one bounded vertical slice; do not absorb adjacent backlog because it is nearby.
5. Reuse existing authorities/services/contracts before adding a new type/system.
6. Keep source evidence, editable Studio data, compiled packages, Runner state, and physical qualification distinct.
7. Add behavioral tests with behavioral changes.
8. Report exact commands/results, commit SHA, changed files, remaining unknowns, and claim boundary.
9. Update the owning issue/checkpoint; do not create a new planning document unless it closes a real authority/evidence gap.
10. Stop when the slice's acceptance criteria pass. Do not continue into the next lane without a new reservation.

### Required language discipline

Use these terms accurately:

- **Implemented:** code exists on the stated exact tree.
- **Software-tested:** deterministic/local tests passed on the stated environment.
- **Packaged:** exact payload/installer contract was built and verified.
- **Installed-Windows-tested:** native installation/launch/persistence/uninstall was observed.
- **Physical-output-tested:** exact backend/profile/mode/address produced observed fixture response.
- **Gig-qualified:** soak/fault/operator/live-event gates passed.
- **Exact / DeterministicallyTranslated / Approximated / PreservedOpaque / Unsupported / Conflicted / MissingDependency / RejectedUnsafe:** migration evidence statuses, not interchangeable marketing terms.

Do not promote a claim to the next level by inference.

---

## 14. Explicit stop/defer list

Until Gate A and the first #90 vertical slice are complete, do not spend primary build time on:

- WOLFMIX parsing/emulation;
- track-specific scripted-audio decoding unless required to prove the current source manifest boundary;
- AI/model-driven lighting;
- broad new default content packs;
- another Autoloop source/runtime model;
- another fixture model/catalog architecture;
- new top-level Win32 UI;
- broad skin designer/marketplace/cloud/plugin work;
- Control One OLED/storage/proprietary DMX expansion;
- macOS;
- generalized remote/tablet/event-workflow superiority;
- new preview numbers without a meaningful validated operator slice.

These remain valid future goals; they are not the next work.

---

## 15. Exact next build-session queue

### Agent 1 — Integration owner

**Start:** `main@86694f0`  
**Task:** Create the convergence branch, integrate `7efd0b0`, resolve only merge/governance/artifact conflicts, run the complete gate, publish one convergence PR, and make it the sole candidate for `main`.

### Agent 2 — SoundSwitch adapter owner

**Start:** analysis/read-only until Agent 1 publishes the convergence anchor; then branch from that exact commit.  
**Task:** implement #90 slices SS26-1 through the smallest testable portion of SS26-4, beginning with bundle probe, exact Venue source map, placement ordering, and evidence-bearing A/B records for `Red - Smooth Pulse`. Do not generate broad project content.

### Agent 3 — OS2L evidence owner

**Start:** inspect/consolidate the existing Blackout feedback slice; code only from the convergence anchor.  
**Task:** package/run installed `os2l_capture`, capture plain/on/off/keepalive plus song transitions, reconcile the result with the current send/feedback implementation, and define the smallest lifecycle fix. Do not start the generic command bridge.

### Agent 4 — UI/toolkit evidence owner (optional parallel lane)

**Start:** convergence anchor.  
**Task:** close one missing #37 measurement/comparison using the existing Fixtures + Static Looks workflow. No new product behavior or Win32 form.

### Owner/operator evidence

Use the current installed build only for explicitly named evidence tasks. Do not ask the owner to compare dozens of generated loops. The next migration comparison is exactly one `Red - Smooth Pulse` vertical slice after its source-map report is correct.

---

## 16. Definition of a successful next build session

The next session is successful when it produces all of the following—not merely more code:

1. A single published convergence PR/commit that preserves both current histories and is the obvious base for new work.
2. Current `main`/handoffs/issues no longer route agents back to already-completed raw-core or Autoloops-foundation work.
3. #90 has a real parser/adapter implementation or a narrowly evidenced blocker at the exact field/record level—not another approximate Ember project.
4. #89's existing feedback slice is preserved and the installed VirtualDJ behavior is captured or the exact capture package is ready for one bounded operator run.
5. Exact fixture qualification no longer hard-blocks opening/importing/testing an output-disabled project; qualification status remains truthful and safety-gated.
6. No new parallel architecture, UI framework, engine, project model, or speculative vendor migration path was introduced.

That is the shortest route from EmberLights' substantial foundation to a coherent, testable SoundSwitch replacement.