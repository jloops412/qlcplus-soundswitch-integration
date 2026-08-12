# Raw Hardware Test Workflow v1

Status: implemented and software-tested; no owned hardware observation has been
performed.

## Safety boundary

The workflow qualifies exactly one named physical fixture unit through one
enabled output backend. It cannot receive a `CompiledShow`, `Runner`, Look,
Autoloop, layer stack, or full project frame. The session copies an immutable
bounded plan, generates each 512-slot DMX frame internally, and gives its
transport only:

1. an all-zero blackout frame; or
2. a one-hot frame whose sole nonzero channel is inside the bound fixture's
   exact address and footprint.

The current production transport adapter reuses the hardened SoundSwitch Micro
session and refuses non-`soundswitch-micro:uN` bindings. The abstract transport
boundary permits separately qualified Art-Net, sACN, DMX USB Pro, or Control One
adapters later without weakening the plan or state machine.

Opening this workflow does not open Live, compile a show, or activate any other
patched fixture.

## Plan construction

Before output can open, the caller supplies:

- the active input project SHA-256;
- the current candidate project;
- fixture ID and independent physical unit/serial label;
- enabled output backend;
- exact migration or invalidation markers proposed for supersession;
- bounded observation and whole-session timeouts;
- blackout-frame repetition count; and
- one explicit reviewed criterion for every one-based fixture slot.

Each slot criterion includes a nonzero raw value, expected observed behavior,
and mandatory no-spill condition. The builder refuses missing, duplicate, or
out-of-range slots. It does not infer truth for RGBWAUV, intensity, strobe,
macro/program, neutral/default, fine, or multi-cell boundary channels from an
imported semantic property name. Operators must define and observe those
criteria against the exact manual and physical unit.

The resulting immutable plan binds input and candidate project hashes, fixture
and unit identity, manufacturer/model/mode, profile/revision/behavior
fingerprint, universe/address/footprint, backend, safety policy, marker strings,
and the SHA-256 of every internally generated raw frame.

## State machine

For every requirement, including the blackout requirement, the single-use
session performs this sequence:

1. verify the transport is still connected;
2. send the configured bounded blackout-before sequence;
3. send exactly one internally generated stimulus frame;
4. wait for one bounded operator observation; and
5. send the configured bounded blackout-after sequence.

An accepted observation records actual behavior, pass/no-spill decisions, raw
frame digest, blackout results, timeout, device-loss, and failure fields. Only
then may the session advance to the next slot. After the last successful
observation it closes the transport.

Open failure, write failure, rejected behavior, spill, timeout, cancellation,
or device loss terminates the run. Every termination after an open attempts an
additional bounded blackout and closes the transport. Attempted and accepted
frame counters remain distinct. A failed blackout is recorded as a failure;
later accepted retries do not retroactively claim it succeeded.

Destroying an active session also attempts bounded blackout and close. A
session cannot be restarted or reused for a second fixture.

## Audit and graduation

Every terminal session can produce a content-addressed attempt record. Failed,
cancelled, and interrupted records retain partial observations and terminal
failure evidence but cannot authorize physical output.

```text
RAW_HARDWARE_TEST_ATTEMPT\t1\t<content-sha256>\t<hex-canonical-payload>
```

The canonical payload contains schema version, start/completion UTC timestamps,
terminal phase/error/message, attempted/accepted frame counters, and a nested
content-addressed fixture-attestation payload. The record is append-only and is
excluded from the project-basis digest alongside the other qualification
artifacts, preventing audit append from invalidating its own evidence.

A completed attempt graduates transactionally: validate the attempt against
the current project, append the attempt audit, append the immutable successful
attestation, append supersessions for only the exact named markers, then replace
the project. Any stale project/profile/binding/marker, malformed record, replay,
or attestation failure leaves the project unchanged.

Failed attempt audits do not alter the qualification gate. Physical output
remains blocked until complete current evidence exists for every affected
fixture unit/backend/marker tuple.

## Software verification

The native test suite proves:

- deterministic complete plan coverage and frame digests;
- every emitted test frame is blackout or one-hot inside the selected fixture;
- no full-patch or neighboring channel can be emitted by the workflow;
- successful ordered observations and transactional graduation;
- independent unit-label binding;
- malformed criteria, markers, configurations, plans, and audit envelopes;
- open, stimulus-write, blackout-before, and blackout-after failures;
- timeout, device loss, operator rejection/no-spill failure, and cancellation;
- shutdown-blackout attempts and close on every terminal fault;
- partial failed-attempt audit persistence without gate authorization;
- content tamper, replay, cross-project reuse, and profile/project staleness.

These are software-contract results only. SoundSwitch Micro device discovery,
installed Windows behavior, real fixture functions, blackout visibility,
neighbor/multi-cell spill, repeated open, and unplug/replug still require
operator evidence on the owned bench before any physical or gig-readiness claim.
