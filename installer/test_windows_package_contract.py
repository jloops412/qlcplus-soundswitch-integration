from __future__ import annotations

import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import windows_package_contract as contract  # noqa: E402


VERSION = "0.1.0-preview.123"
COMMIT = "0123456789abcdef0123456789abcdef01234567"


class WindowsPackageContractTests(unittest.TestCase):
    def make_stage(self, root: Path) -> None:
        for index, relative in enumerate(contract.REQUIRED_FILES):
            destination = root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(f"payload-{index}-{relative}\n".encode("utf-8"))

    def write_minimal_pe(self, path: Path, imported_dll: str, machine: int = 0x8664) -> None:
        image = bytearray(0x400)
        image[:2] = b"MZ"
        struct.pack_into("<I", image, 0x3C, 0x80)
        image[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<HHIIIHH", image, 0x84, machine, 1, 0, 0, 0, 0xF0, 0x22)
        optional = 0x98
        struct.pack_into("<H", image, optional, 0x20B)
        struct.pack_into("<I", image, optional + 108, 16)
        struct.pack_into("<II", image, optional + 120, 0x1000, 40)
        section = optional + 0xF0
        image[section : section + 8] = b".idata\0\0"
        struct.pack_into("<IIII", image, section + 8, 0x200, 0x1000, 0x200, 0x200)
        struct.pack_into("<IIIII", image, 0x200, 0, 0, 0, 0x1040, 0)
        encoded_name = imported_dll.encode("ascii") + b"\0"
        image[0x240 : 0x240 + len(encoded_name)] = encoded_name
        path.write_bytes(image)

    def test_manifest_is_deterministic_and_round_trips(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            first = contract.create_manifest(root, VERSION, COMMIT)
            first_bytes = (root / contract.MANIFEST_NAME).read_bytes()
            second = contract.create_manifest(root, VERSION, COMMIT.upper())
            second_bytes = (root / contract.MANIFEST_NAME).read_bytes()
            verified = contract.verify_manifest(root, VERSION, COMMIT)
            self.assertEqual(first, second)
            self.assertEqual(first_bytes, second_bytes)
            self.assertEqual(verified, first)
            self.assertEqual(verified["safetyBoundary"], contract.SAFETY_BOUNDARY)

    def test_create_rejects_missing_required_payload(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            (root / contract.REQUIRED_FILES[-1]).unlink()
            with self.assertRaisesRegex(contract.ContractError, "missing required"):
                contract.create_manifest(root, VERSION, COMMIT)

    def test_create_rejects_unbundled_cross_toolchain_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            self.write_minimal_pe(root / "EmberLights.exe", "libc++.dll")
            with self.assertRaisesRegex(contract.ContractError, "unbundled runtime"):
                contract.create_manifest(root, VERSION, COMMIT)
            (root / "libc++.dll").write_bytes(b"bundled runtime")
            contract.create_manifest(root, VERSION, COMMIT)

    def test_create_accepts_system_import_and_rejects_non_x64_image(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            application = root / "EmberLights.exe"
            self.write_minimal_pe(application, "KERNEL32.dll")
            contract.create_manifest(root, VERSION, COMMIT)
            self.write_minimal_pe(application, "KERNEL32.dll", machine=0x14C)
            with self.assertRaisesRegex(contract.ContractError, "not x86-64"):
                contract.create_manifest(root, VERSION, COMMIT)

    def test_verify_rejects_payload_mutation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            contract.create_manifest(root, VERSION, COMMIT)
            (root / "EmberLights.exe").write_bytes(b"changed")
            with self.assertRaisesRegex(contract.ContractError, "size mismatch|SHA-256 mismatch"):
                contract.verify_manifest(root, VERSION, COMMIT)

    def test_verify_rejects_unlisted_files_except_inno_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            contract.create_manifest(root, VERSION, COMMIT)
            (root / "unexpected.dll").write_bytes(b"unexpected")
            with self.assertRaisesRegex(contract.ContractError, "unexpected.dll"):
                contract.verify_manifest(root, VERSION, COMMIT)
            (root / "unexpected.dll").unlink()
            (root / "unins000.exe").write_bytes(b"inno uninstaller")
            (root / "unins000.dat").write_bytes(b"inno metadata")
            contract.verify_manifest(root, VERSION, COMMIT, "inno-installed")
            with self.assertRaisesRegex(contract.ContractError, "unins000"):
                contract.verify_manifest(root, VERSION, COMMIT, "exact")

    def test_verify_rejects_identity_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            contract.create_manifest(root, VERSION, COMMIT)
            with self.assertRaisesRegex(contract.ContractError, "does not match expected"):
                contract.verify_manifest(root, "0.1.0-preview.124", COMMIT)
            with self.assertRaisesRegex(contract.ContractError, "does not match expected"):
                contract.verify_manifest(root, VERSION, "1" * 40)
            with self.assertRaisesRegex(contract.ContractError, "exact 40-character"):
                contract.verify_manifest(root, VERSION, "not-a-commit")

    def test_verify_rejects_traversal_and_duplicate_json_keys(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            manifest = contract.create_manifest(root, VERSION, COMMIT)
            manifest["files"][0]["path"] = "../outside.exe"
            (root / contract.MANIFEST_NAME).write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            with self.assertRaisesRegex(contract.ContractError, "normalized"):
                contract.verify_manifest(root, VERSION, COMMIT)
            (root / contract.MANIFEST_NAME).write_text(
                '{"format":"a","format":"b"}', encoding="utf-8"
            )
            with self.assertRaisesRegex(contract.ContractError, "duplicate key"):
                contract.verify_manifest(root, VERSION, COMMIT)

    def test_verify_rejects_windows_illegal_manifest_path(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            manifest = contract.create_manifest(root, VERSION, COMMIT)
            manifest["files"][0]["path"] = "bad?.exe"
            (root / contract.MANIFEST_NAME).write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            with self.assertRaisesRegex(contract.ContractError, "illegal Windows"):
                contract.verify_manifest(root, VERSION, COMMIT)

    def test_verify_rejects_boolean_format_version(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            manifest = contract.create_manifest(root, VERSION, COMMIT)
            manifest["formatVersion"] = True
            (root / contract.MANIFEST_NAME).write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            with self.assertRaisesRegex(contract.ContractError, "format or version"):
                contract.verify_manifest(root, VERSION, COMMIT)

    def test_create_rejects_case_collisions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            collision = root / "EMBERLIGHTS.EXE"
            try:
                collision.write_bytes(b"collision")
            except OSError:
                self.skipTest("filesystem does not permit a case-distinct collision")
            if collision.samefile(root / "EmberLights.exe"):
                self.skipTest("filesystem is case-insensitive")
            with self.assertRaisesRegex(contract.ContractError, "collide on Windows"):
                contract.create_manifest(root, VERSION, COMMIT)

    def test_create_rejects_symbolic_links(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.make_stage(root)
            link = root / "linked-payload"
            try:
                link.symlink_to(root / "EmberLights.exe")
            except OSError:
                self.skipTest("symbolic links are unavailable")
            with self.assertRaisesRegex(contract.ContractError, "symbolic link"):
                contract.create_manifest(root, VERSION, COMMIT)

    def test_inno_installer_consumes_the_complete_staged_tree(self) -> None:
        installer = (Path(__file__).parent / "EmberLights.iss").read_text(
            encoding="utf-8"
        )
        source_lines = [
            line.strip() for line in installer.splitlines() if line.startswith("Source:")
        ]
        self.assertEqual(
            source_lines,
            [
                'Source: "{#BuildDir}\\*"; DestDir: "{app}"; '
                "Flags: ignoreversion recursesubdirs createallsubdirs"
            ],
        )
        extension_line = next(
            line
            for line in installer.splitlines()
            if 'Subkey: "Software\\Classes\\.emberlights"' in line
        )
        self.assertIn("uninsdeletevalue", extension_line)
        self.assertIn("uninsdeletekeyifempty", extension_line)

    def test_nsis_fallback_is_per_user_gated_and_cleans_exact_payload(self) -> None:
        installer = (Path(__file__).parent / "EmberLights.nsi").read_text(
            encoding="utf-8"
        )
        self.assertIn('RequestExecutionLevel user', installer)
        self.assertIn('InstallDir "$LOCALAPPDATA\\Programs\\${AppName}"', installer)
        self.assertIn('${RunningX64}', installer)
        self.assertIn('${AtLeastWin10}', installer)
        self.assertIn('FindWindow $0 "EmberLightsMainWindow"', installer)
        self.assertIn('File /r "${BuildDir}\\*"', installer)
        self.assertNotIn('RMDir /r "$INSTDIR"', installer)
        for relative in contract.REQUIRED_FILES:
            windows_path = relative.replace("/", "\\")
            self.assertIn(f'Delete "$INSTDIR\\{windows_path}"', installer)
        self.assertIn(
            f'Delete "$INSTDIR\\{contract.MANIFEST_NAME}"', installer
        )

    def test_windows_cmake_stages_end_user_tools_under_tools(self) -> None:
        cmake = (Path(__file__).parents[1] / "native-core/CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        self.assertIn("install(TARGETS EmberLights RUNTIME DESTINATION .)", cmake)
        self.assertIn(
            "install(TARGETS midi_capture emberlights_qualify emberlights_migrate",
            cmake,
        )
        self.assertIn("RUNTIME DESTINATION Tools)", cmake)
        self.assertIn("Tools/midi_capture.exe", contract.REQUIRED_FILES)

    def test_windows_workflow_wires_only_non_outputting_package_smokes(self) -> None:
        workflow = (Path(__file__).parents[1] / ".github/workflows/native-core.yml").read_text(
            encoding="utf-8"
        )
        windows_job = workflow.split("  package-windows:", 1)[1].split(
            "  package-linux:", 1
        )[0]
        self.assertIn("EMBERLIGHTS_SOURCE_COMMIT: ${{ github.sha }}", workflow)
        self.assertIn("EMBERLIGHTS_SOURCE_HEAD_COMMIT:", workflow)
        self.assertIn("git rev-parse HEAD", windows_job)
        self.assertIn("windows_package_contract.py create", windows_job)
        self.assertGreaterEqual(windows_job.count("windows_package_contract.py verify"), 3)
        self.assertIn("installed-smoke-qualification.json", windows_job)
        self.assertIn("EmberLights-Windows-payload-manifest.json", windows_job)
        self.assertIn("--self-test", windows_job)
        self.assertIn("unins*.exe", windows_job)
        self.assertNotIn("--active-test", windows_job)
        self.assertNotIn("--network-loopback", windows_job)


if __name__ == "__main__":
    unittest.main()
