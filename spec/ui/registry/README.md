# EmberLights UI Registries

Status: the SKIN2-001 generated registry spine now includes generation-2
contract catalogs for the first non-callable Component/Capability family and
the reusable values, enum, units, targets, and invocation results referenced by
the current native corpus. The older seed files remain reconciliation evidence
and are not an accepted second authority.

## Canonical implementation source

```text
spec/ui/registry/source/
  -> native-core/tools/generate_ui_registry.py
  -> native-core/include/emberlights/generated/ui_registry.generated.hpp
  -> spec/ui/registry/generated/ui-registry.catalog.json
  -> spec/ui/registry/generated/surface-cross-reference-report.json
  -> docs/generated/ui-registry/REFERENCE.md
```

The source integrates the exact 29 native commands and 39 native Live state
definitions, plus eight non-callable Runtime/Safe component contracts,
one accepted non-callable content capability, eleven reusable value contracts,
thirteen explicit invocation results, and five interactions. Explicit native
ordinals preserve the existing command/state/result ABI. General source JSON is
never parsed by Runner or the DMX scheduler.

Generate and gate:

```bash
python3 native-core/tools/generate_ui_registry.py generate
python3 native-core/tools/generate_ui_registry.py check
make -C native-core surface-contract-gate
python3 native-core/tools/generate_ui_registry.py diff \
  --baseline spec/ui/registry/baselines/ui-registry-v1.json \
  --candidate spec/ui/registry/generated/ui-registry.catalog.json \
  --expect compatibleAdditive
```

Generation 2 is registry set `1.4.0`, generator `1.1.0`. Against the frozen v1
baseline it adds 20 definitions (8 components, 1 capability, 11 values) and
enriches 13 result definitions with labels/descriptions while preserving their
IDs, terminal semantics, and exact ten-entry native ABI. The compatibility diff
must report zero breaking changes and zero removals.

## Generation-2 bounded contract families

- Components: `ember.activeLayers`, `ember.authoringWorkbench`,
  `ember.autoloopMatrix`, `ember.connectionPanel`, `ember.diagnostics`,
  `ember.fixtureFunctionBrowser` (displayed as Fixture Control Inspector),
  `ember.fixtureProfileEditor`, and `ember.staticLookMatrix`. Bridged contracts
  describe toolkit-neutral native seams; all remain `callable: false`, so no
  production skin-runtime activation is implied.
- Capability: `content.staticLooks`, the exact ID already used by the accepted
  Ember Action contract. It describes existing stable-ID Static Look behavior;
  it is not a new command or dynamic capability service.
- Values: `value.adapterState`, `value.fixtureProperty`, the `bpm`, `beats`, and
  `normalized` units, and every target kind currently referenced by the 29/39
  corpus.
- Results: ten existing native outcomes retain ordinals 0–9. `missingTarget`,
  `cancelled`, and `startedAsync` remain reserved catalog-only outcomes.

The generator fails closed on unknown schema/unit/target/component/capability
references, duplicate IDs/tokens, capability/replacement cycles, invalid
lifecycle metadata, unbounded numeric contracts, and semantic type, safety,
terminal, lifecycle, or absence-policy changes in compatibility fixtures.

The source-format decision, weighted evidence, result/realtime/update policies,
seed disposition, version/digest rules, and bounded gaps are recorded in
`docs/adr/0007-canonical-ui-registry-source.md`.

## Historical seeds

These files reduce naming ambiguity and preserve issue #31 planning evidence.
They are not generated artifacts and do not establish callable behavior:

Files:

- `command-registry-seed-v0.json`
- `state-registry-seed-v0.json`
- collection schemas in `../schema/`

## Ownership

Issue #31 owns the accepted registry. Changes to stable semantic IDs after #31 begins require:

- schema/registry version impact review;
- references updated across layouts, bindings, tests, docs, and skins;
- compatibility alias or explicit pre-public reset decision;
- no competing command list inside one skin/controller UI.

## Naming rules

### Commands

```text
<noun-or-domain>.<object-or-subdomain>.<verb>
```

Examples:

```text
show.start
show.stop
output.blackout.set
output.blackout.toggle
output.workLight.toggle
autoloop.launch
autoloop.bankFilter.selectExclusive
staticLook.activate
override.releaseAll
connection.os2l.reconnect
view.panel.open
```

Rules:

- command IDs describe product semantics, not Win32 controls or skin widgets;
- verbs are explicit where idempotence matters (`start`, `stop`, `set`); toggle exists for human surfaces but automation may prefer explicit set/start/stop;
- one command may be invoked by UI, keyboard, MIDI, controller, external adapter, and tests when scope permits;
- dangerous/emergency delivery class is metadata, not encoded only in the name;
- project targets use stable IDs or typed semantic roles, never list indices;
- no vendor/controller name appears unless the command genuinely manages that adapter/service.

### States

```text
<authority-domain>.<object>.<property>
```

Examples:

```text
runner.state
transport.bpm
connection.os2l.status
output.universe[0].status
output.blackout
output.workLight
autoloop.active.progress
staticLook.active.id
override.activePropertyCount
safety.hazard.fog.armed
```

Rules:

- state keys identify the authoritative product fact, not a label/control;
- zero-based array indices are internal; UI labels Universe 1 and Universe 2;
- related snapshots use `snapshotGroup` to preserve coherence;
- UI timers may interpolate presentation but never become authority;
- persisted settings/document fields and live status remain distinct state definitions;
- sensitive local paths/serials/errors declare privacy.

## Canonical blackout/work-light naming

Use:

```text
commands: output.blackout.*
state:    output.blackout
commands: output.workLight.*
state:    output.workLight
```

Use `safety.*` for:

- hazard arming;
- authored safety policies/caps;
- restrictions/interlocks;
- summarized safety status.

Some early prose uses `safety.blackout`. Treat that as conceptual wording, not a second registry key. Issue #31 should not publish both names unless a temporary compatibility alias is deliberately required.

## Interaction versus binding behavior

The command defines the semantic action; the binding defines the hardware/control behavior where possible.

Example:

- command: `staticLook.activate(lookId)`;
- momentary/hold behavior: press activates, release invokes clear/release through the binding/controller service;
- toggle behavior: binding alternates activation/clear or uses a registered `staticLook.toggle` if domain ownership requires one atomic command;
- active feedback: `staticLook.active.id` and ownership state.

Issue #38's Static Look Toggle/Hold work may refine the exact command set. The registry must preserve one core ownership model shared by UI and MIDI, not embed separate behavior in each skin.

## Availability expressions

Seed expressions such as:

```text
runner.state == 'running' && autoloop.exists(bank,slot)
```

are conceptual planning expressions. The implementation may compile generated predicates/native functions. The public metadata must remain side-effect-free, bounded, inspectable, and usable by validation/Command Explorer.

## Registry generation

Preferred implementation:

1. Define accepted commands/states in one generated/native source of truth.
2. Generate C++ enums/types/lookup tables where useful.
3. Generate JSON registry artifacts for skin/binding validation and developer inspection.
4. Verify generated artifacts are deterministic and up to date in CI.
5. Never parse the full JSON registry on the DMX scheduler.

## Required #31 reconciliation

Before promoting `planningSeed` to `implementationDraft`:

- inventory every existing control/callback/status path;
- reconcile #38 service/command changes;
- add missing project/authoring/connection/output/safety commands;
- resolve explicit Set/Toggle/Hold ownership semantics;
- define exact invocation results;
- extend reusable value schemas beyond the current native enum/unit/target set
  as new accepted definitions require them;
- define localization keys;
- validate every example layout and binding against the accepted registry;
- add uniqueness, deprecation, and cross-reference tests;
- publish the remaining legacy bypass ledger.

## Non-goals of the seed

- freezing every authoring CRUD command before implementation evidence;
- defining proprietary Control One functions;
- claiming exact VirtualDJ transport capabilities not yet captured;
- creating separate Default/Reference registries;
- requiring JSON interpretation in the live output path.
