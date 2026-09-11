# IR-4 fixture definition provenance

`Both-Lighting-IR-4-(BOIR4).qxf` closes a missing dependency in the preserved
QLC+ workspace. It is an independently authored definition for the workspace's
exact `Both Lighting / IR-4 (BOIR4) / 10 Channel` identity. No fixture address,
Scene value, color order, or plug-in intensity span was changed to accommodate it.

## Primary evidence

The channel facts come from the manufacturer's
[BO-IR4 user manual, PDF page 5](https://cdn.shopify.com/s/files/1/0716/8645/5572/files/BL_IR-4_BO-IR4.pdf?v=1679519527).
The 359,627-byte PDF was downloaded and its table visually inspected on
2026-09-10. Its SHA-256 is:

`1267e289b2c0577ec749f0de5265105db5e86b6ae3b2e12414cc00777fd3c03a`

That matches the evidence recorded in historical commit `9e96c44`, documents
`28_LIVE_FIXTURE_SOURCE_FIDELITY_PASS.md` and
`32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md`. Those records were
used to locate the primary source; the PDF was checked again for this repair.
The manufacturer's [support page](https://bothlightingusa.com/pages/support)
also identifies IR-4 and provides its manuals and profiles.

## Exact 10-channel mapping

| Display channel | QLC+ offset | Definition semantic |
|---:|---:|---|
| 1 | 0 | Master dimmer |
| 2 | 1 | Red |
| 3 | 2 | Green |
| 4 | 3 | Blue |
| 5 | 4 | White |
| 6 | 5 | Amber |
| 7 | 6 | Purple / UV |
| 8 | 7 | Strobe, raw range |
| 9 | 8 | Internal program / macro, raw range |
| 10 | 9 | Color selection / speed within program, raw range |

The first seven channels use QLC+ native intensity presets. Channels 8-10 use
non-intensity groups and default to zero. Every IR-4 payload in V30 already
keeps those three control bytes at zero; V31 now enforces that invariant across
physical IDs 0-3 and private IDs 100-103. Timed show pulses use authored light
levels rather than an internal fixture strobe program.

## Deliberate limits

- The manual's DMX table calls the sixth emitter Purple, while its specification
  identifies RGBWA+UV LEDs. The label retains both terms; the QLC+ preset uses UV.
- The manual provides no strobe frequency or off-range partition. The definition
  does not invent one. Zero remains the established show value, subject to the
  physical fixture check.
- Internal program and color/speed controls remain raw. Their compound behavior
  is outside this show definition's authored capability set. The XML is not a
  hardware lock; manual editing can still transmit nonzero values.
- Only the required 10-channel mode is included. Other modes and firmware
  variants require separately verified definitions.

The owner-uploaded `Both Lighting-BO-IR4 LED Mini Spotlight.plfix` was inspected
as provenance but is opaque encoded data. Its 7,408-byte source hash is
`8f2ade5155721555437943a99e58d205cba56082498f2f5fa850e61fc30a1373`.
No decoded channel claim is made from that file, and its payload is not bundled.

## Validation and installation

Definition SHA-256:

`082552a1d28ea9777dd8f3601ab654a34d4d61886aafbc76e60ff2931cee4cff`

The definition passes the `fixture.xsd` schema from pinned QLC+ source
`a124abebe0b5ad6077727c561a5a0e1f3730810c`. `V31RigIntegrity.py` verifies exact
identity, channel order, native intensity presets, defaults, raw controls, mode
references, head mapping, and the show control-byte invariant.

Copy the `.qxf` into the QLC+ user `Fixtures` folder, backing up any existing
definition with the same identity first. Restart QLC+ and confirm all eight
physical/private IR-4 instances resolve to `10 Channel` without a generic
substitute. The filename also matches QLC+'s project-local fallback name.

Schema and workspace validation do not prove the installed hardware or firmware.
On the exact host, confirm the 10-channel menu selection and isolated master,
red, green, blue, white, amber, and Purple/UV output. In particular, preserve the
manual-backed White-at-5 / Amber-at-6 order unless recorded physical evidence
supports a separately named local variant.
