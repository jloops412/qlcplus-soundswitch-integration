# Project Instructions for Build Agents

## Mandatory current handoff

Before changing code, scope, issues, artifacts, or branches, read:

1. `docs/handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md`
2. issue #87 — product convergence gate
3. the smallest owning issue/handoff for the assigned lane

The current handoff is binding until #87 Gate A lands one accepted convergence anchor on `main`.

### Current repository warning

As of 2026-08-19:

- `main` is `86694f02d89ef856b06d846026fe558e39206f90`;
- PR #86 / `agent/backyard-party-v2` is `7efd0b083cc7c42df3b2a5ee3e760687a15e390b`;
- the histories are diverged, PR #86 is 23 commits ahead and 7 behind, and GitHub reports it non-mergeable;
- `main` contains the newest current-2026 SoundSwitch decoder/pilot history and a main-only historical Preview 97 installer upload;
- PR #86 contains the broad current implementation and independently added decoder research.

**Do not begin broad feature work from either line independently.** The first implementation task is one deliberate convergence branch per #87/current handoff. Never use blanket `ours`/`theirs`, a merge commit with a tree that silently discards a parent, or issue-title assumptions to resolve this split.

## Immediate execution order

1. **#87 Gate A:** converge current `main` and PR #86 into one validated source of truth.
2. **#90:** implement the evidence-backed current `2026.ssproj` source adapter/import vertical slice, beginning with exactly `Medium / S1 / Red - Smooth Pulse`.
3. **#89:** preserve the existing bounded OS2L Blackout feedback implementation, capture real installed VirtualDJ behavior, then finish application-owned discovery/listener/reconnect lifetime.
4. Reconcile remaining owned-rig/profile/patch evidence without blocking safe output-disabled import/project testing.
5. Finish #37 toolkit evidence, #32 Safe/runtime activation, and #33's first product-shaped Fixtures + Static Looks Default slice.
6. Address #88 perceptual fade quality, then expand Autoloops/Live/mapping/migration parity.

No broad WOLFMIX, AI, cloud/marketplace, Control One proprietary expansion, macOS, new Autoloop engine/model, new fixture model, new UI runtime, or new product-facing Win32 editor belongs ahead of that sequence.

---

## Lane routing

### Repository convergence

Read issue #87 and the current handoff. One integration owner controls conflicting governance/build/generated/shared files until Gate A closes. Other lanes may inspect but must not independently reconcile the same histories.

### SoundSwitch current-project migration

Read issue #90, issue #60, and the research under `research/migration/soundswitch-autoloops/`.

Binding migration rules:

- source projects and archives are read-only;
- target IDs are resolved from the exact source project's Venue database and are not stable across projects/versions;
- use decoded placement arrays, not raw entry-list order;
- never derive choreography from names, filenames, bank labels, or visual guesses;
- never copy raw source DMX into unrelated destination profiles;
- preserve unknown/unclaimed payloads and exact source evidence;
- missing local color context is explicit `MissingColorSource` / `NeedsColorContext`, not a reason to borrow color from another target;
- provisional interpolation/high-byte semantics remain translated/opaque until controlled-delta evidence proves them;
- production import targets the accepted Autoloops V2 semantic source/program model, not format-1 helper-Look chunking;
- imported proposals remain output-disabled until review;
- exact fixture qualification must **not** block opening, inspecting, simulating, saving, or re-importing an output-disabled project. It gates support claims/automatic output enablement, not basic testability.

Do not expand beyond the first one-loop vertical slice until its exact current-project source map and migration report are correct.

### OS2L / VirtualDJ

Read issue #89, `docs/46_OS2L_RELIABILITY_AND_VDJ_CONTROL_CHECKPOINT.md`, and `docs/handoffs/OS2L_RELIABILITY_BUILD_AGENT_HANDOFF_2026-08-18.md` after convergence.

Treat OS2L as an application-owned transport service over canonical commands/state:

- listener/discovery lifetime is not owned by Runner or song transport;
- TCP client state and beat/sync state are separate facts;
- authoritative EmberLights state drives feedback; VirtualDJ variables are not lighting authority;
- network/discovery/feedback/capture work never runs on the DMX scheduler;
- existing connection/session epochs protect against stale releases/owners;
- direct-IP/keepalive may remain compatibility fallbacks, not routine connection management.

PR #86 head already contains a bounded Blackout feedback send path. Review/integrate it before reimplementing. The next evidence task is installed raw capture of plain/on/off/keepalive and song transitions. Do not begin the generic `os2l_cmd` bridge until P0 feedback/lifecycle is proven.

### Fixture/profile/patch and hardware qualification

Read issues #52/#79 plus the current fixture plans. For the current destination rig use:

- four Both Lighting IR-4 fixtures, 10CH, U1 `001/011/021/031`;
- four BO-TUBE192/360 Tubes, 80CH, U1 `041/121/201/281`;
- no duplicate generic Both Lighting uplight destination fixtures.

A source SoundSwitch uplight identity may be semantically reconciled to the current IR-4 destination; do not add a second destination fixture row merely because the source fixture model differs.

Fixture-library match, project validation, adapter writes, and physical qualification remain distinct. Physical support claims require exact observed evidence. Safe project/import testing does not.

### Studio / Autoloops V2 / AutoScript

Read #57–#60 and `docs/35_AUTOLOOPS_V2_CONTINUITY_CHECKPOINT.md` from the convergence tree.

Substantial Autoloops V2 work already exists on PR #86: source model/compiler, authoring, 128-placement original pack, persistence, deterministic AutoScript, Studio bridge, palette realization, preview, director/runtime/Runner. Inspect the converged tree before adding anything. Do not create another timeline, project, color, preview, generator, runtime, or persistence model.

The current high-value Studio work is #90 migration fidelity and product exposure—not another default content pack or name-derived converter.

### UI / skins / controls

Read `docs/UI_PROGRAM_START_HERE.md`, issue #37, issue #32, issue #33, D-091, and the current handoff.

- Legacy Win32 is a frozen transitional/Safe bridge.
- No new top-level Win32 editor/property form/product skin.
- Use the same renderer-neutral command/state/capability/component model.
- Slint/C++ is the first product-shaped candidate; WinUI 3 is the bounded Windows comparison; Direct2D/Win32 is the Safe baseline.
- Raw DMX belongs behind Advanced diagnostics.
- Reusable controls/primitives are welcome only inside the accepted toolkit/runtime and must include keyboard/accessibility/state semantics.
- Do not introduce another UI framework, skin runtime, script VM, or private domain behavior.

### Skins/actions/mappings

For any user-visible command/state/capability/component, keyboard/MIDI/controller mapping, Ember Action, skin, overlay, or designer change, read issue #63 and `docs/SKINS_PLATFORM_V2_START_HERE.md`.

Every user-visible behavior change requires registry, generated artifact, bundled surface, mapping/action, schema, test, deprecation, and migration reconciliation in the same bounded work. No new untracked UI/controller callback bypass.

---

## Authority order

1. Direct current instructions from Joshua.
2. `docs/handoffs/CURRENT_BUILD_EXECUTION_HANDOFF_2026-08-19.md` while #87 Gate A is open.
3. Issue #87 for branch/product convergence.
4. Issue #90 for current SoundSwitch 2026 source adapter/import.
5. Issue #89 and doc 46 for OS2L/VirtualDJ reliability.
6. Accepted entries in `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`.
7. The owning issue and smallest matching current handoff/checkpoint.
8. `docs/00_START_HERE.md`, `docs/06_PRIORITIZED_BACKLOG.md`, `docs/13_SOUNDSWITCH_PARITY_LEDGER.md`, and `soundswitch-replacement-product-ledger.md` after convergence reconciliation.
9. Older handoffs/research, interpreted through later accepted evidence.
10. Implementation details and provisional recommendations.

When older documents still say raw SoundSwitch Micro proof or Autoloops-foundation construction is the universal first task, the current handoff supersedes that ordering. Remaining physical/support gates still matter; they are not permission to ignore the branch split or rebuild completed foundations.

---

## Accepted product invariants

- EmberLights is a uniquely owned, general-purpose, gig-ready SoundSwitch replacement—not a generic DMX console or WOLFMIX clone.
- SoundSwitch workflow/import parity is the minimum product bar; WOLFMIX is secondary.
- Windows is the launch platform; V1 exposes exactly two universes.
- VirtualDJ/OS2L is first; Serato follows after VirtualDJ is stable.
- MIDI/controller behavior is device-agnostic; Control One is the first bundled profile. Proprietary Control One DMX/OLED/storage remain isolated experimental lanes.
- Same-PC and separate-PC modes are equal.
- Runner is deterministic, offline, fixed-capacity, and contains no AI/model calls.
- Skins/presentations/mappings/remotes share one engine and one versioned command/state/capability model.
- Ember Actions are typed bounded compositions, not arbitrary JavaScript/Lua/WASM/native code and not a show-timing engine.
- SoundSwitch migration source evidence, editable Studio data, compiled Runner packages, live state, and qualification evidence are separate representations.
- Source/library indexing, migration decoding, audio analysis, waveform generation, asset processing, skin design, package import, action compilation, discovery/network I/O, and diagnostics formatting stay off the Runner/DMX scheduler.
- Fixture profiles are immutable/provenance-bearing project snapshots; library updates never silently mutate project truth.
- Unknown migration/fixture payloads are preserved losslessly where feasible and never silently reinterpreted.
- Blackout remains authoritative, non-droppable, and independent of client connection.

---

## Required engineering behavior

- Start from the exact assigned base commit and report it.
- Reserve exact files before editing shared code.
- Reuse existing authorities/services/contracts before adding a type/system.
- Keep hardware, DJ, audio, controller, fixture-source, renderer, and output adapters replaceable.
- Never destructively modify a SoundSwitch project, exported personality, fixture manual, or the user's only audio/source copies.
- Add/update tests with every behavioral change.
- Distinguish verified facts, inferences, and unresolved hypotheses.
- Distinguish software-tested, packaged, installed-Windows-tested, physical-output-tested, and gig-qualified.
- Host-accepted USB writes are not proof of DMX/fixture response.
- A structurally valid migration project is not source fidelity.
- Use raw-output tests to separate transport defects from profile/address/fixture defects.
- Preserve source/native hashes, field/range evidence, conversion warnings, provenance, and qualification state.
- Invalidate qualification when behavior-affecting profile/patch/backend data changes.
- Keep active hardware tests bounded and fail to Blackout.
- Do not allocate on the DMX scheduling path after package load.
- Route user-callable behavior through canonical registered commands and publish authoritative state/result feedback.
- Treat skins, overlays, actions, mappings, profiles, migration inputs, and packages as untrusted bounded data with no direct device/file/network/state authority.
- Regenerate generated contracts from source; do not hand-merge generated outputs while leaving generators stale.
- Never close/supersede a branch/issue by title alone; prove unique code/tests/docs/evidence are present or deliberately rejected.

---

## Agent checkpoint format

Every implementation checkpoint must state:

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

Use precise status language:

- **Implemented:** code exists on the exact stated tree.
- **Software-tested:** deterministic tests passed in the stated environment.
- **Packaged:** exact payload/installer contract was verified.
- **Installed-Windows-tested:** native install/launch/persistence/uninstall was observed.
- **Physical-output-tested:** exact backend/profile/mode/address produced observed response.
- **Gig-qualified:** soak/fault/operator/live-event gates passed.

For migration, use the accepted evidence statuses exactly: Exact, DeterministicallyTranslated, Approximated, PreservedOpaque, Unsupported, Conflicted, MissingDependency, RejectedUnsafe.

Do not let a polished UI, broad catalog, speculative profile, hand-authored migration replacement, AI feature, WOLFMIX experiment, or proprietary hardware trick delay the convergence/import/OS2L/product-shell critical path.