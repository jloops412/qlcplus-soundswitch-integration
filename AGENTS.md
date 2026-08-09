# Project Instructions for Build Agents

Read `docs/00_START_HERE.md` before changing code or scope.

## Authority order

1. Direct, current instructions from Joshua.
2. `docs/08_DECISIONS_AND_OPEN_QUESTIONS.md` entries marked **Accepted**.
3. `soundswitch-replacement-product-ledger.md`.
4. The remaining handoff documents.
5. Implementation details and provisional recommendations.

When sources conflict, later accepted decisions supersede earlier research. Important examples:

- Do not fork the complete QLC+ application. Use the accepted hybrid strategy.
- V1 exposes two universes even though post-V1 architecture must not block expansion.
- SoundSwitch is the primary workflow reference; Wolfmix is secondary.
- MIDI is device-agnostic; Control One is the first bundled profile.

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

## Scope discipline

The immediate order is:

1. Correct semantic core.
2. Reliable timing and output.
3. Real hardware/DJ probes.
4. Minimal gig UI.
5. Studio authoring and migration.
6. Advanced automation and event-aware features.

Do not let a polished UI, AI feature, Wolfmix emulation, or proprietary hardware experiment delay the gig-safe SoundSwitch replacement core.
