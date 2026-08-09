# Product Requirements

## Target users and first qualification use case

The product is for mobile DJs, event companies, lighting operators, live performers, venues, and other users who need music-aware lighting without being locked to one fixture brand, controller, DJ platform, or event type. A user must be able to bring an unrelated rig, build or import accurate fixture profiles, patch a venue, author content, map any suitable MIDI controller, and run a show without Love & Light-specific assets.

Joshua's VirtualDJ, Pioneer DDJ-REV7, Control One, and Love & Light fixtures are the first real qualification environment, not the definition of the product. The system must run complete four-to-six-hour events reliably on the DJ computer or a separate lighting computer over a private network. The show model remains event-agnostic; event phases are reusable templates rather than hard-coded engine modes.

## Product completeness contract

- Full relevant SoundSwitch functional parity is the minimum finished-product bar, tracked in `13_SOUNDSWITCH_PARITY_LEDGER.md`.
- Early laboratory, shadow-gig, and pilot milestones may intentionally implement only a subset; those builds must be labeled honestly and cannot be called parity-complete or public 1.0.
- A parity item may be satisfied by a compatible implementation, a clearly better equivalent, or—only for a proprietary vendor-bound feature—a documented limitation with an approved interoperability path.
- Love & Light hardware and workflow preferences may influence defaults and testing, but cannot create dependencies in the domain model.

## Jobs to be done

1. Patch a venue/rig once and reuse it.
2. Run good beat-synchronized lighting automatically when no custom song script exists.
3. Run exact track-specific scripts when transport identity and playhead are trustworthy.
4. Override any relevant property immediately from MIDI without destroying the underlying show.
5. Handle event moments—entrances, formal dances, speeches, open dancing, send-off—more naturally than a generic console.
6. Survive network, controller, output-device, and project faults safely.
7. Migrate as much existing SoundSwitch investment as lawfully and technically possible.

## Functional requirements

### Fixtures, venues, and output

- Up to two active universes in V1; internal identifiers must not prevent future expansion.
- Fixture modes, 8/16-bit channels, RGB-family emitters, CMY, color wheels, dimmer/shutter, pan/tilt, gobos, prism, focus, zoom, multi-cell fixtures, and effect fixtures.
- Stable fixture identity independent of DMX address.
- Fixture groups, roles, physical placement, and named positions/attributes.
- Patch validation for overlap, range, missing profile, and unsafe capability.
- Art-Net and sACN as first-class network outputs.
- Selected published-protocol USB-DMX adapters.
- Optional QLC+ compatibility bridge for otherwise unsupported adapters.
- Fixture-profile creation, editing, import, validation, provenance, local storage, and safe sharing workflows.
- QLC+ QXF import as an ecosystem on-ramp, with deterministic conversion reports, source provenance, and quarantine for semantics that cannot be represented safely.
- Generic channel defaults, discrete ranges, inverted ranges, 8/16-bit functions, arbitrary attributes, multi-cell fixtures, and effect fixtures.

### Lighting content

- Initial authoring and migration priority is Static Looks first, then Autoloop banks; full custom track-script migration is not a first-use gate but remains mandatory for product parity.
- Autoloops with banks, flexible musical lengths, and reusable semantic steps.
- Sparse Static Looks with `SET`, `RELEASE_TO_LOWER_LAYER`, and `FORCE_ZERO` semantics.
- Static Looks latch until changed or cleared and use an author-adjustable crossfade; the provisional default is 750 ms.
- One-shot Autoloops start immediately when manually triggered, complete their full musical length, and return at their natural boundary; repeat-until-cleared and repeat-for-track modes are also supported.
- Momentary FX release on button-up with a short author-adjustable anti-snap fade; the provisional default is 100 ms.
- Master, group, and fixture-level programming.
- Track scripts with tempo, seek, loop, cue-jump, pitch, reverse, and fader-aware transport handling as parity matures.
- Position and Attribute Cues.
- Movement, color, intensity, beam, and strobe effects.
- Event-moment overlays and a safe idle/recovery look.
- Full manual track scripting, beatgrid/phrase editing, Main/Group/Fixture tracks, effect generation, and deterministic AutoScripting before parity-complete release.
- SoundSwitch-equivalent Autoloop organization and performance behavior, including the complete 128-loop/four-bank baseline, reordering, duplication, population, exclusivity, progress feedback, and repeat policies.
- The native library must exceed that baseline: 32 slots per bank, up to 64 banks/2,048 loops in a compiled V1 package, with any four banks exposed as a pageable controller window rather than a storage limit.

### DJ synchronization

- VirtualDJ OS2L beat, BPM, button, command, and feedback support.
- Versioned VirtualDJ adapter capable of adding documented database/Network Control/native-plugin paths when proven necessary.
- One-to-four-deck normalized state model even though initial testing may focus on two decks.
- Crossfader and upfader modes including Blend, Cut, Scratch, and Upfader Only parity targets.
- Sync state machine: healthy, predictive hold, audio fallback, recovery, manual, safe unsynchronized.
- Serato direct integration, Ableton Link, MIDI Clock input/output, MTC output, and the documented fallback integrations are required parity tracks after VirtualDJ is stable.
- Engine DJ/Engine Lighting behavior is a required vendor-bound investigation; no support claim is allowed without an official/public integration path.

### MIDI and physical control

- MIDI Learn for any controller.
- Multiple simultaneous input/output devices.
- Notes, CC, pitch, pads, faders, knobs, absolute/relative encoders, modifiers, and layers.
- Scaling, inversion, curves, range limits, and soft takeover.
- Momentary, toggle, latch, timed, and musical-boundary release behaviors.
- Outbound LED/ring feedback when a device supports it.
- Bundled Control One profile after messages are captured and verified.

### Migration and ownership

- Import `.ssproj` projects, packaged lighting files, and copied audio metadata where recoverable.
- Migration report: imported, translated, approximated, unsupported, missing, and conflicted.
- Read-only preservation of every original source artifact and unknown field.
- Never require mutation of the user's only audio copy.
- Open, documented native project/export format.
- Atomic saves, checksums, version history, schema validation, and recovery mode.

## Non-functional requirements

- Windows is the required production-launch platform. macOS support follows later; shared domain and adapter contracts must remain portable, but macOS cannot delay Windows qualification.
- Fully functional offline after installation.
- No license-server contact required to run a show.
- No dedicated GPU required for Runner.
- Runner cold start target under two seconds; release ceiling four seconds.
- Runner target under 50 MB headless and under 75 MB with its performance UI; ceilings 100/125 MB.
- Two-universe rendering target under 2% CPU on the reference DJ laptop; ceiling 5%.
- MIDI-to-render target under 20 ms; ceiling 30 ms.
- Typical input-to-visible-DMX target under 35 ms; ceiling 60 ms.
- DMX scheduling jitter p99 target at or below 2 ms; ceiling 5 ms.
- No allocation on the DMX scheduling thread after package load.
- One corrupt fixture, track, adapter, or imported payload cannot prevent Runner startup.
- Eight-hour stress run with no lost engine state before gig qualification.

## Explicit exclusions from the first vertical slice

- Polished waveform/timeline editor.
- AutoScripting AI.
- Serato, Engine DJ, Rekordbox, Hue, or Nanoleaf production integration.
- Proprietary Control One DMX/OLED/storage support.
- More than two exposed universes.
- Full Wolfmix feature parity.
- Public commercial licensing/payment infrastructure.

These are sequencing exclusions only. They do not remove any SoundSwitch capability from the product-completeness contract.
