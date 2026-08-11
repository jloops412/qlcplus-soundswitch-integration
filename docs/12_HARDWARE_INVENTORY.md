# Hardware Inventory

Status: user-reported names captured; exact USB identities and modes pending.

| Device | Role | Confidence | Next evidence |
| --- | --- | --- | --- |
| Pioneer DDJ-REV7 | Primary VirtualDJ DJ controller | Confirmed | VirtualDJ version, firmware, Windows device details, transport captures. |
| SoundSwitch Control One | Primary lighting MIDI surface; two proprietary DMX outputs | Confirmed | MIDI port names, VID/PID, complete input map, safe output-feedback map. |
| WOLFmix W1 family | Possible later standalone controller / DMX-input bridge; not a documented PC USB-DMX interface | User requested later compatibility; exact model/ownership unverified | Exact model/firmware, activated feature set, DMX-input availability, connector routing, and controlled bridge test. |
| ADJ MyDMX Buddy | Possible USB-DMX adapter | User believes owned; exact model unverified | Label/photo, VID/PID, driver, QLC+ compatibility, channel/license limits. |
| SoundSwitch DMX Micro Interface | Native one-universe WinUSB output candidate | `VID_15E4/PID_0053`, interface GUID and bulk OUT `0x01` verified on Joshua's PC; preview.310 A/B/C payloads physically disproved; native JLS1 initialization and 522-byte frame independently established from SoundSwitch 2.10.3 | Physical response with the JLS1 build, disconnect/reconnect, sustained frame output. |

## Reported lighting fixtures

| Fixture | Likely role/capabilities | Confidence | Evidence still needed |
| --- | --- | --- | --- |
| [Both Lighting IR-4](https://bothlightingusa.com/products/ir-4) | Wireless battery RGBWA+UV uplight | Manufacturer manual verified: 6-channel mode is R/G/B/W/Amber/UV; 10-channel mode also exists. Exact built-in 6-channel profile and U1/address-001 frame qualification are implemented. | Quantity, label/photo, firmware if exposed, operator confirmation of current mode, physical raw/Runner response, blackout, and reconnect. |
| [Both Lighting 360 LED Tube](https://bothlightingusa.com/products/360-tubes) | Wireless battery pixel tube | Model family confirmed from user wording | Quantity, exact revision/model label, pixel count, current DMX mode, channel chart/manual. |
| [CHAUVET DJ Wash FX Hex](https://www.chauvetdj.com/products/wash-fx-hex/) | Six-zone RGBAW+UV wash/chase/blinder effect | Model confirmed from user wording | Quantity, confirm original versus ILS variant, current DMX personality, label/photo. |
| Other fixtures | First profile pack | Known to exist, names pending | Model names, quantities, manuals, and which are part of the normal rig. |

Do not infer a channel map from a retail name. Profiles are accepted only after the active mode and its DMX table match the physical fixture.

## Capture checklist on Windows

For each device:

1. Photograph front/back/label and cable ports.
2. Record exact marketing/model name.
3. Device Manager → device name, provider, driver version/date.
4. Hardware IDs including VID/PID and interface identifiers.
5. MIDI input/output port names, if any.
6. Whether SoundSwitch, QLC+, VirtualDJ, or another app has the device open.
7. Maximum universes and any software/license restriction.
8. Disconnect/reconnect behavior.
9. For Control One, capture MIDI and proprietary USB interfaces separately; do not infer DMX support from working MIDI.
10. For WOLFmix, record whether DMX input/WLINK is activated and test it only as an external bridge unless the manufacturer publishes a host-output API.

Do not upload serial numbers, license keys, or credentials. Redact them from screenshots/captures.

## Benchmark machines

Still needed:

- Joshua's primary DJ laptop model, CPU, RAM, GPU, Windows version, and normal background workload.
- A lower-end Windows test machine representing the minimum supported tier.
- Optional separate lighting PC/mini-PC and wired-network hardware.
