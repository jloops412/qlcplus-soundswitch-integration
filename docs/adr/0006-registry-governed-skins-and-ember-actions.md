# ADR 0006 — Registry-Governed Skins, Visual Authoring, and Ember Actions

- **Status:** Accepted
- **Date:** 2026-08-12
- **Decision owners:** Product owner and EmberLights architecture program

## Context

EmberLights must support bundled and user-created skins, custom controls, keyboard/MIDI/controller mappings, import/export, and eventual migration assistance from products such as VirtualDJ. The system must remain modular as features are added, removed, renamed, or changed.

VirtualDJ demonstrates the value of one action vocabulary across skins, custom buttons, keyboard, and controller mappings, but its authoring model depends heavily on manually managed skin/XML/image files and compact script strings. EmberLights also has stricter requirements: deterministic Runner behavior, safe DMX continuity, typed project targets, explicit persistence/Undo, package sandboxing, and mandatory emergency controls.

The existing UI program already establishes a typed command/state facade, declarative `.emberskin` packages, a Safe fallback, and future overlays/custom controls. The missing decision is how full visual authoring and richer action composition fit without becoming arbitrary code or a second lighting engine.

## Decision

1. EmberLights will treat the command, state, component, capability, interaction, theme-token, value-schema, and invocation-result registries as the versioned UI/control-surface ABI.
2. Every user-visible behavior change must reconcile those registries, generated artifacts, bundled skins, bindings, actions, profiles, examples, compatibility tests, and migration mappings in the same bounded work.
3. Skins remain validated declarative presentation/binding packages. They never implement lighting semantics or access devices/files/network directly.
4. Users may customize immutable base skins through `.emberoverlay` artifacts and later create/fork complete `.emberskin` packages with a Studio-side visual Skin Designer.
5. Rich custom behavior uses **Ember Actions**: typed, bounded, inspectable action graphs over registered commands and state predicates.
6. An optional expert **Ember Action Script** text form may round-trip to the same canonical Action IR. It cannot introduce behavior unavailable to the graph/IR and is never evaluated as arbitrary runtime code.
7. The action executor runs on an approved UI/control service, not the DMX scheduler. Musical timing, repeats, transitions, ownership, persistence, safety, and output remain domain/Runner responsibilities exposed through typed commands.
8. The lean Perform/Runner path loads only validated compiled View Graph, Action IR, Binding Tables, and bounded state subscriptions. The full designer, source importers, arbitrary language runtimes, and migration tools remain outside that path.
9. Imported skins/mappings/actions are read-only untrusted source evidence. Source adapters produce explicit `exact`, `translated`, `approximated`, `opaque`, `unsupported`, or `conflicted` migration results and never create vendor-specific runtime engines.
10. Default, SoundSwitch Reference, and Safe remain separate first-party compatibility fixtures over identical domain behavior. The SoundSwitch Reference skin provides familiar Performance/Autoloop/Static Look/Override landmarks with original EmberLights assets and can be forked/customized.

## Consequences

### Positive

- User interfaces, mappings, actions, and future remotes remain portable across one engine.
- Non-programmers can build useful control surfaces visually.
- Expert users gain composition without arbitrary code or fragile hidden globals.
- Feature drift becomes mechanically detectable through code generation and compatibility gates.
- Skin/action failures cannot stop DMX or bypass emergency authority.
- Migration tools target one canonical model and preserve source evidence.
- The Perform runtime remains lean and deterministic.

### Costs

- Commands/states/components/capabilities require more complete metadata and lifecycle discipline.
- Code generation, registry diffing, compatibility fixtures, and migration diagnostics must be built and maintained.
- A full visual designer is a staged product program, not one cosmetic UI task.
- Some compact script conveniences are intentionally rejected until represented safely as typed domain commands or bounded graph nodes.

## Rejected alternatives

### Arbitrary JavaScript/Lua/WebAssembly/native plugins in skins

Rejected because they create code-execution, device/file/network, determinism, compatibility, and safety risks and would make skin packages alternate application engines.

### Skin-specific callbacks and behavior

Rejected because Default, Reference, user skins, controllers, and remotes would drift and duplicate domain logic.

### UI timers for lighting macros

Rejected because musical timing and return/transition semantics must remain Runner/domain authority.

### Manual XML/JSON/script editing as the primary product UX

Rejected because the owner explicitly requires grid/layout tools, prebuilt elements, color/icon pickers, discoverable functions, and a more accessible system than legacy skin workflows.

### One monolithic hard-coded SoundSwitch clone

Rejected because SoundSwitch familiarity is a bundled Reference presentation, not the permanent engine or only workflow.

## Binding follow-ups

- `docs/SKINS_PLATFORM_V2_START_HERE.md`
- `docs/SKINS_PLATFORM_V2_VISUAL_DESIGNER_ACTIONS_AND_CONTINUITY_PLAN.md`
- `spec/ui/ember-actions-contract-v1.md`
- `spec/ui/skin-designer-contract-v1.md`
- `spec/ui/registry/REGISTRY_LIFECYCLE_AND_COMPATIBILITY_POLICY.md`
- GitHub Skins Platform V2 epic and SKIN2 work issues
- agent/bootstrap/UI-program entrypoint updates
