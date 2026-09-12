#!/usr/bin/env python3
"""Focused regressions for V34's audited creative corrections.

Checks generated DMX endpoints, visible palette events, and pulse durations;
does not infer physical movement, brightness balance, or aesthetic approval.
"""
import copy
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

import V31RigIntegrity as rig
import V32CreativeProgram as baseline
import V34CreativeRepairs as repair

WORKSPACE = Path(__file__).with_name("IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V32-CREATIVE.qxw")


class CreativeRepairTests(unittest.TestCase):
    def test_sweep_endpoints_and_full_ir_hits(self):
        for fid in (636, 637, 638, 639, 640, 650, 651):
            frames = [repair.raw_frame(fid, t) for t in range(16)]
            self.assertEqual([max(frame[fx][0] for frame in frames) for fx in range(4)],
                             [230] * 4, fid)
            self.assertEqual([max(frame[fx][9] for frame in frames) for fx in (9, 10)],
                             [175, 175], fid)
            self.assertTrue(all(frame[fx][0] == 0 for frame in frames[1::2] for fx in range(4)), fid)
            self.assertTrue(all(frame[fx][9] == 0 for frame in frames[1::2] for fx in (9, 10)), fid)

    def test_palette_colors_are_visible_not_just_declared(self):
        for fid in (582, 583, 584, 585, 638, 655):
            frames = [repair.raw_frame(fid, t) for t in range(16)]
            # IR-4 color channels are independent of their dimmer. Require
            # each declared hue at a substantial lit event, not a dark slot.
            colors = {tuple(frame[fx][1:7]) for frame in frames for fx in range(4)
                      if frame[fx][0] >= 40}
            expected = {baseline.COLORS[name] for name in baseline.SCORES[fid].palette}
            self.assertTrue(expected <= colors, (fid, expected - colors))
        # White/Color/White keeps both colors on the active alternating side.
        frames = [repair.raw_frame(655, t) for t in (0, 4, 8, 12)]
        self.assertEqual([tuple(frames[n][fx][1:7]) for n, fx in enumerate((0, 3, 0, 3))],
                         [baseline.COLORS[name] for name in ("white", "pink", "white", "blue")])

    def test_actual_rhythm_at_existing_step_durations(self):
        white = [repair.raw_frame(628, t)[0][0] > 0 for t in range(16)]
        half = [repair.raw_frame(653, t)[0][0] > 0 for t in range(16)]
        self.assertEqual([i for i, lit in enumerate(white) if lit], [0, 4, 8, 12])
        self.assertEqual([i for i, lit in enumerate(half) if lit], list(range(0, 16, 2)))
        self.assertEqual(baseline.timing(628), (0, 250))
        self.assertEqual(baseline.timing(653), (0, 500))

    def test_continuous_and_mechanical_channels_unchanged(self):
        for fid in repair.REPAIRED_LOOP_IDS:
            for step in range(16):
                old, new = baseline.raw_frame(fid, step), repair.raw_frame(fid, step)
                for fx in (9, 10):
                    self.assertEqual(new[fx][:9], old[fx][:9], (fid, step, fx))
                    self.assertEqual(new[fx][10:], old[fx][10:], (fid, step, fx))
                self.assertTrue(all(0 <= value <= 255 for values in new.values() for value in values))

    def test_scope_timing_console_and_all_unrelated_scenes_preserved(self):
        before = ET.parse(WORKSPACE).getroot()
        after = copy.deepcopy(before)
        stats = repair.apply_repairs(after)
        old = rig.id_map(before, "Function")
        new = rig.id_map(after, "Function")
        leaves = {int(ref.text) for fid in repair.REPAIRED_LOOP_IDS for ref in old[fid].findall(rig.q("Step"))}
        self.assertEqual(set(old), set(new))
        changed = set()
        for fid in old:
            if rig.semantic(old[fid]) != rig.semantic(new[fid]):
                changed.add(fid)
            if fid in leaves:
                a, b = copy.deepcopy(old[fid]), copy.deepcopy(new[fid])
                for node in (a, b):
                    for value in node.findall(rig.q("FixtureVal")):
                        node.remove(value)
                self.assertEqual(rig.semantic(a), rig.semantic(b), fid)
            else:
                self.assertEqual(rig.semantic(old[fid]), rig.semantic(new[fid]), fid)
        self.assertEqual(changed, set(stats["changed_scene_ids"]))
        self.assertEqual(rig.semantic(before.find(rig.q("VirtualConsole"))),
                         rig.semantic(after.find(rig.q("VirtualConsole"))))
        first = rig.semantic(after)
        repair.apply_repairs(after)
        self.assertEqual(first, rig.semantic(after), "repair must be idempotent")

    def test_all_focus_mirrors_get_exact_zero_based_exclusions(self):
        root = ET.parse(WORKSPACE).getroot()
        engine = root.find(rig.q("Engine"))
        fixtures = rig.id_map(root, "Fixture")
        for source, target in ((9, 309), (10, 310), (9, 409), (10, 410)):
            fixture = copy.deepcopy(fixtures[source])
            fixture.find(rig.q("ID")).text = str(target)
            engine.append(fixture)
        repair.apply_repairs(root)
        fixtures = rig.id_map(root, "Fixture")
        for fid in (9, 10, 109, 110, 209, 210, 309, 310, 409, 410):
            nodes = fixtures[fid].findall(rig.q("ExcludeFade"))
            self.assertEqual(len(nodes), 1)
            self.assertFalse(list(nodes[0]))
            actual = set(map(int, nodes[0].text.split(",")))
            self.assertEqual(actual, {4, 5, 6, 7, 8, 10, 13, 14, 15, 16, 17})
            self.assertFalse(actual & {0, 1, 2, 3, 9, 11, 12})


if __name__ == "__main__":
    unittest.main()
