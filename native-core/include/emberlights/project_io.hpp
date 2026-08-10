#pragma once

#include "emberlights/project.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

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
    const ProjectDocument& project);

[[nodiscard]] std::filesystem::path project_backup_path(
    const std::filesystem::path& path);
[[nodiscard]] std::filesystem::path project_active_path(
    const std::filesystem::path& path);

}  // namespace emberlights
