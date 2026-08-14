# Start Here

## Mission

Create a uniquely owned, general-purpose, gig-ready replacement for SoundSwitch. The product must be usable by DJs and lighting operators with rigs unrelated to Love & Light Entertainment. It must eventually match every relevant SoundSwitch capability, then surpass it with open hardware, safer persistence, portable semantic scripts, SoundSwitch migration, event-aware workflows, and a fully user-extensible skins/control-surface platform.

This is not a generic DMX console and not a Wolfmix clone.

## Current state

- The original `Build Custom Lighting Software` conversation was reviewed in full on 2026-08-08.
- Later research superseded the initial suggestion to fork QLC+ wholesale.
- Joshua confirmed a hybrid QLC+ strategy is acceptable if the result remains uniquely ours.
- The handoff package now exists.
- The compiled/tested native vertical slice and first end-to-end `runner_lab` exist.
- EmberLights now has a versioned/checksummed project model and compiler, a deterministic service Runner, Art-Net, sACN, and initial ENTTEC DMX USB Pro serial output, WinMM MIDI/learn foundations, bounded Studio-only QLC+ QXF import plus official OFL search/download of selected immutable snapshots, a native Windows Studio/Live shell with toolkit-neutral Default 2.2 visual and authoring-workbench bridges, and a renderer-neutral fixture-parameter catalog shared by structured profile authoring, Static Looks, Live overrides, Autoloops, MIDI/controllers, Ember Actions, and future skins. Static Looks, Live, committed Autoloop V2 event authoring, and persistent MIDI/controller mapping consume target-specific named profile functions through that catalog. Preview 94 adds a general Local-profile channel-map workbench, a bounded fixture-function component model, and a deterministic planner that compiles an exact fixture or eligible group choice into the existing Ember Action command surface. The component model is not yet activated by a skin runtime, and the planned Action is not yet a persisted/activated user asset. Profile duplication can atomically rebind every affected fixture after validation/compilation; White and Amber are ordinary Color parameters handled through the same reviewed direct-channel exchange as other safe mappings rather than a permanent W/A repair button. Unsigned installer `0.1.0-preview.94.0` is contract-tested from exact source `5c1f1f4b98343d08c82e9201af0fde4d20d1794b` at SHA-256 `69eacbe90be204620b92f0ec9d35120af738f6c729898ad966ad71deb5a095ef`; its payload manifest SHA-256 is `1af2441dd355e641ea8c6d0a21324a45d3c2c5b5c1b0290a4e9e35916f91e74f`. Installed-Windows lifecycle, physical IR-4 White/Amber qualification, cell-aware fixtures, a pinned/distributable offline corpus, exact SoundSwitch controlled-delta decoding, any WOLFMIX parser/import, VirtualDJ, controller/hardware qualification, issue #37 toolkit qualification, eight-hour Windows soak, and live-event qualification remain active gates.
- Preview 95 closes the software half of core-recovery CR-3: the production Runner publishes a bounded immutable snapshot of its actual pre-blackout and routed frames, output-route outcomes, and per-channel fixture/profile/mapping/winning-layer/default/constant/capability/conflict/safety attribution. System Diagnostics renders this evidence and automatically compares an active packaged IR-4 Blackout/R/G/B/W/A bench look with its exact manual reference. The included editable Local bench project begins with every physical output disabled and keeps profile correction in the general channel workbench. This distinguishes engine/profile/route truth from physical response; it does not qualify the Micro, either IR-4, Windows installation, or gig behavior. Unsigned installer `0.1.0-preview.95.0` is contract-tested from exact source `68cc13be7c0b7d9735609b8ca7533be762b90094` at SHA-256 `a099852f1fe5642d55eb69e2cd4f4653fd94e367b73703af14b77160ad5e2dac`; its 20-file payload manifest SHA-256 is `2d35c338e363b535b08e8231d8e11fa685e7510f5b3017f5f51ae2a41665e82e`. Native installed-Windows and physical qualification remain open.
- Preview 96 implements the software CR-4 evidence join. For a recognized active IR-4 bench look, System Diagnostics now binds the exact current routed-frame SHA-256 and SoundSwitch Micro per-frame host-accept result to a cryptographically intact completed Raw Hardware Test attempt embedded in the project. It reports the authored-reference requirement and current routed-frame requirement independently, including the prior operator-observed physical behavior. A profile revision can retain explicitly historical evidence only while fixture/profile identity, manufacturer/model/mode, universe, and address remain stable; a repatch fails closed. The report always states that EmberLights did not observe the fixture's current physical response. Unsigned installer `0.1.0-preview.96.0` is contract-tested from exact source `41b16fb2528cfd8050132bf5be104204b501d50c` at SHA-256 `9d087f27ec5d247c81bfdacab42530a90fc9fc33d5c5eeecadef64dfe0c8341c`; its 20-file payload manifest SHA-256 is `8947ef4c8de352d94c5a27d5adb5c1310f6c48b502fa2fea333e0b6db334e4da`. Installed-Windows and owned-hardware evidence remain open.
- Preview 97 is the durable recovery package for that same Preview 96 product checkpoint after the earlier installer bytes were unavailable. It is rebuilt from clean source `c9421dda3920d75ee70d817652aa1f001601acf3`, which adds only the Preview 96 evidence record after the feature commit. The unsigned installer `0.1.0-preview.97.0` is contract-tested at SHA-256 `70f9c26c667aa206b8cc34d9ed991206e6b41a9e5645e69af20d4f62d34d7ee0`; its exact 20-file payload manifest SHA-256 is `8519e997243fc30b3cd8283700dcf139b78b0eb051a349c9795f32bedf0ff209`. This supersedes the unavailable Preview 96 download, not its feature/evidence scope. Native Windows lifecycle and owned-hardware qualification remain open.
- Default 2.2 replaces six unrelated flat Studio forms with one registry-governed, toolkit-neutral Library + contextual Inspector workbench for Profiles, Patch, Groups, Static Looks, Autoloops, and Track Scripts. Search is bounded and stable-ID-aware; counts/empty states, read-only/draft status, unsaved-selection protection, `Ctrl+F`, `Esc`, responsive panels, and denser Look/Autoloop property layouts are shared behavior rather than a Win32-only repaint. Registry `1.3.0`, generation 2, digest `a3edd0e488a49bbd15b2655be4fb2ee36506dd23a98498874e2bddbb46b790de` adds only bridged `ember.authoringWorkbench`; the 29 commands, 39 states, project schema, Runner, and output path are unchanged. See `41_DEFAULT_2_2_AUTHORING_WORKBENCH_CHECKPOINT.md`.
- Joshua clarified on 2026-08-08 that his fixtures are later validation inputs, not the product scope, and that full SoundSwitch feature parity is the minimum finished-product bar.
- `13_SOUNDSWITCH_PARITY_LEDGER.md` is the binding completeness checklist. Milestone exclusions sequence delivery; they do not remove features from the finished product.
- The modular UI program defines Default, SoundSwitch Reference, Safe, and future user skins over one engine. The Skins Platform V2 program adds a visual Skin Designer, overlays, typed bounded Ember Actions, cross-source import/export, and mandatory registry/code-generation compatibility governance.

## Binding product statement

> A general-purpose, SoundSwitch-first, automation-first DJ/event lighting workstation with a lightweight offline Runner, a capable Studio, open MIDI and DMX choices, first-class migration, user-extensible control surfaces, and no dependency on one user's fixtures or hardware.

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
- EmberLights Default, SoundSwitch Reference, Safe, user skins, keyboard, MIDI/controller profiles, DJ commands, Ember Actions, and future remotes invoke one shared versioned command/state/capability model.
- Skins are validated presentation/binding packages, not alternate engines. User-created skins and overlays are authored visually and imported/exported through bounded contracts.
- Ember Actions are typed bounded compositions over registered commands/state; arbitrary JavaScript/Lua/WASM/native plugins and UI-timer show scheduling are not accepted.
- Every user-visible feature addition/removal/change must keep registries, generated artifacts, bundled skins, actions/mappings/profiles, schemas, compatibility/deprecation data, tests, and migration rules current.

## What is not accepted yet

- Production implementation language and UI toolkit.
- Exact native support for MyDMX Buddy; Control One DMX now has a
  contract-tested Windows Runner transport and explicit experimental opt-in,
  but still needs owned-device physical qualification before a support claim.
- Default live-control takeover/return semantics.
- Exact first-use balance between manual song scripting and AutoScripting.
- Any claim that Control One DMX is physically verified or gig-qualified, or
  that its OLED/storage functions are supported.
- Final expert Ember Action Script grammar, designer-source container, signing/trust UX, and public distribution/marketplace policy; these cannot weaken the accepted typed/sandboxed architecture.

## Required reading order

1. This file.
2. `01_PRODUCT_REQUIREMENTS.md`.
3. `03_ARCHITECTURE.md`.
4. `04_V1_SCOPE_AND_ACCEPTANCE.md`.
5. `08_DECISIONS_AND_OPEN_QUESTIONS.md`.
6. `09_BUILD_AND_TEST_STANDARDS.md`.
7. The smallest route named by `AGENT_BOOTSTRAP_PROMPT.md` for the assigned lane.
8. For any UI/user-visible behavior/skin/mapping/action/designer work, begin with `UI_PROGRAM_START_HERE.md`; issue #63 work additionally begins with `SKINS_PLATFORM_V2_START_HERE.md`.
9. The remaining research/backlog files as needed.

## Definition of progress

Progress is measured by a working, testable replacement path:

- exact inputs captured;
- deterministic state produced;
- correct DMX emitted;
- failures handled safely;
- footprint and latency measured;
- real hardware and VirtualDJ verified;
- user workflow tested;
- user-visible behavior exposed through maintained versioned contracts rather than one-off surfaces;
- skin/action/mapping compatibility validated and migration outcomes explicit.

Lines of code and mock screens are not milestones by themselves.
