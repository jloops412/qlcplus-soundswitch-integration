#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
GENERATOR_PATH = ROOT / "native-core/tools/generate_ui_registry.py"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("generate_ui_registry", GENERATOR_PATH)
assert SPEC is not None and SPEC.loader is not None
registry = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(registry)

FIXTURES = ROOT / "native-core/tests/fixtures/ui_registry"

EXPECTED_COMMAND_IDS = [
    "show.start", "show.stop", "show.toggleRunning", "output.blackout.set",
    "output.blackout.toggle", "output.workLight.set", "output.workLight.toggle",
    "override.releaseAll", "transport.manualBpm.set", "transport.tap",
    "safety.hazard.setArmed", "safety.hazard.disarmAll", "staticLook.activate",
    "staticLook.toggle", "staticLook.hold", "staticLook.clear",
    "autoloop.launch", "autoloop.clear", "autoloop.next", "autoloop.previous",
    "autoloop.bankFilter.enableAll", "autoloop.bankFilter.selectExclusive",
    "autoloop.bankFilter.setEnabled", "trackScript.start", "trackScript.clear",
    "fixture.override.property.set", "fixture.override.property.release",
    "group.override.property.set", "group.override.property.release",
    "staticLook.preview.start", "staticLook.preview.stop",
]

EXPECTED_STATE_IDS = [
    "project.active.id", "project.active.name", "runner.state", "runner.generation",
    "runner.frames", "runner.health", "transport.bpm", "transport.beatPosition",
    "transport.clockSource", "transport.syncState", "connection.os2l.status",
    "connection.os2l.discoveryStatus", "controller.input.status",
    "controller.output.status", "output.artnet.status", "output.sacn.status",
    "output.dmxUsbPro[0].status", "output.dmxUsbPro[1].status",
    "output.micro.status", "output.blackout", "output.workLight",
    "safety.hazard.fog.armed", "safety.hazard.haze.armed",
    "safety.hazard.laser.armed", "safety.hazard.spark.armed",
    "staticLook.active.id", "autoloop.active.id", "autoloop.active.bank",
    "autoloop.active.slot", "autoloop.active.progress", "autoloop.active.repeat",
    "autoloop.active.completedCycles", "autoloop.bankFilter.mask",
    "trackScript.active.id", "trackScript.elapsedBeat",
    "trackScript.consumedCueCount", "override.activePropertyCount",
    "output.controlOne.status", "output.controlOne.experimental",
    "staticLook.active.packageGeneration",
    "staticLook.active.activationGeneration", "staticLook.active.ownerKind",
    "staticLook.active.ownerFeedbackKey", "staticLook.active.behavior",
    "staticLook.active.status", "staticLook.active.transitionProgress",
    "staticLook.preview.state", "staticLook.preview.mode",
    "staticLook.preview.error", "staticLook.preview.remainingMs",
    "staticLook.preview.outputCap", "staticLook.preview.selectedFixtureCount",
    "staticLook.preview.updateCount", "staticLook.preview.frameDigest",
]

EXPECTED_COMPONENT_IDS = [
    "ember.activeLayers", "ember.authoringWorkbench", "ember.autoloopMatrix",
    "ember.connectionPanel", "ember.diagnostics", "ember.fixtureControlSurface",
    "ember.fixtureFunctionBrowser", "ember.fixtureProfileEditor",
    "ember.staticLookMatrix", "ember.staticLookPreview",
]

EXPECTED_CAPABILITY_IDS = ["content.staticLookPreview", "content.staticLooks"]

EXPECTED_VALUE_IDS = [
    "target.autoloop", "target.fixture", "target.fixtureGroup", "target.project",
    "target.staticLook", "target.trackScript", "unit.beats", "unit.bpm",
    "unit.normalized", "value.adapterState", "value.fixtureProperty",
]


def load_fixture(name: str):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


class UiRegistryTests(unittest.TestCase):
    def test_current_native_ordinals_and_ids_are_exact(self):
        manifest, collections, digest = registry.load_registry()
        self.assertEqual(manifest["nativeMode"], "integrated")
        self.assertEqual(manifest["registrySetVersion"], "1.9.0")
        self.assertEqual(manifest["registryGeneration"], 2)
        self.assertRegex(digest, r"^[0-9a-f]{64}$")
        commands = sorted(collections["commands"], key=lambda item: item["nativeOrdinal"])
        states = sorted(collections["states"], key=lambda item: item["nativeOrdinal"])
        self.assertEqual([item["id"] for item in commands], EXPECTED_COMMAND_IDS)
        self.assertEqual([item["nativeOrdinal"] for item in commands], list(range(31)))
        self.assertEqual([item["id"] for item in states], EXPECTED_STATE_IDS)
        self.assertEqual([item["nativeOrdinal"] for item in states], list(range(54)))
        self.assertEqual([item["id"] for item in collections["components"]], EXPECTED_COMPONENT_IDS)
        self.assertEqual([item["id"] for item in collections["capabilities"]], EXPECTED_CAPABILITY_IDS)
        self.assertEqual([item["id"] for item in collections["values"]], EXPECTED_VALUE_IDS)
        for kind in ("components", "capabilities", "values"):
            self.assertTrue(all(item["callable"] is False for item in collections[kind]))

    def test_committed_outputs_are_clean_and_repeatable(self):
        first = registry.output_map()
        second = registry.output_map()
        self.assertEqual(first, second)
        for path, content in first.items():
            self.assertEqual(path.read_text(encoding="utf-8"), content, path)

    def test_schema_documents_and_generated_catalog_contract(self):
        schema_root = ROOT / "spec/ui/schema"
        source_schema = json.loads((schema_root / "ui-registry-source.schema.json").read_text(encoding="utf-8"))
        catalog_schema = json.loads((schema_root / "ui-registry-catalog.schema.json").read_text(encoding="utf-8"))
        diff_schema = json.loads((schema_root / "ui-registry-diff.schema.json").read_text(encoding="utf-8"))
        self.assertEqual(source_schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertEqual(catalog_schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertEqual(diff_schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        catalog = json.loads((ROOT / "spec/ui/registry/generated/ui-registry.catalog.json").read_text(encoding="utf-8"))
        for required in catalog_schema["required"]:
            self.assertIn(required, catalog)
        definition_collections = {
            "command": "commands",
            "state": "states",
            "result": "results",
            "component": "components",
            "capability": "capabilities",
            "reusableValue": "values",
        }
        for kind, collection_name in definition_collections.items():
            required = catalog_schema["$defs"][kind]["required"]
            collection = catalog[collection_name]
            for definition in collection:
                for field in required:
                    self.assertIn(field, definition, f"{definition.get('id')} missing {field}")

    def test_stale_generated_output_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            paths = {
                "HEADER_PATH": temporary / "registry.hpp",
                "CATALOG_PATH": temporary / "catalog.json",
                "REFERENCE_PATH": temporary / "reference.md",
                "CROSS_REFERENCE_PATH": temporary / "cross-reference.json",
            }
            with contextlib.ExitStack() as stack:
                for name, path in paths.items():
                    stack.enter_context(mock.patch.object(registry, name, path))
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(registry.generate(False), 0)
                paths["HEADER_PATH"].write_text("stale\n", encoding="utf-8")
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(registry.generate(True), 1)

    def test_duplicate_and_unknown_references_fail(self):
        duplicate = load_fixture("duplicate.json")
        with self.assertRaises(registry.RegistryError):
            registry.unique_by_id(duplicate["kind"], duplicate["definitions"])
        _, collections, _ = registry.load_registry()
        unknown = load_fixture("unknown-reference.json")
        for kind in (
            "commands", "states", "components", "capabilities", "values", "results", "interactions"
        ):
            isolated = copy.deepcopy(unknown)
            for other_kind in isolated["references"]:
                if other_kind != kind:
                    isolated["references"][other_kind] = []
            with self.subTest(kind=kind), self.assertRaises(registry.RegistryError):
                registry.validate_cross_reference_artifact(
                    isolated, collections, f"unknown-reference-{kind}"
                )

        duplicate_tokens = load_fixture("duplicate-value-token.json")
        with self.assertRaises(registry.RegistryError):
            registry.validate_value_definitions(duplicate_tokens["definitions"], 2)

    def test_compatibility_fixtures_classify_exactly(self):
        baseline = load_fixture("diff-baseline.json")
        cases = {
            "additive.json": "compatibleAdditive",
            "deprecated-replacement.json": "compatibleWithDeprecations",
            "breaking.json": "breaking",
        }
        for name, expected in cases.items():
            with self.subTest(name=name):
                report = registry.registry_diff(baseline, load_fixture(name))
                self.assertEqual(report["classification"], expected)
        deprecated = load_fixture("deprecated-replacement.json")
        registry.validate_replacement_graph("commands", deprecated["commands"])

        contract_baseline = load_fixture("contract-baseline.json")
        for name in ("contract-breaking.json", "lifecycle-regression.json"):
            with self.subTest(name=name):
                report = registry.registry_diff(contract_baseline, load_fixture(name))
                self.assertEqual(report["classification"], "breaking")
                self.assertTrue(report["manualActionRequired"])

        mismatch = load_fixture("replacement-type-mismatch.json")
        with self.assertRaises(registry.RegistryError):
            registry.validate_replacement_graph(mismatch["kind"], mismatch["definitions"])

    def test_command_feedback_addition_is_compatible_but_removal_is_breaking(self):
        baseline = load_fixture("diff-baseline.json")
        candidate = copy.deepcopy(baseline)
        candidate["commands"][0]["feedback"] = ["state.original", "state.added"]
        baseline["commands"][0]["feedback"] = ["state.original"]
        report = registry.registry_diff(baseline, candidate)
        self.assertEqual(report["classification"], "compatibleAdditive")

        removed = copy.deepcopy(candidate)
        removed["commands"][0]["feedback"] = []
        report = registry.registry_diff(candidate, removed)
        self.assertEqual(report["classification"], "breaking")

    def test_generation_two_is_compatible_additive_against_v1(self):
        manifest, collections, digest = registry.load_registry()
        baseline = json.loads(
            (ROOT / "spec/ui/registry/baselines/ui-registry-v1.json").read_text(encoding="utf-8")
        )
        report = registry.registry_diff(
            baseline, registry.generate_catalog(manifest, collections, digest)
        )
        self.assertEqual(report["classification"], "compatibleAdditive")
        self.assertEqual(report["summary"]["added"], 40)
        self.assertEqual(report["summary"]["changedCompatible"], 17)
        self.assertEqual(report["summary"]["changedBreaking"], 0)
        self.assertEqual(report["summary"]["removed"], 0)
        self.assertFalse(report["manualActionRequired"])

    def test_value_component_capability_and_result_validation_fails_closed(self):
        manifest, collections, _ = registry.load_registry()

        mutations = []

        def definition(candidate, kind, identifier):
            return next(item for item in candidate[kind] if item["id"] == identifier)

        unknown_schema = copy.deepcopy(collections)
        definition(unknown_schema, "states", "connection.os2l.status")["value"]["schemaRef"] = "value.missing"
        mutations.append(unknown_schema)

        unknown_unit = copy.deepcopy(collections)
        definition(unknown_unit, "commands", "transport.manualBpm.set")["parameters"]["bpm"]["unit"] = "missingUnit"
        mutations.append(unknown_unit)

        unknown_target = copy.deepcopy(collections)
        definition(unknown_target, "commands", "staticLook.activate")["parameters"]["lookId"]["targetKind"] = "missingTarget"
        mutations.append(unknown_target)

        unknown_component_command = copy.deepcopy(collections)
        unknown_component_command["components"][0]["requiredCommands"].append("missing.command")
        mutations.append(unknown_component_command)

        capability_cycle = copy.deepcopy(collections)
        capability_cycle["capabilities"][0]["dependencies"].append("content.staticLooks")
        mutations.append(capability_cycle)

        invalid_result = copy.deepcopy(collections)
        invalid_result["results"][0]["terminal"] = "yes"
        mutations.append(invalid_result)

        lifecycle_mismatch = copy.deepcopy(collections)
        lifecycle_mismatch["components"][0]["status"] = "deprecated"
        mutations.append(lifecycle_mismatch)

        invalid_binding_policy = copy.deepcopy(collections)
        definition(
            invalid_binding_policy,
            "commands",
            "staticLook.preview.start",
        )["actionBindable"] = "false"
        mutations.append(invalid_binding_policy)

        for index, candidate in enumerate(mutations):
            with self.subTest(index=index), self.assertRaises(registry.RegistryError):
                registry.validate_registry(manifest, candidate)

    def test_invocation_results_are_explicit_without_native_abi_expansion(self):
        _, collections, _ = registry.load_registry()
        results = collections["results"]
        native = sorted(
            (item for item in results if "nativeOrdinal" in item),
            key=lambda item: item["nativeOrdinal"],
        )
        self.assertEqual([item["nativeOrdinal"] for item in native], list(range(10)))
        self.assertEqual(
            {item["id"] for item in results if item["status"] == "reserved"},
            {"cancelled", "missingTarget", "startedAsync"},
        )
        for result in results:
            self.assertIsInstance(result["terminal"], bool)
            self.assertTrue(result["label"])
            self.assertTrue(result["description"])

    def test_reserved_autoloop_v2_names_are_not_published(self):
        manifest, collections, _ = registry.load_registry()
        reservations = manifest["ownerReservations"]
        self.assertEqual(reservations[0]["issue"], 59)
        published = {item["id"] for item in collections["states"]}
        for semantic_slot in reservations[0]["semanticSlots"]:
            self.assertNotIn(semantic_slot, published)

    def test_bypass_ledger_has_no_new_area(self):
        ledger = (ROOT / "spec/ui/current-win32-direct-callback-bypass-ledger-v0.md").read_text(
            encoding="utf-8"
        )
        areas = []
        for line in ledger.splitlines():
            if not line.startswith("| ") or line.startswith("| Area") or line.startswith("| ---"):
                continue
            areas.append(line.split("|", 2)[1].strip())
        self.assertEqual(areas, ["Connections", "Project lifecycle", "Authoring", "Navigation"])

    def test_native_first_control_is_deterministic(self):
        source = ROOT / "native-core/tools/ui_registry_native_control_spike.cpp"
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "native-control"
            subprocess.run(
                ["g++", "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                 str(source), "-o", str(executable)],
                check=True,
                cwd=ROOT,
            )
            first = subprocess.check_output([str(executable)], text=True)
            second = subprocess.check_output([str(executable)], text=True)
        self.assertEqual(first, second)
        output = json.loads(first)
        self.assertEqual([item["id"] for item in output["commands"]], [
            "show.start", "output.blackout.set", "staticLook.hold",
            "autoloop.launch", "group.override.property.set",
        ])
        self.assertEqual(len(output["states"]), 5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
