# Control One Mapping Reference

The `.qxi` profile is authoritative. This condensed map documents the stable logical channels used by the V20 workspace and plug-in.

| Channels | Role |
|---|---|
| 0–31 | Active-page performance pads 1–32 |
| 32–35 | Banks 1–4 |
| 36–44 | Red through Full Color overrides |
| 45–47 | White, Black, UV holds |
| 48–63 | Select/touch/transport/mode/group controls |
| 71–72 | Pan/Speed and Tilt/Color touch sensors |
| 160–163 | Shift + Banks 1–4 |
| 164–172 | Nine shifted position override pads |
| 173–175 | White, Black, UV toggles |
| 182 | Link / Shift + BPM |
| 188 | Shift + Auto Loop / Bank-All scope |
| 189 | Scripted intensity layer |
| 278 | Select encoder, normalized absolute |
| 327 | Pan/Speed encoder, Autoloop chase multiplier |
| 328 | Tilt/Color encoder, reserved/partial |
| 330–339 | Ten Autoplay starts: four Banks plus All, sequential/random |
| 469 | Playback ready/stopped state |
| 470–474 | Chase speed 0.25x, 0.5x, 1x, 2x, 4x |
| 480–484 | Autoplay dwell 1, 2, 4, 8, 16 measures |
| 485–486 | Sequential/random order state |
| 494–499 | Auto Bank 1–4, Auto All, and Manual owner state |
| 500–502 | Running, paused, and Priority Looks mode state |
| 503–508 | Global, Groups 1–4, and Scripted intensity targets |
| 510 | Return to the Control One Live page |
| 511 | Absolute touch-strip intensity |
| 600–631 | Priority Look pads 1–32 and LED feedback |
| 632 | Absolute seek within the active Autoplay parent |
| 800–803 | V20 clickable bank-bar UI feedback/commands |

Base pad channels are intentionally page-relative. The active bank is carried separately; do not remap later bank pages to 32–127 inside nested frames.

## Physical roles still reserved or partial

- Select encoder: cataloged and normalized, but no final live role.
- Pan/Speed: speed multiplier is implemented; Pan override remains future work.
- Tilt/Color: cataloged; Tilt and color-encoder workflow remain future work.
- Movement, Strobe, Hue, Smoke, Back, Link, and position pads: mapped or reserved, but not all have useful Functions for the current fixture set.
- OLED: unsupported and out of scope.
