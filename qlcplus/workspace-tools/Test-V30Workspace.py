#!/usr/bin/env python3
"""Validate the V30 full-rig performance-recovery workspace."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

NS_URI = "http://www.qlcplus.org/Workspace"
NS = f"{{{NS_URI}}}"
SOURCE_SHA256 = "a4f7559930e93485f3ea2815a0b44ca8e40acdfcc43fb3b0430e863c64b2dc4b"
RAW_LOOP_IDS = tuple(range(532, 660))
SEQUENTIAL_PARENT_IDS = tuple(range(788, 793))
RANDOM_PARENT_IDS = tuple(range(793, 798))
COLOR_OVERRIDE_IDS = tuple(range(37, 46))
HOLD_OVERRIDE_IDS = tuple(range(2185, 2194))
OLD_SPEED_DIAL_IDS = tuple(range(1022, 1045))
HOLD_BUTTON_IDS = tuple(range(1590, 1599))
SPEED_DIAL_IDS = tuple(range(1600, 1694))
VARIABLE_TIMING_IDS = (573, 576, 632, 633, 635, 636, 645, 657, 658)
SHUFFLE_MULTIPLIERS = (13, 21, 29, 37, 45)
SHUFFLE_OFFSETS = (7, 19, 3, 25, 51)
PRESET_NAMES = ("0.25x", "0.5x", "1x", "2x", "4x")
PRESET_CHANNELS = (470, 471, 472, 473, 474)


def q(name: str) -> str:
    return NS + name


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def semantic(element: ET.Element):
    return (
        element.tag,
        tuple(sorted(element.attrib.items())),
        (element.text or "").strip(),
        tuple(semantic(child) for child in list(element)),
    )


def function_map(root: ET.Element) -> dict[int, ET.Element]:
    engine = root.find(q("Engine"))
    require(engine is not None, "Engine is missing")
    result: dict[int, ET.Element] = {}
    for function in engine.findall(q("Function")):
        fid = int(function.get("ID", "-1"))
        require(fid not in result, f"duplicate Function ID {fid}")
        result[fid] = function
    return result


def widget_map(root: ET.Element) -> dict[int, ET.Element]:
    console = root.find(q("VirtualConsole"))
    require(console is not None, "VirtualConsole is missing")
    result: dict[int, ET.Element] = {}
    for node in console.iter():
        raw_id = node.get("ID")
        if raw_id is None or node.find(q("WindowState")) is None:
            continue
        wid = int(raw_id)
        require(wid not in result, f"duplicate widget ID {wid}")
        result[wid] = node
    return result


def parent_map(root: ET.Element) -> dict[ET.Element, ET.Element]:
    return {child: parent for parent in root.iter() for child in parent}


def fixture_values(scene: ET.Element) -> dict[int, str]:
    return {int(value.get("ID", "-1")): (value.text or "")
            for value in scene.findall(q("FixtureVal"))}


def step_total(step: ET.Element) -> int:
    if step.get("Duration") is not None:
        return int(step.get("Duration", "0"))
    return (int(step.get("FadeIn", "0")) + int(step.get("Hold", "0")) +
            int(step.get("FadeOut", "0")))


def speed_tuple(function: ET.Element) -> tuple[int, int, int]:
    speed = function.find(q("Speed"))
    require(speed is not None, f"Function {function.get('ID')} has no Speed")
    return (int(speed.get("FadeIn", "0")),
            int(speed.get("FadeOut", "0")),
            int(speed.get("Duration", "0")))


def step_fids(function: ET.Element) -> list[int]:
    return [int(step.text or "-1") for step in function.findall(q("Step"))]


def reverse_low_bits(value: int, bits: int) -> int:
    result = 0
    for bit in range(bits):
        result = (result << 1) | ((value >> bit) & 1)
    return result


def shuffled_step_at(position: int, count: int, target: int) -> int:
    bits = 7 if count == 128 else 5
    return ((reverse_low_bits(position, bits) * SHUFFLE_MULTIPLIERS[target]) +
            SHUFFLE_OFFSETS[target]) & (count - 1)


def encode_seek(bank: int, pad: int, all_banks: bool,
                randomized: bool) -> int:
    count = 128 if all_banks else 32
    target = 4 if all_banks else bank
    logical = bank * 32 + pad if all_banks else pad
    position = logical
    if randomized:
        position = next(pos for pos in range(count)
                        if shuffled_step_at(pos, count, target) == logical)
    return 255 - position * (256 // count)


def decode_seek(value: int, count: int) -> int:
    level = 255 - value
    step_size = 256.0 / count
    if level >= 256.0 - step_size:
        return count - 1
    return int(level / step_size)


def validate_fixture_patch(source: ET.Element, candidate: ET.Element) -> None:
    source_engine = source.find(q("Engine"))
    candidate_engine = candidate.find(q("Engine"))
    require(source_engine is not None and candidate_engine is not None,
            "Engine missing during fixture comparison")
    source_fixtures = source_engine.findall(q("Fixture"))
    candidate_fixtures = candidate_engine.findall(q("Fixture"))
    require(len(source_fixtures) == 22 and len(candidate_fixtures) == 22,
            "V30 must preserve the 22-fixture physical/private patch")
    require([semantic(node) for node in source_fixtures] ==
            [semantic(node) for node in candidate_fixtures],
            "fixture definitions or addresses changed from V27")


def validate_functions(source_root: ET.Element,
                       candidate_root: ET.Element) -> None:
    source = function_map(source_root)
    candidate = function_map(candidate_root)
    require(len(source) == 2100, "reviewed V27 Function count changed")
    require(len(candidate) == 2109, "V30 must contain 2109 Functions")
    require(set(candidate) - set(source) == set(HOLD_OVERRIDE_IDS),
            "unexpected new V30 Function IDs")
    require(set(source) - set(candidate) == set(),
            "V30 removed an existing Function")

    allowed_changes = (set(RAW_LOOP_IDS) | set(RANDOM_PARENT_IDS) |
                       set(COLOR_OVERRIDE_IDS))
    for fid in set(source) - allowed_changes:
        require(semantic(source[fid]) == semantic(candidate[fid]),
                f"unrelated Function {fid} changed")

    variable_found: list[int] = []
    for fid in RAW_LOOP_IDS:
        before = source[fid]
        after = candidate[fid]
        require(before.get("Name") == after.get("Name") and
                before.get("Type") == after.get("Type"),
                f"raw Autoloop {fid} identity changed")
        require(speed_tuple(before) == speed_tuple(after),
                f"raw Autoloop {fid} top timing changed")
        require(step_fids(before) == step_fids(after),
                f"raw Autoloop {fid} creative step order changed")
        original_timings = {
            (int(step.get("FadeIn", "0")),
             int(step.get("FadeOut", "0")), step_total(step))
            for step in before.findall(q("Step"))
        }
        if len(original_timings) > 1:
            variable_found.append(fid)

        modes = after.find(q("SpeedModes"))
        require(modes is not None and
                modes.get("FadeIn") == modes.get("FadeOut") ==
                modes.get("Duration") == "Common",
                f"raw Autoloop {fid} is not fully Common-timed")
        fade_in, fade_out, duration = speed_tuple(after)
        for step in after.findall(q("Step")):
            require(int(step.get("FadeIn", "-1")) == fade_in and
                    int(step.get("FadeOut", "-1")) == fade_out and
                    step_total(step) == duration,
                    f"raw Autoloop {fid} step timing is not normalized")
    require(tuple(variable_found) == VARIABLE_TIMING_IDS,
            f"unexpected variable-timing source loops: {variable_found}")

    for fid in SEQUENTIAL_PARENT_IDS:
        require(semantic(source[fid]) == semantic(candidate[fid]),
                f"sequential autoplay parent {fid} drifted")

    for target, fid in enumerate(RANDOM_PARENT_IDS):
        before = source[fid]
        after = candidate[fid]
        require(speed_tuple(before) == speed_tuple(after),
                f"random autoplay parent {fid} timing changed")
        run_order = after.find(q("RunOrder"))
        require(run_order is not None and run_order.text == "Loop",
                f"randomized-cycle parent {fid} must use Loop order")
        original = step_fids(before)
        expected = [original[shuffled_step_at(pos, len(original), target)]
                    for pos in range(len(original))]
        require(step_fids(after) == expected,
                f"randomized-cycle parent {fid} permutation is wrong")
        require("RANDOMIZED CYCLE" in after.get("Name", ""),
                f"randomized-cycle parent {fid} is mislabeled")

    for index, fid in enumerate(COLOR_OVERRIDE_IDS):
        before_values = fixture_values(source[fid])
        after_values = fixture_values(candidate[fid])
        require(set(before_values) == set(range(11)),
                f"source color Scene {fid} physical coverage changed")
        require(set(after_values) == set(range(11)) | set(range(100, 111)),
                f"color Scene {fid} does not cover all physical/private fixtures")
        for physical_id in range(11):
            require(after_values[physical_id] == before_values[physical_id],
                    f"color Scene {fid} changed physical fixture {physical_id}")
            require(after_values[physical_id + 100] == before_values[physical_id],
                    f"color Scene {fid} private fixture clone is wrong")

        hold_id = HOLD_OVERRIDE_IDS[index]
        hold_values = fixture_values(candidate[hold_id])
        require(hold_values == after_values,
                f"hold color Scene {hold_id} differs from latch Scene {fid}")
        require(candidate[hold_id].get("Type") == "Scene" and
                "COLOR HOLD OVERRIDE" in candidate[hold_id].get("Name", ""),
                f"hold color Function {hold_id} identity is wrong")


def input_hits(root: ET.Element, channel: int) -> list[ET.Element]:
    console = root.find(q("VirtualConsole"))
    require(console is not None, "VirtualConsole missing")
    return [node for node in console.iter(q("Input"))
            if node.get("Universe") == "1" and
            node.get("Channel") == str(channel)]


def validate_speed_dials(candidate_root: ET.Element,
                         widgets: dict[int, ET.Element]) -> None:
    speed_dials = [node for node in candidate_root.iter(q("SpeedDial"))]
    require(len(speed_dials) == 95,
            f"expected dwell engine plus 94 V30 speed dials; found {len(speed_dials)}")
    require(1021 in widgets and widgets[1021].tag == q("SpeedDial"),
            "Autoplay dwell engine SpeedDial was lost")
    require(all(widget_id not in widgets for widget_id in OLD_SPEED_DIAL_IDS),
            "an obsolete V27 speed dial remains")
    require(all(widget_id in widgets for widget_id in SPEED_DIAL_IDS),
            "V30 speed dial ID range is incomplete")

    duration_coverage: dict[int, int] = {fid: 0 for fid in RAW_LOOP_IDS}
    fade_coverage: dict[int, int] = {fid: 0 for fid in RAW_LOOP_IDS}
    expected_ids = iter(SPEED_DIAL_IDS)
    for dial in [widgets[wid] for wid in SPEED_DIAL_IDS]:
        require(int(dial.get("ID", "-1")) == next(expected_ids),
                "V30 speed dial ordering is unstable")
        caption = dial.get("Caption", "")
        is_duration = " DURATION " in caption
        is_fade = " FADE IN " in caption
        require(is_duration ^ is_fade,
                f"speed dial {dial.get('ID')} has ambiguous timing type")
        base = int((dial.findtext(q("Time")) or "0"))
        absolute = dial.find(q("AbsoluteValue"))
        require(absolute is not None, f"speed dial {dial.get('ID')} has no range")
        expected_values = (base * 4, base * 2, base,
                           max(1, base // 2), max(1, base // 4))
        require(int(absolute.get("Minimum", "-1")) == min(expected_values) and
                int(absolute.get("Maximum", "-1")) == max(expected_values),
                f"speed dial {dial.get('ID')} range is wrong")

        bindings = dial.findall(q("Function"))
        require(bindings, f"speed dial {dial.get('ID')} has no Functions")
        for binding in bindings:
            fid = int(binding.text or "-1")
            require(fid in duration_coverage,
                    f"speed dial {dial.get('ID')} controls non-raw Function {fid}")
            require(binding.get("FadeOut") == "0",
                    f"speed dial {dial.get('ID')} would introduce FadeOut")
            if is_duration:
                require(binding.get("Duration") == "6" and
                        binding.get("FadeIn") == "0",
                        f"duration dial {dial.get('ID')} has wrong factors")
                duration_coverage[fid] += 1
            else:
                require(binding.get("FadeIn") == "6" and
                        binding.get("Duration") == "0",
                        f"fade dial {dial.get('ID')} has wrong factors")
                fade_coverage[fid] += 1

        presets = dial.findall(q("Preset"))
        require(len(presets) == 5, f"speed dial {dial.get('ID')} has wrong presets")
        for index, preset in enumerate(presets):
            require(int(preset.get("ID", "-1")) == index and
                    preset.findtext(q("Name")) == PRESET_NAMES[index] and
                    int(preset.findtext(q("Value")) or "-1") ==
                    expected_values[index],
                    f"speed dial {dial.get('ID')} preset {index} is wrong")
            input_node = preset.find(q("Input"))
            require(input_node is not None and
                    input_node.get("Universe") == "1" and
                    input_node.get("Channel") == str(PRESET_CHANNELS[index]),
                    f"speed dial {dial.get('ID')} preset {index} input is wrong")

    require(set(duration_coverage.values()) == {1},
            "every raw Autoloop must have exactly one duration controller")
    require(set(fade_coverage.values()) == {1},
            "every raw Autoloop must have exactly one FadeIn controller")


def validate_widgets(source_root: ET.Element,
                     candidate_root: ET.Element) -> None:
    source = widget_map(source_root)
    candidate = widget_map(candidate_root)
    require(len(source) == 573, "reviewed V27 widget count changed")
    require(len(candidate) == 653, "V30 must contain 653 widgets")
    require(set(source) - set(candidate) == set(OLD_SPEED_DIAL_IDS),
            "V30 removed unexpected source widgets")
    require(set(candidate) - set(source) ==
            set(HOLD_BUTTON_IDS) | set(SPEED_DIAL_IDS),
            "V30 added unexpected widget IDs")

    source_parents = parent_map(source_root)
    excluded: set[int] = set(OLD_SPEED_DIAL_IDS) | {
        1000, 1252, 1343, 1588, *range(1578, 1587)
    }
    # Parent widgets semantically include their modified descendants.
    for widget_id in list(excluded):
        node = source.get(widget_id)
        while node is not None and node in source_parents:
            node = source_parents[node]
            if node.get("ID") is not None and node.find(q("WindowState")) is not None:
                excluded.add(int(node.get("ID", "-1")))
    for widget_id in set(source) & set(candidate) - excluded:
        require(semantic(source[widget_id]) == semantic(candidate[widget_id]),
                f"unrelated widget {widget_id} changed")

    for index, widget_id in enumerate(range(1578, 1587)):
        button = candidate[widget_id]
        function = button.find(q("Function"))
        input_node = button.find(q("Input"))
        require(function is not None and function.get("ID") == str(2175 + index),
                f"Focus position button {widget_id} target changed")
        require(input_node is not None and input_node.get("Universe") == "1" and
                input_node.get("Channel") == str(128 + index),
                f"Focus position button {widget_id} is not Shift+pad {index + 1}")
        require(len(input_hits(candidate_root, 128 + index)) == 1,
                f"Shift+pad channel {128 + index} is not exclusive")

    for index, widget_id in enumerate(HOLD_BUTTON_IDS):
        button = candidate[widget_id]
        function = button.find(q("Function"))
        action = button.find(q("Action"))
        input_node = button.find(q("Input"))
        window = button.find(q("WindowState"))
        require(function is not None and
                function.get("ID") == str(HOLD_OVERRIDE_IDS[index]),
                f"hold button {widget_id} targets wrong Function")
        require(action is not None and action.text == "Flash" and
                action.get("Override") == "1" and action.get("ForceLTP") == "1",
                f"hold button {widget_id} is not an LTP Flash override")
        require(input_node is not None and input_node.get("Universe") == "1" and
                input_node.get("Channel") == str(164 + index),
                f"hold button {widget_id} has wrong shifted color input")
        require(window is not None and window.get("Width") == "1" and
                window.get("Height") == "1",
                f"hold button {widget_id} is not hidden")
        require(len(input_hits(candidate_root, 164 + index)) == 1,
                f"Shift+color channel {164 + index} is not exclusive")

    for index, widget_id in enumerate(range(1253, 1262)):
        button = candidate[widget_id]
        function = button.find(q("Function"))
        input_node = button.find(q("Input"))
        require(function is not None and function.get("ID") == str(37 + index),
                f"latched color button {widget_id} target changed")
        require(input_node is not None and input_node.get("Channel") == str(36 + index),
                f"latched color button {widget_id} input changed")
        require(len(input_hits(candidate_root, 36 + index)) == 1,
                f"base color channel {36 + index} is not exclusive")

    validate_speed_dials(candidate_root, candidate)
    require("SHIFT + PRESS = HOLD" in candidate[1252].get("Caption", ""),
            "V30 color hold operator label is missing")
    require("SHIFT + PERFORMANCE PADS 1–9" in
            candidate[1343].get("Caption", ""),
            "V30 Focus position remap is not documented on the console")


def validate_autoplay_start(candidate_root: ET.Element) -> None:
    functions = function_map(candidate_root)
    for bank in range(4):
        sequential_parent = functions[788 + bank]
        random_parent = functions[793 + bank]
        sequential_steps = step_fids(sequential_parent)
        random_steps = step_fids(random_parent)
        for pad in range(32):
            seek = encode_seek(bank, pad, False, False)
            position = decode_seek(seek, 32)
            require(sequential_steps[position] == 532 + bank * 32 + pad,
                    f"sequential Bank {bank + 1} pad {pad + 1} starts wrong loop")
            nudge = 1 if seek == 0 else seek - 1
            require(decode_seek(nudge, 32) == position,
                    "sequential repeated-seek nudge crosses a step")

            seek = encode_seek(bank, pad, False, True)
            position = decode_seek(seek, 32)
            require(random_steps[position] == 532 + bank * 32 + pad,
                    f"randomized Bank {bank + 1} pad {pad + 1} starts wrong loop")
            nudge = 1 if seek == 0 else seek - 1
            require(decode_seek(nudge, 32) == position,
                    "randomized repeated-seek nudge crosses a step")

    sequential_all = step_fids(functions[792])
    random_all = step_fids(functions[797])
    for bank in range(4):
        for pad in range(32):
            wanted = 532 + bank * 32 + pad
            seek = encode_seek(bank, pad, True, False)
            position = decode_seek(seek, 128)
            require(sequential_all[position] == wanted,
                    f"sequential All bank {bank + 1} pad {pad + 1} starts wrong loop")
            seek = encode_seek(bank, pad, True, True)
            position = decode_seek(seek, 128)
            require(random_all[position] == wanted,
                    f"randomized All bank {bank + 1} pad {pad + 1} starts wrong loop")
            nudge = 1 if seek == 0 else seek - 1
            require(decode_seek(nudge, 128) == position,
                    "All-bank repeated-seek nudge crosses a step")


def validate_io(source_root: ET.Element, candidate_root: ET.Element) -> None:
    source_map = source_root.find(f"{q('Engine')}/{q('InputOutputMap')}")
    candidate_map = candidate_root.find(f"{q('Engine')}/{q('InputOutputMap')}")
    require(source_map is not None and candidate_map is not None,
            "InputOutputMap is missing")
    require(semantic(source_map) == semantic(candidate_map),
            "fixture I/O patch changed from V27")
    universe = next((node for node in candidate_map.findall(q("Universe"))
                     if node.get("ID") == "1"), None)
    require(universe is not None, "Control One universe 1 is missing")
    feedback = universe.findall(q("Feedback"))
    require(len(feedback) == 1 and
            feedback[0].get("UID") == "soundswitch:controlone:surface",
            "Control One must use one unified Surface feedback patch")
    priority_output = next((node for node in candidate_map.findall(q("Universe"))
                            if node.get("ID") == "2"), None)
    require(priority_output is not None and
            any(node.get("UID") == "soundswitch:priority-layer"
                for node in priority_output.findall(q("Output"))),
            "private Priority Layer output is missing")


def validate(source_path: Path, candidate_path: Path) -> None:
    require(source_path.is_file(), f"source not found: {source_path}")
    require(candidate_path.is_file(), f"candidate not found: {candidate_path}")
    require(sha256(source_path) == SOURCE_SHA256,
            "source is not the reviewed V27 workspace")
    source_root = ET.parse(source_path).getroot()
    candidate_root = ET.parse(candidate_path).getroot()
    require(source_root.tag == candidate_root.tag == q("Workspace"),
            "workspace root is invalid")

    validate_fixture_patch(source_root, candidate_root)
    validate_functions(source_root, candidate_root)
    validate_widgets(source_root, candidate_root)
    validate_autoplay_start(candidate_root)
    validate_io(source_root, candidate_root)

    print("PASS: V30 workspace validation")
    print("  Fixture patch: all 22 physical/private fixtures preserved")
    print("  Speed: 128 raw loops have live Common timing and 94 controls")
    print("  Auto start: all 320 sequential/randomized bank/all selections exact")
    print("  Priority: private patch and unified feedback preserved")
    print("  Color: latch + Shift-hold cover every physical/private fixture")
    print("  Focus positions: Shift + performance pads 1-9")
    print(f"  Candidate SHA-256: {sha256(candidate_path)}")


def parse_args(argv: list[str]) -> argparse.Namespace:
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path,
                        default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V27-FULL-RIG.qxw")
    parser.add_argument("--candidate", type=Path,
                        default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V30-PERFORMANCE-RECOVERY.qxw")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        validate(args.source.resolve(), args.candidate.resolve())
        return 0
    except (OSError, RuntimeError, ET.ParseError, StopIteration) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
