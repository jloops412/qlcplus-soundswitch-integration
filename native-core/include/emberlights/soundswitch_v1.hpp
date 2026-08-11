#pragma once

#include "emberlights/project.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace emberlights {

enum class SoundSwitchV1MigrationError : std::uint8_t {
    None,
    MissingSource,
    UnsupportedSource,
    ReadFailed,
    InvalidProject
};

struct SoundSwitchV1MigrationResult {
    SoundSwitchV1MigrationError error{SoundSwitchV1MigrationError::None};
    std::string message;
    ProjectDocument project;
    std::string manifest_id;
    std::string venue_sha256;
    std::string autoloops_sha256;
    std::vector<std::string> source_autoloop_names;
    std::vector<std::string> recognized_fixture_models;
    std::vector<std::string> warnings;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SoundSwitchV1MigrationError::None;
    }
};

// Builds a conservative first-pilot project from the recognized SoundSwitch
// 2.10.x color-rig artifacts. It reads only the manifest, venue database, and
// active Autoloop database. Network and USB-DMX outputs remain disabled until
// the operator confirms the staged patch against the physical fixtures.
[[nodiscard]] SoundSwitchV1MigrationResult create_soundswitch_v1_project(
    const std::filesystem::path& source_root);

// Creates the same safe staged project without source provenance. This is used
// for the installed starter template and never claims that source cues were
// decoded.
[[nodiscard]] ProjectDocument make_safe_color_rig_v1_template(
    const std::vector<std::string>& autoloop_names = {});

[[nodiscard]] std::string serialize_soundswitch_v1_migration_report(
    const SoundSwitchV1MigrationResult& migration);

}  // namespace emberlights
