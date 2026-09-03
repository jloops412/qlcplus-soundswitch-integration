#!/usr/bin/env python3
"""Build the deterministic V30 full-rig performance-recovery workspace.

V30 deliberately starts from the reviewed V27 full-rig workspace. It repairs
runtime contracts that drifted in later local derivatives without redesigning
the Autoloop library itself:

* every raw Autoloop uses Common timing, so QLC+'s SpeedDial changes the timing
  the ChaserRunner actually reads;
* hidden speed groups are regenerated from final V27 timing values;
* sequential and randomized autoplay parents support exact selected-loop start;
* shifted color pads get independent momentary hold Scenes;
* all color overrides address physical and private Priority fixtures;
* Focus position shortcuts move to Shift + performance pads 1-9.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import os
from collections import defaultdict
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

NS_URI = "http://www.qlcplus.org/Workspace"
NS = f"{{{NS_URI}}}"
ET.register_namespace("", NS_URI)

SOURCE_SHA256 = "a4f7559930e93485f3ea2815a0b44ca8e40acdfcc43fb3b0430e863c64b2dc4b"
RAW_LOOP_IDS = tuple(range(532, 660))
SEQUENTIAL_PARENT_IDS = tuple(range(788, 793))
RANDOM_PARENT_IDS = tuple(range(793, 798))
COLOR_OVERRIDE_IDS = tuple(range(37, 46))
HOLD_OVERRIDE_IDS = tuple(range(2185, 2194))
OLD_SPEED_DIAL_IDS = tuple(range(1022, 1045))
HOLD_BUTTON_IDS = tuple(range(1590, 1599))
NEW_SPEED_DIAL_FIRST_ID = 1600

SHUFFLE_MULTIPLIERS = (13, 21, 29, 37, 45)
SHUFFLE_OFFSETS = (7, 19, 3, 25, 51)
PRESET_NAMES = ("0.25x", "0.5x", "1x", "2x", "4x")
PRESET_CHANNELS = (470, 471, 472, 473, 474)


def q(name: str) -> str:
    return NS + name


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def function_map(engine: ET.Element) -> dict[int, ET.Element]:
    result: dict[int, ET.Element] = {}
    for function in engine.findall(q("Function")):
        fid = int(function.get("ID", "-1"))
        require(fid not in result, f"duplicate Function ID {fid}")
        result[fid] = function
    return result


def widget_map(virtual_console: ET.Element) -> dict[int, ET.Element]:
    result: dict[int, ET.Element] = {}
    for widget in virtual_console.iter():
        raw_id = widget.get("ID")
        if raw_id is None or widget.find(q("WindowState")) is None:
            continue
        wid = int(raw_id)
        require(wid not in result, f"duplicate Virtual Console widget ID {wid}")
        result[wid] = widget
    return result


def parent_map(root: ET.Element) -> dict[ET.Element, ET.Element]:
    return {child: parent for parent in root.iter() for child in parent}


def step_total(step: ET.Element) -> int:
    duration = step.get("Duration")
    if duration is not None:
        return int(duration)
    return (int(step.get("FadeIn", "0")) + int(step.get("Hold", "0")) +
            int(step.get("FadeOut", "0")))


def normalize_raw_chaser(function: ET.Element) -> bool:
    speed = function.find(q("Speed"))
    modes = function.find(q("SpeedModes"))
    require(speed is not None and modes is not None,
            f"raw Chaser {function.get('ID')} has no Speed/SpeedModes")

    fade_in = int(speed.get("FadeIn", "0"))
    fade_out = int(speed.get("FadeOut", "0"))
    duration = int(speed.get("Duration", "0"))
    require(duration >= fade_in + fade_out,
            f"raw Chaser {function.get('ID')} has invalid top-level timing")

    original_timing = {
        (int(step.get("FadeIn", "0")), int(step.get("FadeOut", "0")),
         step_total(step))
        for step in function.findall(q("Step"))
    }
    variable = len(original_timing) > 1

    modes.set("FadeIn", "Common")
    modes.set("FadeOut", "Common")
    modes.set("Duration", "Common")
    for step in function.findall(q("Step")):
        step.set("FadeIn", str(fade_in))
        step.set("FadeOut", str(fade_out))
        if step.get("Duration") is not None:
            step.set("Duration", str(duration))
        elif step.get("Hold") is not None:
            step.set("Hold", str(duration - fade_in - fade_out))
        else:
            step.set("Duration", str(duration))
    return variable


def reverse_low_bits(value: int, bits: int) -> int:
    result = 0
    for bit in range(bits):
        result = (result << 1) | ((value >> bit) & 1)
    return result


def shuffled_step_at(position: int, count: int, target: int) -> int:
    bits = 7 if count == 128 else 5
    return ((reverse_low_bits(position, bits) * SHUFFLE_MULTIPLIERS[target]) +
            SHUFFLE_OFFSETS[target]) & (count - 1)


def convert_random_parent(function: ET.Element, target: int) -> None:
    steps = function.findall(q("Step"))
    count = len(steps)
    require(count in (32, 128),
            f"random parent {function.get('ID')} has {count} steps")
    original_fids = [int(step.text or "-1") for step in steps]
    permuted_fids = [original_fids[shuffled_step_at(position, count, target)]
                     for position in range(count)]
    require(len(set(permuted_fids)) == count,
            f"random parent {function.get('ID')} is not a permutation")

    run_order = function.find(q("RunOrder"))
    require(run_order is not None, f"random parent {function.get('ID')} has no RunOrder")
    run_order.text = "Loop"
    name = function.get("Name", "")
    function.set("Name", name.replace(" - RANDOM", " - RANDOMIZED CYCLE"))
    for number, (step, fid) in enumerate(zip(steps, permuted_fids)):
        step.set("Number", str(number))
        step.text = str(fid)


def add_private_fixture_values(scene: ET.Element) -> None:
    values = scene.findall(q("FixtureVal"))
    physical = {int(value.get("ID", "-1")): value for value in values
                if 0 <= int(value.get("ID", "-1")) <= 10}
    require(set(physical) == set(range(11)),
            f"Scene {scene.get('ID')} does not cover all 11 physical fixtures")
    existing = {int(value.get("ID", "-1")) for value in values}
    for fixture_id in range(11):
        private_id = fixture_id + 100
        if private_id in existing:
            continue
        duplicate = copy.deepcopy(physical[fixture_id])
        duplicate.set("ID", str(private_id))
        scene.append(duplicate)


def clone_hold_scenes(engine: ET.Element,
                      functions: dict[int, ET.Element]) -> None:
    monitor = engine.find(q("Monitor"))
    require(monitor is not None, "Engine Monitor node is missing")
    insertion_index = list(engine).index(monitor)
    for index, source_id in enumerate(COLOR_OVERRIDE_IDS):
        source = functions[source_id]
        clone = copy.deepcopy(source)
        new_id = HOLD_OVERRIDE_IDS[index]
        clone.set("ID", str(new_id))
        clone.set("Name", source.get("Name", "COLOR OVERRIDE").replace(
            "COLOR OVERRIDE", "COLOR HOLD OVERRIDE"))
        engine.insert(insertion_index + index, clone)
        functions[new_id] = clone


def replace_text_child(parent: ET.Element, name: str, text: str) -> None:
    child = parent.find(q(name))
    require(child is not None, f"{parent.tag} is missing {name}")
    child.text = text


def build_hold_buttons(virtual_console: ET.Element,
                       widgets: dict[int, ET.Element]) -> None:
    page = widgets[0]
    for index, source_widget_id in enumerate(range(1253, 1262)):
        source = widgets[source_widget_id]
        button = copy.deepcopy(source)
        button.set("ID", str(HOLD_BUTTON_IDS[index]))
        button.set("Caption", f"HOLD {source.get('Caption', '')}")
        function = button.find(q("Function"))
        input_node = button.find(q("Input"))
        window = button.find(q("WindowState"))
        action = button.find(q("Action"))
        require(function is not None and input_node is not None and
                window is not None and action is not None,
                f"source color button {source_widget_id} is incomplete")
        function.set("ID", str(HOLD_OVERRIDE_IDS[index]))
        input_node.set("Universe", "1")
        input_node.set("Channel", str(164 + index))
        action.text = "Flash"
        action.set("Override", "1")
        action.set("ForceLTP", "1")
        window.set("Visible", "True")
        window.set("X", "4")
        window.set("Y", "4")
        window.set("Width", "1")
        window.set("Height", "1")
        page.append(button)
        widgets[HOLD_BUTTON_IDS[index]] = button


def remap_focus_position_shortcuts(widgets: dict[int, ET.Element]) -> None:
    for index, widget_id in enumerate(range(1578, 1587)):
        button = widgets[widget_id]
        input_node = button.find(q("Input"))
        require(input_node is not None,
                f"Focus position button {widget_id} has no input")
        input_node.set("Universe", "1")
        input_node.set("Channel", str(128 + index))
        original = button.get("Caption", str(index + 1))
        button.set("Caption", f"{index + 1}  {original.split(' ', 1)[-1]}")


def remove_old_speed_dials(virtual_console: ET.Element,
                           widgets: dict[int, ET.Element]) -> ET.Element:
    parents = parent_map(virtual_console)
    template = copy.deepcopy(widgets[1022])
    for widget_id in OLD_SPEED_DIAL_IDS:
        widget = widgets[widget_id]
        parents[widget].remove(widget)
        del widgets[widget_id]
    return template


def preset_values(base: int) -> tuple[int, int, int, int, int]:
    return (base * 4, base * 2, base, max(1, base // 2),
            max(1, base // 4))


def clear_speed_dial_contents(dial: ET.Element) -> None:
    for child in list(dial):
        if child.tag in (q("Function"), q("Preset")):
            dial.remove(child)


def add_speed_dial(page: ET.Element, template: ET.Element, widget_id: int,
                   timing_type: str, base: int,
                   function_ids: list[int]) -> ET.Element:
    dial = copy.deepcopy(template)
    dial.set("ID", str(widget_id))
    dial.set("Caption", f"V30 AUTOLOOP SPEED {timing_type} {base}")
    clear_speed_dial_contents(dial)

    absolute = dial.find(q("AbsoluteValue"))
    time = dial.find(q("Time"))
    window = dial.find(q("WindowState"))
    require(absolute is not None and time is not None and window is not None,
            "SpeedDial template is incomplete")
    values = preset_values(base)
    absolute.set("Minimum", str(min(values)))
    absolute.set("Maximum", str(max(values)))
    time.text = str(base)
    window.set("Visible", "True")
    window.set("X", "4")
    window.set("Y", "4")
    window.set("Width", "1")
    window.set("Height", "1")

    for fid in sorted(function_ids):
        binding = ET.Element(q("Function"))
        binding.set("FadeIn", "6" if timing_type == "FADE IN" else "0")
        binding.set("FadeOut", "0")
        binding.set("Duration", "6" if timing_type == "DURATION" else "0")
        binding.text = str(fid)
        dial.append(binding)

    for preset_id, (name, channel, value) in enumerate(zip(
            PRESET_NAMES, PRESET_CHANNELS, values)):
        preset = ET.SubElement(dial, q("Preset"), {"ID": str(preset_id)})
        ET.SubElement(preset, q("Name")).text = name
        ET.SubElement(preset, q("Value")).text = str(value)
        ET.SubElement(preset, q("Input"), {
            "Universe": "1", "Channel": str(channel)})

    page.append(dial)
    return dial


def rebuild_speed_dials(virtual_console: ET.Element,
                        functions: dict[int, ET.Element],
                        widgets: dict[int, ET.Element],
                        template: ET.Element) -> int:
    duration_groups: dict[int, list[int]] = defaultdict(list)
    fade_in_groups: dict[int, list[int]] = defaultdict(list)
    for fid in RAW_LOOP_IDS:
        speed = functions[fid].find(q("Speed"))
        require(speed is not None, f"raw loop {fid} has no Speed")
        duration_groups[int(speed.get("Duration", "0"))].append(fid)
        fade_in_groups[int(speed.get("FadeIn", "0"))].append(fid)

    page = widgets[0]
    widget_id = NEW_SPEED_DIAL_FIRST_ID
    for base in sorted(duration_groups):
        dial = add_speed_dial(page, template, widget_id, "DURATION", base,
                              duration_groups[base])
        widgets[widget_id] = dial
        widget_id += 1
    for base in sorted(fade_in_groups):
        dial = add_speed_dial(page, template, widget_id, "FADE IN", base,
                              fade_in_groups[base])
        widgets[widget_id] = dial
        widget_id += 1
    return widget_id - NEW_SPEED_DIAL_FIRST_ID


def update_operator_labels(widgets: dict[int, ET.Element]) -> None:
    widgets[1252].set(
        "Caption", "COLOR OVERRIDES • PRESS = LATCH • SHIFT + PRESS = HOLD")
    widgets[1343].set(
        "Caption",
        "COLOR PADS → latch full-rig color override\n"
        "SHIFT + COLOR PADS → momentary full-rig color hold\n"
        "SHIFT + PERFORMANCE PADS 1–9 → Focus A/B positions\n"
        "SHIFT + WHITE / BLACK / UV → latched effects")
    widgets[1000].set(
        "Caption",
        "CONTROL ONE • V30 PERFORMANCE RECOVERY\n"
        "WORKING SPEED • EXACT AUTO START • PRIORITY LOOKS • HOLD COLORS")
    if 1588 in widgets:
        widgets[1588].set(
            "Caption",
            widgets[1588].get("Caption", "").replace("IN V27", "IN V30"))


def save_workspace(root: ET.Element, destination: Path) -> None:
    ET.indent(root, space="  ")
    body = ET.tostring(root, encoding="unicode", short_empty_elements=True)
    text = '<?xml version="1.0" encoding="UTF-8"?>\n' + body + "\n"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(text.replace("\n", "\r\n").encode("utf-8"))
    os.replace(temporary, destination)


def build(source: Path, output: Path, force: bool) -> None:
    require(source.is_file(), f"source workspace not found: {source}")
    actual_hash = sha256(source)
    require(actual_hash == SOURCE_SHA256,
            f"V30 requires reviewed V27 SHA-256 {SOURCE_SHA256}; found {actual_hash}")
    if output.exists() and not force:
        raise RuntimeError(f"output already exists: {output}; pass --force")

    tree = ET.parse(source)
    root = tree.getroot()
    engine = root.find(q("Engine"))
    virtual_console = root.find(q("VirtualConsole"))
    require(engine is not None and virtual_console is not None,
            "workspace Engine or VirtualConsole is missing")
    functions = function_map(engine)
    widgets = widget_map(virtual_console)
    require(set(RAW_LOOP_IDS).issubset(functions), "raw Autoloop set is incomplete")
    require(set(COLOR_OVERRIDE_IDS).issubset(functions),
            "color override set is incomplete")
    require(set(RANDOM_PARENT_IDS).issubset(functions),
            "random autoplay parent set is incomplete")

    variable_raw_loops: list[int] = []
    for fid in RAW_LOOP_IDS:
        if normalize_raw_chaser(functions[fid]):
            variable_raw_loops.append(fid)

    for target, fid in enumerate(RANDOM_PARENT_IDS):
        convert_random_parent(functions[fid], target)

    for fid in COLOR_OVERRIDE_IDS:
        add_private_fixture_values(functions[fid])
    clone_hold_scenes(engine, functions)

    build_hold_buttons(virtual_console, widgets)
    remap_focus_position_shortcuts(widgets)
    speed_template = remove_old_speed_dials(virtual_console, widgets)
    speed_dial_count = rebuild_speed_dials(
        virtual_console, functions, widgets, speed_template)
    update_operator_labels(widgets)

    require(variable_raw_loops == [573, 576, 632, 633, 635, 636, 645, 657, 658],
            f"unexpected variable-timing raw loops: {variable_raw_loops}")
    require(speed_dial_count == 94,
            f"expected 94 regenerated speed dials; found {speed_dial_count}")
    require(len(function_map(engine)) == 2109,
            "V30 must contain 2109 Functions")
    require(len(widget_map(virtual_console)) == 653,
            "V30 must contain 653 Virtual Console widgets")

    save_workspace(root, output)
    # Parse once after serialization so a malformed output can never be
    # presented as a release candidate.
    ET.parse(output)

    print("PASS: V30 deterministic performance-recovery workspace build")
    print(f"  Source SHA-256: {actual_hash}")
    print(f"  Raw Autoloops normalized to Common timing: {len(RAW_LOOP_IDS)}")
    print(f"  Variable per-step loops flattened to reviewed top timing: {variable_raw_loops}")
    print(f"  Regenerated hidden speed groups: {speed_dial_count}")
    print("  Random order: five stable non-repeating randomized cycles")
    print("  Color override coverage: 11 physical + 11 private fixtures")
    print("  Shift + color: nine independent momentary hold layers")
    print(f"  Output SHA-256: {sha256(output)}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source", type=Path,
        default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw")
    parser.add_argument(
        "--output", type=Path,
        default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V30-PERFORMANCE-RECOVERY.qxw")
    parser.add_argument("--force", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        build(args.source.resolve(), args.output.resolve(), args.force)
        return 0
    except (OSError, RuntimeError, ET.ParseError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
