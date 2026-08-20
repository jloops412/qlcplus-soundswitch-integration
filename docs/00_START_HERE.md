# Start Here

## Mandatory current execution checkpoint — 2026-08-19

Before changing code or scope, read:

1. `handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`;
2. `../AGENTS.md`;
3. issue #87;
4. the smallest owning issue/handoff for the assigned lane.

## Repository state

The protected pre-coordination implementation inputs are:

- `main@86694f02d89ef856b06d846026fe558e39206f90` — current-2026 SoundSwitch decoder/pilot lineage plus a historical Preview 97 installer upload;
- PR #86 / `agent/backyard-party-v2@7efd0b083cc7c42df3b2a5ee3e760687a15e390b` — broad implementation lineage: Autoloops V2, Studio/fixture/UI foundations, Preview 101 lineage, Slint Fixtures + Static Looks lab, qualification work, and the bounded OS2L Blackout-feedback software slice;
- merge base `47bb45d6e84609a7a73358517ed77e492c29f882`.

At review time PR #86 was 23 commits ahead, 7 behind, diverged, and non-mergeable.

Once this coordination package lands, **the latest `main` head—not `86694f0` itself—is the convergence starting point**. Verify `86694f0` remains its ancestor, then integrate exact `7efd0b0`. Do not extend either protected implementation line independently and do not resolve the split with blanket `ours`/`theirs` or a tree that silently drops a parent.

The immediate order is:

1. #87 Gate A repository convergence;
2. #90 current `2026.ssproj` source adapter/import vertical slice;
3. #89 installed VirtualDJ capture plus OS2L lifecycle/feedback reliability;
4. truthful fixture/physical evidence without blocking safe output-disabled import/project testing;
5. #37 toolkit decision, #32 Safe/runtime, and #33's first product-shaped Fixtures + Static Looks Default slice;
6. #88 perceptual fade quality, then broader Autoloops/Live/mapping/migration parity.

Older documents that present raw Micro proof or Autoloops-foundation construction as the universal first coding task are historical sequencing inputs. Remaining support/physical gates still matter; they do not justify rebuilding completed foundations or ignoring the branch split.

---

## Mission

Create a uniquely owned, general-purpose, gig-ready replacement for SoundSwitch. It must work for DJs and lighting operators with rigs unrelated to Love & Light Entertainment, match relevant SoundSwitch capabilities, and then surpass them with open hardware, safer persistence, portable semantic scripts, evidence-backed migration, event-aware workflows, and user-extensible control surfaces.

This is not a generic DMX console and not a WOLFMIX clone.

> **Product statement:** A SoundSwitch-first, automation-first DJ/event lighting workstation with a lightweight offline Runner, capable Studio, open MIDI/DMX choices, first-class migration, reusable control surfaces, and no dependency on one user's fixtures or hardware.

---

## Current implementation truth

The integration lineage already contains substantial foundations that must be converged rather than rebuilt:

- versioned/checksummed project model and deterministic compiler/Runner;
- semantic ownership/layer resolution and fixed-frame two-universe rendering;
- Art-Net, sACN, initial USB-DMX adapters, SoundSwitch Micro qualification tooling, frame attribution, diagnostics, Blackout, and recovery foundations;
- WinMM MIDI/learn/mapping and session-safe ownership foundations;
- Studio fixture/profile/patch/group services, structured channel-function models, capability-aware Static Look authoring, exact output-disabled preview, and bounded physical-preview foundations;
- canonical UI/Ember Action registry/code-generation foundations;
- a UI course correction freezing new product-facing Win32 forms and using one renderer-neutral Fixtures + Static Looks model plus an opt-in Slint lab;
- Autoloops V2 semantic source/compiler, authoring, original 128-placement starter pack, persistence, deterministic AutoScript, Studio transaction bridge, Palette realization, preview, runtime/director, and Runner integration;
- bounded OS2L Blackout-feedback software on PR #86, with installed VirtualDJ capture/lifecycle proof still open;
- current SoundSwitch evidence for exact bank/placement/length metadata, project-local Venue target mapping, A/B timeline records, Static Look intensity/RGBWAUV data, and the invalid old-map Pilot 01.

Preview 101 is the latest identified installed legacy-shell test line. It is a deliberately small ownership-safety preview—not the replacement-shell product UI and not the current PR #86 source head.

The latest stacked beta candidate on 2026-08-20 is Preview 104: it preserves Preview 103's application-owned OS2L listener/capture path and adds the first product-callable #90 current-2026 import slice. The File import flow and migration CLI can create a separate output-disabled project containing exact `Medium / S1 / Red - Smooth Pulse` catalog/target/timeline evidence, mapped to four IR-4 fixtures at U1 `001/011/021/031` and four tube blocks at `041/121/201/281`, with no duplicate generic uplights. The unsigned Windows x64 testing bundle is bound to source commit `9e96c44dff794a16fbdfcb4f5efc68e2f02645fe`, published from draft PR #95, and recorded in `docs/handoffs/BACKYARD_PARTY_OPERATOR_PREVIEW_104_HANDOFF_2026-08-20.md`. It is one decoded loop, not broad SoundSwitch parity; installed Windows, VirtualDJ, physical output, and the other source content remain open evidence gates.

Hosted GitHub Actions is currently blocked before runner assignment by the account/repository Actions payment or spending-limit gate. Empty workflow jobs are not validation. The converged tree requires fresh complete local/Windows evidence before merge or publication.

---

## Immediate product truth

### SoundSwitch migration

SoundSwitch import is core. Current research is implementation-ready for proven fields, and issue #90 owns the first production vertical slice.

Binding rules:

- parse the exact source project/version and exact Venue hierarchy;
- target IDs are project-local and never reused from another source lineage;
- use decoded placement arrays rather than raw entry order;
- retain source artifact/range evidence and unknown bytes;
- never infer choreography from names;
- never copy raw source DMX into unrelated profiles;
- missing color context is explicit and never borrowed from another target;
- production import targets Autoloops V2, not sampled format-1 helper-Look chunking;
- imported content remains output-disabled until review;
- exact fixture qualification must not block opening, inspecting, simulating, editing, saving, or re-importing an output-disabled project. It gates support claims and automatic output enablement.

The first vertical slice is exactly `Medium / S1 / Red - Smooth Pulse`. Do not generate all 112 loops before that one slice proves its source map, evidence, preview, save/reopen, and idempotent re-import.

### Current destination rig

For the current migration/bench destination:

- four Both Lighting IR-4, 10CH, U1 `001/011/021/031`;
- four BO-TUBE192/360 Tubes, 80CH, U1 `041/121/201/281`;
- no duplicate generic Both Lighting uplight destination fixtures.

Source fixture identities are reconciled semantically to destination profile capabilities; they are not copied as duplicate destination fixtures or raw channels.

### OS2L / VirtualDJ

OS2L is an application-owned transport over canonical commands/state. Listener/discovery lifetime is separate from Runner/song transport; TCP connection and beat/sync are separate facts; authoritative EmberLights state drives outbound feedback; network work stays off the DMX scheduler.

PR #86 already has the first bounded Blackout-feedback implementation. The next task is installed raw capture of plain/on/off/keepalive and song transitions, then the smallest service-lifetime fix. Generic `os2l_cmd` mapping waits until P0 behavior is proven.

### UI direction

**UI course correction (2026-08-14):** the reusable domain models and contracts remain authoritative, while the legacy Win32 shell is frozen as a transitional/Safe bridge rather than EmberLights Default. New product presentation proceeds through the renderer-neutral Fixtures + Static Looks replacement-shell gate; raw DMX remains Advanced diagnostics.

Legacy Win32 is a transitional/Safe bridge. No new top-level Win32 editor/property form/product skin is accepted. #37 measures Slint/C++ against a bounded WinUI 3 comparison and Direct2D/Win32 Safe baseline using the same real Fixtures + Static Looks workflow. Public command/state/skin contracts remain renderer-neutral. Raw DMX stays Advanced.

---

## Accepted product invariants

- Windows launches first; V1 exposes exactly two universes.
- VirtualDJ/OS2L is first; Serato follows after VirtualDJ is stable.
- Predictive hold, live-audio fallback, tap tempo, then safe unsynchronized behavior.
- Control One is the first bundled MIDI profile; proprietary DMX/OLED/storage remain isolated experiments.
- Any MIDI controller maps to canonical actions/parameters.
- Same-PC and separate-PC modes are equal.
- SoundSwitch workflow/import outranks WOLFMIX ideas.
- Original semantic/layered core; optional QLC+ output and Studio-only QXF/OFL compatibility boundaries.
- OFL enters through a versioned Studio adapter/quarantine; Runner consumes immutable compiled profile snapshots.
- Autoloops use scalable 32-slot banks; four banks are a controller/UI window, not an engine limit.
- Runner contains no AI/model calls and stays deterministic/fixed-capacity.
- Default, Reference, Safe, user skins, keyboard, MIDI/controllers, DJ commands, Ember Actions, and future remotes share one versioned command/state/capability model.
- Skins are validated presentation/binding packages, not alternate engines.
- Ember Actions are typed bounded compositions, not arbitrary code and not a show-timing engine.
- User-visible changes reconcile registries, generated artifacts, surfaces, actions/mappings/profiles, schemas, compatibility/deprecation, tests, and migration.
- Exact fixture qualification gates physical/support claims—not safe output-disabled testability.

---

## What remains open

- one accepted implementation convergence tree on `main`;
- production current-2026 SoundSwitch source adapter/import;
- installed VirtualDJ packet/lifecycle evidence and application-owned OS2L reliability;
- installed Windows, physical-output, soak, and gig qualification for claimed backends/profiles;
- production toolkit/renderer and trusted Safe activation;
- first installed product-shaped Fixtures + Static Looks Default journey;
- exact SoundSwitch interpolation/unidentified packed-byte semantics and broader movement/effect/source-version coverage;
- perceptual fade quality;
- broader fixture corpus, track scripting, complete Studio/Live UX, mappings, Reference presentation, and full parity.

---

## Required reading order

1. This file.
2. `handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`.
3. `../AGENTS.md`.
4. Issue #87 and the owning issue (#90, #89, #52/#79, #37/#32/#33, or #88).
5. `01_PRODUCT_REQUIREMENTS.md`.
6. `03_ARCHITECTURE.md`.
7. `04_V1_SCOPE_AND_ACCEPTANCE.md`.
8. Accepted entries in `08_DECISIONS_AND_OPEN_QUESTIONS.md`.
9. `09_BUILD_AND_TEST_STANDARDS.md`.
10. `13_SOUNDSWITCH_PARITY_LEDGER.md` and the smallest current handoff/checkpoint.
11. For UI/user-visible behavior, `UI_PROGRAM_START_HERE.md`; for issue #63 work, `SKINS_PLATFORM_V2_START_HERE.md`.

---

## Definition of progress

Progress means a working, testable replacement path:

- one unambiguous source tree;
- exact source identity/evidence;
- deterministic editable state and compiled package;
- correct semantic targets/profiles;
- correct DMX only when explicitly enabled;
- honest unknown/degraded/failure reporting;
- read-only migration source and safe output defaults;
- measured footprint/latency/scheduler isolation;
- installed Windows, real VirtualDJ, physical hardware, and operator workflow tested at the correct claim level;
- user-visible behavior exposed through maintained versioned contracts rather than one-off surfaces.

Lines of code, stale issue checkboxes, structurally valid approximations, and polished mock screens are not milestones by themselves.
