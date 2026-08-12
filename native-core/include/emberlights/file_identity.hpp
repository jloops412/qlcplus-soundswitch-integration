#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace emberlights {

// Studio-side file identity. This deliberately has no Runner dependency: media
// paths and hashes are resolved before a show is activated, never while DMX is
// being scheduled.
struct FileIdentityResult {
    bool success{false};
    std::string sha256;
    std::uint64_t bytes{0};
    std::array<std::uint8_t, 4> header{};
    std::size_t header_size{0};
    std::string message;
};

[[nodiscard]] FileIdentityResult identify_file_sha256(
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes = 16ULL * 1024ULL * 1024ULL * 1024ULL);

[[nodiscard]] bool is_sha256_digest(std::string_view value) noexcept;

// Deterministic in-memory identity for Studio evidence, compiled fixture
// behavior, and offline frame inspection. These helpers perform no I/O.
[[nodiscard]] std::string sha256_bytes(
    std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string sha256_text(std::string_view text);

}  // namespace emberlights
