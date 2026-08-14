#!/usr/bin/env python3
"""Generate the const Ember Action view of the accepted UI registry catalog.

This is an Action-compiler build tool. It consumes the deterministic catalog
produced by generate_ui_registry.py and emits compact C++ metadata. It does not
change the canonical registry source or ship a JSON parser in Runner.
"""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = ROOT / "spec/ui/registry/generated/ui-registry.catalog.json"
HEADER_PATH = (
    ROOT
    / "native-core/include/emberlights/generated/ember_action_registry_adapter.generated.hpp"
)
GENERATOR_VERSION = "ember-action-registry-adapter/1.1.0"
CPP_NAME = re.compile(r"^[A-Z][A-Za-z0-9]*$")
VALUE_TYPES = {
    "boolean",
    "integer",
    "number",
    "enum",
    "string",
    "id",
    "semanticRole",
    "color",
    "duration",
    "object",
    "list",
    "result",
    "void",
}


class AdapterGenerationError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AdapterGenerationError(message)


def load_catalog() -> dict[str, Any]:
    try:
        catalog = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AdapterGenerationError(f"unable to read generated registry catalog: {exc}") from exc
    require(isinstance(catalog, dict), "registry catalog root must be an object")
    require(catalog.get("schemaVersion") == 1, "registry catalog schemaVersion must be 1")
    require(
        isinstance(catalog.get("registryGeneration"), int)
        and catalog["registryGeneration"] > 0,
        "registry catalog needs a positive registryGeneration",
    )
    digest = catalog.get("sourceDigest")
    require(
        isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest) is not None,
        "registry catalog sourceDigest must be lowercase SHA-256",
    )
    for kind in ("commands", "states"):
        require(isinstance(catalog.get(kind), list), f"registry catalog {kind} must be an array")
    return catalog


def cpp_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def cpp_double(value: Any) -> str:
    require(
        isinstance(value, (int, float)) and not isinstance(value, bool),
        f"numeric bound must be a number, got {value!r}",
    )
    converted = float(value)
    require(math.isfinite(converted), "numeric bounds must be finite")
    text = repr(converted)
    return text if any(character in text for character in ".eE") else text + ".0"


def cpp_bool(value: Any, field: str) -> str:
    require(isinstance(value, bool), f"{field} must be boolean")
    return "true" if value else "false"


def validate_value(value: Any, owner: str) -> dict[str, Any]:
    require(isinstance(value, dict), f"{owner}: value contract must be an object")
    kind = value.get("type")
    require(kind in VALUE_TYPES, f"{owner}: unsupported value type {kind!r}")
    for field in ("unit", "targetKind", "schemaRef"):
        require(field not in value or isinstance(value[field], str), f"{owner}: {field} must be string")
    enum_values = value.get("enumValues", [])
    require(
        isinstance(enum_values, list)
        and all(isinstance(item, str) for item in enum_values)
        and len(enum_values) == len(set(enum_values)),
        f"{owner}: enumValues must be unique strings",
    )
    for field in ("maxItems", "maxLength", "maximumStringBytes"):
        require(
            field not in value
            or (
                isinstance(value[field], int)
                and not isinstance(value[field], bool)
                and value[field] >= 0
            ),
            f"{owner}: {field} must be a non-negative integer",
        )
    if "minimum" in value:
        cpp_double(value["minimum"])
    if "maximum" in value:
        cpp_double(value["maximum"])
    if "minimum" in value and "maximum" in value:
        require(value["minimum"] <= value["maximum"], f"{owner}: minimum exceeds maximum")
    return value


class HeaderBuilder:
    def __init__(self, catalog_values: list[dict[str, Any]]) -> None:
        self.auxiliary: list[str] = []
        self.schema_values: dict[str, dict[str, Any]] = {}
        for value in catalog_values:
            require(isinstance(value, dict), "registry catalog value must be an object")
            identifier = value.get("id")
            require(isinstance(identifier, str) and identifier, "registry catalog value needs string id")
            require(identifier not in self.schema_values, f"duplicate registry value {identifier}")
            self.schema_values[identifier] = value

    def value(self, value: dict[str, Any], symbol: str) -> str:
        value = dict(value)
        schema_ref = value.get("schemaRef")
        if schema_ref:
            schema = self.schema_values.get(schema_ref)
            require(schema is not None, f"{symbol}: unknown schemaRef {schema_ref}")
            schema_type = schema.get("valueType")
            require(
                schema_type == value.get("type"),
                f"{symbol}: schemaRef {schema_ref} type does not match",
            )
            if value.get("type") == "enum" and "enumValues" not in value:
                value["enumValues"] = schema.get("enumValues", [])
        value = validate_value(value, symbol)
        enum_values = value.get("enumValues", [])
        enum_pointer = "nullptr"
        if enum_values:
            enum_symbol = f"k{symbol}EnumValues"
            rendered = ", ".join(cpp_string(item) for item in enum_values)
            self.auxiliary.extend(
                [
                    f"inline constexpr std::array<std::string_view, {len(enum_values)}> {enum_symbol}{{{{",
                    f"    {rendered}",
                    "}};",
                    "",
                ]
            )
            enum_pointer = f"{enum_symbol}.data()"
        minimum = cpp_double(value.get("minimum", 0))
        maximum = cpp_double(value.get("maximum", 0))
        maximum_string = value.get("maximumStringBytes", value.get("maxLength", 0))
        return (
            "GeneratedValueMetadata{"
            f"{cpp_string(value['type'])}, "
            f"{cpp_string(value.get('unit', ''))}, "
            f"{minimum}, {maximum}, "
            f"{'true' if 'minimum' in value else 'false'}, "
            f"{'true' if 'maximum' in value else 'false'}, "
            f"{enum_pointer}, {len(enum_values)}U, "
            f"{cpp_string(value.get('targetKind', ''))}, "
            f"{cpp_string(value.get('schemaRef', ''))}, "
            f"{value.get('maxItems', 0)}U, {maximum_string}U"
            "}"
        )

    def command_arguments(self, command: dict[str, Any]) -> tuple[str, int]:
        parameters = command.get("parameters", {})
        require(isinstance(parameters, dict), f"command {command['id']}: parameters must be an object")
        if not parameters:
            return "nullptr", 0
        symbol = f"kCommand{command['nativeName']}Arguments"
        lines = [
            f"inline constexpr std::array<GeneratedCommandArgumentMetadata, {len(parameters)}> {symbol}{{{{"
        ]
        for name in sorted(parameters):
            require(isinstance(name, str) and name, f"command {command['id']}: bad parameter name")
            parameter = parameters[name]
            require(isinstance(parameter, dict), f"command {command['id']}: parameter {name} must be object")
            required = parameter.get("required", True)
            require(isinstance(required, bool), f"command {command['id']}: required must be boolean")
            rendered = self.value(parameter, f"Command{command['nativeName']}{name.title()}")
            lines.append(
                f"    {{{cpp_string(name)}, {rendered}, {'true' if required else 'false'}}},"
            )
        lines.extend(["}};", ""])
        self.auxiliary.extend(lines)
        return f"{symbol}.data()", len(parameters)

    def command_capabilities(self, command: dict[str, Any]) -> tuple[str, int]:
        capabilities = command.get("requiredCapabilities", [])
        require(
            isinstance(capabilities, list)
            and all(isinstance(item, str) for item in capabilities)
            and len(capabilities) == len(set(capabilities)),
            f"command {command['id']}: requiredCapabilities must be unique strings",
        )
        if not capabilities:
            return "nullptr", 0
        symbol = f"kCommand{command['nativeName']}Capabilities"
        rendered = ", ".join(cpp_string(item) for item in capabilities)
        self.auxiliary.extend(
            [
                f"inline constexpr std::array<std::string_view, {len(capabilities)}> {symbol}{{{{",
                f"    {rendered}",
                "}};",
                "",
            ]
        )
        return f"{symbol}.data()", len(capabilities)


def validate_native_definition(definition: Any, kind: str) -> dict[str, Any]:
    require(isinstance(definition, dict), f"{kind} definition must be an object")
    require(isinstance(definition.get("id"), str), f"{kind} definition needs string id")
    ordinal = definition.get("nativeOrdinal")
    if kind == "command":
        name = definition.get("nativeName")
        require(
            isinstance(name, str) and CPP_NAME.fullmatch(name),
            f"{kind} {definition['id']}: bad nativeName",
        )
    require(
        isinstance(ordinal, int) and not isinstance(ordinal, bool) and 0 <= ordinal <= 255,
        f"{kind} {definition['id']}: bad nativeOrdinal",
    )
    require(
        definition.get("status") in {"implemented", "bridged", "planned", "deprecated"},
        f"{kind} {definition['id']}: unsupported status",
    )
    require(isinstance(definition.get("deprecated", False), bool), f"{kind} {definition['id']}: deprecated must be boolean")
    require(
        "replacement" not in definition or isinstance(definition["replacement"], str),
        f"{kind} {definition['id']}: replacement must be string",
    )
    return definition


def generate_header(catalog: dict[str, Any]) -> str:
    commands = sorted(
        (validate_native_definition(item, "command") for item in catalog["commands"]),
        key=lambda item: item["nativeOrdinal"],
    )
    states = sorted(
        (validate_native_definition(item, "state") for item in catalog["states"]),
        key=lambda item: item["nativeOrdinal"],
    )
    require(
        [item["nativeOrdinal"] for item in commands] == list(range(len(commands))),
        "command native ordinals must be contiguous",
    )
    require(
        [item["nativeOrdinal"] for item in states] == list(range(len(states))),
        "state native ordinals must be contiguous",
    )

    catalog_values = catalog.get("values", [])
    require(isinstance(catalog_values, list), "registry catalog values must be an array")
    builder = HeaderBuilder(catalog_values)
    command_rows: list[str] = []
    for command in commands:
        require(
            command.get("realtimeClass")
            in {
                "viewLocal",
                "studioMutation",
                "runnerCommand",
                "runnerPriority",
                "utilityAsync",
                "blockingForbiddenLive",
            },
            f"command {command['id']}: invalid realtimeClass",
        )
        arguments, argument_count = builder.command_arguments(command)
        capabilities, capability_count = builder.command_capabilities(command)
        result = builder.value(command.get("result", {"type": "result"}), f"Command{command['nativeName']}Result")
        command_rows.append(
            "    {"
            f"UiCommandId::{command['nativeName']}, {cpp_string(command['id'])}, "
            f"{cpp_string(command['realtimeClass'])}, {cpp_string(command['status'])}, "
            f"{cpp_string(command.get('replacement', ''))}, "
            f"{cpp_bool(command.get('parallelCompatible', False), 'parallelCompatible')}, "
            f"{cpp_bool(command.get('studioTransactionCompatible', False), 'studioTransactionCompatible')}, "
            f"{cpp_bool(command.get('onActivateSafe', False), 'onActivateSafe')}, "
            f"{arguments}, {argument_count}U, {capabilities}, {capability_count}U, {result}"
            "},"
        )

    state_rows: list[str] = []
    for state in states:
        value = builder.value(state.get("value"), f"State{state['nativeOrdinal']}Value")
        state_rows.append(
            "    {"
            f"{state['nativeOrdinal']}U, {cpp_string(state['id'])}, "
            f"{cpp_string(state['status'])}, {cpp_string(state.get('replacement', ''))}, {value}"
            "},"
        )

    lines = [
        "// Generated by native-core/tools/generate_ember_action_registry_adapter.py; DO NOT EDIT.",
        f"// Generator: {GENERATOR_VERSION}",
        f"// Registry generation: {catalog['registryGeneration']}",
        f"// Canonical source SHA-256: {catalog['sourceDigest']}",
        "#pragma once",
        "",
        "#include \"emberlights/generated/ui_registry.generated.hpp\"",
        "",
        "#include <array>",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace emberlights::generated_action_registry {",
        "",
        f"inline constexpr std::uint32_t kRegistryGeneration = {catalog['registryGeneration']}U;",
        f"inline constexpr std::string_view kSourceDigest = {cpp_string(catalog['sourceDigest'])};",
        "",
        "struct GeneratedValueMetadata {",
        "    std::string_view type{};",
        "    std::string_view unit{};",
        "    double minimum{0.0};",
        "    double maximum{0.0};",
        "    bool has_minimum{false};",
        "    bool has_maximum{false};",
        "    const std::string_view* enum_values{nullptr};",
        "    std::size_t enum_value_count{0U};",
        "    std::string_view target_kind{};",
        "    std::string_view schema_ref{};",
        "    std::size_t maximum_items{0U};",
        "    std::size_t maximum_string_bytes{0U};",
        "};",
        "",
        "struct GeneratedCommandArgumentMetadata {",
        "    std::string_view name{};",
        "    GeneratedValueMetadata value{};",
        "    bool required{true};",
        "};",
        "",
        "struct GeneratedCommandMetadata {",
        "    UiCommandId command{UiCommandId::ShowStart};",
        "    std::string_view id{};",
        "    std::string_view realtime_class{};",
        "    std::string_view status{};",
        "    std::string_view replacement_id{};",
        "    bool parallel_compatible{false};",
        "    bool studio_transaction_compatible{false};",
        "    bool on_activate_safe{false};",
        "    const GeneratedCommandArgumentMetadata* arguments{nullptr};",
        "    std::size_t argument_count{0U};",
        "    const std::string_view* required_capabilities{nullptr};",
        "    std::size_t required_capability_count{0U};",
        "    GeneratedValueMetadata result{};",
        "};",
        "",
        "struct GeneratedStateMetadata {",
        "    std::size_t native_ordinal{0U};",
        "    std::string_view id{};",
        "    std::string_view status{};",
        "    std::string_view replacement_id{};",
        "    GeneratedValueMetadata value{};",
        "};",
        "",
        *builder.auxiliary,
        f"inline constexpr std::array<GeneratedCommandMetadata, {len(commands)}> kCommands{{{{",
        *command_rows,
        "}};",
        "",
        f"inline constexpr std::array<GeneratedStateMetadata, {len(states)}> kStates{{{{",
        *state_rows,
        "}};",
        "",
        "static_assert(kRegistryGeneration == kUiRegistryGeneration);",
        "static_assert(kSourceDigest == kUiRegistryDigest);",
        "static_assert(kCommands.size() == kUiCommandDefinitions.size());",
        "static_assert(kStates.size() == kLiveCoreUiStates.size());",
    ]
    for index in range(len(commands)):
        lines.append(f"static_assert(kCommands[{index}].command == kUiCommandDefinitions[{index}].command);")
        lines.append(f"static_assert(kCommands[{index}].id == kUiCommandDefinitions[{index}].id);")
    for index in range(len(states)):
        lines.append(f"static_assert(kStates[{index}].id == kLiveCoreUiStates[{index}].id);")
    lines.extend(["", "}  // namespace emberlights::generated_action_registry", ""])
    return "\n".join(lines)


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
    ) as stream:
        stream.write(content)
        temporary = Path(stream.name)
    os.replace(temporary, path)


def run(check: bool) -> int:
    catalog = load_catalog()
    content = generate_header(catalog)
    current = HEADER_PATH.read_text(encoding="utf-8") if HEADER_PATH.exists() else None
    if current != content:
        if check:
            print(
                "stale generated Ember Action registry adapter: "
                + str(HEADER_PATH.relative_to(ROOT)),
                file=sys.stderr,
            )
            return 1
        atomic_write(HEADER_PATH, content)
    verb = "checked" if check else "generated"
    print(
        f"Ember Action registry adapter {verb}: "
        f"generation={catalog['registryGeneration']} "
        f"commands={len(catalog['commands'])} states={len(catalog['states'])} "
        f"digest={catalog['sourceDigest']}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("generate", "check"))
    args = parser.parse_args()
    try:
        return run(args.command == "check")
    except AdapterGenerationError as exc:
        print(f"Ember Action registry adapter error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
