# Focused QLC+ compatibility patches

These patches are the complete non-SoundSwitch changes carried by the packaged QLC+ modules. They are kept separate from the show workspace and the SoundSwitch hardware plug-in so future QLC+ upgrades can be evaluated cleanly.

## `0001-os2l-use-reported-bpm.patch`

Applies to the pinned QLC+ source commit:

```text
a124abebe0b5ad6077727c561a5a0e1f3730810c
```

The stock OS2L plug-in ignores the `bpm` field in OS2L beat messages and emits a QLC+ beat for every received message. Bursty delivery therefore becomes a false high BPM. The patch:

- reads and bounds reported BPM;
- drives one precise Qt timer at `60000 / bpm` milliseconds;
- treats later messages as tempo/keepalive updates;
- stops timing on disconnect or source silence;
- retains a rate-limited compatibility path for senders without BPM; and
- connects the existing disconnect slot correctly.

Apply from the root of the matching QLC+ source tree:

```powershell
git apply --unidiff-zero path\to\0001-os2l-use-reported-bpm.patch
```

Then rebuild the `os2l` target against the exact QLC+/Qt tuple. The V26 binary was produced with Qt 6.8.1 and is packaged only for QLC+ 5.3.0 GIT a124abe.

No QLC+ core/UI patch is carried. In particular, the stock amber Virtual Console Monitoring state is left unchanged.
