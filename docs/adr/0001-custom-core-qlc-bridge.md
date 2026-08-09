# ADR 0001: Original Core with Optional QLC+ Bridge

- Status: Accepted
- Date: 2026-08-08

## Context

QLC+ offers broad device/protocol support and mature console behavior, but its current engine is a large Qt-linked general lighting-console model. The product requires track transport, portable semantic scripts, event moments, sparse property layers, and a very small Runner.

## Decision

Build and own the DJ/event domain core, package format, automation, MIDI semantics, and UI. Implement small open standards directly. Use QLC+ in three bounded ways:

1. reference and interoperability testing;
2. optional Art-Net-to-QLC+ hardware bridge;
3. selective adaptation of isolated Apache-2.0 components after dependency/license review.

QLC+ and Qt are not mandatory Runner dependencies.

## Consequences

- The product remains uniquely ours and domain-correct.
- We can support obscure hardware sooner through a higher-footprint optional bridge.
- Native drivers still require deliberate work.
- Any copied/adapted QLC+ code requires attribution, notice preservation, and modified-file markers.
