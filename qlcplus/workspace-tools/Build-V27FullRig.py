#!/usr/bin/env python3
"""Build the V27 full-rig workspace from the immutable V26 release.

This builder owns structure, fixture identity, deterministic serialization, and
the boundary around creative programming.  The artistic DMX frames live in a
separate provider module so a fixture-only or zero-filled scaffold cannot be
mistaken for a releasable show.

The default provider path is ``V27FullRigCreative.py`` next to this script.  A
provider must expose::

    PROGRAM_COMPLETE = True
    PROGRAM_VERSION = "a reviewed, stable identifier"

    def scene_fixture_values(context):
        return {
            # Fixture ID -> either a channel/value mapping or a complete
            # positional list of channel values.
            4: {0: 0, 1: 0, ...},
            9: [pan, pan_fine, tilt, ...],
        }

    def extend_workspace(context):
        # Add only Functions 2175-2184, new Virtual Console widgets, physical
        # Monitor items, and label-only Input/Output-map updates.  The context
        # intentionally provides the parsed XML nodes and q(name) helper.
        ...

The hook is called once for each of the 1,140 live creative Scene leaves.  Its
``context`` is a plain dictionary containing the Scene identity, category,
root/step uses, immutable source FixtureVal data, and the fixture manifest.
The provider may also return values for existing fixtures on the same physical
or private layer when a reviewed creative improvement calls for it.

No candidate is written unless the provider declares itself complete and every
required fixture and additive movement control passes the structural checks in
this file.
"""

from __future__ import annotations

import argparse
from collections import Counter
import copy
import hashlib
import importlib.util
import os
import re
import sys
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path
from types import ModuleType
from typing import Any, Iterable, Mapping, Sequence
import xml.etree.ElementTree as ET


NS = "http://www.qlcplus.org/Workspace"
ET.register_namespace("", NS)

EXPECTED_V26_SHA256 = (
    "ED97E3EBAEA120BC6FF5FF9747485DA54E1808479F64A02AB4BC044744FAB570"
)
EXPECTED_FUNCTION_COUNT = 2090
EXPECTED_ADDITIVE_FUNCTION_IDS = frozenset(range(2175, 2185))
EXPECTED_CANDIDATE_FUNCTION_COUNT = EXPECTED_FUNCTION_COUNT + len(EXPECTED_ADDITIVE_FUNCTION_IDS)
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
EXPECTED_SOURCE_FIXTURE_IDS = {
    0, 1, 2, 3, 5, 6, 7, 8,
    100, 101, 102, 103, 105, 106, 107, 108,
}

RAW_LOOP_IDS = tuple(range(532, 660))
PRIORITY_ROOT_IDS = tuple(range(5, 37))
PERFORMANCE_SCENE_IDS = frozenset(range(0, 5))
OVERRIDE_SCENE_IDS = frozenset(range(37, 46))
UI_CONTROL_SCENE_IDS = frozenset(range(1982, 1999))

INTEGER_DMX_TOKEN = re.compile(r"^(?:0|[1-9][0-9]*)$")
LEGACY_INTEGER_DECIMAL_DMX_TOKEN = re.compile(r"^(?:0|[1-9][0-9]*)\.0$")


class BuildError(RuntimeError):
    """A deterministic V27 build precondition or invariant failed."""


@dataclass(frozen=True)
class FixtureSpec:
    role: str
    physical_id: int
    private_id: int
    manufacturer: str
    model: str
    mode: str
    address: int  # QLC+ XML is zero-based.
    channels: int
    operator_name: str

    def for_layer(self, private: bool) -> dict[str, Any]:
        fixture_id = self.private_id if private else self.physical_id
        return {
            "role": self.role,
            "id": fixture_id,
            "paired_id": self.physical_id if private else self.private_id,
            "manufacturer": self.manufacturer,
            "model": self.model,
            "mode": self.mode,
            "universe": 2 if private else 0,
            "address": self.address,
            "channels": self.channels,
            "name": (
                f"{self.operator_name} — Priority Layer"
                if private
                else self.operator_name
            ),
            "private": private,
        }


# All V26 physical addresses remain unchanged.  The full-rig fixtures fill the
# intentionally open 1-based range 41-116; 117-174 stays reserved and the
# existing tubes continue at 175.
NEW_FIXTURE_SPECS = (
    FixtureSpec(
        role="wash",
        physical_id=4,
        private_id=104,
        manufacturer="Chauvet",
        model="Wash FX Hex",
        mode="40 Channel",
        address=40,
        channels=40,
        operator_name="Wash FX Hex — Dance Floor",
    ),
    FixtureSpec(
        role="focus_a",
        physical_id=9,
        private_id=109,
        manufacturer="American DJ",
        model="Focus Spot Two",
        mode="18 Channel",
        address=80,
        channels=18,
        operator_name="Focus Spot Two — A",
    ),
    FixtureSpec(
        role="focus_b",
        physical_id=10,
        private_id=110,
        manufacturer="American DJ",
        model="Focus Spot Two",
        mode="18 Channel",
        address=98,
        channels=18,
        operator_name="Focus Spot Two — B",
    ),
)

NEW_PHYSICAL_IDS = frozenset(spec.physical_id for spec in NEW_FIXTURE_SPECS)
NEW_PRIVATE_IDS = frozenset(spec.private_id for spec in NEW_FIXTURE_SPECS)


def q(name: str) -> str:
    return f"{{{NS}}}{name}"


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise BuildError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def canonical(element: ET.Element, *, drop_direct: frozenset[str] = frozenset()) -> tuple[Any, ...]:
    """Return a whitespace/attribute-order-independent XML signature."""

    children = []
    for child in list(element):
        if local_name(child.tag) in drop_direct:
            continue
        children.append(canonical(child))
    text = (element.text or "").strip()
    return (
        element.tag,
        tuple(sorted(element.attrib.items())),
        text,
        tuple(children),
    )


def io_operational_signature(element: ET.Element | None) -> tuple[Any, ...]:
    """Compare I/O behavior while permitting operator-facing label updates."""

    require(element is not None, "Input/Output map is missing.")
    assert element is not None
    clone = copy.deepcopy(element)
    for node in clone.iter():
        node.attrib.pop("Name", None)
    return canonical(clone)


def workspace_shell_signature(root: ET.Element) -> tuple[Any, ...]:
    require(
        [local_name(child.tag) for child in root] == ["Creator", "Engine", "VirtualConsole"],
        "Workspace top-level structure is unexpected.",
    )
    return (root.tag, tuple(sorted(root.attrib.items())), canonical(root.find(q("Creator"))))


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
    """Return only bindings owned by this widget, excluding nested widgets."""

    bindings = []

    def visit(node: ET.Element) -> None:
        for child in node:
            # Frames and SoloFrames contain child widgets.  Adding a new child
            # must not make the existing container look like its own binding
            # changed; the child is validated independently by widget ID.
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


def validate_console_extension(source: ET.Element, candidate: ET.Element) -> None:
    source_widgets = widget_map(source)
    candidate_widgets = widget_map(candidate)
    require(set(source_widgets) <= set(candidate_widgets), "A V26 Virtual Console widget was removed.")
    for widget_id, source_widget in source_widgets.items():
        require(
            widget_binding_signature(candidate_widgets[widget_id], normalise_move_target=(widget_id == 1004))
            == widget_binding_signature(source_widget),
            f"Existing widget {widget_id} changed its Function/Input binding.",
        )
    new_widget_ids = set(candidate_widgets) - set(source_widgets)
    require(
        all(widget_id > max(source_widgets) for widget_id in new_widget_ids),
        "Additive Virtual Console widgets must use new IDs above the V26 allocator ceiling.",
    )
    move_target = candidate_widgets[1004].find(q("Function"))
    require(
        move_target is not None and move_target.get("ID") == "2184",
        "Existing MOVE widget 1004 must target additive movement Chaser 2184.",
    )


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
    virtual_console: ET.Element,
    source_widget_ids: frozenset[int],
) -> None:
    """Pin the decoded Focus A/B positions and their exclusive controls."""

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
        values = fixture_values(scene)
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
        require(frame is not None, f"Position control input {channel} is not inside a SoloFrame.")
        assert frame is not None
        frames.add(frame)
        raw_widget_id = widget.get("ID", "")
        require(raw_widget_id.isdigit() and int(raw_widget_id) not in source_widget_ids,
                f"Position input {channel} reuses a V26 widget ID.")
    require(len(frames) == 1, "All nine position buttons must share one exclusive SoloFrame.")
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


def validate_monitor_extension(source: ET.Element | None, candidate: ET.Element | None) -> None:
    require(source is not None and candidate is not None, "Monitor configuration is missing.")
    assert source is not None and candidate is not None
    require(source.attrib == candidate.attrib, "Monitor root settings changed.")
    source_non_items = [canonical(child) for child in source if child.tag not in {q("FxItem"), q("Grid")}]
    candidate_non_items = [canonical(child) for child in candidate if child.tag not in {q("FxItem"), q("Grid")}]
    require(source_non_items == candidate_non_items, "Monitor display/grid settings changed.")
    source_grid = source.find(q("Grid"))
    candidate_grid = candidate.find(q("Grid"))
    require(source_grid is not None and candidate_grid is not None, "Monitor Grid is missing.")
    assert source_grid is not None and candidate_grid is not None
    for attribute in ("Units", "POV"):
        require(candidate_grid.get(attribute) == source_grid.get(attribute), f"Monitor Grid {attribute} changed.")
    for attribute in ("Width", "Height", "Depth"):
        source_size = int(source_grid.get(attribute, "0"))
        candidate_size = int(candidate_grid.get(attribute, "0"))
        require(source_size <= candidate_size <= 20, f"Monitor Grid {attribute} is not a sensible expansion.")
    candidate_items: dict[int, tuple[int, int, int]] = {}
    for item in candidate.findall(q("FxItem")):
        require(set(item.attrib) == {"ID", "XPos", "YPos", "ZPos"},
                "Monitor FxItem changes must be position-only.")
        raw_id = item.get("ID", "")
        require(raw_id.isdigit(), f"Monitor has malformed FxItem ID {raw_id!r}.")
        fixture_id = int(raw_id)
        require(fixture_id not in candidate_items, f"Duplicate Monitor FxItem ID {fixture_id}.")
        try:
            position = tuple(int(item.get(attribute, "")) for attribute in ("XPos", "YPos", "ZPos"))
        except ValueError as exc:
            raise BuildError(f"Monitor item {fixture_id} has a malformed position.") from exc
        require(all(-10 <= coordinate <= 10 for coordinate in position),
                f"Monitor item {fixture_id} position {position} is outside the visual bench.")
        candidate_items[fixture_id] = position  # type: ignore[assignment]
    require(set(candidate_items) == set(range(0, 11)), "Monitor must contain exactly all 11 physical fixtures.")
    require(len(set(candidate_items.values())) == len(candidate_items), "Visual-bench fixture positions overlap.")


def load_workspace(path: Path) -> tuple[ET.ElementTree, ET.Element, ET.Element, ET.Element]:
    try:
        tree = ET.parse(path)
    except (ET.ParseError, OSError) as exc:
        raise BuildError(f"Unable to parse workspace {path}: {exc}") from exc
    root = tree.getroot()
    require(local_name(root.tag) == "Workspace", "Workspace root element is missing.")
    engine = root.find(q("Engine"))
    virtual_console = root.find(q("VirtualConsole"))
    require(engine is not None, "Workspace Engine is missing.")
    require(virtual_console is not None, "Workspace VirtualConsole is missing.")
    return tree, root, engine, virtual_console


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
    for step in function.findall(q("Step")):
        raw = (step.text or "").strip()
        require(raw.isdigit(), f"Function {function.get('ID')} has malformed Step reference {raw!r}.")
        references.append(int(raw))
    return references


def validate_all_function_references(functions: Mapping[int, ET.Element]) -> None:
    for owner_id, function in functions.items():
        for target_id in step_references(function):
            require(
                target_id in functions,
                f"Function {owner_id} references missing Function {target_id}.",
            )


def closure(functions: Mapping[int, ET.Element], roots: Iterable[int]) -> set[int]:
    """Return a reference closure and reject cycles or missing roots."""

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

    for root_id in sorted(roots):
        visit(root_id)
    return visited


def scene_ids_in(functions: Mapping[int, ET.Element], ids: Iterable[int]) -> set[int]:
    return {
        function_id
        for function_id in ids
        if functions[function_id].get("Type") == "Scene"
    }


def collect_scene_uses(
    functions: Mapping[int, ET.Element],
    roots: Iterable[int],
    category: str,
) -> dict[int, list[dict[str, Any]]]:
    uses: dict[int, list[dict[str, Any]]] = {}
    for root_id in sorted(roots):
        root = functions[root_id]
        if root.get("Type") == "Scene":
            uses.setdefault(root_id, []).append(
                {
                    "category": category,
                    "root_id": root_id,
                    "root_name": root.get("Name", ""),
                    "step_index": None,
                }
            )
            continue
        require(root.get("Type") == "Chaser", f"Creative root {root_id} is not a Scene/Chaser.")
        for index, scene_id in enumerate(step_references(root)):
            require(
                functions[scene_id].get("Type") == "Scene",
                f"Creative Chaser {root_id} step {index} does not reference a Scene.",
            )
            uses.setdefault(scene_id, []).append(
                {
                    "category": category,
                    "root_id": root_id,
                    "root_name": root.get("Name", ""),
                    "step_index": index,
                }
            )
    return uses


def fixture_values(scene: ET.Element) -> dict[int, dict[int, int]]:
    values: dict[int, dict[int, int]] = {}
    for fixture_value in scene.findall(q("FixtureVal")):
        raw_id = fixture_value.get("ID", "")
        require(raw_id.isdigit(), f"Scene {scene.get('ID')} has malformed FixtureVal ID {raw_id!r}.")
        fixture_id = int(raw_id)
        require(fixture_id not in values, f"Scene {scene.get('ID')} repeats FixtureVal {fixture_id}.")
        tokens = [token.strip() for token in (fixture_value.text or "").split(",") if token.strip()]
        require(
            tokens and len(tokens) % 2 == 0,
            f"Scene {scene.get('ID')} FixtureVal {fixture_id} is malformed.",
        )
        channel_values: dict[int, int] = {}
        for offset in range(0, len(tokens), 2):
            channel_token = tokens[offset]
            value_token = tokens[offset + 1]
            require(
                INTEGER_DMX_TOKEN.fullmatch(channel_token) is not None,
                f"Scene {scene.get('ID')} FixtureVal {fixture_id} has non-integer channel {channel_token!r}.",
            )
            require(
                INTEGER_DMX_TOKEN.fullmatch(value_token) is not None
                or LEGACY_INTEGER_DECIMAL_DMX_TOKEN.fullmatch(value_token) is not None,
                f"Scene {scene.get('ID')} FixtureVal {fixture_id} has non-integral DMX value {value_token!r}.",
            )
            channel = int(channel_token)
            value = int(value_token[:-2] if value_token.endswith(".0") else value_token)
            require(channel not in channel_values, f"Scene {scene.get('ID')} repeats channel {channel}.")
            require(0 <= value <= 255, f"Scene {scene.get('ID')} has DMX value {value} outside 0..255.")
            channel_values[channel] = value
        values[fixture_id] = channel_values
    return values


def normalise_provider_values(
    fixture_id: int,
    raw: Any,
    channel_count: int,
    scene_id: int,
) -> dict[int, int]:
    if isinstance(raw, Mapping):
        pairs = ((int(channel), int(value)) for channel, value in raw.items())
    elif isinstance(raw, Sequence) and not isinstance(raw, (str, bytes, bytearray)):
        require(
            len(raw) == channel_count,
            f"Provider Scene {scene_id} fixture {fixture_id} positional frame must contain "
            f"{channel_count} values, found {len(raw)}.",
        )
        pairs = enumerate(int(value) for value in raw)
    else:
        raise BuildError(
            f"Provider Scene {scene_id} fixture {fixture_id} must be a mapping or positional sequence."
        )

    result: dict[int, int] = {}
    for channel, value in pairs:
        require(channel not in result, f"Provider Scene {scene_id} repeats fixture {fixture_id} channel {channel}.")
        require(
            0 <= channel < channel_count,
            f"Provider Scene {scene_id} fixture {fixture_id} channel {channel} is outside the mode.",
        )
        require(
            0 <= value <= 255,
            f"Provider Scene {scene_id} fixture {fixture_id} value {value} is outside 0..255.",
        )
        result[channel] = value
    require(result, f"Provider Scene {scene_id} fixture {fixture_id} returned an empty frame.")
    return result


def set_fixture_values(scene: ET.Element, replacements: Mapping[int, Mapping[int, int]]) -> None:
    merged = fixture_values(scene)
    for fixture_id, values in replacements.items():
        merged[fixture_id] = dict(values)

    children = list(scene)
    fixture_indexes = [index for index, child in enumerate(children) if child.tag == q("FixtureVal")]
    insert_at = min(fixture_indexes) if fixture_indexes else len(children)
    for child in children:
        if child.tag == q("FixtureVal"):
            scene.remove(child)

    for fixture_id in sorted(merged):
        element = ET.Element(q("FixtureVal"), {"ID": str(fixture_id)})
        flattened: list[str] = []
        for channel, value in sorted(merged[fixture_id].items()):
            flattened.extend((str(channel), str(value)))
        element.text = ",".join(flattened)
        scene.insert(insert_at, element)
        insert_at += 1


def make_fixture_element(spec: FixtureSpec, private: bool) -> ET.Element:
    data = spec.for_layer(private)
    fixture = ET.Element(q("Fixture"))
    for name, value in (
        ("Manufacturer", data["manufacturer"]),
        ("Model", data["model"]),
        ("Mode", data["mode"]),
        ("ID", data["id"]),
        ("Name", data["name"]),
        ("Universe", data["universe"]),
        ("Address", data["address"]),
        ("Channels", data["channels"]),
    ):
        child = ET.SubElement(fixture, q(name))
        child.text = str(value)
    return fixture


def insert_fixture_sorted(engine: ET.Element, fixture: ET.Element) -> None:
    fixture_id = int(fixture.findtext(q("ID"), "-1"))
    universe = int(fixture.findtext(q("Universe"), "-1"))
    children = list(engine)
    for index, existing in enumerate(children):
        if existing.tag == q("Function"):
            engine.insert(index, fixture)
            return
        if existing.tag != q("Fixture"):
            continue
        existing_key = (
            int(existing.findtext(q("Universe"), "-1")),
            int(existing.findtext(q("ID"), "-1")),
        )
        if existing_key > (universe, fixture_id):
            engine.insert(index, fixture)
            return
    engine.append(fixture)


def load_creative_provider(path: Path) -> ModuleType:
    require(path.is_file(), f"Creative provider is missing: {path}")
    spec = importlib.util.spec_from_file_location("v27_full_rig_creative_provider", path)
    require(spec is not None and spec.loader is not None, f"Cannot load creative provider: {path}")
    module = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(module)
    except Exception as exc:  # The provider is reviewed repository code.
        raise BuildError(f"Creative provider failed to import: {exc}") from exc
    require(
        getattr(module, "PROGRAM_COMPLETE", False) is True,
        "Creative provider must declare PROGRAM_COMPLETE = True before a V27 candidate can be built.",
    )
    version = getattr(module, "PROGRAM_VERSION", "")
    require(isinstance(version, str) and version.strip(), "Creative provider PROGRAM_VERSION is missing.")
    require(
        callable(getattr(module, "scene_fixture_values", None)),
        "Creative provider must define scene_fixture_values(context).",
    )
    require(
        callable(getattr(module, "extend_workspace", None)),
        "Creative provider must define extend_workspace(context) for the additive movement controls.",
    )
    return module


def provider_manifest() -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for spec in NEW_FIXTURE_SPECS:
        item = asdict(spec)
        item["physical"] = spec.for_layer(False)
        item["private"] = spec.for_layer(True)
        result.append(item)
    return result


def validate_fixture_spans(fixtures: Mapping[int, ET.Element]) -> None:
    by_universe: dict[int, list[tuple[int, int, int]]] = {}
    for fixture_id, fixture in fixtures.items():
        universe = int(fixture.findtext(q("Universe"), "-1"))
        address = int(fixture.findtext(q("Address"), "-1"))
        channels = int(fixture.findtext(q("Channels"), "-1"))
        require(0 <= universe, f"Fixture {fixture_id} has an invalid universe.")
        require(0 <= address < 512 and 1 <= channels <= 512, f"Fixture {fixture_id} has an invalid patch.")
        require(address + channels <= 512, f"Fixture {fixture_id} exceeds its DMX universe.")
        by_universe.setdefault(universe, []).append((address, address + channels, fixture_id))
    for universe, spans in by_universe.items():
        spans.sort()
        for previous, current in zip(spans, spans[1:]):
            require(
                previous[1] <= current[0],
                f"Universe {universe + 1} fixtures {previous[2]} and {current[2]} overlap.",
            )


def validate_new_fixture_pairs(fixtures: Mapping[int, ET.Element]) -> None:
    for spec in NEW_FIXTURE_SPECS:
        for private in (False, True):
            expected = spec.for_layer(private)
            fixture_id = expected["id"]
            require(fixture_id in fixtures, f"Fixture {fixture_id} is missing.")
            fixture = fixtures[fixture_id]
            actual = {
                "manufacturer": fixture.findtext(q("Manufacturer"), ""),
                "model": fixture.findtext(q("Model"), ""),
                "mode": fixture.findtext(q("Mode"), ""),
                "universe": int(fixture.findtext(q("Universe"), "-1")),
                "address": int(fixture.findtext(q("Address"), "-1")),
                "channels": int(fixture.findtext(q("Channels"), "-1")),
                "name": fixture.findtext(q("Name"), ""),
            }
            for key in actual:
                require(actual[key] == expected[key], f"Fixture {fixture_id} {key} mismatch.")


def validate_live_coverage(
    functions: Mapping[int, ET.Element],
    fixtures: Mapping[int, ET.Element],
    raw_scene_ids: set[int],
    priority_scene_ids: set[int],
) -> None:
    physical_ids = {
        fixture_id
        for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 0
    }
    private_ids = {
        fixture_id
        for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 2
    }

    require(len(raw_scene_ids) == 1024, f"Expected 1,024 raw Autoloop Scenes; found {len(raw_scene_ids)}.")
    require(len(priority_scene_ids) == 102, f"Expected 102 Priority Scenes; found {len(priority_scene_ids)}.")

    for scene_id in sorted(raw_scene_ids):
        values = fixture_values(functions[scene_id])
        require(set(values) == physical_ids, f"Raw Scene {scene_id} does not cover the full physical rig.")
        for fixture_id in physical_ids:
            channels = int(fixtures[fixture_id].findtext(q("Channels"), "-1"))
            require(
                set(values[fixture_id]) == set(range(channels)),
                f"Raw Scene {scene_id} fixture {fixture_id} is not a complete frame.",
            )

    for scene_id in sorted(priority_scene_ids):
        values = fixture_values(functions[scene_id])
        require(set(values) == private_ids, f"Priority Scene {scene_id} does not cover the full private rig.")
        for fixture_id in private_ids:
            channels = int(fixtures[fixture_id].findtext(q("Channels"), "-1"))
            require(
                set(values[fixture_id]) == set(range(channels)),
                f"Priority Scene {scene_id} fixture {fixture_id} is not a complete frame.",
            )

    for scene_id in sorted(PERFORMANCE_SCENE_IDS | OVERRIDE_SCENE_IDS):
        values = fixture_values(functions[scene_id])
        require(
            set(values) == physical_ids,
            f"Performance/override Scene {scene_id} does not cover the full physical rig.",
        )
        if scene_id in PERFORMANCE_SCENE_IDS:
            for fixture_id in physical_ids:
                channels = int(fixtures[fixture_id].findtext(q("Channels"), "-1"))
                require(set(values[fixture_id]) == set(range(channels)),
                        f"Performance Scene {scene_id} fixture {fixture_id} is not a complete frame.")


def validate_integer_candidate_dmx(functions: Mapping[int, ET.Element]) -> None:
    """Reject decimal or otherwise non-canonical DMX tokens in the candidate."""

    for function_id, function in functions.items():
        if function.get("Type") != "Scene":
            continue
        for fixture_value in function.findall(q("FixtureVal")):
            fixture_id = fixture_value.get("ID", "")
            tokens = [token.strip() for token in (fixture_value.text or "").split(",")]
            require(
                tokens
                and len(tokens) % 2 == 0
                and all(INTEGER_DMX_TOKEN.fullmatch(token) is not None for token in tokens),
                f"Candidate Scene {function_id} FixtureVal {fixture_id} contains a non-integer DMX token.",
            )


def strip_formatting(element: ET.Element) -> None:
    if element.text is not None and not element.text.strip():
        element.text = None
    if element.tail is not None and not element.tail.strip():
        element.tail = None
    for child in list(element):
        strip_formatting(child)


def serialise_workspace(root: ET.Element) -> bytes:
    strip_formatting(root)
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    text = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<!DOCTYPE Workspace []>\n'
        f"{body}\n"
    )
    return text.replace("\n", "\r\n").encode("utf-8")


def write_atomic(path: Path, payload: bytes, *, force: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    require(force or not path.exists(), f"Output already exists; use --force to replace it: {path}")
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", suffix=".tmp", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def build(source: Path, output: Path, provider_path: Path, *, force: bool) -> None:
    source = source.resolve()
    output = output.resolve()
    provider_path = provider_path.resolve()
    require(source.is_file(), f"V26 source workspace is missing: {source}")
    require(source != output, "Source and output workspace paths must differ.")
    require(
        sha256(source) == EXPECTED_V26_SHA256,
        f"V27 must descend from exact V26 SHA-256 {EXPECTED_V26_SHA256}; found {sha256(source)}.",
    )
    provider = load_creative_provider(provider_path)

    _, root, engine, virtual_console = load_workspace(source)
    functions = function_map(engine)
    fixtures = fixture_map(engine)
    require(len(functions) == EXPECTED_FUNCTION_COUNT, f"Expected {EXPECTED_FUNCTION_COUNT} V26 Functions.")
    require(set(fixtures) == EXPECTED_SOURCE_FIXTURE_IDS, "The V26 source fixture-ID set is unexpected.")
    validate_all_function_references(functions)

    source_vc = copy.deepcopy(virtual_console)
    source_shell = workspace_shell_signature(root)
    source_function_ids = frozenset(functions)
    source_function_skeletons = {
        function_id: canonical(function, drop_direct=frozenset({"FixtureVal"}))
        for function_id, function in functions.items()
    }
    source_function_xml = {function_id: canonical(function) for function_id, function in functions.items()}
    source_fixture_xml = {fixture_id: canonical(fixture) for fixture_id, fixture in fixtures.items()}
    source_io = io_operational_signature(engine.find(q("InputOutputMap")))
    source_monitor = copy.deepcopy(engine.find(q("Monitor")))

    raw_closure = closure(functions, RAW_LOOP_IDS)
    priority_closure = closure(functions, PRIORITY_ROOT_IDS)
    raw_scene_ids = scene_ids_in(functions, raw_closure)
    priority_scene_ids = scene_ids_in(functions, priority_closure)
    require(len(raw_scene_ids) == 1024, f"V26 raw closure is not 1,024 Scenes: {len(raw_scene_ids)}.")
    require(len(priority_scene_ids) == 102, f"V26 Priority closure is not 102 Scenes: {len(priority_scene_ids)}.")

    live_scene_categories: dict[int, str] = {}
    for scene_id in raw_scene_ids:
        live_scene_categories[scene_id] = "raw"
    for scene_id in priority_scene_ids:
        require(scene_id not in live_scene_categories, f"Scene {scene_id} crosses physical/private closures.")
        live_scene_categories[scene_id] = "priority"
    for scene_id in PERFORMANCE_SCENE_IDS:
        require(scene_id not in live_scene_categories, f"Performance Scene {scene_id} overlaps another category.")
        live_scene_categories[scene_id] = "performance"
    for scene_id in OVERRIDE_SCENE_IDS:
        require(scene_id not in live_scene_categories, f"Override Scene {scene_id} overlaps another category.")
        live_scene_categories[scene_id] = "override"
    require(len(live_scene_categories) == 1140, f"Expected 1,140 live creative Scenes; found {len(live_scene_categories)}.")

    scene_uses: dict[int, list[dict[str, Any]]] = {}
    for mapping in (
        collect_scene_uses(functions, RAW_LOOP_IDS, "raw"),
        collect_scene_uses(functions, PRIORITY_ROOT_IDS, "priority"),
        collect_scene_uses(functions, PERFORMANCE_SCENE_IDS, "performance"),
        collect_scene_uses(functions, OVERRIDE_SCENE_IDS, "override"),
    ):
        for scene_id, uses in mapping.items():
            scene_uses.setdefault(scene_id, []).extend(uses)

    for spec in NEW_FIXTURE_SPECS:
        insert_fixture_sorted(engine, make_fixture_element(spec, False))
        insert_fixture_sorted(engine, make_fixture_element(spec, True))
    fixtures = fixture_map(engine)
    validate_fixture_spans(fixtures)
    validate_new_fixture_pairs(fixtures)

    physical_ids = {
        fixture_id
        for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 0
    }
    private_ids = {
        fixture_id
        for fixture_id, fixture in fixtures.items()
        if int(fixture.findtext(q("Universe"), "-1")) == 2
    }
    fixture_channels = {
        fixture_id: int(fixture.findtext(q("Channels"), "-1"))
        for fixture_id, fixture in fixtures.items()
    }

    for scene_id in sorted(live_scene_categories):
        scene = functions[scene_id]
        category = live_scene_categories[scene_id]
        allowed_ids = private_ids if category == "priority" else physical_ids
        required_ids = NEW_PRIVATE_IDS if category == "priority" else NEW_PHYSICAL_IDS
        context = {
            "format": "qlcplus-v27-full-rig-scene-context",
            "format_version": 1,
            "source_workspace_sha256": EXPECTED_V26_SHA256,
            "program_version": provider.PROGRAM_VERSION,
            "scene_id": scene_id,
            "scene_name": scene.get("Name", ""),
            "category": category,
            "uses": copy.deepcopy(scene_uses.get(scene_id, [])),
            "required_fixture_ids": sorted(required_ids),
            "allowed_fixture_ids": sorted(allowed_ids),
            "source_fixture_values": copy.deepcopy(fixture_values(scene)),
            "fixtures": copy.deepcopy(provider_manifest()),
        }
        try:
            raw_result = provider.scene_fixture_values(context)
        except Exception as exc:
            raise BuildError(f"Creative provider failed for Scene {scene_id}: {exc}") from exc
        require(isinstance(raw_result, Mapping), f"Provider Scene {scene_id} did not return a mapping.")

        replacements: dict[int, dict[int, int]] = {}
        for raw_fixture_id, raw_values in raw_result.items():
            fixture_id = int(raw_fixture_id)
            require(fixture_id in allowed_ids, f"Provider Scene {scene_id} targets wrong-layer fixture {fixture_id}.")
            replacements[fixture_id] = normalise_provider_values(
                fixture_id,
                raw_values,
                fixture_channels[fixture_id],
                scene_id,
            )
        require(
            required_ids <= set(replacements),
            f"Provider Scene {scene_id} omits required fixtures {sorted(required_ids - set(replacements))}.",
        )
        if category in {"raw", "priority"}:
            for fixture_id in required_ids:
                require(
                    set(replacements[fixture_id]) == set(range(fixture_channels[fixture_id])),
                    f"Provider Scene {scene_id} fixture {fixture_id} must be a complete frame.",
                )
        set_fixture_values(scene, replacements)

    extension_context = {
        "format": "qlcplus-v27-full-rig-workspace-extension-context",
        "format_version": 1,
        "source_workspace_sha256": EXPECTED_V26_SHA256,
        "program_version": provider.PROGRAM_VERSION,
        "root": root,
        "engine": engine,
        "virtual_console": virtual_console,
        "q": q,
        "namespace": NS,
        "new_physical_fixture_ids": sorted(NEW_PHYSICAL_IDS),
        "new_private_fixture_ids": sorted(NEW_PRIVATE_IDS),
        "reserved_function_ids": sorted(EXPECTED_ADDITIVE_FUNCTION_IDS),
        "source_widget_ids": sorted(widget_map(virtual_console)),
    }
    try:
        provider.extend_workspace(extension_context)
    except Exception as exc:
        raise BuildError(f"Creative provider workspace extension failed: {exc}") from exc

    candidate_functions = function_map(engine)
    candidate_fixtures = fixture_map(engine)
    require(
        source_function_ids <= frozenset(candidate_functions),
        "An existing V26 Function ID was removed during the V27 build.",
    )
    require(
        frozenset(candidate_functions) - source_function_ids == EXPECTED_ADDITIVE_FUNCTION_IDS,
        "V27 must add exactly reserved Functions 2175-2184.",
    )
    require(
        len(candidate_functions) == EXPECTED_CANDIDATE_FUNCTION_COUNT,
        f"V27 must contain exactly {EXPECTED_CANDIDATE_FUNCTION_COUNT} Functions.",
    )
    additive_types = Counter(
        candidate_functions[function_id].get("Type")
        for function_id in EXPECTED_ADDITIVE_FUNCTION_IDS
    )
    require(
        additive_types == Counter({"Scene": 9, "Chaser": 1}),
        "V27 additive controls must be nine position Scenes and one movement Chaser.",
    )
    for function_id in source_function_ids:
        function = candidate_functions[function_id]
        require(
            canonical(function, drop_direct=frozenset({"FixtureVal"}))
            == source_function_skeletons[function_id],
            f"Function {function_id} name/type/timing/reference skeleton changed.",
        )
        if function_id not in live_scene_categories:
            require(
                canonical(function) == source_function_xml[function_id],
                f"Non-live Function {function_id} changed during the creative pass.",
            )
    for fixture_id, signature in source_fixture_xml.items():
        require(canonical(candidate_fixtures[fixture_id]) == signature, f"Existing Fixture {fixture_id} changed.")
    require(workspace_shell_signature(root) == source_shell, "Workspace Creator/root structure changed.")
    require(virtual_console.attrib == source_vc.attrib, "Virtual Console root settings changed.")
    validate_console_extension(source_vc, virtual_console)
    validate_additive_movement(
        candidate_functions,
        virtual_console,
        frozenset(widget_map(source_vc)),
    )
    require(
        io_operational_signature(engine.find(q("InputOutputMap"))) == source_io,
        "Input/Output operational mapping changed during the V27 creative pass.",
    )
    validate_monitor_extension(source_monitor, engine.find(q("Monitor")))
    validate_all_function_references(candidate_functions)
    validate_fixture_spans(candidate_fixtures)
    validate_new_fixture_pairs(candidate_fixtures)
    validate_live_coverage(candidate_functions, candidate_fixtures, raw_scene_ids, priority_scene_ids)
    for scene_id in UI_CONTROL_SCENE_IDS:
        require(
            not candidate_functions[scene_id].findall(q("FixtureVal")),
            f"UI command Scene {scene_id} must remain empty.",
        )
    validate_integer_candidate_dmx(candidate_functions)

    payload = serialise_workspace(root)
    write_atomic(output, payload, force=force)
    require(sha256(source) == EXPECTED_V26_SHA256, "The protected V26 source changed during the build.")

    print("PASS: V27 full-rig workspace build")
    print(f"  Source V26 SHA-256: {EXPECTED_V26_SHA256}")
    print(f"  Creative program: {provider.PROGRAM_VERSION}")
    print("  Added fixtures: Wash FX Hex x1; Focus Spot Two x2; private duplicates x3")
    print("  Live creative coverage: 1,024 Autoloop + 102 Priority + 5 performance + 9 override Scenes")
    print("  Existing Function IDs/timing/references/bindings: preserved from V26")
    print("  Additive movement controls: Functions 2175-2184 plus new console/monitor items")
    print(f"  Output SHA-256: {sha256(output)}")


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
    output = script.with_name("IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw")
    provider = script.with_name("V27FullRigCreative.py")
    return source, output, provider


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    source, output, provider = default_paths()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-workspace", type=Path, default=source)
    parser.add_argument("--output-workspace", type=Path, default=output)
    parser.add_argument("--creative-provider", type=Path, default=provider)
    parser.add_argument("--force", action="store_true", help="Replace an existing V27 generated output.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        build(
            args.source_workspace,
            args.output_workspace,
            args.creative_provider,
            force=args.force,
        )
    except BuildError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
