#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace emberlights {

enum class SoundSwitchArtifactKind : std::uint8_t {
    ProjectManifest,
    VenueDatabase,
    AutoloopDatabase,
    ExtendedAutoloopDatabase,
    TrackMap,
    AutoloopScript,
    TrackScript,
    RecordableData,
    Audio,
    Unknown
};

enum class SoundSwitchIssueSeverity : std::uint8_t {
    Warning,
    Error
};

struct SoundSwitchInspectionIssue {
    SoundSwitchIssueSeverity severity{SoundSwitchIssueSeverity::Warning};
    std::string code;
    std::string subject;
    std::string message;
};

struct SoundSwitchArtifact {
    std::string relative_path;
    SoundSwitchArtifactKind kind{SoundSwitchArtifactKind::Unknown};
    std::uint64_t size{0};
    std::string sha256;
    bool recognized_ssfile_header{false};
};

struct SoundSwitchInspectionOptions {
    std::size_t maximum_files{100000U};
    std::uint64_t maximum_file_bytes{16ULL * 1024ULL * 1024ULL * 1024ULL};
    std::uint64_t maximum_total_bytes{256ULL * 1024ULL * 1024ULL * 1024ULL};
};

struct SoundSwitchInspection {
    std::string format{"emberlights-soundswitch-inspection"};
    std::uint32_t format_version{1U};
    std::string source_root;
    std::vector<SoundSwitchArtifact> artifacts;
    std::vector<SoundSwitchInspectionIssue> issues;
    std::uint64_t total_bytes{0};
    std::size_t known_artifacts{0};
    std::size_t unknown_artifacts{0};
    std::size_t recognized_ssfiles{0};

    [[nodiscard]] std::size_t error_count() const noexcept;
    [[nodiscard]] std::size_t warning_count() const noexcept;
    [[nodiscard]] bool complete() const noexcept { return error_count() == 0U; }
};

enum class SoundSwitchBundleError : std::uint8_t {
    None,
    InspectionFailed,
    DestinationExists,
    CreateFailed,
    CopyFailed,
    VerificationFailed,
    ReportFailed,
    ActivateFailed
};

struct SoundSwitchBundleResult {
    SoundSwitchBundleError error{SoundSwitchBundleError::None};
    std::string message;
    SoundSwitchInspection inspection;
    std::filesystem::path destination;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SoundSwitchBundleError::None;
    }
};

[[nodiscard]] SoundSwitchInspection inspect_soundswitch_project(
    const std::filesystem::path& source_root,
    const SoundSwitchInspectionOptions& options = {});

[[nodiscard]] std::string serialize_soundswitch_inspection(
    const SoundSwitchInspection& inspection);

[[nodiscard]] bool save_soundswitch_inspection_atomic(
    const std::filesystem::path& report_path,
    const SoundSwitchInspection& inspection,
    std::string& error_message);

// The source tree is only read. The destination must not exist, and it is
// published only after every copied payload matches the source SHA-256.
[[nodiscard]] SoundSwitchBundleResult create_soundswitch_source_bundle(
    const std::filesystem::path& source_root,
    const std::filesystem::path& destination,
    const SoundSwitchInspectionOptions& options = {});

[[nodiscard]] const char* soundswitch_artifact_kind_name(
    SoundSwitchArtifactKind kind) noexcept;

}  // namespace emberlights
