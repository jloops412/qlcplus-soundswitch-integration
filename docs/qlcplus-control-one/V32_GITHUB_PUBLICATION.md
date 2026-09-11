# V32 GitHub publication

The owner requested GitHub publication on 2026-09-11 after completion of the V31 reliability and V32 creative passes.

The active review lane is [PR #112](https://github.com/jloops412/qlcplus-soundswitch-integration/pull/112), on `fix/v30-performance-controls`, with qualification tracked in [issue #113](https://github.com/jloops412/qlcplus-soundswitch-integration/issues/113). This advances the existing lighting lane without changing protected releases or the separate Booth staging work.

## Exact source continuity

The connected GitHub publication preserves both source trees exactly. GitHub assigned new commit metadata, so the public commit IDs differ from the local checkpoint IDs.

| Pass | Local checkpoint commit | GitHub source snapshot | Identical Git tree |
|---|---|---|---|
| V31 reliability | `7808ed1aad81716eff0fe9ae3da4fc1a120d29a3` | `9c08250dc83b4b06e635c0269b2bd248437825d7` | `bc5ec7c8720dbc1900625b8f3a0afb5cb06be683` |
| V32 creative | `455bbdac2369cbc754736cf48f2d4204559c5252` | `d75a39cc3cbbbc3f00366f97f6a4ccf88df83a03` | `7d29cc6fda4208952bce409d688a79a378681b3b` |

The public V31 snapshot descends from the existing V30 head `f842153acb8edee790b8c47650c81b0285dcd33d`. The V32 snapshot descends from that public V31 snapshot. A subsequent documentation commit records this publication and supersedes the earlier request-to-wait language in active entry points. Historical audit/evidence files remain pre-publication snapshots.

- Protected V31 workspace SHA-256: `c6e03f865cfeda3fb5c578221ade3ef6883d87c033072a41bf5f1511f7f13651`.
- V32 workspace SHA-256: `16c676c531aa9aefa354eb2052e0f643be3c27d9e5fa71953deebf99257478c0`.
- All V26/V27 release bytes, fixture definitions and the input profile remain unchanged from the completed V32 checkpoint.

## Review and validation

Read [V32_CREATIVE_PASS.md](V32_CREATIVE_PASS.md), [the local validation evidence](V32_VALIDATION_EVIDENCE.json), and [the complete catalog](v32-review/V32_CREATIVE_CATALOG.md). Download `v32-review/V32_Creative_Review.html` and open it locally for the offline before/after review of all 160 scores.

Local V32 preservation, deterministic rebuilds and eight corruption checks passed. The protected V31 checks, V26/V27 package validators, and all six native software targets also passed. The PR Checks tab is authoritative for GitHub CI on the latest head; do not transfer an older V30 or local result to a different commit without evidence.

The SoundSwitch workflow builds for Windows x64 with Qt 6.8.1 and MinGW 13.1.0 against QLC+ source `a124abebe0b5ad6077727c561a5a0e1f3730810c`. Its test/build artifact does not prove loading in the complete pinned QLC+ host or physical output.

## Remaining qualification

This is source/workspace publication for review. It does not create an installable V32 release, replace a released DLL, emit live DMX, or establish gig readiness.

The remaining route is the matched Windows build and smoke check, pinned-host validation, a complete matching install/rollback package, physical review of all scores and control combinations, then the combined workload. Keep U3 Priority and U4 effects internal. Existing MOVE takeover, physical aims and independently scheduled Priority-frame boundaries remain documented in the creative pass.
