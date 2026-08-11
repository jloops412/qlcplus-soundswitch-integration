# `.emberskin` Package and Safety Limits v0

Status: provisional binding limits for issue #32 and toolkit spike #37. Values may be tightened after benchmark evidence; increases require explicit memory/startup/security review.

Related:

- `command-state-skin-contract-v0.md`
- `../../docs/21_UI_IMPLEMENTATION_PROGRAM.md`
- `../../docs/23_UI_TOOLKIT_EVALUATION_AND_SPIKE_PLAN.md`

## Purpose

A skin is untrusted presentation and binding data. It must never become a path to arbitrary code execution, unbounded memory use, scheduler contention, unsafe lighting actions, or loss of the current usable Live surface.

The runtime validates, resolves, and compiles a package before activation. Runner output continues with the current skin or Safe fallback while validation occurs.

## Package form

Provisional extension:

```text
.emberskin
```

Supported physical forms during development:

- directory package for bundled/development skins;
- deterministic ZIP-compatible archive for distributed skins.

The archive reader must not extract to arbitrary filesystem paths. It reads through a bounded virtual package interface or extracts only into an application-owned cache after full path validation.

## Required package structure

```text
manifest.json
locales/<locale>.json
styles/theme.json
layouts/<workspace>-<variant>.json
bindings/default.json
assets/...
```

Optional paths:

```text
layouts/components/*.json
styles/overrides/*.json
assets/icons/*.svg
assets/images/*.{png,webp}
metadata/preview.json
```

Executable files, dynamic libraries, scripts, shaders supplied by the skin, symbolic links, device files, and external URL dependencies are prohibited.

## Manifest minimum

```json
{
  "schemaVersion": 1,
  "id": "com.example.skin",
  "nameKey": "skin.name",
  "version": "1.0.0",
  "author": "Example",
  "minimumAppVersion": "0.1.0",
  "workspaces": ["live", "studio"],
  "variants": ["compact", "standard", "wide", "touch-live"],
  "fallbackVariant": "compact",
  "requiredCommands": [],
  "requiredStates": [],
  "optionalCapabilities": [],
  "contentHash": "sha256:..."
}
```

The final schema defines exact fields and canonical serialization. Unknown fields are rejected for security-sensitive sections and preserved/ignored only where explicitly versioned as extensible metadata.

## Provisional package limits

### Archive and files

| Limit | v0 value | Rationale |
| --- | ---: | --- |
| compressed archive | 16 MiB | bounded distribution/startup |
| total decoded package bytes | 32 MiB | protects Runner-with-UI memory budget |
| file count | 256 | prevents archive fan-out abuse |
| path length | 240 UTF-8 bytes | predictable Windows/cache behavior |
| path segments | 16 | prevents pathological nesting |
| individual JSON file | 2 MiB | layouts remain inspectable/bounded |
| combined JSON/localization | 6 MiB | keeps parsing/compiled graph bounded |
| individual raster asset | 4 MiB encoded | avoids giant textures |
| individual SVG asset | 512 KiB | limits path/parser complexity |
| locale files | 32 | sufficient initial localization range |
| variants per workspace | 8 | prevents combinatorial load |

Bundled first-party skins may exceed one individual asset limit only through a compile-time reviewed exception; total decoded and runtime-memory budgets still apply.

### Images

| Limit | v0 value |
| --- | ---: |
| raster width or height | 4096 px |
| total raster pixels decoded per package | 32 megapixels |
| animated raster formats | prohibited v0 |
| external color profiles | stripped/ignored safely |
| EXIF/metadata | ignored; not surfaced to UI |
| SVG element count | 4096 |
| SVG path commands | 65,536 total |
| SVG filters/scripts/external refs | prohibited |

Images decode off the Runner scheduler and are committed only after full validation. Failure to decode one required asset rejects the candidate package; failure of an explicitly optional asset uses the declared fallback.

### View graph

| Limit | v0 value |
| --- | ---: |
| total widget nodes in package | 8192 |
| active widget nodes in one rendered variant | 3072 |
| maximum tree depth | 32 |
| reusable component definitions | 256 |
| component expansion depth | 16 |
| named style rules | 1024 |
| state subscriptions active per variant | 2048 |
| command bindings per variant | 2048 |
| conditions/predicates per widget | 16 |
| tabs/pages in one container | 64 |
| grid children in one matrix | 2048 |
| focusable controls active | 1024 |

The compiler detects component cycles and expansion bombs before allocation of the final graph.

### Text and identifiers

| Limit | v0 value |
| --- | ---: |
| ID length | 128 UTF-8 bytes |
| localization key length | 160 bytes |
| ordinary displayed string | 4096 Unicode scalar values |
| tooltip/help string | 8192 scalar values |
| list item label | 512 scalar values |
| command argument literal string | 1024 bytes |
| expression source | 2048 bytes |

Strings are validated as UTF-8 and normalized according to the final localization policy. Control IDs are unique within their effective view graph.

## Allowed data and behavior

A skin may:

- define layouts and responsive variants;
- select approved components;
- map semantic theme tokens;
- bind controls to registered commands with typed constant/context parameters;
- subscribe to registered states;
- apply bounded formatting and presentation transforms;
- define focus order, labels, accessibility names, tooltips, and shortcuts subject to conflict rules;
- include original/licensed static image/vector assets;
- expose optional capabilities conditionally.

## Prohibited data and behavior

A skin may not:

- execute JavaScript, Lua, WebAssembly, native code, shell commands, or macros outside the approved command-composition model;
- open arbitrary files, registry keys, URLs, sockets, USB devices, MIDI devices, microphones, or cameras;
- load fonts, DLLs, plugins, or shaders from the package in v0;
- access project data except through approved component properties/state views;
- write app/project/live state directly;
- bypass command availability, safety gates, or priority classes;
- replace or intercept the emergency blackout path;
- poll state through unbounded timers;
- define arbitrary recursive evaluation;
- use external HTTP assets;
- persist secrets or account credentials;
- hide mandatory Safe controls in the Safe fallback surface;
- change Runner frame rate, adapter protocol, or output data except through approved commands.

## Expression and predicate model

V0 expressions are declarative and side-effect-free.

Allowed operations:

- boolean `and`, `or`, `not`;
- equality/inequality;
- numeric comparison;
- null/availability checks;
- bounded string selection/formatting;
- enum matching;
- basic arithmetic for presentation only;
- clamp/map/round/percent/unit formatting;
- conditional value selection;
- lookup in a bounded static map declared in the package.

Prohibited:

- loops;
- recursion;
- dynamic allocation visible to the skin;
- filesystem/network calls;
- command invocation from an expression;
- user-defined functions in v0;
- regular expressions unless a future bounded implementation is separately approved;
- time-of-day or random behavior as lighting authority.

Compiled expressions have a maximum operation count and stack depth. Evaluation failure produces a safe default and structured diagnostic; it never blocks the UI or Runner.

## Command binding rules

Every binding is validated against command metadata:

- command exists and is not removed;
- scope permits the skin/workspace;
- parameter names/types/ranges match;
- required target context is available;
- interaction type is compatible with the widget;
- safety gate is core-owned;
- emergency command restrictions are honored;
- deprecated commands produce diagnostics and use documented replacements where safe.

A skin cannot remap the hard emergency F8 blackout binding without a deliberate application-level owner preference and a guaranteed alternative. The Safe surface always has a direct Blackout control.

## State subscription rules

States declare update classes:

```text
static          changes only on project/skin activation
slow            up to 1 Hz typical
health          up to 4 Hz typical
transport       up to 20–60 Hz
progress        up to 30–60 Hz
onDemand        queried by visible native component
```

The UI runtime:

- coalesces updates;
- unsubscribes or throttles hidden panels;
- never requests a higher rate than state metadata permits;
- renders the latest snapshot and may drop visual frames;
- never feeds UI refresh timing back into show timing;
- records dropped/coalesced UI updates for qualification.

## Responsive variant selection

Variant selection uses validated app client size, DPI, input mode, and optional owner preference.

Selection order:

1. exact compatible preferred variant;
2. best compatible declared variant;
3. package fallback variant;
4. bundled Default compatible variant where policy permits;
5. bundled Safe surface.

A resize/DPI change compiles or selects the candidate view before replacing the current view. Active focus, page, bank window, and user-local panel state migrate only when compatible.

## Activation transaction

```text
Discover candidate
  -> read bounded package
  -> validate paths/types/limits
  -> verify manifest/schema/hash
  -> resolve localization/theme/assets
  -> validate commands/states/capabilities
  -> compile immutable view graph
  -> instantiate hidden candidate view
  -> smoke layout/focus/accessibility
  -> atomically swap UI view
  -> retire old UI resources after acknowledgement
```

Runner/project/show package are not reloaded by this transaction.

### First-load failure

- log structured failure;
- activate Safe fallback;
- keep Runner/output alive;
- offer Default/reset/open-diagnostics actions.

### Reload/switch failure

- keep the current skin and view active;
- display non-modal failure;
- do not partially apply assets/theme/layout;
- do not mutate user preference to the invalid candidate.

### Runtime component failure

- isolate the failed optional component where possible;
- show an error placeholder with diagnostic ID;
- mandatory Live component failure triggers Safe fallback only after the fallback view is ready;
- output continues throughout.

## Safe fallback surface

The Safe fallback is built, signed/hashed with the application, and independent of user skin assets. It may be hard-coded or compiled as trusted first-party resources, but must not depend on loading the failed package.

Minimum information:

- EmberLights/version channel;
- active project/package name and generation;
- Runner state/health;
- DJ/sync status and BPM;
- Universe 1 status;
- Universe 2 status;
- controller status;
- override count;
- safety/hazard status;
- last critical fault.

Minimum commands:

- Start when safe and available;
- Stop;
- Blackout set/release;
- Work Light set/release;
- Release All Overrides;
- Retry/reconnect affected adapters through validated commands;
- open Diagnostics;
- select bundled Default skin;
- quit safely.

Blackout and Stop retain their approved priority/emergency delivery paths.

## Package trust and provenance

V0 supports:

- bundled first-party package;
- local user-developed package;
- imported third-party package.

Every package records:

- ID/version/author;
- content hash;
- source path/import time;
- schema/app compatibility;
- validation result;
- asset/license metadata where provided;
- whether it is bundled, locally edited, or imported.

Future signing/community distribution is additive. Lack of a signature does not grant broader permissions; all packages remain sandboxed by capability.

## Caching

Compiled package cache:

- application-owned directory;
- keyed by package content hash + app/skin-schema/toolkit compiler version;
- bounded total size and LRU pruning;
- no executable content;
- invalidated on any validation/compiler version change;
- cache failure falls back to recompilation without affecting Runner;
- source package remains authoritative.

App-local user layout overlays are stored separately from the original package and are schema-validated. Reset removes the overlay, not the bundled package.

## Diagnostics

Validation errors include:

```text
package ID/version
file/path
JSON pointer or component ID
error category
limit and actual value
unknown command/state/token/capability
expected and actual type
fallback action taken
correlation ID
```

Do not expose personal filesystem paths in ordinary Live status; full local paths may appear in owner diagnostics/export with explicit consent.

## Fuzz and abuse tests

Required cases:

- ZIP slip and absolute/drive/UNC paths;
- duplicate/case-colliding paths;
- symlinks/reparse points;
- compression bomb;
- excessive files/nesting/path length;
- invalid UTF-8/JSON/Unicode edge cases;
- component cycles/deep expansion;
- excessive widgets/subscriptions/bindings;
- huge strings/images/SVG path counts;
- missing/duplicate IDs;
- unknown/deprecated commands/states/tokens;
- incompatible widget-command interaction;
- unsafe command attempt;
- malformed localization;
- hash/schema/app-version mismatch;
- failed asset decode;
- focus trap and unreachable emergency control;
- repeated rapid reload/switch;
- package deletion during validation;
- cache corruption;
- out-of-memory simulation where practical.

## Performance qualification

Measure:

- package read/validate/compile time;
- peak memory during validation and activation;
- final view graph memory;
- image decode/upload memory;
- first frame and interactive time;
- idle/active repaint cost;
- hidden-panel subscription cost;
- switch/reload resource retirement;
- 100 sequential valid/invalid switches;
- scheduler jitter during validation, activation, and failure.

Package work occurs off the scheduler. UI activation must remain inside the product startup/Runner-with-UI ceilings.

## Limit-change policy

To raise a limit:

1. identify a legitimate bundled/user use case;
2. include a representative package fixture;
3. measure parse/compile/runtime memory and CPU;
4. update fuzz/resource-abuse tests;
5. document compatibility/security impact;
6. update this spec and decision ledger.

Limits may be lowered in a new schema generation when required for safety. Existing packages receive explicit validation diagnostics and migration guidance.

## V0 acceptance

1. The validator enforces every implemented limit before activation.
2. Paths/assets/JSON cannot escape the package boundary.
3. No package can execute code or access devices/network/files directly.
4. Command/state/type/safety validation is complete.
5. Invalid first load reaches Safe; invalid reload preserves the current view.
6. Skin switching does not stop DMX, reset active content, or recompile the show.
7. Safe surface works with all optional skin assets unavailable.
8. Package fuzz/limit tests pass.
9. Performance evidence remains inside the Runner-with-UI envelope.
10. Limits and any benchmark-driven changes are machine-readable and documented.
