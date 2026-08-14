# IR-4 6CH Editable Bench and Runner Frame Test

This installer includes an output-disabled, one-fixture project named
`Templates/EmberLights-IR4-6CH-Editable-Bench.emberlights`. Its only profile is
an editable Local clone of the manual-backed Both Lighting BO-IR4 6CH profile.
Its six zero-fade Static Looks are Blackout, Red, Green, Blue, White, and Amber.

The initial semantic mapping is:

| Look | Exact expected U1 byte |
| --- | --- |
| Blackout | all 512 channels `0` |
| Red | CH1 `255`; every other channel `0` |
| Green | CH2 `255`; every other channel `0` |
| Blue | CH3 `255`; every other channel `0` |
| White | CH4 `255`; every other channel `0` |
| Amber | CH5 `255`; every other channel `0` |

CH6 is the manual's Purple/UV function and remains forced to zero in every
safe bench look. Software tests prove that these exact semantic frames compile;
they do not prove how a particular physical fixture responds.

## Prepare one isolated fixture

1. Close SoundSwitch and any other DMX application. Disconnect every fixture
   except one IR-4. Do not connect fog, haze, laser, spark, or another effect.
2. Put the physical IR-4 in its exact 6-channel mode at DMX address 1. Keep its
   manual and a physical emergency disconnect within reach.
3. Open **Start > EmberLights > IR-4 6CH Editable Bench**. Immediately use
   **File > Save As** to create a working copy outside the installation folder.
4. The packaged project has every physical output disabled. In **System >
   Connections**, select SoundSwitch Micro universe 1, verify Native JLS1
   framing, then choose **Save & Apply**. Do not enable another output backend.

## Compare what EmberLights routed with what the fixture did

1. Start Show. Use the Static Looks list in Live; double-click Blackout, Red,
   Green, Blue, White, and Amber one at a time.
2. After each selection, open **System > Diagnostics**. The **Runner frame
   inspection** section reports the actual latest production Runner frame:

   - generation, sequence, age, and global-blackout state;
   - pre-blackout and routed frame SHA-256 values;
   - nonzero channels and exact byte values;
   - fixture, profile, physical mapped channel, semantic property, encoding,
     winning layer, value mode, and renderer origin;
   - each configured output route's attempted and accepted frame counts and
     last error.

3. Choose **Copy Diagnostics** or **Save Diagnostics** after White and Amber.
   A host-accepted write is transport evidence, not proof of visible output.
   Record the observed physical color separately.

If the working project was graduated by the bounded Raw Hardware Test, the
same Diagnostics report also includes **Raw Hardware Test -> Runner parity**.
It shows the exact prior attempt/attestation identities, whether that attempt
matches the current project basis or is historical, the raw requirement that
matches the authored reference, the raw requirement that matches the actual
routed frame, and the operator's prior observation for each. A profile-only
revision may retain historical comparison while fixture/profile identity,
manufacturer/model/mode, universe, and address remain unchanged. A repatch is
not silently reused. The section always says current physical response was not
observed by EmberLights; only a new bounded physical test can establish it.

The initial White report must name U1 CH4 = 255 / property White. The initial
Amber report must name U1 CH5 = 255 / property Amber. If those bytes are exact
but the physical colors are reversed, the renderer and route are following the
profile and the physical/profile channel truth is the remaining mismatch.

## Correct the editable profile without a permanent W/A repair button

1. Stop Show. A running Live package is an immutable snapshot; Studio profile
   edits cannot change it until Stop Show / Start Show.
2. Open **Studio > Fixture Profiles**, select the Local IR-4 operator profile,
   then open its channel workbench.
3. Select the White row at CH4 and Amber row at CH5 as the reviewed pair, then
   use the general **Exchange Channel Functions** operation. Review the exact
   before/after physical channels and confirm.
4. Choose **Save Profile**, save the project, then Start Show again. Repeat the
   White and Amber test and save both Diagnostics reports.

After the exchange, White must be attributed to physical CH5 and Amber to CH4.
If the routed bytes change exactly but the fixture still does not follow them,
stop and retain the before/after reports; do not keep guessing at unrelated
channels or claim the fixture qualified.

## Evidence boundary

This workflow isolates profile truth from production Runner/render/routing
truth. The parity report can join a prior bounded Raw Hardware Test to those
software bytes, but it does not replace a fresh observation, blackout/spill
checks, reconnect/endurance testing, or physical qualification. The editable
project is deliberately not a graduated fixture profile and its manual-derived
source remains visible in the profile revision.
