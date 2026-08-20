#pragma once

#include "emberlights/autoloop_source.hpp"
#include "emberlights/project.hpp"

#include "showcore/autoloop_program.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace emberlights {

// Resolution is intentionally limited to project-owned Studio palette
// swatches. A V2 Palette event's reference_id is an exact stable swatch ID;
// display names, palette names, channel names, and raw DMX values are never
// interpreted as references.
enum class AutoloopPaletteResolutionStatus : std::uint8_t {
    Exact,
    Degraded,
    Unsupported,
    Missing,
    Ambiguous
};

struct AutoloopPaletteResolution {
    AutoloopPaletteResolutionStatus status{
        AutoloopPaletteResolutionStatus::Missing};
    std::string reference_id;
    showcore::CompiledAutoloopTargetKind target_kind{
        showcore::CompiledAutoloopTargetKind::Master};
    std::string target_stable_ref;
    std::vector<std::string> palette_ids;
    std::size_t fixture_count{0U};
    std::size_t exact_fixture_count{0U};
    std::size_t degraded_fixture_count{0U};
    std::size_t unsupported_fixture_count{0U};
    std::vector<std::string> warnings;
};

// Owns all target/reference binding storage consumed during one Studio compile.
// The project and source passed to the constructor must outlive this object.
// It has no Runner, output adapter, backend, or device-session dependency.
class AutoloopPaletteCompileEnvironment {
public:
    AutoloopPaletteCompileEnvironment(
        const ProjectDocument& project,
        const AutoloopSourceDocument& source,
        showcore::AutoloopCompileLimits limits = {});
    ~AutoloopPaletteCompileEnvironment();

    AutoloopPaletteCompileEnvironment(
        const AutoloopPaletteCompileEnvironment&) = delete;
    AutoloopPaletteCompileEnvironment& operator=(
        const AutoloopPaletteCompileEnvironment&) = delete;
    AutoloopPaletteCompileEnvironment(
        AutoloopPaletteCompileEnvironment&&) = delete;
    AutoloopPaletteCompileEnvironment& operator=(
        AutoloopPaletteCompileEnvironment&&) = delete;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] bool degraded() const noexcept;
    [[nodiscard]] showcore::AutoloopCompileEnvironment environment()
        const noexcept;
    [[nodiscard]] std::span<const AutoloopPaletteResolution> resolutions()
        const noexcept;
    [[nodiscard]] std::span<const showcore::AutoloopCompileDiagnostic>
        diagnostics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* autoloop_palette_resolution_status_name(
    AutoloopPaletteResolutionStatus status) noexcept;

}  // namespace emberlights
