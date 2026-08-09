# Runner Laboratory Build

## Purpose

`runner_lab` is the first end-to-end executable that joins the native components in the production direction:

1. VirtualDJ or another sender connects by OS2L TCP.
2. The adapter decodes bounded messages and survives reconnects.
3. Beat events enter a fixed-capacity single-producer/single-consumer queue.
4. Blackout uses a separate atomic emergency state so a full beat queue cannot discard it.
5. A dedicated scheduling thread resolves the clock, advances an Autoloop, renders two DMX universes, and optionally sends Art-Net.
6. The main thread reports connection, synchronization, queue, output, and jitter counters.

This is a transport/output laboratory tool, not yet the user-configurable or gig-qualified Runner. Its patch is intentionally two generic five-channel RGB fixtures at Universe 1/Address 1 and Universe 2/Address 1. Do not connect it to an unknown physical patch.

## Safe first run

From `native-core`:

```bash
make runner_lab
./build/runner_lab --manual-bpm 120
```

Dry-run is the default. It renders frames but sends no DMX network packets.

The default OS2L listener is `127.0.0.1:9996`. This is suitable when VirtualDJ and the lab Runner share a computer. The listener remains available after a client disconnects.

## Art-Net receiver test

Only after confirming the receiver/visualizer patch:

```bash
./build/runner_lab --manual-bpm 120 --artnet 192.168.1.50
```

The reference frames use Art-Net port-addresses 0 and 1. Change the first address with `--artnet-base`.

## Separate lighting computer

Bind to the lighting computer's private wired-LAN IPv4 address:

```bash
./build/runner_lab --os2l-bind 192.168.1.20 --artnet 192.168.1.50
```

Configure VirtualDJ `os2lDirectIp` for that address and port. Never expose the unauthenticated OS2L listener to a public or untrusted network.

## Test controls

- `--duration SECONDS` stops automatically for smoke/rehearsal tests.
- `--fps HZ` accepts 10 through 60; V1 normally uses 40.
- `--manual-bpm BPM` provides the manual fallback clock.
- An OS2L button named `blackout` applies or releases the emergency intensity-zero layer immediately through a non-droppable state path.

## Current evidence

- Reconnect lifecycle test: connect, decode, partial message, disconnect, reconnect, decode cleanly.
- End-to-end dry-run: 10 OS2L messages at 126 BPM, 82 scheduled frames, zero decode errors, zero queue drops.
- Art-Net sender: exact packet received and compared over a real loopback UDP socket.
- One-second finite-duration Runner smoke test passes at 40 Hz.

## Remaining qualification

- Build and test on Windows.
- Connect actual VirtualDJ rather than the loopback client.
- Verify an Art-Net node/visualizer and the QLC+ bridge.
- Replace the reference patch with a validated compiled show package.
- Compile and exercise the implemented WinMM enumeration/input/output adapter and `midi_capture` utility, then build the Control One profile from owned-device captures.
- Complete output reconnect policy, event logging, timing histograms, and eight-hour soak tests.
