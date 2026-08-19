# Start Here

## Mandatory current execution checkpoint — 2026-08-19

Before changing code or scope, read:

1. `handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`;
2. issue #87;
3. the smallest owning issue/handoff for the assigned lane.

The repository is currently **diverged**:

- `main@86694f02d89ef856b06d846026fe558e39206f90` contains the newest current-2026 SoundSwitch decoder/pilot history and a historical Preview 97 installer upload;
- PR #86 / `agent/backyard-party-v2@7efd0b083cc7c42df3b2a5ee3e760687a15e390b` contains the broad current implementation, including Autoloops V2, Studio/fixture/UI foundations, Preview 101 lineage, the Slint Fixtures + Static Looks lab, and the first bounded OS2L Blackout-feedback software slice;
- GitHub reports PR #86 23 commits ahead, 7 behind, diverged, and non-mergeable.

No broad feature agent should extend either line independently. The immediate task is #87 Gate A: one deliberate convergence tree preserving both histories and their evidence. After that, the critical implementation order is:

1. #90 current `2026.ssproj` source adapter/import vertical slice;
2. #89 installed VirtualDJ capture, OS2L lifecycle/service reliability, and authoritative feedback proof;
3. remaining fixture/physical evidence without blocking safe output-disabled project/import testing;
4. #37 toolkit decision, #32 Safe/runtime, and #33's first product-shaped Fixtures + Static Looks Default slice;
5. #88 perceptual fade quality, then broader Autoloops/Live/mapping/migration parity.

Older documents that still present raw Micro proof or Autoloops-foundation construction as the universal first coding task are historical sequencing inputs. The current handoff supersedes that ordering while preserving all remaining physical/support gates.

## Mission

Create a uniquely owned, general-purpose, gig-ready replacement for SoundSwitch. The product must be usable by DJs and lighting operators with rigs unrelated to Love & Light Entertainment. It must eventually match every relevant SoundSwitch capability, then surpass it with open hardware, safer persistence, portable semantic scripts, SoundSwitch migration, event-aware workflows, and a fully user-extensible skins/control-surface platform.

This is not a generic DMX console and not a WOLFMIX clone.

## Binding product statement

> A general-purpose, SoundSwitch-first, automation-first DJ/event lighting workstation with a lightweight offline Runner, a capable Studio, open MIDI and DMX choices, first-class migration, user-extensible control surfaces, and no dependency on one user's fixtures or hardware.

## Current implementation state

The integrated lineage already contains substantial working foundations that must be converged rather than rebuilt:

- versioned/checksummed project model and deterministic compiler/Runner;
- semantic property ownership/layer resolution and fixed-frame two-universe rendering;
- Art-Net, sACN, initial USB-DMX adapters, SoundSwitch Micro qualification tooling, frame attribution, diagnostics, Blackout authority, and recovery foundations;
- WinMM MIDI/learn/mapping and session-safe ownership foundations;
- Studio fixture/profile/patch/group services, structured profile/channel-function models, capability-aware Static Look authoring, exact output-disabled preview, and bounded physical-preview foundations;
- an accepted UI course correction that freezes new product-facing Win32 forms and uses a renderer-neutral Fixtures + Static Looks model plus an opt-in Slint product-shaped lab;
- canonical UI/Ember Action registry/code-generation foundations;
- Autoloops V2 semantic source/compiler, authoring, original 128-placement starter pack, persistence, deterministic AutoScript, Studio transaction bridge, palette realization, preview, runtime/director, and Runner integration;
- initial bounded outbound OS2L Blackout feedback code on PR #86, with installed VirtualDJ lifecycle/capture/qualification still open;
- current SoundSwitch source evidence for exact bank/placement/length metadata, project-local Venue target mapping, A/B timeline records, Static Look intensity/RGBWAUV data, and the failed old-map Pilot 01.

The latest identified installed legacy-shell test line is Preview 101. It is a deliberately small ownership-safety preview, not the replacement-shell product UI and not the current PR #86 source head.

GitHub Actions is currently blocked before runner assignment by the repository/account Actions payment or spending-limit gate. Empty workflow jobs are not source validation. The converged tree requires fresh complete local/Windows evidence before merge or publication.

## Immediate product truth

### SoundSwitch migration

SoundSwitch import is core, and the current research is now implementation-ready for proven fields. Issue #90 owns the first production vertical slice.

Binding rules:

- parse the exact source project/version and exact Venue target hierarchy;
- target IDs are project-local and never reused from another source lineage;
- use decoded placement arrays rather than raw entry order;
- carry source artifact/range evidence and preserve unknown bytes;
- never infer choreography from names or copy raw DMX into unrelated profiles;
- missing color context is explicit, never borrowed from another target;
- production import targets Autoloops V2, not sampled format-1 helper-Look chunking;
- imported content stays output-disabled until review;
- lack of exact fixture qualification must not block opening, inspecting, simulating, saving, or re-importing a project. It gates support claims and automatic output enablement.

### Current owned destination rig

For the present migration/bench destination:

- four Both Lighting IR-4, 10CH, U1 `001/011/021/031`;
- four BO-TUBE192/360 Tubes, 80CH, U1 `041/121/201/281`;
- no duplicate generic Both Lighting uplight fixtures in the destination project.

Source fixture identities are reconciled semantically to destination fixture/profile capabilities; they are not copied as duplicate destination fixtures or raw channels.

### UI direction

Legacy Win32 remains a transitional/Safe bridge. No new top-level Win32 editor/property form/product skin is accepted. The production toolkit decision is evidence-based under #37 using the same real Fixtures + Static Looks workflow. Slint/C++ is the first candidate, WinUI 3 the bounded Windows comparison, and Direct2D/Win32 the Safe baseline. Public command/state/skin contracts remain renderer-neutral.

## What is accepted

- Windows is the required launch platform; macOS follows later and cannot block Windows delivery.
- V1 exposes exactly two universes; post-V1 architecture must not block expansion.
- VirtualDJ/OS2L first; Serato is the second direct DJ-integration priority after VirtualDJ is stable.
- Predictive hold, then live-audio beat fallback, then tap tempo, then a safe unsynchronized look.
- Control One first as MIDI; onboard DMX/OLED/storage are isolated experimental lanes.
- Any MIDI controller can map to canonical actions and parameters.
- Same-PC and separate-PC modes are equal.
- SoundSwitch import is core; SoundSwitch workflow outranks WOLFMIX ideas.
- Original semantic/layered core; optional QLC+ output bridge, Studio-only QXF/OFL compatibility adapters, and selective audited reuse.
- OFL enters through a versioned Studio adapter/quarantine boundary; Runner consumes only stable compiled profile snapshots.
- Autoloops use a scalable 32-slot bank library; four banks are a controller/UI window, not an engine limit.
- No AI/model call in Runner's live output path.
- Full-V1 Windows testing builds are distributed through an installer while qualification continues.
- Default, SoundSwitch Reference, Safe, user skins, keyboard, MIDI/controller profiles, DJ commands, Ember Actions, and future remotes invoke one shared versioned command/state/capability model.
- Skins are validated presentation/binding packages, not alternate engines.
- Ember Actions are typed bounded compositions, not arbitrary JavaScript/Lua/WASM/native plugins and not a show-timing engine.
- Every user-visible feature change reconciles registries, generated artifacts, bundled surfaces, actions/mappings/profiles, schemas, compatibility/deprecation data, tests, and migration rules.
- Exact fixture qualification gates support/physical claims, not safe output-disabled import/project testability.

## What remains open

- one accepted convergence tree on `main`;
- a production current-2026 SoundSwitch source adapter/import path;
- installed VirtualDJ packet/lifecycle capture and application-owned OS2L reliability;
- native installed-Windows, physical-output, soak, and gig qualification for claimed backends/profiles;
- final production toolkit/renderer and trusted Safe activation;
- first installed product-shaped Fixtures + Static Looks Default journey;
- exact default live-control takeover/return details where not already proven by current ownership tests;
- exact SoundSwitch interpolation/unidentified packed-byte semantics and broader movement/effect/source-version coverage;
- broader fixture corpus, track-specific scripting, complete Live/Studio UX, mappings, Reference skin, and full parity.

## Required reading order

1. This file.
2. `handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`.
3. `../AGENTS.md`.
4. Issue #87 and the owning issue (#90, #89, #52/#79, #37/#32/#33, or #88).
5. `01_PRODUCT_REQUIREMENTS.md`.
6. `03_ARCHITECTURE.md`.
7. `04_V1_SCOPE_AND_ACCEPTANCE.md`.
8. `08_DECISIONS_AND_OPEN_QUESTIONS.md` entries marked Accepted.
9. `09_BUILD_AND_TEST_STANDARDS.md`.
10. `13_SOUNDSWITCH_PARITY_LEDGER.md` and the smallest matching handoff/checkpoint.
11. For UI/user-visible behavior, `UI_PROGRAM_START_HERE.md`; for issue #63 work, `SKINS_PLATFORM_V2_START_HERE.md`.

## Definition of progress

Progress is measured by a working, testable replacement path:

- one unambiguous accepted source tree;
- exact inputs and source identity captured;
- deterministic editable state and compiled package produced;
- correct semantic targets/profiles resolved;
- correct DMX emitted when explicitly enabled;
- unknowns/degradations/failures reported honestly;
- source remains read-only and output remains safe;
- footprint, latency, and scheduler isolation measured;
- installed Windows, real VirtualDJ, physical hardware, and operator workflow tested at the appropriate claim level;
- user-visible behavior exposed through maintained versioned contracts rather than one-off surfaces.

Lines of code, issue checkboxes, structurally valid approximations, and polished mock screens are not milestones by themselves.