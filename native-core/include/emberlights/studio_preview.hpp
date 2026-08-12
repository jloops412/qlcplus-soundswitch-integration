#pragma once

#include "emberlights/autoloop_authoring.hpp"
#include "emberlights/autoloop_palette_resolution.hpp"
#include "emberlights/compiler.hpp"
#include "emberlights/studio_color.hpp"
#include "emberlights/studio_document.hpp"

#include "showcore/autoloop.hpp"
#include "showcore/autoloop_program.hpp"
#include "showcore/look.hpp"
#include "showcore/types.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr MusicalTick kMaximumStudioAutoloopPreviewTransportTick =
    4LL * 1024LL * 1024LL * kMusicalTicksPerQuarter;
inline constexpr std::size_t kMaximumStudioAutoloopPreviewTraceEntries = 256U;

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

enum class StudioPreviewAutoloopFormat : std::uint8_t {
    None,
    Format1,
    V2
};

enum class StudioPreviewRealization : std::uint8_t {
    None,
    Exact,
    Degraded,
    Unsupported
};

struct StudioPreviewOwnershipSnapshot {
    std::string fixture_id;
    showcore::Property property{showcore::Property::Intensity};
    showcore::PropertyValue value{};
};

struct StudioPreviewFrameTrace {
    MusicalTick transport_tick{0};
    MusicalTick loop_tick{0};
    double beat_position{0.0};
    double phase{0.0};
    std::uint64_t completed_loops{0U};
    std::string frame_sha256;
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
    StudioDocumentGeneration source_generation{0U};
    StudioPreviewContentKind content_kind{StudioPreviewContentKind::None};
    StudioPreviewAutoloopFormat autoloop_format{
        StudioPreviewAutoloopFormat::None};
    StudioPreviewRealization realization{StudioPreviewRealization::None};
    std::string content_id;
    std::string asset_id;
    std::string program_id;
    std::string placement_id;
    std::string source_digest;
    std::string compiled_digest;
    std::string frame_sha256;
    bool output_disabled{true};
    bool draft_color_active{false};
    MusicalTick transport_tick{0};
    MusicalTick loop_tick{0};
    double beat_position{0.0};
    double phase{0.0};
    std::uint64_t completed_loops{0U};
    std::uint64_t now_ms{0U};
    ProjectValidation validation;
    showcore::DmxFrames dmx_frames{};
    std::vector<StudioPreviewFixtureSnapshot> fixtures;
    std::vector<StudioPreviewOwnershipSnapshot> ownership;
    std::vector<StudioPreviewFrameTrace> frame_trace;
    std::vector<AutoloopPaletteResolution> palette_resolutions;
    std::size_t dropped_trace_entries{0U};
};

struct StudioPreviewOutcome {
    StudioPreviewResult result{StudioPreviewResult::InvalidArgument};
    StudioDocumentGeneration generation{0U};
    StudioDocumentGeneration source_generation{0U};
    ProjectValidation validation;
    std::string message;
    std::string source_digest;
    std::string compiled_digest;
    StudioPreviewRealization realization{StudioPreviewRealization::None};
    std::vector<AutoloopPaletteResolution> palette_resolutions;
    std::vector<showcore::AutoloopCompileDiagnostic> autoloop_diagnostics;

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

    // Atomically compiles one generation-stamped canonical V2 source snapshot
    // against the candidate venue and the same production fixture renderer as
    // format-1 preview. No output adapter or device session is part of this
    // service boundary.
    [[nodiscard]] StudioPreviewOutcome load_autoloop_v2(
        const StudioDocumentSnapshot& document_snapshot,
        const AutoloopAuthoringSnapshot& source_snapshot);

    [[nodiscard]] StudioPreviewOutcome preview_autoloop_v2(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation,
        std::string_view placement_id);

    [[nodiscard]] StudioPreviewOutcome restart_autoloop_v2(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation);
    [[nodiscard]] StudioPreviewOutcome seek_autoloop_v2(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation,
        MusicalTick transport_tick);
    [[nodiscard]] StudioPreviewOutcome seek_autoloop_v2_beat(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation,
        double beat_position);
    [[nodiscard]] StudioPreviewOutcome seek_autoloop_v2_phase(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation,
        double phase);
    [[nodiscard]] StudioPreviewOutcome advance_autoloop_v2(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation,
        MusicalTick delta_ticks);

    [[nodiscard]] StudioPreviewOutcome preview_draft_color(
        StudioDocumentGeneration expected_generation,
        std::string_view target_id,
        const StudioColor& color);

    // Resolves a committed project palette swatch, then uses the same
    // temporary semantic no-output layer as an in-progress picker gesture.
    [[nodiscard]] StudioPreviewOutcome preview_palette_swatch(
        StudioDocumentGeneration expected_generation,
        std::string_view target_id,
        std::string_view palette_id,
        std::string_view swatch_id);

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
    [[nodiscard]] bool source_generation_matches(
        StudioDocumentGeneration expected_generation) const noexcept;
    [[nodiscard]] StudioPreviewOutcome validate_v2_transport(
        StudioDocumentGeneration expected_generation,
        StudioDocumentGeneration expected_source_generation) const;
    [[nodiscard]] StudioPreviewOutcome apply_v2_transport_tick(
        MusicalTick transport_tick);
    [[nodiscard]] bool render_autoloop_v2_frame(MusicalTick transport_tick);
    void reset_v2_playback_snapshot() noexcept;
    void reset_v2_source() noexcept;
    void reset_players_and_layers() noexcept;
    void render_snapshot(std::uint64_t now_ms, double beat_position);

    ProjectDocument project_;
    std::unique_ptr<CompiledShow> show_;
    StudioDocumentGeneration generation_{0U};
    showcore::StaticLookPlayer look_player_{showcore::LayerId::EventMoment};
    showcore::AutoloopPlayer autoloop_player_{showcore::LayerId::ManualAutoloop};
    AutoloopSourceDocument autoloop_v2_source_;
    std::unique_ptr<const showcore::CompiledAutoloopPackage>
        autoloop_v2_package_;
    StudioDocumentGeneration autoloop_v2_source_generation_{0U};
    std::string autoloop_v2_source_digest_;
    StudioPreviewRealization autoloop_v2_realization_{
        StudioPreviewRealization::None};
    std::vector<AutoloopPaletteResolution>
        autoloop_v2_palette_resolutions_;
    showcore::AutoloopProgramEvaluator autoloop_v2_evaluator_;
    showcore::LayerBuffer autoloop_v2_layer_{};
    std::size_t autoloop_v2_program_index_{
        showcore::kInvalidCompiledAutoloopIndex};
    MusicalTick autoloop_v2_program_length_{0};
    bool autoloop_v2_active_{false};
    showcore::LayerBuffer draft_color_layer_{};
    StudioPreviewSnapshot snapshot_;
};

[[nodiscard]] const char* studio_preview_result_name(
    StudioPreviewResult result) noexcept;

}  // namespace emberlights
