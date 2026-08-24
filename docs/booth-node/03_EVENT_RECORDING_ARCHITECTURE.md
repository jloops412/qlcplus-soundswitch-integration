# LLE Booth Node — Highest-Quality Event Recording Architecture

Status: design accepted for bench validation; no recording path is yet qualified for live events.

## Goal

Capture the event mix at the highest practical quality while preserving:

- the actual output of the REV7 mixer;
- music, microphones, and intended analog sources;
- headroom with no clipping;
- continuity if venue Internet fails;
- independence from QLC+/lighting;
- at least two verified copies after the event;
- a simple recovery story.

The design must not make the DJ mix depend on a network audio transport, a cloud service, or an experimental real-time pipeline.

## Controlling decision

### Primary capture

Record **locally on the DJ laptop** through VirtualDJ using the DDJ-REV7's dedicated digital recording return:

```text
REV7 mixer
  -> USB 5/6 MIX(REC OUT)
  -> VirtualDJ record input
  -> 24-bit FLAC or WAV
  -> DJ-laptop local SSD
```

### Secondary copy

After VirtualDJ closes/finalizes the file:

```text
completed local recording
  -> wired booth LAN
  -> booth-node recording share
  -> SHA-256 comparison
  -> JSON verification manifest
```

The network copy is bit-for-bit replication, not a transcoding step. It cannot lower audio quality.

### Why this wins over direct network recording

VirtualDJ can choose a recording path, so a network share may be technically selectable. It is not the production default because a cable pull, router reboot, SMB pause, credential problem, or booth-node failure could interrupt the only recording. Writing the primary file to the DJ laptop's local SSD keeps capture independent of the ReadyNet and booth node.

Likewise, sending live PCM through VBAN, SonoBus, NDI, or another network-audio layer can preserve excellent quality, but it introduces routing software, buffers, clock behavior, and another failure path. That is an optional later experiment, not the archival baseline.

## Source configuration: DDJ-REV7

In the Pioneer/AlphaTheta DDJ-REV7 Setting Utility:

```text
USB 5/6 Mixer Output = MIX(REC OUT)
```

VirtualDJ's REV7 integration provides a preconfigured record input for this return. The returned mix is intended to include sources routed through the mixer, including analog CH1/CH2 sources. The exact microphone and auxiliary routing used in the LLE rig must still be verified with a bench recording.

The Setting Utility's default USB recording-output level is `-19 dB`, deliberately leaving substantial headroom. Do not raise it merely because the waveform appears visually small. First record the loudest realistic combination:

- two music decks at normal operating gain;
- DJ microphone at normal and intentionally loud speech level;
- ceremony/speech input if routed through the mixer;
- sampler/effects used in real work;
- master level at the normal show position.

Then inspect true peaks. The acceptance criterion is no clipping under the worst credible event condition. A conservative peak range can be normalized later without quality loss in a 24-bit file.

## VirtualDJ recording settings

Recommended production starting point:

| Setting | Value | Reason |
|---|---|---|
| `recordFormat` | `flac` | Lossless PCM quality with smaller files than WAV |
| `recordBitDepth` | `24` | More capture headroom; supported for FLAC/WAV |
| `recordFile` | local SSD folder | Recording survives LAN/booth failure |
| `recordAutoStart` | off initially | Manual pre-event confirmation is clearer |
| `recordWaitForSound` | off | Do not miss pre-music speech/ambient program audio |
| `recordPauseOnSilence` | off | Preserve real event timing and quiet sections |
| `recordAutoSplit` | off | Do not split every crossfader move |
| `recordMicrophone` | on | Preserve software-routed microphone where applicable |

Use WAV instead of FLAC only when a downstream tool specifically requires WAV. At the same bit depth and sample rate, FLAC is lossless and does not reduce audio quality.

Do not change VirtualDJ's working sample rate solely to make a “bigger number.” Keep the stable rate that matches the soundcard and the majority of the actual source material. VirtualDJ specifically notes that a larger sample rate does not automatically mean better quality and recommends matching source, internal processing, and hardware where possible.

## File location and naming

Create a dedicated local folder on the DJ laptop, preferably on its internal SSD:

```text
D:\LLE Event Recordings\Incoming
```

Use the actual fast local volume available on the DJ laptop. Do not use a removable thumb drive as the only recording destination.

Recommended name:

```text
YYYY-MM-DD__event-name__master__rev7__24bit.flac
```

Example:

```text
2026-09-12__smith-jones-wedding__master__rev7__24bit.flac
```

Avoid client-sensitive details beyond what is operationally necessary. The post-event manifest can hold internal event identifiers.

## Storage planning

Uncompressed 24-bit/48-kHz stereo PCM is approximately:

```text
48,000 samples/sec × 24 bits × 2 channels = 2.304 Mbps
```

That is about:

- `17.28 MB/minute`;
- `1.04 GB/hour`;
- `6.22 GB` for six hours.

WAV will be close to those sizes. FLAC is smaller, with the exact ratio depending on the program audio.

Before an event, require enough local free space for at least twice the expected uncompressed recording plus normal Windows/VirtualDJ operating margin. A practical initial gate is **25 GB free** on the recording volume for a typical wedding-length capture; increase it for video or multichannel work.

## Event-day recording procedure

### Before doors/program start

1. Confirm `MIX(REC OUT)` in the REV7 utility.
2. Confirm VirtualDJ shows the dedicated record input.
3. Confirm 24-bit FLAC and the intended local path.
4. Verify at least the required free space.
5. Record a 60-second test containing music and microphone.
6. Stop, play it back through headphones, and verify both channels plus microphone.
7. Rename/delete only the test file, not prior event recordings.
8. Start the actual recording before the first program audio that matters.
9. Confirm the recording timer/file-size indicator advances.

### During the event

- Do not change format, path, or record-input routing.
- Do not browse/open the growing file with another application.
- Do not start the network-copy script against the open file.
- Check the recording indicator at major timeline transitions.
- Keep master/mic gain managed for the room; do not chase recording loudness during a live moment.

### After the event

1. Stop VirtualDJ recording normally.
2. Wait for the file to finish closing/finalizing.
3. Confirm the file exists and has a plausible duration/size.
4. Play the beginning, a microphone section, and the end.
5. Run `Copy-LLERecording.ps1` to the booth-node destination.
6. Preserve the source until the SHA-256-verified copy and manifest pass.
7. Do not delete either copy at teardown.
8. Sync to the long-term private archive later under the separate retention policy.

## Prepared verification copy

From the DJ laptop:

```powershell
Set-ExecutionPolicy -Scope Process Bypass

.\tools\booth-node\Copy-LLERecording.ps1 `
  -SourceFile 'D:\LLE Event Recordings\Incoming\2026-09-12__smith-jones-wedding__master__rev7__24bit.flac' `
  -DestinationRoot '\\10.52.0.20\LLE-Recordings' `
  -EventId '2026-09-12__smith-jones-wedding'
```

The script:

- refuses a missing or empty source;
- waits until the file can be opened for exclusive read, indicating the recorder has released it;
- copies without transcoding;
- preserves normal file metadata;
- computes source and destination SHA-256 hashes;
- fails if the hashes differ;
- writes a verification manifest beside the destination;
- never deletes the source.

The SMB share and a dedicated least-privilege recording-sync credential will be configured during the recording milestone. Port 445 should be restricted to the DJ and booth hosts, not the whole guest network.

## Independent redundant recorder experiment

The REV7 has two independent USB computer ports. This creates a promising later experiment:

```text
REV7 USB A -> DJ laptop / VirtualDJ / primary capture
REV7 USB B -> booth node / dedicated recorder / secondary capture
```

This could produce a second, independent digital `MIX(REC OUT)` recording without analog conversion and without relying on the ReadyNet for transport.

It is **not yet an accepted capability**. Before use, physically prove:

1. the booth node can see the intended REV7 recording channels through USB B;
2. USB A and B can expose the mix return simultaneously in the actual Windows driver;
3. the second recorder can write 24-bit PCM/FLAC/WAV;
4. using USB B does not disturb VirtualDJ/controller behavior on USB A;
5. the recorder survives a two-hour combined lighting/OS2L/MIDI/DMX workload;
6. unplugging/restarting the booth-side recorder cannot affect the DJ-laptop audio path;
7. both recordings align closely enough for recovery or post-production use.

Select the booth recording application only after inspecting the booth OS, audio endpoint/driver modes, and available storage. Do not install a large production suite merely to prove the concept.

## Network-audio experiment backlog

A live lossless network stream remains possible. Candidate classes include:

- native PCM over VBAN;
- uncompressed PCM through SonoBus;
- NDI audio;
- a dedicated Dante/AES67-style hardware/software path later.

Any network-streaming test must remain a **secondary recorder only** until it proves:

- stable capture at 24-bit quality;
- no clock drift or dropouts over a full event duration;
- automatic reconnection behavior;
- no measurable VirtualDJ audio impact;
- no interference with OS2L/QLC web control;
- a recoverable file if the stream drops.

The ReadyNet's 100-Mbps Ethernet capacity is ample for stereo PCM, but bandwidth alone does not make a network recorder safer than direct digital USB capture.

## Post-processing

Preserve the untouched master as the source of record. Make derivative copies for:

- loudness normalization;
- noise reduction;
- speech extraction;
- track/event markers;
- audiograms;
- client delivery.

Do not destructively process or overwrite the original. The later Ampersand Audiogram/audio-cleanup pipeline may consume a verified derivative while the archival master remains immutable.

## Privacy and retention gate

Before production recording becomes standard, define:

- which event types are recorded;
- client/venue notice and consent workflow;
- whether guests' private conversations could be captured;
- storage encryption/access controls;
- retention/deletion timing;
- what is delivered to clients versus retained operationally;
- incident handling if a device is lost.

This policy is part of promotion to production, not an optional afterthought.

## Official references

- VirtualDJ recording formats and operation: https://virtualdj.com/manuals/virtualdj/settings/record.html
- VirtualDJ recording and sample-rate options: https://virtualdj.com/manuals/virtualdj/appendix/optionslist.html
- VirtualDJ DDJ-REV7 recording setup: https://virtualdj.com/manuals/hardware/pioneer/ddjrev7/advanced.html