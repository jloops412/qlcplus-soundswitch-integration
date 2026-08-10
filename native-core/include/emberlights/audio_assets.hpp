#pragma once

#include "emberlights/project.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace emberlights {

enum class AudioAssetFileStatus : std::uint8_t {
    Available,
    Missing,
    Changed,
    Unsupported,
    Invalid
};

struct AudioAssetFileResult {
    AudioAssetFileStatus status{AudioAssetFileStatus::Invalid};
    std::string message;
    std::uint64_t size_bytes{0};
    std::string sha256;

    [[nodiscard]] bool available() const noexcept {
        return status == AudioAssetFileStatus::Available;
    }
};

// Imports only a stable, read-only identity. The audio file remains where the
// DJ keeps it; EmberLights never copies, tags, or modifies source audio.
[[nodiscard]] AudioAssetFileResult inspect_audio_asset_file(
    const std::filesystem::path& path);

[[nodiscard]] AudioAssetFileResult make_audio_asset(
    const std::filesystem::path& path,
    std::string id,
    AudioAssetDefinition& asset);

// A relink is accepted only when both the recorded byte size and SHA-256 match.
// A same-named but different file is never silently attached to a script.
[[nodiscard]] AudioAssetFileResult relink_audio_asset(
    AudioAssetDefinition& asset,
    const std::filesystem::path& path);

[[nodiscard]] AudioAssetFileResult verify_audio_asset(
    const AudioAssetDefinition& asset);

struct AudioAssetDirectoryResolveOptions {
    std::size_t maximum_files{100000U};
    std::uint64_t maximum_total_bytes{256ULL * 1024ULL * 1024ULL * 1024ULL};
};

// Studio-only recovery for a moved music library. Only supported audio files
// with a matching recorded size are hashed, and paths are changed only after an
// exact digest match. Source media is never copied, renamed, tagged, or edited.
struct AudioAssetDirectoryResolveResult {
    std::size_t files_examined{0};
    std::size_t hash_candidates{0};
    std::size_t matched_assets{0};
    std::size_t updated_assets{0};
    std::size_t unreadable_files{0};
    bool limit_reached{false};
    std::string message;
    std::vector<std::string> matched_asset_ids;

    [[nodiscard]] bool complete() const noexcept {
        return message.empty() && !limit_reached;
    }
};

[[nodiscard]] AudioAssetDirectoryResolveResult resolve_audio_assets_in_directory(
    ProjectDocument& project,
    const std::filesystem::path& directory,
    const AudioAssetDirectoryResolveOptions& options = {});

[[nodiscard]] const char* audio_asset_file_status_name(AudioAssetFileStatus status) noexcept;

}  // namespace emberlights
