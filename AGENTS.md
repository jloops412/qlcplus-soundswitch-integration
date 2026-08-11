# Project Instructions for Build Agents

Read `docs/00_START_HERE.md` before changing code or scope.

**Immediate mandatory handoff:** for the active core-recovery implementation, read `docs/handoffs/CORE_RECOVERY_BUILD_AGENT_HANDOFF_2026-08-11.md` first, then execute `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md`. For fixture-library, fixture-profile, migration-patch, or one-fixture bench work, also read and execute `docs/23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md`. For Joshua's owned IR-4, BO-TUBE192 360 Tube, or CHAUVET DJ Wash FX Hex qualification, also read `docs/24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md`. Until the core-ready gates pass, the active implementation priority is raw SoundSwitch Micro proof, reusable adapter/session hardening, frame/fixture truth, Connections, OS2L startup, and Static Look Toggle/Hold semantics.

## Authority order

1. Direct, current instructions from Joshua.
2. `docs/handoffs/CORE_RECOVERY_BUILD_AGENT_HANDOFF_2026-08-11.md` for the scheduled core build slice.
3. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` while its recovery gates remain open.
4. `docs/23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md` for fixture source, profile, patch, provenance, and hardware-qualification work.
5. `docs/24_OWNED_FIXTURE_SOURCE_INVENTORY_AND_FIRST_BENCH_PLAN.md` for the first owned-fixture bench slice.
6. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` entries marked **Accepted**.
7. `soundswitch-replacement-product-ledger.md`.
8. The remaining handoff documents.
9. Implementation details and provisional recommendations.

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

## Required engineering behavior

- Keep Runner deterministic, offline, and free of AI/model calls.
- Do not allocate memory on the DMX scheduling path after a show package is loaded.
- Keep hardware, DJ, audio, controller, and fixture-source adapters replaceable.
- Treat undocumented or proprietary Control One features as experimental and isolated.
- Never destructively modify a SoundSwitch source project, exported personality, fixture manual, or the user's only audio copies.
- Preserve unknown migration and fixture-source payloads losslessly.
- Add or update tests with every behavioral change.
- Record meaningful product/architecture changes in `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`.
- Distinguish verified facts, inferences, and unresolved hypotheses.
- Report adapter lifecycle stages separately from physical qualification.
- Use raw-output tests to separate transport defects from fixture/profile/address defects.
- Preserve fixture provenance, source/native hashes, conversion warnings, and qualification state.
- Invalidate profile/patch qualification when behavior-affecting data changes.
- Keep every active hardware test bounded and fail to blackout.

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

Do not let a polished UI, AI feature, broad catalog, Wolfmix emulation, speculative fixture profile, or proprietary hardware experiment delay the gig-safe SoundSwitch replacement core.
