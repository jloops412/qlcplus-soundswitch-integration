#!/usr/bin/env python3
"""Create and verify the deterministic EmberLights Windows payload contract."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
from typing import Any, Iterable


FORMAT = "emberlights-windows-payload-manifest"
FORMAT_VERSION = 1
MANIFEST_NAME = "EmberLights-Windows-payload-manifest.json"
MAX_FILES = 4096
MAX_MANIFEST_BYTES = 4 * 1024 * 1024

REQUIRED_FILES = (
    "EmberLights.exe",
    "Tools/emberlights_migrate.exe",
    "Tools/emberlights_qualify.exe",
    "Tools/midi_capture.exe",
    "Tools/soundswitch_control_one_probe.exe",
    "Tools/soundswitch_micro_probe.exe",
    "Templates/EmberLights-2026-V1-Template.emberlights",
    "docs/README.md",
    "docs/THIRD_PARTY_NOTICES.md",
    "docs/15_WINDOWS_V1_TESTING.md",
    "docs/16_QLC_FIXTURE_IMPORT.md",
    "docs/17_PRODUCTION_RELEASE_GATE.md",
    "docs/18_SOUNDSWITCH_MIGRATION.md",
    "docs/20_CONTROL_ONE_DMX_QUALIFICATION.md",
    "docs/31_LIMITED_BETA_OS2L_AND_INSTALLER_TEST.md",
    "docs/32_FIXTURE_TRUTH_AND_STATIC_LOOK_BUILDER_CHECKPOINT.md",
    "docs/MORNING_HARDWARE_TEST.md",
)

SAFETY_BOUNDARY = {
    "releaseClaim": "testing-preview",
    "defaultOutput": "disabled",
    "selfTests": "non-outputting",
    "physicalQualification": "not-claimed",
}

_VERSION_PATTERN = re.compile(r"^[0-9A-Za-z][0-9A-Za-z.+-]{0,63}$")
_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
_INNO_METADATA_PATTERN = re.compile(
    r"^unins[0-9]{3}\.(?:dat|exe|msg)$", re.IGNORECASE
)
_WINDOWS_RESERVED_NAMES = {
    "CON",
    "PRN",
    "AUX",
    "NUL",
    *(f"COM{index}" for index in range(1, 10)),
    *(f"LPT{index}" for index in range(1, 10)),
}


class ContractError(RuntimeError):
    """Raised when a staged or installed payload violates the contract."""


def _validate_identity(version: str, commit: str) -> tuple[str, str]:
    if not isinstance(version, str) or not _VERSION_PATTERN.fullmatch(version):
        raise ContractError(
            "version must be 1-64 ASCII letters, digits, dots, pluses, or hyphens"
        )
    if not isinstance(commit, str):
        raise ContractError("commit must be an exact 40-character hexadecimal SHA")
    normalized_commit = commit.lower()
    if not _COMMIT_PATTERN.fullmatch(normalized_commit):
        raise ContractError("commit must be an exact 40-character hexadecimal SHA")
    return version, normalized_commit


def _validate_relative_path(value: Any) -> str:
    if not isinstance(value, str) or not value or len(value) > 512:
        raise ContractError("manifest file paths must be non-empty strings up to 512 characters")
    if "\\" in value or ":" in value or value.startswith("/"):
        raise ContractError(f"manifest path is not portable and relative: {value!r}")
    parsed = PurePosixPath(value)
    if parsed.as_posix() != value or any(part in ("", ".", "..") for part in parsed.parts):
        raise ContractError(f"manifest path is not normalized: {value!r}")
    for part in parsed.parts:
        if any(ord(character) < 0x20 or character in '<>"|?*' for character in part):
            raise ContractError(f"manifest path has an illegal Windows character: {value!r}")
        if part.endswith((" ", ".")):
            raise ContractError(f"manifest path has a Windows-ambiguous component: {value!r}")
        basename = part.split(".", 1)[0].upper()
        if basename in _WINDOWS_RESERVED_NAMES:
            raise ContractError(f"manifest path uses a reserved Windows name: {value!r}")
    if value == MANIFEST_NAME:
        raise ContractError("the payload manifest must not hash itself")
    return value


def _reject_duplicate_keys(pairs: Iterable[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ContractError(f"payload manifest contains duplicate key {key!r}")
        result[key] = value
    return result


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _root_directory(root: Path) -> Path:
    if not root.exists() or not root.is_dir() or root.is_symlink():
        raise ContractError(f"payload root is not a real directory: {root}")
    return root.resolve()


def _payload_files(root: Path) -> list[tuple[str, Path]]:
    root = _root_directory(root)
    files: list[tuple[str, Path]] = []
    casefolded: dict[str, str] = {}
    for candidate in sorted(root.rglob("*"), key=lambda item: item.as_posix().casefold()):
        relative = candidate.relative_to(root).as_posix()
        if candidate.is_symlink():
            raise ContractError(f"payload contains a symbolic link: {relative}")
        if candidate.is_dir():
            continue
        if not candidate.is_file():
            raise ContractError(f"payload contains a non-regular file: {relative}")
        if relative == MANIFEST_NAME:
            continue
        normalized = _validate_relative_path(relative)
        folded = normalized.casefold()
        if folded in casefolded:
            raise ContractError(
                "payload contains paths that collide on Windows: "
                f"{casefolded[folded]!r} and {normalized!r}"
            )
        casefolded[folded] = normalized
        files.append((normalized, candidate))
    if len(files) > MAX_FILES:
        raise ContractError(f"payload exceeds the {MAX_FILES}-file contract limit")
    return files


def _assert_required(paths: set[str]) -> None:
    missing = [path for path in REQUIRED_FILES if path not in paths]
    if missing:
        raise ContractError("payload is missing required files: " + ", ".join(missing))


def create_manifest(root: Path, version: str, commit: str) -> dict[str, Any]:
    root = _root_directory(root)
    version, commit = _validate_identity(version, commit)
    payload = _payload_files(root)
    _assert_required({relative for relative, _ in payload})
    manifest: dict[str, Any] = {
        "format": FORMAT,
        "formatVersion": FORMAT_VERSION,
        "product": "EmberLights",
        "platform": "windows-x64",
        "version": version,
        "commit": commit,
        "safetyBoundary": dict(SAFETY_BOUNDARY),
        "files": [
            {
                "path": relative,
                "size": candidate.stat().st_size,
                "sha256": _sha256(candidate),
            }
            for relative, candidate in payload
        ],
    }
    encoded = (json.dumps(manifest, indent=2, ensure_ascii=True) + "\n").encode("utf-8")
    if len(encoded) > MAX_MANIFEST_BYTES:
        raise ContractError("generated payload manifest exceeds its size limit")
    destination = root / MANIFEST_NAME
    if destination.is_symlink():
        raise ContractError("payload manifest destination must not be a symbolic link")
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=f".{MANIFEST_NAME}.", dir=root, delete=False
        ) as temporary:
            temporary.write(encoded)
            temporary.flush()
            os.fsync(temporary.fileno())
            temporary_name = temporary.name
        os.replace(temporary_name, destination)
        temporary_name = None
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return manifest


def _read_manifest(root: Path) -> dict[str, Any]:
    manifest_path = root / MANIFEST_NAME
    if manifest_path.is_symlink() or not manifest_path.is_file():
        raise ContractError(f"payload manifest is missing or not a regular file: {manifest_path}")
    if manifest_path.stat().st_size > MAX_MANIFEST_BYTES:
        raise ContractError("payload manifest exceeds its size limit")
    try:
        parsed = json.loads(
            manifest_path.read_text(encoding="utf-8"),
            object_pairs_hook=_reject_duplicate_keys,
        )
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ContractError(f"payload manifest is not valid UTF-8 JSON: {error}") from error
    if not isinstance(parsed, dict):
        raise ContractError("payload manifest root must be an object")
    return parsed


def verify_manifest(
    root: Path,
    expected_version: str | None = None,
    expected_commit: str | None = None,
    layout: str = "exact",
) -> dict[str, Any]:
    root = _root_directory(root)
    if (
        expected_version is not None
        and (
            not isinstance(expected_version, str)
            or not _VERSION_PATTERN.fullmatch(expected_version)
        )
    ):
        raise ContractError("expected version does not satisfy the package identity contract")
    if expected_commit is not None:
        if not isinstance(expected_commit, str):
            raise ContractError("expected commit must be an exact 40-character hexadecimal SHA")
        expected_commit = expected_commit.lower()
        if not _COMMIT_PATTERN.fullmatch(expected_commit):
            raise ContractError("expected commit must be an exact 40-character hexadecimal SHA")
    manifest = _read_manifest(root)
    expected_keys = {
        "format",
        "formatVersion",
        "product",
        "platform",
        "version",
        "commit",
        "safetyBoundary",
        "files",
    }
    if set(manifest) != expected_keys:
        unknown = sorted(set(manifest) - expected_keys)
        missing = sorted(expected_keys - set(manifest))
        raise ContractError(
            "payload manifest fields do not match the contract; "
            f"unknown={unknown}, missing={missing}"
        )
    if (
        manifest["format"] != FORMAT
        or type(manifest["formatVersion"]) is not int
        or manifest["formatVersion"] != FORMAT_VERSION
    ):
        raise ContractError("payload manifest format or version is unsupported")
    if manifest["product"] != "EmberLights" or manifest["platform"] != "windows-x64":
        raise ContractError("payload manifest product or platform is incorrect")
    version, commit = _validate_identity(manifest["version"], manifest["commit"])
    if expected_version is not None and version != expected_version:
        raise ContractError(
            f"payload version {version!r} does not match expected {expected_version!r}"
        )
    if expected_commit is not None and commit != expected_commit:
        raise ContractError(
            f"payload commit {commit!r} does not match expected {expected_commit!r}"
        )
    if manifest["safetyBoundary"] != SAFETY_BOUNDARY:
        raise ContractError("payload safety boundary is missing or has changed")
    entries = manifest["files"]
    if not isinstance(entries, list) or not entries or len(entries) > MAX_FILES:
        raise ContractError("payload manifest file table is empty or exceeds its limit")

    listed: dict[str, dict[str, Any]] = {}
    casefolded: dict[str, str] = {}
    for entry in entries:
        if not isinstance(entry, dict) or set(entry) != {"path", "size", "sha256"}:
            raise ContractError("payload manifest file entries must contain path, size, and sha256")
        relative = _validate_relative_path(entry["path"])
        folded = relative.casefold()
        if relative in listed or folded in casefolded:
            raise ContractError(f"payload manifest contains duplicate Windows path {relative!r}")
        size = entry["size"]
        sha256 = entry["sha256"]
        if isinstance(size, bool) or not isinstance(size, int) or size < 0:
            raise ContractError(f"payload size is invalid for {relative!r}")
        if not isinstance(sha256, str) or not _SHA256_PATTERN.fullmatch(sha256):
            raise ContractError(f"payload SHA-256 is invalid for {relative!r}")
        listed[relative] = entry
        casefolded[folded] = relative

    _assert_required(set(listed))
    actual = {relative: candidate for relative, candidate in _payload_files(root)}
    missing_files = sorted(set(listed) - set(actual))
    extras = sorted(set(actual) - set(listed))
    if layout == "inno-installed":
        extras = [path for path in extras if not _INNO_METADATA_PATTERN.fullmatch(path)]
    elif layout != "exact":
        raise ContractError(f"unsupported verification layout: {layout}")
    if missing_files or extras:
        raise ContractError(
            f"payload tree differs from manifest; missing={missing_files}, unexpected={extras}"
        )
    for relative, entry in listed.items():
        candidate = actual[relative]
        actual_size = candidate.stat().st_size
        if actual_size != entry["size"]:
            raise ContractError(
                f"payload size mismatch for {relative!r}: {actual_size} != {entry['size']}"
            )
        actual_sha256 = _sha256(candidate)
        if actual_sha256 != entry["sha256"]:
            raise ContractError(f"payload SHA-256 mismatch for {relative!r}")
    return manifest


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    create = subparsers.add_parser("create", help="create a manifest in a staged payload")
    create.add_argument("--root", type=Path, required=True)
    create.add_argument("--version", required=True)
    create.add_argument("--commit", required=True)
    verify = subparsers.add_parser("verify", help="verify a staged, portable, or installed payload")
    verify.add_argument("--root", type=Path, required=True)
    verify.add_argument("--expected-version")
    verify.add_argument("--expected-commit")
    verify.add_argument("--layout", choices=("exact", "inno-installed"), default="exact")
    return parser


def main(argv: list[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        if arguments.command == "create":
            manifest = create_manifest(arguments.root, arguments.version, arguments.commit)
        else:
            manifest = verify_manifest(
                arguments.root,
                arguments.expected_version,
                arguments.expected_commit,
                arguments.layout,
            )
        manifest_path = arguments.root / MANIFEST_NAME
        print(
            f"Windows payload {arguments.command} passed: "
            f"version={manifest['version']} commit={manifest['commit']} "
            f"files={len(manifest['files'])} manifestSha256={_sha256(manifest_path)}"
        )
        return 0
    except (ContractError, OSError, TypeError) as error:
        print(f"Windows payload {arguments.command} failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
