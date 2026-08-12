#pragma once

#include "emberlights/raw_hardware_test.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kRawHardwareTestOperatorManifestVersion = 1U;
inline constexpr std::uint32_t kRawHardwareTestOperatorAuditVersion = 1U;
inline constexpr std::string_view kRawHardwareTestOperatorManifestHeader =
    "EMBERLIGHTS_RAW_HARDWARE_TEST_OPERATOR";
inline constexpr std::string_view kRawHardwareTestOperatorAuditHeader =
    "EMBERLIGHTS_RAW_HARDWARE_TEST_AUDIT";

enum class RawHardwareTestOperatorError : std::uint8_t {
    None,
    ReadFailed,
    TooLarge,
    InvalidHeader,
    InvalidField,
    DuplicateField,
    MissingField,
    InvalidPath,
    InvalidProject,
    InvalidPlan,
    SessionIncomplete,
    AuditInvalid,
    AuditWriteFailed,
    GraduationRejected,
    GraduationWriteFailed
};

struct RawHardwareTestOperatorCheck {
    RawHardwareTestOperatorError error{RawHardwareTestOperatorError::None};
    std::size_t line{0U};
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return error == RawHardwareTestOperatorError::None;
    }
};

// A manifest describes exactly one candidate project, one fixture, one named
// physical unit, and one SoundSwitch Micro universe binding. It contains no
// arbitrary frame or channel-list escape hatch.
struct RawHardwareTestOperatorManifest {
    std::uint32_t schema_version{kRawHardwareTestOperatorManifestVersion};
    std::filesystem::path project_path;
    std::filesystem::path graduated_project_path;
    std::filesystem::path audit_path;
    std::string input_project_sha256;
    std::string fixture_id;
    std::string unit_label;
    std::string output_backend;
    std::string operator_id;
    RawHardwareTestConfig config{};
    std::vector<RawHardwareTestSlotCriterion> criteria;
    std::vector<std::string> markers_to_supersede;
};

struct PreparedRawHardwareTestOperatorRun {
    RawHardwareTestOperatorManifest manifest;
    ProjectDocument candidate_project;
    RawHardwareTestPlan plan;
    std::string candidate_file_sha256;
};

struct RawHardwareTestOperatorCompletion {
    RawHardwareTestAttempt attempt;
    bool audit_appended{false};
    bool graduated{false};
};

[[nodiscard]] const char* raw_hardware_test_operator_error_name(
    RawHardwareTestOperatorError error) noexcept;

// Relative paths are resolved against source_directory. The grammar is a
// strict tab-separated v1 document; unknown, duplicate, or missing fields are
// rejected instead of ignored.
[[nodiscard]] RawHardwareTestOperatorCheck
parse_raw_hardware_test_operator_manifest(
    std::string_view text,
    const std::filesystem::path& source_directory,
    RawHardwareTestOperatorManifest& manifest);

[[nodiscard]] RawHardwareTestOperatorCheck
load_raw_hardware_test_operator_manifest(
    const std::filesystem::path& path,
    RawHardwareTestOperatorManifest& manifest);

// Performs all project, binding, criterion, marker, path, and existing-audit
// validation without opening an output device.
[[nodiscard]] RawHardwareTestOperatorCheck
prepare_raw_hardware_test_operator_run(
    RawHardwareTestOperatorManifest manifest,
    PreparedRawHardwareTestOperatorRun& prepared);

[[nodiscard]] std::string raw_hardware_test_operator_acknowledgement(
    const PreparedRawHardwareTestOperatorRun& prepared);

[[nodiscard]] bool raw_hardware_test_operator_acknowledged(
    const PreparedRawHardwareTestOperatorRun& prepared,
    std::string_view response);

// A missing audit is a valid empty destination. Existing content must be the
// exact v1 header followed only by valid, non-duplicated attempt envelopes.
[[nodiscard]] RawHardwareTestOperatorCheck
validate_raw_hardware_test_operator_audit(
    const std::filesystem::path& path);

// Finalization always seals and appends a terminal attempt first. Only a fully
// successful attempt is considered for graduation, which reloads the current
// candidate and refuses stale/tampered/replayed evidence or an existing output.
[[nodiscard]] RawHardwareTestOperatorCheck
finalize_raw_hardware_test_operator_run(
    const PreparedRawHardwareTestOperatorRun& prepared,
    const RawHardwareTestSession& session,
    std::string_view completed_at_utc,
    RawHardwareTestOperatorCompletion& completion);

}  // namespace emberlights
