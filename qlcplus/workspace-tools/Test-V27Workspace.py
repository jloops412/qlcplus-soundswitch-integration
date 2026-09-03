#!/usr/bin/env python3
"""Validate a V27 full-rig workspace against the immutable V26 release.

The check is intentionally independent from the creative provider used by the
builder.  It proves fixture patch/pair identity, public Function and control
stability, complete live creative coverage, reference/ID integrity, safe
channel constraints, and non-trivial full-rig programming.  It does not claim
physical output or aesthetic/gig qualification.
"""

from __future__ import annotations

import argparse
from collections import Counter
import copy
import hashlib
from pathlib import Path
import re
import sys
from typing import Any, Iterable, Mapping, Sequence
import xml.etree.ElementTree as ET


NS = "http://www.qlcplus.org/Workspace"
FIXTURE_NS = "http://www.qlcplus.org/FixtureDefinition"
EXPECTED_V26_SHA256 = (
    "ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570"
)
EXPECTED_SOURCE_FUNCTION_COUNT = 2090
EXPECTED_ADDITIVE_FUNCTION_IDS = frozenset(range(2175, 2185))
EXPECTED_CANDIDATE_FUNCTION_COUNT = EXPECTED_SOURCE_FUNCTION_COUNT + len(EXPECTED_ADDITIVE_FUNCTION_IDS)
EXPECTED_CANDIDATE_FUNCTION_TYPES = {"Scene": 1812, "Chaser": 150, "Collection": 138}
EXPECTED_POSITION_FUNCTIONS = {
    2175: ("Crossed Out Down", ((46027, 8286), (40693, 6629))),
    2176: ("Crossed In Down", ((50752, 6930), (53342, 3917))),
    2177: ("Stage Right", ((35663, 7231), (33682, 3917))),
    2178: ("Stage Left", ((20727, 65384), (23013, 63878))),
    2179: ("Straight Ahead", ((39473, 2260), (47094, 1055))),
    2180: ("Crossed Out Up", ((40693, 2561), (45875, 1356))),
    2181: ("Crossed In Up", ((39626, 3314), (47094, 1356))),
    2182: ("Up", ((45417, 15066), (40997, 13107))),
    2183: ("Down", ((50142, 8587), (49532, 6629))),
}
EXPECTED_MOVEMENT_CHASER_NAME = "MOVEMENT — FOCUS A/B SWEEP"
POSITION_INPUT_CHANNELS = tuple(range(164, 173))

RAW_LOOP_IDS = tuple(range(532, 660))
MANUAL_OWNER_IDS = tuple(range(660, 788))
AUTOPLAY_PARENT_IDS = tuple(range(788, 798))
AUTOPLAY_OWNER_IDS = tuple(range(798, 808))
PRIORITY_ROOT_IDS = tuple(range(5, 37))
PRIORITY_CHASER_IDS = frozenset({12, 20, 21, 29, 30, 32, 33, 34, 35, 36})
PERFORMANCE_SCENE_IDS = frozenset(range(0, 5))
OVERRIDE_SCENE_IDS = frozenset(range(37, 46))
UI_CONTROL_SCENE_IDS = frozenset(range(1982, 1999))

EXPECTED_SOURCE_FIXTURE_IDS = frozenset({
    0, 1, 2, 3, 5, 6, 7, 8,
    100, 101, 102, 103, 105, 106, 107, 108,
})
EXPECTED_PHYSICAL_IDS = frozenset(range(0, 11))
EXPECTED_PRIVATE_IDS = frozenset(range(100, 111))
EXPECTED_CANDIDATE_FIXTURE_IDS = EXPECTED_PHYSICAL_IDS | EXPECTED_PRIVATE_IDS
NEW_PHYSICAL_IDS = frozenset({4, 9, 10})
NEW_PRIVATE_IDS = frozenset({104, 109, 110})
EXPECTED_NEW_FIXTURE_NAMES = {
    4: "Wash FX Hex — Dance Floor",
    9: "Focus Spot Two — A",
    10: "Focus Spot Two — B",
}

# QLC+ stores zero-based addresses.  These tuples preserve every V26 address,
# fill 41-116 (one-based), and leave 117-174 reserved before the tubes.
EXPECTED_PATCH = {
    0: ("Both Lighting", "IR-4 (BOIR4)", "10 Channel", 0, 0, 10),
    1: ("Both Lighting", "IR-4 (BOIR4)", "10 Channel", 0, 10, 10),
    2: ("Both Lighting", "IR-4 (BOIR4)", "10 Channel", 0, 20, 10),
    3: ("Both Lighting", "IR-4 (BOIR4)", "10 Channel", 0, 30, 10),
    4: ("Chauvet", "Wash FX Hex", "40 Channel", 0, 40, 40),
    5: ("Both Lighting", "BO-TUBE192", "40 Channel", 0, 174, 40),
    6: ("Both Lighting", "BO-TUBE192", "40 Channel", 0, 214, 40),
    7: ("Both Lighting", "BO-TUBE192", "40 Channel", 0, 254, 40),
    8: ("Both Lighting", "BO-TUBE192", "40 Channel", 0, 294, 40),
    9: ("American DJ", "Focus Spot Two", "18 Channel", 0, 80, 18),
    10: ("American DJ", "Focus Spot Two", "18 Channel", 0, 98, 18),
}
for _physical_id, _tuple in list(EXPECTED_PATCH.items()):
    EXPECTED_PATCH[_physical_id + 100] = (*_tuple[:3], 2, *_tuple[4:])

WASH_IDS = frozenset({4, 104})
FOCUS_IDS = frozenset({9, 10, 109, 110})
WASH_DIRECT_COLOR_CHANNELS = frozenset(range(4, 40))
FOCUS_COLOR_CHANNELS = frozenset({4})
FOCUS_SAFE_ZERO_CHANNELS = frozenset({13, 14, 15, 17})
EXPECTED_FOCUS_CHANNEL_NAMES = (
    "Pan Movement", "Pan Fine", "Tilt Movement", "Tilt Fine", "Color Wheel",
    "Gobo Wheel", "Gobo Rotation", "Prism", "Main Strobe/Shutter",
    "Main Master Dimmer", "UV Strobe/Shutter", "UV Master Dimmer", "Focus",
    "Show", "Show Speed", "Dimmer Modes", "Pan/Tilt Speed", "Function",
)

PERSONAL_LEAK_PATTERN = re.compile(
    r"C:\\Users\\|(?:^|[\s'\"])/Users/|(?:^|[\s'\"])/home/|"
    r"(?:^|[\s'\"])/workspace/|\bjloop\b|\bJ:\\",
    re.IGNORECASE | re.MULTILINE,
)


class ValidationError(RuntimeError):
    """A V27 structural or regression invariant failed."""


def q(name: str) -> str:
    return f"{{{NS}}}{name}"


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def canonical(element: ET.Element | None, *, drop_direct: frozenset[str] = frozenset()) -> tuple[Any, ...]:
    require(element is not None, "A required XML element is missing.")
    assert element is not None
    children = []
    for child in list(element):
        if local_name(child.tag) in drop_direct:
            continue
        children.append(canonical(child))
    return (
        element.tag,
        tuple(sorted(element.attrib.items())),
        (element.text or "").strip(),
        tuple(children),
    )


def io_operational_signature(element: ET.Element | None) -> tuple[Any, ...]:
    require(element is not None, "Input/Output map is missing.")
    assert element is not None
    clone = copy.deepcopy(element)
    for node in clone.iter():
        node.attrib.pop("Name", None)
    return canonical(clone)


def validate_workspace_shell(source: ET.Element, candidate: ET.Element) -> None:
    expected_children = ["Creator", "Engine", "VirtualConsole"]
    require([local_name(child.tag) for child in source] == expected_children,
            "Pinned V26 top-level structure is unexpected.")
    require([local_name(child.tag) for child in candidate] == expected_children,
            "V27 top-level structure changed.")
    require(source.tag == candidate.tag and source.attrib == candidate.attrib,
            "Workspace root settings changed.")
    require(canonical(source.find(q("Creator"))) == canonical(candidate.find(q("Creator"))),
            "Workspace Creator metadata changed.")


def widget_map(virtual_console: ET.Element) -> dict[int, ET.Element]:
    result: dict[int, ET.Element] = {}
    for node in virtual_console.iter():
        raw_id = node.get("ID")
        if raw_id is None or node.find(q("WindowState")) is None:
            continue
        require(raw_id.isdigit(), f"Malformed Virtual Console widget ID {raw_id!r}.")
        widget_id = int(raw_id)
        require(widget_id not in result, f"Duplicate Virtual Console widget ID {widget_id}.")
        result[widget_id] = node
    return result


def widget_binding_signature(widget: ET.Element, *, normalise_move_target: bool = False) -> tuple[Any, ...]:
    """Return bindings owned by this widget without absorbing child widgets."""

    bindings = []

    def visit(node: ET.Element) -> None:
        for child in node:
            if child.get("ID") is not None and child.find(q("WindowState")) is not None:
                continue
            if local_name(child.tag) in {"Function", "Input", "Action", "Intensity"}:
                clone = copy.deepcopy(child)
                if (
                    normalise_move_target
                    and local_name(clone.tag) == "Function"
                    and clone.get("ID") == "2184"
                ):
                    clone.set("ID", "4294967295")
                bindings.append(canonical(clone))
            else:
                visit(child)

    visit(widget)
    return tuple(bindings)


def load_workspace(path: Path) -> tuple[ET.Element, ET.Element, ET.Element]:
    require(path.is_file(), f"Workspace is missing: {path}")
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        raise ValidationError(f"Unable to parse workspace {path}: {exc}") from exc
    require(local_name(root.tag) == "Workspace", f"{path} has no Workspace root.")
    engine = root.find(q("Engine"))
    virtual_console = root.find(q("VirtualConsole"))
    require(engine is not None, f"{path} has no Engine.")
    require(virtual_console is not None, f"{path} has no VirtualConsole.")
    assert engine is not None and virtual_console is not None
    return root, engine, virtual_console


def validate_focus_definition(path: Path) -> None:
    require(path.is_file(), f"Custom Focus Spot Two fixture definition is missing: {path}")
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        raise ValidationError(f"Unable to parse Focus Spot Two fixture definition: {exc}") from exc
    fq = lambda name: f"{{{FIXTURE_NS}}}{name}"
    require(local_name(root.tag) == "FixtureDefinition", "Focus Spot Two .qxf has the wrong root.")
    require(root.findtext(fq("Manufacturer"), "") == "American DJ", "Focus Spot Two .qxf manufacturer is wrong.")
    require(root.findtext(fq("Model"), "") == "Focus Spot Two", "Focus Spot Two .qxf model is wrong.")
    mode = next((item for item in root.findall(fq("Mode")) if item.get("Name") == "18 Channel"), None)
    require(mode is not None, "Focus Spot Two .qxf has no 18 Channel mode.")
    assert mode is not None
    channels: dict[int, str] = {}
    for channel in mode.findall(fq("Channel")):
        raw_number = channel.get("Number", "")
        require(raw_number.isdigit(), f"Focus Spot Two mode has malformed channel number {raw_number!r}.")
        number = int(raw_number)
        require(number not in channels, f"Focus Spot Two mode repeats channel {number}.")
        channels[number] = (channel.text or "").strip()
    require(tuple(channels) == tuple(range(18)), "Focus Spot Two mode must cover channels 0-17 exactly once.")
    require(tuple(channels[index] for index in range(18)) == EXPECTED_FOCUS_CHANNEL_NAMES,
            "Focus Spot Two mode channel order does not match the reviewed 18-channel manual map.")


def function_map(engine: ET.Element) -> dict[int, ET.Element]:
    result: dict[int, ET.Element] = {}
    for function in engine.findall(q("Function")):
        raw_id = function.get("ID", "")
        require(raw_id.isdigit(), f"Malformed Function ID {raw_id!r}.")
        function_id = int(raw_id)
        require(function_id not in result, f"Duplicate Function ID {function_id}.")
        result[function_id] = function
    return result


def fixture_map(engine: ET.Element) -> dict[int, ET.Element]:
    result: dict[int, ET.Element] = {}
    for fixture in engine.findall(q("Fixture")):
        raw_id = (fixture.findtext(q("ID")) or "").strip()
        require(raw_id.isdigit(), f"Malformed Fixture ID {raw_id!r}.")
        fixture_id = int(raw_id)
        require(fixture_id not in result, f"Duplicate Fixture ID {fixture_id}.")
        result[fixture_id] = fixture
    return result


def step_references(function: ET.Element) -> list[int]:
    references: list[int] = []
    seen_numbers: set[int] = set()
    for position, step in enumerate(function.findall(q("Step"))):
        raw = (step.text or "").strip()
        require(raw.isdigit(), f"Function {function.get('ID')} has malformed Step reference {raw!r}.")
        number = step.get("Number")
        if number is not None:
            require(number.isdigit(), f"Function {function.get('ID')} has malformed Step Number {number!r}.")
            parsed_number = int(number)
            require(parsed_number not in seen_numbers, f"Function {function.get('ID')} repeats Step {parsed_number}.")
            seen_numbers.add(parsed_number)
            require(parsed_number == position, f"Function {function.get('ID')} Step numbering is not consecutive.")
        references.append(int(raw))
    return references


def validate_function_references(functions: Mapping[int, ET.Element]) -> None:
    for owner_id, function in functions.items():
        for target_id in step_references(function):
            require(target_id in functions, f"Function {owner_id} references missing Function {target_id}.")


def closure(functions: Mapping[int, ET.Element], roots: Iterable[int]) -> set[int]:
    visited: set[int] = set()
    active: set[int] = set()

    def visit(function_id: int) -> None:
        require(function_id in functions, f"Required Function {function_id} is missing.")
        if function_id in visited:
            return
        require(function_id not in active, f"Function reference cycle reaches {function_id}.")
        active.add(function_id)
        for target_id in step_references(functions[function_id]):
            visit(target_id)
        active.remove(function_id)
        visited.add(function_id)

    for root_id in roots:
        visit(root_id)
    return visited


def scene_ids_in(functions: Mapping[int, ET.Element], ids: Iterable[int]) -> set[int]:
    return {function_id for function_id in ids if functions[function_id].get("Type") == "Scene"}


def fixture_tuple(fixture: ET.Element) -> tuple[str, str, str, int, int, int]:
    return (
        fixture.findtext(q("Manufacturer"), ""),
        fixture.findtext(q("Model"), ""),
        fixture.findtext(q("Mode"), ""),
        int(fixture.findtext(q("Universe"), "-1")),
        int(fixture.findtext(q("Address"), "-1")),
        int(fixture.findtext(q("Channels"), "-1")),
    )


def fixture_values(scene: ET.Element, fixtures: Mapping[int, ET.Element]) -> dict[int, dict[int, int]]:
    result: dict[int, dict[int, int]] = {}
    for fixture_value in scene.findall(q("FixtureVal")):
        raw_id = fixture_value.get("ID", "")
        require(raw_id.isdigit(), f"Scene {scene.get('ID')} has malformed FixtureVal ID {raw_id!r}.")
        fixture_id = int(raw_id)
        require(fixture_id in fixtures, f"Scene {scene.get('ID')} targets missing Fixture {fixture_id}.")
        require(fixture_id not in result, f"Scene {scene.get('ID')} repeats FixtureVal {fixture_id}.")
        raw_text = (fixture_value.text or "").strip()
        require(raw_text and not raw_text.startswith(",") and not raw_text.endswith(","),
                f"Scene {scene.get('ID')} FixtureVal {fixture_id} is empty/malformed.")
        tokens = [token.strip() for token in raw_text.split(",")]
        require(
            len(tokens) % 2 == 0 and all(token.isdigit() for token in tokens),
            f"Scene {scene.get('ID')} FixtureVal {fixture_id} is malformed.",
        )
        channel_count = int(fixtures[fixture_id].findtext(q("Channels"), "-1"))
        channel_values: dict[int, int] = {}
        for offset in range(0, len(tokens), 2):
            channel = int(tokens[offset])
            value = int(tokens[offset + 1])
            require(channel not in channel_values, f"Scene {scene.get('ID')} repeats fixture {fixture_id} channel {channel}.")
            require(0 <= channel < channel_count, f"Scene {scene.get('ID')} fixture {fixture_id} channel {channel} is outside its mode.")
            require(0 <= value <= 255, f"Scene {scene.get('ID')} fixture {fixture_id} has value {value} outside 0..255.")
            channel_values[channel] = value
        result[fixture_id] = channel_values
    return result


def complete_frame(values: Mapping[int, int], channels: int) -> tuple[int, ...]:
    require(set(values) == set(range(channels)), f"Expected a complete {channels}-channel frame.")
    return tuple(values[index] for index in range(channels))


def validate_fixture_patch(
    source_fixtures: Mapping[int, ET.Element],
    fixtures: Mapping[int, ET.Element],
) -> None:
    require(set(source_fixtures) == EXPECTED_SOURCE_FIXTURE_IDS, "The pinned V26 fixture-ID set is unexpected.")
    require(set(fixtures) == EXPECTED_CANDIDATE_FIXTURE_IDS, "V27 must contain exactly 11 physical and 11 private fixtures.")

    for fixture_id, source_fixture in source_fixtures.items():
        require(canonical(fixtures[fixture_id]) == canonical(source_fixture), f"Existing Fixture {fixture_id} changed from V26.")
    for fixture_id, expected in EXPECTED_PATCH.items():
        require(fixture_tuple(fixtures[fixture_id]) == expected, f"Fixture {fixture_id} patch/profile tuple is wrong.")
    for fixture_id, expected_name in EXPECTED_NEW_FIXTURE_NAMES.items():
        require(fixtures[fixture_id].findtext(q("Name"), "") == expected_name,
                f"Fixture {fixture_id} operator name is wrong.")

    for physical_id in sorted(EXPECTED_PHYSICAL_IDS):
        private_id = physical_id + 100
        physical = fixtures[physical_id]
        private = fixtures[private_id]
        physical_tuple = fixture_tuple(physical)
        private_tuple = fixture_tuple(private)
        require(physical_tuple[3] == 0 and private_tuple[3] == 2, f"Fixture pair {physical_id}/{private_id} is on the wrong universe.")
        require(
            physical_tuple[:3] + physical_tuple[4:] == private_tuple[:3] + private_tuple[4:],
            f"Private Fixture {private_id} is not an exact patch/mode duplicate of {physical_id}.",
        )
        physical_name = physical.findtext(q("Name"), "")
        private_name = private.findtext(q("Name"), "")
        require(private_name == f"{physical_name} — Priority Layer", f"Private Fixture {private_id} has the wrong name.")

    spans: dict[int, list[tuple[int, int, int]]] = {}
    for fixture_id, fixture in fixtures.items():
        _, _, _, universe, address, channels = fixture_tuple(fixture)
        require(0 <= address < 512 and 1 <= channels <= 512 and address + channels <= 512,
                f"Fixture {fixture_id} has an invalid DMX span.")
        spans.setdefault(universe, []).append((address, address + channels, fixture_id))
    for universe, universe_spans in spans.items():
        universe_spans.sort()
        for previous, current in zip(universe_spans, universe_spans[1:]):
            require(previous[1] <= current[0], f"Universe {universe + 1} fixtures {previous[2]} and {current[2]} overlap.")

    for universe in (0, 2):
        require(
            all(end <= 116 or start >= 174 for start, end, _ in spans[universe]),
            f"Universe {universe + 1} reserved addresses 117-174 are not clear.",
        )


def validate_input_output(engine: ET.Element, source_engine: ET.Element) -> None:
    candidate_map = engine.find(q("InputOutputMap"))
    source_map = source_engine.find(q("InputOutputMap"))
    require(
        io_operational_signature(candidate_map) == io_operational_signature(source_map),
        "Input/Output operational topology changed from V26; only labels may change.",
    )
    assert candidate_map is not None

    universes = {
        int(universe.get("ID", "-1")): universe
        for universe in candidate_map.findall(q("Universe"))
    }
    require(0 in universes and 2 in universes, "Physical or private universe mapping is missing.")
    for universe_id in (0, 2):
        label = universes[universe_id].get("Name", "").upper()
        require(
            "WASH" in label and "FOCUS" in label,
            f"Universe {universe_id + 1} operator label does not identify the Wash/Focus full rig.",
        )
    for universe_id in (0, 2):
        outputs = universes[universe_id].findall(q("Output"))
        require(outputs, f"Universe {universe_id + 1} has no output declaration.")
        for output in outputs:
            parameters = output.find(q("PluginParameters"))
            require(parameters is not None, f"Universe {universe_id + 1} output has no channel capacity.")
            assert parameters is not None
            require(int(parameters.get("UniverseChannels", "0")) >= 334,
                    f"Universe {universe_id + 1} output does not carry the full 334-channel rig frame.")
    private_outputs = universes[2].findall(q("Output"))
    require(len(private_outputs) == 1, "Private Universe 3 must have exactly one internal output.")
    require(
        private_outputs[0].get("UID") == "soundswitch:priority-layer"
        and private_outputs[0].get("Line") == "4",
        "Universe 3 is not isolated on the internal Priority layer.",
    )


def validate_monitor(engine: ET.Element, source_engine: ET.Element) -> None:
    monitor = engine.find(q("Monitor"))
    source = source_engine.find(q("Monitor"))
    require(monitor is not None and source is not None, "Monitor configuration is missing.")
    assert monitor is not None and source is not None
    require(monitor.attrib == source.attrib, "Monitor root settings changed.")
    source_non_items = [canonical(child) for child in source if child.tag not in {q("FxItem"), q("Grid")}]
    candidate_non_items = [canonical(child) for child in monitor if child.tag not in {q("FxItem"), q("Grid")}]
    require(source_non_items == candidate_non_items, "Monitor display settings changed.")
    source_grid = source.find(q("Grid"))
    candidate_grid = monitor.find(q("Grid"))
    require(source_grid is not None and candidate_grid is not None, "Monitor Grid is missing.")
    assert source_grid is not None and candidate_grid is not None
    for attribute in ("Units", "POV"):
        require(candidate_grid.get(attribute) == source_grid.get(attribute),
                f"Monitor Grid {attribute} changed.")
    for attribute in ("Width", "Height", "Depth"):
        source_size = int(source_grid.get(attribute, "0"))
        candidate_size = int(candidate_grid.get(attribute, "0"))
        require(source_size <= candidate_size <= 20,
                f"Monitor Grid {attribute} is not a sensible expansion.")

    items: dict[int, tuple[int, int, int]] = {}
    for item in monitor.findall(q("FxItem")):
        require(set(item.attrib) == {"ID", "XPos", "YPos", "ZPos"},
                "Monitor FxItem changes must be position-only.")
        raw_id = item.get("ID", "")
        require(raw_id.isdigit(), f"Monitor has malformed FxItem ID {raw_id!r}.")
        fixture_id = int(raw_id)
        require(fixture_id not in items, f"Monitor repeats FxItem {fixture_id}.")
        try:
            position = tuple(int(item.get(attribute, "")) for attribute in ("XPos", "YPos", "ZPos"))
        except ValueError as exc:
            raise ValidationError(f"Monitor item {fixture_id} has a malformed position.") from exc
        require(all(-10 <= coordinate <= 10 for coordinate in position),
                f"Monitor item {fixture_id} position {position} is outside the visual bench.")
        items[fixture_id] = position  # type: ignore[assignment]
    require(set(items) == EXPECTED_PHYSICAL_IDS, "Monitor must contain every physical fixture exactly once and no private fixture.")
    require(len(set(items.values())) == len(items), "Visual-bench fixture positions overlap.")


def validate_virtual_console(
    virtual_console: ET.Element,
    source_virtual_console: ET.Element,
    functions: Mapping[int, ET.Element],
) -> int:
    source_widgets = widget_map(source_virtual_console)
    candidate_widgets = widget_map(virtual_console)
    require(set(source_widgets) <= set(candidate_widgets), "A V26 Virtual Console widget was removed.")
    for widget_id, source_widget in source_widgets.items():
        require(
            widget_binding_signature(candidate_widgets[widget_id], normalise_move_target=(widget_id == 1004))
            == widget_binding_signature(source_widget),
            f"Existing widget {widget_id} changed its public binding/action.",
        )
    new_widget_ids = set(candidate_widgets) - set(source_widgets)
    require(
        all(widget_id > max(source_widgets) for widget_id in new_widget_ids),
        "New Virtual Console widgets must use IDs above the V26 allocator ceiling.",
    )
    move_target = candidate_widgets[1004].find(q("Function"))
    require(move_target is not None and move_target.get("ID") == "2184",
            "Existing MOVE widget 1004 must target additive movement Chaser 2184.")
    captions = " ".join(node.get("Caption", "") for node in virtual_console.iter()).upper()
    require("WASH" in captions and "FOCUS" in captions,
            "Virtual Console/manual page does not identify the Wash and Focus full rig.")

    invalid_function_id = 4294967295
    for reference in virtual_console.iter(q("Function")):
        raw = reference.get("ID")
        if raw is None:
            raw = (reference.text or "").strip()
        require(raw.isdigit(), f"Malformed Virtual Console Function reference {raw!r}.")
        function_id = int(raw)
        require(
            function_id == invalid_function_id or function_id in functions,
            f"Virtual Console references missing Function {function_id}.",
        )
    return len(candidate_widgets)


def nearest_solo_frame(
    node: ET.Element,
    parent_map: Mapping[ET.Element, ET.Element],
) -> ET.Element | None:
    parent = parent_map.get(node)
    while parent is not None:
        if parent.tag == q("SoloFrame"):
            return parent
        parent = parent_map.get(parent)
    return None


def position_channels(pan: int, tilt: int) -> dict[int, int]:
    return {
        0: pan >> 8,
        1: pan & 0xFF,
        2: tilt >> 8,
        3: tilt & 0xFF,
    }


def validate_additive_movement(
    functions: Mapping[int, ET.Element],
    fixtures: Mapping[int, ET.Element],
    virtual_console: ET.Element,
    source_virtual_console: ET.Element,
) -> None:
    for function_id, (name, positions) in EXPECTED_POSITION_FUNCTIONS.items():
        scene = functions[function_id]
        require(scene.get("Type") == "Scene", f"Position Function {function_id} is not a Scene.")
        require(scene.get("Name") == name, f"Position Scene {function_id} has the wrong name.")
        speed = scene.find(q("Speed"))
        require(
            speed is not None
            and speed.get("FadeIn") == "0"
            and speed.get("FadeOut") == "0"
            and speed.get("Duration") == "0",
            f"Position Scene {function_id} must be an instantaneous sparse target.",
        )
        values = fixture_values(scene, fixtures)
        require(set(values) == {9, 10}, f"Position Scene {function_id} must target only Focus A/B.")
        for fixture_id, (pan, tilt) in zip((9, 10), positions):
            require(
                values[fixture_id] == position_channels(pan, tilt),
                f"Position Scene {function_id} fixture {fixture_id} does not match decoded source pan/tilt.",
            )

    chaser = functions[2184]
    require(chaser.get("Type") == "Chaser", "Function 2184 is not the movement Chaser.")
    require(chaser.get("Name") == EXPECTED_MOVEMENT_CHASER_NAME, "Movement Chaser name is wrong.")
    require(chaser.findtext(q("Tempo"), "") == "Beats", "Movement Chaser must use Beats tempo.")
    require(chaser.findtext(q("Direction"), "") == "Forward", "Movement Chaser direction must be Forward.")
    require(chaser.findtext(q("RunOrder"), "") == "Loop", "Movement Chaser must loop.")
    speed = chaser.find(q("Speed"))
    require(
        speed is not None
        and speed.get("FadeIn") == "750"
        and speed.get("FadeOut") == "0"
        and speed.get("Duration") == "1000",
        "Movement Chaser global timing must be FadeIn 750 / Hold 250 / FadeOut 0.",
    )
    speed_modes = chaser.find(q("SpeedModes"))
    require(
        speed_modes is not None
        and all(speed_modes.get(key) == "PerStep" for key in ("FadeIn", "FadeOut", "Duration")),
        "Movement Chaser must use per-step timing.",
    )
    steps = chaser.findall(q("Step"))
    require(len(steps) == 9, "Movement Chaser must contain exactly nine steps.")
    for index, step in enumerate(steps):
        require(step.get("Number") == str(index), "Movement Chaser step numbering is not consecutive.")
        require((step.text or "").strip() == str(2175 + index), "Movement Chaser position order changed.")
        require(
            step.get("FadeIn") == "750"
            and step.get("Hold") == "250"
            and step.get("FadeOut") == "0",
            f"Movement Chaser step {index} timing is wrong.",
        )

    source_widget_ids = frozenset(widget_map(source_virtual_console))
    widgets = widget_map(virtual_console)
    parent_map = {child: parent for parent in virtual_console.iter() for child in parent}
    global_input_counts: Counter[int] = Counter()
    for input_ref in virtual_console.iter(q("Input")):
        if input_ref.get("Universe") != "1":
            continue
        raw_channel = input_ref.get("Channel", "")
        if raw_channel.isdigit() and int(raw_channel) in POSITION_INPUT_CHANNELS:
            global_input_counts[int(raw_channel)] += 1
    require(
        global_input_counts == Counter(POSITION_INPUT_CHANNELS),
        "Position inputs 164-172 must each occur exactly once in the Virtual Console.",
    )
    controls: dict[int, tuple[ET.Element, ET.Element | None]] = {}
    position_target_counts: Counter[int] = Counter()
    for widget in widgets.values():
        function_ref = widget.find(q("Function"))
        if function_ref is not None and (function_ref.get("ID") or "").isdigit():
            target = int(function_ref.get("ID", "-1"))
            if target in EXPECTED_POSITION_FUNCTIONS:
                position_target_counts[target] += 1
        for input_ref in widget.findall(q("Input")):
            if input_ref.get("Universe") != "1":
                continue
            raw_channel = input_ref.get("Channel", "")
            if not raw_channel.isdigit() or int(raw_channel) not in POSITION_INPUT_CHANNELS:
                continue
            channel = int(raw_channel)
            require(channel not in controls, f"Input channel {channel} has more than one position control.")
            controls[channel] = (widget, function_ref)

    require(set(controls) == set(POSITION_INPUT_CHANNELS), "Position inputs 164-172 are not covered exactly once.")
    frames: set[ET.Element] = set()
    for offset, channel in enumerate(POSITION_INPUT_CHANNELS):
        widget, function_ref = controls[channel]
        require(local_name(widget.tag) == "Button", f"Position input {channel} is not attached to a Button.")
        require(function_ref is not None and function_ref.get("ID") == str(2175 + offset),
                f"Position input {channel} targets the wrong Scene.")
        frame = nearest_solo_frame(widget, parent_map)
        require(frame is not None, f"Position input {channel} is not inside a SoloFrame.")
        assert frame is not None
        frames.add(frame)
        raw_widget_id = widget.get("ID", "")
        require(raw_widget_id.isdigit() and int(raw_widget_id) not in source_widget_ids,
                f"Position input {channel} reuses a V26 widget ID.")
    require(len(frames) == 1, "All nine position controls must share one exclusive SoloFrame.")
    frame = next(iter(frames))
    raw_frame_id = frame.get("ID", "")
    require(raw_frame_id.isdigit() and int(raw_frame_id) not in source_widget_ids,
            "Position SoloFrame must use a new widget ID.")
    require(
        position_target_counts == Counter({function_id: 1 for function_id in EXPECTED_POSITION_FUNCTIONS}),
        "Each position Scene must be bound to exactly one Virtual Console button.",
    )
    frame_targets = []
    for descendant in frame.iter(q("Button")):
        reference = descendant.find(q("Function"))
        if reference is not None and (reference.get("ID") or "").isdigit():
            frame_targets.append(int(reference.get("ID", "-1")))
    require(
        Counter(frame_targets) == Counter(EXPECTED_POSITION_FUNCTIONS.keys()),
        "The position SoloFrame is not exclusive to the nine position Scenes.",
    )


def validate_safe_new_fixture_values(
    scene_id: int,
    values: Mapping[int, Mapping[int, int]],
    root_names: Iterable[str] = (),
) -> None:
    del root_names  # Reserved for future context-specific safety policy.
    for fixture_id in WASH_IDS & set(values):
        if scene_id not in OVERRIDE_SCENE_IDS:
            for channel in range(0, 4):
                if channel in values[fixture_id]:
                    require(values[fixture_id][channel] == 0,
                            f"Scene {scene_id} Wash fixture {fixture_id} enables auto/strobe channel {channel}.")
    for fixture_id in FOCUS_IDS & set(values):
        frame = values[fixture_id]
        if {0, 1, 2, 3} <= set(frame):
            side = 0 if fixture_id in {9, 109} else 1
            pan = (frame[0] << 8) | frame[1]
            tilt = (frame[2] << 8) | frame[3]
            allowed_positions = {
                positions[side]
                for _name, positions in EXPECTED_POSITION_FUNCTIONS.values()
            }
            require((pan, tilt) in allowed_positions,
                    f"Scene {scene_id} Focus fixture {fixture_id} uses an unreviewed pan/tilt preset.")
        if 16 in frame:
            require(frame[16] in {40, 110, 150, 210, 220},
                    f"Scene {scene_id} Focus fixture {fixture_id} uses an unreviewed P/T speed.")
        for channel in FOCUS_SAFE_ZERO_CHANNELS:
            if channel in frame:
                require(frame[channel] == 0,
                        f"Scene {scene_id} Focus fixture {fixture_id} enables unsafe channel {channel}.")
        if 8 in frame:
            require(frame[8] in {0, 8},
                    f"Scene {scene_id} Focus fixture {fixture_id} has unauthorized main shutter {frame[8]}.")
        if 10 in frame:
            require(frame[10] == 0,
                    f"Scene {scene_id} Focus fixture {fixture_id} enables the unbenchmarked UV shutter.")
        if 11 in frame:
            require(frame[11] == 0,
                    f"Scene {scene_id} Focus fixture {fixture_id} enables unbenchmarked UV output.")


def scene_root_names(
    functions: Mapping[int, ET.Element],
    roots: Iterable[int],
) -> dict[int, set[str]]:
    result: dict[int, set[str]] = {}
    for root_id in roots:
        root = functions[root_id]
        name = root.get("Name", "")
        if root.get("Type") == "Scene":
            result.setdefault(root_id, set()).add(name)
            continue
        for scene_id in step_references(root):
            result.setdefault(scene_id, set()).add(name)
    return result


def scene_root_ids(
    functions: Mapping[int, ET.Element],
    roots: Iterable[int],
) -> dict[int, set[int]]:
    result: dict[int, set[int]] = {}
    for root_id in roots:
        root = functions[root_id]
        if root.get("Type") == "Scene":
            result.setdefault(root_id, set()).add(root_id)
            continue
        for scene_id in step_references(root):
            result.setdefault(scene_id, set()).add(root_id)
    return result


def new_fixture_emits(fixture_id: int, frame: Sequence[int]) -> bool:
    if fixture_id in WASH_IDS:
        return any(frame[channel] > 0 for channel in range(4, 40))
    if fixture_id in FOCUS_IDS:
        main_lit = frame[8] != 0 and frame[9] > 0
        uv_lit = frame[10] != 0 and frame[11] > 0
        return main_lit or uv_lit
    return False


def validate_live_coverage(
    functions: Mapping[int, ET.Element],
    fixtures: Mapping[int, ET.Element],
) -> dict[str, int]:
    physical_ids = {
        fixture_id for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 0
    }
    private_ids = {
        fixture_id for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 2
    }
    require(physical_ids == EXPECTED_PHYSICAL_IDS, "Physical fixture set is incomplete.")
    require(private_ids == EXPECTED_PRIVATE_IDS, "Private fixture set is incomplete.")

    for root_id in RAW_LOOP_IDS:
        root = functions[root_id]
        require(root.get("Type") == "Chaser", f"Raw Autoloop {root_id} is not a Chaser.")
        require(len(step_references(root)) == 8, f"Raw Autoloop {root_id} does not have eight steps.")
    raw_closure = closure(functions, RAW_LOOP_IDS)
    raw_scene_ids = scene_ids_in(functions, raw_closure)
    raw_names = scene_root_names(functions, RAW_LOOP_IDS)
    raw_roots = scene_root_ids(functions, RAW_LOOP_IDS)
    require(len(raw_scene_ids) == 1024, f"Expected 1,024 distinct raw Autoloop Scenes; found {len(raw_scene_ids)}.")
    require(
        Counter(functions[function_id].get("Type") for function_id in raw_closure)
        == Counter({"Scene": 1024, "Chaser": 128}),
        "Raw Autoloop closure has unexpected Function types.",
    )

    priority_chasers = {
        root_id for root_id in PRIORITY_ROOT_IDS if functions[root_id].get("Type") == "Chaser"
    }
    require(priority_chasers == PRIORITY_CHASER_IDS, "Priority Scene/Chaser root types changed.")
    for root_id in priority_chasers:
        require(len(step_references(functions[root_id])) == 8, f"Priority Chaser {root_id} does not have eight steps.")
    priority_closure = closure(functions, PRIORITY_ROOT_IDS)
    priority_scene_ids = scene_ids_in(functions, priority_closure)
    priority_names = scene_root_names(functions, PRIORITY_ROOT_IDS)
    require(len(priority_scene_ids) == 102, f"Expected 102 distinct Priority Scenes; found {len(priority_scene_ids)}.")
    require(
        Counter(functions[function_id].get("Type") for function_id in priority_closure)
        == Counter({"Scene": 102, "Chaser": 10}),
        "Priority closure has unexpected Function types.",
    )

    for scene_id in sorted(raw_scene_ids):
        values = fixture_values(functions[scene_id], fixtures)
        require(set(values) == physical_ids, f"Raw Scene {scene_id} does not cover exactly the physical rig.")
        for fixture_id in physical_ids:
            complete_frame(values[fixture_id], int(fixtures[fixture_id].findtext(q("Channels"), "-1")))
        validate_safe_new_fixture_values(scene_id, values, raw_names.get(scene_id, ()))
        require(len(raw_roots.get(scene_id, ())) == 1,
                f"Raw Scene {scene_id} does not have one unambiguous Autoloop owner.")
        root_id = next(iter(raw_roots[scene_id]))
        expected_pt_speed = (
            150 if root_id <= 563 else
            110 if root_id <= 595 else
            210 if root_id <= 627 else
            40
        )
        for fixture_id in (9, 10):
            require(values[fixture_id][16] == expected_pt_speed,
                    f"Raw Scene {scene_id} fixture {fixture_id} has the wrong bank P/T speed.")

    for scene_id in sorted(priority_scene_ids):
        values = fixture_values(functions[scene_id], fixtures)
        require(set(values) == private_ids, f"Priority Scene {scene_id} does not cover exactly the private rig.")
        for fixture_id in private_ids:
            complete_frame(values[fixture_id], int(fixtures[fixture_id].findtext(q("Channels"), "-1")))
        validate_safe_new_fixture_values(scene_id, values, priority_names.get(scene_id, ()))
        for fixture_id in (109, 110):
            require(values[fixture_id][16] == 220,
                    f"Priority Scene {scene_id} fixture {fixture_id} must use Priority P/T speed 220.")

    for scene_id in sorted(PERFORMANCE_SCENE_IDS | OVERRIDE_SCENE_IDS):
        require(functions[scene_id].get("Type") == "Scene", f"Live Scene root {scene_id} is not a Scene.")
        values = fixture_values(functions[scene_id], fixtures)
        require(set(values) == physical_ids, f"Live Scene {scene_id} does not cover the full physical rig.")
        if scene_id in PERFORMANCE_SCENE_IDS:
            for fixture_id in physical_ids:
                complete_frame(values[fixture_id], int(fixtures[fixture_id].findtext(q("Channels"), "-1")))
        validate_safe_new_fixture_values(
            scene_id,
            values,
            (functions[scene_id].get("Name", ""),),
        )

    # Color overrides must remain sparse: change emitter/color-wheel parameters
    # without taking intensity or movement ownership.
    for scene_id in sorted(OVERRIDE_SCENE_IDS):
        values = fixture_values(functions[scene_id], fixtures)
        require(set(values[4]) <= WASH_DIRECT_COLOR_CHANNELS and values[4],
                f"Color override {scene_id} drives non-color Wash channels.")
        for fixture_id in (9, 10):
            require(set(values[fixture_id]) == FOCUS_COLOR_CHANNELS,
                    f"Color override {scene_id} must target only Focus color-wheel channel 4.")

    # Blackout must explicitly extinguish every new emitter/dimmer without
    # relying on an earlier Scene releasing in a favorable state.
    blackout = fixture_values(functions[0], fixtures)
    require(set(range(4, 40)) <= set(blackout[4]), "Blackout omits Wash emitter channels.")
    require(all(blackout[4][channel] == 0 for channel in range(4, 40)), "Blackout leaves a Wash emitter active.")
    for fixture_id in (9, 10):
        require({8, 9, 10, 11} <= set(blackout[fixture_id]), f"Blackout omits Focus dimmer/shutter channels for {fixture_id}.")
        require(all(blackout[fixture_id][channel] == 0 for channel in (8, 9, 10, 11)),
                f"Blackout leaves Focus fixture {fixture_id} active.")

    # Inclusion alone is insufficient.  Every Autoloop and moving Priority
    # root must contain a changing new-rig frame and must light each new member
    # at least once.  Purposeful steady behavior on one fixture remains legal.
    for root_id in RAW_LOOP_IDS:
        scene_ids = step_references(functions[root_id])
        combined_frames: list[tuple[Any, ...]] = []
        active_by_fixture = {fixture_id: False for fixture_id in NEW_PHYSICAL_IDS}
        for scene_id in scene_ids:
            values = fixture_values(functions[scene_id], fixtures)
            frame_parts = []
            for fixture_id in sorted(NEW_PHYSICAL_IDS):
                frame = complete_frame(values[fixture_id], int(fixtures[fixture_id].findtext(q("Channels"), "-1")))
                frame_parts.append(frame)
                active_by_fixture[fixture_id] |= new_fixture_emits(fixture_id, frame)
            combined_frames.append(tuple(frame_parts))
        require(len(set(combined_frames)) >= 2, f"Autoloop {root_id} gives the new rig one static frame.")
        for fixture_id, active in active_by_fixture.items():
            require(active, f"Autoloop {root_id} never activates new fixture {fixture_id}.")

    for root_id in sorted(PRIORITY_CHASER_IDS):
        scene_ids = step_references(functions[root_id])
        combined_frames = []
        active_by_fixture = {fixture_id: False for fixture_id in NEW_PRIVATE_IDS}
        for scene_id in scene_ids:
            values = fixture_values(functions[scene_id], fixtures)
            frame_parts = []
            for fixture_id in sorted(NEW_PRIVATE_IDS):
                frame = complete_frame(values[fixture_id], int(fixtures[fixture_id].findtext(q("Channels"), "-1")))
                frame_parts.append(frame)
                active_by_fixture[fixture_id] |= new_fixture_emits(fixture_id, frame)
            combined_frames.append(tuple(frame_parts))
        require(len(set(combined_frames)) >= 2, f"Priority Chaser {root_id} gives the new rig one static frame.")
        for fixture_id, active in active_by_fixture.items():
            require(active, f"Priority Chaser {root_id} never activates private fixture {fixture_id}.")

    override_signatures: dict[int, set[tuple[tuple[int, int], ...]]] = {
        fixture_id: set() for fixture_id in NEW_PHYSICAL_IDS
    }
    for scene_id in OVERRIDE_SCENE_IDS:
        values = fixture_values(functions[scene_id], fixtures)
        for fixture_id in NEW_PHYSICAL_IDS:
            override_signatures[fixture_id].add(tuple(sorted(values[fixture_id].items())))
    for fixture_id, signatures in override_signatures.items():
        require(len(signatures) >= 5, f"New fixture {fixture_id} has fewer than five distinct color overrides.")

    # Chaser 808 is intentionally inherited through existing white/black Scenes.
    require(
        closure(functions, (808,)) == {0, 1, 808},
        "Controlled white pulse 808 no longer inherits performance white/black.",
    )

    live_scenes = raw_scene_ids | priority_scene_ids | PERFORMANCE_SCENE_IDS | OVERRIDE_SCENE_IDS
    require(len(live_scenes) == 1140, f"Expected exactly 1,140 live creative Scenes; found {len(live_scenes)}.")
    return {
        "raw_scenes": len(raw_scene_ids),
        "priority_scenes": len(priority_scene_ids),
        "live_scenes": len(live_scenes),
    }


def validate_autoplay_and_owners(functions: Mapping[int, ET.Element]) -> None:
    for index, owner_id in enumerate(MANUAL_OWNER_IDS):
        owner = functions[owner_id]
        require(owner.get("Type") == "Collection", f"Manual owner {owner_id} is not a Collection.")
        require(step_references(owner) == [RAW_LOOP_IDS[index]], f"Manual owner {owner_id} lost its raw Autoloop.")
    for parent_id in AUTOPLAY_PARENT_IDS:
        parent = functions[parent_id]
        require(parent.get("Type") == "Chaser", f"Autoplay parent {parent_id} is not a Chaser.")
        expected_steps = 128 if parent_id in {792, 797} else 32
        require(len(step_references(parent)) == expected_steps, f"Autoplay parent {parent_id} has the wrong scope.")
        require(parent.findtext(q("Tempo"), "") == "Beats", f"Autoplay parent {parent_id} is not beat-counted.")
        speed = parent.find(q("Speed"))
        require(speed is not None and speed.get("Duration") == "32000",
                f"Autoplay parent {parent_id} no longer defaults to 8 measures.")
    for owner_id in AUTOPLAY_OWNER_IDS:
        require(functions[owner_id].get("Type") == "Collection", f"Autoplay owner {owner_id} is not a Collection.")


def validate_candidate(
    source: Path,
    candidate: Path,
    focus_definition: Path,
) -> dict[str, Any]:
    source = source.resolve()
    candidate = candidate.resolve()
    require(source != candidate, "Source and candidate paths must differ.")
    require(sha256(source) == EXPECTED_V26_SHA256,
            f"V27 source must be exact V26 SHA-256 {EXPECTED_V26_SHA256}; found {sha256(source)}.")

    source_root, source_engine, source_vc = load_workspace(source)
    root, engine, virtual_console = load_workspace(candidate)
    validate_workspace_shell(source_root, root)
    require(virtual_console.attrib == source_vc.attrib, "Virtual Console root settings changed.")
    source_functions = function_map(source_engine)
    functions = function_map(engine)
    source_fixtures = fixture_map(source_engine)
    fixtures = fixture_map(engine)

    require(len(source_functions) == EXPECTED_SOURCE_FUNCTION_COUNT, "Pinned V26 Function count is unexpected.")
    require(len(functions) == EXPECTED_CANDIDATE_FUNCTION_COUNT,
            f"Expected exactly {EXPECTED_CANDIDATE_FUNCTION_COUNT} V27 Functions.")
    require(set(source_functions) <= set(functions), "A V26 Function ID was removed.")
    require(
        set(functions) - set(source_functions) == EXPECTED_ADDITIVE_FUNCTION_IDS,
        "V27 must add exactly Functions 2175-2184.",
    )
    require(
        Counter(function.get("Type") for function in functions.values())
        == Counter(EXPECTED_CANDIDATE_FUNCTION_TYPES),
        "V27 Function type counts are wrong.",
    )

    live_closure = (
        scene_ids_in(source_functions, closure(source_functions, RAW_LOOP_IDS))
        | scene_ids_in(source_functions, closure(source_functions, PRIORITY_ROOT_IDS))
        | PERFORMANCE_SCENE_IDS
        | OVERRIDE_SCENE_IDS
    )
    require(len(live_closure) == 1140, "Pinned V26 live creative closure is unexpected.")
    for function_id, source_function in source_functions.items():
        function = functions[function_id]
        require(
            canonical(function, drop_direct=frozenset({"FixtureVal"}))
            == canonical(source_function, drop_direct=frozenset({"FixtureVal"})),
            f"Function {function_id} public identity/timing/reference skeleton changed from V26.",
        )
        if function_id not in live_closure:
            require(canonical(function) == canonical(source_function),
                    f"Dormant/UI/control Function {function_id} changed from V26.")

    validate_function_references(functions)
    validate_fixture_patch(source_fixtures, fixtures)
    validate_focus_definition(focus_definition.resolve())
    validate_input_output(engine, source_engine)
    validate_monitor(engine, source_engine)
    widget_count = validate_virtual_console(virtual_console, source_vc, functions)
    validate_additive_movement(functions, fixtures, virtual_console, source_vc)
    for function_id, function in functions.items():
        if function.get("Type") == "Scene":
            fixture_values(function, fixtures)
    for scene_id in UI_CONTROL_SCENE_IDS:
        require(not functions[scene_id].findall(q("FixtureVal")),
                f"UI command Scene {scene_id} must remain empty.")
    validate_autoplay_and_owners(functions)
    coverage = validate_live_coverage(functions, fixtures)

    text = candidate.read_text(encoding="utf-8")
    require(PERSONAL_LEAK_PATTERN.search(text) is None, "A personal path or username leaked into V27.")
    require(sha256(source) == EXPECTED_V26_SHA256, "Protected V26 changed during validation.")

    return {
        **coverage,
        "widgets": widget_count,
        "functions": len(functions),
        "fixtures": len(fixtures),
        "candidate_sha256": sha256(candidate),
    }


def default_paths() -> tuple[Path, Path, Path]:
    script = Path(__file__).resolve()
    repository = script.parents[2]
    source = (
        repository
        / "releases"
        / "qlcplus-control-one"
        / "v26"
        / "IR4-TUBES-CONTROL-ONE-V26-AUTOPLAY-CLARITY.qxw"
    )
    candidate = script.with_name("IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw")
    focus_definition = repository / "qlcplus" / "fixture-definitions" / "American-DJ-Focus-Spot-Two.qxf"
    return source, candidate, focus_definition


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    source, candidate, focus_definition = default_paths()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-workspace", type=Path, default=source)
    parser.add_argument("--candidate-workspace", type=Path, default=candidate)
    parser.add_argument("--focus-definition", type=Path, default=focus_definition)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        report = validate_candidate(
            args.source_workspace,
            args.candidate_workspace,
            args.focus_definition,
        )
    except (ValidationError, OSError, UnicodeError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1

    print("PASS: V27 full-rig workspace regression")
    print(f"  Protected V26 SHA-256: {EXPECTED_V26_SHA256}")
    print(f"  Functions: {report['functions']} = V26 preserved + 9 positions + 1 movement Chaser")
    print(f"  Fixtures: {report['fixtures']} = 11 physical + 11 private Priority duplicates")
    print(
        "  Live coverage: "
        f"{report['raw_scenes']} Autoloop + {report['priority_scenes']} Priority + "
        "5 performance + 9 override Scenes"
    )
    print(f"  Virtual Console widgets: {report['widgets']} with additive exclusive position controls")
    print(f"  Candidate SHA-256: {report['candidate_sha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
