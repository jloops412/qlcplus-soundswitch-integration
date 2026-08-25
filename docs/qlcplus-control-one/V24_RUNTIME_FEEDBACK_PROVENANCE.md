# V24 Runtime Feedback Provenance

## Inputs

- Source workspace: V23 Live Console, SHA-256 `E953C3483EB09D2E600D32495887B27A03021DC47EF7BA5797552C4F5A21547B`.
- Pinned QLC+ source: `a124abebe0b5ad6077727c561a5a0e1f3730810c`.
- Workspace builder: `qlcplus/workspace-tools/Build-V24RuntimeFeedback.ps1`.
- Plug-in source: `qlcplus/plugins/soundswitch/`.

## Workspace delta

The builder makes two bounded changes:

1. Remove the duplicate `soundswitch:priority-layer` feedback declaration from Universe 2, leaving one `soundswitch:controlone:surface` feedback patch.
2. Replace V23 monitor frame `1413` with 32 visible disabled frames `1542–1573`. Each frame contains the four existing raw-Chaser monitor Buttons for one physical pad.

The builder asserts that all 2,090 Engine Functions remain byte-equivalent, every existing Function/Input widget binding remains unchanged, the fixture patch remains unchanged, exactly one channel `811` mode source remains, and the 128 raw-Chaser monitors still cover Functions `532–659` exactly once.

Output workspace SHA-256: `DAA76DAEB2CD8BA0C964C8A82B283A1FE9640E6A9E0B6180BD9E802A77632ACF`.

## Plug-in delta

- Surface Feedback is advertised and opens without requiring an attached Control One MIDI-output handle.
- Priority ownership channels `600–631` are accepted on the unified Surface line and remain compatible with the older Priority feedback binding.
- Empty Scene UI commands act only on their positive edge.
- The smoke test now proves both mode directions, ignores trailing zero, selects Bank 3, and selects the 2× chase-speed preset.

Packaged plug-in SHA-256: `2DC776DD97A322D64E3923D22CBCF39A53E4DC6121B56EDCAF815A4A49F470AC`.

## Runtime evidence

The V24 workspace was copied to an isolated test variant with:

- internal 120 BPM timing;
- no OS2L input;
- no physical DMX outputs; and
- only the hardware-independent logical Control One input/Surface feedback route.

The runtime test observed:

- Bank 2 changed the native Autoloop bank page from 0 to 1;
- the mode button changed Autoloops → Priority Looks → Autoloops;
- the speed control changed the visible preset page;
- Start Bank remained active while its parent advanced from loop 1 to loop 2;
- Start All remained active while its parent advanced; and
- the monitor corresponding to the current parent step entered QLC+'s Monitoring state.

No synthetic OS2L beat data was sent during the final test. Fixture output was deliberately disconnected; physical-output confirmation remains an owner test.

## Claim

V24 is structurally validated and software-tested against the pinned QLC+ build. It inherits earlier physical evidence for Micro, Control One DMX 1, Control One DMX 2 independently, Control One MIDI/LEDs, OS2L, and the Priority Look workflow. It is not yet gig-qualified.
