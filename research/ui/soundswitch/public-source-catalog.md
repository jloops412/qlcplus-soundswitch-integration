# Public SoundSwitch UI Evidence Catalog

Last reviewed: 2026-08-11.

Purpose: index public/official sources that support the SoundSwitch Reference research program without copying their images into EmberLights assets or repeatedly rediscovering the same pages.

This catalog is Tier B/C evidence under `docs/19_SOUNDSWITCH_UI_FORENSICS_AND_CAPTURE_PLAN.md`. It can establish workflow, component presence, state vocabulary, and change history. It cannot freeze exact pixel dimensions, fonts, colors, or current native-window behavior; Tier A native captures remain required.

## Use rules

- Link to public sources; do not package source screenshots as skin assets.
- Preserve page title, URL, review date, and visible version context.
- Treat article arrows, red boxes, callouts, captions, and video chrome as documentation overlays unless native observation proves they belong to the app.
- Do not infer literal CSS, toolkit, source code, component names, or hidden behavior.
- Do not measure exact visual tokens from resized/compressed article images.
- Record native verification gaps in issue #30.
- Use original EmberLights icons, typography, colors, code, and assets.

## Official support sources

| ID | Source | Evidence supported | UI states/components to inspect | Confidence and gaps |
| --- | --- | --- | --- | --- |
| SS-PUB-001 | [What is SoundSwitch](https://support.soundswitch.com/en/support/solutions/articles/69000847410-soundswitch-what-is-soundswitch-) | Edit versus Performance mental model; music-aware scripted and automatic workflows; venue/fixture concepts | high-level Edit/Performance structure, library/timeline/performance surfaces | Official overview; images may be promotional or scaled. Verify current installed version natively. |
| SS-PUB-002 | [Getting Started in Edit Mode](https://support.soundswitch.com/en/support/solutions/articles/69000847416-soundswitch-getting-started-in-edit-mode) | Library navigation, loading audio, venue tabs, fixture tracks, timeline editing sequence | music library, venue/project area, central timeline, fixture/group track regions, waveform/transport | Official workflow evidence. Capture native empty/loaded/selected states and precise panel proportions. |
| SS-PUB-003 | [Control Tracks in SoundSwitch](https://support.soundswitch.com/en/support/solutions/articles/69000847597-control-tracks-in-soundswitch) | Master/Main, Group, and Fixture hierarchy and precedence | track headers, hierarchy, group expansion, fixture scope, cue placement | Strong behavioral evidence. Capture dense hierarchy, selection, mute/visibility/context states natively. |
| SS-PUB-004 | [UI and Status Indicator Changes in SoundSwitch 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858488-soundswitch-ui-and-status-indicator-changes-in-soundswitch-2-9) | DJ source selection, connected/disconnected state, hardware status, notifications | source selector, red/green or equivalent health, hardware indicator, notification affordance | Official versioned change evidence. Verify exact current colors/icons/text and multi-device/fault states in installed 2.10.x/current build. |
| SS-PUB-005 | [Autoloop Improvements in SoundSwitch 2.9](https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9) | Four banks × 32 loops, move/reorder/duplicate/populate, progress, direct launch over script, repeat policies, exclusive banks | bank selectors, 32-slot matrix, selected versus active, progress, context operations, repeat/exclusive state | Strong behavioral evidence. Native captures required for every bank, empty/disabled/active/progress/repeat/exclusive state and compact dimensions. |
| SS-PUB-006 | [Static Looks Explained](https://support.soundswitch.com/en/support/solutions/articles/69000863339-soundswitch-static-looks-explained) | Fixture inclusion/exclusion and explicit zero behavior; Performance use | Static Look editor, fixture inclusion, value/zero state, Live Look matrix and active state | Strong semantics supporting EmberLights RELEASE/SET/FORCE_ZERO. Native capture needed to understand actual editor controls and active/transition feedback. |
| SS-PUB-007 | [Performance Mode Preferences Explained](https://support.soundswitch.com/en/support/solutions/articles/69000847088-soundswitch-performance-mode-preferences-explained) | Live override, intensity, movement/strobe/fader preferences and behavior | Performance preferences tabs/controls, crossfader/upfader options, intensity/override settings | Official behavioral/settings evidence. Verify which controls are visible in current normal Live versus Preferences and which require restart/reconnect. |
| SS-PUB-008 | [How to Autoscript Autoloops](https://support.soundswitch.com/en/support/solutions/articles/69000844240-soundswitch-how-to-autoscript-autoloops) | Autoloop creation/generation workflow and editor relationship | Autoloop generation dialogs, categories/styles/options, resulting loop content | Useful authoring-flow evidence. Do not use as proof of future EmberLights AutoScripting engine; map only implemented capabilities. |
| SS-PUB-009 | [Saving Projects and Light Shows](https://support.soundswitch.com/en/support/solutions/articles/69000853039-soundswitch-saving-projects-and-light-shows) | SoundSwitch project/lightshow persistence concepts and troubleshooting | project save/reopen mental model, loaded audio/lightshow relationship | Behavioral evidence for migration friction. EmberLights deliberately uses clearer save state and scope; native capture of save/dirty/error UI remains useful. |
| SS-PUB-010 | [What's New in SoundSwitch 2.10](https://support.soundswitch.com/en/support/solutions/articles/69000869542-soundswitch-what-s-new-in-soundswitch-2-10-) | Version context, licensing/account changes, performance/bug-fix direction | 2.10 change history and whether 2.9 UI patterns continued | Official release context, not a complete UI specification. Record the exact installed build during Tier A capture. |

## Official product/tutorial sources to add when reviewed

These rows are intentionally open research tasks rather than unsupported claims.

| ID | Target source area | Evidence sought | Required action |
| --- | --- | --- | --- |
| SS-PUB-011 | Official Control One setup/mapping/performance documentation | hardware/software mirroring, bank/pad/fader feedback, shift/layer behavior | Locate current official pages/videos; record version and exact control states. |
| SS-PUB-012 | Official fixture manager/DMX-addressing documentation | profile search, local/public sources, universe chart, address overlap, venue setup | Locate current official pages/videos and native captures. |
| SS-PUB-013 | Official Position and Attribute Cue documentation | editor affordances, update propagation, per-fixture/group scope | Catalog current articles/videos and capture selected/editor/live states. |
| SS-PUB-014 | Official effects/movement/color/strobe editor documentation | timeline block geometry, property editors, effect categories, duration/speed/size | Catalog current tutorial sources and isolate documentation overlays. |
| SS-PUB-015 | Official troubleshooting pages for Performance/AutoScript connection behavior | disconnected, missing script, source mismatch, fallback states | Record edge-state screenshots and whether UI/version is current. |
| SS-PUB-016 | Official current release notes newer than 2.10, when present | changes to navigation, status, banks, editor, integrations, licensing | Check before Reference visual freeze and public release; update the observation ledger instead of silently assuming 2.10 remains current. |

## Community evidence policy

Community screenshots/videos are Tier D and may reveal:

- real-world window sizes and density;
- unusual faults and connection states;
- older-version changes;
- pain points and discoverability issues;
- workflows omitted from official documentation.

They do not establish exact current behavior without version/context. Record:

```text
source URL
publisher/date
visible SoundSwitch version if known
OS/resolution if known
screen/state
whether image/video is resized or annotated
claim supported
native verification gap
```

Do not quote community opinion as product fact; use it to generate research questions and usability tests.

## Source-to-capture mapping

| Public evidence | Required native follow-up |
| --- | --- |
| Edit Mode overview | empty and loaded project at 1366×768 and 1920×1080; panels, tabs, selection, context menu, dirty/save state |
| Master/Group/Fixture tracks | hierarchy expanded/collapsed; selected/muted/hidden/individual fixture; dense project |
| 2.9 source/status changes | every source option; connected/waiting/stale/disconnected/fault; hardware none/one/multiple; notification open/closed |
| Autoloop improvements | banks 1–4; slots empty/populated; selected/active/progress; exclusive/all; one-shot/infinite/track-duration; drag/context operations |
| Static Looks | editor inclusion/exclusion/zero; active/transition/clear; underlying Autoloop continuation |
| Performance preferences | each major tab/section; saved versus applied; restart/reconnect requirement; Live result |
| Save/lightshow guidance | new/open/save/save-as/dirty/failure/reopen; project versus audio/lightshow relationship |

## Evidence extraction outputs

Each reviewed source should contribute only the applicable artifacts:

- component inventory;
- workflow steps;
- visible state vocabulary;
- command/state candidates;
- screenshot capture gaps;
- Preserve/Improve/Reject row;
- evidence-tagged design requirement;
- source reference in the screen analysis.

Do not create a separate UI specification per support article. Consolidate findings into the observation ledger, Reference spec, command/state contracts, component contracts, and capture matrix.

## Refresh cadence

Re-check official SoundSwitch documentation:

- before the Tier A capture session;
- before exact Reference token/layout freeze;
- before a parity-complete/public release claim;
- when the installed SoundSwitch version differs materially from the catalog;
- when official release notes announce UI, Performance, Autoloop, fixture, controller, or integration changes.

Update the review date and change summary. Do not broadly re-research unchanged pages during ordinary implementation.
