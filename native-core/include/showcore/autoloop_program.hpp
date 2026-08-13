#pragma once

#include "showcore/autoloop.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/look.hpp"
#include "showcore/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {
struct AutoloopSourceDocument;
}

namespace showcore {

inline constexpr std::uint32_t kCompiledAutoloopProgramFormatVersion = 1U;
inline constexpr std::uint32_t kInvalidCompiledAutoloopIndex =
    std::numeric_limits<std::uint32_t>::max();

static_assert(kPropertyCount <= 64U);
static_assert(sizeof(float) == 4U && std::numeric_limits<float>::is_iec559);

[[nodiscard]] constexpr std::uint64_t autoloop_property_mask(
    Property property) noexcept {
    return property < Property::Count
        ? (std::uint64_t{1U} << static_cast<std::uint8_t>(property))
        : 0U;
}

[[nodiscard]] constexpr std::uint64_t all_autoloop_property_mask() noexcept {
    std::uint64_t mask = 0U;
    for (std::size_t index = 0U; index < kPropertyCount; ++index) {
        mask |= std::uint64_t{1U} << index;
    }
    return mask;
}

enum class CompiledAutoloopTargetKind : std::uint8_t {
    Master,
    Group,
    Fixture,
    RoleSelector,
    Count
};

enum class CompiledAutoloopEventKind : std::uint8_t {
    LegacyLook,
    PropertyBlock,
    PropertyCurve,
    Palette,
    Position,
    Attribute,
    Movement,
    Effect,
    Count
};

enum class CompiledAutoloopReferenceKind : std::uint8_t {
    LegacyLook,
    Palette,
    Position,
    Attribute,
    Movement,
    Effect,
    Count
};

enum class CompiledAutoloopInterpolation : std::uint8_t {
    Hold,
    Linear,
    SmoothStep,
    Count
};

enum class CompiledAutoloopLaunchQuantization : std::uint8_t {
    Immediate,
    NextBeat,
    NextBar,
    NextPhrase,
    Count
};

enum class CompiledAutoloopPhaseOrigin : std::uint8_t {
    Launch,
    Track,
    Bar,
    Phrase,
    Count
};

enum class CompiledAutoloopPlaybackMode : std::uint8_t {
    Overlay,
    Replace,
    Count
};

enum class CompiledAutoloopGeneratorKind : std::uint8_t {
    None,
    Circle,
    HorizontalScan,
    VerticalScan,
    Oval,
    FigureEight,
    TripleEight,
    Square,
    Sine,
    Triangle,
    Pulse,
    Count
};

struct AutoloopTargetBinding {
    CompiledAutoloopTargetKind kind{CompiledAutoloopTargetKind::Master};
    // Empty for Master; otherwise the stable group/fixture/role selector ID.
    std::string_view stable_ref;
    std::span<const std::uint16_t> fixture_ids;
    // Intersection of supported properties for every fixture in this binding.
    std::uint64_t supported_property_mask{all_autoloop_property_mask()};
};

struct AutoloopReferenceBinding {
    CompiledAutoloopReferenceKind kind{
        CompiledAutoloopReferenceKind::LegacyLook};
    std::string_view stable_id;
    CompiledAutoloopTargetKind target_kind{
        CompiledAutoloopTargetKind::Master};
    std::string_view target_stable_ref;
    // Studio resolves references to semantic assignments before compilation.
    // These are runtime fixture IDs, not source fixture strings or DMX slots.
    std::span<const LookAssignment> assignments;
    CompiledAutoloopGeneratorKind generator_kind{
        CompiledAutoloopGeneratorKind::None};
    std::uint32_t semantic_version{1U};
};

struct AutoloopCompileEnvironment {
    std::span<const AutoloopTargetBinding> targets;
    std::span<const AutoloopReferenceBinding> references;
};

struct AutoloopCompileLimits {
    std::size_t maximum_programs{kMaxAutoloops};
    std::size_t maximum_target_spans{65536U};
    std::size_t maximum_target_fixture_ids{262144U};
    std::size_t maximum_events{65536U};
    std::size_t maximum_curve_points{262144U};
    std::size_t maximum_references{65536U};
    std::size_t maximum_reference_assignments{262144U};
    std::size_t maximum_canonical_bytes{64U * 1024U * 1024U};
};

enum class AutoloopCompileError : std::uint8_t {
    None,
    InvalidSource,
    CapacityExceeded,
    MissingTarget,
    AmbiguousTarget,
    InvalidTargetBinding,
    MissingCapability,
    MissingReference,
    AmbiguousReference,
    InvalidReferenceBinding,
    UnsupportedPayload,
    InternalError
};

enum class AutoloopArenaKind : std::uint8_t {
    None,
    Programs,
    TargetSpans,
    TargetFixtureIds,
    Events,
    CurvePoints,
    References,
    ReferenceAssignments,
    CanonicalBytes
};

struct AutoloopCompileDiagnostic {
    AutoloopCompileError error{AutoloopCompileError::None};
    AutoloopArenaKind arena{AutoloopArenaKind::None};
    std::string code;
    std::string subject;
    std::string message;
};

using CompiledAutoloopStableKey = std::array<std::uint8_t, 32U>;

struct CompiledAutoloopPlacement {
    std::uint32_t program_index{kInvalidCompiledAutoloopIndex};
    CompiledAutoloopStableKey asset_key{};
    AutoloopRepeat repeat{AutoloopRepeat::Once};
    CompiledAutoloopLaunchQuantization launch{
        CompiledAutoloopLaunchQuantization::Immediate};
    CompiledAutoloopPhaseOrigin phase_origin{
        CompiledAutoloopPhaseOrigin::Launch};
    CompiledAutoloopPlaybackMode mode{
        CompiledAutoloopPlaybackMode::Overlay};
    std::int64_t return_fade_ticks{0};
    bool track_boundary_required{false};

    [[nodiscard]] bool populated() const noexcept {
        return program_index != kInvalidCompiledAutoloopIndex;
    }
};

struct CompiledAutoloopProgramHeader {
    CompiledAutoloopStableKey program_key{};
    std::int64_t length_ticks{0};
    std::uint16_t time_signature_numerator{4U};
    std::uint16_t time_signature_denominator{4U};
    std::uint32_t target_offset{0U};
    std::uint32_t target_count{0U};
    std::uint32_t event_offset{0U};
    std::uint32_t event_count{0U};
};

struct CompiledAutoloopTargetSpan {
    std::uint32_t fixture_offset{0U};
    std::uint32_t fixture_count{0U};
    std::uint64_t required_property_mask{0U};
};

struct CompiledAutoloopReference {
    CompiledAutoloopStableKey reference_key{};
    CompiledAutoloopReferenceKind kind{
        CompiledAutoloopReferenceKind::LegacyLook};
    CompiledAutoloopGeneratorKind generator_kind{
        CompiledAutoloopGeneratorKind::None};
    std::uint32_t semantic_version{1U};
    std::uint32_t assignment_offset{0U};
    std::uint32_t assignment_count{0U};
};

struct CompiledAutoloopCurvePoint {
    std::int64_t tick{0};
    PropertyValue value{};
};

struct CompiledAutoloopGeneratorParameters {
    float rate_start{1.0F};
    float rate_end{1.0F};
    float size_start{1.0F};
    float size_end{1.0F};
    float phase{0.0F};
    float spread{0.0F};
    float base_primary{0.5F};
    float base_secondary{0.5F};
    std::uint64_t seed{0U};
};

struct CompiledAutoloopEvent {
    CompiledAutoloopEventKind kind{
        CompiledAutoloopEventKind::PropertyBlock};
    std::int64_t start_tick{0};
    std::int64_t end_tick{0};
    std::uint32_t target_span_index{0U};
    std::uint16_t lane_priority{0U};
    Property property{Property::Intensity};
    PropertyValue value{};
    CompiledAutoloopInterpolation interpolation{
        CompiledAutoloopInterpolation::Hold};
    std::uint32_t curve_offset{0U};
    std::uint32_t curve_count{0U};
    std::uint32_t reference_index{kInvalidCompiledAutoloopIndex};
    std::uint32_t next_legacy_reference_index{kInvalidCompiledAutoloopIndex};
    std::uint32_t payload_version{1U};
    CompiledAutoloopGeneratorParameters generator{};
    AutoloopTransition legacy_transition{AutoloopTransition::Cut};
};

struct AutoloopCompileResult;

class CompiledAutoloopPackage {
public:
    CompiledAutoloopPackage(const CompiledAutoloopPackage&) = delete;
    CompiledAutoloopPackage& operator=(const CompiledAutoloopPackage&) = delete;
    CompiledAutoloopPackage(CompiledAutoloopPackage&&) = delete;
    CompiledAutoloopPackage& operator=(CompiledAutoloopPackage&&) = delete;

    [[nodiscard]] std::uint32_t format_version() const noexcept {
        return kCompiledAutoloopProgramFormatVersion;
    }
    [[nodiscard]] const CompiledAutoloopPlacement* placement(
        AutoloopAddress address) const noexcept;
    [[nodiscard]] const CompiledAutoloopProgramHeader* program(
        std::size_t index) const noexcept;
    [[nodiscard]] std::span<const CompiledAutoloopProgramHeader> programs()
        const noexcept { return programs_; }
    [[nodiscard]] std::span<const CompiledAutoloopTargetSpan> target_spans()
        const noexcept { return target_spans_; }
    [[nodiscard]] std::span<const std::uint16_t> target_fixture_ids()
        const noexcept { return target_fixture_ids_; }
    [[nodiscard]] std::span<const CompiledAutoloopEvent> events()
        const noexcept { return events_; }
    [[nodiscard]] std::span<const CompiledAutoloopCurvePoint> curve_points()
        const noexcept { return curve_points_; }
    [[nodiscard]] std::span<const CompiledAutoloopReference> references()
        const noexcept { return references_; }
    [[nodiscard]] std::span<const LookAssignment> reference_assignments()
        const noexcept { return reference_assignments_; }
    [[nodiscard]] std::span<const std::uint8_t> canonical_bytes()
        const noexcept { return canonical_bytes_; }
    [[nodiscard]] std::string_view digest() const noexcept { return digest_; }
    [[nodiscard]] std::size_t arena_bytes() const noexcept;

private:
    friend struct AutoloopCompileResult;
    friend AutoloopCompileResult compile_autoloop_programs(
        const emberlights::AutoloopSourceDocument&,
        const AutoloopCompileEnvironment&,
        const AutoloopCompileLimits&);

    CompiledAutoloopPackage();

    std::array<CompiledAutoloopPlacement, kMaxAutoloops> placements_{};
    std::vector<CompiledAutoloopProgramHeader> programs_;
    std::vector<CompiledAutoloopTargetSpan> target_spans_;
    std::vector<std::uint16_t> target_fixture_ids_;
    std::vector<CompiledAutoloopEvent> events_;
    std::vector<CompiledAutoloopCurvePoint> curve_points_;
    std::vector<CompiledAutoloopReference> references_;
    std::vector<LookAssignment> reference_assignments_;
    std::vector<std::uint8_t> canonical_bytes_;
    std::string digest_;
};

struct AutoloopCompileResult {
    std::unique_ptr<const CompiledAutoloopPackage> package;
    std::vector<AutoloopCompileDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept { return package != nullptr; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
};

[[nodiscard]] AutoloopCompileResult compile_autoloop_programs(
    const emberlights::AutoloopSourceDocument& source,
    const AutoloopCompileEnvironment& environment,
    const AutoloopCompileLimits& limits = {});

// Stateful only for fixed-size scratch buffers. Construction and compilation
// may allocate elsewhere; evaluate performs no dynamic allocation.
class AutoloopProgramEvaluator {
public:
    [[nodiscard]] bool evaluate(
        const CompiledAutoloopPackage& package,
        std::size_t program_index,
        std::int64_t tick,
        LayerBuffer& output) noexcept;

private:
    LayerBuffer current_reference_{};
    LayerBuffer next_reference_{};
};

[[nodiscard]] const char* autoloop_compile_error_name(
    AutoloopCompileError error) noexcept;
[[nodiscard]] const char* autoloop_arena_kind_name(
    AutoloopArenaKind arena) noexcept;

}  // namespace showcore
