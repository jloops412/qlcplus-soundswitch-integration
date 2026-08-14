#!/usr/bin/env python3
"""Deterministic host-side generator for the EmberLights UI registry.

The source fragments and this tool are Studio/build inputs. Generated compact
C++ is consumed by native code; no source JSON parser is linked into Runner.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
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
GENERATOR_VERSION = "1.2.0"
GENERATOR_ID = f"emberlights-ui-registry-generator/{GENERATOR_VERSION}"
ID_PATTERN = re.compile(r"^[a-z][a-zA-Z0-9]*(?:\.[a-zA-Z0-9_\[\]-]+)+$")
TOKEN_PATTERN = re.compile(r"^[a-z][a-zA-Z0-9]*$")
LIFECYCLE_STATUSES = {"implemented", "bridged", "planned", "deprecated"}
VALUE_TYPES = {"boolean", "enum", "id", "integer", "number", "string"}


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
    require(manifest.get("generatorVersion") == GENERATOR_VERSION,
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
            compatibility = definition.get("replacementCompatibility")
            require(compatibility in {"exact", "lossless", "manual"},
                    f"{kind} {definition['id']}: invalid replacementCompatibility")
            edges[definition["id"]] = replacement
    for start in edges:
        seen: set[str] = set()
        current = start
        while current in edges:
            require(current not in seen, f"{kind}: replacement cycle starting at {start}")
            seen.add(current)
            current = edges[current]
    ignored = {
        "id", "nativeName", "nativeOrdinal", "label", "description", "notes",
        "status", "introducedGeneration", "deprecated", "replacement",
        "replacementCompatibility", "deprecatedGeneration", "plannedRemovalGeneration",
    }
    for definition in definitions:
        if not definition.get("deprecated") or definition.get("replacementCompatibility") != "exact":
            continue
        replacement = by_id[definition["replacement"]]
        before = {key: value for key, value in definition.items() if key not in ignored}
        after = {key: value for key, value in replacement.items() if key not in ignored}
        require(before == after,
                f"{kind} {definition['id']}: exact replacement changes semantic contract")


def require_string(value: Any, message: str) -> str:
    require(isinstance(value, str) and bool(value), message)
    return value


def require_unique_strings(value: Any, message: str) -> list[str]:
    require(
        isinstance(value, list)
        and all(isinstance(item, str) and bool(item) for item in value)
        and len(value) == len(set(value)),
        message,
    )
    return value


def validate_lifecycle(
    kind: str,
    definition: dict[str, Any],
    generation: int,
) -> None:
    status = definition.get("status")
    require(status in LIFECYCLE_STATUSES,
            f"{kind} {definition['id']}: invalid status {status!r}")
    introduced = definition.get("introducedGeneration")
    require(
        isinstance(introduced, int)
        and not isinstance(introduced, bool)
        and 1 <= introduced <= generation,
        f"{kind} {definition['id']}: invalid introducedGeneration",
    )
    deprecated = definition.get("deprecated")
    require(isinstance(deprecated, bool),
            f"{kind} {definition['id']}: deprecated must be boolean")
    require((status == "deprecated") == deprecated,
            f"{kind} {definition['id']}: status/deprecated mismatch")
    if not deprecated:
        require("replacement" not in definition,
                f"{kind} {definition['id']}: active definition cannot declare replacement")


def validate_value_contract(
    value: Any,
    owner: str,
    values_by_id: dict[str, dict[str, Any]],
    unit_by_token: dict[str, dict[str, Any]],
    target_by_token: dict[str, dict[str, Any]],
) -> None:
    require(isinstance(value, dict), f"{owner}: value contract must be an object")
    value_type = value.get("type")
    require(value_type in VALUE_TYPES, f"{owner}: invalid value type {value_type!r}")
    if "minimum" in value or "maximum" in value:
        require(value_type in {"integer", "number"},
                f"{owner}: only numeric values may declare bounds")
    for field in ("minimum", "maximum"):
        if field in value:
            bound = value[field]
            require(
                isinstance(bound, (int, float))
                and not isinstance(bound, bool)
                and math.isfinite(float(bound)),
                f"{owner}: {field} must be finite",
            )
    if "minimum" in value and "maximum" in value:
        require(value["minimum"] <= value["maximum"],
                f"{owner}: minimum exceeds maximum")
    if "maxLength" in value:
        maximum_length = value["maxLength"]
        require(isinstance(maximum_length, int) and not isinstance(maximum_length, bool)
                and maximum_length > 0,
                f"{owner}: maxLength must be a positive integer")
    enum_values = value.get("enumValues")
    schema_ref = value.get("schemaRef")
    if enum_values is not None:
        require(value_type == "enum", f"{owner}: enumValues require enum type")
        require_unique_strings(enum_values, f"{owner}: enumValues must be unique strings")
        require(bool(enum_values), f"{owner}: enumValues cannot be empty")
    if schema_ref is not None:
        require(isinstance(schema_ref, str), f"{owner}: schemaRef must be a string")
        require(schema_ref in values_by_id, f"{owner}: unknown schemaRef {schema_ref}")
        if value_type == "enum":
            require(values_by_id[schema_ref].get("kind") == "enum",
                    f"{owner}: enum schemaRef {schema_ref} is not an enum")
    if value_type == "enum":
        require(enum_values is not None or schema_ref is not None,
                f"{owner}: enum needs enumValues or schemaRef")
    unit = value.get("unit")
    if unit is not None:
        require(value_type in {"integer", "number"},
                f"{owner}: unit is only valid for numeric values")
        require(isinstance(unit, str) and unit in unit_by_token,
                f"{owner}: unknown unit {unit!r}")
    target = value.get("targetKind")
    if target is not None:
        require(value_type == "id", f"{owner}: targetKind requires id type")
        require(isinstance(target, str) and target in target_by_token,
                f"{owner}: unknown targetKind {target!r}")
    if value_type == "id":
        require(isinstance(target, str), f"{owner}: id requires targetKind")


def validate_value_definitions(
    values: list[dict[str, Any]],
    generation: int,
) -> tuple[
    dict[str, dict[str, Any]],
    dict[str, dict[str, Any]],
    dict[str, dict[str, Any]],
]:
    values_by_id = {item["id"]: item for item in values}
    unit_by_token: dict[str, dict[str, Any]] = {}
    target_by_token: dict[str, dict[str, Any]] = {}
    for value in values:
        validate_lifecycle("value", value, generation)
        require(value.get("callable") is False,
                f"value {value['id']}: reusable values are non-callable")
        require_string(value.get("label"), f"value {value['id']}: label is required")
        require_string(value.get("description"),
                       f"value {value['id']}: description is required")
        kind = value.get("kind")
        require(kind in {"enum", "target", "unit"},
                f"value {value['id']}: invalid kind {kind!r}")
        if kind == "enum":
            require(value.get("valueType") == "enum",
                    f"value {value['id']}: enum valueType must be enum")
            entries = require_unique_strings(
                value.get("enumValues"), f"value {value['id']}: enumValues must be unique strings"
            )
            require(bool(entries), f"value {value['id']}: enumValues cannot be empty")
            require(value.get("unknownValueBehavior") in {"preserve", "reject"},
                    f"value {value['id']}: invalid unknownValueBehavior")
        else:
            token = value.get("token")
            require(isinstance(token, str) and TOKEN_PATTERN.fullmatch(token) is not None,
                    f"value {value['id']}: invalid token {token!r}")
            index = unit_by_token if kind == "unit" else target_by_token
            require(token not in index, f"values: duplicate {kind} token {token}")
            index[token] = value
            expected_type = "number" if kind == "unit" else "id"
            require(value.get("valueType") == expected_type,
                    f"value {value['id']}: {kind} valueType must be {expected_type}")
            if kind == "target":
                require(value.get("missingBehavior") in {"conflicted", "unavailable"},
                        f"value {value['id']}: invalid missingBehavior")
            for field in ("minimum", "maximum"):
                if field in value:
                    bound = value[field]
                    require(isinstance(bound, (int, float)) and not isinstance(bound, bool)
                            and math.isfinite(float(bound)),
                            f"value {value['id']}: {field} must be finite")
            if "minimum" in value and "maximum" in value:
                require(value["minimum"] <= value["maximum"],
                        f"value {value['id']}: minimum exceeds maximum")
    return values_by_id, unit_by_token, target_by_token


def validate_reference_list(
    owner: str,
    field: str,
    definition: dict[str, Any],
    known: set[str],
) -> None:
    references = require_unique_strings(
        definition.get(field, []), f"{owner}: {field} must be unique strings"
    )
    missing = sorted(set(references) - known)
    require(not missing, f"{owner}: unknown {field}: {', '.join(missing)}")


def validate_capability_graph(capabilities: list[dict[str, Any]]) -> None:
    edges = {item["id"]: item.get("dependencies", []) for item in capabilities}
    for start in edges:
        active: set[str] = set()
        complete: set[str] = set()

        def visit(current: str) -> None:
            require(current not in active,
                    f"capabilities: dependency cycle starting at {start}")
            if current in complete:
                return
            active.add(current)
            for dependency in edges[current]:
                visit(dependency)
            active.remove(current)
            complete.add(current)

        visit(start)


def validate_registry(
    manifest: dict[str, Any],
    collections: dict[str, list[dict[str, Any]]],
) -> None:
    integrated = manifest.get("nativeMode") == "integrated"
    generation = manifest.get("registryGeneration")
    require(isinstance(generation, int) and not isinstance(generation, bool) and generation > 0,
            "registryGeneration must be a positive integer")
    commands = collections.get("commands", [])
    states = collections.get("states", [])
    results = collections.get("results", [])
    interactions = collections.get("interactions", [])
    components = collections.get("components", [])
    capabilities = collections.get("capabilities", [])
    values = collections.get("values", [])
    require(commands, "command registry cannot be empty")
    require(states, "state registry cannot be empty")
    require(results, "result registry cannot be empty")
    require(interactions, "interaction registry cannot be empty")
    require(components, "component registry cannot be empty")
    require(capabilities, "capability registry cannot be empty")
    require(values, "value registry cannot be empty")

    validate_native_ordinals("commands", commands, integrated)
    validate_native_ordinals("results", results, False)
    validate_native_ordinals("interactions", interactions, False)
    if integrated:
        state_ordinals = [item.get("nativeOrdinal") for item in states]
        require(all(isinstance(item, int) for item in state_ordinals),
                "states: integrated definitions require nativeOrdinal")
        require(set(state_ordinals) == set(range(len(states))),
                f"states: integrated native ordinals must be contiguous 0..{len(states) - 1}")
    for kind in ("commands", "states", "components", "capabilities", "values", "results"):
        validate_replacement_graph(kind, collections[kind])

    command_ids = {item["id"] for item in commands}
    state_ids = {item["id"] for item in states}
    component_ids = {item["id"] for item in components}
    capability_ids = {item["id"] for item in capabilities}
    interaction_ids = {item["id"] for item in interactions}
    values_by_id, unit_by_token, target_by_token = validate_value_definitions(
        values, generation
    )

    for interaction in interactions:
        require(interaction.get("status") == "implemented",
                f"interaction {interaction['id']}: unsupported status")
        require("nativeName" in interaction and "nativeOrdinal" in interaction,
                f"interaction {interaction['id']}: native metadata is required")

    for result in results:
        status = result.get("status")
        require(status in {"implemented", "reserved", "deprecated"},
                f"result {result['id']}: invalid status {status!r}")
        introduced = result.get("introducedGeneration")
        require(isinstance(introduced, int) and not isinstance(introduced, bool)
                and 1 <= introduced <= generation,
                f"result {result['id']}: invalid introducedGeneration")
        require_string(result.get("label"), f"result {result['id']}: label is required")
        require_string(result.get("description"),
                       f"result {result['id']}: description is required")
        require(isinstance(result.get("terminal"), bool),
                f"result {result['id']}: terminal must be boolean")
        if status == "implemented":
            require("nativeName" in result and "nativeOrdinal" in result,
                    f"result {result['id']}: implemented result needs native metadata")
        if status == "reserved":
            require("nativeName" not in result and "nativeOrdinal" not in result,
                    f"result {result['id']}: reserved result cannot occupy native ABI")
            owners = result.get("ownerIssues")
            require(isinstance(owners, list) and bool(owners)
                    and all(isinstance(item, int) and item > 0 for item in owners),
                    f"result {result['id']}: reserved result needs ownerIssues")
        payload = result.get("payloadSchema")
        if payload is not None:
            require(isinstance(payload, dict),
                    f"result {result['id']}: payloadSchema must be an object")
            if payload == {"operationId": "boundedString"}:
                continue
            require(payload.get("type") == "object",
                    f"result {result['id']}: payloadSchema must be an object schema")
            require(payload.get("additionalProperties") is False,
                    f"result {result['id']}: payloadSchema must fail closed")
            properties = payload.get("properties")
            required = payload.get("required")
            require(isinstance(properties, dict) and isinstance(required, list),
                    f"result {result['id']}: payloadSchema properties/required are needed")
            require(set(required) <= set(properties),
                    f"result {result['id']}: payloadSchema has unknown required field")
            for name, definition in properties.items():
                validate_value_contract(
                    definition,
                    f"result {result['id']} payload {name}",
                    values_by_id,
                    unit_by_token,
                    target_by_token,
                )

    for command in commands:
        validate_lifecycle("command", command, generation)
        for field in ("midiBindable", "keyboardBindable", "actionBindable"):
            if field in command:
                require(isinstance(command[field], bool),
                        f"command {command['id']}: {field} must be boolean")
        require(command.get("interaction") in interaction_ids,
                f"command {command['id']}: unknown interaction {command.get('interaction')}")
        require(command.get("realtimeClass") in {
            "viewLocal", "studioMutation", "runnerCommand", "runnerPriority",
            "utilityAsync", "blockingForbiddenLive"
        }, f"command {command['id']}: invalid realtimeClass")
        require(command.get("persistenceScope") in {
            "none", "appLocal", "projectAuthored", "liveTransient"
        }, f"command {command['id']}: invalid persistenceScope")
        require_string(command.get("safetyClass"),
                       f"command {command['id']}: safetyClass is required")
        parameters = command.get("parameters")
        require(isinstance(parameters, dict),
                f"command {command['id']}: parameters must be an object")
        for name, value in parameters.items():
            require(isinstance(name, str) and bool(name),
                    f"command {command['id']}: invalid parameter name")
            validate_value_contract(
                value,
                f"command {command['id']} parameter {name}",
                values_by_id,
                unit_by_token,
                target_by_token,
            )
            require(isinstance(value.get("required"), bool),
                    f"command {command['id']} parameter {name}: required must be boolean")
        for state_id in command.get("feedback", []):
            require(state_id in state_ids,
                    f"command {command['id']}: unknown feedback state {state_id}")
        validate_reference_list(
            f"command {command['id']}", "requiredCapabilities", command, capability_ids
        )

    for state in states:
        validate_lifecycle("state", state, generation)
        validate_value_contract(
            state.get("value"),
            f"state {state['id']}",
            values_by_id,
            unit_by_token,
            target_by_token,
        )
        require(state.get("nativeUpdateClass") in {"Event", "Health", "Realtime"},
                f"state {state['id']}: invalid nativeUpdateClass")
        if state.get("updateClass") in {"slow", "health", "transport", "progress"}:
            rate = state.get("maximumPublishHz")
            require(isinstance(rate, (int, float)) and 0 < rate <= 1000,
                    f"state {state['id']}: bounded maximumPublishHz is required")
        if state.get("authority") == "runnerScheduler":
            require(state.get("persisted") is False,
                    f"state {state['id']}: scheduler state cannot be persisted")

    for component in components:
        validate_lifecycle("component", component, generation)
        require(component.get("callable") is False,
                f"component {component['id']}: component contracts are non-callable")
        require(component.get("classification") in {"nativeComplex", "primitive"},
                f"component {component['id']}: invalid classification")
        require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", component.get("contractVersion", ""))
                is not None,
                f"component {component['id']}: invalid contractVersion")
        require_string(component.get("label"),
                       f"component {component['id']}: label is required")
        require_string(component.get("description"),
                       f"component {component['id']}: description is required")
        workspaces = require_unique_strings(
            component.get("supportedWorkspaces"),
            f"component {component['id']}: supportedWorkspaces must be unique strings",
        )
        require(bool(workspaces) and set(workspaces) <= {"live", "safe", "studio"},
                f"component {component['id']}: invalid supportedWorkspaces")
        variants = require_unique_strings(
            component.get("supportedVariants"),
            f"component {component['id']}: supportedVariants must be unique strings",
        )
        require(bool(variants), f"component {component['id']}: supportedVariants cannot be empty")
        inputs = require_unique_strings(
            component.get("inputModes"),
            f"component {component['id']}: inputModes must be unique strings",
        )
        require(bool(inputs) and set(inputs) <= {"keyboard", "pointer", "touch"},
                f"component {component['id']}: invalid inputModes")
        properties = component.get("properties")
        require(isinstance(properties, dict),
                f"component {component['id']}: properties must be an object")
        for name, value in properties.items():
            validate_value_contract(
                value,
                f"component {component['id']} property {name}",
                values_by_id,
                unit_by_token,
                target_by_token,
            )
        require_unique_strings(component.get("events"),
                               f"component {component['id']}: events must be unique strings")
        require_unique_strings(component.get("slots"),
                               f"component {component['id']}: slots must be unique strings")
        validate_reference_list(
            f"component {component['id']}", "requiredCommands", component, command_ids
        )
        validate_reference_list(
            f"component {component['id']}", "requiredStates", component, state_ids
        )
        validate_reference_list(
            f"component {component['id']}", "requiredCapabilities", component, capability_ids
        )
        validate_reference_list(
            f"component {component['id']}", "optionalCapabilities", component, capability_ids
        )
        require_string(component.get("accessibilityRole"),
                       f"component {component['id']}: accessibilityRole is required")
        require(component.get("performanceClass") in {
            "bounded", "healthBounded", "snapshotBounded", "virtualized"
        }, f"component {component['id']}: invalid performanceClass")
        require(component.get("failureBehavior") in {"placeholder", "safeFallback"},
                f"component {component['id']}: invalid failureBehavior")

    for capability in capabilities:
        validate_lifecycle("capability", capability, generation)
        require(capability.get("callable") is False,
                f"capability {capability['id']}: capabilities are non-callable")
        require_string(capability.get("label"),
                       f"capability {capability['id']}: label is required")
        require_string(capability.get("description"),
                       f"capability {capability['id']}: description is required")
        require_string(capability.get("category"),
                       f"capability {capability['id']}: category is required")
        require(capability.get("origin") in {"adapter", "application", "engine", "platform", "project"},
                f"capability {capability['id']}: invalid origin")
        require(capability.get("absenceBehavior") in {"degrade", "disable", "hide", "reject"},
                f"capability {capability['id']}: invalid absenceBehavior")
        validate_reference_list(
            f"capability {capability['id']}", "dependencies", capability, capability_ids
        )
        validate_reference_list(
            f"capability {capability['id']}", "conflicts", capability, capability_ids
        )
        require(capability["id"] not in capability.get("dependencies", []),
                f"capability {capability['id']}: cannot depend on itself")
        require(capability["id"] not in capability.get("conflicts", []),
                f"capability {capability['id']}: cannot conflict with itself")
        require(not (set(capability.get("dependencies", []))
                     & set(capability.get("conflicts", []))),
                f"capability {capability['id']}: dependency cannot also conflict")
        validate_reference_list(
            f"capability {capability['id']}", "relevantCommands", capability, command_ids
        )
        validate_reference_list(
            f"capability {capability['id']}", "relevantStates", capability, state_ids
        )
        validate_reference_list(
            f"capability {capability['id']}", "relevantComponents", capability, component_ids
        )
        require(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", capability.get("version", ""))
                is not None,
                f"capability {capability['id']}: invalid version")
        require(capability.get("qualificationStatus") in {
            "experimental", "softwareVerified", "unverified"
        }, f"capability {capability['id']}: invalid qualificationStatus")
        require(isinstance(capability.get("experimental"), bool),
                f"capability {capability['id']}: experimental must be boolean")
    validate_capability_graph(capabilities)


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
        "    bool action_bindable{true};",
        "};",
        "",
        f"inline constexpr std::array<UiCommandDefinition, {len(commands)}> kUiCommandDefinitions{{{{",
    ])
    interaction_by_id = {item["id"]: item["nativeName"] for item in interactions}
    for command in commands:
        emergency = "true" if command.get("safetyClass") == "emergency" else "false"
        midi = "true" if command.get("midiBindable", True) else "false"
        keyboard = "true" if command.get("keyboardBindable", True) else "false"
        action = "true" if command.get("actionBindable", True) else "false"
        lines.append(
            "    {UiCommandId::%s, %s, %s, UiCommandInteraction::%s, %s, %s, %s, %s},"
            % (
                command["nativeName"], cpp_string(command["id"]),
                cpp_string(command["label"]), interaction_by_id[command["interaction"]],
                emergency, midi, keyboard, action,
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
    for kind in (
        "commands", "states", "components", "capabilities", "values", "results", "interactions"
    ):
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
        "scope": "canonical-registry-contracts-generation-2",
        "status": "valid",
        "artifacts": reports,
        "deferredPlanningArtifacts": {
            "status": "notValidatedByGeneration2",
            "reason": "Full bundled layout/profile validation still includes planned commands, components, and states owned by later #31/#32/#59 work; generation 2 does not publish fake callable IDs to make those artifacts pass."
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
            ["ID", "Status", "Terminal", "Native"],
            ([item["id"], item["status"], str(item["terminal"]).lower(), item.get("nativeName", "—")]
             for item in collections["results"]),
        ),
        "",
        "## Components",
        "",
        *markdown_table(
            ["ID", "Status", "Class", "Workspaces", "Performance"],
            ([
                item["id"], item["status"], item["classification"],
                ", ".join(item["supportedWorkspaces"]), item["performanceClass"],
            ] for item in collections["components"]),
        ),
        "",
        "## Capabilities",
        "",
        *markdown_table(
            ["ID", "Status", "Origin", "Absence", "Qualification"],
            ([
                item["id"], item["status"], item["origin"], item["absenceBehavior"],
                item["qualificationStatus"],
            ] for item in collections["capabilities"]),
        ),
        "",
        "## Reusable values, units, and targets",
        "",
        *markdown_table(
            ["ID", "Status", "Kind", "Token / values"],
            ([
                item["id"], item["status"], item["kind"],
                item.get("token", ", ".join(item.get("enumValues", []))),
            ] for item in collections["values"]),
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
        f"components={len(collections['components'])} "
        f"capabilities={len(collections['capabilities'])} values={len(collections['values'])} "
        f"digest={digest}"
    )
    return 0


def compatibility_key(kind: str, definition: dict[str, Any]) -> dict[str, Any]:
    ignored = {
        "label", "description", "notes", "status", "introducedGeneration",
        "deprecated", "replacement", "replacementCompatibility",
    }
    return {key: value for key, value in definition.items() if key not in ignored}


def contract_change_is_compatible(
    kind: str,
    before: dict[str, Any],
    after: dict[str, Any],
) -> bool:
    before_key = compatibility_key(kind, before)
    after_key = compatibility_key(kind, after)
    if kind == "commands":
        before_feedback = before_key.pop("feedback", [])
        after_feedback = after_key.pop("feedback", [])
        if not (
            isinstance(before_feedback, list)
            and isinstance(after_feedback, list)
            and set(before_feedback).issubset(after_feedback)
        ):
            return False
    return before_key == after_key


def lifecycle_change_is_compatible(before: str | None, after: str | None) -> bool:
    return (before, after) in {
        ("planned", "bridged"),
        ("planned", "implemented"),
        ("bridged", "implemented"),
    }


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
    for kind in (
        "commands", "states", "components", "capabilities", "values", "results", "interactions"
    ):
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
            elif before[identifier].get("status") != after[identifier].get("status"):
                if (
                    lifecycle_change_is_compatible(
                        before[identifier].get("status"), after[identifier].get("status")
                    )
                    and contract_change_is_compatible(
                        kind, before[identifier], after[identifier]
                    )
                ):
                    detail["changedCompatible"].append(identifier)
                else:
                    detail["changedBreaking"].append(identifier)
            elif contract_change_is_compatible(
                kind, before[identifier], after[identifier]
            ):
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
                f"components={len(collections['components'])} "
                f"capabilities={len(collections['capabilities'])} values={len(collections['values'])} "
                f"digest={digest}"
            )
            return 0
        return diff_command(args)
    except RegistryError as exc:
        print(f"UI registry error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
