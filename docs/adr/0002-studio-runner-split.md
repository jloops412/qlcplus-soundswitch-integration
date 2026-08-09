# ADR 0002: Studio/Runner Operational Split

- Status: Accepted
- Date: 2026-08-08

## Decision

Author and analyze in Studio; compile an immutable package; perform it in a separate lean Runner mode. The normal gig launches Runner only. Audio analysis is on-demand, and unstable hardware may use an isolated adapter process.

## Consequences

- Waveforms, AI, fixture libraries, and migrations do not consume gig resources.
- Packages require explicit compilation/versioning.
- Studio and Runner contracts must be versioned and backwards compatible within defined support windows.
