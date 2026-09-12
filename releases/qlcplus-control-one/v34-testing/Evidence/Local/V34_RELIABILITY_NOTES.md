# V34 reliability evidence and operating changes

The immutable V34 workspace is SHA-256
`781909bd7081e4e058f09e726af2f5a9eee259b5808fbd60cdb3d4ce2fe5f702`.
The candidate source was published as `87d5aea4be9cc39daa0dced2237450dd7701172a`.
The local native recorder ran before that Git commit was created and therefore
records the exact working-file hashes. Those hashes bind the tested files; the
separate GitHub native and Windows checks run on the published commit.

Actual pinned-engine tests exposed both kinds of STOP failure: native StopAll
leaves Flash sources active, and an immediate button acknowledgement can precede
a queued Function start. V34 uses native running feedback as the acknowledgement,
releases Flash sources, then calls StopAll. The command Scene is deliberately
empty and self-terminating. A persistent marker Scene was tested and rejected
because it could remain running. Physically held Shift remains a modifier after
STOP, preventing a subsequent Shift+Play press from becoming ordinary Play.

WHITE/BLACK/UV hold and latch Scenes have independent ownership. BLACK uses a
final emitter-intensity gate. Separate native color hold/latch universes preserve
per-fixture/cell digital peaks and dark cells while retaining authored spatial
palettes. This is not a claim of equal perceived brightness across different
colors. Native Grand Master must retain Reduce / Intensity mode.

The tiny raw-loop/color/position status strips are inert Labels in V34. The
pinned QML loader does not restore the saved frame Disabled property, so the old
monitor Buttons could be clicked to start Functions. Their live borders are
intentionally removed. Enabled offscreen observers carry native raw-loop feedback
to Control One LEDs independently of those separators. Main controls remain.

U1 physical output is intentionally unassigned until the operator selects the
connected device in QLC+ and saves a working show. Old numeric fallback could
select private outputs when USB enumeration changed. U2 is controls only; U3-U6
remain internal and must never feed physical DMX, Art-Net or sACN.

Fourteen loops received targeted palette, sweep-endpoint and pulse corrections
(144 Scene payloads). All128 loop IDs,32 Priority Looks and138 playback Collections
are preserved. Focus discrete wheel/gobo/prism/shutter/function channels snap;
pan/tilt/focus/dimmer channels remain fadeable. Physical wheel travel, aim,
appearance, USB/MIDI behavior and full DJ/audio/OS2L rehearsal require rig tests.

This package is a testing candidate, not full SoundSwitch parity or gig approval.
Read the exact scope in the native records and original Windows build evidence.
