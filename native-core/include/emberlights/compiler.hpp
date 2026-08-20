#pragma once

#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"
#include "showcore/autoloop.hpp"
#include "showcore/autoloop_program.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture_library.hpp"
#include "showcore/look.hpp"
#include "showcore/midi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace emberlights {

inline constexpr std::size_t kMaximumCompiledLooks = kMaximumStaticLooks;
inline constexpr std::size_t kMaximumCompiledLookAssignments =
    kMaximumStaticLookAssignments;
inline constexpr std::size_t kMaximumCompiledGroups = kMaximumFixtureGroups;
inline constexpr std::size_t kCompiledNameCapacity = 96;

struct CompiledTrackCue {
    float at_beat{0.0F};
    TrackCueAction action{TrackCueAction::TriggerLook};
    std::uint16_t target{0};
};

struct CompiledTrackScript {
    const CompiledTrackCue* cues{nullptr};
    std::size_t cue_count{0};
};

struct CompilationResult;

class CompiledShow {
public:
    CompiledShow() noexcept = default;
    CompiledShow(const CompiledShow&) = delete;
    CompiledShow& operator=(const CompiledShow&) = delete;
    CompiledShow(CompiledShow&&) = delete;
    CompiledShow& operator=(CompiledShow&&) = delete;

    [[nodiscard]] showcore::Engine& engine() noexcept { return engine_; }
    [[nodiscard]] const showcore::Engine& engine() const noexcept { return engine_; }
    [[nodiscard]] showcore::AutoloopCatalog& autoloops() noexcept { return catalog_; }
    [[nodiscard]] const showcore::AutoloopCatalog& autoloops() const noexcept { return catalog_; }
    [[nodiscard]] showcore::MidiMappingEngine& midi_mappings() noexcept { return midi_mappings_; }
    [[nodiscard]] const showcore::MidiMappingEngine& midi_mappings() const noexcept {
        return midi_mappings_;
    }

    [[nodiscard]] std::size_t look_count() const noexcept { return look_count_; }
    [[nodiscard]] const showcore::StaticLook* look(std::size_t index) const noexcept;
    [[nodiscard]] std::uint32_t look_fade_ms(std::size_t index) const noexcept;
    [[nodiscard]] showcore::AutoloopRepeat autoloop_repeat(
        showcore::AutoloopAddress address) const noexcept;
    [[nodiscard]] std::size_t track_script_count() const noexcept {
        return track_script_count_;
    }
    [[nodiscard]] const CompiledTrackScript* track_script(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t fixture_count() const noexcept {
        return engine_.patch().size();
    }
    [[nodiscard]] const showcore::FixtureGroup* group(std::size_t index) const noexcept;
    [[nodiscard]] const showcore::CompiledAutoloopPackage* autoloop_v2_package()
        const noexcept { return autoloop_v2_package_.get(); }

private:
    friend struct CompilationResult;
    friend CompilationResult compile_project(const ProjectDocument& project);
    friend CompilationResult compile_project(
        const ProjectDocument& project,
        const AutoloopSourceDocument& source,
        const showcore::AutoloopCompileLimits& limits);

    showcore::CompiledFixtureLibrary fixture_library_{};
    showcore::Engine engine_{};
    std::array<std::array<char, kCompiledNameCapacity + 1U>, kMaximumCompiledLooks>
        look_names_{};
    std::array<showcore::StaticLook, kMaximumCompiledLooks> looks_{};
    std::array<std::uint32_t, kMaximumCompiledLooks> look_fades_{};
    std::array<showcore::LookAssignment, kMaximumCompiledLookAssignments>
        look_assignments_{};
    std::array<std::array<char, kCompiledNameCapacity + 1U>, showcore::kMaxAutoloops>
        autoloop_names_{};
    std::array<showcore::AutoloopPattern, showcore::kMaxAutoloops> patterns_{};
    std::array<showcore::AutoloopRepeat, showcore::kMaxAutoloops> repeats_{};
    showcore::AutoloopCatalog catalog_{};
    std::array<CompiledTrackCue, kMaximumTrackCues> track_cues_{};
    std::array<CompiledTrackScript, kMaximumTrackScripts> track_scripts_{};
    std::array<showcore::FixtureGroup, kMaximumCompiledGroups> groups_{};
    showcore::MidiMappingEngine midi_mappings_{};
    std::unique_ptr<const showcore::CompiledAutoloopPackage>
        autoloop_v2_package_{};
    std::size_t look_count_{0};
    std::size_t look_assignment_count_{0};
    std::size_t track_cue_count_{0};
    std::size_t track_script_count_{0};
    std::size_t group_count_{0};
};

struct CompilationResult {
    std::unique_ptr<CompiledShow> show;
    ProjectValidation validation;

    [[nodiscard]] explicit operator bool() const noexcept {
        return show != nullptr && validation.ok();
    }
};

[[nodiscard]] CompilationResult compile_project(const ProjectDocument& project);
// Explicit opt-in overload. The format-1-only overload above is deliberately
// unchanged and never synthesizes or activates V2 content.
[[nodiscard]] CompilationResult compile_project(
    const ProjectDocument& project,
    const AutoloopSourceDocument& source,
    const showcore::AutoloopCompileLimits& limits = {});

// Production document entry point. A recognized persisted V2 record is
// compiled and activated exactly as stored; an absent record preserves the
// format-1 path. Malformed or unsupported records fail closed.
[[nodiscard]] CompilationResult compile_project_with_persisted_autoloops(
    const ProjectDocument& project,
    const showcore::AutoloopCompileLimits& limits = {});

}  // namespace emberlights
