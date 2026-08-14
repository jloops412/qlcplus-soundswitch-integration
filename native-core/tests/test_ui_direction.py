#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
DIRECTION = ROOT / "spec/ui/ui-course-correction-v1.json"
LAYOUT = ROOT / "spec/ui/examples/default-studio-fixtures-looks-standard.layout.json"
CATALOG = ROOT / "spec/ui/registry/generated/ui-registry.catalog.json"


def load(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def walk(node):
    yield node
    for child in node.get("children", []):
        yield from walk(child)
    for children in node.get("slots", {}).values():
        for child in children:
            yield from walk(child)


class UiDirectionTests(unittest.TestCase):
    def test_legacy_win32_shell_is_frozen_at_or_below_baseline(self):
        direction = load(DIRECTION)
        legacy = direction["legacyShell"]
        self.assertEqual(direction["decisionId"], "D-091")
        self.assertEqual(legacy["classification"], "frozenBridge")
        source = (ROOT / legacy["source"]).read_text(encoding="utf-8")
        self.assertLessEqual(
            source.count("CreateWindowExW"),
            legacy["maximumCreateWindowExWCallSites"],
        )
        self.assertLessEqual(
            source.count("WS_EX_TOOLWINDOW"),
            legacy["maximumToolWindowCallSites"],
        )
        self.assertLessEqual(
            source.count("Browse Controls"),
            legacy["maximumBrowseControlsEntrypoints"],
        )
        self.assertIn("newTopLevelEditor", legacy["forbiddenChangeClasses"])
        self.assertIn("newPropertyForm", legacy["forbiddenChangeClasses"])

    def test_first_slice_uses_registered_task_facing_components(self):
        layout = load(LAYOUT)
        catalog = load(CATALOG)
        components = {item["id"]: item for item in catalog["components"]}
        nodes = list(walk(layout["root"]))
        ids = [node["id"] for node in nodes]
        self.assertEqual(len(ids), len(set(ids)))
        native_nodes = [node for node in nodes if node["type"] == "NativeComponent"]
        self.assertTrue(native_nodes)
        for node in native_nodes:
            self.assertIn(node["componentType"], components)
            contract = components[node["componentType"]]
            self.assertIn(layout["workspace"], contract["supportedWorkspaces"])
            self.assertIn(node["variant"], contract["supportedVariants"])
            self.assertTrue(
                set(node.get("properties", {})).issubset(contract["properties"])
            )
            for name, value in node.get("properties", {}).items():
                definition = contract["properties"][name]
                if definition["type"] == "boolean":
                    self.assertIs(type(value), bool)
                elif definition["type"] == "integer":
                    self.assertIs(type(value), int)
                elif definition["type"] == "number":
                    self.assertIn(type(value), (int, float))
                elif definition["type"] == "string":
                    self.assertIs(type(value), str)
                elif definition["type"] == "enum":
                    self.assertIn(value, definition["enumValues"])

        by_component = {node["componentType"]: node for node in native_nodes}
        primary = by_component["ember.fixtureControlSurface"]
        self.assertFalse(primary["properties"]["includeAdvanced"])
        self.assertEqual(primary["properties"]["query"], "")
        self.assertEqual(primary["properties"]["selectedControlId"], "")
        self.assertFalse(primary["properties"]["showDiagnostics"])
        self.assertEqual(primary["properties"]["surface"], "staticLook")
        self.assertFalse(by_component["ember.fixtureProfileEditor"]["properties"]["showDiagnostics"])
        diagnostics = by_component["ember.fixtureFunctionBrowser"]
        self.assertTrue(diagnostics["properties"]["showDiagnostics"])
        self.assertTrue(any(node["type"] == "Drawer" for node in nodes))

        primary_root = next(
            node for node in nodes
            if node["id"] == "default.studio.fixturesLooks.lookEditor"
        )
        self.assertFalse(any(node["type"] == "Table" for node in walk(primary_root)))
        labels = {
            node.get("labelKey") for node in nodes if node["type"] == "Label"
        }
        self.assertTrue({
            "staticLook.ownership.release",
            "staticLook.ownership.set",
            "staticLook.ownership.forceZero",
        }.issubset(labels))

    def test_first_slice_and_installer_policy_are_explicit(self):
        direction = load(DIRECTION)
        first_slice = direction["firstProductSlice"]
        self.assertEqual(first_slice["id"], "studio.fixtures-static-looks")
        self.assertEqual(
            first_slice["layoutFixture"],
            "spec/ui/examples/default-studio-fixtures-looks-standard.layout.json",
        )
        self.assertEqual(first_slice["requiredViewports"], ["1366x768", "1920x1080"])
        self.assertIn("rawDmxAsOrdinaryControl", first_slice["primarySurfaceProhibitions"])
        self.assertFalse(direction["installerPolicy"]["modelOnlyChangesAdvancePreview"])
        self.assertFalse(direction["replacementShellGate"]["toolkitSelected"])

    def test_canonical_docs_publish_the_course_correction(self):
        marker = "UI course correction (2026-08-14)"
        for relative in (
            "docs/00_START_HERE.md",
            "docs/06_PRIORITIZED_BACKLOG.md",
            "docs/UI_PROGRAM_START_HERE.md",
            "docs/41_DEFAULT_2_2_AUTHORING_WORKBENCH_CHECKPOINT.md",
            "docs/42_UNIFIED_FIXTURE_ATTRIBUTE_CHECKPOINT.md",
            "docs/43_FIXTURE_CONTROL_INSPECTOR_CHECKPOINT.md",
            "docs/44_UI_COURSE_CORRECTION_AND_REPLACEMENT_SHELL_GATE.md",
        ):
            content = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn(marker, content, relative)


if __name__ == "__main__":
    unittest.main()
