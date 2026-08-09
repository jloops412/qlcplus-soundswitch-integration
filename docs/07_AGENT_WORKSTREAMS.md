# Workstreams

These are bounded ownership areas for future contributors or agents. They are not permission to implement conflicting architectures in parallel.

## Core semantics

Owns property types, layers, merge rules, groups, Autoloops, scripts, safety policies, fixtures, patch validation, and deterministic replay.

Must not own device I/O or UI.

## DJ transport

Owns OS2L, VirtualDJ capability probes, normalized deck/mixer state, track identity, clock prediction, loops/seeks/reverse, and adapter versioning.

Must emit versioned normalized events rather than mutate fixture output directly.

## MIDI/controllers

Owns device enumeration, Learn, mappings, modifiers, takeover, feedback, and bundled profiles including Control One.

Must remain device-agnostic; no core action may require Control One.

## DMX/hardware

Owns Art-Net, sACN, USB adapters, QLC+ bridge, frame cadence, reconnect behavior, device diagnostics, and protocol compliance.

Must consume completed universe frames; it does not decide lighting intent.

## Studio and persistence

Owns project schema, migrations, atomic saves, fixture import, library/timeline tools, package compiler, preview, and recovery utilities.

Must compile a self-contained Runner package and preserve source/unknown migration data.

## Audio analysis

Owns WASAPI capture, BPM/onset/phase/confidence/silence/energy analysis, and fallback worker lifecycle.

Must not run a heavyweight model continuously or claim exact track scripting from audio alone.

## Quality and performance

Owns replay corpora, protocol fixtures, sanitizer/static analysis, fault injection, soak tests, benchmark machines, budgets, and regression reports.

Can block a release that breaches a safety or release ceiling.

## UX/product

Owns SoundSwitch-first workflow mapping, Runner ergonomics, Studio information architecture, accessibility, event modes, onboarding, and user testing.

Must not let visual polish redefine the core model or delay gig-safety gates.

## Coordination contract

- Each workstream changes public contracts through an accepted decision record.
- Test fixtures and packet/hardware captures are shared assets with provenance.
- Adapter failures are isolated and reported through normalized diagnostics.
- No workstream may make internet access, cloud state, proprietary Control One behavior, or QLC+ mandatory for Runner.
