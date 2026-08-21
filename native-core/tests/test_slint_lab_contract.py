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
PRIMITIVES = ROOT / "native-core/slint-lab/ember_ui_primitives.slint"
ADAPTER = ROOT / "native-core/slint-lab/fixtures_looks_lab.cpp"
CMAKE = ROOT / "native-core/CMakeLists.txt"
WORKFLOW = ROOT / ".github/workflows/slint-fixtures-looks-lab.yml"


class SlintLabContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.surface = SLINT.read_text(encoding="utf-8")
        cls.primitives = PRIMITIVES.read_text(encoding="utf-8")
        cls.markup = cls.surface + cls.primitives
        cls.adapter = ADAPTER.read_text(encoding="utf-8")
        cls.cmake = CMAKE.read_text(encoding="utf-8")
        cls.workflow = WORKFLOW.read_text(encoding="utf-8")

    def test_surface_has_a_pinned_lab_and_an_accepted_product_mode(self):
        self.assertIn("Slint 1.17.1 LAB", self.surface)
        self.assertIn("not a product preview", self.surface)
        self.assertIn("find_package(Slint 1.17.1 EXACT", self.cmake)
        self.assertIn("EMBERLIGHTS_BUILD_SLINT_LAB", self.cmake)
        self.assertNotIn("install(TARGETS EmberLightsSlintLab", self.cmake)
        self.assertIn("EMBERLIGHTS_BUILD_SLINT_DEFAULT", self.cmake)
        self.assertIn("EMBERLIGHTS_PRODUCT_SHELL=1", self.cmake)
        self.assertIn(
            "install(TARGETS EmberLights EmberLightsSafe RUNTIME DESTINATION .)",
            self.cmake,
        )
        self.assertIn("Powered by Slint 1.17.1", self.surface)
        self.assertIn("EmberLights-Safe", self.cmake)

    def test_first_slice_has_real_fixture_and_static_look_workflows(self):
        required = (
            "Fixture library",
            "PATCH TARGETS",
            "Static Looks",
            "New Look",
            "Save Look",
            "Save Project",
            'text: "Undo"',
            'text: "Redo"',
            "Duplicate",
            "FIXTURE PARAMETERS",
            "COLOR EMITTERS",
            "ColorMixerSummary",
            "color-preview-visible",
            "Profile-backed RGBWA emitter preview",
            "PARAMETER CARDS",
            "ParameterFamilyCard",
            "StateBadge",
            "ValueReadout",
            "value-controls",
            "choices",
            'label: "Release"',
            'label: "Set"',
            'label: "Force zero"',
            "profile-search-changed",
            "look-search-changed",
            "parameter-search-changed",
            "select-control-group",
            "preview-simulate",
            "preview-physical",
            "preview-stop",
            "Preview fixtures",
            "Blackout & stop",
        )
        for text in required:
            self.assertIn(text, self.markup, text)
        for obsolete in (
            "intensity-choice-id",
            "red-choice-id",
            "focus-choice-id",
            "SELECTOR OWNERSHIP",
            "PROFILE FUNCTIONS & RANGES",
        ):
            self.assertNotIn(obsolete, self.markup, obsolete)

    def test_parameter_families_keep_controls_choices_and_ownership_together(self):
        for text in (
            "export struct ParameterFamilyItem",
            "for item in root.parameter-family-items: ParameterFamilyCard",
            "for control in item.value-controls: ParameterValueRow",
            "for choice in item.choices: ChoiceTile",
            "item.ownership-choice-id",
        ):
            self.assertIn(text, self.markup, text)
        for text in (
            "model.control_groups",
            "control.widget_id != group.stable_id",
            "family.value_controls",
            "family.choices",
            "set_parameter_family_items",
        ):
            self.assertIn(text, self.adapter, text)

    def test_raw_dmx_is_advanced_evidence_not_ordinary_authoring(self):
        advanced = self.surface.index("if root.advanced-open")
        raw_dmx = self.surface.index("Raw DMX")
        self.assertGreater(raw_dmx, advanced)
        ordinary = self.surface[:advanced]
        self.assertNotIn("Raw DMX", ordinary)
        self.assertIn("diagnostic-items", self.surface[advanced:])
        self.assertIn("control_diagnostics", self.adapter)
        self.assertNotIn("Gobo / Dots  •  Ch 9", self.surface)

    def test_minimum_viewport_and_accessibility_contract_are_present(self):
        self.assertIn("preferred-width: 1366px", self.surface)
        self.assertIn("preferred-height: 768px", self.surface)
        self.assertIn("min-width: 1024px", self.surface)
        self.assertIn("min-height: 640px", self.surface)
        self.assertIn("property <length> library-width: 250px", self.surface)
        self.assertIn("property <length> looks-width: 280px", self.surface)
        self.assertGreaterEqual(self.surface.count("accessible-role"), 6)
        self.assertGreaterEqual(self.surface.count("accessible-label"), 6)

    def test_lab_chrome_does_not_advertise_unimplemented_workflows(self):
        """The focused evaluation slice must not look like a fake full workstation."""
        for inert_control in (
            'Button { text: "Command Explorer"; }',
            'Button { text: "Import OFL / QXF"; }',
            'Button { text: "Categories"; }',
            "WorkspaceRailButton",
            "not wired",
        ):
            self.assertNotIn(inert_control, self.surface, inert_control)
        self.assertIn('label: "Workspace"', self.surface)
        self.assertIn('state: root.product-shell ? root.workspace-mode == "live"', self.surface)
        self.assertIn('"Blackout & stop"', self.surface)
        self.assertIn("Profile-backed RGBWA emitter preview", self.surface)

    def test_shared_visual_primitives_stay_outside_the_screen_composition(self):
        self.assertIn('from "ember_ui_primitives.slint"', self.surface)
        for primitive in (
            "export global Theme",
            "export component Hairline",
            "export component StatusPill",
            "export component SectionTitle",
            "export component StateBadge",
            "export component ValueReadout",
            "export component WorkspaceTab",
            "export component HealthCard",
        ):
            self.assertIn(primitive, self.primitives, primitive)

    def test_product_shell_has_distinct_studio_and_live_workspaces(self):
        for text in (
            'in-out property <string> workspace-mode: "studio"',
            'label: "STUDIO"',
            'detail: "Build and rehearse"',
            'label: "LIVE"',
            'detail: "Perform the show"',
            'root.workspace-mode == "studio"',
            'root.workspace-mode == "live"',
            'text: "LIVE PERFORMANCE"',
            "HealthCard",
        ):
            self.assertIn(text, self.surface, text)
        self.assertIn("root.product-shell ? 150px : 142px", self.surface)
        self.assertNotIn("root.product-shell ? 222px", self.surface)

    def test_adapter_uses_domain_commands_and_keeps_output_behind_explicit_authority(self):
        for function in (
            "build_fixtures_looks_shell_model",
            "apply_static_look_property",
            "remove_static_look_property",
            "apply_static_look_control_choice",
            "duplicate_static_look_draft",
            "StudioDocumentService",
            "commit_static_look_draft",
            "save_project_atomic",
            "acknowledge_saved",
            "StaticLookPreviewCoordinator",
            "UiCommandFacade",
            "StaticLookPreviewStart",
            "--allow-physical-preview",
        ):
            self.assertIn(function, self.adapter)
        self.assertIn("--model-smoke", self.adapter)
        self.assertIn("--startup-smoke", self.adapter)
        self.assertIn("--project", self.adapter)
        self.assertIn("state.document.undo", self.adapter)
        self.assertIn("state.document.redo", self.adapter)
        self.assertIn("no DMX output", self.adapter)
        self.assertIn("requires an explicit --project path", self.adapter)
        self.assertIn("static_look_physical_preview_output_configured", self.adapter)
        self.assertNotIn("state.project.looks.push_back", self.adapter)
        self.assertNotIn("OutputBackend", self.adapter)
        self.assertNotIn("start_output", self.adapter)
        self.assertNotIn("RunnerService::start", self.adapter)

    def test_product_file_workflows_and_safe_bridge_are_wired(self):
        for callback in (
            "new-project",
            "open-project",
            "save-project-as",
            "open-safe",
        ):
            self.assertIn(callback, self.surface)
        for function in (
            "choose_project_path",
            "save_studio_project_as",
            "launch_safe_shell",
        ):
            self.assertIn(function, self.adapter)

    def test_product_shell_owns_virtualdj_listener_and_safe_handoff(self):
        for text in (
            "Os2lService",
            "configure_product_os2l",
            "publish_blackout(true)",
            "state.os2l_service->stop()",
            "slint::quit_event_loop",
        ):
            self.assertIn(text, self.adapter)
        for text in (
            'label: "VirtualDJ"',
            "os2l-state",
            "os2l-detail",
            'text: "Compatibility Tools"',
        ):
            self.assertIn(text, self.surface)

    def test_product_shell_owns_a_canonical_live_control_strip(self):
        for text in (
            "ProductLiveCommandHost",
            "compile_project_with_persisted_autoloops",
            "project_active_path",
            "UiCommandId::ShowToggleRunning",
            "UiCommandId::BlackoutToggle",
            "UiCommandId::WorkLightToggle",
            "UiCommandId::ReleaseAllOverrides",
            "UiCommandId::StaticLookToggle",
            "Live owns output. Stop Live before editing Static Looks.",
        ):
            self.assertIn(text, self.adapter, text)
        for text in (
            'label: "Live"',
            'label: "Sync"',
            '"Start Live"',
            '"Stop Live"',
            '"Take Look Live"',
            '"Work Light"',
            'text: "Release Overrides"',
            '"BLACKOUT"',
            "live-toggle",
            "live-look-toggle",
        ):
            self.assertIn(text, self.surface, text)

    def test_product_shell_projects_and_controls_persisted_autoloops(self):
        for text in (
            "LiveViewModel",
            "refresh_projection",
            "AutoloopLaunch",
            "AutoloopPrevious",
            "AutoloopNext",
            "AutoloopClear",
            "AutoloopBankFilterEnableAll",
            "AutoloopBankFilterSelectExclusive",
            "active_autoloop_progress",
            "active_autoloop_completed_cycles",
        ):
            self.assertIn(text, self.adapter, text)
        for text in (
            "LIVE AUTOLOOPS",
            "AutoloopBankChip",
            "AutoloopPadTile",
            "autoloop-page-previous",
            "autoloop-page-next",
            "autoloop-select-bank",
            "autoloop-select-slot",
            "autoloop-launch",
            'text: "Selected bank"',
            'text: "Clear Autoloop"',
            "autoloop-pad-row-one-items",
            "autoloop-pad-row-two-items",
            "autoloop-pad-row-three-items",
            "autoloop-pad-row-four-items",
        ):
            self.assertIn(text, self.surface, text)
        for text in (
            "set_autoloop_pad_row_one_items",
            "set_autoloop_pad_row_two_items",
            "set_autoloop_pad_row_three_items",
            "set_autoloop_pad_row_four_items",
        ):
            self.assertIn(text, self.adapter, text)

    def test_windows_artifact_route_is_scoped_pinned_and_non_product(self):
        self.assertIn("workflow_dispatch", self.workflow)
        self.assertIn("pull_request:", self.workflow)
        self.assertIn('"native-core/slint-lab/**"', self.workflow)
        self.assertIn(
            '"native-core/src/fixture_control_surface.cpp"', self.workflow
        )
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
            compiled = subprocess.run(
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
                capture_output=True,
                text=True,
            )
            self.assertNotIn("warning", compiled.stderr.lower())
            self.assertTrue(generated.is_file())
            self.assertIn(
                "Slint compiler version 1.17.1",
                generated.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
