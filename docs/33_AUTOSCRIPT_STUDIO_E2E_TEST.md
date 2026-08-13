# AutoScript Studio end-to-end test

This preview adds the first complete V2 Autoloop user journey to the Windows
app. The proposal is deterministic, the preview cannot open an output adapter,
and Commit is one Undo transaction through the authoritative Studio document
service.

## Before generating

1. Open or create an EmberLights project.
2. Patch at least one RGB Dimmer fixture. AutoScript currently authors
   Intensity, Red, Green, and Blue, so a dimmer-only fixture cannot realize the
   preview.
3. Optionally give fixtures a role in **Studio • Patch** (for example `wash`).
   Leave AutoScript's role field blank to target every compatible fixture.

## Generate, review, and commit

1. Open **Studio • AutoScript**.
2. Set track bars, loop length, grid, style, complexity, energy, first bank and
   slot, and a stable whole-number seed.
3. Select **Generate + Offline Preview**.
4. Confirm the review panel says output adapters are `DISABLED` and shows a
   proposal digest, candidate-source digest, compiled digest, frame digest, and
   per-fixture DMX values.
5. Use **Preview Start** and **Preview Middle** to compare exact frames without
   transmitting them.
6. Select **Commit to Project**. The rich V2 source is now part of the project
   as one Undo step. It is not durable until the normal project Save succeeds.

If any project edit occurs after proposal generation, Commit fails stale. Use
**Discard**, then generate again from the newer document.

## Save, reopen, and perform

1. Save the project, close it, and reopen it.
2. Start Show normally. Production preflight compiles the exact persisted V2
   source; malformed, unsupported, or digest-mismatched source fails closed.
3. In **Live • Home**, the Autoloop list displays persisted entries as
   `B# / S# — V2 — name`.
4. Select the generated entry and choose **Launch**. Previous, Next, Clear, and
   all 64 bank filters continue through the typed Live command facade.

Once a persisted V2 catalog exists it is the authoritative live Autoloop
catalog. Format-1 Autoloops remain byte-for-byte in the project for
compatibility and migration, but the Runner does not mix both engines.

## Current boundary

- This slice generates one musical section per gesture. Generate additional
  sections into the next open bank/slot as needed.
- Position, movement, attribute, effect, and rich palette-reference authoring
  remain follow-up work. Unsupported semantic references fail closed.
- The Windows package is unsigned and still requires native Windows install,
  launch, uninstall, and physical output qualification before production use.
