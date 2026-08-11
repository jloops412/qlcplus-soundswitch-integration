# EmberLights UI Design System and Brand Direction

Status: binding visual/interaction direction for EmberLights-owned skins and components. Exact token values remain `DESIGN_TARGET` until implemented, measured, accessibility-tested, and approved.

Related:

- `18_UI_UX_MODULAR_SKIN_ARCHITECTURE.md`
- `20_SOUNDSWITCH_REFERENCE_SKIN_V0_SPEC.md`
- `24_DEFAULT_UI_INFORMATION_ARCHITECTURE_AND_JOURNEYS.md`
- `spec/ui/command-state-skin-contract-v0.md`

## Brand experience

EmberLights should communicate:

- **calm control** in a dark, busy DJ booth;
- **warm creative energy** without looking like a gaming RGB dashboard;
- **professional reliability** appropriate for weddings, corporate events, venues, and touring use;
- **technical precision** for fixture programming and diagnostics;
- **independent ownership and openness** rather than hardware-vendor lock-in.

Working design phrase:

> **Calm in the booth. Expressive on the floor.**

The UI is subdued around the edges so lighting content, active state, safety, and faults are easy to see.

## Visual relationship to SoundSwitch

### EmberLights Default

Modern and distinctly EmberLights:

- warm ember accent;
- restrained dark surfaces;
- clear contextual hierarchy;
- soft but not bubbly geometry;
- spacious enough to scan, dense enough for serious authoring;
- strong state communication;
- minimal decorative animation.

### SoundSwitch Reference

Familiar structural rhythm with original EmberLights expression:

- neutral dark/charcoal technical workspace;
- lower-radius rectangular controls;
- denser timeline and utility chrome;
- cool selection/accent treatment where that aids SoundSwitch familiarity;
- EmberLights identity and safety/status tokens remain intact;
- no SoundSwitch logos, proprietary icons, fonts, screenshots, or copied artwork.

The Reference skin may use a different theme package and geometry density while sharing semantic status/safety tokens and components.

## Design principles

1. **Content carries color.** Cue, palette, fixture, and Look colors must remain distinguishable from application chrome.
2. **Status is semantic.** Brand color is not reused indiscriminately for warning, connected, selection, or danger.
3. **Danger is rare.** Red is reserved for faults, destructive confirmation, hazardous state, or blackout—not branding.
4. **Warmth without glare.** Ember accent identifies primary EmberLights actions and identity; it does not flood large surfaces.
5. **State before decoration.** Active, selected, queued, disabled, owned, released, degraded, and unsafe must be unambiguous.
6. **Density is contextual.** Studio can be compact; Live favors larger targets and immediate scanning.
7. **Motion explains change.** Animation is short, optional, and never the sole state signal.
8. **Numbers are instruments.** BPM, DMX address, frame rate, intensity, progress, latency, and jitter receive stable alignment and units.
9. **Every surface survives high DPI.** Use vector/path icons and density-independent layout tokens.
10. **Skins vary presentation, not safety.** Core danger/status behavior cannot be themed into invisibility.

## Theme architecture

Every visual value is referenced through semantic tokens. Bundled skins may map tokens differently but cannot redefine their meaning.

### Token categories

```text
color.surface.*
color.text.*
color.border.*
color.brand.*
color.action.*
color.status.*
color.selection.*
color.content.*
color.safety.*
space.*
size.control.*
size.icon.*
size.touch.*
radius.*
stroke.*
font.*
type.*
motion.*
elevation.*
opacity.*
z.*
```

Do not expose toolkit-specific properties as the public design system.

## Ember Dark — provisional default theme

All values are `DESIGN_TARGET` and require contrast/accessibility review.

### Core surfaces

```text
color.surface.app             #111416
color.surface.chrome          #171B1E
color.surface.panel           #1D2226
color.surface.panelRaised     #252B30
color.surface.canvas          #131719
color.surface.control         #293036
color.surface.controlHover    #333C43
color.surface.controlPressed  #1B2024
color.surface.overlay         rgba(8,10,12,0.88)
color.surface.input           #171C20
```

### Text

```text
color.text.primary            #EEF1F3
color.text.secondary          #B7C0C6
color.text.muted              #7F8A92
color.text.disabled           #596269
color.text.onBrand            #17120D
color.text.onDanger           #FFFFFF
```

### Borders and focus

```text
color.border.subtle           #30383E
color.border.standard         #414B53
color.border.strong           #65727C
color.focus.ring              #8BC8FF
color.selection.fill          rgba(63,154,219,0.20)
color.selection.border        #54AEE6
```

A cool focus/selection color is intentionally distinct from the warm brand accent and content colors.

### Brand

```text
color.brand.primary           #F08A3C
color.brand.primaryHover      #FF9A4D
color.brand.primaryPressed    #CF6F2D
color.brand.soft              rgba(240,138,60,0.16)
color.brand.glow              rgba(240,138,60,0.28)
```

Brand accent is used for:

- EmberLights identity;
- primary non-danger actions;
- current recommended path;
- selected high-level workspace in Default;
- restrained progress/accent where no content/state color conflicts.

It is not the warning token.

### Status

```text
color.status.ok               #49C77A
color.status.info             #4FA5DE
color.status.warn             #E7B24A
color.status.error            #E05252
color.status.offline          #7A858D
color.status.degraded         #D8873C
```

### Safety and emergency

```text
color.safety.blackout         #CC3030
color.safety.blackoutActive   #F04444
color.safety.workLight        #ECE6D4
color.safety.armed            #F06A34
color.safety.disarmed         #657078
color.safety.restricted       #C99A3F
```

Blackout must always include explicit text/icon/state, not merely red styling.

### Content colors

Content colors are authored data and use a contrast-normalization function against the theme. Application chrome must not interpret a red Look pad as an error unless the status layer explicitly marks an error.

Recommended separation:

- background/fill comes from content color;
- text/icon automatically chooses high-contrast foreground;
- active/selected/queued state uses border, progress, badges, and pattern—not destructive recoloring of the content.

## Additional bundled themes

### Booth High Contrast

Required by qualification, either at v0 or immediate follow-up:

- near-black surfaces;
- brighter text/borders;
- reduced translucency;
- strong focus rings;
- simplified shadow/elevation;
- non-color state patterns/icons;
- suitable for dim booths and users needing stronger contrast.

### Daylight

Planned, not a V0 gate unless inexpensive:

- light neutral surfaces;
- outdoor/daytime readability;
- content colors normalized for light background;
- the same semantic status/safety relationships.

### SoundSwitch Reference

Separate theme mapping defined in document 20 and refined from Tier A evidence. It may be cooler and denser, but cannot weaken EmberLights safety/accessibility invariants.

## Typography

Do not bundle or redistribute third-party font files without a verified license. Prefer a high-quality system/native UI font stack for v0 and preserve typography through semantic roles rather than font filenames.

### Type roles

```text
type.display          project/startup identity; rare
type.title            workspace/panel title
type.section          group heading
type.body             ordinary text
type.control          button/tab/pad label
type.caption          metadata/help
type.numeric          BPM, DMX, time, progress, metrics
type.code             command IDs, endpoints, diagnostic values
```

### Provisional scale

Density-independent targets:

```text
type.display.size     24–28
type.title.size       18–22
type.section.size     14–16
type.body.size        13–15
type.control.size     12–14
type.caption.size     11–12
type.numeric.size     contextual; 13–32
type.lineHeight       1.25–1.45 by role
```

Compact Studio may use the lower end. Touch-live and critical status use the upper end.

### Numeric behavior

- tabular digits;
- stable decimal precision;
- units visible where ambiguity exists;
- avoid jumping widths during live updates;
- BPM can emphasize integer/decimal hierarchy;
- DMX addresses use fixed alignment;
- latency/jitter always label milliseconds;
- progress exposes both graphical and accessible numeric value.

## Spacing system

Use a 4-unit base with semantic aliases rather than arbitrary per-screen values.

```text
space.0      0
space.1      4
space.2      8
space.3      12
space.4      16
space.5      20
space.6      24
space.8      32
space.10     40
space.12     48
```

Rules:

- internal control padding: typically 8–12;
- related-control gaps: 4–8;
- component-group gaps: 12–16;
- panel separation: 16–24;
- Live high-priority zones receive more spatial isolation;
- compact variants reduce group gaps before reducing target usability.

## Geometry

### Radius

```text
radius.none       0
radius.small      3
radius.medium     6
radius.large      10
radius.round      999
```

Default favors `small`/`medium`; Reference favors `none`/`small`; knobs/badges may be round.

### Strokes

```text
stroke.hairline   1 device pixel where supported
stroke.standard   1 DIP
stroke.strong     2 DIP
stroke.focus      2–3 DIP
```

High-DPI rendering must avoid blurry half-pixel boundaries.

### Elevation

Use elevation sparingly:

- canvas and dock hierarchy primarily use surface tone/borders;
- drawers/menus/modals may use shadow and raised surface;
- Live emergency controls use separation, not dramatic glow;
- avoid layered card-on-card visual clutter.

## Iconography

Use a coherent original icon set built from licensed/open vector sources or original EmberLights paths.

### Style

- simple geometric outline or restrained filled state;
- 1.5–2 DIP effective stroke;
- rounded joins where compatible with the brand;
- recognizable at 16, 20, 24, and 32 DIP;
- active/filled variants only when state requires them;
- no imitation of SoundSwitch proprietary icon artwork.

### Semantic icons

Required core vocabulary:

- project/open/save/history/recovery;
- Studio/Live;
- fixture/group/venue/profile/patch;
- Look/Autoloop/Track Script;
- palette/color/movement/position/attribute/strobe/intensity;
- MIDI/controller/learn/binding;
- VirtualDJ/DJ source generic integration icon;
- network/USB/universe/output;
- connected/degraded/fault/offline;
- play/pause/stop/previous/next/repeat/one-shot;
- release/clear/force-zero;
- safety/armed/restricted;
- Work Light/Blackout;
- search/filter/favorite/duplicate/move/swap;
- command/state/info/help/warning.

Vendor logos may only appear where trademark use is appropriate and licensed/allowed; the core UI should work with generic adapter icons.

## Component state model

Every interactive component supports the relevant subset of:

```text
rest
hover
focus
pressed
selected
active
queued
latched
disabled
unavailable
loading
success
warning
error
degraded
owned
released
forceZero
```

Do not conflate:

- **selected** — navigation/editor focus;
- **active** — currently affecting output;
- **latched** — remains active until changed;
- **queued** — pending a boundary/acknowledgement;
- **disabled** — intentionally unavailable by configuration;
- **unavailable** — capability/target missing in current state;
- **error** — attempted operation failed.

## Core component direction

### Buttons

Variants:

- primary;
- secondary;
- quiet/toolbar;
- toggle;
- danger;
- emergency;
- icon-only with accessible name;
- split/menu.

Primary is used sparingly. A page should not contain a wall of equally primary buttons.

### Pads

Pads can show:

- authored content color;
- label and optional slot/index;
- selection border;
- active fill/progress edge or overlay;
- queued indicator;
- repeat/exclusive/filter badges;
- mapping/shortcut hint;
- unavailable reason on inspection;
- high-contrast accessible text.

Progress is Runner-owned state, not a UI animation clock.

### Faders

- stable scale and numeric value;
- large grab area even when track is visually narrow;
- soft-takeover marker when hardware differs;
- optional default/unity marker;
- group/target name stays legible;
- keyboard increments and direct numeric entry;
- no uncontrolled changes from scroll-wheel accidents without focus/intent policy.

### Knobs

- do not rely only on angular position;
- show numeric percent/value;
- clear default/reset affordance;
- coarse/fine keyboard or modifier behavior;
- drag direction consistent throughout the app;
- touch-live target size qualified.

### Status badges

Format:

```text
[icon] Label: State [optional detail/action]
```

Examples:

```text
DJ: Connected
U1: Ready
U2: Fault
Sync: Hold 0.4s
Project: Unsaved
```

Color supplements text/icon and never replaces them.

### Inspector fields

- label, control, unit, validation, scope, and reset/default relationship;
- pending reconnect/restart shown next to the field;
- project versus device-local scope available on inspection;
- advanced fields grouped/collapsed;
- destructive changes use preview/confirmation when impact is broad.

### Timeline

- dark low-noise canvas;
- clear major/minor musical grid hierarchy;
- Master/Group/Fixture scope visible in track header and cue styling;
- content colors distinguish semantic cue type while preserving contrast;
- selection, hover, muted, soloed, disabled, and overridden states distinct;
- playhead and loop range remain visible without obscuring cues;
- waveform is subordinate to editable lighting data but aligned precisely;
- motion/animation limited to playhead and active indicators.

### Diagnostics

- structured cards/tables, not one unparsed text blob;
- severity, owner, timestamp, current/recovered state;
- exact error code/details available on expansion/copy;
- safe next action near the fault;
- exportable report;
- dense numeric metrics use tabular type and restrained color.

## Live visual hierarchy

Priority order:

1. Blackout and hazardous safety state;
2. output/DJ/Runner faults;
3. active content and temporary overrides;
4. master/group/content intensity;
5. primary performance parameters;
6. pads/Looks/Moments;
7. navigation and secondary diagnostics;
8. decorative branding.

No decorative element may visually outrank a fault or active blackout.

## Motion system

### Allowed

- 80–160 ms hover/press/selection transitions;
- 150–250 ms drawer/panel transitions;
- state crossfade when it clarifies ownership;
- progress updates from shared state;
- subtle connection/retry activity indicator;
- brief success acknowledgement.

### Restricted

- continuous glows/pulses in ordinary operation;
- large parallax or background motion;
- spring/bounce on live controls;
- animations that delay command dispatch;
- animations tied to beat unless they represent actual output state and are inexpensive;
- flashing error states beyond safety-qualified limits.

Reduced motion disables or shortens non-essential animation.

## Sound and haptics

Desktop v0 should not emit routine UI sounds that compete with DJ monitoring. Optional controller/touch haptics may be considered later through platform preferences and must not become command acknowledgement authority.

## Content writing

### Voice

- direct;
- calm;
- actionable;
- technically accurate;
- no anthropomorphic blame;
- no vague “Something went wrong” without details.

### Status examples

Prefer:

```text
VirtualDJ disconnected — listening on 127.0.0.1:9996. Retry
Universe 1 fault — SoundSwitch Micro write failed (Windows 31). Reconnect
Project not activated — fixture addresses overlap on Universe 1
Skin reload rejected — unknown command `autoloop.fire`
```

Avoid:

```text
Oops!
Not working
Connection failed
Apply settings
```

### Button labels

Use explicit verbs:

- Save Project;
- Reconnect VirtualDJ;
- Release Overrides;
- Activate Package;
- Open Diagnostics;
- Restore Version;
- Move to Next Empty;
- Swap Slots.

Use `Apply` only when a real staged transaction exists and the scope is obvious.

## Branding placement

- full EmberLights wordmark on startup/Project Hub/About;
- compact ember glyph in application chrome;
- minimal branding in dense Studio canvas;
- no watermark over timeline or Live controls;
- version/channel in About/Diagnostics, not prominent chrome;
- Reference skin remains labeled `SoundSwitch Reference` with an explicit independent-implementation description in skin details.

## Asset governance

Every bundled visual asset records:

- source/author;
- license;
- modification status;
- intended sizes;
- semantic name;
- hash/version.

Do not commit or redistribute SoundSwitch screenshots, logos, proprietary icons, or extracted UI assets as application resources.

## Design token versioning

- tokens have stable semantic names;
- value changes do not require skin-schema changes when meaning is preserved;
- removed tokens receive aliases/deprecation for at least one public skin-schema generation;
- safety/status tokens cannot be overridden below contrast/visibility floors;
- custom skins may extend tokens under their namespace but must provide required core roles.

## Qualification

Both Default and Reference must be reviewed for:

- required DPI/resolutions;
- WCAG-informed contrast for ordinary text/control boundaries where applicable;
- Windows focus/UI Automation behavior;
- non-color state communication;
- reduced motion;
- touch-live target sizing;
- content-color contrast normalization;
- emergency control visibility;
- daylight/dim-booth readability on representative displays;
- color-vision-deficiency simulation;
- screenshot/golden stability;
- CPU/repaint/memory effects of shadows, blur, translucency, and animation.

## Design-system acceptance for v0

1. Semantic token registry exists and is used by both bundled skins.
2. Default and Reference have separate theme mappings without duplicating component logic.
3. Brand, selection, status, content, and danger colors remain semantically distinct.
4. Primary controls, pads, faders, knobs, status badges, Inspector fields, timeline, and diagnostics implement the state model.
5. Text/icon/state remains understandable without color.
6. High-DPI and keyboard/focus qualification passes.
7. Safe fallback remains clearly EmberLights but does not depend on optional theme/asset packages.
8. Original asset/license manifest is committed.
9. Motion and advanced effects stay inside UI performance budgets.
10. No SoundSwitch proprietary visual asset appears in bundled EmberLights resources.
