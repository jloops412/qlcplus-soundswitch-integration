#include "emberlights/soundswitch_import.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

inline constexpr std::array<std::uint32_t, 64> kSha256Constants{{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
    0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
    0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
    0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
    0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
    0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
    0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
    0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
    0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
    0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
    0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
    0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
    0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
    0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U}};

[[nodiscard]] constexpr std::uint32_t rotate_right(
    std::uint32_t value,
    std::uint32_t amount) noexcept {
    return (value >> amount) | (value << (32U - amount));
}

class Sha256 {
public:
    void update(const std::uint8_t* bytes, std::size_t count) noexcept {
        total_bytes_ += static_cast<std::uint64_t>(count);
        while (count > 0U) {
            const auto copied = std::min(count, block_.size() - block_size_);
            std::copy_n(bytes, copied, block_.data() + block_size_);
            block_size_ += copied;
            bytes += copied;
            count -= copied;
            if (block_size_ == block_.size()) {
                transform(block_.data());
                block_size_ = 0U;
            }
        }
    }

    [[nodiscard]] std::array<std::uint8_t, 32> finish() noexcept {
        const auto bit_count = total_bytes_ * 8U;
        block_[block_size_++] = 0x80U;
        if (block_size_ > 56U) {
            std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                      block_.end(), std::uint8_t{0});
            transform(block_.data());
            block_size_ = 0U;
        }
        std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                  block_.begin() + 56, std::uint8_t{0});
        for (std::size_t index = 0; index < 8U; ++index) {
            block_[63U - index] = static_cast<std::uint8_t>(bit_count >> (index * 8U));
        }
        transform(block_.data());

        std::array<std::uint8_t, 32> digest{};
        for (std::size_t word = 0; word < state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4U; ++byte) {
                digest[word * 4U + byte] = static_cast<std::uint8_t>(
                    state_[word] >> ((3U - byte) * 8U));
            }
        }
        return digest;
    }

private:
    void transform(const std::uint8_t* block) noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16U; ++index) {
            words[index] =
                (static_cast<std::uint32_t>(block[index * 4U]) << 24U) |
                (static_cast<std::uint32_t>(block[index * 4U + 1U]) << 16U) |
                (static_cast<std::uint32_t>(block[index * 4U + 2U]) << 8U) |
                static_cast<std::uint32_t>(block[index * 4U + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto first = rotate_right(words[index - 15U], 7U) ^
                rotate_right(words[index - 15U], 18U) ^
                (words[index - 15U] >> 3U);
            const auto second = rotate_right(words[index - 2U], 17U) ^
                rotate_right(words[index - 2U], 19U) ^
                (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + first + words[index - 7U] + second;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const auto sum_one = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
                rotate_right(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temporary_one =
                h + sum_one + choice + kSha256Constants[index] + words[index];
            const auto sum_zero = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
                rotate_right(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary_two = sum_zero + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary_one;
            d = c;
            c = b;
            b = a;
            a = temporary_one + temporary_two;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{{
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U}};
    std::array<std::uint8_t, 64> block_{};
    std::size_t block_size_{0};
    std::uint64_t total_bytes_{0};
};

struct FileHashResult {
    bool success{false};
    std::string digest;
    std::uint64_t bytes{0};
    std::array<std::uint8_t, 4> header{};
    std::size_t header_size{0};
    std::string message;
};

[[nodiscard]] std::string hexadecimal(
    const std::array<std::uint8_t, 32>& digest) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(digest.size() * 2U);
    for (const auto byte : digest) {
        encoded.push_back(digits[byte >> 4U]);
        encoded.push_back(digits[byte & 0x0FU]);
    }
    return encoded;
}

[[nodiscard]] FileHashResult hash_file(const std::filesystem::path& path) {
    FileHashResult result;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.message = "Unable to open the file for read-only inspection.";
        return result;
    }
    Sha256 hash;
    std::array<std::uint8_t, 64U * 1024U> buffer{};
    while (input) {
        input.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size()));
        const auto read = input.gcount();
        if (read < 0) {
            result.message = "The file returned an invalid read length.";
            return result;
        }
        const auto count = static_cast<std::size_t>(read);
        if (result.header_size < result.header.size()) {
            const auto header_count = std::min(count, result.header.size() - result.header_size);
            std::copy_n(buffer.data(), header_count, result.header.data() + result.header_size);
            result.header_size += header_count;
        }
        hash.update(buffer.data(), count);
        result.bytes += static_cast<std::uint64_t>(count);
    }
    if (!input.eof()) {
        result.message = "The complete file could not be read.";
        return result;
    }
    result.digest = hexadecimal(hash.finish());
    result.success = true;
    return result;
}

[[nodiscard]] std::string utf8_path(const std::filesystem::path& path) {
    const auto encoded = path.generic_u8string();
    return std::string(
        reinterpret_cast<const char*>(encoded.data()), encoded.size());
}

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value) {
    std::u8string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        encoded.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path(encoded);
}

[[nodiscard]] std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return character >= 'A' && character <= 'Z'
            ? static_cast<char>(character + ('a' - 'A'))
            : static_cast<char>(character);
    });
    return value;
}

[[nodiscard]] bool has_audio_extension(std::string_view extension) noexcept {
    constexpr std::array<std::string_view, 12> extensions{{
        ".aif", ".aiff", ".alac", ".flac", ".m4a", ".mp3",
        ".mp4", ".ogg", ".opus", ".wav", ".wma", ".aac"}};
    return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
}

struct ArtifactClassification {
    SoundSwitchArtifactKind kind{SoundSwitchArtifactKind::Unknown};
    bool is_backup{false};
};

[[nodiscard]] SoundSwitchArtifactKind classify_primary_artifact(
    const std::filesystem::path& relative) {
    const auto filename = lowercase(utf8_path(relative.filename()));
    const auto extension = lowercase(utf8_path(relative.extension()));
    const auto relative_text = lowercase("/" + utf8_path(relative) + "/");
    if (extension == ".ssproj" || filename == ".ssproj") {
        return SoundSwitchArtifactKind::ProjectManifest;
    }
    if (filename == "soundswitchvenues.bin") {
        return SoundSwitchArtifactKind::VenueDatabase;
    }
    if (filename == "soundswitchautoloops.bin") {
        return SoundSwitchArtifactKind::AutoloopDatabase;
    }
    if (filename == "soundswitchautoloopsex.bin") {
        return SoundSwitchArtifactKind::ExtendedAutoloopDatabase;
    }
    if (filename == "soundswitchtrackmap.bin") {
        return SoundSwitchArtifactKind::TrackMap;
    }
    if (extension == ".ssfile") {
        return filename.starts_with("ssautoloop")
            ? SoundSwitchArtifactKind::AutoloopScript
            : SoundSwitchArtifactKind::TrackScript;
    }
    if (extension == ".dat" && relative_text.find("/recordable/") != std::string::npos) {
        return SoundSwitchArtifactKind::RecordableData;
    }
    if (has_audio_extension(extension)) {
        return SoundSwitchArtifactKind::Audio;
    }
    if (extension == ".plfix") {
        return SoundSwitchArtifactKind::FixturePersonality;
    }
    return SoundSwitchArtifactKind::Unknown;
}

[[nodiscard]] ArtifactClassification classify_artifact(
    const std::filesystem::path& relative) {
    const auto extension = lowercase(utf8_path(relative.extension()));
    if (extension != ".bak") {
        return {classify_primary_artifact(relative), false};
    }
    auto primary = relative;
    primary.replace_filename(relative.filename().stem());
    return {classify_primary_artifact(primary), true};
}

void add_issue(
    SoundSwitchInspection& inspection,
    SoundSwitchIssueSeverity severity,
    std::string code,
    std::string subject,
    std::string message) {
    inspection.issues.push_back(
        {severity, std::move(code), std::move(subject), std::move(message)});
}

[[nodiscard]] bool has_active_kind(
    const SoundSwitchInspection& inspection,
    SoundSwitchArtifactKind kind) noexcept {
    return std::any_of(
        inspection.artifacts.begin(), inspection.artifacts.end(),
        [kind](const auto& artifact) {
            return !artifact.is_backup && artifact.kind == kind;
        });
}

[[nodiscard]] bool is_script_kind(SoundSwitchArtifactKind kind) noexcept {
    return kind == SoundSwitchArtifactKind::AutoloopScript ||
        kind == SoundSwitchArtifactKind::TrackScript;
}

void hash_inventory_u64(Sha256& hash, std::uint64_t value) noexcept {
    std::array<std::uint8_t, 8> encoded{};
    for (std::size_t index = 0U; index < encoded.size(); ++index) {
        encoded[encoded.size() - 1U - index] =
            static_cast<std::uint8_t>(value >> (index * 8U));
    }
    hash.update(encoded.data(), encoded.size());
}

void hash_inventory_string(Sha256& hash, std::string_view value) noexcept {
    hash_inventory_u64(hash, static_cast<std::uint64_t>(value.size()));
    hash.update(
        reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

[[nodiscard]] std::string compute_inventory_sha256(
    const SoundSwitchInspection& inspection) {
    std::vector<const SoundSwitchArtifact*> sorted;
    sorted.reserve(inspection.artifacts.size());
    for (const auto& artifact : inspection.artifacts) {
        sorted.push_back(&artifact);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto* first, const auto* second) {
        return first->relative_path < second->relative_path;
    });

    Sha256 hash;
    hash_inventory_string(hash, inspection.inventory_format);
    hash_inventory_u64(hash, inspection.inventory_format_version);
    hash_inventory_string(hash, soundswitch_source_kind_name(inspection.source_kind));
    hash_inventory_u64(hash, static_cast<std::uint64_t>(sorted.size()));
    for (const auto* artifact : sorted) {
        hash_inventory_string(hash, artifact->relative_path);
        hash_inventory_string(hash, soundswitch_artifact_kind_name(artifact->kind));
        const std::array<std::uint8_t, 2> flags{{
            artifact->is_backup ? std::uint8_t{1} : std::uint8_t{0},
            artifact->recognized_ssfile_header ? std::uint8_t{1} : std::uint8_t{0}}};
        hash.update(flags.data(), flags.size());
        hash_inventory_u64(hash, artifact->size);
        hash_inventory_string(hash, artifact->sha256);
    }
    return hexadecimal(hash.finish());
}

inline constexpr std::size_t kMaximumComparisonRanges = 64U;
inline constexpr std::size_t kComparisonBufferBytes = 64U * 1024U;

void add_comparison_issue(
    SoundSwitchComparison& comparison,
    SoundSwitchIssueSeverity severity,
    std::string code,
    std::string subject,
    std::string message) {
    comparison.issues.push_back(
        {severity, std::move(code), std::move(subject), std::move(message)});
}

void append_difference_range(
    SoundSwitchArtifactComparison& artifact,
    std::uint64_t offset,
    std::uint64_t length) {
    if (length == 0U) {
        return;
    }
    if (!artifact.changed_ranges.empty()) {
        auto& previous = artifact.changed_ranges.back();
        if (previous.offset + previous.length == offset) {
            previous.length += length;
            return;
        }
    }
    if (artifact.changed_ranges.size() >= kMaximumComparisonRanges) {
        artifact.ranges_truncated = true;
        return;
    }
    artifact.changed_ranges.push_back({offset, length});
}

[[nodiscard]] bool scan_binary_difference(
    const std::filesystem::path& before_path,
    const std::filesystem::path& after_path,
    SoundSwitchArtifactComparison& artifact,
    std::string& error_message) {
    std::ifstream before(before_path, std::ios::binary);
    std::ifstream after(after_path, std::ios::binary);
    if (!before || !after) {
        error_message = "A changed payload could not be reopened for byte comparison.";
        return false;
    }

    const auto shared_size = std::min(artifact.before_size, artifact.after_size);
    std::array<char, kComparisonBufferBytes> before_buffer{};
    std::array<char, kComparisonBufferBytes> after_buffer{};
    std::uint64_t offset = 0U;
    bool range_open = false;
    std::uint64_t range_offset = 0U;
    std::uint64_t range_length = 0U;
    auto close_range = [&]() {
        if (!range_open) {
            return;
        }
        append_difference_range(artifact, range_offset, range_length);
        range_open = false;
        range_length = 0U;
    };

    while (offset < shared_size) {
        const auto requested = static_cast<std::size_t>(std::min<std::uint64_t>(
            shared_size - offset, before_buffer.size()));
        before.read(before_buffer.data(), static_cast<std::streamsize>(requested));
        after.read(after_buffer.data(), static_cast<std::streamsize>(requested));
        if (before.gcount() != static_cast<std::streamsize>(requested) ||
            after.gcount() != static_cast<std::streamsize>(requested)) {
            error_message = "A changed payload could not be read completely for byte comparison.";
            return false;
        }
        for (std::size_t index = 0U; index < requested; ++index) {
            if (before_buffer[index] != after_buffer[index]) {
                ++artifact.changed_bytes;
                if (!range_open) {
                    range_open = true;
                    range_offset = offset + index;
                    range_length = 0U;
                }
                ++range_length;
            } else {
                close_range();
            }
        }
        offset += requested;
    }
    close_range();

    const auto tail_size = std::max(artifact.before_size, artifact.after_size) - shared_size;
    if (tail_size > 0U) {
        artifact.changed_bytes += tail_size;
        append_difference_range(artifact, shared_size, tail_size);
    }
    return true;
}

void append_json_string(std::ostringstream& output, std::string_view value) {
    constexpr std::string_view digits = "0123456789abcdef";
    output << '"';
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (byte < 0x20U) {
                output << "\\u00" << digits[byte >> 4U] << digits[byte & 0x0FU];
            } else {
                output << character;
            }
            break;
        }
    }
    output << '"';
}

[[nodiscard]] bool is_within(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root) {
    auto candidate_iterator = candidate.begin();
    for (auto root_iterator = root.begin(); root_iterator != root.end(); ++root_iterator) {
        if (candidate_iterator == candidate.end() || *candidate_iterator != *root_iterator) {
            return false;
        }
        ++candidate_iterator;
    }
    return true;
}

[[nodiscard]] std::filesystem::path unused_partial_path(
    const std::filesystem::path& destination) {
    const auto seed = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    for (std::uint32_t attempt = 0U; attempt < 1000U; ++attempt) {
        auto candidate = destination;
        candidate += ".partial-" + std::to_string(seed) + "-" + std::to_string(attempt);
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}

}  // namespace

std::size_t SoundSwitchInspection::error_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == SoundSwitchIssueSeverity::Error;
        }));
}

std::size_t SoundSwitchInspection::warning_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == SoundSwitchIssueSeverity::Warning;
        }));
}

std::size_t SoundSwitchComparison::error_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == SoundSwitchIssueSeverity::Error;
        }));
}

std::size_t SoundSwitchComparison::warning_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const auto& issue) {
            return issue.severity == SoundSwitchIssueSeverity::Warning;
        }));
}

const char* soundswitch_artifact_kind_name(SoundSwitchArtifactKind kind) noexcept {
    switch (kind) {
    case SoundSwitchArtifactKind::ProjectManifest: return "projectManifest";
    case SoundSwitchArtifactKind::VenueDatabase: return "venueDatabase";
    case SoundSwitchArtifactKind::AutoloopDatabase: return "autoloopDatabase";
    case SoundSwitchArtifactKind::ExtendedAutoloopDatabase: return "extendedAutoloopDatabase";
    case SoundSwitchArtifactKind::TrackMap: return "trackMap";
    case SoundSwitchArtifactKind::AutoloopScript: return "autoloopScript";
    case SoundSwitchArtifactKind::TrackScript: return "trackScript";
    case SoundSwitchArtifactKind::RecordableData: return "recordableData";
    case SoundSwitchArtifactKind::Audio: return "audio";
    case SoundSwitchArtifactKind::FixturePersonality: return "fixturePersonality";
    case SoundSwitchArtifactKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* soundswitch_source_kind_name(SoundSwitchSourceKind kind) noexcept {
    switch (kind) {
    case SoundSwitchSourceKind::Unknown: return "unknown";
    case SoundSwitchSourceKind::ExportedProject: return "exportedProject";
    case SoundSwitchSourceKind::ApplicationDataBackup: return "applicationDataBackup";
    }
    return "unknown";
}

const char* soundswitch_artifact_change_name(SoundSwitchArtifactChange change) noexcept {
    switch (change) {
    case SoundSwitchArtifactChange::Unchanged: return "unchanged";
    case SoundSwitchArtifactChange::Added: return "added";
    case SoundSwitchArtifactChange::Removed: return "removed";
    case SoundSwitchArtifactChange::Modified: return "modified";
    case SoundSwitchArtifactChange::Reclassified: return "reclassified";
    }
    return "unknown";
}

SoundSwitchInspection inspect_soundswitch_project(
    const std::filesystem::path& source_root,
    const SoundSwitchInspectionOptions& options) {
    SoundSwitchInspection inspection;
    if (source_root.empty()) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Error,
            "source.empty",
            "Source",
            "Choose a SoundSwitch project directory.");
        return inspection;
    }

    std::error_code error;
    const auto source_status = std::filesystem::symlink_status(source_root, error);
    if (error || !std::filesystem::is_directory(source_status) ||
        std::filesystem::is_symlink(source_status)) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Error,
            "source.notDirectory",
            "source",
            "The source must be a real directory, not a file or symbolic link.");
        return inspection;
    }
    const auto canonical_root = std::filesystem::weakly_canonical(source_root, error);
    if (error) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Error,
            "source.canonicalizeFailed",
            "source",
            "The source directory could not be resolved safely.");
        return inspection;
    }
    inspection.source_root = utf8_path(canonical_root);

    struct PendingFile {
        std::filesystem::path path;
        std::filesystem::path relative;
        std::uint64_t size{0};
        std::filesystem::file_time_type modified{};
    };
    std::vector<PendingFile> pending;
    std::filesystem::recursive_directory_iterator iterator(
        canonical_root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    if (error) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Error,
            "source.enumerationFailed",
            "source",
            "The source directory could not be enumerated.");
        return inspection;
    }
    while (iterator != end) {
        const auto entry = *iterator;
        const auto entry_status = entry.symlink_status(error);
        if (error) {
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Error,
                "file.statusFailed",
                utf8_path(entry.path().lexically_relative(canonical_root)),
                "A source entry could not be inspected.");
            error.clear();
            iterator.increment(error);
            if (error) {
                add_issue(
                    inspection,
                    SoundSwitchIssueSeverity::Error,
                    "source.enumerationInterrupted",
                    "source",
                    "Directory enumeration was interrupted by a filesystem error.");
                break;
            }
            continue;
        }
        if (std::filesystem::is_symlink(entry_status)) {
            if (entry.is_directory(error)) {
                iterator.disable_recursion_pending();
                error.clear();
            }
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Error,
                "file.symlinkRejected",
                utf8_path(entry.path().lexically_relative(canonical_root)),
                "Symbolic links are not followed or bundled.");
        } else if (std::filesystem::is_regular_file(entry_status)) {
            if (pending.size() >= options.maximum_files) {
                add_issue(
                    inspection,
                    SoundSwitchIssueSeverity::Error,
                    "limit.fileCount",
                    "source",
                    "The project exceeds the configured file-count safety limit.");
                break;
            }
            const auto raw_size = entry.file_size(error);
            if (error) {
                add_issue(
                    inspection,
                    SoundSwitchIssueSeverity::Error,
                    "file.sizeFailed",
                    utf8_path(entry.path().lexically_relative(canonical_root)),
                    "A source file size could not be read safely.");
                error.clear();
            } else {
                const auto size = static_cast<std::uint64_t>(raw_size);
                if (size > options.maximum_file_bytes ||
                    inspection.total_bytes > options.maximum_total_bytes -
                        std::min(options.maximum_total_bytes, size)) {
                    add_issue(
                        inspection,
                        SoundSwitchIssueSeverity::Error,
                        "limit.byteCount",
                        utf8_path(entry.path().lexically_relative(canonical_root)),
                        "The project exceeds the configured byte-count safety limit.");
                    break;
                }
                const auto modified = entry.last_write_time(error);
                if (error) {
                    add_issue(
                        inspection,
                        SoundSwitchIssueSeverity::Error,
                        "file.timeFailed",
                        utf8_path(entry.path().lexically_relative(canonical_root)),
                        "A source file timestamp could not be read safely.");
                    error.clear();
                } else {
                    pending.push_back({
                        entry.path(),
                        entry.path().lexically_relative(canonical_root),
                        size,
                        modified});
                    inspection.total_bytes += size;
                }
            }
        } else if (!std::filesystem::is_directory(entry_status)) {
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Warning,
                "file.specialSkipped",
                utf8_path(entry.path().lexically_relative(canonical_root)),
                "A non-regular source entry was skipped.");
        }
        iterator.increment(error);
        if (error) {
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Error,
                "source.enumerationInterrupted",
                "source",
                "Directory enumeration was interrupted by a filesystem error.");
            break;
        }
    }

    std::sort(pending.begin(), pending.end(), [](const auto& first, const auto& second) {
        return utf8_path(first.relative) < utf8_path(second.relative);
    });

    std::size_t unrecognized_ssfiles = 0U;
    for (const auto& file : pending) {
        const auto hash = hash_file(file.path);
        if (!hash.success) {
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Error,
                "file.readFailed",
                utf8_path(file.relative),
                hash.message);
            continue;
        }
        const auto modified_after = std::filesystem::last_write_time(file.path, error);
        if (error || hash.bytes != file.size || modified_after != file.modified) {
            add_issue(
                inspection,
                SoundSwitchIssueSeverity::Error,
                "file.changedDuringInspection",
                utf8_path(file.relative),
                "The source changed while it was being hashed; run the inspection again.");
            error.clear();
            continue;
        }
        SoundSwitchArtifact artifact;
        artifact.relative_path = utf8_path(file.relative);
        const auto classification = classify_artifact(file.relative);
        artifact.kind = classification.kind;
        artifact.is_backup = classification.is_backup;
        artifact.size = file.size;
        artifact.sha256 = hash.digest;
        const bool is_ssfile = is_script_kind(artifact.kind);
        artifact.recognized_ssfile_header = is_ssfile && hash.header_size == 4U &&
            hash.header == std::array<std::uint8_t, 4>{{0xAAU, 0xAAU, 0x09U, 0x55U}};
        if (artifact.recognized_ssfile_header) {
            ++inspection.recognized_ssfiles;
        } else if (is_ssfile) {
            ++unrecognized_ssfiles;
        }
        if (artifact.kind == SoundSwitchArtifactKind::Unknown) {
            ++inspection.unknown_artifacts;
        } else {
            ++inspection.known_artifacts;
        }
        inspection.artifacts.push_back(std::move(artifact));
    }

    const bool has_manifest =
        has_active_kind(inspection, SoundSwitchArtifactKind::ProjectManifest);
    const bool has_application_data_core =
        has_active_kind(inspection, SoundSwitchArtifactKind::VenueDatabase) &&
        has_active_kind(inspection, SoundSwitchArtifactKind::AutoloopDatabase) &&
        has_active_kind(inspection, SoundSwitchArtifactKind::TrackMap);
    if (has_manifest) {
        inspection.source_kind = SoundSwitchSourceKind::ExportedProject;
    } else if (has_application_data_core) {
        inspection.source_kind = SoundSwitchSourceKind::ApplicationDataBackup;
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Warning,
            "format.applicationDataBackup",
            "source",
            "A manifestless SoundSwitch application-data backup was recognized by its required core databases. Its files can be preserved losslessly, but this classification does not claim semantic migration.");
    } else {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Error,
            "format.sourceLayoutUnknown",
            "source",
            "No active .ssproj manifest or complete application-data backup core was found. Select an exported project directory or a backup containing SoundSwitchVenues.bin, SoundSwitchAutoLoops.bin, and SoundSwitchTrackMap.bin.");
    }
    if (!has_active_kind(inspection, SoundSwitchArtifactKind::VenueDatabase)) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Warning,
            "format.venueMissing",
            "source",
            "SoundSwitchVenues.bin was not found; venue and patch conversion may be unavailable.");
    }
    if (!has_active_kind(inspection, SoundSwitchArtifactKind::TrackScript) &&
        !has_active_kind(inspection, SoundSwitchArtifactKind::AutoloopScript)) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Warning,
            "format.scriptsMissing",
            "source",
            "No .ssfile scripts were found in this project directory.");
    }
    if (unrecognized_ssfiles > 0U) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Warning,
            "format.ssfileHeaderUnknown",
            std::to_string(unrecognized_ssfiles) + " .ssfile payload(s)",
            "These scripts do not use the independently observed AA AA 09 55 header and remain opaque.");
    }
    if (inspection.unknown_artifacts > 0U) {
        add_issue(
            inspection,
            SoundSwitchIssueSeverity::Warning,
            "format.unknownPayloads",
            std::to_string(inspection.unknown_artifacts) + " payload(s)",
            "Unknown files are inventoried and preserved losslessly; they are not interpreted.");
    }
    inspection.inventory_sha256 = compute_inventory_sha256(inspection);
    return inspection;
}

std::string serialize_soundswitch_inspection(
    const SoundSwitchInspection& inspection) {
    std::ostringstream output;
    output << "{\n  \"format\": ";
    append_json_string(output, inspection.format);
    output << ",\n  \"formatVersion\": " << inspection.format_version
           << ",\n  \"sourceKind\": ";
    append_json_string(output, soundswitch_source_kind_name(inspection.source_kind));
    output << ",\n  \"inventory\": {\n    \"format\": ";
    append_json_string(output, inspection.inventory_format);
    output << ",\n    \"formatVersion\": " << inspection.inventory_format_version
           << ",\n    \"ordering\": \"relativePathUtf8Bytewise\",\n    \"sha256\": ";
    append_json_string(output, inspection.inventory_sha256);
    output << "\n  }"
           << ",\n  \"complete\": " << (inspection.complete() ? "true" : "false")
           << ",\n  \"summary\": {\n"
           << "    \"files\": " << inspection.artifacts.size() << ",\n"
           << "    \"totalBytes\": " << inspection.total_bytes << ",\n"
           << "    \"knownArtifacts\": " << inspection.known_artifacts << ",\n"
           << "    \"unknownArtifacts\": " << inspection.unknown_artifacts << ",\n"
           << "    \"recognizedSsfiles\": " << inspection.recognized_ssfiles << ",\n"
           << "    \"errors\": " << inspection.error_count() << ",\n"
           << "    \"warnings\": " << inspection.warning_count() << "\n"
           << "  },\n  \"artifacts\": [";
    for (std::size_t index = 0; index < inspection.artifacts.size(); ++index) {
        const auto& artifact = inspection.artifacts[index];
        output << (index == 0U ? "\n" : ",\n") << "    {\"path\": ";
        append_json_string(output, artifact.relative_path);
        output << ", \"kind\": ";
        append_json_string(output, soundswitch_artifact_kind_name(artifact.kind));
        output << ", \"size\": " << artifact.size << ", \"sha256\": ";
        append_json_string(output, artifact.sha256);
        output << ", \"backup\": " << (artifact.is_backup ? "true" : "false");
        output << ", \"recognizedSsfileHeader\": "
               << (artifact.recognized_ssfile_header ? "true" : "false") << '}';
    }
    if (!inspection.artifacts.empty()) {
        output << '\n';
    }
    output << "  ],\n  \"issues\": [";
    for (std::size_t index = 0; index < inspection.issues.size(); ++index) {
        const auto& issue = inspection.issues[index];
        output << (index == 0U ? "\n" : ",\n") << "    {\"severity\": ";
        append_json_string(
            output,
            issue.severity == SoundSwitchIssueSeverity::Error ? "error" : "warning");
        output << ", \"code\": ";
        append_json_string(output, issue.code);
        output << ", \"subject\": ";
        append_json_string(output, issue.subject);
        output << ", \"message\": ";
        append_json_string(output, issue.message);
        output << '}';
    }
    if (!inspection.issues.empty()) {
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

bool save_soundswitch_inspection_atomic(
    const std::filesystem::path& report_path,
    const SoundSwitchInspection& inspection,
    std::string& error_message) {
    error_message.clear();
    if (report_path.empty()) {
        error_message = "The report path is empty.";
        return false;
    }
    std::error_code error;
    const auto parent = report_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            error_message = "The report directory could not be created.";
            return false;
        }
    }
    auto temporary = report_path;
    temporary += ".tmp";
    const auto serialized = serialize_soundswitch_inspection(inspection);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output ||
            !output.write(serialized.data(), static_cast<std::streamsize>(serialized.size())) ||
            !output.flush()) {
            output.close();
            std::filesystem::remove(temporary, error);
            error_message = "The complete inspection report could not be written.";
            return false;
        }
    }
    std::filesystem::rename(temporary, report_path, error);
    if (error) {
        error.clear();
        std::filesystem::remove(report_path, error);
        error.clear();
        std::filesystem::rename(temporary, report_path, error);
    }
    if (error) {
        std::filesystem::remove(temporary, error);
        error_message = "The verified inspection report could not be activated.";
        return false;
    }
    return true;
}

SoundSwitchComparison compare_soundswitch_projects(
    const std::filesystem::path& before_root,
    const std::filesystem::path& after_root,
    const SoundSwitchInspectionOptions& options) {
    SoundSwitchComparison comparison;
    comparison.before = inspect_soundswitch_project(before_root, options);
    comparison.after = inspect_soundswitch_project(after_root, options);
    if (!comparison.before.complete() || !comparison.after.complete()) {
        add_comparison_issue(
            comparison,
            SoundSwitchIssueSeverity::Error,
            "comparison.inspectionFailed",
            "Source inspection",
            "Both exports must pass read-only inspection before they can be compared.");
        return comparison;
    }

    std::map<std::string, const SoundSwitchArtifact*> before_artifacts;
    std::map<std::string, const SoundSwitchArtifact*> after_artifacts;
    for (const auto& artifact : comparison.before.artifacts) {
        before_artifacts.emplace(artifact.relative_path, &artifact);
    }
    for (const auto& artifact : comparison.after.artifacts) {
        after_artifacts.emplace(artifact.relative_path, &artifact);
    }
    std::map<std::string, bool> paths;
    for (const auto& [path, _] : before_artifacts) {
        paths.emplace(path, true);
    }
    for (const auto& [path, _] : after_artifacts) {
        paths.emplace(path, true);
    }

    const auto before_base = path_from_utf8(comparison.before.source_root);
    const auto after_base = path_from_utf8(comparison.after.source_root);
    for (const auto& [path, _] : paths) {
        const auto before = before_artifacts.find(path);
        const auto after = after_artifacts.find(path);
        SoundSwitchArtifactComparison artifact;
        artifact.relative_path = path;
        if (before == before_artifacts.end()) {
            const auto& source = *after->second;
            artifact.kind = source.kind;
            artifact.change = SoundSwitchArtifactChange::Added;
            artifact.after_size = source.size;
            artifact.after_sha256 = source.sha256;
            ++comparison.added_artifacts;
        } else if (after == after_artifacts.end()) {
            const auto& source = *before->second;
            artifact.kind = source.kind;
            artifact.change = SoundSwitchArtifactChange::Removed;
            artifact.before_size = source.size;
            artifact.before_sha256 = source.sha256;
            ++comparison.removed_artifacts;
        } else {
            const auto& first = *before->second;
            const auto& second = *after->second;
            artifact.kind = second.kind;
            artifact.before_size = first.size;
            artifact.after_size = second.size;
            artifact.before_sha256 = first.sha256;
            artifact.after_sha256 = second.sha256;
            if (first.kind != second.kind) {
                artifact.change = SoundSwitchArtifactChange::Reclassified;
                ++comparison.reclassified_artifacts;
            } else if (first.size == second.size && first.sha256 == second.sha256) {
                artifact.change = SoundSwitchArtifactChange::Unchanged;
                ++comparison.unchanged_artifacts;
            } else {
                artifact.change = SoundSwitchArtifactChange::Modified;
                std::string scan_error;
                if (!scan_binary_difference(
                        before_base / path_from_utf8(path),
                        after_base / path_from_utf8(path),
                        artifact,
                        scan_error)) {
                    add_comparison_issue(
                        comparison,
                        SoundSwitchIssueSeverity::Error,
                        "comparison.readFailed",
                        path,
                        std::move(scan_error));
                }
                if (artifact.ranges_truncated) {
                    add_comparison_issue(
                        comparison,
                        SoundSwitchIssueSeverity::Warning,
                        "comparison.rangeLimit",
                        path,
                        "More than 64 separated byte ranges changed; the report lists the first ranges only.");
                }
                ++comparison.modified_artifacts;
            }
        }
        comparison.artifacts.push_back(std::move(artifact));
    }
    return comparison;
}

std::string serialize_soundswitch_comparison(
    const SoundSwitchComparison& comparison) {
    std::ostringstream output;
    output << "{\n  \"format\": ";
    append_json_string(output, comparison.format);
    output << ",\n  \"formatVersion\": " << comparison.format_version
           << ",\n  \"complete\": " << (comparison.complete() ? "true" : "false")
           << ",\n  \"summary\": {\n"
           << "    \"unchanged\": " << comparison.unchanged_artifacts << ",\n"
           << "    \"added\": " << comparison.added_artifacts << ",\n"
           << "    \"removed\": " << comparison.removed_artifacts << ",\n"
           << "    \"modified\": " << comparison.modified_artifacts << ",\n"
           << "    \"reclassified\": " << comparison.reclassified_artifacts << ",\n"
           << "    \"errors\": " << comparison.error_count() << ",\n"
           << "    \"warnings\": " << comparison.warning_count() << "\n"
           << "  },\n  \"artifacts\": [";
    for (std::size_t index = 0U; index < comparison.artifacts.size(); ++index) {
        const auto& artifact = comparison.artifacts[index];
        output << (index == 0U ? "\n" : ",\n") << "    {\"path\": ";
        append_json_string(output, artifact.relative_path);
        output << ", \"kind\": ";
        append_json_string(output, soundswitch_artifact_kind_name(artifact.kind));
        output << ", \"change\": ";
        append_json_string(output, soundswitch_artifact_change_name(artifact.change));
        output << ", \"beforeSize\": " << artifact.before_size
               << ", \"afterSize\": " << artifact.after_size
               << ", \"beforeSha256\": ";
        append_json_string(output, artifact.before_sha256);
        output << ", \"afterSha256\": ";
        append_json_string(output, artifact.after_sha256);
        output << ", \"changedBytes\": " << artifact.changed_bytes
               << ", \"rangesTruncated\": "
               << (artifact.ranges_truncated ? "true" : "false")
               << ", \"changedRanges\": [";
        for (std::size_t range_index = 0U;
             range_index < artifact.changed_ranges.size(); ++range_index) {
            const auto& range = artifact.changed_ranges[range_index];
            output << (range_index == 0U ? "" : ", ")
                   << "{\"offset\": " << range.offset
                   << ", \"length\": " << range.length << '}';
        }
        output << "]}";
    }
    if (!comparison.artifacts.empty()) {
        output << '\n';
    }
    output << "  ],\n  \"issues\": [";
    for (std::size_t index = 0U; index < comparison.issues.size(); ++index) {
        const auto& issue = comparison.issues[index];
        output << (index == 0U ? "\n" : ",\n") << "    {\"severity\": ";
        append_json_string(
            output,
            issue.severity == SoundSwitchIssueSeverity::Error ? "error" : "warning");
        output << ", \"code\": ";
        append_json_string(output, issue.code);
        output << ", \"subject\": ";
        append_json_string(output, issue.subject);
        output << ", \"message\": ";
        append_json_string(output, issue.message);
        output << '}';
    }
    if (!comparison.issues.empty()) {
        output << '\n';
    }
    output << "  ]\n}\n";
    return output.str();
}

bool save_soundswitch_comparison_atomic(
    const std::filesystem::path& report_path,
    const SoundSwitchComparison& comparison,
    std::string& error_message) {
    error_message.clear();
    if (report_path.empty()) {
        error_message = "The report path is empty.";
        return false;
    }
    std::error_code error;
    const auto parent = report_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            error_message = "The report directory could not be created.";
            return false;
        }
    }
    auto temporary = report_path;
    temporary += ".tmp";
    const auto serialized = serialize_soundswitch_comparison(comparison);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output ||
            !output.write(serialized.data(), static_cast<std::streamsize>(serialized.size())) ||
            !output.flush()) {
            output.close();
            std::filesystem::remove(temporary, error);
            error_message = "The complete comparison report could not be written.";
            return false;
        }
    }
    std::filesystem::rename(temporary, report_path, error);
    if (error) {
        error.clear();
        std::filesystem::remove(report_path, error);
        error.clear();
        std::filesystem::rename(temporary, report_path, error);
    }
    if (error) {
        std::filesystem::remove(temporary, error);
        error_message = "The verified comparison report could not be activated.";
        return false;
    }
    return true;
}

SoundSwitchBundleResult create_soundswitch_source_bundle(
    const std::filesystem::path& source_root,
    const std::filesystem::path& destination,
    const SoundSwitchInspectionOptions& options) {
    SoundSwitchBundleResult result;
    result.destination = destination;
    result.inspection = inspect_soundswitch_project(source_root, options);
    if (!result.inspection.complete()) {
        result.error = SoundSwitchBundleError::InspectionFailed;
        result.message = "The source inspection failed; no migration bundle was created.";
        return result;
    }
    if (destination.empty()) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The bundle destination is empty.";
        return result;
    }

    std::error_code error;
    if (std::filesystem::exists(destination, error) || error) {
        result.error = SoundSwitchBundleError::DestinationExists;
        result.message = "The bundle destination already exists or could not be checked.";
        return result;
    }
    const auto canonical_source = std::filesystem::weakly_canonical(source_root, error);
    if (error) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The source path could not be resolved safely.";
        return result;
    }
    auto destination_parent = destination.parent_path();
    if (destination_parent.empty()) {
        destination_parent = std::filesystem::current_path(error);
    }
    if (error) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The destination parent could not be resolved.";
        return result;
    }
    std::filesystem::create_directories(destination_parent, error);
    if (error) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The destination parent could not be created.";
        return result;
    }
    const auto canonical_parent = std::filesystem::weakly_canonical(destination_parent, error);
    if (error) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The destination parent could not be canonicalized.";
        return result;
    }
    const auto resolved_destination = canonical_parent / destination.filename();
    if (is_within(resolved_destination, canonical_source)) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The migration bundle cannot be created inside the SoundSwitch source.";
        return result;
    }
    const auto temporary = unused_partial_path(resolved_destination);
    if (temporary.empty()) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "A unique temporary bundle path could not be reserved.";
        return result;
    }
    const auto payload_root = temporary / "payload";
    std::filesystem::create_directories(payload_root, error);
    if (error) {
        result.error = SoundSwitchBundleError::CreateFailed;
        result.message = "The temporary bundle could not be created.";
        return result;
    }

    auto fail_and_cleanup = [&](SoundSwitchBundleError failure, std::string message) {
        std::error_code cleanup_error;
        std::filesystem::remove_all(temporary, cleanup_error);
        result.error = failure;
        result.message = std::move(message);
    };

    for (const auto& artifact : result.inspection.artifacts) {
        const auto relative = path_from_utf8(artifact.relative_path);
        const auto source = canonical_source / relative;
        const auto copied = payload_root / relative;
        std::filesystem::create_directories(copied.parent_path(), error);
        if (error) {
            fail_and_cleanup(
                SoundSwitchBundleError::CreateFailed,
                "A payload directory could not be created.");
            return result;
        }
        std::filesystem::copy_file(source, copied, std::filesystem::copy_options::none, error);
        if (error) {
            fail_and_cleanup(
                SoundSwitchBundleError::CopyFailed,
                "A source payload could not be copied: " + artifact.relative_path);
            return result;
        }
        const auto verification = hash_file(copied);
        if (!verification.success || verification.bytes != artifact.size ||
            verification.digest != artifact.sha256) {
            fail_and_cleanup(
                SoundSwitchBundleError::VerificationFailed,
                "A copied payload failed SHA-256 verification: " + artifact.relative_path);
            return result;
        }
    }

    std::string report_error;
    if (!save_soundswitch_inspection_atomic(
            temporary / "inventory.json", result.inspection, report_error)) {
        fail_and_cleanup(SoundSwitchBundleError::ReportFailed, std::move(report_error));
        return result;
    }
    std::filesystem::rename(temporary, resolved_destination, error);
    if (error) {
        fail_and_cleanup(
            SoundSwitchBundleError::ActivateFailed,
            "The verified bundle could not be moved into its final destination.");
        return result;
    }
    result.destination = resolved_destination;
    result.message = "The source was copied and every payload matched its SHA-256 inventory.";
    return result;
}

}  // namespace emberlights
