# ADR 0004: Device-Agnostic MIDI

- Status: Accepted
- Date: 2026-08-08

## Decision

MIDI Learn, transformations, behavior, layers, feedback, and multiple devices are core platform services. Control One is the first bundled profile, not a hard-coded control path.

## Consequences

- Other DJs can use any MIDI controller.
- Controller profiles are versioned data.
- Proprietary Control One DMX/OLED/storage cannot block V1.
