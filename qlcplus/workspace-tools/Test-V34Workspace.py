#!/usr/bin/env python3
"""Independent preservation and runtime-wiring contracts for the V34 candidate.

Reads the immutable V32 workspace and the built V34 artifact. This validator
does not import or call the V34 builder. Structural results do not establish
pinned-host, physical-output, visual-quality, or gig qualification.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import V31RigIntegrity as rig
import V34CreativeRepairs as creative

HERE = Path(__file__).resolve().parent
SOURCE = HERE / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw"
CANDIDATE = HERE / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw"
SOURCE_SHA256 = "16c676c531aa9aefa354eb2052e0f643be3c27d9e5fa71953deebf99257478c0"
q, require, semantic = rig.q, rig.require, rig.semantic
RAW = frozenset(range(532, 660))
PRIORITY = frozenset(range(5, 37))
COLORS = frozenset(range(37, 46)) | frozenset(range(2185, 2194))
NEW_FUNCTIONS = frozenset(range(4100, 4104))
NEW_FIXTURES = {212} | set(range(300, 312)) | set(range(400, 412))
NEW_MONITORS = frozenset(range(2100, 2228))
OLD_MONITORS = frozenset(range(1414, 1542))
INERT_STATUS_LABELS = OLD_MONITORS | frozenset(range(1970, 1988))
FOCUS = {9, 10, 109, 110, 209, 210, 309, 310, 409, 410}
EXCLUDED = {4, 5, 6, 7, 8, 10, 13, 14, 15, 16, 17}


def widgets(root):
    console = root.find(q("VirtualConsole"))
    require(console is not None, "V34: missing Virtual Console")
    found = [node for node in console.iter()
             if node.get("ID") is not None and node.find(q("WindowState")) is not None]
    result = {int(node.get("ID")): node for node in found}
    require(len(result) == len(found), "V34: duplicate widget IDs")
    return result


def without_values(node):
    node = copy.deepcopy(node)
    for value in node.findall(q("FixtureVal")):
        node.remove(value)
    return semantic(node)


def frame(function, fixtures):
    return rig.parse_values(function, fixtures)


def check_references(root, functions):
    visiting, visited = set(), set()

    def visit(fid):
        require(fid in functions, f"V34: missing Function reference {fid}")
        require(fid not in visiting, f"V34: recursive Function reference {fid}")
        if fid in visited:
            return
        visiting.add(fid)
        for step in functions[fid].findall(q("Step")):
            visit(int(step.text))
        visiting.remove(fid)
        visited.add(fid)

    for fid in functions:
        visit(fid)
    console = root.find(q("VirtualConsole"))
    for ref in console.iter(q("Function")):
        fid = int(ref.get("ID") if ref.get("ID") is not None else ref.text)
        require(fid == 4294967295 or fid in functions, f"V34: missing VC Function {fid}")


def validate_fixtures(before, after):
    old, new = rig.id_map(before, "Fixture"), rig.id_map(after, "Fixture")
    require(set(new) == set(old) | NEW_FIXTURES, "V34: missing/unexpected fixture IDs")
    for fid, old_fixture in old.items():
        candidate = copy.deepcopy(new[fid])
        if fid in FOCUS:
            for node in candidate.findall(q("ExcludeFade")):
                candidate.remove(node)
        require(semantic(candidate) == semantic(old_fixture), f"V34: original fixture/patch changed {fid}")
    occupied = {}
    for fid, fixture in new.items():
        universe = int(fixture.findtext(q("Universe")))
        address = int(fixture.findtext(q("Address")))
        count = int(fixture.findtext(q("Channels")))
        require(0 <= address and 0 < count and address + count <= 512,
                f"V34: invalid patch span {fid}")
        used = occupied.setdefault(universe, set())
        channels = set(range(address, address + count))
        require(not used & channels, f"V34: fixture overlap {fid}")
        used.update(channels)
    for offset, universe in ((300, 4), (400, 5)):
        for physical in range(11):
            mirror = new[offset + physical]
            require(mirror.findtext(q("Universe")) == str(universe), "V34: color mirror universe")
            for tag in ("Manufacturer", "Model", "Mode", "Address", "Channels"):
                require(mirror.findtext(q(tag)) == old[physical].findtext(q(tag)),
                        f"V34: color mirror {offset + physical} changed {tag}")
    for fid, universe, channels in ((212, 3, 3), (311, 4, 1), (411, 5, 1)):
        marker = new[fid]
        require(tuple(marker.findtext(q(tag)) for tag in
                      ("Manufacturer", "Model", "Universe", "Address", "Channels")) ==
                ("Generic", "Generic", str(universe), "334", str(channels)),
                f"V34: marker {fid} is not the required native HTP span")
        require(marker.findtext(q("Mode")) == f"{channels} Channel", f"V34: marker mode mismatch {fid}")
    identified = {fid for fid, fixture in new.items()
                  if fixture.findtext(q("Manufacturer")) == "American DJ"
                  and fixture.findtext(q("Model")) == "Focus Spot Two"}
    require(identified == FOCUS, "V34: Focus mirror identity mismatch")
    for fid in FOCUS:
        nodes = new[fid].findall(q("ExcludeFade"))
        require(len(nodes) == 1 and not list(nodes[0]), f"V34: bad ExcludeFade schema {fid}")
        tokens = nodes[0].text.split(",")
        values = [int(token) for token in tokens]
        require(len(values) == len(EXCLUDED) and set(values) == EXCLUDED,
                f"V34: discrete Focus channels can fade or continuous channels snap {fid}")
    return old, new


def validate_io(before, after):
    old_io = before.find(q("Engine")).find(q("InputOutputMap"))
    new_io = after.find(q("Engine")).find(q("InputOutputMap"))
    old = {int(n.get("ID")): n for n in old_io.findall(q("Universe"))}
    nodes = new_io.findall(q("Universe"))
    new = {int(n.get("ID")): n for n in nodes}
    require(len(new) == len(nodes) and set(new) == set(old) | {4, 5}, "V34: invalid universe IDs")
    original_physical = copy.deepcopy(old[0])
    for output in original_physical.findall(q("Output")):
        original_physical.remove(output)
    require(not new[0].findall(q("Output")), "V34: unresolved physical output must be selected on target PC")
    require(semantic(new[0]) == semantic(original_physical), "V34: physical input/metadata changed")
    require(semantic(new[2]) == semantic(old[2]), "V34: Priority routing changed")
    require(not new[1].findall(q("Output")), "V34: control universe must not emit physical DMX")
    for tag in ("Input", "Feedback"):
        require([semantic(n) for n in new[1].findall(q(tag))] ==
                [semantic(n) for n in old[1].findall(q(tag))], "V34: MIDI input/feedback route changed")
    for universe, line, uid in ((3, 5, "soundswitch:effect-layer"),
                                (4, 6, "soundswitch:color-latch-layer"),
                                (5, 7, "soundswitch:color-hold-layer")):
        require(len(new[universe]) == 1, f"V34: private U{universe + 1} has extra routes")
        output = new[universe][0]
        require(output.tag == q("Output") and output.get("Plugin") == "SoundSwitch Hardware"
                and output.get("Line") == str(line) and output.get("UID") == uid,
                f"V34: private U{universe + 1} leaks or uses wrong binding")
        params = output.find(q("PluginParameters"))
        count = 512 if params is None else int(params.get("UniverseChannels", "512"))
        require(count >= (337 if universe == 3 else 335), f"V34: truncated private U{universe + 1} marker")
    # Universe metadata may change; tempo/server metadata may not.
    require([semantic(n) for n in old_io if n.tag != q("Universe")] ==
            [semantic(n) for n in new_io if n.tag != q("Universe")], "V34: native beat/server configuration changed")


def validate_console(before, after, functions):
    old, new = widgets(before), widgets(after)
    require(set(new) == set(old) | {2000, 2001} | NEW_MONITORS, "V34: widget identities changed")
    console_root = after.find(q("VirtualConsole"))
    parents = {child: parent for parent in console_root.iter() for child in parent}
    stop = new[1001]
    require(stop.tag == q("Button") and stop.findtext(q("Action")) == "Toggle"
            and stop.find(q("Function")).get("ID") == "4103", "V34: STOP native command pulse missing")
    require([node.attrib for node in stop.findall(q("Input"))] ==
            [{"Universe": "1", "Channel": "819", "UpperValue": "0", "LowerValue": "0", "MonitorValue": "0"},
             {"Universe": "0", "Channel": "6940"}],
            "V34: STOP must request native start without premature StopAll feedback")
    require(stop.find(q("Feedback")) is None, "V34: VCButton has no separate Feedback XML element")
    require(stop.find(q("WindowState")).get("Visible") == "True", "V34: STOP feedback disabled by hidden widget")
    hidden = new[2000]
    require(hidden.tag == q("Button") and hidden.findtext(q("Action")) == "StopAll"
            and hidden.find(q("Function")).get("ID") == "4294967295", "V34: queued StopAll action missing")
    require([node.attrib for node in hidden.findall(q("Input"))] == [{"Universe": "1", "Channel": "818"}],
            "V34: queued StopAll input mismatch")
    barrier = new[2001]
    require(barrier.tag == q("Button") and barrier.findtext(q("Action")) == "Toggle"
            and barrier.find(q("Function")).get("ID") == "4103", "V34: STOP native-running observer missing")
    require([node.attrib for node in barrier.findall(q("Input"))] ==
            [{"Universe": "1", "Channel": "817", "UpperValue": "0", "LowerValue": "0", "MonitorValue": "255"}],
            "V34: STOP must acknowledge only actual native Function running")
    for wid in (2000, 2001):
        rect = new[wid].find(q("WindowState"))
        require(parents[new[wid]] is new[0], f"V34: STOP helper must stay on Live {wid}")
        require(rect.get("Visible") == "True" and rect.get("X") == "-10000" and rect.get("Y") == "-10000"
                and rect.get("Width") == "1" and rect.get("Height") == "1", f"V34: STOP helper must be enabled offscreen {wid}")
    black = new[1015]
    require(black.findtext(q("Action")) == "Blackout", "V34: native blackout replaced")
    require(black.find(q("WindowState")).attrib ==
            {"Visible": "True", "X": "1420", "Y": "10", "Width": "160", "Height": "50"},
            "V34: native blackout not exposed at header")
    require(new[1000].find(q("WindowState")).get("Width") == "1380", "V34: header overlaps blackout")
    require([semantic(n) for n in black.findall(q("Input"))] ==
            [semantic(n) for n in old[1015].findall(q("Input"))], "V34: native blackout input changed")

    reordered_buttons = 0
    for wid, widget in old.items():
        if widget.tag != q("Button") or wid == 1001:
            continue
        inputs = widget.findall(q("Input"))
        expected = [] if wid in OLD_MONITORS else sorted(inputs, key=lambda node: node.get("Universe") != "1")
        require([semantic(node) for node in new[wid].findall(q("Input"))] ==
                [semantic(node) for node in expected], f"V34: input bindings or feedback-first order changed {wid}")
        reordered_buttons += inputs != expected

    for wid in INERT_STATUS_LABELS:
        label = new[wid]
        require(label.tag == q("Label") and label.get("Caption") == "",
                f"V34: visual status strip remains interactive {wid}")
        require(not any(label.find(q(tag)) is not None for tag in ("Function", "Action", "Input", "Intensity")),
                f"V34: inert status label retains a control binding {wid}")
        expected = copy.deepcopy(old[wid])
        expected.tag = q("Label")
        expected.set("Caption", "")
        for node in list(expected):
            if node.tag in {q("Function"), q("Action"), q("Input"), q("Intensity")}:
                expected.remove(node)
        require(semantic(label) == semantic(expected), f"V34: status-strip geometry/appearance changed {wid}")

    for index in range(128):
        monitor = new[2100 + index]
        original = old[1414 + index]
        require(parents[monitor] is new[0], f"V34: raw monitor must be outside disabled visual strips {index}")
        ancestor = parents[monitor]
        while ancestor is not console_root:
            require(ancestor.findtext(q("Disabled"), "False") == "False",
                    f"V34: raw feedback has disabled ancestor {index}")
            ancestor = parents[ancestor]
        require([semantic(n) for n in monitor.findall(q("Input"))] ==
                [semantic(n) for n in original.findall(q("Input"))], f"V34: raw monitor feedback missing {index}")
        require(monitor.find(q("Function")).get("ID") == str(532 + index), f"V34: raw monitor watches wrong Function {index}")
        rect = monitor.find(q("WindowState"))
        require(rect.get("Visible") == "True" and rect.get("X") == "-10000"
                and rect.get("Y") == "-10000" and rect.get("Width") == "1" and rect.get("Height") == "1",
                f"V34: raw monitor visibility/geometry invalid {index}")
        candidate, expected = copy.deepcopy(monitor), copy.deepcopy(original)
        for node in (candidate, expected):
            node.attrib.pop("ID", None)
            node.attrib.pop("Caption", None)
            node.remove(node.find(q("WindowState")))
            for inp in node.findall(q("Input")):
                node.remove(inp)
        require(semantic(candidate) == semantic(expected), f"V34: raw monitor has unintended behavior {index}")

    # Flash performance buttons must have distinct Function identity, so their
    # release cannot stop a simultaneously latched instance of the same look.
    performance_holds = {1018: 1, 1019: 0, 1020: 2}
    for wid, fid in performance_holds.items():
        widget = old[wid]
        updated = copy.deepcopy(new[wid])
        require(updated.findtext(q("Action")) == "Flash" and
                updated.find(q("Function")).get("ID") == str(4100 + fid),
                f"V34: hold still shares its latched Function {wid}")
        updated.find(q("Function")).set("ID", str(fid))
        original = copy.deepcopy(widget)
        for node in (updated, original):
            for value in node.findall(q("Input")):
                node.remove(value)
        require(semantic(updated) == semantic(original), f"V34: unrelated hold-widget change {wid}")
    console = copy.deepcopy(after.find(q("VirtualConsole")))
    parents = {child: parent for parent in console.iter() for child in parent}
    permitted = {1000, 1001, 1015} | set(performance_holds) | INERT_STATUS_LABELS
    for node in list(console.iter()):
        if node.get("ID") is None or node.find(q("WindowState")) is None:
            continue
        wid = int(node.get("ID"))
        if wid in {2000, 2001} or wid in NEW_MONITORS:
            parents[node].remove(node)
        elif wid in permitted:
            parent = parents[node]
            index = list(parent).index(node)
            parent.remove(node)
            parent.insert(index, copy.deepcopy(old[wid]))
    original_console = copy.deepcopy(before.find(q("VirtualConsole")))
    # Input identity/order was checked above. Moving those nodes to the end
    # of each Button is XML-legal and is not a geometry/control change.
    for section in (console, original_console):
        for button in section.iter(q("Button")):
            for value in button.findall(q("Input")):
                button.remove(value)
    require(semantic(console) == semantic(original_console),
            "V34: unrelated Virtual Console/control/speed-table change")
    return len(new), len(performance_holds), reordered_buttons - len(OLD_MONITORS)


def validate(before, after):
    require(before.tag == after.tag == q("Workspace"), "V34: workspace namespace/root mismatch")
    old, new = rig.id_map(before, "Function"), rig.id_map(after, "Function")
    require(set(new) == set(old) | NEW_FUNCTIONS, "V34: missing/unexpected Function IDs")
    old_fixtures, new_fixtures = validate_fixtures(before, after)
    validate_io(before, after)
    require([semantic(node) for node in before if node.tag not in {q("Engine"), q("VirtualConsole")}] ==
            [semantic(node) for node in after if node.tag not in {q("Engine"), q("VirtualConsole")}],
            "V34: unrelated workspace metadata changed")
    omitted = {q("Fixture"), q("Function"), q("InputOutputMap")}
    require([semantic(node) for node in before.find(q("Engine")) if node.tag not in omitted] ==
            [semantic(node) for node in after.find(q("Engine")) if node.tag not in omitted],
            "V34: Engine groups/monitor/bus metadata changed")
    check_references(after, new)
    values = {fid: frame(fn, new_fixtures) for fid, fn in new.items() if fn.get("Type") == "Scene"}
    changed_leaves = rig.scene_leaves(old, creative.REPAIRED_LOOP_IDS)
    changed_functions = COLORS | {0, 1, 2} | changed_leaves
    for fid, function in old.items():
        require(new[fid].get("Type") == function.get("Type"), f"V34: Function type changed {fid}")
        if fid in changed_functions:
            require(without_values(function) == without_values(new[fid]), f"V34: non-payload Scene change {fid}")
        else:
            require(semantic(function) == semantic(new[fid]), f"V34: unrelated Function/owner/timing changed {fid}")
    for fid in RAW:
        refs = new[fid].findall(q("Step"))
        require(len(refs) == 16, f"V34: missing loop steps {fid}")
        for t, ref in enumerate(refs):
            payload = values[int(ref.text)]
            require(set(payload) == set(range(11)), f"V34: incomplete full-rig raw loop {fid}")
            expected = creative.raw_frame(fid, t)
            require(payload == {fx: dict(enumerate(data)) for fx, data in expected.items()},
                    f"V34: raw creative frame drift {fid}/{t}")
    require(len(rig.scene_leaves(new, RAW)) == 2048, "V34: wrong raw creative closure")
    require(len(rig.scene_leaves(new, PRIORITY)) == 102, "V34: wrong Priority closure")
    for fid in COLORS:
        original = frame(old[fid], old_fixtures)
        offset, marker = (300, 311) if fid < 100 else (400, 411)
        expected = {offset + fx: data for fx, data in original.items() if fx < 100}
        expected[marker] = {0: 255}
        require(values[fid] == expected, f"V34: incomplete/color-changing override mirror {fid}")
    require(values[0] == values[4100] == {212: {2: 255}}, "V34: BLACK must be an intensity-only native gate")
    for fid in (1, 2):
        expected = frame(old[fid], old_fixtures)
        expected[212] = {fid - 1: 255}
        require(values[fid] == values[4100 + fid] == expected, f"V34: WHITE/UV hold or marker mismatch {fid}")
    # STOP is a transient command, not a maintained lighting owner. The native
    # running observer above acknowledges preRun before StopAll is issued;
    # an immediate button edge can outrun pending MasterTimer start requests.
    # The empty Scene self-terminates and never contributes lighting values.
    require(values[4103] == {}, "V34: STOP command must be an empty self-terminating Scene")
    for fid in NEW_FUNCTIONS:
        require(new[fid].get("Type") == "Scene" and new[fid].find(q("Speed")).attrib ==
                {"FadeIn": "0", "FadeOut": "0", "Duration": "0"}, f"V34: new latch/hold Scene speed {fid}")
    for fid, payload in values.items():
        for fx in FOCUS & payload.keys():
            require(all(payload[fx].get(ch, 0) == 0 for ch in (10, 11, 13, 14, 15, 17)),
                    f"V34: Focus UV/program/reset enabled {fid}/{fx}")
    widget_count, hold_count, reordered_buttons = validate_console(before, after, new)
    return {"functions": len(new), "widgets": widget_count, "fixtures": len(new_fixtures),
            "raw_loops": len(RAW), "raw_scene_leaves": 2048, "priority_looks": 32,
            "priority_scene_leaves": 102, "unchanged_collections": sum(fn.get("Type") == "Collection" for fn in old.values()),
            "independent_performance_hold_widgets": hold_count, "color_templates": len(COLORS),
            "buttons_with_surface_feedback_moved_first": reordered_buttons,
            "enabled_raw_feedback_monitors": len(NEW_MONITORS),
            "inert_visual_status_labels": len(INERT_STATUS_LABELS),
            "focus_fade_exclusions_checked": len(FOCUS), "physical_output_tested": False}


def corruption_checks(before, after):
    def missing_reference(root):
        rig.id_map(root, "Function")[532].find(q("Step")).text = "999999"
    def duplicate_function(root):
        root.find(q("Engine")).append(copy.deepcopy(rig.id_map(root, "Function")[532]))
    def overlap(root):
        rig.id_map(root, "Fixture")[301].find(q("Address")).text = "0"
    def fade(root):
        rig.id_map(root, "Fixture")[409].find(q("ExcludeFade")).text = "4,5,6"
    def leak(root):
        universe = next(n for n in root.find(q("Engine")).find(q("InputOutputMap")).findall(q("Universe")) if n.get("ID") == "4")
        universe.find(q("Output")).set("Plugin", "ArtNet")
    def missing_color(root):
        fn = rig.id_map(root, "Function")[37]
        fn.remove(next(n for n in fn.findall(q("FixtureVal")) if n.get("ID") == "308"))
    def shared_hold(root):
        for node in widgets(root).values():
            if node.findtext(q("Action")) == "Flash" and node.find(q("Function")) is not None and node.find(q("Function")).get("ID") == "4101":
                node.find(q("Function")).set("ID", "1")
                return
        raise RuntimeError("corruption setup: no WHITE hold")
    def immediate_stop_feedback(root):
        node = widgets(root)[1001].find(q("Input"))
        node.attrib.clear()
        node.attrib.update(Universe="1", Channel="817")
    def persistent_stop_payload(root):
        fn = rig.id_map(root, "Function")[4103]
        ET.SubElement(fn, q("FixtureVal"), {"ID": "212"}).text = "0,0"
    def missing_stop_barrier(root):
        ws = widgets(root)
        ws[0].remove(ws[2001])
    def stop_feedback_order(root):
        stop = widgets(root)[1001]
        inputs = stop.findall(q("Input"))
        for node in inputs:
            stop.remove(node)
        for node in reversed(inputs):
            stop.append(node)
    def duplicate_widget(root):
        console = root.find(q("VirtualConsole"))
        console.append(copy.deepcopy(widgets(root)[1001]))
    def speed(root):
        rig.id_map(root, "Function")[532].find(q("SpeedModes")).set("Duration", "PerStep")
    def black_gate(root):
        rig.id_map(root, "Function")[0].find(q("FixtureVal")).text = "2,0"
    def disabled_monitor_ancestor(root):
        ws = widgets(root)
        ws[0].remove(ws[2100])
        ws[1542].append(ws[2100])
    def missing_monitor_feedback(root):
        monitor = widgets(root)[2100]
        monitor.remove(monitor.find(q("Input")))
    def interactive_status_strip(root):
        label = widgets(root)[1414]
        label.tag = q("Button")
        ET.SubElement(label, q("Function"), {"ID": "532"})
        ET.SubElement(label, q("Action")).text = "Toggle"
    mutations = (missing_reference, duplicate_function, overlap, fade, leak, missing_color,
                 shared_hold, immediate_stop_feedback, stop_feedback_order, duplicate_widget, speed, black_gate,
                 disabled_monitor_ancestor, missing_monitor_feedback, interactive_status_strip,
                 persistent_stop_payload, missing_stop_barrier)
    for mutate in mutations:
        corrupt = copy.deepcopy(after)
        mutate(corrupt)
        try:
            validate(before, corrupt)
        except (RuntimeError, KeyError, ValueError):
            continue
        raise RuntimeError("V34: corruption was not rejected: " + mutate.__name__)
    return len(mutations)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=SOURCE)
    parser.add_argument("--candidate", type=Path, default=CANDIDATE)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    require(hashlib.sha256(args.source.read_bytes()).hexdigest() == SOURCE_SHA256, "V34: protected V32 source changed")
    before, after = ET.parse(args.source).getroot(), ET.parse(args.candidate).getroot()
    result = validate(before, after)
    if args.self_test:
        result["corruption_cases_rejected"] = corruption_checks(before, after)
    result["workspace_sha256"] = hashlib.sha256(args.candidate.read_bytes()).hexdigest()
    text = json.dumps(result, indent=2) + "\n"
    if args.json:
        args.json.write_text(text)
    print(text, end="")


if __name__ == "__main__":
    main()
