# SoundSwitch Functional Parity Ledger

Last audited: 2026-08-13 against SoundSwitch 2.10 public product/support documentation and the Both Lighting BO-IR4 manual.

## Contract

This is the binding product-completeness checklist. Early engineering milestones are deliberately narrower, but a build is not “SoundSwitch parity,” a general public 1.0, or a finished replacement until every applicable row is **Verified**, **Equivalent**, or explicitly accepted as **Vendor-bound** with a safe interoperability path.

Statuses:

- **Verified** — implemented and covered by release evidence.
- **Partial** — a tested foundation exists, but user-visible parity is incomplete.
- **Planned** — accepted and sequenced, not implemented.
- **Research** — exact behavior or public integration path must be established.
- **Vendor-bound** — depends on proprietary hardware/software access; remains visible and cannot be silently dropped.

## Projects, fixtures, and venues

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| FIX-01 | Venue tabs/projects with reusable fixture patches | Partial | Native project open/save, fixture-profile and patch editors, validation, and fixed-capacity compiled loading exist; reusable venue tabs/templates remain. |
| FIX-02 | Searchable fixture library with manufacturer/model/mode selection | Partial | Profiles now includes bounded official Open Fixture Library search, exact-result selection, HTTPS QLC+ export download, per-mode quarantine, and one-click project import. Selected snapshots retain source URLs, MIT attribution, exact QXF SHA-256, and adapter evidence and never auto-update. A pinned offline corpus/index, richer mode presentation/direct transformer, and conformance qualification remain. |
| FIX-03 | Create and edit local fixture profiles | Partial | Native Windows profile editor persists semantic channel maps and compiles them; QXF/OFL modes import as inspectable read-only profiles that can be duplicated locally. The mapping assistant now includes a dedicated named-range editor for compound 8-bit channels, generated stable controller paths, exact ranges/preferred values, slot/continuous behavior, access/role, direction, owner/head/cell labels, and blackout/highlight. Saving a duplicate can atomically rebind every affected fixture after project validation and compilation. White/Amber uses the same generic Color mapping flow as every other emitter. Immutable revisions, qualification invalidation, broader usability, and physical evidence remain. |
| FIX-04 | Production/public/local fixture sources and safe sharing | Partial | QLC+ and live OFL provenance survives conversion with exact source evidence; downloaded snapshots are explicitly unreviewed and immutable. Signed updates, a pinned/bundled corpus, community review, and sharing workflows remain. |
| FIX-05 | RGB, RGBW, RGBA, RGBWAUV, CMY, color-wheel, dimmer, shutter, and strobe functions | Partial | A stable renderer-neutral catalog exposes all current color/intensity/position/beam/image/effect/atmosphere/custom lanes with explicit safety and DMX-chart policies. Persisted named slots and continuous ranges allow shutter/open/strobe and wheel-style functions to share one byte; QXF/OFL capability rows convert into unreviewed native snapshots. Protected service/custom ranges never activate and same-layer compound conflicts black out. Named choice controls on all authoring/live surfaces plus a representative physical conformance corpus remain. |
| FIX-06 | 8/16-bit pan/tilt and named Position Cues | Partial | 16-bit rendering exists; authoring, cue override, and transition tests pending. |
| FIX-07 | Gobo, prism, focus, zoom, iris, frost, rotation, and arbitrary Attribute Cues | Partial | Core semantics, sixteen custom lanes, persisted named range authoring, stable capability binding resolution, and bounded compound-channel playback pass. Static Look/Autoloop Attribute Cue pickers, dependency updates, and physical conformance remain. |
| FIX-08 | Multi-cell and effect fixtures | Partial | Channel-level fixture/head/cell ownership now persists and audits without entering the realtime scheduler as strings. Head/cell grouping, switching dependencies, matrix/pixel realization, representative fixtures, and UI remain. |
| FIX-09 | Fixture groups and category/role assignments | Partial | Native group and fixture-role editors persist stable IDs; group-targeted Static Look rows expand deterministically with portable tests. Category defaults and richer selection behavior remain. |
| FIX-10 | Stable repatching/replacement without cue loss | Partial | Stable IDs accepted; migration/relink tests pending. |
| FIX-11 | DMX chart, address editing, overlap/range validation | Partial | Native patch editor and overlap/range checks exist; graphical universe chart and hardware evidence remain. |
| FIX-12 | Two DMX universes | Partial | Fixed two-universe frames pass; complete output/hardware qualification pending. |

## Editing and authored content

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| EDT-01 | Add supported audio files and maintain durable track associations | Partial | Studio can add an external supported audio file as a SHA-256/size-identified asset, attach it to a Track Script, detect a missing/changed path, and relink one file or a bounded music-folder scan only when content is byte-identical. Metadata/fingerprint enrichment, waveform/beatgrid, and DJ-library integration remain. |
| EDT-02 | Waveform, beatgrid, zoom, looping preview, hotkeys, and trackpad editing | Planned | Studio usability and timing-accuracy suite. |
| EDT-03 | Main/Master control tracks | Partial | Semantic global intent exists; timeline editor/playback pending. |
| EDT-04 | Group and individual Fixture control tracks with precedence | Partial | Layer precedence passes; timeline authoring pending. |
| EDT-05 | Intensity curves, fades, flashes, and chases | Partial | Normalized layers/Autoloop interpolation pass; full effect library pending. |
| EDT-06 | Color picker, saved swatches, RGBWAUV mixing, and color transitions | Partial | Static Looks expose a Windows RGB picker, direct independent RGBWAUV/Master controls, pure-emitter swatches, capability support counts, and deterministic RGB hex round-trips. Versioned project palettes support RGB/HSV/HSL/CMY/hex/Kelvin-tint intent, transactions and Undo/Redo; stable swatch IDs resolve Autoloop V2 Palette events through explicit exact/degraded capability evidence in output-disabled preview. RGB intentionally does not synthesize W/A/UV; production toolkit exposure, measured calibration, and broader transition authoring remain. |
| EDT-07 | Position Cue creation, placement, and per-fixture overrides | Partial | Position properties render; cue authoring/override pending. |
| EDT-08 | Attribute Cue creation, global updating, placement, and AutoScript use | Planned | Named arbitrary attributes and dependency-update tests. |
| EDT-09 | Movement shapes: circle, scans, ovals, figure-eights, triple-eights, square, ramped size/speed | Planned | Deterministic effect generators and golden traces. |
| EDT-10 | Strobe blocks with start/end rates | Partial | Safe strobe property/cap exists; ramp authoring pending. |
| EDT-11 | Drag/drop Effects Generator with musical durations | Planned | Effect catalog, parameter editor, and deterministic traces. |
| EDT-12 | Static Looks with sparse fixture inclusion and explicit off | Partial | Compiled SET/RELEASE/FORCE_ZERO assignments, authored-fade activation/clear, and Live triggering remain. Studio has fixture/group capability selection, explicit included/off/follow-lower ownership, full-color RGBWAUV/master/strobe-safe assignment, dependency-aware management, exact offline DMX trace, and a selected-target physical preview that runs only with Live stopped, caps output, updates edits in realtime, times out, and fails closed to blackout. Toolkit-neutral drafts pass one-transaction generation/Undo tests; broad Win32 document-authority migration, installed usability, and owned-hardware qualification remain. |
| EDT-13 | Scripted tracks precisely tied to beatgrid/playhead | Partial | Native projects now persist optional portable audio keys and ordered beat-addressed semantic cues; fixed-capacity compilation, TrackScript-layer playback, Runner-published elapsed-beat/consumed-cue feedback, Live start/clear controls, and rewind replay pass core tests. Automatic track association, waveform/beatgrid Studio editing, loop/reverse/pitch/cue-jump replay corpus, and DJ-software qualification remain. |
| EDT-14 | Phrase detection and phrase editing | Planned | Offline analysis worker, manual correction, and persistence tests. |
| EDT-15 | AutoScript tracks, folders/playlists/crates, presets, styles, and custom settings | Partial | A deterministic bounded rule-based Autoloop proposal service accepts explicit seed plus musical-section or energy-band inputs, produces immutable provenance/digests and editable semantic content, and commits through authoritative Studio Undo/save/reopen. Track/folder/playlist orchestration, analysis worker, richer presets/styles/settings, bulk generation, UI review, and quality evaluation remain. |
| EDT-16 | AutoScript Autoloops using categories, colors, positions, attributes, effects, and randomization | Partial | Reproducible seeded Intensity/RGB semantic Autoloop proposals, strict work/content/cancellation limits, preview-without-mutation, occupied-ID refusal, and one-transaction commit pass. Palette realization exists after generation; category selection plus Position/Attribute/Movement/Effect generation and broader controlled randomization remain. |
| EDT-17 | Save projects and per-track lightshows safely | Partial | Versioned checksummed project format, atomic save/verify/replace, backup recovery, unknown-record preservation, and fixed-capacity compiler tests pass. Normal saves create verified, bounded 20-version restore history with a Windows restore flow; per-track lightshow files and cross-machine history/export workflow remain. |

## Autoloops and live performance

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| RUN-01 | Beat-synchronized fallback Autoloops for unscripted tracks | Partial | Beat-driven native playback, compiled project loops, live selection, manual BPM, and OS2L timing exist; VirtualDJ/hardware qualification and automatic selection policy remain. |
| RUN-02 | At least 128 Autoloops in four banks of 32 | Partial | Native catalog/compiler/authoring support 64 banks/2,048 loops, and an original deterministic starter pack provides 128 semantic programs/placements across banks 0–3. Installed default exposure, production UI and controller paging qualification remain. |
| RUN-03 | Reorder, move between banks, duplicate, populate empty, and reset defaults safely | Partial | V2 Studio authoring supports deep duplicate/delete with dependency reporting, assign/unassign/move/explicit swap/next-open placement across the 64×32 grid, one-step source Undo/Redo, idempotent populate-empty, and exact pack-owned reset that preserves unrelated content. Production list/drag UI and richer guided warnings remain. |
| RUN-04 | Exclusive banks and return-to-all-banks behavior | Partial | Runner-owned 64-bank enable/exclusive masks and return-to-all are status-visible, preserve across compatible live package activation, and pass core lifecycle tests. Live exposes a pageable four-bank window with per-bank enable, exclusive, and all-bank controls for Previous/Next navigation; MIDI Learn can select any exclusive bank, enable/disable any bank according to momentary/toggle behavior, or restore all banks. Hardware qualification remains. |
| RUN-05 | Trigger an Autoloop over a script, show progress, then return naturally | Partial | Manual activation, musically complete one-shot release, and return to the still-running scripted layer pass. Runner atomically reports the highest-priority active loop’s address, repeat behavior, 0–100% progress, and completed cycles; Live renders the named loop/progress/repeat/cycle and Diagnostics records the same data. Native TrackScript cues can start/clear Autoloops. DJ/hardware qualification remains. |
| RUN-06 | Infinite and track-duration repeat | Partial | Infinite/Once behavior and deterministic return pass through the V2 director/Runner package path. `TrackDuration` deliberately fails closed as `TrackBoundaryUnavailable` until Runner exposes a trustworthy track epoch; production UI and DJ lifecycle qualification remain. |
| RUN-07 | Static Looks in performance mode | Partial | Runner-owned latch, toggle, and hold/release behavior now shares one look-selection state across Live, MIDI, and named VirtualDJ OS2L buttons. Toggle switches or clears on a second press; momentary/OS2L release is target-aware so stale button-up events cannot clear a newer look. Crossfaded switching/clearing and lifecycle regressions pass; outbound controller/OS2L feedback and hardware qualification remain. |
| RUN-08 | Live Override FX for color, movement, intensity, strobe, white/UV, blackout, and effect actions | Partial | A dedicated Live Overrides page applies or releases any normalized property on an active-package fixture/group, now with a 0–100 slider and 0/25/50/100 presets, Runner-counted active total, scoped release, and Release All. Group changes use one validated fixture-mask command, never a partial UI sequence; MIDI can drive normalized properties or scoped literal-zero blackout. Overrides are disabled while Studio owns the bounded hardware-preview lease. Effect-oriented controls, hardware qualification, and richer color/movement tooling remain. |
| RUN-09 | Standalone operation without supported DJ software/controller | Partial | Manual clock/safe unsynchronized core passes; usable workflow pending. |
| RUN-10 | Clear DJ, interface, clock, active-loop, bank, and fault status feedback | Partial | Native Live exposes Runner, clock, adapter, frame, p99/max jitter, active Autoloop telemetry, active TrackScript elapsed-beat/consumed-cue status, pageable navigation-bank mask, and the current count of manual fixture overrides. Diagnostics includes the exact 64-bit bank mask and manual-override count. A JSON qualification tool records sustained health. Durable session logging, usability, and fault-injection acceptance remain. |

## DJ transport, mixing, and synchronization

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| SYN-01 | Direct VirtualDJ scripted-track, Autoloop, fader, reverse, and MIDI behavior | Partial | Reconnecting OS2L server and end-to-end beat→sync→Autoloop→frame lab path pass. Exact Static Look names plus explicit `Look: <name>` and `Autoloop: <name>` buttons now drive the same generation-stamped Runner state used by Live/MIDI, including stale-release protection and overflow evidence. Joshua verified live VirtualDJ BPM/beat input; VirtualDJ must use `os2l=Yes` rather than `Auto` to connect without first pressing a DMX pad. Full transport and outbound feedback remain unverified. |
| SYN-02 | Direct Serato DJ Pro integration | Research | Accepted as the second direct DJ integration after VirtualDJ; official/public route and replay corpus required before implementation claims. |
| SYN-03 | One-to-four-deck normalized state | Partial | Contract accepted; mixer engine and adapters pending. |
| SYN-04 | Blend, Cut, Scratch, and Upfader Only modes | Planned | Attribute-specific mixer rules and golden frame tests. |
| SYN-05 | Deck 1-2, 3-4, 1-4 selection and active-fader configuration | Planned | Multi-deck routing matrix. |
| SYN-06 | Upfader-intensity-only and external-mixer workflows | Planned | Mapping/configuration and replay tests. |
| SYN-07 | Reverse, scratch, seek, loop, loop-roll, slip, pitch, cue-jump, and playhead smoothing | Planned | Captured deterministic transport corpus. |
| SYN-08 | Loop Auto Strobe for sub-half-beat loops/rolls | Planned | Configurable, safety-capped trigger tests. |
| SYN-09 | Live audio BPM/phase detection | Planned | On-demand worker, confidence/failover, varied-music evaluation. |
| SYN-10 | Ableton Link single/multi-computer sync | Planned | Official SDK/license review and network qualification. |
| SYN-11 | MIDI Clock input and output plus MTC output | Planned | Byte/timing golden tests and hardware loopback. |
| SYN-12 | Traktor, Rekordbox, Ableton Live, and djay Autoloop workflows via Link/BPM/MIDI | Planned | Per-application compatibility matrix. |
| SYN-13 | Engine DJ/Engine Lighting integration and standalone export | Vendor-bound | Officially permitted integration/export path must be identified. |

## MIDI, controllers, and feedback

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| MID-01 | MIDI Learn and mapping for any available controller | Partial | Device-agnostic mapping, WinMM ports, Learn, persistence, and compilation exist. Fixture profiles now resolve generated stable named-capability paths into the same semantic property/value contract used by Static Looks, Live, MIDI, and future skins; protected ranges refuse binding. The MIDI editor does not yet browse those named paths. Named Track Scripts, group properties/blackout, bank controls, and release-all already work; controller UI and Windows hardware/hot-plug qualification remain. |
| MID-02 | Notes, CC, pitch, 7/14-bit, absolute/relative encoders, curves, inversion, scaling | Partial | Native mapping tests pass; additional encoder dialects pending. |
| MID-03 | Multiple simultaneous MIDI inputs/outputs | Partial | WinMM adapter represents 16 simultaneous inputs and outputs with isolated per-input queues; Windows compile, hardware, hot-plug, and fault tests remain. |
| MID-04 | Soft takeover, modifiers/layers, momentary/toggle/latch/timed/musical release | Partial | Soft takeover passes; complete behavior state engine pending. |
| MID-05 | LED/ring/pad feedback and conflict prevention | Partial | Portable short-message encoder and WinMM output adapter exist; bounded feedback rules, echo suppression, Windows hardware tests, and profiles remain. |
| MID-06 | Bundled Control One MIDI profile | Research | WinMM capture CLI is ready; owned-device capture and verified map remain. |
| MID-07 | Control One displays, onboard DMX, firmware, and standalone data | Partial | Two-port DMX identity, initialization, framing, routing, and Windows lifecycle are independently implemented with exact contract tests and an installed fail-closed qualifier. Owned-device electrical output/blackout/replug evidence remains; display, firmware, and standalone storage remain Vendor-bound. |

## Output, smart lighting, and interoperability

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| OUT-01 | Reliable DMX output through supported interfaces | Partial | Art-Net and sACN pass byte/loopback tests. Native ENTTEC DMX USB Pro serial framing, SoundSwitch Micro, and the isolated two-universe Control One JLC1/JLS1 transport pass exact contract/lifecycle tests. Physical Micro and Control One response, interface qualification, and the broader USB matrix remain. |
| OUT-02 | Art-Net output and third-party visualizer interoperability | Partial | ArtDMX golden bytes and exact loopback UDP delivery pass; real node/visualizer qualification and discovery remain. |
| OUT-03 | sACN/E1.31 output | Partial | E1.31 packet construction, deterministic CID, unicast/multicast sender, byte-level tests, and UDP loopback pass; real receiver qualification remains. |
| OUT-04 | QLC+ bridge for otherwise unsupported USB interfaces | Partial | Boundary specified; live loopback qualification pending. |
| OUT-05 | Interface hot-plug, disconnect/reconnect, and visible status | Partial | Network, DMX USB Pro, and SoundSwitch Micro outputs retry independently. Diagnostics separates Micro open state, accepted/failed writes, last Windows error, and nonzero slot count; stale queued frames are superseded. The installed `--active-test` entry is wired to a strict one-fixture manifest/acknowledgement/audit/graduation operator using the production Micro session, bounded presence checks, cancellation/device-loss handling, terminal blackout/close, and post-attempt candidate re-hash. Installed-Windows execution, owned-device disconnect/reconnect observations, CR-4 raw-to-Runner parity, and broader adapter qualification remain. |
| OUT-06 | Philips Hue integration | Planned | Local bridge integration with latency/degradation tests. |
| OUT-07 | Nanoleaf Shapes and Lines integration | Planned | Official local API capability/latency qualification. |
| OUT-08 | Wireless DMX compatibility through transparent DMX transmitters | Planned | Documented transport guidance and representative hardware test. |

## Migration, portability, and reliability

| ID | SoundSwitch capability to match or exceed | Status | Acceptance evidence |
| --- | --- | --- | --- |
| DAT-01 | Import/export projects between computers | Partial | Self-contained `.emberlights` projects open/save through native dialogs with checksums and unknown-record retention; assets/bundle packaging remains. |
| DAT-02 | Inspect and migrate `.ssproj` venues, patch, groups, looks, positions, attributes, and Autoloops | Partial | Read-only inventory, lossless source bundle, and controlled two-export byte-range comparison exist. A source-qualified 2.10.x converter produces an output-disabled 71-fixture color-rig patch, 18 native looks, 32 source-named semantic Autoloops, hashes, and an approximation report. A typed project/source review now verifies the recorded source identity separately from semantics and reports validation, output-disable state, per-area source/project counts, approximated/source-only/unqualified/missing/not-imported status, and stable repair actions. Exact address/cue decoding, movers/effects, broad-version support, and hardware qualification remain. |
| DAT-03 | Recover packaged lighting files and copied audio-associated script metadata where decodable | Research | Read-only probes; unknown payloads preserved losslessly. |
| DAT-04 | Relink moved audio and preserve scripted work | Partial | A moved file can relink one-at-a-time or through a bounded recursive music-folder scan only after an exact SHA-256/size match; same-named substitutions are refused and scripts retain their association. Metadata-assisted matching, background/cancellable scan progress, and conflict UI remain. |
| DAT-05 | Atomic saves, checksums, history, last-known-good activation, and recovery | Partial | Atomic temp-write/verify/replace, CRC32 validation, `.bak` recovery, verified bounded 20-version history/restore, 100-state Studio Undo/Redo, generation-stamped live hot-swap, one-step persisted Save & Apply Connections, and automatic last-project reopen are implemented. Canonical Autoloop V2 rich source now round-trips in one bounded versioned record with full generation/stamp checks, unrelated-record preservation, one document Undo/Redo, and save/reopen. Cross-version compatibility corpus and asset-bundle rollback remain. |
| DAT-06 | Fully offline show operation without a license server | Partial | Runner architecture requires no cloud; installer/activation proof pending. |
| DAT-07 | Windows application and installer | Partial | Native Win32 application, per-user Inno Setup recipe, file association, portable ZIP, packaged qualification tool, checksums, and release manifest exist; signing, upgrade/rollback, and low-end qualification remain. |
| DAT-08 | macOS parity | Planned | Explicitly follows the Windows production launch; portable core/adapter boundaries retained, platform qualification pending. |

## Source baseline

The audit uses official SoundSwitch 2.10 product and support material, including its Edit Mode, Performance Mode, Fixture Manager, integrations, updates, and compatibility documentation. The ledger records user-facing capabilities, not SoundSwitch's subscription/account mechanism or proprietary visual trade dress; our offline ownership model intentionally replaces those constraints.
