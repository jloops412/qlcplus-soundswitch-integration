# V26 Autoplay Clarity provenance

## Reviewed source

V26 is generated from:

```text
IR4-TUBES-CONTROL-ONE-V25-LEAN-FEEDBACK.qxw
SHA-256 2EE9FEDEDA8CEDE6F4D2659C296035B9BF1340952DE8C46E50E401DDEF28BC9B
```

The deterministic builder refuses any other source hash and preserves a protected V25 backup before writing output.

## Workspace delta

V26 changes only Virtual Console presentation:

- keeps the 25 page-specific dwell Buttons required by QLC+ Multipage behavior;
- labels all five visible values on every page as `1M/2M/4M/8M/16M`;
- keeps the selected dwell value green through the native page state;
- expands Frames `1542–1573` from tiny vertical rails to 246×12 read-only strips below their matching pads;
- gives inactive rail segments a quiet dark background so the stock amber Monitoring border is unambiguous;
- retains their 128 raw-Chaser bindings, covering Functions `532–659` exactly once; and
- updates headings to explain that stock QLC+ amber means the raw Chaser is running.

The Engine subtree is byte-identical to V25. Fixtures, Input/Output, 2,090 lighting Functions, public Function IDs, Priority Looks, Autoloops, Control One bindings, and logical channels do not change.

Output:

```text
IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw
SHA-256 ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570
```

## Native Autoplay state

- QLC+ Chasers `788–797` remain the ten sequential/random Bank/All-Banks playback engines.
- Collection owners `798–807` remain the exclusive playback owners.
- Ten clipped Cue Lists retain absolute seek on logical channel `632`.
- The 128 disabled monitor Buttons observe raw-Chaser state and own no Input.
- No timer, polling script, external tracker, or second runtime application is added.

## Dwell timing

Autoplay parent tempo remains `Beats`. The SpeedDial preset values are:

| UI | Beat duration | Four-beat measures |
|---|---:|---:|
| 1M | 4,000 | 1 |
| 2M | 8,000 | 2 |
| 4M | 16,000 | 4 |
| 8M | 32,000 | 8 |
| 16M | 64,000 | 16 |

These are QLC+ beat-counted duration units. They do not mean 4/8/16/32/64 wall-clock seconds.

## OS2L binary delta

The pinned QLC+ source commit is:

```text
a124abebe0b5ad6077727c561a5a0e1f3730810c
```

Its stock OS2L plug-in emits a beat for every received beat message while ignoring the message's `bpm` value. Live packet bursts produced impossible displayed rates. V26's focused source patch is preserved at:

```text
qlcplus/patches/0001-os2l-use-reported-bpm.patch
```

The built `os2l.dll` SHA-256 is:

```text
EF611B26FAC5D090711AF242EF7DA880DBF1E1D59D5F22D36B5FB1918BDF6513
```

The patch was compile-checked as the QLC+ `os2l` target. It does not change the QLC+ executable or introduce another runtime process.

## Validation claims

The deterministic workspace build and regression checks prove:

- Engine identity with V25;
- unique widget IDs and valid Function references;
- exact Autoplay parents, tempo, dwell, seek, and monitor coverage;
- all five visible dwell controls per native page;
- the expected fixture patch; and
- no personal path/username leakage.

The release validator additionally proves package hashes, PowerShell syntax, Control One profile integrity, and that no replacement `qlcplus5.exe` is distributed.

Physical fixture output, live moving-strip observation, stable known-song BPM, reconnect recovery, simultaneous Control One ports, and the two-hour combined workload remain owner/gig qualification steps.
