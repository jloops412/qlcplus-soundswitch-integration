#pragma once

#include "emberlights/compiler.hpp"
#include "emberlights/studio_color.hpp"
#include "emberlights/studio_document.hpp"

#include "showcore/autoloop.hpp"
#include "showcore/look.hpp"
#include "showcore/types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class StudioPreviewResult : std::uint8_t {
    Loaded,
    Applied,
    NoChange,
    NotLoaded,
    StaleGeneration,
    CompilationFailed,
    MissingContent,
    UnsupportedTarget,
    InvalidArgument
};

enum class StudioPreviewContentKind : std::uint8_t {
    None,
    StaticLook,
    Autoloop
};

struct StudioPreviewFixtureSnapshot {
    std::string fixture_id;
    std::string fixture_name;
    std::uint8_t universe{1U};
    std::uint16_t address{1U};
    float intensity{0.0F};
    StudioColor emitted{};
    StudioRgbColor display_rgb{};
    std::vector<std::uint8_t> dmx_values;
};

struct StudioPreviewSnapshot {
    StudioDocumentGeneration generation{0U};
    StudioPreviewContentKind content_kind{StudioPreviewContentKind::None};
    std::string content_id;
    bool draft_color_active{false};
    double beat_position{0.0};
    std::uint64_t now_ms{0U};
    ProjectValidation validation;
    showcore::DmxFrames dmx_frames{};
    std::vector<StudioPreviewFixtureSnapshot> fixtures;
};

struct StudioPreviewOutcome {
    StudioPreviewResult result{StudioPreviewResult::InvalidArgument};
    StudioDocumentGeneration generation{0U};
    ProjectValidation validation;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return result == StudioPreviewResult::Loaded ||
            result == StudioPreviewResult::Applied ||
            result == StudioPreviewResult::NoChange;
    }
};

// Studio-only, no-output preview. Committed Looks and Autoloops run through the
// normal immutable CompiledShow and renderer. Picker gestures are a temporary
// semantic overlay on that candidate; they never mutate the document or open a
// DMX/network/USB adapter.
class StudioPreviewService {
public:
    [[nodiscard]] StudioPreviewOutcome load(
        const StudioDocumentSnapshot& document_snapshot);

    [[nodiscard]] StudioPreviewOutcome preview_look(
        StudioDocumentGeneration expected_generation,
        std::string_view look_id,
        std::uint64_t now_ms,
        bool respect_authored_fade = true);

    [[nodiscard]] StudioPreviewOutcome preview_autoloop(
        StudioDocumentGeneration expected_generation,
        std::string_view autoloop_id,
        double beat_position);

    [[nodiscard]] StudioPreviewOutcome preview_draft_color(
        StudioDocumentGeneration expected_generation,
        std::string_view target_id,
        const StudioColor& color);

    [[nodiscard]] StudioPreviewOutcome clear_draft_color(
        StudioDocumentGeneration expected_generation);

    [[nodiscard]] StudioPreviewOutcome tick(
        StudioDocumentGeneration expected_generation,
        std::uint64_t now_ms,
        double beat_position);

    [[nodiscard]] StudioPreviewOutcome clear(
        StudioDocumentGeneration expected_generation);

    [[nodiscard]] const StudioPreviewSnapshot& snapshot() const noexcept {
        return snapshot_;
    }

private:
    [[nodiscard]] StudioPreviewOutcome outcome(
        StudioPreviewResult result,
        std::string message = {}) const;
    [[nodiscard]] bool generation_matches(
        StudioDocumentGeneration expected_generation) const noexcept;
    void reset_players_and_layers() noexcept;
    void render_snapshot(std::uint64_t now_ms, double beat_position);

    ProjectDocument project_;
    std::unique_ptr<CompiledShow> show_;
    StudioDocumentGeneration generation_{0U};
    showcore::StaticLookPlayer look_player_{showcore::LayerId::EventMoment};
    showcore::AutoloopPlayer autoloop_player_{showcore::LayerId::ManualAutoloop};
    showcore::LayerBuffer draft_color_layer_{};
    StudioPreviewSnapshot snapshot_;
};

[[nodiscard]] const char* studio_preview_result_name(
    StudioPreviewResult result) noexcept;

}  // namespace emberlights
