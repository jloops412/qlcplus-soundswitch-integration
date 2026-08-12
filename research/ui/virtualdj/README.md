# VirtualDJ Skins, VDJScript, Pads, and Controller-Mapping Evidence

Status: **official-source research ledger and clean-room design input** for SKIN2-002, SKIN2-003, SKIN2-004, and SKIN2-006.

- Captured: 2026-08-12
- Source class: public official VirtualDJ manuals and VDJPedia pages
- Purpose: understand the documented source artifacts and workflows EmberLights may learn from or migrate from
- Non-purpose: copy VirtualDJ source code, proprietary artwork, product-specific audio behavior, or undocumented runtime internals

## 1. Research rule

VirtualDJ is the principal architectural inspiration for one action vocabulary shared across skins, custom controls, pad pages, keyboard mappings, and controllers. It is **not** the EmberLights runtime model.

EmberLights must preserve the useful separation while improving:

- visual authoring;
- responsive layout;
- type safety;
- compatibility and migration diagnostics;
- action discoverability;
- controller feedback authoring;
- package security;
- deterministic Runner isolation;
- ongoing registry continuity as features change.

All source-specific claims below remain tied to the listed official page. A migration adapter may parse only user-authorized source files and must classify each translated element independently.

## 2. Official source ledger

| Evidence ID | Official source | Documented behavior/artifact | EmberLights consequence |
| --- | --- | --- | --- |
| `vdj.skin-sdk` | https://virtualdj.com/wiki/skin%20sdk%20.html | Skin XML has a root skin element, pixel dimensions, image/preview metadata, visual child elements, and nested containers. | A VirtualDJ importer may inspect XML structure, dimensions, elements, containers, coordinates, and referenced assets. EmberLights converts into a draft designer source; it does not make XML/pixels the canonical runtime. |
| `vdj.interface-settings` | https://virtualdj.com/manuals/virtualdj/settings/interface.html | Installed skins can be selected, previewed, downloaded, and exported for editing. | EmberLights needs a first-class artifact manager, side-by-side versions, preview metadata, fork/edit flow, and explicit install/update/reset behavior. |
| `vdj.custom-buttons` | https://virtualdj.com/manuals/virtualdj/interface/decks/decksadvanced/custombuttons.html | Users assign VDJScript actions to custom buttons/dials through an editor with categories, descriptions, autofill, labels, and multiple assigned actions. | Command/Action Explorer, compatible control filtering, typed property editors, reusable actions, and multiple presets belong in the visual editor. Opaque action strings do not. |
| `vdj.pads-editor` | https://virtualdj.com/manuals/virtualdj/editors/padseditor.html | Users create/manage pad pages, assign push/pressure/color behavior, use shift actions, parameters, menus, and 8/16-pad layouts. | EmberLights custom pages need typed press/release/value/pressure-capable events where supported, shift/modifier layers, page parameters/context, feedback colors, and controller/software parity. Page capacity remains data-driven rather than copied. |
| `vdj.vdjscript` | https://virtualdj.com/wiki/VDJScript%29 | VDJScript is shared by skins, keyboard shortcuts, and controller mappers and supports simple actions plus complex macros. | Ember Actions must be the shared bounded composition vocabulary for software and hardware surfaces, with a visual graph and optional expert text over one canonical IR. |
| `vdj.verb-catalog` | https://virtualdj.com/manuals/virtualdj/appendix/vdjscriptverbs.html | VirtualDJ publishes a broad action/verb catalog used by editors and mappings. | EmberLights must generate Command/State/Component/Capability Explorer catalogs from canonical registries; no separate handwritten action list becomes authoritative. |
| `vdj.controller-overview` | https://virtualdj.com/wiki/Controller%20Developers | A controller uses a definition file for named MIDI/HID zones and a mapping file that associates names with VDJScript actions. | Keep physical device definition/capabilities separate from reusable action mappings. Both consume the same canonical command/action vocabulary. |
| `vdj.controller-mapper` | https://virtualdj.com/wiki/ControllerMappingFile_v8.html | Mapping XML associates a named control with a VDJScript action and records device/version metadata. | A migration adapter may propose Ember controller-profile bindings from user-supplied mapper XML, preserving source action text and rule evidence. |
| `vdj.controller-midi-definition` | https://virtualdj.com/wiki/controllerdefinitionmidi.html | MIDI definition XML can describe buttons, sliders, high-resolution controls, encoders, LEDs, colors, displays, SysEx, device matching, and physical pad geometry. | The Ember controller-profile model needs explicit input type/range/resolution/relative dialect, output feedback, logical matching, physical-layout metadata, and strict separation from skin/project content. |

## 3. Adopt / improve / reject matrix

### Adopt conceptually

1. One callable vocabulary across software controls, keyboard, pads, and controllers.
2. Replaceable interfaces rather than one fixed application layout.
3. User-editable custom buttons, dials, pad pages, shift layers, labels, and colors.
4. Device definition separate from action mapping.
5. Discoverable action categories, descriptions, and autocomplete.
6. Multiple mappings and skins for different workflows.
7. Editable/exportable user artifacts.
8. Software and controller feedback derived from the same functional state.

### Improve deliberately

1. **XML/image editing → visual responsive designer.**
2. **Pixel-first layout → toolkit-neutral constraints, variants, DPI, touch, and accessibility.**
3. **Opaque action strings → typed Ember Action Graph and canonical IR.**
4. **Manual file placement → artifact manager, validation, side-by-side install, updates, rollback, and Safe fallback.**
5. **Separate skin/pad/controller editors → one registry-driven authoring environment with shared components and bindings.**
6. **Implicit variable/action semantics → explicit types, units, target kinds, scopes, results, capabilities, deprecations, and diagnostics.**
7. **Ad-hoc compatibility → registry generations, semantic diff, migration preview, and deterministic relink.**
8. **Controller feedback as specialized mapping detail → first-class selector-derived software/hardware feedback contract.**
9. **Fixed source layouts/pad counts → dynamic bounded collections and responsive virtualization.**
10. **Manual scripting required for depth → visual recipes first, advanced graph second, expert text optional.**

### Reject for EmberLights

1. Arbitrary or unrestricted source code in skin packages.
2. Executing imported VDJScript.
3. UI timers/sleeps as lighting or musical timing authority.
4. Direct device, file, network, USB, MIDI, or DMX access from a skin/action.
5. Display labels, coordinates, or array positions as stable product identities.
6. Silent conversion of deck/audio/video verbs into unrelated lighting behavior.
7. Pixel-for-pixel copying of proprietary assets or trade dress.
8. A mandatory script interpreter or browser runtime in lean Perform.
9. Source-specific behavior in the canonical Ember Runner.
10. Claiming universal conversion when a source action has no semantic Ember equivalent.

## 4. Source artifacts a future adapter may recognize

The first VirtualDJ migration adapter may probe user-authorized copies of:

```text
skin ZIP/archive
skin XML
skin raster/vector assets referenced by XML
preview image metadata
controller definition XML
controller mapper XML
keyboard mapper XML where available
custom button/pad-page definitions where a supported export/source representation exists
```

The adapter must not assume all user customizations are independently exportable. When the official application stores or synthesizes an item in an unavailable form, the result is `unsupported` or `missingSourceEvidence`, not a guessed conversion.

## 5. Normalized migration mapping targets

| VirtualDJ source concept | Candidate Ember target | Required status discipline |
| --- | --- | --- |
| Skin root dimensions/image | Designer artboard/reference layer and source metadata | `exact` for parsed source facts; responsive conversion separately classified |
| Container hierarchy | Grid/stack/overlay/dock/layout proposal | exact only when semantics match; otherwise translated/approximated |
| Button/slider/text element | Registered primitive/native component proposal | element and properties classified separately |
| Action attribute/string | Preserved source action + versioned rule to Command or Ember Action proposal | never execute; unknown remains opaque |
| Custom button/dial | Custom control or action preset | typed target/interaction compatibility required |
| Pad page/pad/shift/parameter/color | Custom page, control, modifier/context, and feedback proposal | preserve page/control identities and source behavior evidence |
| Controller definition zone | Logical controller control/input/output capability | physical protocol fields remain source evidence until mapped |
| Controller mapper entry | Controller-profile binding to Command/Ember Action | rule/version and source pointer required |
| LED/color/text display | Selector/action-feedback output proposal | only when output capability and value mapping are proven |
| DJ/deck/audio/video behavior | Usually opaque or unsupported | do not invent lighting semantics |

## 6. First controlled synthetic corpus

CI must use synthetic, independently authored fixtures. Do not commit third-party skins or copyrighted artwork.

### Fixture A — minimal source skin

- one root size;
- one nested group/container;
- one button with one documented action string;
- one slider;
- one text/value element;
- one referenced synthetic image;
- one optional/unknown element.

Expected outcomes:

- source identity/hash exact;
- known geometry and hierarchy parsed;
- compatible primitives proposed;
- source action preserved;
- known mapped action translated with rule ID;
- unknown element retained opaque;
- no package execution.

### Fixture B — pad-page source

- one page;
- eight ordinary pads;
- shift actions;
- one pressure/value action where represented;
- two parameters;
- one menu;
- color/feedback definitions;
- one unknown action.

Expected outcomes:

- page/control identities stable;
- input entry points mapped explicitly;
- shift/context modeled rather than duplicated into hidden code;
- feedback mapped through registered state/action output only;
- unknown action retained and shown for manual mapping.

### Fixture C — controller source

- synthetic MIDI definition with button, absolute fader, 14-bit control, relative encoder, LED, RGB output, and physical pad geometry;
- mapper XML assigning documented and unknown action strings;
- one machine-specific identifier requiring redaction.

Expected outcomes:

- definition and mapping remain separate;
- input ranges/resolution/dialect preserved;
- known actions proposed through canonical mappings;
- feedback only proposed where source output and Ember selector semantics match;
- local identity redacted from portable output;
- unknown SysEx or vendor behavior retained opaque.

### Fixture D — abuse and failure set

- archive path traversal;
- duplicate/case-colliding paths;
- oversized XML/image;
- excessive depth/count;
- external entity and external URL attempts;
- malformed encoding;
- missing asset;
- unknown root/schema version;
- cyclic/invalid references;
- action string exceeding limits.

Expected outcome: deterministic rejection or bounded opaque preservation with no partial install and no effect on Runner/DMX.

## 7. Adapter lifecycle

```text
Probe source type/version
  -> inventory files and hashes read-only
  -> parse bounded source evidence
  -> create normalized migration IR
  -> apply versioned mapping rules
  -> show source-versus-proposed review
  -> resolve targets/capabilities/licenses/conflicts
  -> validate canonical designer/action/profile source
  -> create in staging
  -> transactional install or save
  -> deterministic report and re-import identity
```

Every source item receives:

- source artifact ID and hash;
- source pointer/path/XML location;
- adapter and rule version;
- parsed source facts;
- proposed canonical target;
- status: `exact`, `translated`, `approximated`, `opaque`, `unsupported`, or `conflicted`;
- diagnostic and owner decision;
- output artifact ID if accepted.

## 8. Security and licensing

- Source is immutable and read-only.
- XML external entities, external URLs, scripts, DLLs, plugins, and source code are never executed.
- Archive, path, XML, image, count, depth, and decoded-size limits apply before conversion.
- User-owned assets may be used locally, but export/distribution requires explicit license/provenance review.
- Ordinary reports redact absolute paths, serials, account information, and machine identifiers.
- CI fixtures are synthetic or explicitly redistributable.
- Import failure leaves the current skin, project, Runner, and DMX unchanged.

## 9. Open evidence gaps

1. Exact current on-disk/export representation for every custom-button and pad-page scenario must be established from user-authorized source evidence before parser support is claimed.
2. Skin SDK pages document many element types; support must be added in bounded slices, not by claiming the entire SDK at once.
3. Controller HID definitions require their own official-source and synthetic corpus before HID-specific import is claimed.
4. Complex VDJScript behavior needs a versioned mapping rule catalog and must remain opaque when no honest lighting equivalent exists.
5. Source assets may be usable locally but not redistributable; the designer/artifact manager must retain provenance and warn/block export accordingly.

## 10. First adapter proof gate

SKIN2-006 may claim a VirtualDJ proof adapter only when:

- the source probe is versioned and read-only;
- Fixtures A–D pass;
- every item appears in the migration report;
- no VDJScript executes;
- known action mappings are explicit and versioned;
- unknown behavior remains opaque or unsupported;
- responsive conversion is honestly classified;
- source/output directories are separate;
- re-import is deterministic and does not duplicate accepted identities;
- current active skin/project/Runner/DMX remain unchanged on failure;
- a second synthetic source proves the migration IR is not VirtualDJ-specific before broad adapter expansion.
