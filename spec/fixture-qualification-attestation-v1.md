# Fixture Qualification Attestation v1

Status: implemented software contract; physical qualification is still pending.

This contract provides the trusted, append-only transition from an imported or
profile-upgraded project that carries `MIGRATED_PATCH_UNVERIFIED` or
`QUALIFICATION_INVALIDATED` to a project whose exact fixture units may use an
enabled physical-output backend. Attestation validation itself does not emit
output; the paired bounded workflow is specified in
`raw-hardware-test-workflow-v1.md`. Neither contract bypasses normal output
safety or claims that any device has been physically tested.

## Trust and scope

V1 uses content-addressed integrity rather than a cryptographic operator
signature. The operator ID and UTC timestamp are assertions stored inside the
hashed payload. Changing any payload field changes the digest and invalidates
the embedded record.

An attestation is bound to all of the following:

- the serialized project basis, excluding only qualification attestations,
  Raw Hardware Test attempt audits, supersessions, and revocations;
- one fixture ID and one physical unit label;
- manufacturer, model, mode, profile ID, profile source revision, and compiled
  behavior fingerprint;
- universe and address;
- one enabled output backend for that fixture universe;
- the complete safety-policy digest;
- the exact migration/invalidation marker or markers being superseded.

Consequently, normal edits to the project, patch, profile, connection settings,
safety policy, or qualification marker make prior evidence stale. Qualification
artifacts themselves are excluded from the project-basis digest so append-only
graduation, revocation, and requalification do not create a circular hash.

## Required raw observations

Evidence is bounded to at most 513 requirements. A complete fixture attestation
contains exactly:

1. one all-zero blackout frame; and
2. one one-hot frame for every DMX slot in the bound fixture footprint.

Each requirement stores a unique ID, kind, absolute DMX channel, non-zero value
for one-hot tests, expected behavior, required no-spill flag, and SHA-256 of the
complete 512-slot raw frame. The implementation recomputes the frame digest from
the kind/channel/value tuple. A one-hot channel outside the bound fixture
footprint is rejected.

Each requirement has exactly one observation containing the same raw-frame
digest, observed behavior, pass/no-spill results, blackout-before and
blackout-after results, timeout state, device-loss state, and failure text. A
graduating observation must pass, must report no spill, must have successful
bounded blackout before and after, must not time out or lose the device, and
must have no failure text.

Testing every footprint slot is deliberate. It covers color emitters,
intensity, strobe, macros/default channels, and multicell boundaries without
assuming that an imported semantic label is already correct. The operator's
observed behavior remains part of the immutable evidence.

## Canonical payload

Every value is written in the following length-prefixed form, concatenated
without separators:

```text
<decimal byte length>:<value bytes>
```

Integers use unsigned decimal text and booleans use `0` or `1`. Arrays start
with their element count. Fields appear in this order:

1. schema version;
2. active input project SHA-256;
3. candidate project SHA-256;
4. candidate binding SHA-256;
5. operator ID and observation UTC timestamp;
6. binding fields in the order listed in the trust section;
7. requirement count and requirements;
8. observation count and observations;
9. marker count and exact marker strings.

The content digest is lowercase SHA-256 of these canonical payload bytes. The
payload is lowercase hex encoded before it is stored as an unknown project
record, so embedded tabs or newlines cannot alter the project record grammar.

## Append-only records

The original migration/invalidation marker is never removed.

```text
FIXTURE_QUALIFICATION_ATTESTATION\t1\t<content-sha256>\t<hex-canonical-payload>
QUALIFICATION_SUPERSESSION\t1\t<marker-sha256>\t<attestation-sha256>
QUALIFICATION_REVOCATION\t1\t<attestation-sha256>\t<reason-sha256>\t<hex-reason>
```

Graduation validates the complete attestation against the current project on a
copy, refuses a repeated digest, then appends the attestation and one exact
supersession per named marker. The project is replaced only after the complete
transaction succeeds.

Revocation requires an embedded attestation digest and a non-empty bounded
reason. It appends a revocation; it does not delete evidence or supersessions.
Requalification appends a new attestation and new supersessions. Two active
attestations for the same fixture/backend/marker are contradictory and block
physical output until stale evidence is revoked.

## Gate behavior

With physical output disabled, unresolved markers and invalid evidence are
project warnings. Studio can therefore open, inspect, repair, save, and stage a
project without emitting DMX.

With physical output enabled, malformed, tampered, stale, partial, missing,
replayed, contradictory, or revoked evidence is a project validation error. A
supersession counts only when its marker digest, attestation digest, immutable
payload, current binding, and exact marker all agree.

A marker containing an exact fixture-ID token is scoped to that fixture.
Otherwise the marker is global. A global marker is cleared only when every
fixture/backend endpoint that can emit through the current connection settings
has exactly one active matching attestation. Evidence for one physical unit does
not graduate another unit of the same make/model.

Output backend tokens are versioned by this schema and currently use:

- `artnet:uN`
- `sacn:uN`
- `dmx-usb-pro:uN`
- `soundswitch-micro:uN`
- `soundswitch-control-one:uN`

Enabling another backend or changing its project connection state changes the
project basis and requires current evidence for the newly reachable endpoint.

## Verification matrix

The native contract tests cover deterministic seal/parse/round-trip,
transactional graduation, original-marker preservation, output-disabled
staging, physical-output blocking, complete footprint coverage, missing
observation, timeout, device loss, blackout failure, spill, duplicate channel,
cross-project/stale basis, payload tampering, replay, revocation,
requalification, contradictory active evidence, and independent qualification
of two identical physical units.

Hardware observations, Windows installed-app smoke, and owned SoundSwitch Micro
or fixture results must be recorded separately. Passing these software tests is
not physical qualification.
