# QLC+ Integration Files

`plugins/soundswitch/` is the current QLC+ plug-in delta, including Micro/Control One USB transport, Control One MIDI/feedback, reconnect logic, Priority Look selection, and smoke tests.

`input-profiles/SoundSwitch-Control-One-Performance.qxi` names the logical channels exposed by the translator.

The source branch began from QLC+ 5.2.2 commits `f2dec053d` and `e27b4abb1`, then accumulated the Control One MIDI/control work in the working tree. The current DJ-PC profile was saved by QLC+ `5.3.0 GIT a124abe`. Rebuild against the exact target QLC+/Qt toolchain before distributing a new DLL; do not assume plug-in ABI compatibility across builds.

The original upstream remote was `https://github.com/mcallegari/qlcplus.git`. This directory is a focused source snapshot, not a complete QLC+ source tree.
