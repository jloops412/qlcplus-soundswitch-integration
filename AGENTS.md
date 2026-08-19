# Project Instructions for Build Agents

## Mandatory current routing

Before changing code, scope, issues, artifacts, or branches, read:

1. `docs/handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`;
2. issue #87 — product convergence gate;
3. the smallest owning issue/handoff for the assigned lane.

The current handoff is binding until #87 Gate A lands one accepted implementation anchor on `main`.

## Current repository warning

The exact pre-coordination implementation/evidence baseline was:

- `main@86694f02d89ef856b06d846026fe558e39206f90`;
- PR #86 / `agent/backyard-party-v2@7efd0b083cc7c42df3b2a5ee3e760687a15e390b`;
- merge base `47bb45d6e84609a7a73358517ed77e492c29f882`.

At review time PR #86 was 23 commits ahead, 7 behind, diverged, and non-mergeable. `86694f0` contains the current-2026 SoundSwitch decoder/pilot lineage and a historical Preview 97 installer upload; PR #86 contains the broad implementation and independently added decoder research.

After this coordination package lands, **start convergence from the latest `main` head**, verify `86694f0` is its ancestor, and integrate exact `7efd0b0`. Do not reset `main` back to `86694f0`, and do not extend either implementation line independently.

Never use blanket `ours`/`theirs`, a merge commit whose tree silently discards one parent, or issue-title assumptions to resolve the split.

## Immediate execution order

1. **#87 Gate A:** converge latest `main` and exact PR #86 into one validated implementation truth.
2. **#90:** implement the evidence-backed current `2026.ssproj` source adapter/import vertical slice, beginning with exactly `Medium / S1 / Red - Smooth Pulse`.
3. **#89:** preserve the existing bounded OS2L Blackout-feedback implementation, capture real installed VirtualDJ behavior, then finish application-owned listener/discovery/reconnect lifetime.
4. Reconcile remaining owned-rig/profile/patch evidence without blocking safe output-disabled import/project testing.
5. Finish #37 toolkit evidence, #32 Safe/runtime activation, and #33's first product-shaped Fixtures + Static Looks Default slice.
6. Address #88 perceptual fade quality, then expand Autoloops/Live/mapping/migration parity.

No broad WOLFMIX, AI, cloud/marketplace, Control One proprietary expansion, macOS, new Autoloop engine/model, new fixture model, new UI runtime, or new product-facing Win32 editor belongs ahead of that sequence.

---

## Lane routing

### Repository convergence

One integration owner controls conflicting governance/build/generated/shared files until Gate A closes. Other lanes may inspect but must not independently reconcile the same histories.

The convergence owner must preserve/equate both protected implementation inputs, reconcile generated contracts from source, handle the historical Preview 97 binary explicitly, run the complete native/Windows/package/UI/registry gate, and review older PRs for unique code/tests/docs/evidence before superseding them.

### SoundSwitch current-project migration

Read issue #90, issue #60, and `research/migration/soundswitch-autoloops/`.

Binding rules:

- source projects/archives are read-only;
- target IDs come from the exact source project's Venue database and are not stable across projects/versions;
- use decoded placement arrays, not raw entry-list order;
- never derive choreography from names, filenames, bank labels, or visual guesses;
- never copy raw source DMX into unrelated destination profiles;
- preserve unknown/unclaimed payloads and exact source evidence;
- missing local color context is `MissingColorSource` / `NeedsColorContext`, never borrowed from another target;
- provisional interpolation/high-byte semantics remain translated/opaque until controlled-delta evidence proves them;
- production import targets the accepted Autoloops V2 semantic source/program model, not format-1 helper-Look chunking;
- imported proposals remain output-disabled until review;
- exact fixture qualification must not block opening, inspecting, simulating, editing, saving, or re-importing an output-disabled project. It gates automatic output enablement and support claims, not basic testability.

Do not expand beyond the first one-loop vertical slice until its current-project source map, evidence report, save/reopen, and idempotent re-import are correct.

### OS2L / VirtualDJ

Read issue #89, `docs/46_OS2L_RELIABILITY_AND_VDJ_CONTROL_CHECKPOINT.md`, and `docs/handoffs/OS2L_RELIABILITY_BUILD_AGENT_HANDOFF_2026-08-18.md` after convergence.

Treat OS2L as an application-owned transport service over canonical commands/state:

- listener/discovery lifetime is not owned by Runner or song transport;
- TCP client state and beat/sync state are separate facts;
- authoritative EmberLights state drives feedback;
- VirtualDJ variables are not lighting authority;
- network/discovery/feedback/capture work never runs on the DMX scheduler;
- connection/session epochs protect against stale releases/owners;
- direct-IP/keepalive are compatibility fallbacks, not routine connection management.

PR #86 head already contains a bounded Blackout-feedback send path. Review/integrate it before reimplementing. The next evidence task is installed raw capture of plain/on/off/keepalive plus song transitions. Do not begin the generic `os2l_cmd` bridge until P0 feedback/lifecycle is proven.

### Fixture/profile/patch and hardware qualification

Read issues #52/#79 plus the current fixture plans. Current destination rig:

- four Both Lighting IR-4, 10CH, U1 `001/011/021/031`;
- four BO-TUBE192/360 Tubes, 80CH, U1 `041/121/201/281`;
- no duplicate generic Both Lighting uplight destination fixtures.

A SoundSwitch source uplight may be semantically reconciled to the current IR-4 destination. Do not add a second destination fixture row merely because the source model differs.

Fixture-library match, project validation, adapter writes, installed behavior, physical response, and gig qualification are distinct. Physical support claims require exact observed evidence. Safe output-disabled project/import testing does not.

### Studio / Autoloops V2 / AutoScript

Read #57–#60 and `docs/35_AUTOLOOPS_V2_CONTINUITY_CHECKPOINT.md` from the convergence tree.

Substantial work already exists on PR #86: source model/compiler, authoring, original 128-placement pack, persistence, deterministic AutoScript, Studio bridge, Palette realization, preview, director/runtime, and Runner. Inspect the converged tree before adding anything. Do not create another timeline, project, color, preview, generator, runtime, or persistence model.

The current high-value Studio work is #90 migration fidelity and product exposure—not another default pack or name-derived converter.

### UI / skins / controls

Read `docs/UI_PROGRAM_START_HERE.md`, #37, #32, #33, D-091, and the current handoff.

- Legacy Win32 is a frozen transitional/Safe bridge.
- No new top-level Win32 editor/property form/product skin.
- Use the same renderer-neutral command/state/capability/component model.
- Slint/C++ is the first product-shaped candidate; WinUI 3 is the bounded Windows comparison; Direct2D/Win32 is the Safe baseline.
- Raw DMX belongs behind Advanced diagnostics.
- Reusable controls are built only inside the accepted toolkit/runtime and include keyboard/accessibility/state/ownership/safety semantics.
- Do not introduce another UI framework, skin runtime, script VM, or private domain behavior.

For any user-visible command/state/capability/component, keyboard/MIDI/controller mapping, Ember Action, skin, overlay, or designer change, also read issue #63 and `docs/SKINS_PLATFORM_V2_START_HERE.md`. Reconcile registry source, generated artifacts, bundled surfaces, mappings/actions, schemas, tests, compatibility/deprecation, and migration in the same bounded work.

---

## Authority order

1. Direct current instructions from Joshua.
2. `docs/handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md` while #87 Gate A is open.
3. Issue #87 for branch/product convergence.
4. Issue #90 for the current SoundSwitch 2026 source adapter/import.
5. Issue #89 and doc 46 for OS2L/VirtualDJ reliability.
6. Accepted entries in `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`.
7. The owning issue and smallest matching current handoff/checkpoint.
8. `docs/00_START_HERE.md`, `docs/06_PRIORITIZED_BACKLOG.md`, `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`, and the product ledger after convergence reconciliation.
9. Older handoffs/research interpreted through later evidence.
10. Implementation details and provisional recommendations.

The dynamic-head note in the current handoff controls historical references to `main@86694f0`: start from latest `main`, verify ancestry, and reconcile exact PR #86.

When older docs say raw SoundSwitch Micro proof or Autoloops-foundation construction is the universal first task, the current handoff supersedes that sequencing. Remaining physical/support gates still matter.

---

## Accepted product invariants

- EmberLights is a uniquely owned, general-purpose, gig-ready SoundSwitch replacement—not a generic DMX console or WOLFMIX clone.
- SoundSwitch workflow/import parity is the minimum bar; WOLFMIX is secondary.
- Windows launches first; V1 exposes exactly two universes.
- VirtualDJ/OS2L is first; Serato follows after VirtualDJ is stable.
- MIDI/controller behavior is device-agnostic; Control One is the first bundled profile. Proprietary Control One DMX/OLED/storage remain isolated experiments.
- Same-PC and separate-PC modes are equal.
- Runner is deterministic, offline, fixed-capacity, and contains no AI/model calls.
- Skins/presentations/mappings/remotes share one engine and one versioned command/state/capability model.
- Ember Actions are typed bounded compositions, not arbitrary code and not a show-timing engine.
- Migration source evidence, editable Studio data, compiled packages, live state, and qualification evidence are separate representations.
- Source indexing, migration decoding, audio analysis, asset processing, package import, discovery/network I/O, and diagnostics formatting stay off the Runner/DMX scheduler.
- Fixture profiles are immutable/provenance-bearing project snapshots; library updates never silently mutate project truth.
- Unknown migration/fixture payloads are preserved where feasible and never silently reinterpreted.
- Blackout remains authoritative, non-droppable, and independent of client connection.

---

## Required engineering behavior

- Start from the exact assigned current base and report it.
- Reserve exact files before editing shared code.
- Reuse existing authorities/services/contracts before adding a type/system.
- Keep hardware, DJ, audio, controller, fixture-source, renderer, and output adapters replaceable.
- Never destructively modify a SoundSwitch source, exported personality, fixture manual, or the user's only audio/source copies.
- Add/update tests with every behavioral change.
- Separate verified facts, inferences, and unresolved hypotheses.
- Separate software-tested, packaged, installed-Windows-tested, physical-output-tested, and gig-qualified.
- Host-accepted USB writes are not fixture response.
- A structurally valid migration project is not source fidelity.
- Preserve source hashes, field/range evidence, warnings, provenance, and qualification state.
- Invalidate qualification when behavior-affecting profile/patch/backend data changes.
- Keep active hardware tests bounded and fail to Blackout.
- Do not allocate on the DMX scheduling path after package load.
- Route user-callable behavior through canonical registered commands and publish authoritative state/result feedback.
- Treat skins, actions, mappings, profiles, migration inputs, and packages as untrusted bounded data without direct device/file/network/state authority.
- Regenerate contracts from source; do not hand-merge generated outputs while generators remain stale.
- Never close/supersede a branch/issue by title alone; prove unique code/tests/docs/evidence are present or deliberately rejected.

## Agent checkpoint format

Every implementation checkpoint states:

```text
Owning issue:
Exact base/ref:
Branch/worktree:
Reserved files:
Delivered behavior:
Explicit non-goals:
Tests/evidence run:
Exact commit:
Remaining unknowns/claim boundary:
```

Use precise claims:

- **Implemented:** code exists on the exact stated tree.
- **Software-tested:** deterministic tests passed in the stated environment.
- **Packaged:** exact payload/installer contract was verified.
- **Installed-Windows-tested:** native install/launch/persistence/uninstall was observed.
- **Physical-output-tested:** exact backend/profile/mode/address produced observed response.
- **Gig-qualified:** soak/fault/operator/live-event gates passed.

Migration statuses are exact: Exact, DeterministicallyTranslated, Approximated, PreservedOpaque, Unsupported, Conflicted, MissingDependency, RejectedUnsafe.

Do not let a polished UI, broad catalog, speculative profile, hand-authored migration replacement, AI feature, WOLFMIX experiment, or proprietary hardware trick delay the convergence/import/OS2L/product-shell critical path.