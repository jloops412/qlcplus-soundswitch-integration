# ADR 0003: Two Universes in V1

- Status: Accepted
- Date: 2026-08-08

## Decision

Expose and validate exactly two universes (1,024 slots) in V1. Use identifiers/schema shapes that can expand after V1 without rewriting song or fixture semantics.

## Consequences

- Smaller hardware matrix and simpler gig qualification.
- No early enterprise universe-routing work.
- Post-V1 expansion remains a schema/runtime evolution, not a content-model redesign.
