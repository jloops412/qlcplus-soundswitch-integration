# Named Fixture Control Surfaces Checkpoint

Date: 2026-08-13

Status: **Preview 94 contract-packaged; the shared catalog now also serves a general profile workbench, a fixture-function component model, and exact Ember Action planning; native installed-Windows and hardware evidence remain pending**

Predecessor: `39_MULTI_CAPABILITY_FIXTURE_CHANNEL_CHECKPOINT.md`

## Outcome

Fixture profiles now drive usable named controls instead of stopping at a channel/range editor. A deterministic toolkit-neutral service resolves one patched fixture or group into choices such as Color Wheel / Red, Shutter / Open, or Strobe / Slow–Fast. Static Looks and Live Overrides expose those choices in the current Windows adapter without creating Win32-only fixture semantics.

The same choice record carries stable fixture/profile/binding identity, property, category, capability name, normalized semantic value, exact raw byte/range, group coverage, safety status, and Live compatibility. Preview 93 proves that reusable boundary in Autoloop V2 event authoring and MIDI/controller mapping as well as Static Looks and Live. The Preview 94 source checkpoint adds a bounded native-component snapshot/controller model and deterministic fixture-control Ember Action planner over the catalog. These are reusable implementation boundaries, not claims that a skin runtime activates the browser or that planned Actions persist as project assets.

## Mixed-profile truth

Different profiles can place the same named function at different raw bytes and different normalized positions.

- Static Look authoring expands a group choice to exact per-fixture semantic assignments in one draft transaction. The normal compiler then produces each profile's exact output byte.
- Live group override remains one immutable fixture-mask command. EmberLights offers a named group choice only when one semantic normalized value is correct for every supporting profile; it never sends a partial UI command sequence or silently approximates members.
- A partial choice is reported rather than pretending every group member supports it.

This distinction preserves both authoring accuracy and live atomicity.

## Preview 93 continuation

### Autoloop V2 event authoring

- A toolkit-neutral planner accepts one persisted V2 program, fixture/group target, stable named choice, exact half-open 960-PPQ range, and range position.
- One mixed-profile group gesture expands into exact fixture targets, lanes, and semantic `PropertyBlock` events. Each fixture retains its profile-specific normalized value; no raw DMX byte enters the Autoloop source.
- The complete candidate is canonicalized and validated before one generation-checked `AutoloopAuthoringService` transaction. Stale generations, invalid time, identifier collisions, ownership conflicts, work/capacity limits, protected functions, and safety-gated functions fail without partial source mutation.
- The Windows AutoScript bridge can select a committed V2 placement, preview the candidate through the output-disabled production compiler/renderer, and only then commit it through the existing persisted-source Studio transaction. It does not introduce a second preview or lighting engine.

### Persistent MIDI/controller bindings

- A learned device gesture is only the input prototype. Target, action type, property, exact semantic endpoints, and stable named-function provenance come from the shared fixture catalog.
- A homogeneous group remains one `SetGroupProperty` mapping. A mixed-profile group expands atomically into exact per-fixture `SetProperty` mappings when one shared gesture still fits the fixed project and actions-per-message budgets.
- Slots persist one exact semantic value and disable meaningless soft takeover. Continuous named ranges persist their exact zero-to-one semantic endpoints while retaining the learned curve, inversion, and pickup settings.
- The stable choice ID round-trips in the existing format-1 MIDI record using a previously reserved field. Older projects still parse with an empty provenance value; no display label, list index, or raw byte becomes identity.
- Protected choices never enter the catalog. Safety-gated choices require an explicit authoring opt-in at the service boundary and remain hidden from the current quick-pick adapters.

### Migration portability evidence

Preview 93 also adds a content-safe lifecycle report for Probe, Inventory, Decode, Reconcile, Plan, Commit, and Upgrade. SoundSwitch artifact/hash identity is reported separately from semantic decoder qualification. WOLFMIX is explicitly research-only with `wolfmix.controlled_delta_corpus_unavailable`; this report is not a WOLFMIX parser, importer, or runtime engine.

## Preview 94 continuation

### General fixture channel-map workbench

- Studio Fixture Profiles now opens one ordered channel-map workbench rather than requiring stable parameter IDs or a fixture-specific repair control. Its catalog contains all 53 current semantic parameters with category, control, safety, and availability explanations; only ordinary safe direct 8-bit presets can be assigned automatically.
- **Add at next open channel** fills the first unused slot inside the entered footprint and grows the footprint by one only when every existing slot is occupied. **Fill unused gaps safely** writes explicit zero-valued safe constants and respects 16-bit fine-channel occupancy.
- Any two compatible Local direct 8-bit mappings can be selected for a reviewed function exchange. The two semantic properties move while physical channel offsets, ranges, defaults, blackout/highlight values, and owners remain fixed. Fine/coarse pairs, constants, named/compound ranges, chart-dependent or safety-restricted functions, incompatible layouts, stale reviews, and invalid candidates fail atomically.
- Built-in and imported snapshots remain read-only. **Duplicate to Edit** creates a Local draft; workbench changes stay staged until **Save Profile** validates and compiles the candidate and the operator accepts any affected Patch rebind.
- A saved project edit does not mutate a running package. If Live is active, stop and start the show so the new compiled snapshot becomes authoritative.

### Fixture-function component model

- `ember.fixtureFunctionBrowser` now has a toolkit-neutral, bounded snapshot model with stable row IDs, ASCII case-insensitive token search, category filtering, ordered accessibility labels, target coverage, safety status, and profile-specific raw-DMX diagnostics.
- Set/release construction re-resolves the stable selection against the current immutable project and only creates the existing exact fixture/group `UiCommandInvocation`. Partial groups, profile-divergence, stale selections, protected functions, and safety-gated rows are refused; the model never dispatches a command itself.
- The registry also describes `ember.fixtureProfileEditor`. Both component contracts are `bridged`: production skin-runtime activation, visual Designer placement, persistence, and toolkit rendering are not part of this checkpoint.

### Exact fixture-control Ember Action planner

- One catalog selection is re-resolved before deterministic canonical Ember Action source, prepared source, immutable foundation, and executable IR are accepted. A fixture plans exact set/release commands; only a complete, semantically uniform eligible group plans the atomic group commands.
- A mixed, partial, divergent, stale, missing, protected, or unconfirmed safety-gated choice fails instead of compiling a sequence of partial Runner operations. Continuous ranges are frozen at the reviewed catalog position until the executable Action subset supports variable mapping.
- The generated registry adapter now materializes enum values supplied through canonical `schemaRef` contracts, allowing the fixture-property singleton parameter to compile without copying a second hand-maintained enum.
- The planner does not persist, activate, schedule, bind, or expose Actions in a skin. Existing Runner command validation and safety authority remain final at execution.

### Registry compatibility

Registry set `1.2.0`, generation 2, source digest `0a647969b836a52106395709a8d83e5b22126f3c173d52466f9d056d4bf83699` contains seven component descriptions. The two new bridged components are a compatible-additive contract change. Callable scope is unchanged at 29 commands and 39 states; no private fixture command, state, capability, or alternate runtime engine was added.

## Surface behavior

### Studio / Static Looks

- The selected fixture/group produces a readable **Named profile function** list.
- Slot choices apply their profile-preferred value directly.
- Continuous ranges reuse the visible 0–100 control as position inside the named range, not as an undocumented raw DMX byte.
- Applying a mixed-profile group choice writes exact per-fixture values and refreshes the existing offline/physical preview path.
- Protected reset/service/reserved/unverified-custom functions are never offered.

### Live / Fixture Overrides

- The active-package fixture/group produces the same named catalog.
- Slots and continuous ranges invoke the existing registered `fixture.override.property.set` or `group.override.property.set` command path.
- Existing arming/safety gates, active-package checks, preview-lease exclusion, scoped release, and Release All remain authoritative.
- A mixed group choice that cannot be represented by one exact semantic command is unavailable with an operator explanation; fixture control or Static Look authoring remains available.

## White and Amber

There is no permanent White/Amber swap control. White and Amber remain ordinary direct Color mappings, and the general reviewed exchange can correct any compatible pair in a Local profile without teaching the engine a global W/A inversion. The manual-backed IR-4 regression compiles the Local 6CH profile after exchanging CH4/CH5 and proves a semantic White write reaches the selected White offset while Amber reaches the selected Amber offset. That is software mapping evidence only: the physical fixture mode, address, interface, raw response, and both owned IR-4 units remain unqualified. A real disagreement should become an evidence-backed Local profile revision after those facts are verified.

## Modular and skin boundary

`fixture_control_choices(...)`, `apply_static_look_control_choice(...)`, the fixture-function component model, and the fixture-control Action planner contain no Win32 types. The current workbench and comboboxes are replaceable adapters. Future controls should browse the same catalog, bind stable choice identity, and dispatch through canonical Studio or Runner boundaries. They must not reimplement range math, parse display labels, expose raw protected bytes, or create a skin-specific fixture engine.

The registry adds two bridged component descriptions but no callable command, state, or capability. Live and the component invocation builder supply arguments to existing registered property-override commands. The channel workbench, Autoloop editing, and mapping Learn remain recorded in the Authoring bypass-ledger area until typed Undo-aware Studio/profile/mapping commands and authoritative state replace that transitional host. The Action planner compiles existing registered commands but does not add project persistence or a private activation path.

## Focused evidence

The fixture-group regression uses two intentionally different profiles:

- a two-slot versus three-slot Color Wheel proves one named Static choice can require different semantic values;
- a shared Shutter / Open choice proves different profile raw bytes can still share one Live-safe semantic value;
- a continuous safety-gated Strobe range proves one position can resolve to different exact raw bytes while retaining one semantic value;
- a protected factory-reset range proves protected functions never enter the catalog;
- exact offline output proves the compiler maps the authored semantic values to the expected per-profile bytes.

The complete Make-driven native suite passes, including `core_tests`, `static_look_authoring_tests`, `live_ui_tests`, `fixture_profile_editor_tests`, physical-preview safety, OFL, Autoloops, hardware protocols, registry generation, and allocation-free Ember Action execution. The exact source also passes a Windows x64 Zig/Clang warnings-as-errors build with the real COFF resource and manifest.

Preview 92 was built from clean source `bff65d731a5ba35b04afdfa59c2ecf55210a09f4`. Seventeen package-contract regressions pass; the 18-file product stage plus manifest verifies; two NSIS compiles are byte-identical; 7-Zip tests/extracts the archive; and all 19 extracted payload files match the verified stage. The installer SHA-256 is `9729777492013ab7df6f4398462ee2ed69308a30f35cef0716dbd6e2dd5c049e`; the payload manifest SHA-256 is `cb38359ceb9c68c5b27dc12c1d34ddcf7ad0004bc79d45e3c55b268d287c9b4f`.

Preview 93 warning-fatal Make `all` and the complete 30-executable Make test surface pass. A fresh CMake/CTest build registers and passes the three focused tests covering exact mixed-profile Autoloop expansion, half-open timing, collision/ownership/capacity/stale/safety refusal, exact slot and continuous controller bindings, format-1 provenance round-trip, bounded gesture fanout, and content-safe migration lifecycle reporting. `autoloop_fixture_controls_tests`, `fixture_controller_binding_tests`, and `migration_portability_review_tests` pass.

Preview 94 source passes warning-fatal Make `all` and the complete 32-executable Make test surface. A fresh focused CMake/CTest build passes 6/6 across profile-workbench mutation, fixture-function component, fixture-control Action, existing fixture-controller binding, and Action foundation/executor coverage. The IR-4 regression verifies both semantic outputs after the reviewed direct-channel exchange; swap planning/apply also covers read-only, stale, fine-channel, compound, unsafe, incompatible, no-op, and atomic-failure boundaries. Registry generation/checks pass at set `1.2.0`, generation 2, digest `0a647969b836a52106395709a8d83e5b22126f3c173d52466f9d056d4bf83699`. The exact source also passes the Windows x64 Zig/Clang warnings-as-errors build and all packaging targets with the real COFF resource and manifest.

The clean Windows x64 cross-package from source `5c1f1f4b98343d08c82e9201af0fde4d20d1794b` passes all 17 package-contract regressions. Its 18-file product stage plus generated manifest verifies, two NSIS compiles are byte-identical, 7-Zip tests/extracts the archive, and all 19 embedded payload files match the stage. Unsigned installer `0.1.0-preview.94.0` is 1,927,262 bytes at SHA-256 `69eacbe90be204620b92f0ec9d35120af738f6c729898ad966ad71deb5a095ef`; manifest SHA-256 is `1af2441dd355e641ea8c6d0a21324a45d3c2c5b5c1b0290a4e9e35916f91e74f`. This is contract-tested off-Windows evidence, not a native install/upgrade/launch/uninstall or physical hardware claim.

The clean Windows x64 cross-package from source `375348e72ec37f7ed302f0cdc52d7c7dacaff865` passes all 17 package-contract regressions. Its 18-file product stage plus generated manifest verifies, two NSIS compiles are byte-identical, 7-Zip tests/extracts the archive, and all 19 embedded payload files match the stage. Unsigned installer `0.1.0-preview.93.0` is 1,904,991 bytes at SHA-256 `cda9457c6c2b3aca8c1624d52fe19ab36088c08e070b628aefaa4efd2864a69f`; manifest SHA-256 is `00d77eef6afe39db9483192528d992bd445734916a1f63798988f00c1b15aec4`. This is contract-tested off-Windows evidence, not a native install/upgrade/launch/uninstall claim.

## Honest remaining gaps

- activate the bridged fixture-function/profile components through the production skin runtime and Designer, persist and activate authored fixture-control Actions, and replace the current Autoloop/MIDI quick-pick adapters with that binding/component path;
- provide richer type-ahead browsing, category filters, icons, controller feedback, soft takeover, and responsive production-toolkit components;
- add reusable Position/Attribute assets, switching dependencies, multi-head/cell and pixel/matrix realization, and richer 16-bit ranges;
- pin a distributable offline fixture corpus and implement qualification/invalidation evidence;
- perform installed-Windows usability/accessibility/DPI tests and physical fixture/controller qualification.
- acquire authorized controlled-delta evidence before any exact SoundSwitch timeline decoder or WOLFMIX import claim.

Passing software tests proves deterministic mapping and fail-closed behavior. It does not prove a downloaded profile, physical mode/address, interface, fixture response, or gig readiness.
