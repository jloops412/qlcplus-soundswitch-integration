#!/usr/bin/env python3
"""Independent V31 workspace regression gates, including QLC ownership rules.

This is structural validation. It does not claim that XML checks demonstrate
an exact Windows-host load, hardware output, creative quality, or gig fitness.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

import V31RigIntegrity as rig

NS = "{http://www.qlcplus.org/Workspace}"
SOURCE_SHA256 = "530e6d4166bc150442660bfbbff790bd1c1e4803f0c6d02c99302ebb30d79331"
COMMAND_FIDS = set(range(2200, 2328)) | set(range(2330, 2339)) | set(range(2340, 2349))
COMMAND_WIDGET_IDS = set(range(1800, 1928)) | set(range(1930, 1939)) | set(range(1940, 1949))
NEW_WIDGET_IDS = (set(range(1700, 1705)) | COMMAND_WIDGET_IDS |
                  set(range(1950, 1968)) | set(range(1970, 1988)) | set(range(1990, 1999)))
LABEL_IDS = {1000, 1252, 1343, 1576, 1588}
PAD_WIDGET_IDS = {1058 + bank * 43 + pad for bank in range(4) for pad in range(32)}


def q(name):
    return NS + name


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def semantic(node):
    return (node.tag, tuple(sorted(node.attrib.items())), (node.text or "").strip(),
            tuple(semantic(child) for child in node))


def widget_map(root):
    result = {}
    for node in root.find(q("VirtualConsole")).iter():
        if node.get("ID") is not None and node.find(q("WindowState")) is not None:
            wid = int(node.get("ID"))
            require(wid not in result, f"duplicate widget ID {wid}")
            result[wid] = node
    return result


def function_map(root):
    result = {}
    for node in root.find(q("Engine")).findall(q("Function")):
        fid = int(node.get("ID", "-1"))
        require(fid not in result and fid >= 0, f"duplicate/invalid Function ID {fid}")
        result[fid] = node
    return result


def ancestors(node, parents):
    result = []
    while node in parents:
        node = parents[node]
        result.append(node)
    return result


def absolute_rectangle(widget, parents):
    window = widget.find(q("WindowState"))
    x, y = int(window.get("X")), int(window.get("Y"))
    for parent in ancestors(widget, parents):
        parent_window = parent.find(q("WindowState"))
        if parent_window is not None:
            x += int(parent_window.get("X"))
            y += int(parent_window.get("Y"))
    return x, y, int(window.get("Width")), int(window.get("Height"))


def own_properties(widget):
    node = copy.deepcopy(widget)
    for child in list(node):
        if child.get("ID") is not None and child.find(q("WindowState")) is not None:
            node.remove(child)
    return node


def input_route(node, channel):
    inputs = node.findall(q("Input"))
    return len(inputs) == 1 and inputs[0].get("Universe") == "1" and inputs[0].get("Channel") == str(channel)


def hidden_active(node):
    window = node.find(q("WindowState"))
    return (window is not None and window.get("Visible") == "True" and
            window.get("Width") == window.get("Height") == "1" and
            int(window.get("X", "0")) < 0 and int(window.get("Y", "0")) < 0)


def flash_owner(node, fid, channel):
    action = node.find(q("Action"))
    require(node.find(q("Function")).get("ID") == str(fid) and
            action.text == "Flash" and action.get("Override") == "1" and
            action.get("ForceLTP") == "1" and input_route(node, channel) and
            hidden_active(node), f"invalid persistent Flash owner {node.get('ID')}")


def validate_functions(source, candidate):
    before, after = function_map(source), function_map(candidate)
    require(len(before) == 2109 and set(before) <= set(after), "V30 Function set was removed or changed")
    require(set(after) - set(before) == COMMAND_FIDS, "unexpected V31 Function additions")
    for fid, original in before.items():
        if fid not in rig.MIRROR_IDS:
            require(semantic(original) == semantic(after[fid]), f"unrelated Function {fid} changed")
        else:
            # The rig gate checks all physical and cloned values; retain every
            # other Scene property, including speed and identity, exactly.
            a, b = copy.deepcopy(original), copy.deepcopy(after[fid])
            for node in (a, b):
                for values in node.findall(q("FixtureVal")):
                    node.remove(values)
            require(semantic(a) == semantic(b), f"non-payload Scene property changed for {fid}")
    for fid in COMMAND_FIDS:
        fn = after[fid]
        require(fn.get("Type") == "Scene" and fn.get("Name", "").startswith("UI V31 — "),
                f"invalid command identity {fid}")
        require(len(fn) == 1 and fn[0].tag == q("Speed") and
                fn[0].attrib == {"FadeIn": "0", "FadeOut": "0", "Duration": "0"},
                f"command Scene {fid} must be empty and self-terminating")
    for fid, fn in after.items():
        refs = []
        if fn.get("Type") == "Chaser":
            refs = [int(step.text) for step in fn.findall(q("Step"))]
        elif fn.get("Type") == "Collection":
            refs = [int(step.text) for step in fn.findall(q("Step"))]
        require(all(ref in after for ref in refs), f"broken Function reference in {fid}")


def validate_controls(source, candidate):
    before, after = widget_map(source), widget_map(candidate)
    parents = {child: parent for parent in candidate.iter() for child in parent}
    source_parents = {child: parent for parent in source.iter() for child in parent}
    require(set(before) <= set(after), "V31 removed a V30 widget")
    require(set(after) - set(before) == NEW_WIDGET_IDS, "unexpected V31 widget additions")
    geometry_changed = PAD_WIDGET_IDS | set(range(1253, 1262)) | set(range(1578, 1587))
    for wid, original in before.items():
        changed = after[wid]
        require(parents[changed].get("ID") == source_parents[original].get("ID"),
                f"existing widget {wid} changed its parent")
        a, b = own_properties(original), own_properties(changed)
        if wid in LABEL_IDS:
            a.attrib.pop("Caption", None)
            b.attrib.pop("Caption", None)
        if wid in geometry_changed:
            require(hidden_active(changed), f"original owner {wid} was hidden by visibility instead of geometry")
            for node in (a, b):
                node.remove(node.find(q("WindowState")))
        if 1578 <= wid <= 1586:
            for node in (a, b):
                node.remove(node.find(q("Action")))
        if 1414 <= wid <= 1541:
            for input_node in b.findall(q("Input")):
                b.remove(input_node)
        require(semantic(a) == semantic(b), f"unrelated widget property changed for {wid}")

    commands = []
    for bank in range(4):
        for pad in range(32):
            index = bank * 32 + pad
            commands.append((1800 + index, 2200 + index, 900 + index))
            native = after[1058 + bank * 43 + pad]
            require(native.find(q("Function")).get("ID") == str(660 + index) and
                    native.find(q("Action")).text == "Toggle" and
                    any(node.get("ID") == "1046" for node in ancestors(native, parents)),
                    f"native manual owner {index} lost SoloFrame authority")
            proxy = after[1800 + index]
            require(parents[proxy].get("ID") == str(1701 + bank), "mouse pad is in the wrong bank lane")
            require(semantic(proxy.find(q("WindowState"))) ==
                    semantic(before[1058 + bank * 43 + pad].find(q("WindowState"))),
                    "mouse pad click area drifted")
    for index in range(9):
        commands.extend(((1930 + index, 2330 + index, 850 + index),
                         (1940 + index, 2340 + index, 860 + index)))
        flash_owner(after[1253 + index], 37 + index, 36 + index)
        for wid in (1578 + index, 1990 + index):
            flash_owner(after[wid], 2175 + index, 128 + index)
            # Paged Flash parents can auto-unflash when a page changes. Top
            # console pages do not mutate the C++ widget visibility property.
            require(not any(parent.find(q("Multipage")) is not None
                            for parent in ancestors(after[wid], parents)),
                    "position Flash owner has a multipage ancestor")
        require(parents[after[1990 + index]].get("ID") == "0", "Live position route missing")
        for frame_id, button_id, fid in ((1950 + index, 1970 + index, 37 + index),
                                         (1959 + index, 1979 + index, 2175 + index)):
            monitor, frame_node = after[button_id], after[frame_id]
            require(parents[monitor] is frame_node and frame_node.findtext(q("Disabled")) == "True" and
                    monitor.findtext(q("Action")) == "Toggle" and not monitor.findall(q("Input")) and
                    monitor.find(q("Function")).get("ID") == str(fid),
                    f"native monitor {button_id} gained control authority")

    for wid, fid, channel in commands:
        button = after[wid]
        require(button.find(q("Function")).get("ID") == str(fid) and
                button.findtext(q("Action")) == "Toggle" and input_route(button, channel),
                f"command route {wid} is invalid")
        require(not any(parent.tag == q("SoloFrame") for parent in ancestors(button, parents)),
                f"command {wid} would stop the playback owner through SoloFrame exclusivity")
        if 1800 <= wid < 1928:
            bank, pad = divmod(wid - 1800, 32)
            original_id = 1058 + bank * 43 + pad
        elif 1930 <= wid < 1939:
            original_id = 1253 + wid - 1930
        else:
            original_id = 1578 + wid - 1940
        require(absolute_rectangle(button, parents) ==
                absolute_rectangle(before[original_id], source_parents) and
                button.get("Caption") == before[original_id].get("Caption"),
                f"mouse command {wid} changed its original click area/caption")
        window, parent_window = button.find(q("WindowState")), parents[button].find(q("WindowState"))
        require(all(0 <= int(window.get(axis)) and
                    int(window.get(axis)) + int(window.get(size)) <= int(parent_window.get(size))
                    for axis, size in (("X", "Width"), ("Y", "Height"))),
                f"mouse command {wid} falls outside its frame")
        hits = [node for node in candidate.find(q("VirtualConsole")).iter(q("Input"))
                if node.get("Universe") == "1" and int(node.get("Channel", "-1")) & 0xffff == channel]
        require(len(hits) == 1, f"mouse command {channel} has an input collision")
    pad_frame = after[1700]
    require(parents[pad_frame].get("ID") == "1045" and pad_frame.get("Page") == "0", "mouse pads bypass mode paging")
    page_config = pad_frame.find(q("Multipage"))
    require(page_config is not None and page_config.get("PagesNum") == "4" and page_config.get("CurrentPage") == "0",
            "mouse pad bank pager is missing")
    for bank, shortcut in enumerate(pad_frame.findall(q("Shortcut"))):
        require(shortcut.get("Page") == str(bank) and input_route(shortcut, 32 + bank), "mouse bank shortcut drifted")
    require(len(pad_frame.findall(q("Shortcut"))) == 4, "mouse bank shortcuts incomplete")

    # Keep all old recovery contracts, rather than validating only the new work.
    spec = importlib.util.spec_from_file_location("v30_validation", Path(__file__).with_name("Test-V30Workspace.py"))
    v30 = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(v30)
    v30.validate_speed_dials(candidate, after)
    v30.validate_autoplay_start(candidate)
    v30.validate_io(source, candidate)
    for wid in range(1542, 1574):
        strip = copy.deepcopy(after[wid])
        for button in strip.findall(q("Button")):
            for input_node in button.findall(q("Input")):
                button.remove(input_node)
        require(semantic(before[wid]) == semantic(strip), "V26/V30 raw-loop monitoring strip regressed")
    for index in range(128):
        monitor = after[1414 + index]
        input_node = monitor.find(q("Input"))
        require(input_route(monitor, 1100 + index) and
                input_node.get("LowerValue") == "0" and input_node.get("UpperValue") == "255" and
                input_node.get("MonitorValue") == "255" and
                monitor.findtext(q("Action")) == "Toggle" and
                monitor.find(q("Function")).get("ID") == str(532 + index) and
                parents[monitor].findtext(q("Disabled")) == "True" and
                not any(node.tag == q("SoloFrame") for node in ancestors(monitor, parents)),
                f"raw-Chaser LED feedback route {index} is invalid")
        hits = [node for node in candidate.find(q("VirtualConsole")).iter(q("Input"))
                if node.get("Universe") == "1" and int(node.get("Channel", "-1")) & 0xffff == 1100 + index]
        require(len(hits) == 1, "raw-Chaser feedback channels must be dedicated")
    for wid in range(1590, 1599):
        require(semantic(before[wid]) == semantic(after[wid]), "V30 Shift color hold regressed")
    return {"native_owners": 138, "mouse_pad_commands": 128, "mouse_latch_commands": 18,
            "native_latch_monitors": 18, "native_raw_led_routes": 128,
            "preserved_mouse_rectangles": 146, "widgets": len(after)}


def validate_roots(source, candidate):
    require(source.tag == candidate.tag == q("Workspace"), "invalid workspace root")
    evidence = rig.validate(source, candidate)
    validate_functions(source, candidate)
    evidence.update(validate_controls(source, candidate))
    before, after = source.find(q("Engine")), candidate.find(q("Engine"))
    require([semantic(n) for n in before if n.tag != q("Function")] ==
            [semantic(n) for n in after if n.tag != q("Function")], "non-Function Engine configuration changed")
    def console_properties(root):
        return [semantic(node) for node in root.find(q("VirtualConsole"))
                if node.find(q("WindowState")) is None]
    require(console_properties(source) == console_properties(candidate),
            "Virtual Console global settings changed")
    require([semantic(node) for node in source if node.tag not in (q("Engine"), q("VirtualConsole"))] ==
            [semantic(node) for node in candidate if node.tag not in (q("Engine"), q("VirtualConsole"))],
            "unrelated workspace metadata changed")
    return evidence


def mutation_checks(source, candidate):
    def mutate(wid, fn):
        return lambda root: fn(widget_map(root)[wid])
    checks = [
        ("mouse command dropped", mutate(1800, lambda node: node.find(q("Input")).set("Channel", "899"))),
        ("native owner changed", mutate(1058, lambda node: node.find(q("Function")).set("ID", "661"))),
        ("owner visibility disabled", mutate(1253, lambda node: node.find(q("WindowState")).set("Visible", "False"))),
        ("position override removed", mutate(1990, lambda node: node.find(q("Action")).set("ForceLTP", "0"))),
        ("monitor input added", mutate(1970, lambda node: ET.SubElement(node, q("Input"), {"Universe": "1", "Channel": "36"}))),
        ("raw monitoring feedback dark", mutate(1414, lambda node: node.find(q("Input")).set("MonitorValue", "0"))),
    ]
    # Resolve the actual persistent mode widget independently of its numeric ID.
    mode_wid = next(wid for wid, node in widget_map(candidate).items()
                    if node.tag == q("Button") and node.find(q("Function")) is not None and
                    node.find(q("Function")).get("ID") == "1993")
    checks.append(("old mode command lost", mutate(mode_wid, lambda node: node.find(q("Function")).set("ID", "1992"))))
    def move_command_into_solo(root):
        ws = widget_map(root)
        ws[1701].remove(ws[1800])
        ws[1046].append(ws[1800])
    checks.append(("command moved into owner SoloFrame", move_command_into_solo))
    def drop_private_black(root):
        fn = function_map(root)[0]
        fn.remove(next(node for node in fn.findall(q("FixtureVal")) if node.get("ID") == "109"))
    checks.append(("private BLACK omitted", drop_private_black))
    for label, change in checks:
        damaged = copy.deepcopy(candidate)
        change(damaged)
        try:
            validate_roots(source, damaged)
        except RuntimeError:
            continue
        raise RuntimeError(f"negative regression check accepted: {label}")
    return len(checks)


def main(argv):
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V30-PERFORMANCE-RECOVERY.qxw")
    parser.add_argument("--candidate", type=Path, default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V31-RELIABILITY.qxw")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    try:
        require(hashlib.sha256(args.source.read_bytes()).hexdigest() == SOURCE_SHA256, "source is not immutable reviewed V30")
        source, candidate = ET.parse(args.source).getroot(), ET.parse(args.candidate).getroot()
        evidence = validate_roots(source, candidate)
        if args.self_test:
            evidence["negative_regression_checks"] = mutation_checks(source, candidate)
        print("PASS: V31 structural workspace validation")
        for key, value in evidence.items():
            print(f"  {key}: {value}")
        print("  Matched plug-in, exact host, physical output and gig qualification remain separate gates")
        print("  SHA-256:", hashlib.sha256(args.candidate.read_bytes()).hexdigest())
        return 0
    except (OSError, RuntimeError, ET.ParseError, KeyError, StopIteration) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
