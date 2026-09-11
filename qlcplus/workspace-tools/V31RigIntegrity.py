#!/usr/bin/env python3
"""Add missing private-layer control payloads without redesigning the V30 show.

QLC+ merges Scenes separately on each universe.  The SoundSwitch plug-in then
selects the private Priority universe wholesale, so a physical-only performance
or position Scene cannot affect an active Priority Look.  This module adds the
same existing values to the corresponding private fixtures.  It does not alter
pan/tilt calibration, intensity, color, timing, fixture definitions, or routing.

``repair(root)`` mutates an ElementTree workspace root.  ``validate(source,
candidate)`` independently checks the permitted Scene-payload delta and rig
integrity.  The containing V31 validator owns control-widget/runtime semantics.
"""

from __future__ import annotations

import copy
from pathlib import Path
import xml.etree.ElementTree as ET

NS = "{http://www.qlcplus.org/Workspace}"
PERFORMANCE_IDS = frozenset(range(5))
POSITION_IDS = frozenset(range(2175, 2184))
MIRROR_IDS = PERFORMANCE_IDS | POSITION_IDS
PHYSICAL_IDS = frozenset(range(11))
PRIVATE_IDS = frozenset(range(100, 111))
RAW_IDS = frozenset(range(532, 660))
PRIORITY_IDS = frozenset(range(5, 37))
# Zero-based address, channel count, exact QLC+ mode.
PATCH = {
    0: (0, 10, "10 Channel"), 1: (10, 10, "10 Channel"),
    2: (20, 10, "10 Channel"), 3: (30, 10, "10 Channel"),
    4: (40, 40, "40 Channel"),
    5: (174, 40, "40 Channel"), 6: (214, 40, "40 Channel"),
    7: (254, 40, "40 Channel"), 8: (294, 40, "40 Channel"),
    9: (80, 18, "18 Channel"), 10: (98, 18, "18 Channel"),
}


def q(name: str) -> str:
    return NS + name


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def semantic(node: ET.Element):
    return (node.tag, tuple(sorted(node.attrib.items())),
            (node.text or "").strip(), tuple(semantic(child) for child in node))


def engine(root: ET.Element) -> ET.Element:
    result = root.find(q("Engine"))
    require(result is not None, "V31 rig: missing Engine")
    return result


def id_map(root: ET.Element, tag: str) -> dict[int, ET.Element]:
    result = {}
    for node in engine(root).findall(q(tag)):
        node_id = int(node.findtext(q("ID"), "-1") if tag == "Fixture"
                      else node.get("ID", "-1"))
        require(node_id >= 0 and node_id not in result,
                f"V31 rig: invalid or duplicate {tag} ID {node_id}")
        result[node_id] = node
    return result


def payload(scene: ET.Element) -> dict[int, ET.Element]:
    result = {}
    for node in scene.findall(q("FixtureVal")):
        fixture_id = int(node.get("ID", "-1"))
        require(fixture_id not in result,
                f"V31 rig: Scene {scene.get('ID')} repeats fixture {fixture_id}")
        result[fixture_id] = node
    return result


def repair(root: ET.Element) -> None:
    functions = id_map(root, "Function")
    for fid in sorted(MIRROR_IDS):
        scene = functions[fid]
        require(scene.get("Type") == "Scene", f"V31 rig: {fid} is not a Scene")
        values = payload(scene)
        expected = PHYSICAL_IDS if fid in PERFORMANCE_IDS else frozenset({9, 10})
        require(set(values) == expected,
                f"V31 rig: unexpected source payload for Scene {fid}")
        for fixture_id in sorted(expected):
            clone = copy.deepcopy(values[fixture_id])
            clone.set("ID", str(fixture_id + 100))
            scene.append(clone)


def parse_values(scene: ET.Element, fixtures: dict[int, ET.Element]):
    result = {}
    for fixture_id, node in payload(scene).items():
        require(fixture_id in fixtures,
                f"V31 rig: Scene {scene.get('ID')} references missing fixture {fixture_id}")
        tokens = (node.text or "").strip().split(",")
        require(len(tokens) % 2 == 0 and tokens != [""],
                f"V31 rig: malformed FixtureVal in Scene {scene.get('ID')}")
        # Some protected dormant Scenes retain legacy integral decimals.  QLC+
        # source parsing was validated in V27; do not rewrite their payloads.
        numbers = [float(token) for token in tokens]
        require(all(number.is_integer() for number in numbers),
                f"V31 rig: non-integral DMX in Scene {scene.get('ID')}")
        values = {}
        count = int(fixtures[fixture_id].findtext(q("Channels"), "-1"))
        for channel, value in zip(numbers[::2], numbers[1::2]):
            channel, value = int(channel), int(value)
            require(0 <= channel < count and channel not in values and 0 <= value <= 255,
                    f"V31 rig: invalid DMX pair in Scene {scene.get('ID')}, fixture {fixture_id}")
            values[channel] = value
        result[fixture_id] = values
    return result


def scene_leaves(functions: dict[int, ET.Element], roots):
    visited, leaves = set(), set()
    def visit(fid):
        require(fid in functions, f"V31 rig: missing Function reference {fid}")
        if fid in visited:
            return
        visited.add(fid)
        fn = functions[fid]
        if fn.get("Type") == "Scene":
            leaves.add(fid)
        for step in fn.findall(q("Step")):
            visit(int(step.text or "-1"))
    for fid in roots:
        visit(fid)
    return leaves


def validate_ir4_definition(path: Path | None = None) -> None:
    """Check the manufacturer-backed dependency required by all eight IR-4s."""
    path = path or (Path(__file__).resolve().parents[1] / "fixture-definitions" /
                    "Both-Lighting-IR-4-(BOIR4).qxf")
    root = ET.parse(path).getroot()
    fq = lambda name: "{http://www.qlcplus.org/FixtureDefinition}" + name
    require(root.tag == fq("FixtureDefinition") and
            root.findtext(fq("Manufacturer")) == "Both Lighting" and
            root.findtext(fq("Model")) == "IR-4 (BOIR4)" and
            root.findtext(fq("Type")) == "Color Changer",
            "V31 rig: IR-4 definition identity is wrong")
    channels = root.findall(fq("Channel"))
    expected_names = ("Master Dimmer", "Red", "Green", "Blue", "White", "Amber",
                      "Purple / UV", "Strobe", "Program / Macro (Show Uses 0)",
                      "Color Selection / Speed (Show Uses 0)")
    require(tuple(ch.get("Name") for ch in channels) == expected_names,
            "V31 rig: IR-4 channel order differs from manufacturer 10-channel table")
    presets = ("IntensityMasterDimmer", "IntensityRed", "IntensityGreen",
               "IntensityBlue", "IntensityWhite", "IntensityAmber", "IntensityUV")
    require(tuple(ch.get("Preset") for ch in channels[:7]) == presets,
            "V31 rig: IR-4 emitter/master semantics changed")
    require(all(ch.get("Default") == "0" for ch in channels),
            "V31 rig: IR-4 definition must default every channel to zero")
    for channel, group in zip(channels[7:], ("Shutter", "Effect", "Effect")):
        require(channel.get("Preset") is None and channel.findtext(fq("Group")) == group,
                "V31 rig: IR-4 raw control classified as an emitter or assumed preset")
        capabilities = channel.findall(fq("Capability"))
        require(len(capabilities) == 1 and capabilities[0].get("Min") == "0" and
                capabilities[0].get("Max") == "255" and
                capabilities[0].get("Preset") is None,
                "V31 rig: IR-4 raw control acquired unreviewed subranges")
    modes = root.findall(fq("Mode"))
    require(len(modes) == 1 and modes[0].get("Name") == "10 Channel",
            "V31 rig: IR-4 required 10 Channel mode is missing")
    bindings = modes[0].findall(fq("Channel"))
    require(tuple((ch.get("Number"), ch.text) for ch in bindings) ==
            tuple((str(i), name) for i, name in enumerate(expected_names)),
            "V31 rig: IR-4 mode channel numbers/references are wrong")
    heads = modes[0].findall(fq("Head"))
    require(len(heads) == 1 and [ch.text for ch in heads[0].findall(fq("Channel"))] ==
            [str(i) for i in range(8)], "V31 rig: IR-4 head mapping is wrong")


def validate(source: ET.Element, candidate: ET.Element) -> dict[str, int]:
    validate_ir4_definition()
    before_fixtures, fixtures = id_map(source, "Fixture"), id_map(candidate, "Fixture")
    require(set(fixtures) == PHYSICAL_IDS | PRIVATE_IDS,
            "V31 rig: physical/private fixture set changed")
    require(set(before_fixtures) == set(fixtures), "V31 rig: fixture IDs changed")
    occupied = {0: set(), 2: set()}
    for fixture_id, fixture in fixtures.items():
        require(semantic(fixture) == semantic(before_fixtures[fixture_id]),
                f"V31 rig: fixture {fixture_id} changed")
        physical_id = fixture_id if fixture_id < 100 else fixture_id - 100
        address, channels, mode = PATCH[physical_id]
        universe = 0 if fixture_id < 100 else 2
        require(fixture.findtext(q("Universe")) == str(universe) and
                fixture.findtext(q("Address")) == str(address) and
                fixture.findtext(q("Channels")) == str(channels) and
                fixture.findtext(q("Mode")) == mode,
                f"V31 rig: fixture {fixture_id} patch/mode is wrong")
        span = set(range(address, address + channels))
        require(not occupied[universe] & span, f"V31 rig: overlapping fixture {fixture_id}")
        occupied[universe].update(span)

    before, functions = id_map(source, "Function"), id_map(candidate, "Function")
    require(set(before) <= set(functions), "V31 rig: source Function ID removed")
    for fid, original in before.items():
        if original.get("Type") != "Scene":
            continue
        changed = functions[fid]
        require(changed.get("Type") == "Scene", f"V31 rig: Scene {fid} type changed")
        original_values, changed_values = payload(original), payload(changed)
        if fid not in MIRROR_IDS:
            require([semantic(node) for node in original_values.values()] ==
                    [semantic(node) for node in changed_values.values()],
                    f"V31 rig: creative Scene {fid} payload changed")
            continue
        expected = PHYSICAL_IDS if fid in PERFORMANCE_IDS else frozenset({9, 10})
        require(set(original_values) == expected and
                set(changed_values) == expected | {i + 100 for i in expected},
                f"V31 rig: Scene {fid} does not have exactly its physical/private pairs")
        for fixture_id in expected:
            require(semantic(original_values[fixture_id]) == semantic(changed_values[fixture_id]),
                    f"V31 rig: Scene {fid} changed physical fixture {fixture_id}")
            clone = copy.deepcopy(changed_values[fixture_id + 100])
            clone.set("ID", str(fixture_id))
            require(semantic(clone) == semantic(original_values[fixture_id]),
                    f"V31 rig: Scene {fid} private fixture {fixture_id + 100} differs")

    all_values = {fid: parse_values(fn, fixtures) for fid, fn in functions.items()
                  if fn.get("Type") == "Scene"}
    raw, priority = scene_leaves(functions, RAW_IDS), scene_leaves(functions, PRIORITY_IDS)
    require(len(raw) == 1024 and len(priority) == 102,
            "V31 rig: live Autoloop/Priority creative closure changed")
    for leaves, expected in ((raw, PHYSICAL_IDS), (priority, PRIVATE_IDS),
                             (PERFORMANCE_IDS, PHYSICAL_IDS | PRIVATE_IDS)):
        for fid in leaves:
            values = all_values[fid]
            require(set(values) == expected, f"V31 rig: Scene {fid} full-rig coverage incomplete")
            for fixture_id, frame in values.items():
                count = int(fixtures[fixture_id].findtext(q("Channels")))
                require(set(frame) == set(range(count)),
                        f"V31 rig: Scene {fid} fixture {fixture_id} is not a complete frame")

    # An invalid reference/value in a dormant Scene is still an integrity error.
    # The new fixture safety contract applies to every emitted Scene payload.
    for fid, values in all_values.items():
        for fixture_id in {0, 1, 2, 3, 100, 101, 102, 103} & set(values):
            require(all(values[fixture_id].get(ch, 0) == 0 for ch in (7, 8, 9)),
                    f"V31 rig: Scene {fid} enables unqualified IR-4 strobe/internal program")
        for fixture_id in {4, 104} & set(values):
            require(all(values[fixture_id].get(ch, 0) == 0 for ch in range(4)),
                    f"V31 rig: Scene {fid} enables Wash internal program/strobe")
        for fixture_id in {9, 10, 109, 110} & set(values):
            require(all(values[fixture_id].get(ch, 0) == 0 for ch in (10, 11, 13, 14, 15, 17)),
                    f"V31 rig: Scene {fid} enables Focus UV/internal show/reset")

    black = all_values[0]
    for fixture_id, frame in black.items():
        physical_id = fixture_id % 100
        emitters = (range(10) if physical_id <= 3 else
                    range(4, 40) if physical_id == 4 else
                    range(40) if 5 <= physical_id <= 8 else (8, 9, 10, 11))
        require(all(frame.get(ch) == 0 for ch in emitters),
                f"V31 rig: Blackout leaves fixture {fixture_id} emitting")
    for fid in POSITION_IDS:
        require(all(set(frame) == {0, 1, 2, 3} for frame in all_values[fid].values()),
                f"V31 rig: position Scene {fid} takes non-position ownership")

    for fid in (*range(37, 46), *range(2185, 2194)):
        require(set(all_values[fid]) == PHYSICAL_IDS | PRIVATE_IDS,
                f"V31 rig: color override {fid} lost physical/private coverage")

    before_io = engine(source).find(q("InputOutputMap"))
    after_io = engine(candidate).find(q("InputOutputMap"))
    require(before_io is not None and after_io is not None and
            semantic(before_io) == semantic(after_io), "V31 rig: I/O routing changed")
    private = next((u for u in after_io.findall(q("Universe")) if u.get("ID") == "2"), None)
    require(private is not None, "V31 rig: private universe missing")
    outputs = private.findall(q("Output"))
    require(len(outputs) == 1 and outputs[0].get("Plugin") == "SoundSwitch Hardware" and
            outputs[0].get("UID") == "soundswitch:priority-layer",
            "V31 rig: private Priority universe must have only its internal output")
    return {"fixtures": len(fixtures), "raw_scenes": len(raw),
            "priority_scenes": len(priority), "mirrored_scenes": len(MIRROR_IDS),
            "added_private_payloads": 5 * 11 + 9 * 2}
