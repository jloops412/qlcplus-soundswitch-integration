#!/usr/bin/env python3

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SLINT = ROOT / "native-core/slint-lab/fixtures_looks_lab.slint"
ADAPTER = ROOT / "native-core/slint-lab/fixtures_looks_lab.cpp"
CMAKE = ROOT / "native-core/CMakeLists.txt"
WORKFLOW = ROOT / ".github/workflows/slint-fixtures-looks-lab.yml"


class SlintLabContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.surface = SLINT.read_text(encoding="utf-8")
        cls.adapter = ADAPTER.read_text(encoding="utf-8")
        cls.cmake = CMAKE.read_text(encoding="utf-8")
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_surface_is_explicitly_a_pinned_non_product_lab(self):
        self.assertIn("Slint 1.17.1 LAB", self.surface)
        self.assertIn("not a product preview", self.surface)
        self.assertIn("find_package(Slint 1.17.1 EXACT", self.cmake)
        self.assertIn("EMBERLIGHTS_BUILD_SLINT_LAB", self.cmake)
        self.assertNotIn("install(TARGETS EmberLightsSlintLab", self.cmake)

    def test_first_slice_has_real_fixture_and_static_look_workflows(self):
        required = (
            "Fixture library",
            "PATCH TARGETS",
            "Static Looks",
            "New Look",
            "Save Look",
            "Duplicate",
            "INTENSITY & OWNERSHIP",
            "COLOR MIXER",
            "POSITION & BEAM",
            "GOBO & PROFILE CHOICES",
            'label: "Release"',
            'label: "Set"',
            'label: "Force zero"',
            "profile-search-changed",
            "look-search-changed",
            "preview-start",
            "preview-stop",
        )
        for text in required:
            self.assertIn(text, self.surface, text)

    def test_raw_dmx_is_advanced_evidence_not_ordinary_authoring(self):
        advanced = self.surface.index("if root.advanced-open")
        raw_dmx = self.surface.index("Raw DMX")
        self.assertGreater(raw_dmx, advanced)
        ordinary = self.surface[:advanced]
        self.assertNotIn("Raw DMX", ordinary)

    def test_minimum_viewport_and_accessibility_contract_are_present(self):
        self.assertIn("preferred-width: 1366px", self.surface)
        self.assertIn("preferred-height: 768px", self.surface)
        self.assertIn("min-width: 1366px", self.surface)
        self.assertIn("min-height: 768px", self.surface)
        self.assertGreaterEqual(self.surface.count("accessible-role"), 6)
        self.assertGreaterEqual(self.surface.count("accessible-label"), 6)

    def test_adapter_uses_existing_domain_mutations_and_opens_no_output(self):
        for function in (
            "build_fixtures_looks_shell_model",
            "apply_static_look_property",
            "remove_static_look_property",
            "apply_static_look_control_choice",
            "duplicate_static_look_draft",
        ):
            self.assertIn(function, self.adapter)
        self.assertIn("--model-smoke", self.adapter)
        self.assertIn("no DMX output", self.adapter)
        self.assertNotIn("OutputBackend", self.adapter)
        self.assertNotIn("start_output", self.adapter)

    def test_windows_artifact_route_is_manual_pinned_and_non_product(self):
        self.assertIn("workflow_dispatch", self.workflow)
        self.assertNotIn("pull_request:", self.workflow)
        self.assertNotIn("push:", self.workflow)
        self.assertIn("Slint-cpp-1.17.1-win64-MSVC-AMD64.exe", self.workflow)
        self.assertIn(
            "f5b537da448c1e3d72a24a774e19518ae412b9706b8ef49bdee64b62b878fe56",
            self.workflow,
        )
        self.assertIn(
            "EmberLights-Fixtures-Looks-Slint-Lab-win-x64.zip",
            self.workflow,
        )
        self.assertNotIn("softprops/action-gh-release", self.workflow)

    def test_markup_compiles_when_exact_compiler_is_available(self):
        compiler = os.environ.get("SLINT_COMPILER") or shutil.which(
            "slint-compiler"
        )
        if compiler is None:
            self.skipTest("set SLINT_COMPILER to run the pinned syntax check")
        version = subprocess.run(
            [compiler, "--version"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
        self.assertEqual(version, "slint-compiler 1.17.1")
        with tempfile.TemporaryDirectory() as directory:
            generated = Path(directory) / "fixtures_looks_lab.h"
            subprocess.run(
                [
                    compiler,
                    str(SLINT),
                    "-f",
                    "cpp",
                    "-o",
                    str(generated),
                    "--style",
                    "fluent",
                    "--embed-resources=embed-files",
                ],
                check=True,
                cwd=ROOT,
            )
            self.assertTrue(generated.is_file())
            self.assertIn(
                "Slint compiler version 1.17.1",
                generated.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
