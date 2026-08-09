# Research Spikes

Each spike must end with captured evidence, a reproducible test, and a decision. A demo without a recorded result is not complete.

| ID | Question | Method | Exit artifact |
| --- | --- | --- | --- |
| R-001 | What exact OS2L data does current VirtualDJ expose reliably? | Capture same-PC and LAN sessions across play, pause, pitch, cue jumps, loops, reverse, scratch, slip, crossfader, and four-deck changes. | Packet corpus, transport matrix, adapter tests. |
| R-002 | Can extended VDJScript subscriptions replace a native plugin? | Subscribe to exact deck/mixer expressions, measure cadence/jitter, disconnect/reconnect, and version behavior. | Verified capability matrix and go/no-go ADR. |
| R-003 | What MIDI does Control One expose? | Record every button, pad, fader, encoder, touch strip, modifier, and output response. | Machine-readable profile plus annotated control map. |
| R-004 | Which Control One LEDs/rings accept standard MIDI feedback? | Send safe note/CC values to the exposed MIDI output and document responses. Do not probe proprietary USB interfaces in this spike. | Feedback matrix. |
| R-005 | What are the owned USB-DMX devices exactly? | Capture Windows Device Manager names, VID/PID, interfaces, drivers, and photographs/model labels for Control One, MyDMX Buddy, and SoundSwitch USB. | Hardware inventory and driver boundary. |
| R-006 | Is QLC+ a viable temporary hardware bridge? | Send Art-Net U1/U2 to QLC+ locally, drive each supported adapter, measure memory/CPU/jitter/startup/recovery. | Bridge recipe, benchmark, compatibility table. |
| R-007 | Which QLC+ components merit selective reuse? | Compare isolated source/dependencies against simpler standard implementations; complete license/NOTICE inventory. | Accept/reject list per component. |
| R-008 | What is recoverable from `.ssproj`? | Inspect copies of user exports, packaged lighting files, and scripted audio metadata without modifying originals. | Format map, fixture corpus, migration report prototype. |
| R-009 | Can OFL profiles compile into our semantic schema? | Convert representative RGBWAUV, CMY, moving-head, multi-cell, color-wheel, strobe, and fog fixtures. | Conversion tests and unsupported-capability report. |
| R-010 | Which live-audio BPM implementation meets fallback needs? | Benchmark WASAPI loopback/direct input on speech, silence, mixed genres, tempo changes, and crowd noise. | Accuracy/latency/CPU dataset and selected algorithm. |
| R-011 | Rust or C++ for production Runner? | Implement the same core contracts, compile for Windows, compare release binary, RSS, tick latency, driver integration, sanitizers, and developer velocity. | Technology ADR with measurements. |
| R-012 | Which UI toolkit stays within Runner budgets? | Build the same representative performance screen in candidate native toolkits and measure idle/active CPU, memory, startup, and input latency. | Toolkit ADR. |

## Required user-provided fixtures

When convenient:

- exact DJ laptop model, CPU, RAM, GPU, and Windows version;
- one lower-end Windows PC available for testing;
- copied `.ssproj` export, preferably with packaged lighting files;
- copies of one or two scripted songs, never the only originals;
- Device Manager/USB inventory for each lighting interface;
- Control One connected without SoundSwitch running for MIDI capture.
