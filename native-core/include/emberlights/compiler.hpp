#pragma once

#include "emberlights/project.hpp"
#include "showcore/autoloop.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture_library.hpp"
#include "showcore/look.hpp"
#include "showcore/midi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace emberlights {

inline constexpr std::size_t kMaximumCompiledLooks = 256;
inline constexpr std::size_t kMaximumCompiledLookAssignments = 32768;
inline constexpr std::size_t kCompiledNameCapacity = 96;

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
    [[nodiscard]] std::size_t fixture_count() const noexcept {
        return engine_.patch().size();
    }

private:
    friend struct CompilationResult;
    friend CompilationResult compile_project(const ProjectDocument& project);

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
    showcore::MidiMappingEngine midi_mappings_{};
    std::size_t look_count_{0};
    std::size_t look_assignment_count_{0};
};

struct CompilationResult {
    std::unique_ptr<CompiledShow> show;
    ProjectValidation validation;

    [[nodiscard]] explicit operator bool() const noexcept {
        return show != nullptr && validation.ok();
    }
};

[[nodiscard]] CompilationResult compile_project(const ProjectDocument& project);

}  // namespace emberlights
