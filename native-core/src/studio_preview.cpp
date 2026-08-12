#include "emberlights/studio_preview.hpp"

#include <algorithm>
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

    auto& preview_safety = compilation.show->engine().safety();
    preview_safety.strobe_allowed = document_snapshot.document.safety.strobe_allowed;
    preview_safety.max_strobe = document_snapshot.document.safety.max_strobe;
    preview_safety.max_intensity = document_snapshot.document.safety.max_intensity;
    preview_safety.fog_armed = !document_snapshot.document.safety.fog_requires_arm;
    preview_safety.haze_armed = !document_snapshot.document.safety.haze_requires_arm;
    preview_safety.laser_armed = !document_snapshot.document.safety.laser_requires_arm;
    preview_safety.spark_armed = !document_snapshot.document.safety.spark_requires_arm;

    reset_players_and_layers();
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
    snapshot_.content_id = std::string(autoloop_id);
    render_snapshot(snapshot_.now_ms, beat_position);
    return outcome(StudioPreviewResult::Applied);
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
    return {result, generation_, snapshot_.validation, std::move(message)};
}

bool StudioPreviewService::generation_matches(
    StudioDocumentGeneration expected_generation) const noexcept {
    return expected_generation != 0U && expected_generation == generation_;
}

void StudioPreviewService::reset_players_and_layers() noexcept {
    look_player_ = showcore::StaticLookPlayer{showcore::LayerId::EventMoment};
    autoloop_player_ = showcore::AutoloopPlayer{showcore::LayerId::ManualAutoloop};
    draft_color_layer_.clear();
    snapshot_.draft_color_active = false;
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
