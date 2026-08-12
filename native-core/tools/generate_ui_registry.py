#!/usr/bin/env python3
"""Deterministic host-side generator for the EmberLights UI registry.

The source fragments and this tool are Studio/build inputs. Generated compact
C++ is consumed by native code; no source JSON parser is linked into Runner.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = ROOT / "spec/ui/registry/source"
MANIFEST_PATH = SOURCE_ROOT / "registry-set.json"
HEADER_PATH = ROOT / "native-core/include/emberlights/generated/ui_registry.generated.hpp"
CATALOG_PATH = ROOT / "spec/ui/registry/generated/ui-registry.catalog.json"
REFERENCE_PATH = ROOT / "docs/generated/ui-registry/REFERENCE.md"
CROSS_REFERENCE_PATH = ROOT / "spec/ui/registry/generated/surface-cross-reference-report.json"
GENERATOR_ID = "emberlights-ui-registry-generator/1.0.0"
ID_PATTERN = re.compile(r"^[a-z][a-zA-Z0-9]*(?:\.[a-zA-Z0-9_\[\]-]+)+$")
TOKEN_PATTERN = re.compile(r"^[a-z][a-zA-Z0-9]*$")


class RegistryError(RuntimeError):
    pass


def load_json(path: Path) -> Any:
    try:
        with path.open("r", encoding="utf-8") as stream:
            return json.load(stream)
    except (OSError, json.JSONDecodeError) as exc:
        raise RegistryError(f"{path.relative_to(ROOT)}: {exc}") from exc


def canonical_json(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def pretty_json(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        indent=2,
    ) + "\n"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RegistryError(message)


def unique_by_id(kind: str, definitions: list[dict[str, Any]]) -> None:
    seen: set[str] = set()
    for definition in definitions:
        identifier = definition.get("id")
        require(isinstance(identifier, str), f"{kind}: definition is missing string id")
        pattern = TOKEN_PATTERN if kind in {"results", "interactions"} else ID_PATTERN
        require(pattern.fullmatch(identifier) is not None,
                f"{kind}: non-canonical id {identifier!r}")
        require(identifier not in seen, f"{kind}: duplicate id {identifier}")
        seen.add(identifier)


def load_registry() -> tuple[dict[str, Any], dict[str, list[dict[str, Any]]], str]:
    manifest = load_json(MANIFEST_PATH)
    require(manifest.get("schemaVersion") == 1, "registry-set schemaVersion must be 1")
    require(manifest.get("generatorVersion") == "1.0.0",
            "registry-set generatorVersion does not match this generator")
    fragments = manifest.get("fragments")
    require(isinstance(fragments, dict), "registry-set fragments must be an object")

    collections: dict[str, list[dict[str, Any]]] = {}
    digest_inputs: list[tuple[str, Any]] = [("registry-set.json", manifest)]
    singular = {
        "commands": "command",
        "states": "state",
        "results": "result",
        "interactions": "interaction",
        "components": "component",
        "capabilities": "capability",
        "values": "value",
    }
    for kind in sorted(fragments):
        require(kind in singular, f"registry-set contains unsupported fragment kind {kind}")
        paths = fragments[kind]
        require(isinstance(paths, list) and paths, f"{kind}: fragment list must be non-empty")
        merged: list[dict[str, Any]] = []
        for relative in sorted(paths):
            require(isinstance(relative, str), f"{kind}: fragment path must be a string")
            path = (SOURCE_ROOT / relative).resolve()
            require(path.is_relative_to(SOURCE_ROOT.resolve()),
                    f"{kind}: fragment escapes source root: {relative}")
            fragment = load_json(path)
            require(fragment.get("schemaVersion") == 1,
                    f"{relative}: schemaVersion must be 1")
            expected_kind = singular[kind] + "Fragment"
            require(fragment.get("kind") == expected_kind,
                    f"{relative}: expected kind {expected_kind}")
            values = fragment.get(kind)
            require(isinstance(values, list), f"{relative}: {kind} must be an array")
            defaults = fragment.get("defaults", {})
            require(isinstance(defaults, dict), f"{relative}: defaults must be an object")
            for value in values:
                require(isinstance(value, dict), f"{relative}: definitions must be objects")
                merged.append({**defaults, **value})
            digest_inputs.append((relative, fragment))
        unique_by_id(kind, merged)
        collections[kind] = sorted(merged, key=lambda item: item["id"])

    digest = hashlib.sha256()
    for relative, value in sorted(digest_inputs):
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(canonical_json(value).encode("utf-8"))
        digest.update(b"\n")
    validate_registry(manifest, collections)
    return manifest, collections, digest.hexdigest()


def validate_native_ordinals(
    kind: str,
    definitions: list[dict[str, Any]],
    integrated: bool,
) -> None:
    native = [item for item in definitions if "nativeName" in item]
    names: set[str] = set()
    ordinals: set[int] = set()
    for definition in native:
        name = definition["nativeName"]
        ordinal = definition.get("nativeOrdinal")
        require(isinstance(name, str) and re.fullmatch(r"[A-Z][A-Za-z0-9]*", name),
                f"{kind} {definition['id']}: invalid nativeName")
        require(isinstance(ordinal, int) and 0 <= ordinal <= 255,
                f"{kind} {definition['id']}: invalid nativeOrdinal")
        require(name not in names, f"{kind}: duplicate nativeName {name}")
        require(ordinal not in ordinals, f"{kind}: duplicate nativeOrdinal {ordinal}")
        names.add(name)
        ordinals.add(ordinal)
    if integrated:
        require(ordinals == set(range(len(native))),
                f"{kind}: integrated native ordinals must be contiguous 0..{len(native) - 1}")


def validate_replacement_graph(kind: str, definitions: list[dict[str, Any]]) -> None:
    by_id = {item["id"]: item for item in definitions}
    edges: dict[str, str] = {}
    for definition in definitions:
        if definition.get("deprecated"):
            replacement = definition.get("replacement")
            require(isinstance(replacement, str),
                    f"{kind} {definition['id']}: deprecated definition needs replacement")
            require(replacement in by_id,
                    f"{kind} {definition['id']}: unknown replacement {replacement}")
            require(replacement != definition["id"],
                    f"{kind} {definition['id']}: replacement cannot be itself")
            edges[definition["id"]] = replacement
    for start in edges:
        seen: set[str] = set()
        current = start
        while current in edges:
            require(current not in seen, f"{kind}: replacement cycle starting at {start}")
            seen.add(current)
            current = edges[current]


def validate_registry(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
) -> None:
    integrated = manifest.get("nativeMode") == "integrated"
    commands = collections.get("commands", [])
    states = collections.get("states", [])
    results = collections.get("results", [])
    interactions = collections.get("interactions", [])
    require(commands, "command registry cannot be empty")
    require(states, "state registry cannot be empty")
    require(results, "result registry cannot be empty")
    require(interactions, "interaction registry cannot be empty")

    validate_native_ordinals("commands", commands, integrated)
    validate_native_ordinals("results", results, False)
    validate_native_ordinals("interactions", interactions, False)
    if integrated:
        state_ordinals = [item.get("nativeOrdinal") for item in states]
        require(all(isinstance(item, int) for item in state_ordinals),
                "states: integrated definitions require nativeOrdinal")
        require(set(state_ordinals) == set(range(len(states))),
                f"states: integrated native ordinals must be contiguous 0..{len(states) - 1}")
    validate_replacement_graph("commands", commands)
    validate_replacement_graph("states", states)

    state_ids = {item["id"] for item in states}
    interaction_ids = {item["id"] for item in interactions}
    for command in commands:
        require(command.get("status") in {"implemented", "bridged", "planned", "deprecated"},
                f"command {command['id']}: invalid status")
        require(command.get("interaction") in interaction_ids,
                f"command {command['id']}: unknown interaction {command.get('interaction')}")
        require(command.get("realtimeClass") in {
            "viewLocal", "studioMutation", "runnerCommand", "runnerPriority",
            "utilityAsync", "blockingForbiddenLive"
        }, f"command {command['id']}: invalid realtimeClass")
        require(command.get("persistenceScope") in {
            "none", "appLocal", "projectAuthored", "liveTransient"
        }, f"command {command['id']}: invalid persistenceScope")
        for state_id in command.get("feedback", []):
            require(state_id in state_ids,
                    f"command {command['id']}: unknown feedback state {state_id}")

    for state in states:
        require(state.get("status") in {"implemented", "bridged", "planned", "deprecated"},
                f"state {state['id']}: invalid status")
        require(state.get("nativeUpdateClass") in {"Event", "Health", "Realtime"},
                f"state {state['id']}: invalid nativeUpdateClass")
        if state.get("updateClass") in {"slow", "health", "transport", "progress"}:
            rate = state.get("maximumPublishHz")
            require(isinstance(rate, (int, float)) and 0 < rate <= 1000,
                    f"state {state['id']}: bounded maximumPublishHz is required")
        if state.get("authority") == "runnerScheduler":
            require(state.get("persisted") is False,
                    f"state {state['id']}: scheduler state cannot be persisted")


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def enum_lines(definitions: Iterable[dict[str, Any]], indent: str = "    ") -> list[str]:
    return [
        f"{indent}{item['nativeName']} = {item['nativeOrdinal']},"
        for item in sorted(
            (entry for entry in definitions if "nativeName" in entry),
            key=lambda entry: entry["nativeOrdinal"],
        )
    ]


def generate_header(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
    digest: str,
) -> str:
    integrated = manifest.get("nativeMode") == "integrated"
    namespace = "emberlights" if integrated else "emberlights::registry_spike"
    commands = sorted(
        (item for item in collections["commands"] if "nativeName" in item),
        key=lambda item: item["nativeOrdinal"],
    )
    states = sorted(
        collections["states"],
        key=(lambda item: item["nativeOrdinal"]) if integrated else (lambda item: item["id"]),
    )
    results = collections["results"]
    interactions = collections["interactions"]
    lines = [
        "// Generated by native-core/tools/generate_ui_registry.py; DO NOT EDIT.",
        f"// Registry generation: {manifest['registryGeneration']}",
        f"// Canonical source SHA-256: {digest}",
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        f"namespace {namespace} {{",
        "",
        f"inline constexpr std::uint32_t kUiRegistryGeneration = {manifest['registryGeneration']}U;",
        f"inline constexpr std::string_view kUiRegistryDigest = {cpp_string(digest)};",
        "",
        "enum class UiCommandId : std::uint8_t {",
        *enum_lines(commands),
    ]
    if integrated:
        lines.append(f"    Count = {len(commands)},")
    lines.extend([
        "};",
        "",
        "enum class UiInvocationResult : std::uint8_t {",
        *enum_lines(results),
        "};",
        "",
        "enum class UiCommandInteraction : std::uint8_t {",
        *enum_lines(interactions),
        "};",
        "",
        "struct UiCommandDefinition {",
        f"    UiCommandId command{{UiCommandId::{commands[0]['nativeName']}}};",
        "    std::string_view id{};",
        "    std::string_view label{};",
        "    UiCommandInteraction interaction{UiCommandInteraction::Trigger};",
        "    bool emergency{false};",
        "    bool midi_bindable{true};",
        "    bool keyboard_bindable{true};",
        "};",
        "",
        f"inline constexpr std::array<UiCommandDefinition, {len(commands)}> kUiCommandDefinitions{{{{",
    ])
    interaction_by_id = {item["id"]: item["nativeName"] for item in interactions}
    for command in commands:
        emergency = "true" if command.get("safetyClass") == "emergency" else "false"
        midi = "true" if command.get("midiBindable", True) else "false"
        keyboard = "true" if command.get("keyboardBindable", True) else "false"
        lines.append(
            "    {UiCommandId::%s, %s, %s, UiCommandInteraction::%s, %s, %s, %s},"
            % (
                command["nativeName"], cpp_string(command["id"]),
                cpp_string(command["label"]), interaction_by_id[command["interaction"]],
                emergency, midi, keyboard,
            )
        )
    lines.extend([
        "}};",
        "",
        "enum class UiStateUpdateClass : std::uint8_t {",
        "    Event,",
        "    Health,",
        "    Realtime,",
        "};",
        "",
        "struct UiStateDefinition {",
        "    std::string_view id{};",
        "    UiStateUpdateClass update_class{UiStateUpdateClass::Event};",
        "};",
        "",
        f"inline constexpr std::array<UiStateDefinition, {len(states)}> kLiveCoreUiStates{{{{",
    ])
    for state in states:
        lines.append(
            f"    {{{cpp_string(state['id'])}, UiStateUpdateClass::{state['nativeUpdateClass']}}},"
        )
    lines.extend([
        "}};",
        "",
        f"}}  // namespace {namespace}",
        "",
    ])
    return "\n".join(lines)


def generate_catalog(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
    digest: str,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "schemaVersion": 1,
        "registrySetVersion": manifest["registrySetVersion"],
        "registryGeneration": manifest["registryGeneration"],
        "generator": GENERATOR_ID,
        "sourceDigest": digest,
        "status": manifest["status"],
        "ownerReservations": manifest.get("ownerReservations", []),
    }
    for kind in sorted(collections):
        result[kind] = collections[kind]
    return result


def validate_cross_reference_artifact(
    artifact: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
    artifact_name: str,
) -> dict[str, Any]:
    references = artifact.get("references")
    require(isinstance(references, dict), f"{artifact_name}: references must be an object")
    result: dict[str, Any] = {"artifact": artifact_name, "resolved": {}, "status": "valid"}
    for kind in ("commands", "states", "components", "capabilities"):
        requested = references.get(kind, [])
        require(isinstance(requested, list) and all(isinstance(item, str) for item in requested),
                f"{artifact_name}: references.{kind} must be a string array")
        known = {item["id"] for item in collections.get(kind, [])}
        missing = sorted(set(requested) - known)
        require(not missing, f"{artifact_name}: unknown {kind}: {', '.join(missing)}")
        result["resolved"][kind] = sorted(set(requested))
    return result


def generate_cross_reference_report(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
    digest: str,
) -> dict[str, Any]:
    reports: list[dict[str, Any]] = []
    for relative in manifest.get("crossReferenceArtifacts", []):
        require(isinstance(relative, str), "crossReferenceArtifacts entries must be strings")
        path = (ROOT / relative).resolve()
        require(path.is_relative_to(ROOT.resolve()),
                f"cross-reference artifact escapes repository: {relative}")
        reports.append(validate_cross_reference_artifact(load_json(path), collections, relative))
    return {
        "schemaVersion": 1,
        "registryGeneration": manifest["registryGeneration"],
        "sourceDigest": digest,
        "scope": "bounded-native-registry-spike",
        "status": "valid",
        "artifacts": reports,
        "deferredPlanningArtifacts": {
            "status": "notValidatedByFirstSlice",
            "reason": "Bundled layout and inactive capture-required profile references include planned commands/states owned by later #31/#32/#59 work; SKIN2-001 does not publish them as callable to make fixtures pass."
        }
    }


def markdown_table(headers: list[str], rows: Iterable[list[str]]) -> list[str]:
    result = ["| " + " | ".join(headers) + " |", "| " + " | ".join("---" for _ in headers) + " |"]
    result.extend("| " + " | ".join(cell.replace("|", "\\|") for cell in row) + " |" for row in rows)
    return result


def generate_reference(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
    digest: str,
) -> str:
    lines = [
        "# Generated UI Registry Reference",
        "",
        "> Generated by `native-core/tools/generate_ui_registry.py`; do not edit.",
        "",
        f"- Registry set: `{manifest['registrySetVersion']}`",
        f"- Generation: `{manifest['registryGeneration']}`",
        f"- Source digest: `{digest}`",
        f"- Native mode: `{manifest['nativeMode']}`",
        "",
        "## Commands",
        "",
        *markdown_table(
            ["ID", "Status", "Interaction", "Realtime", "Safety"],
            ([item["id"], item["status"], item["interaction"], item["realtimeClass"], item["safetyClass"]]
             for item in collections["commands"]),
        ),
        "",
        "## States",
        "",
        *markdown_table(
            ["ID", "Status", "Type", "Update", "Authority"],
            ([item["id"], item["status"], item["value"]["type"], item["updateClass"], item["authority"]]
             for item in collections["states"]),
        ),
        "",
        "## Invocation results",
        "",
        *markdown_table(
            ["ID", "Status", "Native"],
            ([item["id"], item["status"], item.get("nativeName", "—")]
             for item in collections["results"]),
        ),
        "",
        "## Owner reservations",
        "",
    ]
    for reservation in manifest.get("ownerReservations", []):
        lines.append(
            f"- `{reservation['namespace']}` issue #{reservation['issue']}: {reservation['rule']}"
        )
    return "\n".join(lines) + "\n"


def output_map() -> dict[Path, str]:
    manifest, collections, digest = load_registry()
    catalog = generate_catalog(manifest, collections, digest)
    return {
        HEADER_PATH: generate_header(manifest, collections, digest),
        CATALOG_PATH: pretty_json(catalog),
        CROSS_REFERENCE_PATH: pretty_json(
            generate_cross_reference_report(manifest, collections, digest)
        ),
        REFERENCE_PATH: generate_reference(manifest, collections, digest),
    }


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
    ) as stream:
        stream.write(content)
        temporary = Path(stream.name)
    os.replace(temporary, path)


def display_path(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def generate(check: bool) -> int:
    outputs = output_map()
    stale: list[str] = []
    for path, content in outputs.items():
        if path.exists() and path.read_text(encoding="utf-8") == content:
            continue
        stale.append(display_path(path))
        if not check:
            atomic_write(path, content)
    if check and stale:
        print("stale generated UI registry artifacts:", file=sys.stderr)
        for path in stale:
            print(f"  {path}", file=sys.stderr)
        return 1
    manifest, collections, digest = load_registry()
    verb = "checked" if check else "generated"
    print(
        f"UI registry {verb}: generation={manifest['registryGeneration']} "
        f"commands={len(collections['commands'])} states={len(collections['states'])} "
        f"digest={digest}"
    )
    return 0


def compatibility_key(kind: str, definition: dict[str, Any]) -> dict[str, Any]:
    ignored = {
        "label", "description", "notes", "status", "introducedGeneration",
        "deprecated", "replacement", "replacementCompatibility",
    }
    return {key: value for key, value in definition.items() if key not in ignored}


def registry_diff(baseline: dict[str, Any], candidate: dict[str, Any]) -> dict[str, Any]:
    report: dict[str, Any] = {
        "schemaVersion": 1,
        "baselineDigest": baseline.get("sourceDigest"),
        "candidateDigest": candidate.get("sourceDigest"),
        "classification": "unchanged",
        "registries": {},
        "summary": {
            "added": 0,
            "changedCompatible": 0,
            "deprecated": 0,
            "replacementAvailable": 0,
            "changedBreaking": 0,
            "removed": 0,
            "unchanged": 0,
        },
        "migrationAvailable": False,
        "manualActionRequired": False,
    }
    for kind in ("commands", "states", "components", "capabilities", "results", "interactions"):
        before = {item["id"]: item for item in baseline.get(kind, [])}
        after = {item["id"]: item for item in candidate.get(kind, [])}
        detail = {key: [] for key in (
            "added", "changedCompatible", "deprecated", "replacementAvailable",
            "changedBreaking", "removed", "unchanged"
        )}
        for identifier in sorted(before.keys() | after.keys()):
            if identifier not in before:
                detail["added"].append(identifier)
            elif identifier not in after:
                detail["removed"].append(identifier)
            elif before[identifier] == after[identifier]:
                detail["unchanged"].append(identifier)
            elif after[identifier].get("deprecated") and not before[identifier].get("deprecated"):
                detail["deprecated"].append(identifier)
                replacement = after[identifier].get("replacement")
                if replacement and replacement in after:
                    detail["replacementAvailable"].append(identifier)
            elif compatibility_key(kind, before[identifier]) == compatibility_key(kind, after[identifier]):
                detail["changedCompatible"].append(identifier)
            else:
                detail["changedBreaking"].append(identifier)
        report["registries"][kind] = detail
        for key, values in detail.items():
            report["summary"][key] += len(values)
    if report["summary"]["removed"] or report["summary"]["changedBreaking"]:
        report["classification"] = "breaking"
        report["manualActionRequired"] = True
    elif report["summary"]["deprecated"]:
        report["classification"] = "compatibleWithDeprecations"
        report["migrationAvailable"] = bool(report["summary"]["replacementAvailable"])
    elif report["summary"]["added"] or report["summary"]["changedCompatible"]:
        report["classification"] = "compatibleAdditive"
    return report


def diff_command(args: argparse.Namespace) -> int:
    baseline = load_json(Path(args.baseline))
    candidate = load_json(Path(args.candidate))
    report = registry_diff(baseline, candidate)
    content = pretty_json(report)
    if args.output:
        atomic_write(Path(args.output), content)
    else:
        sys.stdout.write(content)
    if args.expect and report["classification"] != args.expect:
        print(
            f"expected {args.expect}, got {report['classification']}",
            file=sys.stderr,
        )
        return 1
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("generate", help="generate committed artifacts")
    subparsers.add_parser("check", help="fail if generated artifacts are stale")
    subparsers.add_parser("validate", help="validate canonical source only")
    diff_parser = subparsers.add_parser("diff", help="classify two generated catalogs")
    diff_parser.add_argument("--baseline", required=True)
    diff_parser.add_argument("--candidate", required=True)
    diff_parser.add_argument("--output")
    diff_parser.add_argument(
        "--expect",
        choices=("unchanged", "compatibleAdditive", "compatibleWithDeprecations", "breaking"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "generate":
            return generate(False)
        if args.command == "check":
            return generate(True)
        if args.command == "validate":
            manifest, collections, digest = load_registry()
            print(
                f"UI registry valid: generation={manifest['registryGeneration']} "
                f"commands={len(collections['commands'])} states={len(collections['states'])} "
                f"digest={digest}"
            )
            return 0
        return diff_command(args)
    except RegistryError as exc:
        print(f"UI registry error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
