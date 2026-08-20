# EmberLights Preview 103 — Windows x64 testing preview

This branch is an immutable artifact handoff. It is intentionally separate from the product source branches.

## Download

- `EmberLights-0.1.0-preview.103.0-Windows-testing-preview.zip`
- ZIP SHA-256: `2acdf712d7382fb899170920a6e605008e76f436d46fcb363bb9670a11505a14`

The ZIP contains:

- `EmberLights-0.1.0-preview.103.0-Setup.exe`
- `EmberLights-0.1.0-preview.103.0-Windows-payload-manifest.json`
- `EmberLights-0.1.0-preview.103.0-SHA256SUMS.txt`

## Identity

- Version: `0.1.0-preview.103.0`
- Packaged source commit: `4e7c71084f7d67ac5b796c64b9ee97895618b282`
- Packaged source tree: `9f1a14d1bc9581406e93d9bf2ea85f572f1795d0`
- Installer SHA-256: `b16476f577ea4b250d8fe359a7da0d4362b2bc2263981c4a5efa96618efde70c`
- Payload-manifest SHA-256: `6d2573550115314ea459616c04db6580042142e528f1fa854517a83fc60f4493`
- Owning PR: #94
- Handoff record: `docs/handoffs/BACKYARD_PARTY_OPERATOR_PREVIEW_103_HANDOFF_2026-08-20.md` on PR #94

## Evidence boundary

This is a **contract-tested non-Windows unsigned testing preview**. The package contract, clean Windows x64 cross-build, deterministic NSIS rebuild, archive integrity, extraction, and staged-payload byte identity passed. Native Windows install/upgrade/launch/uninstall, real VirtualDJ lifecycle, physical hardware, and gig qualification are not yet claimed.

Keep physical output disabled for the initial installation and OS2L lifecycle test. Do not use this preview as the only lighting controller at an event.
