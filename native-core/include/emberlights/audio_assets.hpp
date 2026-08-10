#pragma once

#include "emberlights/project.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

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

[[nodiscard]] const char* audio_asset_file_status_name(AudioAssetFileStatus status) noexcept;

}  // namespace emberlights
