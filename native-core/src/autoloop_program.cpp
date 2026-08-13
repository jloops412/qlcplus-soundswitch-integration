#include "showcore/autoloop_program.hpp"

#include "emberlights/autoloop_source.hpp"
#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace showcore {
namespace {

struct PlannedTarget {
    const emberlights::AutoloopTargetDefinition* source{nullptr};
    const AutoloopTargetBinding* binding{nullptr};
    std::vector<std::uint16_t> fixture_ids;
    std::uint64_t required_property_mask{0U};
};

struct PlannedEvent {
    const emberlights::AutoloopEventDefinition* source{nullptr};
    const AutoloopReferenceBinding* reference{nullptr};
    std::size_t target_index{0U};
    std::uint16_t lane_priority{0U};
};

struct PlannedProgram {
    const emberlights::AutoloopProgramDefinition* source{nullptr};
    std::vector<PlannedTarget> targets;
    std::vector<PlannedEvent> events;
};

struct PlannedCounts {
    std::size_t programs{0U};
    std::size_t target_spans{0U};
    std::size_t target_fixture_ids{0U};
    std::size_t events{0U};
    std::size_t curve_points{0U};
    std::size_t references{0U};
    std::size_t reference_assignments{0U};
};

[[nodiscard]] bool valid_property_value(PropertyValue value) noexcept {
    switch (value.mode) {
    case ValueMode::Release:
    case ValueMode::ForceZero:
        return true;
    case ValueMode::Set:
        return std::isfinite(value.value) && value.value >= 0.0F &&
            value.value <= 1.0F;
    }
    return false;
}

void add_diagnostic(
    AutoloopCompileResult& result,
    AutoloopCompileError error,
    AutoloopArenaKind arena,
    std::string code,
    std::string subject,
    std::string message) {
    result.diagnostics.push_back({
        error,
        arena,
        std::move(code),
        std::move(subject),
        std::move(message)});
}

[[nodiscard]] bool add_within_limit(
    std::size_t& total,
    std::size_t amount,
    std::size_t maximum) noexcept {
    if (total > maximum || amount > maximum - total) {
        return false;
    }
    total += amount;
    return true;
}

[[nodiscard]] CompiledAutoloopTargetKind compiled_target_kind(
    emberlights::AutoloopTargetKind kind) noexcept {
    switch (kind) {
    case emberlights::AutoloopTargetKind::Master:
        return CompiledAutoloopTargetKind::Master;
    case emberlights::AutoloopTargetKind::Group:
        return CompiledAutoloopTargetKind::Group;
    case emberlights::AutoloopTargetKind::Fixture:
        return CompiledAutoloopTargetKind::Fixture;
    case emberlights::AutoloopTargetKind::RoleSelector:
        return CompiledAutoloopTargetKind::RoleSelector;
    case emberlights::AutoloopTargetKind::Count:
        break;
    }
    return CompiledAutoloopTargetKind::Count;
}

[[nodiscard]] CompiledAutoloopEventKind compiled_event_kind(
    emberlights::AutoloopEventKind kind) noexcept {
    switch (kind) {
    case emberlights::AutoloopEventKind::LegacyLook:
        return CompiledAutoloopEventKind::LegacyLook;
    case emberlights::AutoloopEventKind::PropertyBlock:
        return CompiledAutoloopEventKind::PropertyBlock;
    case emberlights::AutoloopEventKind::PropertyCurve:
        return CompiledAutoloopEventKind::PropertyCurve;
    case emberlights::AutoloopEventKind::Palette:
        return CompiledAutoloopEventKind::Palette;
    case emberlights::AutoloopEventKind::Position:
        return CompiledAutoloopEventKind::Position;
    case emberlights::AutoloopEventKind::Attribute:
        return CompiledAutoloopEventKind::Attribute;
    case emberlights::AutoloopEventKind::Movement:
        return CompiledAutoloopEventKind::Movement;
    case emberlights::AutoloopEventKind::Effect:
        return CompiledAutoloopEventKind::Effect;
    case emberlights::AutoloopEventKind::Count:
        break;
    }
    return CompiledAutoloopEventKind::Count;
}

[[nodiscard]] CompiledAutoloopReferenceKind reference_kind_for_event(
    emberlights::AutoloopEventKind kind) noexcept {
    switch (kind) {
    case emberlights::AutoloopEventKind::LegacyLook:
        return CompiledAutoloopReferenceKind::LegacyLook;
    case emberlights::AutoloopEventKind::Palette:
        return CompiledAutoloopReferenceKind::Palette;
    case emberlights::AutoloopEventKind::Position:
        return CompiledAutoloopReferenceKind::Position;
    case emberlights::AutoloopEventKind::Attribute:
        return CompiledAutoloopReferenceKind::Attribute;
    case emberlights::AutoloopEventKind::Movement:
        return CompiledAutoloopReferenceKind::Movement;
    case emberlights::AutoloopEventKind::Effect:
        return CompiledAutoloopReferenceKind::Effect;
    case emberlights::AutoloopEventKind::PropertyBlock:
    case emberlights::AutoloopEventKind::PropertyCurve:
    case emberlights::AutoloopEventKind::Count:
        break;
    }
    return CompiledAutoloopReferenceKind::Count;
}

[[nodiscard]] CompiledAutoloopInterpolation compiled_interpolation(
    emberlights::AutoloopInterpolation interpolation) noexcept {
    switch (interpolation) {
    case emberlights::AutoloopInterpolation::Hold:
        return CompiledAutoloopInterpolation::Hold;
    case emberlights::AutoloopInterpolation::Linear:
        return CompiledAutoloopInterpolation::Linear;
    case emberlights::AutoloopInterpolation::SmoothStep:
        return CompiledAutoloopInterpolation::SmoothStep;
    case emberlights::AutoloopInterpolation::Count:
        break;
    }
    return CompiledAutoloopInterpolation::Count;
}

[[nodiscard]] CompiledAutoloopLaunchQuantization compiled_launch(
    emberlights::AutoloopLaunchQuantization launch) noexcept {
    return static_cast<CompiledAutoloopLaunchQuantization>(
        static_cast<std::uint8_t>(launch));
}

[[nodiscard]] CompiledAutoloopPhaseOrigin compiled_phase_origin(
    emberlights::AutoloopPhaseOrigin origin) noexcept {
    return static_cast<CompiledAutoloopPhaseOrigin>(
        static_cast<std::uint8_t>(origin));
}

[[nodiscard]] CompiledAutoloopPlaybackMode compiled_playback_mode(
    emberlights::AutoloopPlaybackMode mode) noexcept {
    return static_cast<CompiledAutoloopPlaybackMode>(
        static_cast<std::uint8_t>(mode));
}

[[nodiscard]] const AutoloopTargetBinding* find_target_binding(
    const AutoloopCompileEnvironment& environment,
    CompiledAutoloopTargetKind kind,
    std::string_view stable_ref,
    std::size_t& matches) noexcept {
    const AutoloopTargetBinding* found = nullptr;
    matches = 0U;
    for (const auto& binding : environment.targets) {
        if (binding.kind == kind && binding.stable_ref == stable_ref) {
            found = &binding;
            ++matches;
        }
    }
    return found;
}

[[nodiscard]] const AutoloopReferenceBinding* find_reference_binding(
    const AutoloopCompileEnvironment& environment,
    CompiledAutoloopReferenceKind kind,
    std::string_view stable_id,
    CompiledAutoloopTargetKind target_kind,
    std::string_view target_ref,
    std::size_t& matches) noexcept {
    const AutoloopReferenceBinding* found = nullptr;
    matches = 0U;
    for (const auto& binding : environment.references) {
        if (binding.kind == kind && binding.stable_id == stable_id &&
            binding.target_kind == target_kind &&
            binding.target_stable_ref == target_ref) {
            found = &binding;
            ++matches;
        }
    }
    return found;
}

[[nodiscard]] bool contains_fixture(
    const std::vector<std::uint16_t>& fixtures,
    std::uint16_t fixture_id) noexcept {
    return std::binary_search(fixtures.begin(), fixtures.end(), fixture_id);
}

[[nodiscard]] bool validate_reference_binding(
    const AutoloopReferenceBinding& binding,
    const PlannedTarget& target,
    std::string_view event_id,
    AutoloopCompileResult& result) {
    if (binding.semantic_version == 0U ||
        binding.kind >= CompiledAutoloopReferenceKind::Count ||
        binding.generator_kind >= CompiledAutoloopGeneratorKind::Count) {
        add_diagnostic(
            result,
            AutoloopCompileError::InvalidReferenceBinding,
            AutoloopArenaKind::References,
            "autoloop.compile.reference.invalid",
            std::string(event_id),
            "Resolved reference metadata is outside the compiled contract.");
        return false;
    }
    if (binding.semantic_version != 1U) {
        add_diagnostic(
            result,
            AutoloopCompileError::UnsupportedPayload,
            AutoloopArenaKind::References,
            "autoloop.compile.reference.versionUnsupported",
            std::string(event_id),
            "Resolved reference semantic version is not supported by compiled format 1.");
        return false;
    }
    const bool generator_reference =
        binding.kind == CompiledAutoloopReferenceKind::Movement ||
        binding.kind == CompiledAutoloopReferenceKind::Effect;
    if (generator_reference &&
        binding.generator_kind == CompiledAutoloopGeneratorKind::None) {
        add_diagnostic(
            result,
            AutoloopCompileError::InvalidReferenceBinding,
            AutoloopArenaKind::References,
            "autoloop.compile.reference.generatorMissing",
            std::string(event_id),
            "Movement/effect references require a versioned generator kind.");
        return false;
    }
    if (!generator_reference &&
        binding.generator_kind != CompiledAutoloopGeneratorKind::None) {
        add_diagnostic(
            result,
            AutoloopCompileError::InvalidReferenceBinding,
            AutoloopArenaKind::References,
            "autoloop.compile.reference.unexpectedGenerator",
            std::string(event_id),
            "Only movement/effect references may declare a generator kind.");
        return false;
    }
    if (!generator_reference && binding.assignments.empty()) {
        add_diagnostic(
            result,
            AutoloopCompileError::InvalidReferenceBinding,
            AutoloopArenaKind::ReferenceAssignments,
            "autoloop.compile.reference.empty",
            std::string(event_id),
            "Resolved semantic reference has no assignments.");
        return false;
    }

    std::set<std::uint32_t> assignment_keys;
    for (const auto& assignment : binding.assignments) {
        if (assignment.fixture_id >= kMaxFixtures ||
            assignment.property >= Property::Count ||
            !valid_property_value(assignment.value) ||
            !contains_fixture(target.fixture_ids, assignment.fixture_id)) {
            add_diagnostic(
                result,
                AutoloopCompileError::InvalidReferenceBinding,
                AutoloopArenaKind::ReferenceAssignments,
                "autoloop.compile.reference.assignment",
                std::string(event_id),
                "Resolved assignment is invalid or outside the resolved target.");
            return false;
        }
        if ((target.binding->supported_property_mask &
             autoloop_property_mask(assignment.property)) == 0U) {
            add_diagnostic(
                result,
                AutoloopCompileError::MissingCapability,
                AutoloopArenaKind::ReferenceAssignments,
                "autoloop.compile.reference.capability",
                std::string(event_id),
                "Resolved reference requires a property the target cannot provide.");
            return false;
        }
        const auto key = static_cast<std::uint32_t>(assignment.fixture_id) *
                static_cast<std::uint32_t>(kPropertyCount) +
            static_cast<std::uint32_t>(assignment.property);
        if (!assignment_keys.insert(key).second) {
            add_diagnostic(
                result,
                AutoloopCompileError::InvalidReferenceBinding,
                AutoloopArenaKind::ReferenceAssignments,
                "autoloop.compile.reference.duplicateAssignment",
                std::string(event_id),
                "Resolved reference repeats one fixture/property assignment.");
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool check_capacity(
    std::size_t actual,
    std::size_t maximum,
    AutoloopArenaKind arena,
    std::string_view subject,
    AutoloopCompileResult& result) {
    if (actual <= maximum) {
        return true;
    }
    add_diagnostic(
        result,
        AutoloopCompileError::CapacityExceeded,
        arena,
        std::string("autoloop.compile.capacity.") +
            autoloop_arena_kind_name(arena),
        std::string(subject),
        "Used Autoloop content exceeds the configured immutable arena limit.");
    return false;
}

[[nodiscard]] CompiledAutoloopStableKey stable_key(std::string_view value) {
    const auto digest = emberlights::sha256_text(value);
    CompiledAutoloopStableKey key{};
    const auto nibble = [](char character) -> std::uint8_t {
        if (character >= '0' && character <= '9') {
            return static_cast<std::uint8_t>(character - '0');
        }
        return static_cast<std::uint8_t>(10 + character - 'a');
    };
    for (std::size_t index = 0U; index < key.size(); ++index) {
        key[index] = static_cast<std::uint8_t>(
            (nibble(digest[index * 2U]) << 4U) |
            nibble(digest[index * 2U + 1U]));
    }
    return key;
}

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    for (std::size_t index = 0U; index < 2U; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (std::size_t index = 0U; index < 4U; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void append_u64(std::vector<std::uint8_t>& bytes, std::uint64_t value) {
    for (std::size_t index = 0U; index < 8U; ++index) {
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
    }
}

void append_i64(std::vector<std::uint8_t>& bytes, std::int64_t value) {
    append_u64(bytes, std::bit_cast<std::uint64_t>(value));
}

void append_float(std::vector<std::uint8_t>& bytes, float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

void append_key(
    std::vector<std::uint8_t>& bytes,
    const CompiledAutoloopStableKey& key) {
    bytes.insert(bytes.end(), key.begin(), key.end());
}

void append_value(std::vector<std::uint8_t>& bytes, PropertyValue value) {
    append_u8(bytes, static_cast<std::uint8_t>(value.mode));
    append_float(bytes, value.value);
}

[[nodiscard]] std::vector<std::uint8_t> canonical_package_bytes(
    const CompiledAutoloopPackage& package) {
    std::vector<std::uint8_t> bytes;
    constexpr std::array<std::uint8_t, 8U> magic{{
        'E', 'L', 'A', 'L', 'P', '0', '0', '1'}};
    bytes.insert(bytes.end(), magic.begin(), magic.end());
    append_u32(bytes, package.format_version());
    append_u32(bytes, static_cast<std::uint32_t>(package.programs().size()));
    append_u32(bytes, static_cast<std::uint32_t>(package.target_spans().size()));
    append_u32(
        bytes, static_cast<std::uint32_t>(package.target_fixture_ids().size()));
    append_u32(bytes, static_cast<std::uint32_t>(package.events().size()));
    append_u32(bytes, static_cast<std::uint32_t>(package.curve_points().size()));
    append_u32(bytes, static_cast<std::uint32_t>(package.references().size()));
    append_u32(
        bytes,
        static_cast<std::uint32_t>(package.reference_assignments().size()));

    for (std::size_t bank = 0U; bank < kMaxAutoloopBanks; ++bank) {
        for (std::size_t slot = 0U; slot < kAutoloopsPerBank; ++slot) {
            const auto* placement = package.placement({
                static_cast<std::uint16_t>(bank),
                static_cast<std::uint8_t>(slot)});
            append_u32(bytes, placement->program_index);
            append_key(bytes, placement->asset_key);
            append_u8(bytes, static_cast<std::uint8_t>(placement->repeat));
            append_u8(bytes, static_cast<std::uint8_t>(placement->launch));
            append_u8(bytes, static_cast<std::uint8_t>(placement->phase_origin));
            append_u8(bytes, static_cast<std::uint8_t>(placement->mode));
            append_i64(bytes, placement->return_fade_ticks);
            append_u8(bytes, placement->track_boundary_required ? 1U : 0U);
        }
    }
    for (const auto& program : package.programs()) {
        append_key(bytes, program.program_key);
        append_i64(bytes, program.length_ticks);
        append_u16(bytes, program.time_signature_numerator);
        append_u16(bytes, program.time_signature_denominator);
        append_u32(bytes, program.target_offset);
        append_u32(bytes, program.target_count);
        append_u32(bytes, program.event_offset);
        append_u32(bytes, program.event_count);
    }
    for (const auto& target : package.target_spans()) {
        append_u32(bytes, target.fixture_offset);
        append_u32(bytes, target.fixture_count);
        append_u64(bytes, target.required_property_mask);
    }
    for (const auto fixture_id : package.target_fixture_ids()) {
        append_u16(bytes, fixture_id);
    }
    for (const auto& event : package.events()) {
        append_u8(bytes, static_cast<std::uint8_t>(event.kind));
        append_i64(bytes, event.start_tick);
        append_i64(bytes, event.end_tick);
        append_u32(bytes, event.target_span_index);
        append_u16(bytes, event.lane_priority);
        append_u8(bytes, static_cast<std::uint8_t>(event.property));
        append_value(bytes, event.value);
        append_u8(bytes, static_cast<std::uint8_t>(event.interpolation));
        append_u32(bytes, event.curve_offset);
        append_u32(bytes, event.curve_count);
        append_u32(bytes, event.reference_index);
        append_u32(bytes, event.next_legacy_reference_index);
        append_u32(bytes, event.payload_version);
        append_float(bytes, event.generator.rate_start);
        append_float(bytes, event.generator.rate_end);
        append_float(bytes, event.generator.size_start);
        append_float(bytes, event.generator.size_end);
        append_float(bytes, event.generator.phase);
        append_float(bytes, event.generator.spread);
        append_float(bytes, event.generator.base_primary);
        append_float(bytes, event.generator.base_secondary);
        append_u64(bytes, event.generator.seed);
        append_u8(bytes, static_cast<std::uint8_t>(event.legacy_transition));
    }
    for (const auto& point : package.curve_points()) {
        append_i64(bytes, point.tick);
        append_value(bytes, point.value);
    }
    for (const auto& reference : package.references()) {
        append_key(bytes, reference.reference_key);
        append_u8(bytes, static_cast<std::uint8_t>(reference.kind));
        append_u8(bytes, static_cast<std::uint8_t>(reference.generator_kind));
        append_u32(bytes, reference.semantic_version);
        append_u32(bytes, reference.assignment_offset);
        append_u32(bytes, reference.assignment_count);
    }
    for (const auto& assignment : package.reference_assignments()) {
        append_u16(bytes, assignment.fixture_id);
        append_u8(bytes, static_cast<std::uint8_t>(assignment.property));
        append_value(bytes, assignment.value);
    }
    return bytes;
}

[[nodiscard]] std::size_t placement_index(AutoloopAddress address) noexcept {
    return static_cast<std::size_t>(address.bank) * kAutoloopsPerBank +
        address.slot;
}

[[nodiscard]] PropertyValue interpolated_value(
    PropertyValue first,
    PropertyValue second,
    float amount,
    CompiledAutoloopInterpolation interpolation) noexcept {
    if (amount <= 0.0F) {
        return first;
    }
    if (amount >= 1.0F) {
        return second;
    }
    if (interpolation == CompiledAutoloopInterpolation::Hold) {
        return first;
    }
    if (first.mode != ValueMode::Set || second.mode != ValueMode::Set) {
        return first;
    }
    auto shaped = std::clamp(amount, 0.0F, 1.0F);
    if (interpolation == CompiledAutoloopInterpolation::SmoothStep) {
        shaped = shaped * shaped * (3.0F - 2.0F * shaped);
    }
    return PropertyValue::set(
        first.value + (second.value - first.value) * shaped);
}

[[nodiscard]] float effective_reference_value(PropertyValue value) noexcept {
    return value.mode == ValueMode::Set
        ? std::clamp(value.value, 0.0F, 1.0F)
        : 0.0F;
}

[[nodiscard]] float generator_wave(
    CompiledAutoloopGeneratorKind kind,
    double cycle) noexcept {
    constexpr double two_pi = 6.283185307179586476925286766559;
    const auto fraction = cycle - std::floor(cycle);
    switch (kind) {
    case CompiledAutoloopGeneratorKind::Triangle:
    case CompiledAutoloopGeneratorKind::Square:
        return static_cast<float>(1.0 - std::abs(2.0 * fraction - 1.0));
    case CompiledAutoloopGeneratorKind::Pulse:
        return fraction < 0.5 ? 1.0F : 0.0F;
    case CompiledAutoloopGeneratorKind::FigureEight:
        return static_cast<float>(0.5 + 0.5 * std::sin(two_pi * cycle * 2.0));
    case CompiledAutoloopGeneratorKind::TripleEight:
        return static_cast<float>(0.5 + 0.5 * std::sin(two_pi * cycle * 3.0));
    case CompiledAutoloopGeneratorKind::Circle:
    case CompiledAutoloopGeneratorKind::HorizontalScan:
    case CompiledAutoloopGeneratorKind::VerticalScan:
    case CompiledAutoloopGeneratorKind::Oval:
    case CompiledAutoloopGeneratorKind::Sine:
        return static_cast<float>(0.5 + 0.5 * std::sin(two_pi * cycle));
    case CompiledAutoloopGeneratorKind::None:
    case CompiledAutoloopGeneratorKind::Count:
        break;
    }
    return 0.5F;
}

}  // namespace

CompiledAutoloopPackage::CompiledAutoloopPackage() = default;

const CompiledAutoloopPlacement* CompiledAutoloopPackage::placement(
    AutoloopAddress address) const noexcept {
    return address.valid() ? &placements_[placement_index(address)] : nullptr;
}

const CompiledAutoloopProgramHeader* CompiledAutoloopPackage::program(
    std::size_t index) const noexcept {
    return index < programs_.size() ? &programs_[index] : nullptr;
}

std::size_t CompiledAutoloopPackage::arena_bytes() const noexcept {
    return placements_.size() * sizeof(placements_.front()) +
        programs_.size() * sizeof(programs_.front()) +
        target_spans_.size() * sizeof(target_spans_.front()) +
        target_fixture_ids_.size() * sizeof(target_fixture_ids_.front()) +
        events_.size() * sizeof(events_.front()) +
        curve_points_.size() * sizeof(curve_points_.front()) +
        references_.size() * sizeof(references_.front()) +
        reference_assignments_.size() * sizeof(reference_assignments_.front()) +
        canonical_bytes_.size() * sizeof(canonical_bytes_.front()) +
        digest_.size();
}

AutoloopCompileResult compile_autoloop_programs(
    const emberlights::AutoloopSourceDocument& source,
    const AutoloopCompileEnvironment& environment,
    const AutoloopCompileLimits& limits) {
    AutoloopCompileResult result;
    const auto source_validation =
        emberlights::validate_autoloop_source(source);
    if (!source_validation.ok()) {
        const auto& issue = source_validation.issues.front();
        add_diagnostic(
            result,
            AutoloopCompileError::InvalidSource,
            AutoloopArenaKind::None,
            issue.code,
            issue.subject,
            issue.message);
        return result;
    }

    std::unordered_map<std::string_view,
        const emberlights::AutoloopAssetDefinition*> assets;
    std::unordered_map<std::string_view,
        const emberlights::AutoloopProgramDefinition*> programs;
    std::unordered_map<std::string_view,
        const emberlights::AutoloopLaunchProfileDefinition*> launches;
    for (const auto& asset : source.assets) {
        assets.emplace(asset.id, &asset);
    }
    for (const auto& program : source.programs) {
        programs.emplace(program.id, &program);
    }
    for (const auto& launch : source.launch_profiles) {
        launches.emplace(launch.id, &launch);
    }

    std::set<std::string_view> used_program_ids;
    for (const auto& placement : source.placements) {
        const auto asset = assets.find(placement.asset_id);
        if (asset == assets.end()) {
            add_diagnostic(
                result,
                AutoloopCompileError::InternalError,
                AutoloopArenaKind::Programs,
                "autoloop.compile.assetMissing",
                placement.id,
                "Validated placement asset disappeared during compilation.");
            return result;
        }
        used_program_ids.insert(asset->second->program_id);
    }

    if (!check_capacity(
            used_program_ids.size(), limits.maximum_programs,
            AutoloopArenaKind::Programs, "used-programs", result)) {
        return result;
    }

    PlannedCounts counts;
    counts.programs = used_program_ids.size();
    std::vector<PlannedProgram> plan;
    plan.reserve(counts.programs);

    for (const auto program_id : used_program_ids) {
        const auto source_program = programs.find(program_id);
        if (source_program == programs.end()) {
            add_diagnostic(
                result,
                AutoloopCompileError::InternalError,
                AutoloopArenaKind::Programs,
                "autoloop.compile.programMissing",
                std::string(program_id),
                "Validated program disappeared during compilation.");
            return result;
        }

        PlannedProgram planned;
        planned.source = source_program->second;
        std::vector<const emberlights::AutoloopTargetDefinition*> targets;
        targets.reserve(planned.source->targets.size());
        for (const auto& target : planned.source->targets) {
            targets.push_back(&target);
        }
        std::sort(
            targets.begin(), targets.end(),
            [](const auto* first, const auto* second) {
                return first->id < second->id;
            });

        std::unordered_map<std::string_view, std::size_t> target_indices;
        for (const auto* target : targets) {
            const auto kind = compiled_target_kind(target->kind);
            std::size_t matches = 0U;
            const auto* binding = find_target_binding(
                environment, kind, target->stable_ref, matches);
            if (matches == 0U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::MissingTarget,
                    AutoloopArenaKind::TargetSpans,
                    "autoloop.compile.target.missing",
                    target->id,
                    "No immutable venue binding resolves this target.");
                return result;
            }
            if (matches != 1U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::AmbiguousTarget,
                    AutoloopArenaKind::TargetSpans,
                    "autoloop.compile.target.ambiguous",
                    target->id,
                    "Multiple immutable venue bindings resolve this target.");
                return result;
            }
            if (binding->fixture_ids.empty()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InvalidTargetBinding,
                    AutoloopArenaKind::TargetFixtureIds,
                    "autoloop.compile.target.empty",
                    target->id,
                    "Resolved target contains no fixtures.");
                return result;
            }
            if ((binding->supported_property_mask &
                 ~all_autoloop_property_mask()) != 0U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InvalidTargetBinding,
                    AutoloopArenaKind::TargetSpans,
                    "autoloop.compile.target.propertyMask",
                    target->id,
                    "Resolved target capability mask contains unknown properties.");
                return result;
            }
            PlannedTarget planned_target;
            planned_target.source = target;
            planned_target.binding = binding;
            planned_target.fixture_ids.assign(
                binding->fixture_ids.begin(), binding->fixture_ids.end());
            std::sort(
                planned_target.fixture_ids.begin(),
                planned_target.fixture_ids.end());
            if (planned_target.fixture_ids.back() >= kMaxFixtures ||
                std::adjacent_find(
                    planned_target.fixture_ids.begin(),
                    planned_target.fixture_ids.end()) !=
                    planned_target.fixture_ids.end()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InvalidTargetBinding,
                    AutoloopArenaKind::TargetFixtureIds,
                    "autoloop.compile.target.fixtures",
                    target->id,
                    "Resolved target fixture IDs are invalid or duplicated.");
                return result;
            }
            for (const auto property : target->required_properties) {
                planned_target.required_property_mask |=
                    autoloop_property_mask(property);
            }
            if ((planned_target.required_property_mask &
                 ~binding->supported_property_mask) != 0U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::MissingCapability,
                    AutoloopArenaKind::TargetSpans,
                    "autoloop.compile.target.capability",
                    target->id,
                    "Resolved target does not satisfy declared capabilities.");
                return result;
            }
            target_indices.emplace(target->id, planned.targets.size());
            planned.targets.push_back(std::move(planned_target));
        }

        struct LanePlan {
            std::size_t target_index{0U};
            std::uint16_t priority{0U};
        };
        std::unordered_map<std::string_view, LanePlan> lanes;
        for (const auto& lane : planned.source->lanes) {
            const auto target = target_indices.find(lane.target_id);
            if (target == target_indices.end()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InternalError,
                    AutoloopArenaKind::TargetSpans,
                    "autoloop.compile.laneTargetMissing",
                    lane.id,
                    "Validated lane target disappeared during compilation.");
                return result;
            }
            lanes.emplace(lane.id, LanePlan{target->second, lane.priority});
        }

        std::vector<const emberlights::AutoloopEventDefinition*> events;
        events.reserve(planned.source->events.size());
        for (const auto& event : planned.source->events) {
            events.push_back(&event);
        }
        std::sort(
            events.begin(), events.end(),
            [&lanes](const auto* first, const auto* second) {
                const auto first_lane = lanes.find(first->lane_id);
                const auto second_lane = lanes.find(second->lane_id);
                if (first_lane->second.priority != second_lane->second.priority) {
                    return first_lane->second.priority <
                        second_lane->second.priority;
                }
                if (first->start_tick != second->start_tick) {
                    return first->start_tick < second->start_tick;
                }
                if (first->end_tick != second->end_tick) {
                    return first->end_tick < second->end_tick;
                }
                return first->id < second->id;
            });

        for (const auto* event : events) {
            const auto lane = lanes.find(event->lane_id);
            if (lane == lanes.end()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InternalError,
                    AutoloopArenaKind::Events,
                    "autoloop.compile.eventLaneMissing",
                    event->id,
                    "Validated event lane disappeared during compilation.");
                return result;
            }
            if (!event->transition_reference_id.empty()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::UnsupportedPayload,
                    AutoloopArenaKind::References,
                    "autoloop.compile.transition.unsupported",
                    event->id,
                    "Custom transition references are retained in source but are not yet executable.");
                return result;
            }
            if (event->payload_version != 1U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::UnsupportedPayload,
                    AutoloopArenaKind::Events,
                    "autoloop.compile.event.versionUnsupported",
                    event->id,
                    "Event payload version is not supported by compiled format 1.");
                return result;
            }
            if ((event->kind == emberlights::AutoloopEventKind::Movement ||
                 event->kind == emberlights::AutoloopEventKind::Effect) &&
                event->value.mode != ValueMode::Release) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::UnsupportedPayload,
                    AutoloopArenaKind::Events,
                    "autoloop.compile.generator.valueUnsupported",
                    event->id,
                    "Generator events use versioned generator bases; a separate property value is not supported.");
                return result;
            }

            auto& target = planned.targets[lane->second.target_index];
            const bool property_event =
                event->kind == emberlights::AutoloopEventKind::PropertyBlock ||
                event->kind == emberlights::AutoloopEventKind::PropertyCurve ||
                event->kind == emberlights::AutoloopEventKind::Movement ||
                event->kind == emberlights::AutoloopEventKind::Effect;
            if (property_event &&
                (target.binding->supported_property_mask &
                 autoloop_property_mask(event->property)) == 0U) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::MissingCapability,
                    AutoloopArenaKind::Events,
                    "autoloop.compile.event.capability",
                    event->id,
                    "Event property is unavailable on its resolved target.");
                return result;
            }
            if (property_event) {
                target.required_property_mask |=
                    autoloop_property_mask(event->property);
            }

            const auto reference_kind = reference_kind_for_event(event->kind);
            const AutoloopReferenceBinding* reference = nullptr;
            if (reference_kind != CompiledAutoloopReferenceKind::Count) {
                std::size_t matches = 0U;
                reference = find_reference_binding(
                    environment,
                    reference_kind,
                    event->reference_id,
                    compiled_target_kind(target.source->kind),
                    target.source->stable_ref,
                    matches);
                if (matches == 0U) {
                    add_diagnostic(
                        result,
                        AutoloopCompileError::MissingReference,
                        AutoloopArenaKind::References,
                        "autoloop.compile.reference.missing",
                        event->id,
                        "No resolved semantic reference matches this event and target.");
                    return result;
                }
                if (matches != 1U) {
                    add_diagnostic(
                        result,
                        AutoloopCompileError::AmbiguousReference,
                        AutoloopArenaKind::References,
                        "autoloop.compile.reference.ambiguous",
                        event->id,
                        "Multiple resolved references match this event and target.");
                    return result;
                }
                if (!validate_reference_binding(
                        *reference, target, event->id, result)) {
                    return result;
                }
                for (const auto& assignment : reference->assignments) {
                    target.required_property_mask |=
                        autoloop_property_mask(assignment.property);
                }
            }

            planned.events.push_back({
                event,
                reference,
                lane->second.target_index,
                lane->second.priority});
        }

        if (!add_within_limit(
                counts.target_spans, planned.targets.size(),
                limits.maximum_target_spans)) {
            add_diagnostic(
                result,
                AutoloopCompileError::CapacityExceeded,
                AutoloopArenaKind::TargetSpans,
                "autoloop.compile.capacity.targetSpans",
                planned.source->id,
                "Program targets exceed the immutable target-span arena limit.");
            return result;
        }
        if (!add_within_limit(
                counts.events, planned.events.size(),
                limits.maximum_events)) {
            add_diagnostic(
                result,
                AutoloopCompileError::CapacityExceeded,
                AutoloopArenaKind::Events,
                "autoloop.compile.capacity.events",
                planned.source->id,
                "Program events exceed the immutable event arena limit.");
            return result;
        }
        for (const auto& target : planned.targets) {
            if (!add_within_limit(
                    counts.target_fixture_ids, target.fixture_ids.size(),
                    limits.maximum_target_fixture_ids)) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::CapacityExceeded,
                    AutoloopArenaKind::TargetFixtureIds,
                    "autoloop.compile.capacity.targetFixtureIds",
                    target.source->id,
                    "Resolved fixture spans exceed the immutable arena limit.");
                return result;
            }
        }
        for (const auto& event : planned.events) {
            if (!add_within_limit(
                    counts.curve_points, event.source->curve_points.size(),
                    limits.maximum_curve_points)) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::CapacityExceeded,
                    AutoloopArenaKind::CurvePoints,
                    "autoloop.compile.capacity.curvePoints",
                    event.source->id,
                    "Curve points exceed the immutable arena limit.");
                return result;
            }
            if (event.reference != nullptr) {
                if (!add_within_limit(
                        counts.references, 1U,
                        limits.maximum_references)) {
                    add_diagnostic(
                        result,
                        AutoloopCompileError::CapacityExceeded,
                        AutoloopArenaKind::References,
                        "autoloop.compile.capacity.references",
                        event.source->id,
                        "Resolved references exceed the immutable reference arena limit.");
                    return result;
                }
                if (!add_within_limit(
                        counts.reference_assignments,
                        event.reference->assignments.size(),
                        limits.maximum_reference_assignments)) {
                    add_diagnostic(
                        result,
                        AutoloopCompileError::CapacityExceeded,
                        AutoloopArenaKind::ReferenceAssignments,
                        "autoloop.compile.capacity.referenceAssignments",
                        event.source->id,
                        "Resolved assignments exceed the immutable reference-assignment arena limit.");
                    return result;
                }
            }
        }
        plan.push_back(std::move(planned));
    }

    auto compiled = std::unique_ptr<CompiledAutoloopPackage>(
        new CompiledAutoloopPackage());
    compiled->programs_.reserve(counts.programs);
    compiled->target_spans_.reserve(counts.target_spans);
    compiled->target_fixture_ids_.reserve(counts.target_fixture_ids);
    compiled->events_.reserve(counts.events);
    compiled->curve_points_.reserve(counts.curve_points);
    compiled->references_.reserve(counts.references);
    compiled->reference_assignments_.reserve(counts.reference_assignments);

    std::unordered_map<std::string_view, std::uint32_t> compiled_program_indices;
    for (const auto& planned : plan) {
        CompiledAutoloopProgramHeader header;
        header.program_key = stable_key(planned.source->id);
        header.length_ticks = planned.source->length_ticks;
        header.time_signature_numerator =
            planned.source->time_signature_numerator;
        header.time_signature_denominator =
            planned.source->time_signature_denominator;
        header.target_offset = static_cast<std::uint32_t>(
            compiled->target_spans_.size());
        header.target_count = static_cast<std::uint32_t>(planned.targets.size());
        for (const auto& target : planned.targets) {
            const auto fixture_offset = static_cast<std::uint32_t>(
                compiled->target_fixture_ids_.size());
            compiled->target_fixture_ids_.insert(
                compiled->target_fixture_ids_.end(),
                target.fixture_ids.begin(), target.fixture_ids.end());
            compiled->target_spans_.push_back({
                fixture_offset,
                static_cast<std::uint32_t>(target.fixture_ids.size()),
                target.required_property_mask});
        }

        header.event_offset = static_cast<std::uint32_t>(
            compiled->events_.size());
        header.event_count = static_cast<std::uint32_t>(planned.events.size());
        std::unordered_map<const emberlights::AutoloopEventDefinition*,
            std::uint32_t> event_indices;
        std::unordered_map<const emberlights::AutoloopEventDefinition*,
            std::uint32_t> reference_indices;

        for (const auto& planned_event : planned.events) {
            const auto& source_event = *planned_event.source;
            CompiledAutoloopEvent event;
            event.kind = compiled_event_kind(source_event.kind);
            event.start_tick = source_event.start_tick;
            event.end_tick = source_event.end_tick;
            event.target_span_index = header.target_offset +
                static_cast<std::uint32_t>(planned_event.target_index);
            event.lane_priority = planned_event.lane_priority;
            event.property = source_event.property;
            event.value = source_event.value;
            event.interpolation = compiled_interpolation(
                source_event.interpolation);
            event.curve_offset = static_cast<std::uint32_t>(
                compiled->curve_points_.size());
            event.curve_count = static_cast<std::uint32_t>(
                source_event.curve_points.size());
            for (const auto& point : source_event.curve_points) {
                compiled->curve_points_.push_back({point.tick, point.value});
            }
            if (planned_event.reference != nullptr) {
                const auto& source_reference = *planned_event.reference;
                event.reference_index = static_cast<std::uint32_t>(
                    compiled->references_.size());
                reference_indices.emplace(
                    planned_event.source, event.reference_index);
                std::vector<LookAssignment> assignments(
                    source_reference.assignments.begin(),
                    source_reference.assignments.end());
                std::sort(
                    assignments.begin(), assignments.end(),
                    [](const auto& first, const auto& second) {
                        if (first.fixture_id != second.fixture_id) {
                            return first.fixture_id < second.fixture_id;
                        }
                        return static_cast<std::uint8_t>(first.property) <
                            static_cast<std::uint8_t>(second.property);
                    });
                const auto assignment_offset = static_cast<std::uint32_t>(
                    compiled->reference_assignments_.size());
                compiled->reference_assignments_.insert(
                    compiled->reference_assignments_.end(),
                    assignments.begin(), assignments.end());
                compiled->references_.push_back({
                    stable_key(source_reference.stable_id),
                    source_reference.kind,
                    source_reference.generator_kind,
                    source_reference.semantic_version,
                    assignment_offset,
                    static_cast<std::uint32_t>(assignments.size())});
            }
            event.generator = {
                source_event.generator.rate_start,
                source_event.generator.rate_end,
                source_event.generator.size_start,
                source_event.generator.size_end,
                source_event.generator.phase,
                source_event.generator.spread,
                source_event.generator.base_primary,
                source_event.generator.base_secondary,
                source_event.generator.seed};
            event.legacy_transition = source_event.legacy_transition;
            event_indices.emplace(
                planned_event.source,
                static_cast<std::uint32_t>(compiled->events_.size()));
            compiled->events_.push_back(event);
        }

        for (const auto& planned_event : planned.events) {
            if (planned_event.source->kind !=
                    emberlights::AutoloopEventKind::LegacyLook ||
                planned_event.source->legacy_transition !=
                    AutoloopTransition::Linear) {
                continue;
            }
            const emberlights::AutoloopEventDefinition* next = nullptr;
            for (const auto& candidate : planned.events) {
                if (candidate.source->kind !=
                        emberlights::AutoloopEventKind::LegacyLook ||
                    candidate.source->lane_id !=
                        planned_event.source->lane_id) {
                    continue;
                }
                if (candidate.source->start_tick >
                        planned_event.source->start_tick &&
                    (next == nullptr || candidate.source->start_tick <
                        next->start_tick)) {
                    next = candidate.source;
                }
            }
            if (next == nullptr) {
                for (const auto& candidate : planned.events) {
                    if (candidate.source->kind ==
                            emberlights::AutoloopEventKind::LegacyLook &&
                        candidate.source->lane_id ==
                            planned_event.source->lane_id &&
                        (next == nullptr || candidate.source->start_tick <
                            next->start_tick)) {
                        next = candidate.source;
                    }
                }
            }
            const auto compiled_event = event_indices.find(
                planned_event.source);
            const auto next_reference = reference_indices.find(next);
            if (compiled_event == event_indices.end() ||
                next_reference == reference_indices.end()) {
                add_diagnostic(
                    result,
                    AutoloopCompileError::InternalError,
                    AutoloopArenaKind::Events,
                    "autoloop.compile.legacySequence",
                    planned_event.source->id,
                    "Legacy transition could not resolve its next reference.");
                return result;
            }
            compiled->events_[compiled_event->second].
                next_legacy_reference_index = next_reference->second;
        }

        const auto program_index = static_cast<std::uint32_t>(
            compiled->programs_.size());
        compiled_program_indices.emplace(planned.source->id, program_index);
        compiled->programs_.push_back(header);
    }

    std::vector<const emberlights::AutoloopPlacementDefinition*> placements;
    placements.reserve(source.placements.size());
    for (const auto& placement : source.placements) {
        placements.push_back(&placement);
    }
    std::sort(
        placements.begin(), placements.end(),
        [](const auto* first, const auto* second) {
            if (first->bank != second->bank) {
                return first->bank < second->bank;
            }
            if (first->slot != second->slot) {
                return first->slot < second->slot;
            }
            return first->id < second->id;
        });
    for (const auto* placement : placements) {
        const auto asset = assets.find(placement->asset_id);
        const auto launch = launches.find(asset->second->launch_profile_id);
        const auto program = compiled_program_indices.find(
            asset->second->program_id);
        if (launch == launches.end() ||
            program == compiled_program_indices.end()) {
            add_diagnostic(
                result,
                AutoloopCompileError::InternalError,
                AutoloopArenaKind::Programs,
                "autoloop.compile.placementDependency",
                placement->id,
                "Validated placement dependency disappeared during compilation.");
            return result;
        }
        compiled->placements_[placement_index({
            placement->bank, placement->slot})] = {
                program->second,
                stable_key(asset->second->id),
                launch->second->repeat,
                compiled_launch(launch->second->launch),
                compiled_phase_origin(launch->second->phase_origin),
                compiled_playback_mode(launch->second->mode),
                launch->second->return_fade_ticks,
                launch->second->track_boundary_required};
    }

    compiled->canonical_bytes_ = canonical_package_bytes(*compiled);
    if (!check_capacity(
            compiled->canonical_bytes_.size(),
            limits.maximum_canonical_bytes,
            AutoloopArenaKind::CanonicalBytes,
            "canonical-package",
            result)) {
        return result;
    }
    compiled->digest_ = emberlights::sha256_bytes(
        compiled->canonical_bytes_);
    result.package = std::unique_ptr<const CompiledAutoloopPackage>(
        compiled.release());
    return result;
}

bool AutoloopProgramEvaluator::evaluate(
    const CompiledAutoloopPackage& package,
    std::size_t program_index,
    std::int64_t tick,
    LayerBuffer& output) noexcept {
    output.clear();
    const auto* program = package.program(program_index);
    if (program == nullptr || program->length_ticks <= 0 ||
        program->event_offset > package.events().size() ||
        program->event_count >
            package.events().size() - program->event_offset) {
        return false;
    }

    auto phase = tick % program->length_ticks;
    if (phase < 0) {
        phase += program->length_ticks;
    }
    const auto events = package.events().subspan(
        program->event_offset, program->event_count);
    const auto targets = package.target_spans();
    const auto target_fixtures = package.target_fixture_ids();
    const auto references = package.references();
    const auto assignments = package.reference_assignments();
    const auto curves = package.curve_points();

    const auto target_for = [&](const CompiledAutoloopEvent& event)
        -> const CompiledAutoloopTargetSpan* {
        return event.target_span_index < targets.size()
            ? &targets[event.target_span_index]
            : nullptr;
    };
    const auto fixtures_for = [&](const CompiledAutoloopTargetSpan& target)
        -> std::span<const std::uint16_t> {
        if (target.fixture_offset > target_fixtures.size() ||
            target.fixture_count >
                target_fixtures.size() - target.fixture_offset) {
            return {};
        }
        return target_fixtures.subspan(
            target.fixture_offset, target.fixture_count);
    };
    const auto reference_for = [&](std::uint32_t index)
        -> const CompiledAutoloopReference* {
        return index < references.size() ? &references[index] : nullptr;
    };
    const auto load_reference = [&](std::uint32_t index, LayerBuffer& buffer)
        -> bool {
        buffer.clear();
        const auto* reference = reference_for(index);
        if (reference == nullptr ||
            reference->assignment_offset > assignments.size() ||
            reference->assignment_count >
                assignments.size() - reference->assignment_offset) {
            return false;
        }
        for (const auto& assignment : assignments.subspan(
                 reference->assignment_offset,
                 reference->assignment_count)) {
            buffer.set(
                assignment.fixture_id,
                assignment.property,
                assignment.value);
        }
        return true;
    };
    const auto apply_reference = [&](std::uint32_t index) -> bool {
        const auto* reference = reference_for(index);
        if (reference == nullptr ||
            reference->assignment_offset > assignments.size() ||
            reference->assignment_count >
                assignments.size() - reference->assignment_offset) {
            return false;
        }
        for (const auto& assignment : assignments.subspan(
                 reference->assignment_offset,
                 reference->assignment_count)) {
            if (assignment.value.mode != ValueMode::Release) {
                output.set(
                    assignment.fixture_id,
                    assignment.property,
                    assignment.value);
            }
        }
        return true;
    };

    for (const auto& event : events) {
        if (phase < event.start_tick || phase >= event.end_tick) {
            continue;
        }
        const auto* target = target_for(event);
        if (target == nullptr) {
            output.clear();
            return false;
        }
        const auto fixtures = fixtures_for(*target);
        if (fixtures.empty()) {
            output.clear();
            return false;
        }
        const auto apply_to_target = [&](PropertyValue value) {
            if (value.mode == ValueMode::Release) {
                return;
            }
            for (const auto fixture_id : fixtures) {
                output.set(fixture_id, event.property, value);
            }
        };

        switch (event.kind) {
        case CompiledAutoloopEventKind::LegacyLook: {
            if (!load_reference(event.reference_index, current_reference_)) {
                output.clear();
                return false;
            }
            if (event.legacy_transition == AutoloopTransition::Linear) {
                if (!load_reference(
                        event.next_legacy_reference_index,
                        next_reference_)) {
                    output.clear();
                    return false;
                }
                const auto amount = static_cast<float>(
                    static_cast<long double>(phase - event.start_tick) /
                    static_cast<long double>(
                        event.end_tick - event.start_tick));
                for (const auto fixture_id : fixtures) {
                    for (std::size_t property_index = 0U;
                         property_index < kPropertyCount; ++property_index) {
                        const auto property = static_cast<Property>(
                            property_index);
                        const auto first = current_reference_.get(
                            fixture_id, property);
                        const auto second = next_reference_.get(
                            fixture_id, property);
                        if (first.mode == ValueMode::Release &&
                            second.mode == ValueMode::Release) {
                            continue;
                        }
                        if (amount <= 0.0F) {
                            if (first.mode != ValueMode::Release) {
                                output.set(fixture_id, property, first);
                            }
                            continue;
                        }
                        const auto value = effective_reference_value(first) +
                            (effective_reference_value(second) -
                             effective_reference_value(first)) *
                                std::clamp(amount, 0.0F, 1.0F);
                        output.set(
                            fixture_id, property,
                            PropertyValue::set(value));
                    }
                }
            } else if (!apply_reference(event.reference_index)) {
                output.clear();
                return false;
            }
            break;
        }
        case CompiledAutoloopEventKind::PropertyBlock:
            apply_to_target(event.value);
            break;
        case CompiledAutoloopEventKind::PropertyCurve: {
            if (event.curve_offset > curves.size() || event.curve_count < 2U ||
                event.curve_count > curves.size() - event.curve_offset) {
                output.clear();
                return false;
            }
            const auto points = curves.subspan(
                event.curve_offset, event.curve_count);
            PropertyValue value = points.front().value;
            for (std::size_t index = 1U; index < points.size(); ++index) {
                if (phase > points[index].tick) {
                    value = points[index].value;
                    continue;
                }
                const auto duration = points[index].tick -
                    points[index - 1U].tick;
                const auto amount = duration > 0
                    ? static_cast<float>(
                        static_cast<long double>(
                            phase - points[index - 1U].tick) /
                        static_cast<long double>(duration))
                    : 0.0F;
                value = interpolated_value(
                    points[index - 1U].value,
                    points[index].value,
                    amount,
                    event.interpolation);
                break;
            }
            apply_to_target(value);
            break;
        }
        case CompiledAutoloopEventKind::Palette:
        case CompiledAutoloopEventKind::Position:
        case CompiledAutoloopEventKind::Attribute:
            if (!apply_reference(event.reference_index)) {
                output.clear();
                return false;
            }
            break;
        case CompiledAutoloopEventKind::Movement:
        case CompiledAutoloopEventKind::Effect: {
            if (!apply_reference(event.reference_index)) {
                output.clear();
                return false;
            }
            const auto* reference = reference_for(event.reference_index);
            const auto event_duration = static_cast<double>(
                event.end_tick - event.start_tick);
            const auto local = event_duration > 0.0
                ? static_cast<double>(phase - event.start_tick) /
                    event_duration
                : 0.0;
            const auto elapsed_beats =
                static_cast<double>(phase - event.start_tick) /
                static_cast<double>(
                    emberlights::kMusicalTicksPerQuarter);
            const auto rate_delta = static_cast<double>(
                event.generator.rate_end - event.generator.rate_start);
            const auto integrated_cycles = elapsed_beats *
                (static_cast<double>(event.generator.rate_start) +
                 rate_delta * local * 0.5);
            const auto size = event.generator.size_start +
                (event.generator.size_end - event.generator.size_start) *
                    static_cast<float>(local);
            const auto seed_phase = static_cast<double>(
                event.generator.seed & 0xFFFFU) / 65536.0;
            for (std::size_t index = 0U; index < fixtures.size(); ++index) {
                const auto fixture_phase = integrated_cycles +
                    static_cast<double>(event.generator.phase) + seed_phase +
                    static_cast<double>(event.generator.spread) *
                        static_cast<double>(index);
                const auto wave = generator_wave(
                    reference->generator_kind, fixture_phase);
                const auto endpoint = event.generator.base_primary +
                    (event.generator.base_secondary -
                     event.generator.base_primary) * wave;
                const auto value = std::clamp(
                    event.generator.base_primary +
                        (endpoint - event.generator.base_primary) * size,
                    0.0F,
                    1.0F);
                output.set(
                    fixtures[index], event.property,
                    PropertyValue::set(value));
            }
            break;
        }
        case CompiledAutoloopEventKind::Count:
            output.clear();
            return false;
        }
    }
    return true;
}

const char* autoloop_compile_error_name(AutoloopCompileError error) noexcept {
    switch (error) {
    case AutoloopCompileError::None: return "none";
    case AutoloopCompileError::InvalidSource: return "invalidSource";
    case AutoloopCompileError::CapacityExceeded: return "capacityExceeded";
    case AutoloopCompileError::MissingTarget: return "missingTarget";
    case AutoloopCompileError::AmbiguousTarget: return "ambiguousTarget";
    case AutoloopCompileError::InvalidTargetBinding: return "invalidTargetBinding";
    case AutoloopCompileError::MissingCapability: return "missingCapability";
    case AutoloopCompileError::MissingReference: return "missingReference";
    case AutoloopCompileError::AmbiguousReference: return "ambiguousReference";
    case AutoloopCompileError::InvalidReferenceBinding:
        return "invalidReferenceBinding";
    case AutoloopCompileError::UnsupportedPayload: return "unsupportedPayload";
    case AutoloopCompileError::InternalError: return "internalError";
    }
    return "invalid";
}

const char* autoloop_arena_kind_name(AutoloopArenaKind arena) noexcept {
    switch (arena) {
    case AutoloopArenaKind::None: return "none";
    case AutoloopArenaKind::Programs: return "programs";
    case AutoloopArenaKind::TargetSpans: return "targetSpans";
    case AutoloopArenaKind::TargetFixtureIds: return "targetFixtureIds";
    case AutoloopArenaKind::Events: return "events";
    case AutoloopArenaKind::CurvePoints: return "curvePoints";
    case AutoloopArenaKind::References: return "references";
    case AutoloopArenaKind::ReferenceAssignments:
        return "referenceAssignments";
    case AutoloopArenaKind::CanonicalBytes: return "canonicalBytes";
    }
    return "invalid";
}

}  // namespace showcore
