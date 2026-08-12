# EmberLights Installed Hardware Test

The Start-menu **EmberLights Hardware Test** shortcut still launches
`Tools\soundswitch_micro_probe.exe --active-test`. Its active behavior is now
the evidence-bound Raw Hardware Test v1: one reviewed project, one fixture ID,
one named physical unit, and one enabled SoundSwitch Micro universe binding.

Normal launch creates the unchanged passive `SoundSwitch-Micro-report.txt`.
`--self-test` is also non-outputting. Neither mode sends a USB transfer. Active
output requires a valid manifest and an exact typed acknowledgement after the
complete plan is displayed.

## Prepare an isolated fixture

1. Close SoundSwitch and EmberLights so only Hardware Test can own the Micro.
2. Connect exactly the physical fixture/unit named in the manifest. Verify its
   displayed mode, DMX address, universe, cable or wireless binding, and master
   setting against the candidate project and exact manual.
3. Disconnect every unrelated fixture. Disconnect fog, haze, lasers, sparks,
   and any effect whose isolated response would be hazardous.
4. Keep a physical emergency disconnect within reach. A host-accepted USB
   write is not proof that the fixture responded or blacked out.

## Create a reviewed v1 manifest

Save a UTF-8 tab-separated text file. Relative paths resolve from the manifest
directory. `graduated_project` must not exist: Hardware Test never overwrites a
candidate or previous result. `audit` may exist only if it is already a valid
append-only v1 audit.

```text
EMBERLIGHTS_RAW_HARDWARE_TEST_OPERATOR	1
project	candidate.emberlights
graduated_project	candidate.unit-a-qualified.emberlights
audit	candidate.raw-hardware-attempts-v1.audit
input_project_sha256	<64 lowercase SHA-256 hex characters>
fixture_id	<one exact fixture ID from the candidate>
unit_label	<one physical unit or serial label>
output_backend	soundswitch-micro:u1
operator_id	<operator identity>
observation_timeout_ms	30000
session_timeout_ms	3600000
blackout_repetitions	3
criterion	1	255	<reviewed expected behavior for slot 1>
criterion	2	255	<reviewed expected behavior for slot 2>
marker	MIGRATED_PATCH_UNVERIFIED	<copy the complete exact marker record>
```

The file must contain exactly one value for every single-valued field, one
`criterion` for every slot in the selected fixture's footprint, and at least
one exact current `MIGRATED_PATCH_UNVERIFIED` or `QUALIFICATION_INVALIDATED`
record. Criteria are one-based. Each raw value must be nonzero and every
criterion implicitly requires no spill. Add or remove criterion lines to match
the exact profile footprint; do not copy the two-line example as fixture truth.

The active input SHA-256 comes from the candidate/migration evidence that
produced this project. It is not inferred from a fixture name. RGBWAUV, dimmer,
strobe, macro/program, neutral/default, fine-channel, and multi-cell boundary
behavior must be reviewed against the exact manual and then physically
observed. Imported semantic property names are not proof.

The project must already enable the Micro on the selected fixture universe.
No Art-Net, sACN, DMX USB Pro, Control One, generic backend, arbitrary channel
list, or raw full-frame mode is accepted by this installed workflow.

## Run the bounded active test

1. Open **EmberLights Hardware Test**. The passive descriptor report is saved
   before any active workflow begins.
2. Enter the v1 manifest path. The tool reads the candidate without backup
   recovery, validates its exact file SHA-256, project basis, fixture/profile,
   unit, mode/address/universe, behavior fingerprint, safety policy, backend,
   criteria, markers, time budgets, audit, and unused graduation path.
3. Review every displayed blackout/one-hot requirement and path. The tool does
   not open the Micro during this review.
4. Type the complete generated `QUALIFY ...` line exactly. `TEST`, `yes`, case
   changes, missing fields, or trailing spaces do not authorize output. The
   candidate and destinations are revalidated after acknowledgement.
5. For each stimulus, enter one bounded command before its observation timeout:

   - `PASS <what physically happened>` — expected behavior observed, no spill;
   - `FAIL <what failed>` — behavior did not pass;
   - `SPILL <what else responded>` — a neighbor/cell/other function responded;
   - `CANCEL <reason>` — stop the attempt.

   Escape, Ctrl+C, console-close notification, timeout, device disappearance,
   write failure, or invalid session state also stops the attempt. Invalid text
   commands do not extend the original timeout.

For every requirement—including blackout—the state machine checks connection,
sends a bounded blackout-before train, sends exactly one internally generated
all-zero or one-hot frame inside the selected fixture footprint, waits for one
bounded observation, and sends blackout-after. Every terminal fault attempts a
final blackout and closes the production `SoundSwitchMicroSession`. No Runner,
`CompiledShow`, Look, Autoloop, full patch, neighboring fixture data, timer
script, or arbitrary frame enters this path.

## Audit and graduation

Every terminal session that can be sealed appends one content-addressed record
to the manifest's versioned audit:

```text
EMBERLIGHTS_RAW_HARDWARE_TEST_AUDIT	1
RAW_HARDWARE_TEST_ATTEMPT	1	<content-sha256>	<hex-canonical-payload>
```

The audit is append-only; malformed content, duplicate attempt digests, or an
oversized audit blocks the run or append. A failed, cancelled, timed-out,
device-lost, blackout-failed, stale, or otherwise rejected attempt remains
audit evidence but never creates a graduated project and never authorizes
output. Accepted USB writes alone cannot graduate anything.

Only a fully successful sealed attempt is considered for graduation. After
auditing it, the tool re-hashes and reloads the exact candidate file, validates
the attempt and current markers again, transactionally embeds the attempt,
attestation, and exact supersessions in memory, and atomically creates the new
`graduated_project`. Any byte change, profile/binding/project change, tamper,
replay, missing marker, pre-existing output, or save failure leaves the attempt
audited and produces no authorized project. Other unqualified fixture/backend/
marker tuples may still keep the graduated candidate's physical-output gate
blocked.

## Compatibility and remaining gates

- The passive report filename and descriptor behavior are unchanged.
- The installed shortcut argument remains `--active-test`; it now asks for the
  v1 manifest instead of running the former fixed IR-4/Runner sequence.
- The former `SoundSwitch-Micro-active-test.txt` report is not overwritten.
  Evidence-bound runs use the manifest-selected, explicitly versioned audit.
- CR-4 raw-to-one-fixture Runner byte parity remains a separate acceptance gate
  in issue #40 / the core recovery plan, section 8.4. It is deliberately not
  invoked, erased, or claimed by this isolated qualification workflow.
- Software tests and simulated transports prove only parsing, bounds, state,
  audit, and graduation contracts. They provide no owned-hardware, installed
  Windows, visible blackout, spill, transport endurance, CR-4, or gig-readiness
  evidence. Record those claims only from the corresponding physical runs.
