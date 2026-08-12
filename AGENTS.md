# Project Instructions for Build Agents

Read `docs/00_START_HERE.md` before changing code or scope.

**Immediate mandatory handoff:** for the active core-recovery implementation, read `docs/handoffs/CORE_RECOVERY_BUILD_AGENT_HANDOFF_2026-08-11.md` first, then execute `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`. For fixture-library, fixture-profile, migration-patch, or one-fixture bench work, also read and execute `docs/23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md`. For Joshua's owned IR-4, BO-TUBE192 360 Tube, or CHAUVET DJ Wash FX Hex qualification, also read `docs/24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md`. Until the core-ready gates pass, the active implementation priority is raw SoundSwitch Micro proof, reusable adapter/session hardening, frame/fixture truth, Connections, OS2L startup, and Static Look Toggle/Hold semantics.

**Studio lane:** when the assigned work is Studio authoring, SoundSwitch source compatibility, migration evidence, media/track identity, waveform/beatgrid, timeline, or Studio-side compilation, read issue #46, `docs/28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md`, and `docs/29_STUDIO_V1_BUILD_HANDOFF.md`. The first Studio work slice is the document-service/source-manifest/migration-IR foundation only. It must not modify Live/Runner/output behavior, claim fixture-profile truth, broaden the current approximate converter, or begin cosmetic timeline/skin work before the source/document contracts are implemented. Studio, Live/output, fixture, and UI-platform lanes reserve shared files before editing and integrate only through the versioned contracts in doc 28.

**Skins platform lane:** when the assigned work touches skins, UI commands/state, components, capabilities, bindings, keyboard/MIDI/controller mappings, custom controls, overlays, Ember Actions, the visual Skin Designer, skin/mapping migration, or package compatibility, read issue #63, `docs/SKINS_PLATFORM_V2_START_HERE.md`, and the smallest matching route it names. The registry lifecycle policy is mandatory for every user-visible feature agent, not only skin agents. Do not add hard-coded surface behavior without registered command/state/capability/component reconciliation, and do not create an arbitrary script VM or a second lighting engine. This lane coordinates with #31/#32/#34/#37/#39 and remains subordinate to open core-ready gates for broad rollout.

## Authority order

1. Direct, current instructions from Joshua.
2. `docs/handoffs/CORE_RECOVERY_BUILD_AGENT_HANDOFF_2026-08-11.md` for the scheduled core build slice.
3. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` while its recovery gates remain open.
4. `docs/23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md` for fixture source, profile, patch, provenance, and hardware-qualification work.
5. `docs/24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md` for the first owned-fixture bench slice.
6. `docs/28_STUDIO_V1_AUTHORING_MIGRATION_AND_SOURCE_COMPATIBILITY_PLAN.md` and `docs/29_STUDIO_V1_BUILD_HANDOFF.md` for work explicitly assigned to the Studio lane under issue #46.
7. `docs/SKINS_PLATFORM_V2_START_HERE.md`, ADR 0006, and the linked contracts for work assigned to issue #63 or any user-visible surface/registry change.
8. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` entries marked **Accepted**.
9. `soundswitch-replacement-product-ledger.md`.
10. The remaining handoff documents.
11. Implementation details and provisional recommendations.

`docs/24_FIXTURE_LIBRARY_AND_PROFILE_QUALIFICATION_PLAN.md` is useful supporting research created concurrently. It does not supersede the active fixture authority above; where the first bench target differs, use the newer manufacturer-backed IR-4 6-channel plan.

When sources conflict, later accepted decisions supersede earlier research. Important examples:

- Do not fork the complete QLC+ application. Use the accepted hybrid strategy.
- V1 exposes exactly two universes even though post-V1 architecture must not block expansion.
- SoundSwitch is the primary workflow reference; Wolfmix is secondary.
- MIDI is device-agnostic; Control One is the first bundled profile.
- Host-accepted USB writes are not proof of physical DMX output.
- A staged migration patch that validates internally is not a physically verified fixture patch.
- A fixture-library match or successful import is not hardware qualification.
- Do not scrape or redistribute protected SoundSwitch Production personality content.
- Fixture-library updates must never silently mutate a project snapshot.
- Do not use the OFL `Chauvet DJ WashFX` near-match as a Wash FX Hex profile.
- Control One MIDI support does not prove or imply proprietary Control One DMX/OLED support.
- A `.ssproj` alone is not assumed to contain every saved scripted lightshow; project, lighting-file, copied-audio, metadata-tag, and DJ-library evidence are inventoried independently.
- A retained name, filename, or path is not sufficient evidence for exact cue semantics or automatic track association.
- Skins are presentation/binding packages over one engine; Default, SoundSwitch Reference, Safe, user skins, mappings, and remotes do not get private domain behavior.
- Ember Actions are typed bounded compositions over registered commands/state, not arbitrary JavaScript/Lua/WASM/native code and not a show-timing engine.
- Adding, removing, renaming, or changing a user-visible feature requires registry, generated-artifact, bundled-skin, mapping/action, schema, test, deprecation, and migration reconciliation in the same bounded work.

## Required engineering behavior

- Keep Runner deterministic, offline, and free of AI/model calls.
- Do not allocate memory on the DMX scheduling path after a show package is loaded.
- Keep hardware, DJ, audio, controller, and fixture-source adapters replaceable.
- Treat undocumented or proprietary Control One features as experimental and isolated.
- Never destructively modify a SoundSwitch source project, exported personality, fixture manual, or the user's only audio copies.
- Preserve unknown migration and fixture-source payloads losslessly.
- Add or update tests with every behavioral change.
- Record meaningful product/architecture changes in `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` or an accepted ADR linked from the relevant start file.
- Distinguish verified facts, inferences, and unresolved hypotheses.
- Report adapter lifecycle stages separately from physical qualification.
- Use raw-output tests to separate transport defects from fixture/profile/address defects.
- Preserve fixture provenance, source/native hashes, conversion warnings, and qualification state.
- Invalidate profile/patch qualification when behavior-affecting data changes.
- Keep every active hardware test bounded and fail to blackout.
- Keep SoundSwitch migration source evidence, editable Studio data, and compiled Runner packages as separate representations.
- Keep source/library indexing, audio analysis, waveform generation, migration decoding, skin designing, package import, action compilation, and asset processing off the Runner and DMX scheduler.
- Route new user-callable behavior through canonical registered commands and publish authoritative registered state/result feedback; no new untracked UI/controller callback bypass.
- Treat skins, overlays, actions, mappings, profiles, and migration inputs as untrusted bounded data with no direct device/file/network/state access.
- Keep the lean Perform path limited to validated compiled View Graph, Action IR, Binding Tables, native component adapters, and bounded state subscriptions.

## Scope discipline

The immediate order is:

1. Standalone raw SoundSwitch Micro hardware proof.
2. Reusable Micro adapter/session and reconnect hardening.
3. Rendered-frame inspection and one exact fixture/profile/address proof using the fixture qualification plan.
4. Visible and truthful Connections persistence/apply behavior.
5. Deterministic VirtualDJ/OS2L startup and reconnection.
6. Static Look Toggle/Hold ownership and Autoloop override/return semantics.
7. Reliable timing/output and adapter-service extraction.
8. Minimal gig UI.
9. Studio authoring, migration, searchable fixture-library, and modular skin expansion.
10. Advanced automation and event-aware features.

Parallel work explicitly assigned by Joshua may proceed in isolated lanes when it does not alter or delay the active core. For Studio, follow issue #46 and docs 28/29, reserve shared files, and stop the first slice at the document-service/source-manifest/migration-IR foundation. For the skins platform, follow issue #63 and `docs/SKINS_PLATFORM_V2_START_HERE.md`, coordinate registry names with #31/#64, and do not begin broad visual rollout before its runtime/toolkit/core dependencies.

Do not let a polished UI, AI feature, broad catalog, Wolfmix emulation, speculative fixture profile, or proprietary hardware experiment delay the gig-safe SoundSwitch replacement core.

