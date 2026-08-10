#pragma once

#include "emberlights/project.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class ProjectIoError : std::uint8_t {
    None,
    OpenFailed,
    ReadFailed,
    TooLarge,
    InvalidHeader,
    UnsupportedVersion,
    MissingChecksum,
    ChecksumMismatch,
    InvalidRecord,
    InvalidValue,
    MissingReference,
    WriteFailed,
    ReplaceFailed,
    RecoveryFailed
};

struct ProjectIoResult {
    ProjectIoError error{ProjectIoError::None};
    std::size_t line{0};
    bool recovered_from_backup{false};
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == ProjectIoError::None;
    }
};

// Every normal project save retains a bounded, self-validating restore-point
// history beside the project. Active Runner package snapshots opt out.
inline constexpr std::size_t kMaximumProjectHistoryEntries = 20U;

struct ProjectHistoryEntry {
    std::filesystem::path path;
    std::filesystem::file_time_type modified_at{};
    std::uintmax_t size_bytes{0};
};

[[nodiscard]] std::string serialize_project(const ProjectDocument& project);
[[nodiscard]] ProjectIoResult parse_project(
    std::string_view serialized,
    ProjectDocument& project);
[[nodiscard]] ProjectIoResult load_project(
    const std::filesystem::path& path,
    ProjectDocument& project,
    bool allow_backup_recovery = true);
[[nodiscard]] ProjectIoResult save_project_atomic(
    const std::filesystem::path& path,
    const ProjectDocument& project,
    bool capture_history = true);

// Lists the newest restore points first. A missing history directory is a
// valid empty history rather than an error.
[[nodiscard]] ProjectIoResult list_project_history(
    const std::filesystem::path& path,
    std::vector<ProjectHistoryEntry>& entries);

// Verifies a restore point from this project's history and atomically makes it
// the primary project, retaining the pre-restore primary version as history.
[[nodiscard]] ProjectIoResult restore_project_history(
    const std::filesystem::path& path,
    const std::filesystem::path& history_path,
    ProjectDocument& restored_project);

[[nodiscard]] std::filesystem::path project_backup_path(
    const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path project_active_path(
    const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path project_history_directory(
    const std::filesystem::path& path);

}  // namespace emberlights
