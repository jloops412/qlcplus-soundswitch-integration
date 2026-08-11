# SoundSwitch Reference Deviation Ledger

Purpose: record every deliberate decision to preserve, improve, or reject an observed SoundSwitch UI/workflow pattern.

The ledger protects the project from two opposite failures:

1. blindly inheriting SoundSwitch weaknesses for visual similarity;
2. modernizing so aggressively that migration familiarity and parity testing are lost.

## Decision meanings

- **Preserve** — retain the recognizable mental model or behavior with EmberLights-owned implementation.
- **Improve** — retain the job/workflow but change presentation, hierarchy, feedback, safety, or responsiveness.
- **Reject** — do not reproduce the observed pattern because it conflicts with product architecture, safety, ownership, accessibility, or clarity.

## Ledger

| ID | SoundSwitch screen/pattern | Evidence references | Decision | Reference-skin implementation | Default-skin implementation | Command/state impact | Accessibility/safety impact | Approval |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DEV-001 | Studio/Edit and Performance separation | official docs | Preserve | Studio + Live familiar tabs/workspaces | Studio + Live modern navigation | shared commands/state | keeps Live simple | approved |
| DEV-002 | Fixed Performance layout | screenshots | Improve | familiar initial arrangement, responsive variants | modular pages/panels | no behavior difference | avoids clipping | provisional |
| DEV-003 | Separate project/lightshow save concepts | official troubleshooting | Reject | one project save status plus asset association state | same | project commands/state | reduces data-loss ambiguity | approved |

## Required evidence for approval

A row may be approved from:

- official documented behavior;
- controlled native observation;
- multiple consistent screenshots for visual structure;
- explicit EmberLights architecture/safety requirement.

Exact visual-token deviations require Tier A measurements. Workflow deviations may be approved earlier when official behavior is clear.

## Review checklist

For each row:

- Does this preserve the user's ability to migrate from SoundSwitch?
- Does it preserve a SoundSwitch parity test path?
- Is the same domain function available in the Default skin?
- Is the behavior implemented once through commands/state?
- Does the decision preserve Runner footprint and determinism?
- Are original EmberLights assets used?
- Is the decision accessible without color alone?
- Does it improve or preserve gig safety?
