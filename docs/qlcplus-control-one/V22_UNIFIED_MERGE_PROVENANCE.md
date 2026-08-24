# V22 Unified Merge Provenance

This record prevents the V21 reliability/UI line and the later Variety Pro creative line from drifting apart again.

## Authoritative inputs

| Role | File | SHA-256 |
|---|---|---|
| Host | `IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw` | `87445A94A40768A045A667F8EFEEC03025B9C1AAC2DC4C78B913C8299C4587CF` |
| Creative donor | `IR4-TUBES-ALL-BANKS-VARIETY-PRO.qxw` | `74250B053F82DAEAEBBE1BED4B35CD49BD01EFA4CB33E95194974A5FB13BE206` |
| Result | `IR4-TUBES-CONTROL-ONE-V22-UNIFIED-PRO.qxw` | `7AC6ED5413E2C4593B79D414C58C5ADADBAB2C3474A665F7E8FF3631FBA012E7` |

The similarly named Variety Pro autosave is not an input. It includes 32 additional save-state XML differences absent from the authored donor, so accepting it would make the creative delta ambiguous.

## Forensic delta

The authored Variety Pro workspace differs from its Static Pro predecessor by exactly:

- 22 raw Autoloop Chasers; and
- 176 supporting Scene Functions, eight per changed Chaser.

The 32 Static/Priority Look roots are unchanged by the donor. The V21 Priority Look implementation is therefore retained because it carries the required private-Universe authority and release behavior.

Changed raw Function IDs:

```text
568 572 573 574 575 576 580 581 583 587 589
590 593 632 633 635 636 637 645 647 657 658
```

These correspond to:

- Colorful: Tropical Run, Festival Six, Citrus Chase, Berry Chase, Ocean Chase, Fire Chase, Green Magenta Cross, Blue Gold Cross, Reverse Color Ladder, Teal Gold Sparkle, Mardi Gras, Miami Neon, and Electric Violet.
- Flashy: UV Punch, Gold Hit, Drop Chase, Red Fixture Hits, Blue Fixture Hits, Purple Gold Punch, CMY Quick Cut, Center-Out Drop, and Outside-In Drop.

## Merge rules

`qlcplus/workspace-tools/Merge-V22UnifiedPro.ps1` performs the merge and refuses an unexpected donor delta.

1. Load V21 as the host.
2. Verify the donor changes exactly the expected 22 raw Chasers.
3. Preserve each public raw Chaser ID while replacing its Chaser XML.
4. Import the 176 required donor Scenes.
5. Remap the 17 donor helper IDs that collide with V21 UI-control Functions to fresh private IDs and rewrite the affected steps.
6. Preserve V21 fixtures, I/O, manual owners, Autoplay controls/parents, Priority Looks, logical channels, and plug-in binary.
7. Add a disabled active-loop monitor layer for raw Chasers 532–659, outside the playback owner SoloFrame and without external Inputs.
8. Validate references, IDs, fixtures, ownership, and the exact existing-Function delta.

## Active-loop outline rationale

Putting another playback control inside the owner SoloFrame would compete with the manual/Autoplay Collections and recreate the one-second flash failure. V22 instead places disabled monitor Buttons behind the real pads. Each Button observes one raw Chaser’s native Monitoring state but cannot start or stop anything.

The same outline therefore follows:

- a manually latched Chaser;
- sequential or random Auto Bank;
- sequential or random Auto All; and
- a pad seek while Autoplay remains active.

No new MIDI/logical channel or plug-in behavior is required.

## Verification command

From the repository root:

```powershell
releases/qlcplus-control-one/v22/Test-V22Package.ps1 `
  -HostWorkspace releases/qlcplus-control-one/v21/IR4-TUBES-CONTROL-ONE-V21-RELIABILITY.qxw
```

The validator checks the complete package, exact creative delta, imported Scenes, fixture patch, private Priority closure, monitor coverage, scripts, plug-in hash, and personal-data boundary.
