#!/usr/bin/env python3
"""Bounded successor corrections to the immutable V32 creative scores.

QLC+ still renders every Scene and owns every Chaser's existing timing. Only
the audited scores below are re-authored; this module does not alter the
Virtual Console, owners, speed tables, fixture patch, or Priority programming.
The Focus exclusions use the direct child/comma-list schema from pinned
QLC+ a124abebe0b5ad6077727c561a5a0e1f3730810c fixture.cpp. Channel numbers
are fixture-relative and zero-based. Continuous channels remain fadeable.
"""
from __future__ import annotations

from math import exp, pi, sin
import xml.etree.ElementTree as ET

import V31RigIntegrity as rig
import V32CreativeProgram as program

q = rig.q
PALETTE_IDS = frozenset({582, 583, 584, 585, 638, 655})
SWEEP_IDS = frozenset({636, 637, 638, 639, 640, 650, 651})
RHYTHM_IDS = frozenset({628, 653})
REPAIRED_LOOP_IDS = PALETTE_IDS | SWEEP_IDS | RHYTHM_IDS
FOCUS_IDS = frozenset({9, 10, 109, 110, 209, 210, 309, 310, 409, 410})
REQUIRED_FOCUS_IDS = frozenset({9, 10, 109, 110, 209, 210})
EXCLUDED_FADE_CHANNELS = (4, 5, 6, 7, 8, 10, 13, 14, 15, 16, 17)
FADEABLE_FOCUS_CHANNELS = frozenset({0, 1, 2, 3, 9, 11, 12})


def _mask(fid: int, t: int, x: float, index: int) -> float:
    if fid == 628:
        # Four quarter-beat subdivisions: one pulse per beat.
        return float(t % 4 == 0)
    if fid == 653:
        # Existing half-beat subdivisions: half beat lit, half beat dark.
        return float(t % 2 == 0)
    if fid in SWEEP_IDS:
        # There are four lit steps in each eight-step pass. Include both
        # endpoints and the four IR-4 centers, rather than stopping at .75.
        travel = ((t // 2) % 4) / 3
        texture = program.SCORES[fid].texture
        if texture in {"outward_hit", "inward_hit"}:
            x = abs(x - .5) * 2
            if texture == "inward_hit":
                travel = 1 - travel
        if "reverse" in texture:
            travel = 1 - travel
        width = .17 if "comet" in texture else .13
        return exp(-((x - travel) ** 2) / (2 * width * width))
    return program.mask(program.SCORES[fid].texture, t, x, index)


def _colour(fid: int, t: int, x: float, index: int, role: str):
    score = program.SCORES[fid]
    bank = (fid - 532) // 32
    continuous = bank == 2 or score.texture in {"morph", "tide", "breathe"}
    phase = (t / program.STEPS * len(score.palette)
             if score.texture in {"morph", "walk", "march", "cut", "eighth", "half", "stripes"}
             else t // 8)
    spatial = (x * len(score.palette)
               if score.texture in {"walk", "march", "stripes", "corners", "morph", "cut"}
               else int(x > .5))
    if fid in PALETTE_IDS:
        phase = (t // 2) % len(score.palette) if fid == 638 else t * len(score.palette) // program.STEPS
        # Coherent hue on each traveling hit; White/Color/White uses the
        # declared white,pink,white,blue order across its four alternating hits.
        # A right-side offset would turn both colored events back into white.
        if fid in {638, 655}:
            spatial = 0
    elif role == "ir" and bank < 2:
        return program.palette_color(score, t // 8, 0 if index % 2 == 0 else 1, False)
    return program.palette_color(score, phase, spatial, continuous)


def raw_frame(fid: int, t: int):
    """Return V32's frame or the explicitly targeted corrected scorer.

    Fixture-level rendering is retained from V32, including emitter order,
    tube accents, ceilings, and mechanically fixed Focus optics. The only
    scorer deltas are the masks and palette progression above.
    """
    if fid not in REPAIRED_LOOP_IDS:
        return program.raw_frame(fid, t)
    rig.require(0 <= t < program.STEPS, "V34 creative: invalid step")
    score = program.SCORES[fid]
    bank = (fid - 532) // 32
    ir_peak, wash_peak, tube_peak, focus_peak = (
        (178, 170, 215, 100), (200, 205, 235, 125),
        (115, 90, 130, 62), (230, 230, 255, 175))[bank]
    floor = (.12, .045, .70, 0.)[bank]

    def gate(x, index, role):
        value = _mask(fid, t, x, index)
        if bank == 2:
            return floor + (1 - floor) * value
        if bank == 3:
            if "hit" in score.texture or "comet" in score.texture or "bounce" in score.texture:
                value *= float(t % 2 == 0)
            return max(0., value)
        if role == "ir":
            return .36 + .46 * _mask(fid, (t // 4) * 4, x, index)
        if role == "wash":
            return .14 + .72 * value
        return floor + (1 - floor) * value

    frames = {}
    for index in range(4):
        x = index / 3
        frame = [0] * 10
        frame[0] = program.clamp(ir_peak * gate(x, index, "ir"))
        frame[1:7] = [program.clamp(v) for v in _colour(fid, t, x, index, "ir")]
        frames[index] = frame
    wash = [0] * 40
    for zone in range(6):
        colour = _colour(fid, t, zone / 5, zone, "wash")
        amount = wash_peak / 255 * gate(zone / 5, zone, "wash")
        r, g, b, w, a, u = [program.clamp(v * amount) for v in colour]
        wash[4 + zone * 6:10 + zone * 6] = [r, g, b, a, w, u]
    frames[4] = wash
    for tube in range(4):
        frame = []
        for pixel in range(8):
            index = tube * 8 + pixel
            x = index / 31
            colour = _colour(fid, t, x, index, "tube")
            amount = tube_peak / 255 * gate(x, index, "tube")
            colour = program.mix(colour, program.COLORS[score.palette[-1]], .16 * (pixel / 7))
            r, g, b, w, a, _ = [program.clamp(v * amount) for v in colour]
            frame.extend([r, g, b, w, a])
        frames[5 + tube] = frame
    for side in range(2):
        if bank == 3:
            amount = gate(float(side), side, "focus")
        elif bank == 2:
            amount = .85 + .15 * sin(2 * pi * (t / program.STEPS + side * .5))
        else:
            amount = .40 + .60 * program.mask(
                "halves" if score.texture in {"halves", "pairs", "checker"} else "breathe",
                t, float(side), side)
        frames[9 + side] = program.focus_frame(score, t, side, focus_peak, bank, amount)
    return frames


def apply_repairs(root: ET.Element) -> dict:
    """Apply only the explicit V34 creative delta to a successor workspace."""
    fixtures = rig.id_map(root, "Fixture")
    functions = rig.id_map(root, "Function")
    rig.require(REQUIRED_FOCUS_IDS <= fixtures.keys(), "V34 creative: missing Focus layer")
    excluded_ids = []
    # Match the loaded fixture profile, not an instance's display name or
    # address. This covers every added native color/hold mirror as well.
    focus_ids = {fid for fid, fixture in fixtures.items()
                 if fixture.findtext(q("Manufacturer")) == "American DJ"
                 and fixture.findtext(q("Model")) == "Focus Spot Two"}
    rig.require(REQUIRED_FOCUS_IDS <= focus_ids, "V34 creative: Focus profile identity changed")
    for fixture_id in sorted(focus_ids):
        fixture = fixtures[fixture_id]
        rig.require(fixture.findtext(q("Model")) == "Focus Spot Two"
                    and fixture.findtext(q("Channels")) == "18"
                    and fixture.findtext(q("Mode")) == "18 Channel",
                    f"V34 creative: Focus definition mismatch {fixture_id}")
        nodes = fixture.findall(q("ExcludeFade"))
        rig.require(len(nodes) <= 1, f"V34 creative: duplicate ExcludeFade {fixture_id}")
        if nodes:
            rig.require(not list(nodes[0]), "V34 creative: ExcludeFade must be a comma list")
            existing = {int(value) for value in (nodes[0].text or "").split(",") if value.strip()}
            rig.require(existing <= set(EXCLUDED_FADE_CHANNELS),
                        f"V34 creative: unexpected excluded Focus channels {fixture_id}")
            node = nodes[0]
        else:
            node = ET.SubElement(fixture, q("ExcludeFade"))
        node.text = ",".join(map(str, EXCLUDED_FADE_CHANNELS))
        excluded_ids.append(fixture_id)

    changed_scenes = []
    seen_leaves = set()
    for loop_id in sorted(REPAIRED_LOOP_IDS):
        loop = functions[loop_id]
        rig.require(loop.get("Type") == "Chaser", f"V34 creative: raw loop {loop_id} is not a Chaser")
        steps = loop.findall(q("Step"))
        rig.require(len(steps) == program.STEPS, f"V34 creative: wrong phrase size {loop_id}")
        for step, reference in enumerate(steps):
            scene_id = int(reference.text)
            rig.require(scene_id not in seen_leaves, f"V34 creative: shared raw leaf {scene_id}")
            seen_leaves.add(scene_id)
            scene = functions[scene_id]
            rig.require(scene.get("Type") == "Scene", f"V34 creative: raw leaf {scene_id} is not a Scene")
            old = rig.parse_values(scene, fixtures)
            frame = raw_frame(loop_id, step)
            expected = {fixture_id: dict(enumerate(values)) for fixture_id, values in frame.items()}
            rig.require(set(old) == set(frame), f"V34 creative: incomplete raw frame {scene_id}")
            if old != expected:
                changed_scenes.append(scene_id)
            for value in scene.findall(q("FixtureVal")):
                scene.remove(value)
            for fixture_id, values in sorted(frame.items()):
                node = ET.SubElement(scene, q("FixtureVal"), {"ID": str(fixture_id)})
                node.text = ",".join(str(v) for pair in enumerate(values) for v in pair)
    return {
        "repaired_raw_loop_ids": sorted(REPAIRED_LOOP_IDS),
        "palette_loop_ids": sorted(PALETTE_IDS),
        "sweep_loop_ids": sorted(SWEEP_IDS),
        "rhythm_loop_ids": sorted(RHYTHM_IDS),
        "changed_scene_ids": sorted(changed_scenes),
        "changed_scene_count": len(changed_scenes),
        "focus_exclude_fade_fixture_ids": excluded_ids,
        "focus_excluded_fade_channels": list(EXCLUDED_FADE_CHANNELS),
        "public_loop_ids_timing_and_owners_preserved": True,
    }
