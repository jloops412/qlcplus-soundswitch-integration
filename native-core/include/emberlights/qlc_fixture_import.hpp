#pragma once

#include "emberlights/project.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::string_view kQlcFixtureAdapterVersion = "qlcplus-qxf-v2";

enum class QlcImportIssueSeverity : std::uint8_t {
    Warning,
    Error
};

struct QlcImportIssue {
    QlcImportIssueSeverity severity{QlcImportIssueSeverity::Error};
    std::string code;
    std::string subject;
    std::string message;
};

struct QlcFixtureImportResult {
    std::string manufacturer;
    std::string model;
    std::string fixture_type;
    std::string source_revision;
    showcore::FixtureProfileSource source{showcore::FixtureProfileSource::QlcPlus};
    std::vector<FixtureProfileDefinition> profiles;
    std::vector<QlcImportIssue> issues;

    [[nodiscard]] explicit operator bool() const noexcept {
        return !profiles.empty();
    }
    [[nodiscard]] std::size_t warning_count() const noexcept;
    [[nodiscard]] std::size_t error_count() const noexcept;
};

// QXF is parsed only in Studio. The deterministic Runner receives the same
// validated native FixtureProfileDefinition used by every other source.
[[nodiscard]] QlcFixtureImportResult import_qlc_fixture(
    std::string_view qxf,
    std::string_view source_identity = {});

[[nodiscard]] QlcFixtureImportResult load_qlc_fixture(
    const std::filesystem::path& path);

}  // namespace emberlights
