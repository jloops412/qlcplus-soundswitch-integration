#include "emberlights/audio_assets.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace emberlights {
namespace {

[[nodiscard]] std::string utf8_path(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path(encoded);
}

[[nodiscard]] AudioAssetFileResult asset_error(
    AudioAssetFileStatus status,
    std::string message) {
    AudioAssetFileResult result;
    result.status = status;
    result.message = std::move(message);
    return result;
}

[[nodiscard]] std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z'
            ? static_cast<char>(character + ('a' - 'A'))
            : static_cast<char>(character);
    });
    return value;
}

[[nodiscard]] bool supported_audio_extension(const std::filesystem::path& path) noexcept {
    constexpr std::array<std::string_view, 12> kExtensions{{
        ".aac", ".aif", ".aiff", ".alac", ".flac", ".m4a",
        ".mp3", ".mp4", ".ogg", ".opus", ".wav", ".wma"}};
    const auto extension = lowercase(utf8_path(path.extension()));
    return std::find(kExtensions.begin(), kExtensions.end(), extension) != kExtensions.end();
}

[[nodiscard]] AudioAssetFileResult from_identity(const FileIdentityResult& identity) {
    AudioAssetFileResult result;
    result.status = identity.success ? AudioAssetFileStatus::Available : AudioAssetFileStatus::Invalid;
    result.message = identity.message;
    result.size_bytes = identity.bytes;
    result.sha256 = identity.sha256;
    return result;
}

}  // namespace

AudioAssetFileResult inspect_audio_asset_file(const std::filesystem::path& path) {
    if (!supported_audio_extension(path)) {
        return asset_error(
            AudioAssetFileStatus::Unsupported,
            "Choose a supported audio file (AAC, AIFF, FLAC, M4A, MP3, OGG, OPUS, WAV, or WMA).");
    }
    return from_identity(identify_file_sha256(path));
}

AudioAssetFileResult make_audio_asset(
    const std::filesystem::path& path,
    std::string id,
    AudioAssetDefinition& asset) {
    auto result = inspect_audio_asset_file(path);
    if (!result.available()) {
        return result;
    }
    const auto filename = utf8_path(path.filename());
    if (id.empty() || filename.empty()) {
        return asset_error(
            AudioAssetFileStatus::Invalid,
            "The audio asset needs a stable ID and filename.");
    }
    asset = {};
    asset.id = std::move(id);
    asset.name = filename;
    asset.file_name = filename;
    asset.sha256 = result.sha256;
    asset.size_bytes = result.size_bytes;
    asset.local_path_hint = utf8_path(path);
    return result;
}

AudioAssetFileResult relink_audio_asset(
    AudioAssetDefinition& asset,
    const std::filesystem::path& path) {
    auto result = inspect_audio_asset_file(path);
    if (!result.available()) {
        return result;
    }
    if (result.size_bytes != asset.size_bytes || result.sha256 != asset.sha256) {
        result.status = AudioAssetFileStatus::Changed;
        result.message = "This file does not match the recorded audio identity, so it was not linked.";
        return result;
    }
    asset.local_path_hint = utf8_path(path);
    return result;
}

AudioAssetFileResult verify_audio_asset(const AudioAssetDefinition& asset) {
    if (asset.local_path_hint.empty()) {
        return asset_error(
            AudioAssetFileStatus::Missing,
            "No local path is recorded for this audio asset.");
    }
    const auto path = path_from_utf8(asset.local_path_hint);
    std::error_code filesystem_error;
    if (!std::filesystem::exists(path, filesystem_error) || filesystem_error) {
        return asset_error(
            AudioAssetFileStatus::Missing,
            "The recorded audio file is no longer available at its saved location.");
    }
    auto result = inspect_audio_asset_file(path);
    if (!result.available()) {
        return result;
    }
    if (result.size_bytes != asset.size_bytes || result.sha256 != asset.sha256) {
        result.status = AudioAssetFileStatus::Changed;
        result.message = "The file at the saved audio location does not match the recorded identity.";
    }
    return result;
}

const char* audio_asset_file_status_name(AudioAssetFileStatus status) noexcept {
    switch (status) {
    case AudioAssetFileStatus::Available: return "available";
    case AudioAssetFileStatus::Missing: return "missing";
    case AudioAssetFileStatus::Changed: return "changed";
    case AudioAssetFileStatus::Unsupported: return "unsupported";
    case AudioAssetFileStatus::Invalid: return "invalid";
    }
    return "invalid";
}

}  // namespace emberlights
