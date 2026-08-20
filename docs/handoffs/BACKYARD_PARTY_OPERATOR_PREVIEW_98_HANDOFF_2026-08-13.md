# Backyard-party operator Preview 98 handoff — 2026-08-13

Status: contract-tested unsigned Windows x64 testing preview. Native installed-Windows UI, lifecycle, accessibility, and physical hardware evidence remain pending.

## Primary outcome

Preview 98 is the first Default 2.2 UI/UX pass. It replaces six disconnected flat Studio forms with a consistent **Library + contextual Inspector** workflow while preserving EmberLights' modular engine, shared command/state registry, project services, and safety boundaries.

Profiles, Patch, Groups, Static Looks, Autoloops, and Track Scripts now provide:

- resource-aware bounded search across names, stable IDs, and useful metadata;
- visible counts plus empty, no-match, selected, and filter-hidden states;
- stable source identity even when a filtered row changes visible position;
- creating, editing, read-only, and unsaved Inspector status;
- explicit discard protection when changing selection or starting a new draft;
- `Ctrl+F` to focus the active Library and `Esc` to clear its filter;
- responsive Library/Inspector panels and separated action bars;
- contained, denser Static Look and Autoloop property layouts.

This is shared `ui_authoring` behavior and registered bridged component `ember.authoringWorkbench`, not a Win32-only replacement engine. Registry `1.3.0`, generation 2, remains compatible-additive. The 29 commands, 39 states, project format, compiled package, Runner, output routing, blackout, and hazard behavior are unchanged.

## Installer evidence

- Version: `0.1.0-preview.98.0`
- Source commit: `8eeee1dc7feb1711b96f8e05f49b394ea188e761`
- Installer: `EmberLights-0.1.0-preview.98.0-Setup.exe`
- Installer SHA-256: `5f7defcb946ba45bba1e99d05344ec484f363f503e877f2e46b787b9eb73185e`
- Installer size: `1,971,910` bytes
- Payload manifest: `EmberLights-0.1.0-preview.98.0-Windows-payload-manifest.json`
- Payload manifest SHA-256: `97b7ec5a9f7452c8992da8ec67c3e606ad9b358fe2cc813a7cae8a8f079143f5`
- Checksum evidence: `EmberLights-0.1.0-preview.98.0-SHA256SUMS.txt`
- Checksum-file SHA-256: `d7782e55387659d60f59792dc80e3a26cbd6e4fc97fe0f3cf9cb6db36883c0c0`

## Verification evidence

- focused authoring model tests cover stable identity, filtering, query bounds, UTF-8 preservation, summaries, Inspector modes, and responsive geometry;
- full warning-fatal native Make suite passed;
- registry generator, all 12 registry governance tests, Ember Action generated-adapter check, and surface-contract gate passed;
- clean canonical worktree and exact 40-character source identity;
- package-contract Python regressions: 18/18 passed;
- supported Windows x64 Zig/Clang application and tool targets built;
- exact 20-file CMake stage plus generated payload manifest verified;
- repeated NSIS construction was byte-for-byte identical;
- NSIS archive test and extraction passed;
- normalized extracted 21-file product payload matched the staged bytes and passed the package contract again;
- output-disabled V1 template and editable output-disabled IR-4 bench project are present.

Evidence class: **contract-tested unsigned testing preview** on a non-Windows host. This is not installed-tested, accessibility-qualified, physically qualified, gig-qualified, or a public release.

## Joshua's next test

1. Close EmberLights and SoundSwitch, then run the Preview 98 installer on Windows 11 and allow it to replace the prior per-user EmberLights install.
2. Confirm About reports `0.1.0-preview.98.0` and commit `8eeee1dc7feb1711b96f8e05f49b394ea188e761`; confirm the prior project/settings still open.
3. In Studio, visit Profiles, Patch, Groups, Static Looks, Autoloops, and Track Scripts. Confirm each page has a left Library/search surface and right Inspector/action surface with no clipped controls at the normal window size.
4. Press `Ctrl+F`, search by a name or stable ID, select a result, then press `Esc`. Confirm selection remains the same resource and the full Library returns.
5. Change an Inspector field without saving, then select another item. Choose **No** at the discard prompt and confirm the edited Inspector remains selected; repeat and choose **Yes** to confirm the other item opens.
6. In Static Looks, verify the emitter values, swatches, named function, property ownership, assignment/preview panes, and physical-preview controls do not overlap. In Autoloops, verify Bank/Slot/Length/Repeat and quick-step controls stay contained.
7. Save one harmless project edit, close/reopen it, and report any focus, clipping, search, selection, prompt, save, SmartScreen, installer, or preservation problem verbatim.

Keep an independent backup controller and do not use this preview as the only lighting controller at an event.

## Remaining boundaries

- native clean install, upgrade, launch, file association, Installed Apps uninstall, shortcut/registry cleanup, and project/settings preservation;
- Windows DPI matrix, keyboard traversal, Narrator/UI Automation, contrast, resize, and long-resource-list evidence;
- production toolkit decision, `.emberskin` runtime, Safe/Reference skins, Skin Designer, and full registry-driven authoring commands;
- installed Raw Hardware Test and exact graduated-project reopen;
- physical observations for both IR-4 units, blackout/no-spill, and disconnect/reconnect behavior;
- Authenticode signing, soak, shadow rehearsals, and gig qualification.
