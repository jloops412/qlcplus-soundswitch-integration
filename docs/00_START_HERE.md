# Start Here

## Mission

Create a uniquely owned, general-purpose, gig-ready replacement for SoundSwitch. The product must be usable by DJs and lighting operators with rigs unrelated to Love & Light Entertainment. It must eventually match every relevant SoundSwitch capability, then surpass it with open hardware, safer persistence, portable semantic scripts, SoundSwitch migration, and event-aware workflows.

This is not a generic DMX console and not a Wolfmix clone.

## Current state

- The original `Build Custom Lighting Software` conversation was reviewed in full on 2026-08-08.
- Later research superseded the initial suggestion to fork QLC+ wholesale.
- Joshua confirmed a hybrid QLC+ strategy is acceptable if the result remains uniquely ours.
- The handoff package now exists.
- The compiled/tested native vertical slice and first end-to-end `runner_lab` exist.
- EmberLights now has a versioned/checksummed project model and compiler, a deterministic service Runner, Art-Net, sACN, and initial ENTTEC DMX USB Pro serial output, WinMM MIDI/learn foundations, a bounded Studio-only QLC+ QXF fixture importer, and a native Windows Studio/Live shell. Windows/Linux CI and installer packaging pass on the merged checkpoint; searchable fixture-library distribution, VirtualDJ, MIDI, USB-DMX, broader output-hardware, soak, and live-event qualification remain active gates.
- Joshua clarified on 2026-08-08 that his fixtures are later validation inputs, not the product scope, and that full SoundSwitch feature parity is the minimum finished-product bar.
- `13_SOUNDSWITCH_PARITY_LEDGER.md` is the binding completeness checklist. Milestone exclusions sequence delivery; they do not remove features from the finished product.

## Binding product statement

> A general-purpose, SoundSwitch-first, automation-first DJ/event lighting workstation with a lightweight offline Runner, a capable Studio, open MIDI and DMX choices, first-class migration, and no dependency on one user's fixtures or hardware.

## What is accepted

- Windows is the required launch platform; macOS follows later and cannot block Windows delivery.
- Two DMX universes in V1.
- VirtualDJ/OS2L first; Serato is the second direct DJ-integration priority after VirtualDJ is stable.
- Predictive hold, then live-audio beat fallback, then tap tempo, then a safe unsynchronized look.
- Control One first as MIDI; onboard DMX/OLED/storage are experimental.
- Any MIDI controller can map to actions and parameters.
- Same-PC and separate-PC modes are equal.
- SoundSwitch import is core.
- SoundSwitch workflow outranks Wolfmix ideas.
- Original semantic/layered core; optional QLC+ output bridge, Studio-only QXF compatibility adapter, and selective audited reuse.
- OFL enters through a versioned Studio adapter and quarantine boundary; Runner consumes only our stable compiled profile format.
- Autoloops use a scalable 32-slot bank library; four banks are a controller/UI window, not an engine limit.
- No AI in Runner's live output path.
- The product name is EmberLights, and full-V1 Windows testing builds are distributed through an installer while qualification continues.

## What is not accepted yet

- Production implementation language and UI toolkit.
- Exact native support for MyDMX Buddy or SoundSwitch USB interfaces.
- Default live-control takeover/return semantics.
- Exact first-use balance between manual song scripting and AutoScripting.
- Any claim that proprietary Control One DMX or OLED functions are supported.

## Required reading order

1. This file.
2. `01_PRODUCT_REQUIREMENTS.md`.
3. `03_ARCHITECTURE.md`.
4. `04_V1_SCOPE_AND_ACCEPTANCE.md`.
5. `08_DECISIONS_AND_OPEN_QUESTIONS.md`.
6. `09_BUILD_AND_TEST_STANDARDS.md`.
7. The remaining research/backlog files as needed.

## Definition of progress

Progress is measured by a working, testable replacement path:

- exact inputs captured;
- deterministic state produced;
- correct DMX emitted;
- failures handled safely;
- footprint and latency measured;
- real hardware and VirtualDJ verified;
- user workflow tested.

Lines of code and mock screens are not milestones by themselves.
