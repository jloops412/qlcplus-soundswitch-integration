# Project Instructions for Build Agents

Read `docs/00_START_HERE.md` before changing code or scope.

**Immediate mandatory handoff:** read and execute `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` before beginning additional UI/skin or feature work. Until its core-ready gates pass, the active implementation priority is raw SoundSwitch Micro proof, reusable adapter/session hardening, frame/fixture truth, Connections, OS2L startup, and Static Look Toggle/Hold semantics.

## Authority order

1. Direct, current instructions from Joshua.
2. `docs/21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` while its recovery gates remain open.
3. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` entries marked **Accepted**.
4. `soundswitch-replacement-product-ledger.md`.
5. The remaining handoff documents.
6. Implementation details and provisional recommendations.

When sources conflict, later accepted decisions supersede earlier research. Important examples:

- Do not fork the complete QLC+ application. Use the accepted hybrid strategy.
- V1 exposes two universes even though post-V1 architecture must not block expansion.
- SoundSwitch is the primary workflow reference; Wolfmix is secondary.
- MIDI is device-agnostic; Control One is the first bundled profile.
- Host-accepted USB writes are not proof of physical DMX output.
- A staged migration patch that validates internally is not a physically verified fixture patch.

## Required engineering behavior

- Keep Runner deterministic, offline, and free of AI/model calls.
- Do not allocate memory on the DMX scheduling path after a show package is loaded.
- Keep hardware, DJ, audio, and controller adapters replaceable.
- Treat undocumented or proprietary Control One features as experimental and isolated.
- Never destructively modify a SoundSwitch source project or the user's only audio copies.
- Preserve unknown migration payloads losslessly.
- Add or update tests with every behavioral change.
- Record meaningful product/architecture changes in `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md`.
- Distinguish verified facts, inferences, and unresolved hypotheses.
- Report adapter lifecycle stages separately from physical qualification.
- Use raw-output tests to separate transport defects from fixture/profile/address defects.
- Keep every active hardware test bounded and fail to blackout.

## Scope discipline

The immediate order is:

1. Standalone raw SoundSwitch Micro hardware proof.
2. Reusable Micro adapter/session and reconnect hardening.
3. Rendered-frame inspection and one exact fixture/profile/address proof.
4. Visible and truthful Connections persistence/apply behavior.
5. Deterministic VirtualDJ/OS2L startup and reconnection.
6. Static Look Toggle/Hold ownership and Autoloop override/return semantics.
7. Reliable timing/output and adapter-service extraction.
8. Minimal gig UI.
9. Studio authoring, migration, and modular skin expansion.
10. Advanced automation and event-aware features.

Do not let a polished UI, AI feature, Wolfmix emulation, speculative fixture profile, or proprietary hardware experiment delay the gig-safe SoundSwitch replacement core.
