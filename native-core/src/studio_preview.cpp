#include "emberlights/studio_preview.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

[[nodiscard]] float normalized(float value) noexcept {
    return std::clamp(std::isfinite(value) ? value : 0.0F, 0.0F, 1.0F);
}

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == profile_id; });
    return profile == project.fixture_profiles.end() ? nullptr : &*profile;
}

[[nodiscard]] bool profile_has_property(
    const FixtureProfileDefinition* profile,
    showcore::Property property) noexcept {
    if (profile == nullptr) {
        return false;
    }
    return std::any_of(profile->channels.begin(), profile->channels.end(), [&](const auto& channel) {
        return channel.encoding != showcore::ChannelEncoding::Constant8 &&
            channel.property == property;
    });
}

void configure_preview_safety(
    showcore::SafetyPolicy& preview,
    const SafetySettings& source) noexcept {
    preview.strobe_allowed = source.strobe_allowed;
    preview.max_strobe = source.max_strobe;
    preview.max_intensity = source.max_intensity;
    preview.fog_armed = !source.fog_requires_arm;
    preview.haze_armed = !source.haze_requires_arm;
    preview.laser_armed = !source.laser_requires_arm;
    preview.spark_armed = !source.spark_requires_arm;
}

[[nodiscard]] std::string frame_digest(const showcore::DmxFrames& frames) {
    std::array<std::uint8_t,
        showcore::kV1UniverseCount * showcore::kUniverseSlots> bytes{};
    std::size_t offset = 0U;
    for (const auto& universe : frames.universes) {
        std::copy(universe.begin(), universe.end(), bytes.begin() + offset);
        offset += universe.size();
    }
    return sha256_bytes(bytes);
}

[[nodiscard]] std::vector<std::size_t> target_fixture_indices(
    const ProjectDocument& project,
    std::string_view target_id) {
    std::vector<std::size_t> indices;
    for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
        if (project.fixtures[index].id == target_id) {
            indices.push_back(index);
            return indices;
        }
    }
    const auto group = std::find_if(
        project.groups.begin(), project.groups.end(),
        [&](const auto& candidate) { return candidate.id == target_id; });
    if (group == project.groups.end()) {
        return indices;
    }
    std::unordered_set<std::string_view> requested;
    requested.reserve(group->fixture_ids.size());
    for (const auto& fixture_id : group->fixture_ids) {
        requested.insert(fixture_id);
    }
    for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
        if (requested.contains(project.fixtures[index].id)) {
            indices.push_back(index);
        }
    }
    return indices;
}

[[nodiscard]] float resolved_value(
    showcore::Engine& engine,
    std::uint16_t fixture_id,
    showcore::Property property) noexcept {
    const auto resolved = engine.layers().resolve_safe(fixture_id, property, engine.safety());
    return resolved.owned ? normalized(resolved.value) : 0.0F;
}

[[nodiscard]] StudioRgbColor preview_display_rgb(
    const StudioColor& emitted,
    float intensity) noexcept {
    auto red = emitted.rgb.red + emitted.white + emitted.amber +
        emitted.lime * 0.75F + emitted.indigo * 0.29F + emitted.uv * 0.35F;
    auto green = emitted.rgb.green + emitted.white + emitted.amber * 0.55F +
        emitted.lime + emitted.uv * 0.05F;
    auto blue = emitted.rgb.blue + emitted.white + emitted.indigo * 0.51F +
        emitted.uv * 0.75F;
    return {
        normalized(red * intensity),
        normalized(green * intensity),
        normalized(blue * intensity)};
}

}  // namespace

StudioPreviewOutcome StudioPreviewService::load(
    const StudioDocumentSnapshot& document_snapshot) {
    if (document_snapshot.generation == 0U) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "Studio preview requires a nonzero document generation.");
    }
    if (generation_ != 0U && document_snapshot.generation < generation_) {
        return outcome(
            StudioPreviewResult::StaleGeneration,
            "A newer document generation is already loaded in Studio preview.");
    }
    if (show_ != nullptr && document_snapshot.generation == generation_) {
        return outcome(StudioPreviewResult::NoChange);
    }

    auto compilation = compile_project(document_snapshot.document);
    if (!compilation) {
        StudioPreviewOutcome failed;
        failed.result = StudioPreviewResult::CompilationFailed;
        failed.generation = generation_;
        failed.validation = std::move(compilation.validation);
        failed.message = "The candidate did not compile, so the previous preview remains active.";
        return failed;
    }

    configure_preview_safety(
        compilation.show->engine().safety(), document_snapshot.document.safety);

    reset_players_and_layers();
    reset_v2_source();
    project_ = document_snapshot.document;
    show_ = std::move(compilation.show);
    generation_ = document_snapshot.generation;
    snapshot_ = {};
    snapshot_.generation = generation_;
    snapshot_.validation = std::move(compilation.validation);
    render_snapshot(0U, 0.0);
    return outcome(StudioPreviewResult::Loaded);
}

StudioPreviewOutcome StudioPreviewService::preview_look(
    StudioDocumentGeneration expected_generation,
    std::string_view look_id,
    std::uint64_t now_ms,
    bool respect_authored_fade) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded, "Load a document before previewing content.");
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration, "Preview command used a stale document.");
    }
    const auto source = std::find_if(
        project_.looks.begin(), project_.looks.end(),
        [&](const auto& candidate) { return candidate.id == look_id; });
    if (source == project_.looks.end()) {
        return outcome(StudioPreviewResult::MissingContent, "Static Look was not found.");
    }
    const auto index = static_cast<std::size_t>(source - project_.looks.begin());
    const auto* look = show_->look(index);
    if (look == nullptr) {
        return outcome(StudioPreviewResult::MissingContent, "Compiled Static Look was not found.");
    }

    reset_players_and_layers();
    const auto triggered = look_player_.trigger(
        *look,
        now_ms,
        respect_authored_fade ? show_->look_fade_ms(index) : 0U,
        show_->engine().layers());
    if (!triggered) {
        return outcome(StudioPreviewResult::MissingContent, "Static Look failed preview validation.");
    }
    snapshot_.content_kind = StudioPreviewContentKind::StaticLook;
    snapshot_.realization = StudioPreviewRealization::Exact;
    snapshot_.content_id = std::string(look_id);
    render_snapshot(now_ms, snapshot_.beat_position);
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::preview_autoloop(
    StudioDocumentGeneration expected_generation,
    std::string_view autoloop_id,
    double beat_position) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded, "Load a document before previewing content.");
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration, "Preview command used a stale document.");
    }
    if (!std::isfinite(beat_position)) {
        return outcome(StudioPreviewResult::InvalidArgument, "Beat position must be finite.");
    }
    const auto source = std::find_if(
        project_.autoloops.begin(), project_.autoloops.end(),
        [&](const auto& candidate) { return candidate.id == autoloop_id; });
    if (source == project_.autoloops.end()) {
        return outcome(StudioPreviewResult::MissingContent, "Autoloop was not found.");
    }
    const showcore::AutoloopAddress address{source->bank, source->slot};

    reset_players_and_layers();
    if (!autoloop_player_.trigger(
            show_->autoloops(),
            address,
            showcore::AutoloopRepeat::Infinite,
            0.0,
            true,
            show_->engine().layers())) {
        return outcome(StudioPreviewResult::MissingContent, "Autoloop failed preview validation.");
    }
    autoloop_player_.tick(beat_position, true, show_->engine().layers());
    snapshot_.content_kind = StudioPreviewContentKind::Autoloop;
    snapshot_.autoloop_format = StudioPreviewAutoloopFormat::Format1;
    snapshot_.realization = StudioPreviewRealization::Exact;
    snapshot_.content_id = std::string(autoloop_id);
    render_snapshot(snapshot_.now_ms, beat_position);
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::load_autoloop_v2(
    const StudioDocumentSnapshot& document_snapshot,
    const AutoloopAuthoringSnapshot& source_snapshot) {
    std::string canonical_source_digest;
    const auto candidate_outcome = [&](StudioPreviewResult result,
                                       std::string message) {
        auto value = outcome(result, std::move(message));
        value.source_generation = source_snapshot.generation;
        value.source_digest = canonical_source_digest.empty()
            ? source_snapshot.source_digest
            : canonical_source_digest;
        return value;
    };

    if (document_snapshot.generation == 0U || source_snapshot.generation == 0U) {
        return candidate_outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview requires nonzero document and source generations.");
    }
    if (generation_ != 0U && document_snapshot.generation < generation_) {
        return candidate_outcome(
            StudioPreviewResult::StaleGeneration,
            "A newer document generation is already loaded in Studio preview.");
    }
    if (document_snapshot.generation == generation_ &&
        autoloop_v2_source_generation_ != 0U &&
        source_snapshot.generation < autoloop_v2_source_generation_) {
        return candidate_outcome(
            StudioPreviewResult::StaleGeneration,
            "A newer Autoloop source generation is already loaded in Studio preview.");
    }

    canonical_source_digest =
        autoloop_source_digest(source_snapshot.source);
    if (!canonical_source_digest.empty() &&
        source_snapshot.source_digest != canonical_source_digest) {
        auto failed = candidate_outcome(
            StudioPreviewResult::InvalidArgument,
            "The source digest does not match the canonical V2 source snapshot.");
        failed.realization = StudioPreviewRealization::Unsupported;
        failed.autoloop_diagnostics.push_back({
            showcore::AutoloopCompileError::InvalidSource,
            showcore::AutoloopArenaKind::None,
            "autoloop.preview.sourceDigestMismatch",
            "source",
            "The generation-stamped source digest does not match canonical source bytes."});
        return failed;
    }
    if (show_ != nullptr && autoloop_v2_package_ != nullptr &&
        document_snapshot.generation == generation_ &&
        source_snapshot.generation == autoloop_v2_source_generation_) {
        if (canonical_source_digest == autoloop_v2_source_digest_) {
            return outcome(StudioPreviewResult::NoChange);
        }
        return candidate_outcome(
            StudioPreviewResult::StaleGeneration,
            "Autoloop source bytes changed without advancing their generation.");
    }
    const auto source_validation =
        validate_autoloop_source(source_snapshot.source);
    if (!source_validation.ok()) {
        auto failed = candidate_outcome(
            StudioPreviewResult::CompilationFailed,
            "The canonical V2 source is invalid; the previous preview remains active.");
        failed.realization = StudioPreviewRealization::Unsupported;
        const auto& issue = source_validation.issues.front();
        failed.autoloop_diagnostics.push_back({
            showcore::AutoloopCompileError::InvalidSource,
            showcore::AutoloopArenaKind::None,
            issue.code,
            issue.subject,
            issue.message});
        return failed;
    }

    auto project_compilation = compile_project(document_snapshot.document);
    if (!project_compilation) {
        auto failed = candidate_outcome(
            StudioPreviewResult::CompilationFailed,
            "The candidate project did not compile, so the previous preview remains active.");
        failed.validation = std::move(project_compilation.validation);
        return failed;
    }

    AutoloopPaletteCompileEnvironment palette_environment(
        document_snapshot.document, source_snapshot.source);
    std::vector<AutoloopPaletteResolution> palette_resolutions(
        palette_environment.resolutions().begin(),
        palette_environment.resolutions().end());
    if (!palette_environment.ok()) {
        auto failed = candidate_outcome(
            StudioPreviewResult::CompilationFailed,
            "The V2 Palette references could not be realized safely; the previous preview remains active.");
        failed.validation = std::move(project_compilation.validation);
        failed.realization = StudioPreviewRealization::Unsupported;
        failed.palette_resolutions = std::move(palette_resolutions);
        failed.autoloop_diagnostics.assign(
            palette_environment.diagnostics().begin(),
            palette_environment.diagnostics().end());
        return failed;
    }
    auto autoloop_compilation = showcore::compile_autoloop_programs(
        source_snapshot.source, palette_environment.environment());
    if (!autoloop_compilation) {
        auto failed = candidate_outcome(
            StudioPreviewResult::CompilationFailed,
            "The V2 source could not be realized exactly; the previous preview remains active.");
        failed.validation = std::move(project_compilation.validation);
        failed.realization = StudioPreviewRealization::Unsupported;
        failed.palette_resolutions = std::move(palette_resolutions);
        failed.autoloop_diagnostics =
            std::move(autoloop_compilation.diagnostics);
        return failed;
    }

    configure_preview_safety(
        project_compilation.show->engine().safety(),
        document_snapshot.document.safety);
    reset_players_and_layers();
    reset_v2_source();
    project_ = document_snapshot.document;
    show_ = std::move(project_compilation.show);
    generation_ = document_snapshot.generation;
    autoloop_v2_source_ = source_snapshot.source;
    autoloop_v2_source_generation_ = source_snapshot.generation;
    autoloop_v2_source_digest_ = canonical_source_digest;
    autoloop_v2_realization_ = palette_environment.degraded()
        ? StudioPreviewRealization::Degraded
        : StudioPreviewRealization::Exact;
    autoloop_v2_palette_resolutions_ = std::move(palette_resolutions);
    autoloop_v2_package_ = std::move(autoloop_compilation.package);
    snapshot_ = {};
    snapshot_.generation = generation_;
    snapshot_.source_generation = autoloop_v2_source_generation_;
    snapshot_.source_digest = autoloop_v2_source_digest_;
    snapshot_.compiled_digest = std::string(autoloop_v2_package_->digest());
    snapshot_.realization = autoloop_v2_realization_;
    snapshot_.palette_resolutions = autoloop_v2_palette_resolutions_;
    snapshot_.validation = std::move(project_compilation.validation);
    snapshot_.frame_trace.reserve(
        kMaximumStudioAutoloopPreviewTraceEntries);
    render_snapshot(0U, 0.0);
    return outcome(StudioPreviewResult::Loaded);
}

StudioPreviewOutcome StudioPreviewService::preview_autoloop_v2(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation,
    std::string_view placement_id) {
    if (show_ == nullptr || autoloop_v2_package_ == nullptr) {
        return outcome(
            StudioPreviewResult::NotLoaded,
            "Load a V2 source snapshot before previewing a placement.");
    }
    if (!generation_matches(expected_generation) ||
        !source_generation_matches(expected_source_generation)) {
        return outcome(
            StudioPreviewResult::StaleGeneration,
            "V2 preview command used a stale document or source generation.");
    }
    if (placement_id.empty()) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview requires a stable placement ID.");
    }
    const auto placement = std::find_if(
        autoloop_v2_source_.placements.begin(),
        autoloop_v2_source_.placements.end(),
        [&](const auto& candidate) { return candidate.id == placement_id; });
    if (placement == autoloop_v2_source_.placements.end()) {
        return outcome(
            StudioPreviewResult::MissingContent,
            "The V2 placement was not found.");
    }
    const showcore::AutoloopAddress address{placement->bank, placement->slot};
    const auto* compiled_placement = autoloop_v2_package_->placement(address);
    if (compiled_placement == nullptr || !compiled_placement->populated()) {
        return outcome(
            StudioPreviewResult::MissingContent,
            "The V2 placement has no compiled program.");
    }
    const auto asset = std::find_if(
        autoloop_v2_source_.assets.begin(), autoloop_v2_source_.assets.end(),
        [&](const auto& candidate) {
            return candidate.id == placement->asset_id;
        });
    if (asset == autoloop_v2_source_.assets.end()) {
        return outcome(
            StudioPreviewResult::MissingContent,
            "The V2 placement asset was not found.");
    }
    const auto* program = autoloop_v2_package_->program(
        compiled_placement->program_index);
    if (program == nullptr || program->length_ticks <= 0) {
        return outcome(
            StudioPreviewResult::CompilationFailed,
            "The V2 placement program is unavailable in the compiled package.");
    }

    reset_players_and_layers();
    autoloop_v2_active_ = true;
    autoloop_v2_program_index_ = compiled_placement->program_index;
    autoloop_v2_program_length_ = program->length_ticks;
    snapshot_.content_kind = StudioPreviewContentKind::Autoloop;
    snapshot_.autoloop_format = StudioPreviewAutoloopFormat::V2;
    snapshot_.content_id = asset->id;
    snapshot_.asset_id = asset->id;
    snapshot_.program_id = asset->program_id;
    snapshot_.placement_id = placement->id;
    snapshot_.source_generation = autoloop_v2_source_generation_;
    snapshot_.source_digest = autoloop_v2_source_digest_;
    snapshot_.compiled_digest = std::string(autoloop_v2_package_->digest());
    return apply_v2_transport_tick(0);
}

StudioPreviewOutcome StudioPreviewService::restart_autoloop_v2(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation) {
    const auto validation = validate_v2_transport(
        expected_generation, expected_source_generation);
    if (!validation) {
        return validation;
    }
    return apply_v2_transport_tick(0);
}

StudioPreviewOutcome StudioPreviewService::seek_autoloop_v2(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation,
    MusicalTick transport_tick) {
    const auto validation = validate_v2_transport(
        expected_generation, expected_source_generation);
    if (!validation) {
        return validation;
    }
    return apply_v2_transport_tick(transport_tick);
}

StudioPreviewOutcome StudioPreviewService::seek_autoloop_v2_beat(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation,
    double beat_position) {
    const auto validation = validate_v2_transport(
        expected_generation, expected_source_generation);
    if (!validation) {
        return validation;
    }
    if (!std::isfinite(beat_position) || beat_position < 0.0) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview beat must be finite and nonnegative.");
    }
    const auto scaled = static_cast<long double>(beat_position) *
        static_cast<long double>(kMusicalTicksPerQuarter);
    const auto rounded = std::floor(scaled + 0.5L);
    if (rounded > static_cast<long double>(
            kMaximumStudioAutoloopPreviewTransportTick)) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview beat exceeds the bounded transport range.");
    }
    return apply_v2_transport_tick(static_cast<MusicalTick>(rounded));
}

StudioPreviewOutcome StudioPreviewService::seek_autoloop_v2_phase(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation,
    double phase) {
    const auto validation = validate_v2_transport(
        expected_generation, expected_source_generation);
    if (!validation) {
        return validation;
    }
    if (!std::isfinite(phase) || phase < 0.0 || phase >= 1.0 ||
        autoloop_v2_program_length_ <= 0) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview phase must be finite in the half-open range [0, 1).");
    }
    const auto loop_tick = static_cast<MusicalTick>(std::floor(
        static_cast<long double>(phase) *
        static_cast<long double>(autoloop_v2_program_length_)));
    const auto completed_loops = snapshot_.transport_tick /
        autoloop_v2_program_length_;
    if (completed_loops >
        (kMaximumStudioAutoloopPreviewTransportTick - loop_tick) /
            autoloop_v2_program_length_) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview phase exceeds the bounded transport range.");
    }
    return apply_v2_transport_tick(
        completed_loops * autoloop_v2_program_length_ + loop_tick);
}

StudioPreviewOutcome StudioPreviewService::advance_autoloop_v2(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation,
    MusicalTick delta_ticks) {
    const auto validation = validate_v2_transport(
        expected_generation, expected_source_generation);
    if (!validation) {
        return validation;
    }
    if (delta_ticks < 0 ||
        delta_ticks > kMaximumStudioAutoloopPreviewTransportTick -
            snapshot_.transport_tick) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview advance exceeds the bounded transport range.");
    }
    return apply_v2_transport_tick(snapshot_.transport_tick + delta_ticks);
}

StudioPreviewOutcome StudioPreviewService::preview_draft_color(
    StudioDocumentGeneration expected_generation,
    std::string_view target_id,
    const StudioColor& color) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded, "Load a document before previewing a color.");
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration, "Preview command used a stale document.");
    }
    if (!valid_studio_color(color)) {
        return outcome(StudioPreviewResult::InvalidArgument, "Picker color is invalid.");
    }
    const auto indices = target_fixture_indices(project_, target_id);
    if (indices.empty()) {
        return outcome(StudioPreviewResult::UnsupportedTarget, "Fixture or group target was not found.");
    }

    showcore::LayerBuffer draft;
    std::size_t realized_count = 0U;
    for (const auto index : indices) {
        const auto& fixture = project_.fixtures[index];
        const auto* profile = find_profile(project_, fixture.profile_id);
        if (profile == nullptr) {
            continue;
        }
        const auto realization = realize_studio_color(*profile, color);
        if (!realization.usable()) {
            continue;
        }
        ++realized_count;
        for (const auto& assignment : realization.assignments) {
            draft.set(
                static_cast<std::uint16_t>(index),
                assignment.property,
                assignment.value);
        }
    }
    if (realized_count == 0U) {
        return outcome(
            StudioPreviewResult::UnsupportedTarget,
            "Target has no qualified semantic color channels.");
    }
    draft_color_layer_ = draft;
    show_->engine().layers().replace_layer(
        showcore::LayerId::ManualOverride, draft_color_layer_);
    snapshot_.draft_color_active = true;
    render_snapshot(snapshot_.now_ms, snapshot_.beat_position);
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::preview_palette_swatch(
    StudioDocumentGeneration expected_generation,
    std::string_view target_id,
    std::string_view palette_id,
    std::string_view swatch_id) {
    if (show_ == nullptr) {
        return outcome(
            StudioPreviewResult::NotLoaded,
            "Load a document before previewing a palette swatch.");
    }
    if (!generation_matches(expected_generation)) {
        return outcome(
            StudioPreviewResult::StaleGeneration,
            "Palette preview command used a stale document.");
    }
    const auto palette = std::find_if(
        project_.color_palettes.begin(), project_.color_palettes.end(),
        [&](const auto& candidate) { return candidate.id == palette_id; });
    if (palette == project_.color_palettes.end()) {
        return outcome(StudioPreviewResult::MissingContent, "Studio palette was not found.");
    }
    const auto swatch = std::find_if(
        palette->swatches.begin(), palette->swatches.end(),
        [&](const auto& candidate) { return candidate.id == swatch_id; });
    if (swatch == palette->swatches.end()) {
        return outcome(StudioPreviewResult::MissingContent, "Studio palette swatch was not found.");
    }
    return preview_draft_color(expected_generation, target_id, swatch->color);
}

StudioPreviewOutcome StudioPreviewService::clear_draft_color(
    StudioDocumentGeneration expected_generation) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded);
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration);
    }
    if (!snapshot_.draft_color_active) {
        return outcome(StudioPreviewResult::NoChange);
    }
    draft_color_layer_.clear();
    show_->engine().layers().clear_layer(showcore::LayerId::ManualOverride);
    snapshot_.draft_color_active = false;
    render_snapshot(snapshot_.now_ms, snapshot_.beat_position);
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::tick(
    StudioDocumentGeneration expected_generation,
    std::uint64_t now_ms,
    double beat_position) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded);
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration);
    }
    if (autoloop_v2_active_) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "Use the bounded V2 transport controls for an active V2 preview.");
    }
    if (!std::isfinite(beat_position)) {
        return outcome(StudioPreviewResult::InvalidArgument, "Beat position must be finite.");
    }
    look_player_.tick(now_ms, show_->engine().layers());
    autoloop_player_.tick(beat_position, true, show_->engine().layers());
    if (snapshot_.draft_color_active) {
        show_->engine().layers().replace_layer(
            showcore::LayerId::ManualOverride, draft_color_layer_);
    }
    render_snapshot(now_ms, beat_position);
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::clear(
    StudioDocumentGeneration expected_generation) {
    if (show_ == nullptr) {
        return outcome(StudioPreviewResult::NotLoaded);
    }
    if (!generation_matches(expected_generation)) {
        return outcome(StudioPreviewResult::StaleGeneration);
    }
    const auto already_clear = snapshot_.content_kind == StudioPreviewContentKind::None &&
        !snapshot_.draft_color_active;
    reset_players_and_layers();
    snapshot_.content_kind = StudioPreviewContentKind::None;
    snapshot_.content_id.clear();
    snapshot_.draft_color_active = false;
    render_snapshot(snapshot_.now_ms, snapshot_.beat_position);
    return outcome(already_clear ? StudioPreviewResult::NoChange : StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::outcome(
    StudioPreviewResult result,
    std::string message) const {
    StudioPreviewOutcome value;
    value.result = result;
    value.generation = generation_;
    value.source_generation = autoloop_v2_source_generation_;
    value.validation = snapshot_.validation;
    value.message = std::move(message);
    value.source_digest = autoloop_v2_source_digest_;
    if (autoloop_v2_package_ != nullptr) {
        value.compiled_digest = std::string(autoloop_v2_package_->digest());
    }
    value.realization = snapshot_.realization;
    value.palette_resolutions = autoloop_v2_palette_resolutions_;
    return value;
}

bool StudioPreviewService::generation_matches(
    StudioDocumentGeneration expected_generation) const noexcept {
    return expected_generation != 0U && expected_generation == generation_;
}

bool StudioPreviewService::source_generation_matches(
    StudioDocumentGeneration expected_generation) const noexcept {
    return expected_generation != 0U &&
        expected_generation == autoloop_v2_source_generation_;
}

StudioPreviewOutcome StudioPreviewService::validate_v2_transport(
    StudioDocumentGeneration expected_generation,
    StudioDocumentGeneration expected_source_generation) const {
    if (show_ == nullptr || autoloop_v2_package_ == nullptr) {
        return outcome(
            StudioPreviewResult::NotLoaded,
            "Load a V2 source snapshot before using its transport.");
    }
    if (!generation_matches(expected_generation) ||
        !source_generation_matches(expected_source_generation)) {
        return outcome(
            StudioPreviewResult::StaleGeneration,
            "V2 transport used a stale document or source generation.");
    }
    if (!autoloop_v2_active_) {
        return outcome(
            StudioPreviewResult::MissingContent,
            "Select a V2 placement before using its transport.");
    }
    return outcome(StudioPreviewResult::Applied);
}

StudioPreviewOutcome StudioPreviewService::apply_v2_transport_tick(
    MusicalTick transport_tick) {
    if (transport_tick < 0 ||
        transport_tick > kMaximumStudioAutoloopPreviewTransportTick) {
        return outcome(
            StudioPreviewResult::InvalidArgument,
            "V2 preview tick exceeds the bounded transport range.");
    }
    if (!render_autoloop_v2_frame(transport_tick)) {
        autoloop_v2_active_ = false;
        autoloop_v2_layer_.clear();
        snapshot_.ownership.clear();
        snapshot_.realization = StudioPreviewRealization::Unsupported;
        if (show_ != nullptr) {
            show_->engine().layers().clear_layer(
                showcore::LayerId::ManualAutoloop);
            render_snapshot(snapshot_.now_ms, snapshot_.beat_position);
        }
        auto failed = outcome(
            StudioPreviewResult::CompilationFailed,
            "Compiled V2 evaluation failed closed before rendering content.");
        failed.realization = StudioPreviewRealization::Unsupported;
        failed.autoloop_diagnostics.push_back({
            showcore::AutoloopCompileError::InternalError,
            showcore::AutoloopArenaKind::Events,
            "autoloop.preview.evaluateFailed",
            snapshot_.program_id,
            "The immutable V2 evaluator rejected its compiled program."});
        return failed;
    }
    return outcome(StudioPreviewResult::Applied);
}

bool StudioPreviewService::render_autoloop_v2_frame(
    MusicalTick transport_tick) {
    if (show_ == nullptr || autoloop_v2_package_ == nullptr ||
        !autoloop_v2_active_ || autoloop_v2_program_length_ <= 0 ||
        autoloop_v2_program_index_ ==
            showcore::kInvalidCompiledAutoloopIndex) {
        return false;
    }
    autoloop_v2_layer_.clear();
    if (!autoloop_v2_evaluator_.evaluate(
            *autoloop_v2_package_,
            autoloop_v2_program_index_,
            transport_tick,
            autoloop_v2_layer_)) {
        return false;
    }
    show_->engine().layers().replace_layer(
        showcore::LayerId::ManualAutoloop, autoloop_v2_layer_);
    if (snapshot_.draft_color_active) {
        show_->engine().layers().replace_layer(
            showcore::LayerId::ManualOverride, draft_color_layer_);
    }

    snapshot_.ownership.clear();
    snapshot_.ownership.reserve(
        project_.fixtures.size() * showcore::kPropertyCount);
    for (std::size_t fixture_index = 0U;
         fixture_index < project_.fixtures.size(); ++fixture_index) {
        for (std::size_t property_index = 0U;
             property_index < showcore::kPropertyCount; ++property_index) {
            const auto property = static_cast<showcore::Property>(
                property_index);
            const auto value = autoloop_v2_layer_.get(
                static_cast<std::uint16_t>(fixture_index), property);
            if (value.mode != showcore::ValueMode::Release) {
                snapshot_.ownership.push_back({
                    project_.fixtures[fixture_index].id,
                    property,
                    value});
            }
        }
    }

    snapshot_.transport_tick = transport_tick;
    snapshot_.loop_tick = transport_tick % autoloop_v2_program_length_;
    snapshot_.completed_loops = static_cast<std::uint64_t>(
        transport_tick / autoloop_v2_program_length_);
    snapshot_.phase = static_cast<double>(snapshot_.loop_tick) /
        static_cast<double>(autoloop_v2_program_length_);
    snapshot_.realization = autoloop_v2_realization_;
    render_snapshot(
        snapshot_.now_ms,
        static_cast<double>(transport_tick) /
            static_cast<double>(kMusicalTicksPerQuarter));

    if (snapshot_.frame_trace.size() ==
        kMaximumStudioAutoloopPreviewTraceEntries) {
        snapshot_.frame_trace.erase(snapshot_.frame_trace.begin());
        ++snapshot_.dropped_trace_entries;
    }
    snapshot_.frame_trace.push_back({
        snapshot_.transport_tick,
        snapshot_.loop_tick,
        snapshot_.beat_position,
        snapshot_.phase,
        snapshot_.completed_loops,
        snapshot_.frame_sha256});
    return true;
}

void StudioPreviewService::reset_v2_playback_snapshot() noexcept {
    autoloop_v2_active_ = false;
    autoloop_v2_program_index_ = showcore::kInvalidCompiledAutoloopIndex;
    autoloop_v2_program_length_ = 0;
    autoloop_v2_layer_.clear();
    snapshot_.content_kind = StudioPreviewContentKind::None;
    snapshot_.autoloop_format = StudioPreviewAutoloopFormat::None;
    snapshot_.realization = StudioPreviewRealization::None;
    snapshot_.content_id.clear();
    snapshot_.asset_id.clear();
    snapshot_.program_id.clear();
    snapshot_.placement_id.clear();
    snapshot_.transport_tick = 0;
    snapshot_.loop_tick = 0;
    snapshot_.phase = 0.0;
    snapshot_.completed_loops = 0U;
    snapshot_.ownership.clear();
    snapshot_.frame_trace.clear();
    snapshot_.dropped_trace_entries = 0U;
}

void StudioPreviewService::reset_v2_source() noexcept {
    autoloop_v2_active_ = false;
    autoloop_v2_program_index_ = showcore::kInvalidCompiledAutoloopIndex;
    autoloop_v2_program_length_ = 0;
    autoloop_v2_layer_.clear();
    autoloop_v2_package_.reset();
    autoloop_v2_source_ = {};
    autoloop_v2_source_generation_ = 0U;
    autoloop_v2_source_digest_.clear();
    autoloop_v2_realization_ = StudioPreviewRealization::None;
    autoloop_v2_palette_resolutions_.clear();
}

void StudioPreviewService::reset_players_and_layers() noexcept {
    look_player_ = showcore::StaticLookPlayer{showcore::LayerId::EventMoment};
    autoloop_player_ = showcore::AutoloopPlayer{showcore::LayerId::ManualAutoloop};
    draft_color_layer_.clear();
    snapshot_.draft_color_active = false;
    reset_v2_playback_snapshot();
    if (show_ != nullptr) {
        show_->engine().layers().clear();
    }
}

void StudioPreviewService::render_snapshot(
    std::uint64_t now_ms,
    double beat_position) {
    if (show_ == nullptr) {
        return;
    }
    show_->engine().tick();
    snapshot_.generation = generation_;
    snapshot_.now_ms = now_ms;
    snapshot_.beat_position = beat_position;
    snapshot_.dmx_frames = show_->engine().frames();
    snapshot_.frame_sha256 = frame_digest(snapshot_.dmx_frames);
    snapshot_.fixtures.clear();
    snapshot_.fixtures.reserve(project_.fixtures.size());

    for (std::size_t index = 0U; index < project_.fixtures.size(); ++index) {
        const auto& source = project_.fixtures[index];
        const auto* profile = find_profile(project_, source.profile_id);
        const auto fixture_id = static_cast<std::uint16_t>(index);
        StudioPreviewFixtureSnapshot fixture;
        fixture.fixture_id = source.id;
        fixture.fixture_name = source.name;
        fixture.universe = source.universe;
        fixture.address = source.address;

        const auto has_intensity = profile_has_property(profile, showcore::Property::Intensity);
        const auto resolved_intensity = resolved_value(
            show_->engine(), fixture_id, showcore::Property::Intensity);
        fixture.intensity = has_intensity ? resolved_intensity : 1.0F;
        fixture.emitted.intensity = fixture.intensity;
        fixture.emitted.rgb = {
            resolved_value(show_->engine(), fixture_id, showcore::Property::Red),
            resolved_value(show_->engine(), fixture_id, showcore::Property::Green),
            resolved_value(show_->engine(), fixture_id, showcore::Property::Blue)};
        fixture.emitted.white = resolved_value(
            show_->engine(), fixture_id, showcore::Property::White);
        fixture.emitted.amber = resolved_value(
            show_->engine(), fixture_id, showcore::Property::Amber);
        fixture.emitted.uv = resolved_value(
            show_->engine(), fixture_id, showcore::Property::UV);
        fixture.emitted.lime = resolved_value(
            show_->engine(), fixture_id, showcore::Property::Lime);
        fixture.emitted.indigo = resolved_value(
            show_->engine(), fixture_id, showcore::Property::Indigo);

        const auto has_rgb = profile_has_property(profile, showcore::Property::Red) ||
            profile_has_property(profile, showcore::Property::Green) ||
            profile_has_property(profile, showcore::Property::Blue);
        if (!has_rgb &&
            profile_has_property(profile, showcore::Property::Cyan) &&
            profile_has_property(profile, showcore::Property::Magenta) &&
            profile_has_property(profile, showcore::Property::Yellow)) {
            fixture.emitted.rgb = studio_rgb_from_cmy({
                resolved_value(show_->engine(), fixture_id, showcore::Property::Cyan),
                resolved_value(show_->engine(), fixture_id, showcore::Property::Magenta),
                resolved_value(show_->engine(), fixture_id, showcore::Property::Yellow)});
        }
        fixture.display_rgb = preview_display_rgb(fixture.emitted, fixture.intensity);

        if (profile != nullptr && source.universe >= 1U &&
            source.universe <= showcore::kV1UniverseCount && source.address >= 1U) {
            const auto first_slot = static_cast<std::size_t>(source.address - 1U);
            const auto available = std::min<std::size_t>(
                profile->footprint,
                showcore::kUniverseSlots - std::min(first_slot, showcore::kUniverseSlots));
            fixture.dmx_values.reserve(available);
            for (std::size_t offset = 0U; offset < available; ++offset) {
                fixture.dmx_values.push_back(snapshot_.dmx_frames.universes[
                    static_cast<std::size_t>(source.universe - 1U)][first_slot + offset]);
            }
        }
        snapshot_.fixtures.push_back(std::move(fixture));
    }
}

const char* studio_preview_result_name(StudioPreviewResult result) noexcept {
    switch (result) {
    case StudioPreviewResult::Loaded: return "loaded";
    case StudioPreviewResult::Applied: return "applied";
    case StudioPreviewResult::NoChange: return "noChange";
    case StudioPreviewResult::NotLoaded: return "notLoaded";
    case StudioPreviewResult::StaleGeneration: return "staleGeneration";
    case StudioPreviewResult::CompilationFailed: return "compilationFailed";
    case StudioPreviewResult::MissingContent: return "missingContent";
    case StudioPreviewResult::UnsupportedTarget: return "unsupportedTarget";
    case StudioPreviewResult::InvalidArgument: return "invalidArgument";
    }
    return "invalidArgument";
}

}  // namespace emberlights
