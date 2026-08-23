# QLC+ SoundSwitch Hardware Integration

This directory records the reusable engineering findings from integrating SoundSwitch Micro and Control One hardware directly into QLC+.

The operating model is intentionally small:

```text
DJ software -- OS2L --> QLC+
Control One -- MIDI --> QLC+
QLC+ -- native plug-in --> Micro or Control One DMX
```

Only QLC+ runs during a show. The former EmberLights standalone application is archived as reference material; its proven USB transport work may be reused, but its lighting engine and UI are not part of this integration.

## Boundaries

- QLC+ remains responsible for fixtures, Scenes, Chasers, beat timing, Virtual Console, OS2L, MIDI mapping, persistence, and output merging.
- The custom plug-in is limited to SoundSwitch USB transport, Control One MIDI translation, hot-plug recovery, and hardware feedback.
- Show-specific fixtures, addresses, scene content, controller preferences, machine paths, serial numbers, and installation receipts are deliberately excluded.
- SoundSwitch firmware and OLED reverse engineering are outside the current scope.

## Documents

- [CONTROL_ONE_WORKFLOW_SPEC.md](CONTROL_ONE_WORKFLOW_SPEC.md) defines the intended performer-facing behavior.
- [STATE_MODEL_AND_ARCHITECTURE.md](STATE_MODEL_AND_ARCHITECTURE.md) records the implementation model and the failure modes found during testing.
- [VALIDATION_AND_MAINTENANCE.md](VALIDATION_AND_MAINTENANCE.md) defines regression, hot-plug, and upgrade checks.

## Primary references

- [SoundSwitch Control One Quick Start Guide](https://cdn.inmusicbrands.com/soundswitch/files/User%20Guide%20Control%20One.pdf)
- [SoundSwitch Autoloops Explained](https://support.soundswitch.com/en/support/solutions/articles/69000847100-soundswitch-autoloops-explained)
- [SoundSwitch 2.9 Autoloop Improvements](https://support.soundswitch.com/en/support/solutions/articles/69000858487-soundswitch-autoloop-improvements-in-soundswitch-2-9)
- [SoundSwitch Static Looks Explained](https://support.soundswitch.com/en/support/solutions/articles/69000863339-soundswitch-static-looks-explained)


