# SoundSwitch source provenance

V27 was informed by the previously shared SoundSwitch project, but the QLC+
V26 patch remains authoritative. Source addresses below are provenance only;
they are not copied into V27.

## Authoritative source

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| `2026.ssproj.zip` | — | `2C58ED57965CD12A0702252595D4966EF8CAEF4A3B024E24BC001E245FCFE11C` |
| `.ssproj` | 160 | `2E4F37DA9A5DA2AF6F255525E63AF725A1310B7E1EFE4E7448314167E4E05B47` |
| `SoundSwitchAutoLoops.bin` | 1,876 | `27ABB5C0D0232E79673EA09B7812BAE1AC796A8F895AAD0C742DA69320A23560` |
| `SoundSwitchAutoLoopsEx.bin` | 4,666 | `A2F57B68F945942A4083373D4983CCA1232604E5218DD55926AF2D3FF51A4209` |
| `SoundSwitchVenues.bin` | 447,858 | `22C78D611B5D9A005A615D5D6D90A2063647BADC974E41C05A595FCE8366C508` |

Project ID: `{7CED9022-5F3E-4154-9C40-E5592BE8F145}`. SoundSwitch
version: 2.10.3 build 0. Venue GUID:
`143b8732-6bce-471c-98bf-2840654b346e`.

## Fixture resolution

| Source identity | Source personality | Count | Source starts |
|---|---|---:|---:|
| Chauvet `Wash FX HEX` | `Mode 1`, 11 channels | 1 | 147 |
| American DJ `Focus Spot Two` | `Mode 2`, 18 channels | 2 | 31 and 49 |
| SHEHDS `LED 160W 3IN1 Gobo Light_Kalsec` | `Mode 1`, 18 channels | 2 | 67 and 85 |

The SHEHDS/Kalsec fixtures are distinct from the ADJ Focus Spot Twos: they
have different manufacturer/model strings, GUIDs, profiles, and instance
records. They are not imported as Focus aliases.

The source contains one exact Wash target in all 112 placed timelines. Focus
base references occur in all 112 timelines, while explicit Focus group/motion
blocks occur in 72. V27 deliberately broadens both Focus fixtures to every
live QLC+ Autoloop step, Priority frame, performance Scene, and override.

## Intentional V27 patch

| Destination | Mode | Physical U1 | Private U3 mirror |
|---|---|---:|---:|
| Wash FX Hex | 40-channel | 041–080 | 041–080 |
| Focus A | 18-channel | 081–098 | 081–098 |
| Focus B | 18-channel | 099–116 | 099–116 |

This preserves every V26 fixture address and leaves U1/U3 117–174 free before
the tubes at 175. The physical Wash must be set to **40-channel mode**; the
SoundSwitch source used its 11-channel personality. Private U3 is an internal
Priority layer and must never be routed to physical DMX output.

SoundSwitch did not label the Focus instances left/right. V27 therefore keeps
the proven A/B identity. Confirm which physical head is A and B during the
bench test before relying on stage-left/stage-right wording.

## Recovered Focus positions

The venue stores assigned pan/tilt as duplicated 16-bit words:
`packed32 = raw16 | (raw16 << 16)`. QLC+ receives the high and low bytes as its
coarse/fine channels. These are the exact saved source values, not generated
coordinates.

| Position | Focus A pan / tilt | A coarse/fine | Focus B pan / tilt | B coarse/fine |
|---|---:|---|---:|---|
| Crossed Out Down | 46027 / 8286 | 179/203; 32/94 | 40693 / 6629 | 158/245; 25/229 |
| Crossed In Down | 50752 / 6930 | 198/64; 27/18 | 53342 / 3917 | 208/94; 15/77 |
| Stage Right | 35663 / 7231 | 139/79; 28/63 | 33682 / 3917 | 131/146; 15/77 |
| Stage Left | 20727 / 65384 | 80/247; 255/104 | 23013 / 63878 | 89/229; 249/134 |
| Straight Ahead | 39473 / 2260 | 154/49; 8/212 | 47094 / 1055 | 183/246; 4/31 |
| Crossed Out Up | 40693 / 2561 | 158/245; 10/1 | 45875 / 1356 | 179/51; 5/76 |
| Crossed In Up | 39626 / 3314 | 154/202; 12/242 | 47094 / 1356 | 183/246; 5/76 |
| Up | 45417 / 15066 | 177/105; 58/218 | 40997 / 13107 | 160/37; 51/51 |
| Down | 50142 / 8587 | 195/222; 33/139 | 49532 / 6629 | 193/124; 25/229 |

`Disco Ball` is the position-collection header, not a tenth position. The
ninth true preset is `Up`; its two-character name is easy to miss in a basic
string scan.

Exact coordinates do not remove the need for a physical safety test. Verify
mounting orientation, A/B assignment, stage boundaries, and audience clearance
with shutters closed and dimmers at zero before opening either mover.
