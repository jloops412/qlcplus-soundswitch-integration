# Owned Fixture Source Inventory and First Bench Plan

Status: **Immediate evidence inventory for Joshua's first hardware qualification pass**  
Date: 2026-08-11  
Authority: subordinate to `21_CORE_SYSTEMS_RECOVERY_AND_HARDWARE_QUALIFICATION_PLAN.md` and `23_FIXTURE_LIBRARY_INGESTION_AND_PROFILE_QUALIFICATION_PLAN.md`.  
Scope: accelerate the first physical proof on Joshua's reported fixtures without making those fixtures the general product scope.

## 1. Important new finding

Both Lighting USA publishes manufacturer-hosted manuals and downloadable SoundSwitch `.ssl2` fixture-profile files for Joshua's IR-4 and BO-TUBE192 360 Tubes.

This materially improves the fixture evidence available for the first bench test:

- the IR-4 has an official current manual, an exact model identifier (`BOIR4`), and a manufacturer-hosted SoundSwitch fixture profile;
- the 360 Tube has an official manual, exact model identifier (`BO-TUBE192`), and a manufacturer-hosted SoundSwitch fixture profile;
- the support page also provides Wolfmix profiles for comparison for some modes;
- these source files can be preserved, hashed, inspected, and compared with the manuals;
- redistribution rights for the raw `.ssl2` files are not stated, so do not commit or package them until permission/license is established.

Manufacturer support page:

`https://bothlightingusa.com/pages/support`

## 2. Concrete likely cause of a dark IR-4

The official IR-4 manual documents two fundamentally different DMX modes.

### 2.1 Six-channel mode

```text
CH1 Red
CH2 Green
CH3 Blue
CH4 White
CH5 Amber
CH6 Purple / UV emitter
```

Each channel is direct intensity: `0 = off`, `1–255 = dimmer`.

### 2.2 Ten-channel mode

```text
CH1 Total/master dimmer
CH2 Red
CH3 Green
CH4 Blue
CH5 White
CH6 Amber
CH7 Purple / UV emitter
CH8 Strobe
CH9 Programs / sound modes
CH10 Color selection / speed
```

This means a profile/mode mismatch can produce total darkness while EmberLights correctly reports nonzero slots:

- if the physical IR-4 is in 10-channel mode and EmberLights emits colors as if it were in 6-channel mode, the values land on the wrong functions;
- if EmberLights writes color channels but leaves the 10-channel master dimmer at zero, the fixture remains dark;
- nonzero universe-slot count does not prove that the target fixture's master and expected color channels are nonzero;
- a provisional linear migration profile may also leave a required master/shutter/program safe-open value at zero.

This is now a ranked, testable backend/profile hypothesis. It does not eliminate the separate possibility that the Micro has not entered physical DMX streaming state.

## 3. IR-4 source inventory

### 3.1 Exact identity

- Manufacturer: Both Lighting USA
- Product: IR-4
- Manual model: `BOIR4`
- Emitters: RGBWA+UV
- Documented DMX modes: 6-channel and 10-channel
- Reported control path: wired/wireless DMX, IR, app, master/slave

### 3.2 Official manual

Manufacturer-hosted manual:

`https://cdn.shopify.com/s/files/1/0716/8645/5572/files/IR-4_User_Manual.pdf?v=1785942928`

The manual should be preserved locally as evidence and hashed. Do not rely only on a web-parsed transcription when implementing the profile.

### 3.3 Manufacturer-hosted SoundSwitch fixture profile

Support-page filename/link:

`https://cdn.shopify.com/s/files/1/0716/8645/5572/files/Both_Lighting_6in1_LED_10_Channel.ssl2?v=1753828601`

The link name suggests a reusable six-in-one LED personality containing or targeting the 10-channel layout. The same link appears beside several Both Lighting RGBWA+UV products on the support page, so do not assume it is uniquely authored for IR-4 until its internal manufacturer/model/modes are inspected.

Required handling:

1. download unchanged;
2. calculate SHA-256;
3. record source URL and retrieval date;
4. inspect file signature/container without modifying it;
5. open in SoundSwitch Fixture Manager and record listed manufacturer/model/modes;
6. compare every mode with the IR-4 manual;
7. import only a mode that matches exactly;
8. retain source as private evidence unless redistribution permission is established.

### 3.4 Manual inconsistency to preserve, not hide

The product/specification identifies the sixth emitter as UV, while the DMX table labels it “Purple.” Treat this as a documentation ambiguity:

- preserve the exact manual wording in evidence;
- expose the native semantic as UV only after physical/manual confirmation;
- do not silently reinterpret the upstream profile;
- test visible response carefully and record the result.

## 4. First bench target: IR-4 in 6-channel mode

The IR-4 6-channel mode is the preferred first fixture proof because it has:

- a current manufacturer manual;
- a simple direct six-channel map;
- no required master dimmer;
- no program or strobe channel in the selected footprint;
- an exact manufacturer-hosted SoundSwitch profile available for later comparison.

### 4.1 Physical setup to confirm

Before output:

- close SoundSwitch and any process that may own the Micro;
- set one IR-4 to DMX mode;
- set channel mode to `06` / 6-channel;
- set address to `001` for the first isolated test;
- ensure Master is Off;
- confirm the expected wireless frequency/protocol and transmitter channel;
- isolate one fixture where possible;
- begin with blackout.

Do not infer these settings from the project. Read them from the fixture display.

### 4.2 Raw test sequence

Using the standalone Hardware Test and universe 1:

```text
All channels 0            -> fixture off
CH1 = 255                  -> red
CH1 = 0, CH2 = 255         -> green
CH2 = 0, CH3 = 255         -> blue
CH3 = 0, CH4 = 255         -> white
CH4 = 0, CH5 = 255         -> amber
CH5 = 0                    -> blackout
```

Test CH6 only after the Purple/UV documentation ambiguity is acknowledged in the report.

Every active step must be bounded and return automatically to blackout.

### 4.3 Interpretation

- If raw CH1 red does not work, do not modify the fixture profile. Continue Micro/session/electrical diagnosis.
- If raw CH1 red works, the Micro transport and selected physical universe path are proven for that state.
- If raw works but the one-fixture Runner project does not, compare emitted slots; the defect is profile, patch, compiler, layer, or output routing.
- If Runner bytes match the raw working frame but physical behavior differs, investigate adapter lifecycle/timing/reconnect ownership rather than fixture semantics.

### 4.4 Ten-channel follow-up

After 6-channel qualification, optionally test 10-channel mode:

```text
CH1 = 255, CH2 = 255       -> master open + red
CH1 = 255, CH3 = 255       -> master open + green
CH1 = 255, CH4 = 255       -> master open + blue
```

Keep CH8–CH10 at safe documented defaults during basic color proof. Do not use strobe/program discovery automatically.

A qualified 6-channel profile and a qualified 10-channel profile are separate native profiles with separate hashes and evidence.

## 5. 360 Tube source inventory

### 5.1 Exact identity

- Manufacturer: Both Lighting
- Product: 360 Tube
- Model: `BO-TUBE192`
- Emitters: RGBWA
- Pixel-capable fixture
- Documented modes reported by manufacturer/manual sources: 5, 6, 7, 12, 40, 80, and 160 channels

### 5.2 Official manual

Manufacturer-hosted manual:

`https://cdn.shopify.com/s/files/1/0716/8645/5572/files/360_TUBES_User_Manual.pdf?v=1785942927`

### 5.3 Manufacturer-hosted SoundSwitch profile

`https://cdn.shopify.com/s/files/1/0716/8645/5572/files/Both_Lighting_BO-TUBE192.ssl2?v=1753828601`

### 5.4 Additional manufacturer comparison files

The manufacturer support page also links Wolfmix profiles for:

- 40-channel mode;
- 80-channel mode.

These are useful independent comparison evidence, not authoritative replacements for the manual or physical proof.

### 5.5 Qualification order

Do not use the 360 Tube as the first Micro transport proof. It is a later target because:

- it has many modes;
- higher modes are pixel/multi-cell layouts;
- the current native profile model flattens or quarantines some multi-cell semantics;
- wireless protocol selection and internal effects add more variables.

Recommended order:

1. qualify IR-4 6-channel transport/profile;
2. inspect and hash the BO-TUBE192 manual and `.ssl2`;
3. select one simple exact tube mode from the physical display/manual;
4. raw-test only documented safe channels;
5. qualify a simple whole-tube mode;
6. then add cell-aware 40/80/160-channel work through the richer fixture-model backlog.

## 6. CHAUVET DJ Wash FX Hex source inventory

### 6.1 Exact product facts

The official CHAUVET DJ product page lists:

- exact product: Wash FX Hex;
- RGBAW+UV emitters;
- six zones;
- DMX modes: 3, 6, 11, and 40 channels;
- manufacturer manual and ShowXpress profile downloads.

Official product page:

`https://www.chauvetdj.com/products/wash-fx-hex/`

Regional product pages may expose the same downloads when the main locale redirects.

### 6.2 OFL near-match warning

Open Fixture Library contains a `Chauvet DJ WashFX` profile with 7-channel and 23-channel modes. That is not an exact mode match for Wash FX Hex's documented 3/6/11/40-channel personalities.

Therefore:

- do not import OFL `chauvet-dj/washfx` as Wash FX Hex merely because the names are similar;
- search SoundSwitch Fixture Manager for the exact `Wash FX Hex` model/mode;
- use the official Wash FX Hex manual/ShowXpress profile as source evidence;
- create/import the exact selected mode;
- verify physically.

This is a concrete example of why name matching without footprint/manual comparison is unsafe.

### 6.3 Recommended qualification mode

After IR-4 proof, the Wash FX Hex 6-channel personality is a likely simple color-mode candidate, but the build/test flow must read the exact official channel chart before generating a profile. Do not encode the mapping from a reseller summary.

The 40-channel mode is a multi-zone profile and should follow the same richer cell-aware path as the tube's pixel modes.

## 7. Implications for the current migrated project

The current `SoundSwitch 2026 Color Rig V1 - PATCH REVIEW REQUIRED` project is not adequate for physical fixture qualification because:

- its addresses were staged safely, not decoded from the user's actual patch;
- its profiles are provisional semantic linear profiles;
- exact physical modes were not confirmed;
- required master dimmer/shutter/program-safe values may be absent;
- tube cell layouts are approximated;
- fixture names/profile counts can validate while mode/channel semantics remain wrong.

The report showing 78 nonzero slots proves that the rendered universe is not all black. It does not prove that:

- the target fixture starts at one of those slots;
- the target fixture is in the assumed footprint;
- the master dimmer/open channel is nonzero;
- the physical wireless path sees valid DMX;
- the Micro firmware accepted the application frame as DMX output.

The next installer must mark this project with `MIGRATED_PATCH_UNVERIFIED` and direct the operator to the one-fixture bench workflow.

## 8. Required source-capture tasks for the build agent

For IR-4 and BO-TUBE192:

1. download manual and `.ssl2` from the manufacturer support page;
2. preserve exact bytes outside the repository unless redistribution rights are clear;
3. generate SHA-256, byte size, retrieval timestamp, source URL, and MIME/signature report;
4. inspect whether `.ssl2` is a documented archive, structured text, database, or opaque/protected container;
5. do not bypass encryption or authentication;
6. open the file with Fixture Manager and record visible metadata/modes;
7. compare against the exact manual;
8. create sanitized synthetic fixtures for parser unit tests if raw manufacturer fixtures cannot be redistributed;
9. store only hashes, metadata, conversion reports, and independently authored native profiles in the repository unless permission is established;
10. add the source inventory to the release/qualification report.

For Wash FX Hex:

1. obtain the official manual and ShowXpress profile;
2. search Fixture Manager exact Production/Public/Local results;
3. export one exact personality where permitted;
4. confirm the physical mode on Joshua's fixture;
5. build/qualify the selected native profile;
6. do not use the OFL WashFX near-match.

## 9. Required first-project artifact

Generate a minimal project separate from the migration:

```text
Name: Joshua IR-4 Raw-to-Runner Bench
Universe: 1
Fixture count: 1
Fixture: Both Lighting IR-4
Mode: 6-channel
Address: 1
Output: SoundSwitch Micro universe 1
Autoloops: optional single diagnostic loop only
Static Looks:
  Blackout
  Red
  Green
  Blue
  White
  Amber
```

Every look must render a documented, reviewable frame. The project must include profile/source hashes and remain `BenchReady` until the physical tests pass.

The raw Hardware Test frame and each equivalent Runner frame must be exportable and byte-compared.

## 10. Stop conditions

Stop fixture/profile work and return to transport diagnosis when:

- raw IR-4 CH1 red fails in confirmed 6-channel mode/address 1;
- the transmitter/fixture gives no DMX indication from the standalone test;
- the Micro never reaches Initialized/WarmedUp/Streaming;
- another process owns the device;
- blackout cannot be guaranteed.

Stop profile qualification and quarantine the candidate when:

- source mode/footprint does not exactly match the manual;
- `.ssl2` metadata does not identify the expected fixture/modes;
- required defaults/ranges are ambiguous;
- the source appears protected or cannot be parsed safely;
- the physical behavior contradicts the manual/profile;
- Runner cannot reproduce the successful raw frame.

## 11. Morning information to capture

The minimum operator observations needed are:

- exact fixture selected for bench;
- physical DMX mode shown on display;
- physical address shown on display;
- Master setting;
- wireless frequency/protocol/transmitter selection;
- whether raw CH1 red works;
- whether transmitter shows DMX activity;
- Hardware Test lifecycle state;
- exported raw frame report;
- equivalent Runner frame report;
- physical color/blackout result.

No broad rig inventory is required before the first one-fixture proof.

## 12. Source references

- Both Lighting USA Support & Downloads: `https://bothlightingusa.com/pages/support`
- IR-4 product: `https://bothlightingusa.com/products/ir-4`
- IR-4 manual: `https://cdn.shopify.com/s/files/1/0716/8645/5572/files/IR-4_User_Manual.pdf?v=1785942928`
- Both Lighting six-in-one `.ssl2`: `https://cdn.shopify.com/s/files/1/0716/8645/5572/files/Both_Lighting_6in1_LED_10_Channel.ssl2?v=1753828601`
- BO-TUBE192 manual: `https://cdn.shopify.com/s/files/1/0716/8645/5572/files/360_TUBES_User_Manual.pdf?v=1785942927`
- BO-TUBE192 `.ssl2`: `https://cdn.shopify.com/s/files/1/0716/8645/5572/files/Both_Lighting_BO-TUBE192.ssl2?v=1753828601`
- CHAUVET DJ Wash FX Hex: `https://www.chauvetdj.com/products/wash-fx-hex/`
- OFL WashFX near-match: `https://open-fixture-library.org/chauvet-dj/washfx`
