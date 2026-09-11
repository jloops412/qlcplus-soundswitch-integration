#!/usr/bin/env python3
"""Build V31 from immutable V30 without rewriting the creative library.

Mouse commands deliberately live outside the playback SoloFrame. Their empty
Scenes feed the same narrow SoundSwitch translation used by the Control One;
the original native owner and Flash buttons still execute every Function.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

import V31RigIntegrity as rig

NS_URI = "http://www.qlcplus.org/Workspace"
NS = "{" + NS_URI + "}"
ET.register_namespace("", NS_URI)
SOURCE_SHA256 = "530e6d4166bc150442660bfbbff790bd1c1e4803f0c6d02c99302ebb30d79331"


def q(name):
    return NS + name


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def widgets(root):
    result = {}
    for node in root.iter():
        if node.get("ID") is not None and node.find(q("WindowState")) is not None:
            wid = int(node.get("ID"))
            require(wid not in result, f"duplicate widget ID {wid}")
            result[wid] = node
    return result


def hide(widget):
    window = widget.find(q("WindowState"))
    window.attrib.update(Visible="True", X="-10000", Y="-10000",
                         Width="1", Height="1")


def frame(wid, caption, x, y, width, height, page=None, disabled=False):
    node = ET.Element(q("Frame"), {"ID": str(wid), "Caption": caption})
    if page is not None:
        node.set("Page", str(page))
    ET.SubElement(node, q("WindowState"), {
        "Visible": "True", "X": str(x), "Y": str(y), "Width": str(width),
        "Height": str(height), "Z": "20"})
    for tag, value in (("AllowResize", "False"), ("ShowHeader", "False"),
                       ("ShowEnableButton", "False"), ("Collapsed", "False"),
                       ("Disabled", "True" if disabled else "False")):
        ET.SubElement(node, q(tag)).text = value
    return node


def command_scene(engine, fid, name):
    node = ET.Element(q("Function"), {
        "ID": str(fid), "Type": "Scene", "Name": "UI V31 — " + name})
    ET.SubElement(node, q("Speed"), {"FadeIn": "0", "FadeOut": "0", "Duration": "0"})
    # A zero-duration empty Scene produces one positive feedback edge and its
    # trailing zero. It must not own any lighting channel or child Function.
    monitor = engine.find(q("Monitor"))
    engine.insert(list(engine).index(monitor), node)


def command_button(template, wid, fid, channel):
    node = copy.deepcopy(template)
    node.set("ID", str(wid))
    node.attrib.pop("Page", None)
    node.find(q("Function")).set("ID", str(fid))
    action = node.find(q("Action"))
    action.attrib.clear()
    action.text = "Toggle"
    for child in node.findall(q("Input")):
        node.remove(child)
    ET.SubElement(node, q("Input"), {"Universe": "1", "Channel": str(channel)})
    return node


def indicator(template, frame_id, button_id, fid):
    window = template.find(q("WindowState"))
    panel = frame(frame_id, "NATIVE FUNCTION STATE", int(window.get("X")),
                  int(window.get("Y")) + int(window.get("Height")) + 2,
                  int(window.get("Width")), 6, disabled=True)
    panel.find(q("WindowState")).set("Z", "50")
    button = copy.deepcopy(template)
    button.set("ID", str(button_id))
    button.set("Caption", "")
    button.attrib.pop("Page", None)
    button.find(q("Function")).set("ID", str(fid))
    button.find(q("Action")).attrib.clear()
    button.find(q("Action")).text = "Toggle"
    for node in button.findall(q("Input")):
        button.remove(node)
    button.find(q("WindowState")).attrib.update(
        X="0", Y="0", Width=window.get("Width"), Height="6", Visible="True")
    panel.append(button)
    return panel


def repair_console(root):
    ws = widgets(root)
    engine = root.find(q("Engine"))
    live = ws[0]

    # The overlay covers only the pad lanes. Bank/order/scope buttons and the
    # four-bank raw-Chaser monitors retain their original geometry and targets.
    pad_frame = frame(1700, "MOUSE PADS • SAME PLAYBACK COMMAND AS CONTROL ONE",
                      16, 162, 1128, 338, page=0)
    ET.SubElement(pad_frame, q("Multipage"), {
        "PagesNum": "4", "CurrentPage": "0", "PagesLoop": "True"})
    for bank in range(4):
        shortcut = ET.SubElement(pad_frame, q("Shortcut"), {
            "Page": str(bank), "Name": f"BANK {bank + 1}"})
        ET.SubElement(shortcut, q("Input"), {
            "Universe": "1", "Channel": str(32 + bank)})
        lane = frame(1701 + bank, f"MOUSE PAD BANK {bank + 1}",
                     0, 0, 1128, 338, page=bank)
        for pad in range(32):
            index = bank * 32 + pad
            original = ws[1058 + bank * 43 + pad]
            require(original.find(q("Function")).get("ID") == str(660 + index),
                    f"unexpected manual owner at bank {bank + 1}, pad {pad + 1}")
            fid = 2200 + index
            command_scene(engine, fid, f"BANK {bank + 1} PAD {pad + 1:02}")
            lane.append(command_button(original, 1800 + index, fid, 900 + index))
            hide(original)
        pad_frame.append(lane)
    ws[1045].append(pad_frame)

    # Native raw-Chaser state already drives the disabled visual strips. Add
    # one-way synthetic feedback routes so Control One LEDs follow the same
    # QLC+ authority during autoplay, Priority mode and reconnect. The matched
    # plug-in never emits these channels as input, so they cannot launch pads.
    for index in range(128):
        monitor = ws[1414 + index]
        require(monitor.find(q("Function")).get("ID") == str(532 + index) and
                not monitor.findall(q("Input")), "raw Function monitor changed")
        ET.SubElement(monitor, q("Input"), {
            "Universe": "1", "Channel": str(1100 + index), "LowerValue": "0",
            "UpperValue": "255", "MonitorValue": "255"})

    # The original color Flash controls remain the only channel owners. The
    # mouse click now uses the hardware latch translator and the thin strip
    # observes native Flash state instead of claiming command-Scene activity.
    for index in range(9):
        original = ws[1253 + index]
        fid = 2330 + index
        command_scene(engine, fid, f"COLOR LATCH {index + 1}")
        live.append(command_button(original, 1930 + index, fid, 850 + index))
        live.append(indicator(original, 1950 + index, 1970 + index, 37 + index))
        hide(original)

    # A Toggle Scene's normal LTP layer loses its aim when a running loop
    # writes the same channels. Native ForceLTP Flash provides sustained aim;
    # the plug-in only holds/releases its existing logical input, as for color.
    # QLC+ forwards input solely to the selected top-level console page, so the
    # same native Scene controls are reachable on both Live and Position Bench.
    for index in range(9):
        original = ws[1578 + index]
        fid = 2340 + index
        command_scene(engine, fid, f"FOCUS POSITION LATCH {index + 1}")
        ws[1574].append(command_button(original, 1940 + index, fid, 860 + index))
        # Original position geometry was relative to the old SoloFrame.
        proxy = ws[1574][-1]
        proxy_window = proxy.find(q("WindowState"))
        proxy_window.set("X", str(int(proxy_window.get("X")) + 20))
        proxy_window.set("Y", str(int(proxy_window.get("Y")) + 160))
        ws[1574].append(indicator(proxy, 1959 + index, 1979 + index, 2175 + index))
        action = original.find(q("Action"))
        action.text = "Flash"
        action.attrib.update(Override="1", ForceLTP="1")
        hide(original)
        live_owner = copy.deepcopy(original)
        live_owner.set("ID", str(1990 + index))
        live.append(live_owner)

    ws[1000].set("Caption", "CONTROL ONE • V31 RELIABILITY CANDIDATE\n"
                "SHARED MOUSE + HARDWARE COMMANDS • FULL-RIG OVERRIDES")
    ws[1252].set("Caption", "COLORS • CLICK / PAD = LATCH • SHIFT + COLOR PAD = HOLD")
    ws[1343].set("Caption", "COLOR CLICK / PAD → latch • SHIFT + COLOR PAD → hold\n"
                "SHIFT + PERFORMANCE PADS 1–9 → latched Focus A/B aim\n"
                "SHIFT + WHITE / BLACK / UV → latched effects\n"
                "Use LIVE for show transport; native strips show active Functions")
    ws[1576].set("Caption", "CLICK OR SHIFT + PERFORMANCE PADS 1–9 → LATCH AIM; REPEAT TO RELEASE\n"
                "BENCH A/B IDENTITY AND AIM • REAL FOCUS UV REMAINS DISABLED")
    ws[1588].set("Caption", ws[1588].get("Caption", "").replace("IN V30", "IN V31"))


def build(source, output, force=False):
    require(source.is_file(), f"source not found: {source}")
    require(hashlib.sha256(source.read_bytes()).hexdigest() == SOURCE_SHA256,
            "V31 requires the immutable reviewed V30 source hash")
    require(output.resolve() != source.resolve(), "cannot overwrite protected V30 source")
    require(force or not output.exists(), f"output exists: {output}; use --force")
    root = ET.parse(source).getroot()
    rig.repair(root)
    repair_console(root)
    widgets(root)
    ET.indent(root, space="  ")
    output.parent.mkdir(parents=True, exist_ok=True)
    data = '<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(root, encoding="unicode") + "\n"
    temporary = output.with_suffix(output.suffix + ".tmp")
    temporary.write_bytes(data.replace("\n", "\r\n").encode("utf-8"))
    temporary.replace(output)
    ET.parse(output)
    print("PASS: V31 reliability workspace generated")
    print("  V30 creative library and native playback owners retained")
    print("  Mouse pads/colors/positions use shared hardware command translation")
    print("  Performance and position payloads cover physical/private layers")
    print("  Requires the matched V31 SoundSwitch plug-in; physical qualification pending")
    print("  SHA-256:", hashlib.sha256(output.read_bytes()).hexdigest())


def main(argv):
    here = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V30-PERFORMANCE-RECOVERY.qxw")
    parser.add_argument("--output", type=Path, default=here / "IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V31-RELIABILITY.qxw")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)
    try:
        build(args.source, args.output, args.force)
        return 0
    except (OSError, RuntimeError, ET.ParseError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
