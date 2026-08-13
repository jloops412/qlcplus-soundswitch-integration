#include "emberlights/autoloop_autoscript.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace emberlights {
namespace {

template <typename Integer>
void append_integer(std::string& output, Integer value) {
    std::array<char, 32U> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    if (converted.ec == std::errc{}) {
        output.append(buffer.data(), converted.ptr);
    }
    output.push_back(';');
}

void append_text(std::string& output, std::string_view value) {
    append_integer(output, value.size());
    output.append(value);
    output.push_back(';');
}

[[nodiscard]] std::string canonical_request(
    const AutoloopAutoscriptRequest& request,
    bool include_seed_and_budgets) {
    std::string output;
    output.reserve(4096U);
    append_text(output, "emberlights.autoscript.request");
    append_integer(output, kAutoloopAutoscriptGeneratorVersion);
    append_integer(output, request.track_duration_ticks);
    append_integer(output, request.loop_length_ticks);
    append_integer(output, request.grid_ticks);
    append_integer(output, static_cast<std::uint8_t>(request.style));
    append_integer(output, static_cast<std::uint8_t>(request.complexity));
    append_integer(output, request.first_placement.bank);
    append_integer(output, request.first_placement.slot);
    append_integer(output, request.musical_sections.size());
    for (const auto& section : request.musical_sections) {
        append_integer(output, section.start_tick);
        append_integer(output, section.end_tick);
        append_integer(output, static_cast<std::uint8_t>(section.kind));
        append_integer(output, section.energy_per_mille);
    }
    append_integer(output, request.energy_bands.size());
    for (const auto& band : request.energy_bands) {
        append_integer(output, band.start_tick);
        append_integer(output, band.end_tick);
        append_integer(output, band.minimum_energy_per_mille);
        append_integer(output, band.maximum_energy_per_mille);
    }
    append_integer(output, request.eligible_role_selectors.size());
    for (const auto& role : request.eligible_role_selectors) {
        append_text(output, role);
    }
    if (include_seed_and_budgets) {
        append_integer(output, request.seed.has_value() ? 1U : 0U);
        append_integer(output, request.seed.value_or(0U));
        append_integer(output, request.content_budget.maximum_generated_assets);
        append_integer(output, request.content_budget.maximum_generated_events);
        append_integer(
            output,
            request.content_budget.maximum_candidate_canonical_bytes);
        append_integer(output, request.operation_budget.maximum_operations);
    }
    return output;
}

[[nodiscard]] std::string proposal_digest(
    AutoloopAutoscriptProposalResult result,
    StudioDocumentGeneration base_generation,
    std::string_view base_source_digest,
    std::string_view request_digest,
    std::string_view preview_source_digest,
    const std::vector<AutoloopAutoscriptDiagnostic>& diagnostics,
    std::size_t generated_event_count,
    std::size_t operations_used) {
    std::string canonical;
    canonical.reserve(1024U);
    append_text(canonical, "emberlights.autoscript.proposal");
    append_integer(canonical, kAutoloopAutoscriptGeneratorVersion);
    append_integer(canonical, static_cast<std::uint8_t>(result));
    append_integer(canonical, base_generation);
    append_text(canonical, base_source_digest);
    append_text(canonical, request_digest);
    append_text(canonical, preview_source_digest);
    append_integer(canonical, generated_event_count);
    append_integer(canonical, operations_used);
    append_integer(canonical, diagnostics.size());
    for (const auto& diagnostic : diagnostics) {
        append_integer(
            canonical, static_cast<std::uint8_t>(diagnostic.severity));
        append_text(canonical, diagnostic.code);
        append_text(canonical, diagnostic.subject);
        append_text(canonical, diagnostic.message);
    }
    return sha256_text(canonical);
}

[[nodiscard]] bool valid_identifier(std::string_view value) noexcept {
    return !value.empty() &&
        value.size() <= kMaximumAutoloopSourceIdentifierLength;
}

[[nodiscard]] std::string decimal_index(std::size_t index) {
    std::array<char, 24U> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), index);
    if (converted.ec != std::errc{}) {
        return {};
    }
    std::string result;
    const auto count = static_cast<std::size_t>(converted.ptr - buffer.data());
    if (count < 2U) {
        result.push_back('0');
    }
    result.append(buffer.data(), converted.ptr);
    return result;
}

[[nodiscard]] const char* style_name(
    AutoloopAutoscriptStyle style) noexcept {
    switch (style) {
    case AutoloopAutoscriptStyle::Subtle: return "subtle";
    case AutoloopAutoscriptStyle::Balanced: return "balanced";
    case AutoloopAutoscriptStyle::ColorMotion: return "color-motion";
    case AutoloopAutoscriptStyle::BuildDrop: return "build-drop";
    case AutoloopAutoscriptStyle::Count: break;
    }
    return "invalid";
}

[[nodiscard]] const char* section_name(
    AutoloopAutoscriptSectionKind kind) noexcept {
    switch (kind) {
    case AutoloopAutoscriptSectionKind::Intro: return "Intro";
    case AutoloopAutoscriptSectionKind::Verse: return "Verse";
    case AutoloopAutoscriptSectionKind::Chorus: return "Chorus";
    case AutoloopAutoscriptSectionKind::Build: return "Build";
    case AutoloopAutoscriptSectionKind::Drop: return "Drop";
    case AutoloopAutoscriptSectionKind::Break: return "Break";
    case AutoloopAutoscriptSectionKind::Outro: return "Outro";
    case AutoloopAutoscriptSectionKind::Custom: return "Custom";
    case AutoloopAutoscriptSectionKind::Count: break;
    }
    return "Invalid";
}

class StableRandom {
public:
    explicit StableRandom(std::uint64_t seed) noexcept : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ += 0x9E3779B97F4A7C15ULL;
        auto value = state_;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        return value ^ (value >> 31U);
    }

    [[nodiscard]] std::uint16_t bounded(
        std::uint16_t minimum,
        std::uint16_t maximum) noexcept {
        if (minimum >= maximum) {
            return minimum;
        }
        const auto width = static_cast<std::uint64_t>(maximum - minimum) + 1U;
        return static_cast<std::uint16_t>(minimum + next() % width);
    }

private:
    std::uint64_t state_{0U};
};

[[nodiscard]] std::uint64_t digest_seed(std::string_view digest) noexcept {
    std::uint64_t value = 0U;
    const auto nibble = [](char character) noexcept -> std::uint8_t {
        if (character >= '0' && character <= '9') {
            return static_cast<std::uint8_t>(character - '0');
        }
        if (character >= 'a' && character <= 'f') {
            return static_cast<std::uint8_t>(character - 'a' + 10);
        }
        return 0U;
    };
    const auto count = std::min<std::size_t>(digest.size(), 16U);
    for (std::size_t index = 0U; index < count; ++index) {
        value = (value << 4U) | nibble(digest[index]);
    }
    return value;
}

struct TimelineSegment {
    MusicalTick start_tick{0};
    MusicalTick end_tick{0};
    AutoloopAutoscriptSectionKind section_kind{
        AutoloopAutoscriptSectionKind::Custom};
    std::uint16_t minimum_energy{0U};
    std::uint16_t maximum_energy{0U};
    bool energy_band{false};
};

[[nodiscard]] std::size_t complexity_cell_limit(
    AutoloopAutoscriptComplexity complexity) noexcept {
    switch (complexity) {
    case AutoloopAutoscriptComplexity::Minimal: return 2U;
    case AutoloopAutoscriptComplexity::Low: return 4U;
    case AutoloopAutoscriptComplexity::Medium: return 8U;
    case AutoloopAutoscriptComplexity::High: return 16U;
    case AutoloopAutoscriptComplexity::Count: break;
    }
    return 0U;
}

[[nodiscard]] std::size_t cell_count_for(
    MusicalTick loop_length,
    MusicalTick grid,
    AutoloopAutoscriptComplexity complexity) noexcept {
    const auto grid_cells = static_cast<std::size_t>(loop_length / grid);
    auto cells = std::min(grid_cells, complexity_cell_limit(complexity));
    while (cells > 1U && grid_cells % cells != 0U) {
        --cells;
    }
    return cells;
}

[[nodiscard]] bool add_without_overflow(
    std::size_t first,
    std::size_t second,
    std::size_t& result) noexcept {
    if (second > std::numeric_limits<std::size_t>::max() - first) {
        return false;
    }
    result = first + second;
    return true;
}

[[nodiscard]] bool multiply_without_overflow(
    std::size_t first,
    std::size_t second,
    std::size_t& result) noexcept {
    if (first != 0U &&
        second > std::numeric_limits<std::size_t>::max() / first) {
        return false;
    }
    result = first * second;
    return true;
}

[[nodiscard]] std::uint16_t adjusted_intensity(
    std::uint16_t energy,
    AutoloopAutoscriptStyle style,
    AutoloopAutoscriptSectionKind section,
    std::size_t cell,
    std::size_t cells,
    StableRandom& random) noexcept {
    const auto jitter = static_cast<int>(random.next() % 161U) - 80;
    auto value = static_cast<int>(energy) + jitter;
    if (style == AutoloopAutoscriptStyle::Subtle) {
        value = std::min(value, 650);
    }
    if (style == AutoloopAutoscriptStyle::BuildDrop) {
        if (section == AutoloopAutoscriptSectionKind::Build) {
            value = value * static_cast<int>(cell + 1U) /
                static_cast<int>(cells);
        } else if (section == AutoloopAutoscriptSectionKind::Drop) {
            value += 150;
        }
    }
    return static_cast<std::uint16_t>(std::clamp(value, 50, 1000));
}

[[nodiscard]] AutoloopAuthoringOutcome failed_commit(
    const AutoloopAuthoringService& service,
    StudioDocumentGeneration expected_generation,
    AutoloopAuthoringResult result,
    std::string message) {
    const auto current = service.snapshot();
    AutoloopAuthoringOutcome outcome;
    outcome.result = result;
    outcome.expected_generation = expected_generation;
    outcome.generation = current.generation;
    outcome.validation = validate_autoloop_source(current.source);
    outcome.message = std::move(message);
    return outcome;
}

}  // namespace

AutoloopAutoscriptProposal propose_autoloop_autoscript(
    const AutoloopAuthoringSnapshot& snapshot,
    AutoloopAutoscriptRequest request,
    const AutoloopAutoscriptCancellationToken* cancellation) {
    AutoloopAutoscriptProposal proposal;
    proposal.base_generation_ = snapshot.generation;
    proposal.base_source_digest_ = snapshot.source_digest;

    std::sort(
        request.eligible_role_selectors.begin(),
        request.eligible_role_selectors.end());
    std::sort(
        request.musical_sections.begin(), request.musical_sections.end(),
        [](const auto& first, const auto& second) {
            return std::tie(first.start_tick, first.end_tick, first.kind,
                            first.energy_per_mille) <
                std::tie(second.start_tick, second.end_tick, second.kind,
                         second.energy_per_mille);
        });
    std::sort(
        request.energy_bands.begin(), request.energy_bands.end(),
        [](const auto& first, const auto& second) {
            return std::tie(
                       first.start_tick, first.end_tick,
                       first.minimum_energy_per_mille,
                       first.maximum_energy_per_mille) <
                std::tie(
                       second.start_tick, second.end_tick,
                       second.minimum_energy_per_mille,
                       second.maximum_energy_per_mille);
        });
    const auto canonical = canonical_request(request, true);
    proposal.request_digest_ = sha256_text(canonical);

    const auto finish = [&]() {
        proposal.proposal_digest_ = proposal_digest(
            proposal.result_, proposal.base_generation_,
            proposal.base_source_digest_, proposal.request_digest_,
            proposal.preview_source_digest_, proposal.diagnostics_,
            proposal.generated_event_count_, proposal.operations_used_);
        return proposal;
    };
    const auto fail = [&](AutoloopAutoscriptProposalResult result,
                          std::string code, std::string subject,
                          std::string message) {
        proposal.result_ = result;
        proposal.preview_source_ = {};
        proposal.preview_source_digest_.clear();
        proposal.generated_asset_ids_.clear();
        proposal.generated_placement_ids_.clear();
        proposal.generated_addresses_.clear();
        proposal.generated_event_count_ = 0U;
        proposal.diagnostics_.push_back({
            AutoloopAutoscriptDiagnosticSeverity::Error,
            std::move(code), std::move(subject), std::move(message)});
        return finish();
    };
    const auto cancelled = [&]() noexcept {
        return cancellation != nullptr &&
            cancellation->cancellation_requested();
    };

    if (cancelled()) {
        return fail(
            AutoloopAutoscriptProposalResult::Cancelled,
            "autoscript.cancelled", "request",
            "AutoScript proposal generation was cancelled before work began.");
    }
    if (request.operation_budget.maximum_operations == 0U ||
        request.operation_budget.maximum_operations >
            kMaximumAutoloopAutoscriptOperations) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.budget.operations.invalid", "operationBudget",
            "The operation budget must be non-zero and within the hard service cap.");
    }
    const auto operation = [&]() {
        if (cancelled()) {
            return AutoloopAutoscriptProposalResult::Cancelled;
        }
        if (proposal.operations_used_ >=
            request.operation_budget.maximum_operations) {
            return AutoloopAutoscriptProposalResult::OperationBudgetExceeded;
        }
        ++proposal.operations_used_;
        return AutoloopAutoscriptProposalResult::Ready;
    };
    const auto work_failure = [&](AutoloopAutoscriptProposalResult result) {
        if (result == AutoloopAutoscriptProposalResult::Cancelled) {
            return fail(
                result, "autoscript.cancelled", "request",
                "AutoScript proposal generation was cancelled at a cooperative checkpoint.");
        }
        return fail(
            result, "autoscript.budget.operations.exceeded", "operationBudget",
            "AutoScript proposal generation exhausted its operation budget.");
    };

    if (request.content_budget.maximum_generated_assets == 0U ||
        request.content_budget.maximum_generated_assets >
            kMaximumAutoloopAutoscriptGeneratedAssets ||
        request.content_budget.maximum_generated_events == 0U ||
        request.content_budget.maximum_generated_events >
            kMaximumAutoloopAutoscriptGeneratedEvents ||
        request.content_budget.maximum_candidate_canonical_bytes == 0U ||
        request.content_budget.maximum_candidate_canonical_bytes >
            kMaximumAutoloopAutoscriptCanonicalBytes) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.budget.content.invalid", "contentBudget",
            "Content budgets must be non-zero and within hard service caps.");
    }
    if (snapshot.generation == 0U ||
        !is_sha256_digest(snapshot.source_digest) ||
        autoloop_source_digest(snapshot.source) != snapshot.source_digest ||
        !validate_autoloop_source(snapshot.source).ok()) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidBaseSource,
            "autoscript.base.invalid", "source",
            "The authoring snapshot is not a valid canonical source generation.");
    }
    if (!request.seed.has_value()) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.seed.required", "seed",
            "AutoScript requires an explicit stable seed, including when the intended seed is zero.");
    }
    if (request.style >= AutoloopAutoscriptStyle::Count ||
        request.complexity >= AutoloopAutoscriptComplexity::Count ||
        request.track_duration_ticks <= 0 ||
        request.track_duration_ticks > kMaximumAutoloopAutoscriptTrackTicks ||
        request.loop_length_ticks <= 0 ||
        request.loop_length_ticks > request.track_duration_ticks ||
        request.grid_ticks <= 0 ||
        request.grid_ticks > request.loop_length_ticks ||
        request.loop_length_ticks % request.grid_ticks != 0) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.musicalTime.invalid", "timeline",
            "Track, loop, grid, style, or complexity values are invalid or outside bounded 960-PPQ musical time.");
    }
    const bool has_sections = !request.musical_sections.empty();
    const bool has_bands = !request.energy_bands.empty();
    if (has_sections == has_bands) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.timeline.exclusive", "timeline",
            "Exactly one musical-section or energy-band timeline is required.");
    }
    const auto segment_count = has_sections
        ? request.musical_sections.size()
        : request.energy_bands.size();
    if (segment_count > kMaximumAutoloopAutoscriptSegments) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.timeline.capacity", "timeline",
            "The timeline exceeds the hard AutoScript segment cap.");
    }
    if (request.eligible_role_selectors.size() >
        kMaximumAutoloopAutoscriptRoleSelectors ||
        std::adjacent_find(
            request.eligible_role_selectors.begin(),
            request.eligible_role_selectors.end()) !=
            request.eligible_role_selectors.end() ||
        std::any_of(
            request.eligible_role_selectors.begin(),
            request.eligible_role_selectors.end(),
            [](const auto& role) { return !valid_identifier(role); })) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.targets.invalid", "eligibleRoleSelectors",
            "Role selectors must be unique, stable, bounded semantic identifiers.");
    }
    if (!request.first_placement.valid()) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.placement.invalid", "firstPlacement",
            "The first placement is outside the 64 by 32 catalog.");
    }
    if (canonical.size() > kMaximumAutoloopSourceTextLength) {
        return fail(
            AutoloopAutoscriptProposalResult::InvalidRequest,
            "autoscript.parameters.capacity", "request",
            "The normalized parameter record exceeds the source text cap.");
    }

    std::vector<TimelineSegment> segments;
    segments.reserve(segment_count);
    MusicalTick previous_end = -1;
    if (has_sections) {
        for (const auto& section : request.musical_sections) {
            const auto worked = operation();
            if (worked != AutoloopAutoscriptProposalResult::Ready) {
                return work_failure(worked);
            }
            if (section.start_tick < 0 ||
                section.end_tick <= section.start_tick ||
                section.end_tick > request.track_duration_ticks ||
                section.start_tick < previous_end ||
                section.kind >= AutoloopAutoscriptSectionKind::Count ||
                section.energy_per_mille > 1000U) {
                return fail(
                    AutoloopAutoscriptProposalResult::InvalidRequest,
                    "autoscript.section.invalid", "musicalSections",
                    "Musical sections must be ordered, non-overlapping, bounded, and carry valid energy.");
            }
            segments.push_back({
                section.start_tick, section.end_tick, section.kind,
                section.energy_per_mille, section.energy_per_mille, false});
            previous_end = section.end_tick;
        }
    } else {
        for (const auto& band : request.energy_bands) {
            const auto worked = operation();
            if (worked != AutoloopAutoscriptProposalResult::Ready) {
                return work_failure(worked);
            }
            if (band.start_tick < 0 || band.end_tick <= band.start_tick ||
                band.end_tick > request.track_duration_ticks ||
                band.start_tick < previous_end ||
                band.minimum_energy_per_mille >
                    band.maximum_energy_per_mille ||
                band.maximum_energy_per_mille > 1000U) {
                return fail(
                    AutoloopAutoscriptProposalResult::InvalidRequest,
                    "autoscript.energyBand.invalid", "energyBands",
                    "Energy bands must be ordered, non-overlapping, bounded, and use a valid energy range.");
            }
            segments.push_back({
                band.start_tick, band.end_tick,
                AutoloopAutoscriptSectionKind::Custom,
                band.minimum_energy_per_mille,
                band.maximum_energy_per_mille, true});
            previous_end = band.end_tick;
        }
    }

    const auto role_count = request.eligible_role_selectors.empty()
        ? 1U
        : request.eligible_role_selectors.size();
    const auto cells = cell_count_for(
        request.loop_length_ticks, request.grid_ticks, request.complexity);
    std::size_t events_per_segment = 0U;
    std::size_t generated_events = 0U;
    if (cells == 0U ||
        !multiply_without_overflow(role_count, cells, events_per_segment) ||
        !multiply_without_overflow(events_per_segment, 4U,
                                   events_per_segment) ||
        !multiply_without_overflow(segment_count, events_per_segment,
                                   generated_events)) {
        return fail(
            AutoloopAutoscriptProposalResult::ContentBudgetExceeded,
            "autoscript.content.overflow", "contentBudget",
            "Generated content dimensions overflow the bounded service model.");
    }
    if (segment_count > request.content_budget.maximum_generated_assets ||
        generated_events > request.content_budget.maximum_generated_events) {
        return fail(
            AutoloopAutoscriptProposalResult::ContentBudgetExceeded,
            "autoscript.content.generated.exceeded", "contentBudget",
            "The requested timeline exceeds its generated asset or event budget.");
    }
    std::size_t candidate_events = generated_events;
    for (const auto& program : snapshot.source.programs) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        if (!add_without_overflow(
                candidate_events, program.events.size(), candidate_events)) {
            return fail(
                AutoloopAutoscriptProposalResult::ContentBudgetExceeded,
                "autoscript.content.candidate.overflow", "source",
                "The candidate event count overflows the bounded service model.");
        }
    }
    if (candidate_events > kMaximumAutoloopAutoscriptCandidateEvents) {
        return fail(
            AutoloopAutoscriptProposalResult::ContentBudgetExceeded,
            "autoscript.content.candidate.exceeded", "source",
            "The complete candidate exceeds the hard AutoScript event cap.");
    }
    if (snapshot.source.assets.size() + segment_count >
            kMaximumAutoloopAuthoringAssets ||
        snapshot.source.programs.size() + segment_count >
            kMaximumAutoloopAuthoringPrograms ||
        snapshot.source.launch_profiles.size() + segment_count >
            kMaximumAutoloopAuthoringLaunchProfiles ||
        snapshot.source.provenance.size() + segment_count >
            kMaximumAutoloopAuthoringProvenanceRecords ||
        snapshot.source.placements.size() + segment_count >
            showcore::kMaxAutoloops) {
        return fail(
            AutoloopAutoscriptProposalResult::CapacityExceeded,
            "autoscript.source.capacity", "source",
            "The canonical authoring source has insufficient record capacity.");
    }
    const auto first_index =
        static_cast<std::size_t>(request.first_placement.bank) *
            showcore::kAutoloopsPerBank +
        request.first_placement.slot;
    if (segment_count > showcore::kMaxAutoloops - first_index) {
        return fail(
            AutoloopAutoscriptProposalResult::CapacityExceeded,
            "autoscript.placement.capacity", "firstPlacement",
            "The contiguous proposal placements extend beyond the catalog.");
    }

    std::array<bool, showcore::kMaxAutoloops> occupied{};
    std::unordered_set<std::string_view> asset_ids;
    std::unordered_set<std::string_view> placement_ids;
    std::unordered_set<std::string_view> program_ids;
    std::unordered_set<std::string_view> launch_ids;
    std::unordered_set<std::string_view> provenance_ids;
    for (const auto& value : snapshot.source.assets) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        asset_ids.insert(value.id);
    }
    for (const auto& value : snapshot.source.placements) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        placement_ids.insert(value.id);
        const auto index = static_cast<std::size_t>(value.bank) *
            showcore::kAutoloopsPerBank + value.slot;
        if (index < occupied.size()) {
            occupied[index] = true;
        }
    }
    for (const auto& value : snapshot.source.programs) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        program_ids.insert(value.id);
    }
    for (const auto& value : snapshot.source.launch_profiles) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        launch_ids.insert(value.id);
    }
    for (const auto& value : snapshot.source.provenance) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        provenance_ids.insert(value.id);
    }
    const auto identity_digest = sha256_text(canonical_request(request, false));
    const auto stable_prefix =
        std::string("autoscript.v1.") + identity_digest.substr(0U, 20U);
    for (std::size_t index = 0U; index < segment_count; ++index) {
        const auto item_prefix = stable_prefix + "." + decimal_index(index);
        if (asset_ids.contains(item_prefix + ".asset") ||
            placement_ids.contains(item_prefix + ".placement") ||
            program_ids.contains(item_prefix + ".program") ||
            launch_ids.contains(item_prefix + ".launch") ||
            provenance_ids.contains(item_prefix + ".provenance")) {
            return fail(
                AutoloopAutoscriptProposalResult::StableIdConflict,
                "autoscript.identity.conflict", item_prefix,
                "A generated stable ID already belongs to canonical source content.");
        }
    }
    for (std::size_t index = 0U; index < segment_count; ++index) {
        if (occupied[first_index + index]) {
            return fail(
                AutoloopAutoscriptProposalResult::OccupiedPlacement,
                "autoscript.placement.occupied",
                decimal_index(first_index + index),
                "A requested proposal address is occupied; AutoScript never overwrites or skips unrelated content.");
        }
    }

    auto candidate = snapshot.source;
    candidate.assets.reserve(candidate.assets.size() + segment_count);
    candidate.placements.reserve(candidate.placements.size() + segment_count);
    candidate.programs.reserve(candidate.programs.size() + segment_count);
    candidate.launch_profiles.reserve(
        candidate.launch_profiles.size() + segment_count);
    candidate.provenance.reserve(candidate.provenance.size() + segment_count);
    proposal.generated_asset_ids_.reserve(segment_count);
    proposal.generated_placement_ids_.reserve(segment_count);
    proposal.generated_addresses_.reserve(segment_count);

    StableRandom random(*request.seed ^ digest_seed(identity_digest));
    constexpr std::array<std::array<std::uint16_t, 3U>, 8U> colors{{
        {{1000U, 120U, 80U}},
        {{1000U, 500U, 80U}},
        {{900U, 900U, 120U}},
        {{100U, 1000U, 300U}},
        {{80U, 650U, 1000U}},
        {{240U, 180U, 1000U}},
        {{850U, 150U, 1000U}},
        {{1000U, 180U, 650U}}
    }};
    constexpr std::array<showcore::Property, 4U> properties{{
        showcore::Property::Intensity,
        showcore::Property::Red,
        showcore::Property::Green,
        showcore::Property::Blue
    }};
    const auto grid_cells = static_cast<std::size_t>(
        request.loop_length_ticks / request.grid_ticks);
    const auto grids_per_cell = grid_cells / cells;

    for (std::size_t index = 0U; index < segments.size(); ++index) {
        const auto worked = operation();
        if (worked != AutoloopAutoscriptProposalResult::Ready) {
            return work_failure(worked);
        }
        const auto& segment = segments[index];
        const auto item_index = decimal_index(index);
        const auto item_prefix = stable_prefix + "." + item_index;
        const auto asset_id = item_prefix + ".asset";
        const auto placement_id = item_prefix + ".placement";
        const auto program_id = item_prefix + ".program";
        const auto launch_id = item_prefix + ".launch";
        const auto provenance_id = item_prefix + ".provenance";

        AutoloopAssetDefinition asset;
        asset.id = asset_id;
        asset.name = std::string("AutoScript ") +
            (segment.energy_band ? "Energy Band " :
                                   section_name(segment.section_kind) +
                                       std::string(" ")) +
            item_index;
        asset.description =
            "Deterministic original EmberLights semantic AutoScript proposal.";
        asset.tags = {
            "autoscript",
            segment.energy_band ? "energy-band" : "musical-section"};
        asset.style = style_name(request.style);
        asset.energy = static_cast<float>(
            (static_cast<std::uint32_t>(segment.minimum_energy) +
             segment.maximum_energy) / 2U) / 1000.0F;
        asset.program_id = program_id;
        asset.launch_profile_id = launch_id;
        asset.provenance_id = provenance_id;
        asset.revision = 1U;

        AutoloopProgramDefinition program;
        program.id = program_id;
        program.length_ticks = request.loop_length_ticks;
        program.time_signature_numerator = 4U;
        program.time_signature_denominator = 4U;
        for (std::size_t role_index = 0U; role_index < role_count;
             ++role_index) {
            const auto role_suffix = decimal_index(role_index);
            const auto target_id = item_prefix + ".target." + role_suffix;
            program.targets.push_back({
                target_id,
                request.eligible_role_selectors.empty()
                    ? AutoloopTargetKind::Master
                    : AutoloopTargetKind::RoleSelector,
                request.eligible_role_selectors.empty()
                    ? std::string{}
                    : request.eligible_role_selectors[role_index],
                std::vector<showcore::Property>(
                    properties.begin(), properties.end())});
            const auto lane_id = item_prefix + ".lane." + role_suffix;
            program.lanes.push_back({lane_id, target_id, 0U});

            for (std::size_t cell = 0U; cell < cells; ++cell) {
                const auto color = colors[random.next() % colors.size()];
                const auto energy = random.bounded(
                    segment.minimum_energy, segment.maximum_energy);
                const auto intensity = adjusted_intensity(
                    energy, request.style, segment.section_kind,
                    cell, cells, random);
                auto color_scale = static_cast<std::uint16_t>(
                    std::min<std::uint32_t>(1000U, 450U + energy / 2U));
                if (request.style == AutoloopAutoscriptStyle::Subtle) {
                    color_scale = std::min<std::uint16_t>(color_scale, 700U);
                }
                const std::array<std::uint16_t, 4U> values{{
                    intensity,
                    static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(color[0]) * color_scale /
                        1000U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(color[1]) * color_scale /
                        1000U),
                    static_cast<std::uint16_t>(
                        static_cast<std::uint32_t>(color[2]) * color_scale /
                        1000U)
                }};
                const auto start_tick = static_cast<MusicalTick>(
                    cell * grids_per_cell) * request.grid_ticks;
                const auto end_tick = cell + 1U == cells
                    ? request.loop_length_ticks
                    : static_cast<MusicalTick>(
                          (cell + 1U) * grids_per_cell) * request.grid_ticks;
                for (std::size_t property_index = 0U;
                     property_index < properties.size(); ++property_index) {
                    const auto event_work = operation();
                    if (event_work != AutoloopAutoscriptProposalResult::Ready) {
                        return work_failure(event_work);
                    }
                    AutoloopEventDefinition event;
                    event.id = item_prefix + ".event." + role_suffix + "." +
                        decimal_index(cell) + "." +
                        decimal_index(property_index);
                    event.lane_id = lane_id;
                    event.kind = AutoloopEventKind::PropertyBlock;
                    event.start_tick = start_tick;
                    event.end_tick = end_tick;
                    event.property = properties[property_index];
                    event.value = showcore::PropertyValue::set(
                        static_cast<float>(values[property_index]) / 1000.0F);
                    event.interpolation =
                        request.style == AutoloopAutoscriptStyle::ColorMotion ||
                            request.style ==
                                AutoloopAutoscriptStyle::BuildDrop
                        ? AutoloopInterpolation::Linear
                        : AutoloopInterpolation::Hold;
                    program.events.push_back(std::move(event));
                }
            }
        }

        AutoloopLaunchProfileDefinition launch;
        launch.id = launch_id;
        launch.repeat = showcore::AutoloopRepeat::Infinite;
        launch.launch = AutoloopLaunchQuantization::Immediate;
        launch.phase_origin = AutoloopPhaseOrigin::Launch;
        launch.mode = AutoloopPlaybackMode::Overlay;

        AutoloopProvenanceDefinition provenance;
        provenance.id = provenance_id;
        provenance.origin = AutoloopProvenanceOrigin::Generated;
        provenance.producer_id = std::string(kAutoloopAutoscriptGeneratorId);
        provenance.producer_version =
            std::to_string(kAutoloopAutoscriptGeneratorVersion);
        provenance.seed = *request.seed;
        provenance.source_bundle_id = proposal.request_digest_;
        provenance.source_artifact_id = canonical;
        provenance.source_object_key =
            (segment.energy_band ? "energy-band/" : "musical-section/") +
            item_index;
        provenance.evidence_status = "generated.deterministic";

        const auto address_index = first_index + index;
        const showcore::AutoloopAddress address{
            static_cast<std::uint16_t>(
                address_index / showcore::kAutoloopsPerBank),
            static_cast<std::uint8_t>(
                address_index % showcore::kAutoloopsPerBank)};
        candidate.assets.push_back(std::move(asset));
        candidate.programs.push_back(std::move(program));
        candidate.launch_profiles.push_back(std::move(launch));
        candidate.provenance.push_back(std::move(provenance));
        candidate.placements.push_back({
            placement_id, address.bank, address.slot, asset_id,
            "emberlights.autoscript/v1/" + identity_digest.substr(0U, 20U) +
                "/" + item_index});
        proposal.generated_asset_ids_.push_back(asset_id);
        proposal.generated_placement_ids_.push_back(placement_id);
        proposal.generated_addresses_.push_back(address);
    }

    const auto canonicalization_work = operation();
    if (canonicalization_work != AutoloopAutoscriptProposalResult::Ready) {
        return work_failure(canonicalization_work);
    }
    normalize_autoloop_source(candidate);
    const auto validation = validate_autoloop_source(candidate);
    if (!validation.ok()) {
        for (const auto& issue : validation.issues) {
            proposal.diagnostics_.push_back({
                issue.severity == AutoloopSourceIssueSeverity::Error
                    ? AutoloopAutoscriptDiagnosticSeverity::Error
                    : AutoloopAutoscriptDiagnosticSeverity::Warning,
                issue.code, issue.subject, issue.message});
        }
        proposal.result_ = AutoloopAutoscriptProposalResult::ValidationFailed;
        return finish();
    }
    const auto serialized = serialize_autoloop_source(candidate);
    if (serialized.empty()) {
        return fail(
            AutoloopAutoscriptProposalResult::ValidationFailed,
            "autoscript.candidate.serialize", "source",
            "The generated source candidate could not be canonicalized.");
    }
    if (serialized.size() >
        request.content_budget.maximum_candidate_canonical_bytes) {
        return fail(
            AutoloopAutoscriptProposalResult::ContentBudgetExceeded,
            "autoscript.content.bytes.exceeded", "contentBudget",
            "The canonical proposal exceeds its byte budget.");
    }
    const auto publication_work = operation();
    if (publication_work != AutoloopAutoscriptProposalResult::Ready) {
        return work_failure(publication_work);
    }

    proposal.result_ = AutoloopAutoscriptProposalResult::Ready;
    proposal.generated_event_count_ = generated_events;
    proposal.preview_source_digest_ = sha256_text(serialized);
    proposal.preview_source_ = std::move(candidate);
    if (request.eligible_role_selectors.empty()) {
        proposal.diagnostics_.push_back({
            AutoloopAutoscriptDiagnosticSeverity::Information,
            "autoscript.target.master", "eligibleRoleSelectors",
            "No role selector was supplied; generated programs use the semantic Master target."});
    }
    proposal.diagnostics_.push_back({
        AutoloopAutoscriptDiagnosticSeverity::Information,
        "autoscript.proposal.ready", proposal.preview_source_digest_,
        "The immutable semantic source proposal is ready for output-disabled preview and explicit commit."});
    return finish();
}

AutoloopAuthoringOutcome apply_autoloop_autoscript_proposal(
    AutoloopAuthoringService& service,
    const AutoloopAutoscriptProposal& proposal) {
    if (!proposal.ready()) {
        return failed_commit(
            service, proposal.base_generation_,
            AutoloopAuthoringResult::InvalidCandidate,
            "Only a ready AutoScript proposal can be committed.");
    }
    const auto current = service.snapshot();
    if (current.generation != proposal.base_generation_ ||
        current.source_digest != proposal.base_source_digest_) {
        return failed_commit(
            service, proposal.base_generation_,
            AutoloopAuthoringResult::StaleGeneration,
            "The Autoloop source changed after the AutoScript preview was created.");
    }
    const auto serialized = serialize_autoloop_source(proposal.preview_source_);
    if (serialized.empty() || sha256_text(serialized) !=
            proposal.preview_source_digest_ ||
        proposal_digest(
            proposal.result_, proposal.base_generation_,
            proposal.base_source_digest_, proposal.request_digest_,
            proposal.preview_source_digest_, proposal.diagnostics_,
            proposal.generated_event_count_, proposal.operations_used_) !=
            proposal.proposal_digest_) {
        return failed_commit(
            service, proposal.base_generation_,
            AutoloopAuthoringResult::InvalidCandidate,
            "The AutoScript proposal failed its immutable digest check.");
    }
    return service.apply_candidate(
        proposal.base_generation_, proposal.preview_source_);
}

const char* autoloop_autoscript_proposal_result_name(
    AutoloopAutoscriptProposalResult result) noexcept {
    switch (result) {
    case AutoloopAutoscriptProposalResult::Ready: return "ready";
    case AutoloopAutoscriptProposalResult::InvalidRequest:
        return "invalidRequest";
    case AutoloopAutoscriptProposalResult::Cancelled: return "cancelled";
    case AutoloopAutoscriptProposalResult::OperationBudgetExceeded:
        return "operationBudgetExceeded";
    case AutoloopAutoscriptProposalResult::ContentBudgetExceeded:
        return "contentBudgetExceeded";
    case AutoloopAutoscriptProposalResult::CapacityExceeded:
        return "capacityExceeded";
    case AutoloopAutoscriptProposalResult::OccupiedPlacement:
        return "occupiedPlacement";
    case AutoloopAutoscriptProposalResult::StableIdConflict:
        return "stableIdConflict";
    case AutoloopAutoscriptProposalResult::InvalidBaseSource:
        return "invalidBaseSource";
    case AutoloopAutoscriptProposalResult::ValidationFailed:
        return "validationFailed";
    }
    return "unknown";
}

}  // namespace emberlights
