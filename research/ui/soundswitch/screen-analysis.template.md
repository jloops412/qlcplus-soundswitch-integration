# SoundSwitch Screen Analysis — `<captureId>`

Manifest: `<path>`

## Evidence summary

| Field | Value |
| --- | --- |
| Evidence tier | A/B/C/D/E |
| SoundSwitch version/build | |
| OS / client size / DPI | |
| Workspace / screen / state | |
| Source and connection state | |
| Original SHA-256 | |
| Reviewer | |

## Screenshot boundaries

- Application client bounds:
- Operating-system chrome:
- Support/video annotations:
- Cropping/resizing/compression:
- Privacy redactions:

## Region map

| Region ID | Bounds or relative location | Purpose | Confidence |
| --- | --- | --- | --- |
| `R01` | | | MEASURED / ESTIMATED / BEHAVIORAL |

Recommended region vocabulary:

```text
chrome.menu
chrome.toolbar
chrome.status
navigation.workspace
navigation.venueTabs
library.music
library.effects
studio.trackHeaders
studio.timeline
studio.waveform
studio.inspector
live.primaryControls
live.overrides
live.autoloopBanks
live.autoloopMatrix
live.staticLookMatrix
live.contentFaders
drawer.connection
modal.preferences
```

## Component inventory

| Component | Count | Visible states | EmberLights component candidate | Confidence |
| --- | ---: | --- | --- | --- |
| | | | | |

## Geometry measurements

Every exact value must describe the measurement method.

| Token candidate | Value | Unit | Tag | Method / source region |
| --- | ---: | --- | --- | --- |
| | | px / ratio / degrees | MEASURED / ESTIMATED / DESIGN_TARGET | |

## Color and typography observations

Do not identify a compressed screenshot sample as an exact source token without qualification.

| Role | Sample/description | Tag | Notes |
| --- | --- | --- | --- |
| surface | | ESTIMATED | |
| text primary | | ESTIMATED | |
| text secondary | | ESTIMATED | |
| selection | | BEHAVIORAL | |
| connected | | BEHAVIORAL | |
| disconnected | | BEHAVIORAL | |

## State analysis

Distinguish:

- selected;
- active/output-owning;
- queued/pending;
- disabled/unavailable;
- filtered/excluded;
- connected/degraded/fault;
- dirty/saved;
- hover/focus/pressed;
- repeat/exclusive/override.

| Element | Observed state | How communicated | Ambiguity/problem | EmberLights requirement |
| --- | --- | --- | --- | --- |
| | | | | |

## Interaction inference

Only document interactions proven by official documentation, controlled observation, or a recording.

| User action | Observable result | Evidence | Tag |
| --- | --- | --- | --- |
| | | | BEHAVIORAL |

## Command/state mapping

| Visible control/indicator | Proposed command or state key | Availability/safety notes |
| --- | --- | --- |
| | | |

## Workflow represented

1. 
2. 
3. 

## UX strengths

- 

## UX friction

- 

## Preserve / Improve / Reject

| Element | Decision | Rationale | Reference-skin treatment | Default-skin treatment |
| --- | --- | --- | --- | --- |
| | Preserve / Improve / Reject | | | |

## Accessibility and gig-safety notes

- Is state communicated without color alone?
- Is keyboard focus visible?
- Are targets operable at the captured scale?
- Are destructive/emergency controls isolated?
- Is connection failure actionable?
- Could a blocking dialog obscure Live?
- Does the screen imply hidden transient state?

## Open evidence gaps

- 

## Approved derived requirements

Only list requirements approved for implementation. Each must retain its evidence tag.

- `[BEHAVIORAL]`
- `[MEASURED]`
- `[DESIGN_TARGET]`

## Review

- Product/UI review:
- Technical/runtime review:
- Evidence status: unreviewed / needs-more-evidence / approved-for-layout / approved-for-token-freeze
