# Backyard-party operator preview 90 handoff — 2026-08-13

Status: source-complete, synced to GitHub, and locally contract-tested. Windows installer `0.1.0-preview.90.0` was built from clean GitHub source `4268fe6bc91fb4012eb0d06a03424b9ebe464b6a`.

## Primary outcome

This preview removes the permanent White/Amber repair control and replaces that special case with a shared fixture-parameter catalog and an honest edit-and-rebind workflow. Profiles, Static Looks, Live overrides, and MIDI mappings now name the same semantic parameters consistently.

## What changed

- The fixture editor now exposes all 53 canonical parameters in stable groups: Intensity, Color, Position, Beam, Image, Effect, Atmosphere, and Custom.
- Every parameter carries shared control, surface, fine-channel, safety, and profile-default metadata. The model is toolkit-neutral so later skins and Skin Designer renderers can reuse it.
- Profile rows no longer require users to invent common safe values: `Apply Safe Defaults` derives the default from the selected parameter.
- Profile audit reports mapped/open slots, semantic/manual-chart/safety/custom rows, and repeated semantic assignments with stable issue codes.
- `Duplicate to Edit` remembers its immutable source. On save, EmberLights lists affected patched fixtures and can atomically rebind all of them to the new Local profile.
- Fixture Patch usage is shown by fixture, universe, and address so an operator can confirm that the profile being edited is the profile actually driving the lights.
- The permanent White/Amber correction button and its special selectors were removed. White and Amber are ordinary Color parameters through the same catalog used by looks, Live, and MIDI.
- Existing QXF import and official OFL search/download remain available. Imported profiles remain unreviewed until their channel map is verified against the fixture manual or physical output.

## White/Amber finding

The canonical compiler/output path was already correct: it emits White and Amber at the active profile's configured channel offsets, and focused tests prove those bytes are not globally inverted. The likely operator-facing failure was profile truth plus workflow: editing an immutable profile created a Local copy, but the patched fixture could remain bound to the original. Preview 90 fixes that missing rebind step generically for every parameter, not only White/Amber.

## Surface-contract reconciliation

- Project schema, renderer, persistence, runner safety boundaries, and public skin registries are unchanged.
- One shared descriptor catalog now supplies labels and policies across four Studio/Live authoring surfaces.
- The transitional authoring actions remain recorded in the bypass ledger pending registered-command conversion.
- No QLC+, Qt, OFL, SoundSwitch, or WOLFmix UI source, artwork, or assets were copied.

## Verified before packaging

- warnings-as-errors native build: passed;
- fresh isolated portable CTest suite: 32/32 passed in 18.40 seconds;
- exhaustive 53-parameter descriptor, profile audit, safe-default, generic rebind, and White/Amber byte regressions: passed;
- Windows x64 Zig/Clang warnings-as-errors GUI and profile targets: passed;
- `git diff --check`: passed.

## Installer evidence

- Artifact: `EmberLights-0.1.0-preview.90.0-Setup.exe`
- Size: `1,723,332` bytes
- SHA-256: `47693bd5203004b001d8b9a659cacbae1d8a7550130491e72cdb572909affc83`
- Payload manifest SHA-256: `048d05dddbbcddcd89f216abcd239b878d8b50b820d9f32ee9271c699fbcacfe`
- Source: `4268fe6bc91fb4012eb0d06a03424b9ebe464b6a`
- Version: `0.1.0-preview.90.0`
- Evidence: 17/17 package-contract regressions; exact 18-file payload manifest; repeated byte-identical NSIS compilation from the verified stage; 7-Zip archive test/extraction; exact 19-file staged/extracted payload comparison; normalized extracted-payload verification.
- Boundary: contract-tested unsigned preview built on Linux. Windows SmartScreen may warn. Native Windows clean/upgrade install, launch, Installed Apps uninstall, shortcut/registry cleanup, project/settings preservation, GUI/DPI/accessibility behavior, and physical hardware remain tester gates.

## First IR-4 tester route

1. Stop Live output, then open Studio → Fixture Patch. Note the exact profile on the IR-4 and its universe/address.
2. Open Fixture Profiles, select that same profile, and confirm `PATCH USAGE` lists the IR-4 at the expected universe/address.
3. If the profile is imported or built-in, choose `Duplicate to Edit`. Select the physical amber channel and assign `Color • Amber`; select the physical white channel and assign `Color • White`.
4. Use `Apply Safe Defaults` on both rows. Do not type an encoded parameter string.
5. Save the profile. When asked whether to move the listed patched fixtures to the Local profile, choose **Yes**.
6. Reopen Fixture Patch and confirm the IR-4 now names the new Local profile.
7. With normal Live still stopped, use Static Looks to preview isolated White and isolated Amber at low output. Record the physical result and the exact mode shown on the fixture display.
8. If the colors remain crossed after the Local profile is confirmed in Patch, swap the two semantic assignments in the channel table, save in place, and repeat the isolated preview. That result establishes the IR-4 mode's physical channel truth without a permanent repair control.

## Remaining boundaries

- physical IR-4 6CH/10CH manual/mode evidence and captured output verification;
- capability-range and named-slot editor for compound shutter, gobo, prism, movement, and pixel functions;
- user-defined controller layouts built from the shared semantic catalog;
- pinned offline fixture corpus plus richer manufacturer/model/category filtering;
- registered Studio authoring commands/state replacing transitional Win32 callbacks;
- production skins renderer/skins builder, responsive layouts, accessibility evidence, signed installer, and native Windows lifecycle qualification.
