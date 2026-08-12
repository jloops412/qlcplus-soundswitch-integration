#include "emberlights/soundswitch_migration_ir.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

namespace emberlights {
namespace {

inline constexpr std::size_t kMaximumIdentifierLength = 256U;
inline constexpr std::size_t kMaximumPathLength = 4096U;
inline constexpr std::size_t kMaximumVersionLength = 128U;
inline constexpr std::size_t kMaximumLabelLength = 4096U;
inline constexpr std::size_t kMaximumMessageLength = 8192U;
inline constexpr std::size_t kMaximumDependencyCodes = 256U;

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
    void update(std::string_view text) noexcept {
        update(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    }

    [[nodiscard]] std::string finish_hex() noexcept {
        const auto bit_count = total_bytes_ * 8U;
        block_[block_size_++] = 0x80U;
        if (block_size_ > 56U) {
            std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                      block_.end(), std::uint8_t{0});
            transform();
            block_size_ = 0U;
        }
        std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_),
                  block_.begin() + 56, std::uint8_t{0});
        for (std::size_t index = 0U; index < 8U; ++index) {
            block_[63U - index] = static_cast<std::uint8_t>(bit_count >> (index * 8U));
        }
        transform();

        constexpr std::string_view digits = "0123456789abcdef";
        std::string encoded;
        encoded.reserve(64U);
        for (const auto word : state_) {
            for (std::size_t index = 0U; index < 4U; ++index) {
                const auto byte = static_cast<std::uint8_t>(word >> ((3U - index) * 8U));
                encoded.push_back(digits[byte >> 4U]);
                encoded.push_back(digits[byte & 0x0FU]);
            }
        }
        return encoded;
    }

private:
    void update(const std::uint8_t* bytes, std::size_t count) noexcept {
        total_bytes_ += static_cast<std::uint64_t>(count);
        while (count > 0U) {
            const auto copied = std::min(count, block_.size() - block_size_);
            std::copy_n(bytes, copied, block_.data() + block_size_);
            bytes += copied;
            count -= copied;
            block_size_ += copied;
            if (block_size_ == block_.size()) {
                transform();
                block_size_ = 0U;
            }
        }
    }

    void transform() noexcept {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0U; index < 16U; ++index) {
            words[index] =
                (static_cast<std::uint32_t>(block_[index * 4U]) << 24U) |
                (static_cast<std::uint32_t>(block_[index * 4U + 1U]) << 16U) |
                (static_cast<std::uint32_t>(block_[index * 4U + 2U]) << 8U) |
                static_cast<std::uint32_t>(block_[index * 4U + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto first = rotate_right(words[index - 15U], 7U) ^
                rotate_right(words[index - 15U], 18U) ^ (words[index - 15U] >> 3U);
            const auto second = rotate_right(words[index - 2U], 17U) ^
                rotate_right(words[index - 2U], 19U) ^ (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + first + words[index - 7U] + second;
        }

        auto a = state_[0]; auto b = state_[1]; auto c = state_[2]; auto d = state_[3];
        auto e = state_[4]; auto f = state_[5]; auto g = state_[6]; auto h = state_[7];
        for (std::size_t index = 0U; index < words.size(); ++index) {
            const auto sum_one = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
                rotate_right(e, 25U);
            const auto choice = (e & f) ^ ((~e) & g);
            const auto temporary_one =
                h + sum_one + choice + kSha256Constants[index] + words[index];
            const auto sum_zero = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
                rotate_right(a, 22U);
            const auto temporary_two = sum_zero + ((a & b) ^ (a & c) ^ (b & c));
            h = g; g = f; f = e; e = d + temporary_one;
            d = c; c = b; b = a; a = temporary_one + temporary_two;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{{
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U}};
    std::array<std::uint8_t, 64> block_{};
    std::size_t block_size_{0U};
    std::uint64_t total_bytes_{0U};
};

[[nodiscard]] std::string sha256_text(std::string_view text) {
    Sha256 hash;
    hash.update(text);
    return hash.finish_hex();
}

void sort_unique(std::vector<std::string>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void append_json_string(std::ostringstream& output, std::string_view value) {
    constexpr std::string_view digits = "0123456789abcdef";
    output << '"';
    for (const auto character : value) {
        const auto byte = static_cast<unsigned char>(character);
        switch (byte) {
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

void append_string_array(
    std::ostringstream& output,
    const std::vector<std::string>& values,
    std::string_view indentation) {
    output << '[';
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) output << ", ";
        append_json_string(output, values[index]);
    }
    output << ']';
    (void)indentation;
}

[[nodiscard]] bool portable_relative_path(std::string_view path) noexcept {
    if (path.empty() || path.front() == '/' || path.front() == '\\' ||
        path.find('\\') != std::string_view::npos ||
        (path.size() > 1U && path[1] == ':')) {
        return false;
    }
    std::size_t start = 0U;
    while (start <= path.size()) {
        const auto end = path.find('/', start);
        const auto component = path.substr(
            start, end == std::string_view::npos ? path.size() - start : end - start);
        if (component.empty() || component == "." || component == "..") return false;
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return true;
}

[[nodiscard]] bool stable_code(std::string_view value) noexcept {
    return !value.empty() && value.size() <= kMaximumIdentifierLength &&
        std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return (character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9') || character == '.' ||
                character == '_' || character == '-';
        });
}

[[nodiscard]] MigrationSourceRole source_role(
    SoundSwitchArtifactKind kind,
    SoundSwitchMigrationScope scope) noexcept {
    switch (kind) {
    case SoundSwitchArtifactKind::ProjectManifest:
    case SoundSwitchArtifactKind::VenueDatabase:
    case SoundSwitchArtifactKind::AutoloopDatabase:
    case SoundSwitchArtifactKind::ExtendedAutoloopDatabase:
    case SoundSwitchArtifactKind::FixturePersonality:
        return MigrationSourceRole::Required;
    case SoundSwitchArtifactKind::TrackMap:
    case SoundSwitchArtifactKind::TrackScript:
    case SoundSwitchArtifactKind::RecordableData:
    case SoundSwitchArtifactKind::Audio:
        return scope == SoundSwitchMigrationScope::ScriptedTracks
            ? MigrationSourceRole::Required : MigrationSourceRole::Conditional;
    case SoundSwitchArtifactKind::AutoloopScript:
        return MigrationSourceRole::Conditional;
    case SoundSwitchArtifactKind::Unknown:
        return MigrationSourceRole::Optional;
    }
    return MigrationSourceRole::Optional;
}

[[nodiscard]] std::string artifact_identity(const MigrationSourceArtifact& artifact) {
    std::ostringstream identity;
    identity << soundswitch_artifact_kind_name(artifact.kind) << '\n'
             << artifact.relative_path << '\n' << artifact.size << '\n'
             << artifact.sha256;
    return "ssa1-" + sha256_text(identity.str());
}

[[nodiscard]] std::string bundle_identity(const SoundSwitchCorpusManifest& manifest) {
    std::ostringstream identity;
    identity << "format=1\nsourceVersion=" << manifest.source_version << '\n';
    for (const auto& artifact : manifest.artifacts) {
        identity << artifact.artifact_id << '\n'
                 << migration_source_role_name(artifact.role) << '\n'
                 << migration_source_availability_name(artifact.availability) << '\n';
    }
    for (const auto& code : manifest.missing_dependency_codes) {
        identity << "missing=" << code << '\n';
    }
    return "ssbundle-v1-" + sha256_text(identity.str());
}

[[nodiscard]] bool has_kind(
    const SoundSwitchCorpusManifest& manifest,
    SoundSwitchArtifactKind kind) noexcept {
    return std::any_of(manifest.artifacts.begin(), manifest.artifacts.end(),
        [kind](const auto& artifact) {
            return artifact.kind == kind &&
                artifact.availability == MigrationSourceAvailability::PresentVerified;
        });
}

void add_missing_if(bool condition, std::vector<std::string>& codes, std::string code) {
    if (condition) codes.push_back(std::move(code));
}

[[nodiscard]] MigrationStatusCounts calculate_counts(
    const std::vector<MigrationItem>& items) noexcept {
    MigrationStatusCounts counts;
    for (const auto& item : items) {
        const auto index = static_cast<std::size_t>(item.status);
        if (index < counts.by_status.size()) ++counts.by_status[index];
    }
    return counts;
}

void add_error(MigrationContractValidation& validation, std::string error) {
    validation.errors.push_back(std::move(error));
}

[[nodiscard]] bool bounded(
    std::string_view value,
    std::size_t maximum,
    bool allow_empty = false) noexcept {
    return value.size() <= maximum && (allow_empty || !value.empty());
}

}  // namespace

std::uint32_t MigrationStatusCounts::total() const noexcept {
    std::uint32_t result = 0U;
    for (const auto value : by_status) result += value;
    return result;
}

std::uint32_t MigrationStatusCounts::count(MigrationItemStatus status) const noexcept {
    const auto index = static_cast<std::size_t>(status);
    return index < by_status.size() ? by_status[index] : 0U;
}

SoundSwitchCorpusManifest build_soundswitch_corpus_manifest(
    const SoundSwitchInspection& inspection,
    std::string source_version,
    SoundSwitchMigrationScope scope,
    bool authorized_corpus_available) {
    SoundSwitchCorpusManifest manifest;
    manifest.source_version = std::move(source_version);
    if (!authorized_corpus_available) {
        manifest.missing_dependency_codes.push_back(
            "authorized_soundswitch_corpus_unavailable");
        if (manifest.source_version.empty()) {
            manifest.missing_dependency_codes.push_back(
                "soundswitch.source_version_unverified");
        }
        normalize_soundswitch_corpus_manifest(manifest);
        return manifest;
    }

    manifest.artifacts.reserve(inspection.artifacts.size());
    for (const auto& source : inspection.artifacts) {
        MigrationSourceArtifact artifact;
        artifact.relative_path = source.relative_path;
        artifact.kind = source.kind;
        artifact.size = source.size;
        artifact.sha256 = source.sha256;
        artifact.role = source_role(source.kind, scope);
        artifact.availability = MigrationSourceAvailability::PresentVerified;
        artifact.artifact_id = artifact_identity(artifact);
        manifest.artifacts.push_back(std::move(artifact));
    }

    add_missing_if(!has_kind(manifest, SoundSwitchArtifactKind::ProjectManifest),
        manifest.missing_dependency_codes, "soundswitch.project_manifest_missing");
    add_missing_if(!has_kind(manifest, SoundSwitchArtifactKind::VenueDatabase),
        manifest.missing_dependency_codes, "soundswitch.venue_database_missing");
    add_missing_if(
        !has_kind(manifest, SoundSwitchArtifactKind::AutoloopDatabase) &&
            !has_kind(manifest, SoundSwitchArtifactKind::ExtendedAutoloopDatabase),
        manifest.missing_dependency_codes, "soundswitch.autoloop_database_missing");
    add_missing_if(!has_kind(manifest, SoundSwitchArtifactKind::TrackMap),
        manifest.missing_dependency_codes, "soundswitch.track_map_unavailable");
    add_missing_if(
        !has_kind(manifest, SoundSwitchArtifactKind::TrackScript) &&
            !has_kind(manifest, SoundSwitchArtifactKind::RecordableData),
        manifest.missing_dependency_codes, "soundswitch.lighting_files_unavailable");
    add_missing_if(!has_kind(manifest, SoundSwitchArtifactKind::Audio),
        manifest.missing_dependency_codes, "soundswitch.scripted_audio_unavailable");
    // The selected project root is not proof that an external DJ library was
    // searched. That evidence class remains explicitly unavailable here.
    manifest.missing_dependency_codes.push_back(
        "soundswitch.dj_library_identity_unavailable");
    add_missing_if(manifest.source_version.empty(), manifest.missing_dependency_codes,
        "soundswitch.source_version_unverified");
    normalize_soundswitch_corpus_manifest(manifest);
    return manifest;
}

void normalize_soundswitch_corpus_manifest(SoundSwitchCorpusManifest& manifest) {
    std::sort(manifest.artifacts.begin(), manifest.artifacts.end(),
        [](const auto& first, const auto& second) {
            if (first.relative_path != second.relative_path) {
                return first.relative_path < second.relative_path;
            }
            if (first.kind != second.kind) return first.kind < second.kind;
            return first.artifact_id < second.artifact_id;
        });
    sort_unique(manifest.missing_dependency_codes);
    manifest.bundle_id = bundle_identity(manifest);
}

void normalize_soundswitch_migration_report(SoundSwitchMigrationReport& report) {
    for (auto& item : report.items) {
        std::sort(item.evidence.begin(), item.evidence.end(),
            [](const auto& first, const auto& second) {
                if (first.artifact_id != second.artifact_id) {
                    return first.artifact_id < second.artifact_id;
                }
                if (first.has_byte_range != second.has_byte_range) {
                    return first.has_byte_range < second.has_byte_range;
                }
                if (first.offset != second.offset) return first.offset < second.offset;
                if (first.length != second.length) return first.length < second.length;
                if (first.decoder_id != second.decoder_id) {
                    return first.decoder_id < second.decoder_id;
                }
                return first.decoder_version < second.decoder_version;
            });
        sort_unique(item.warnings);
        sort_unique(item.blockers);
    }
    std::sort(report.items.begin(), report.items.end(), [](const auto& first, const auto& second) {
        if (first.item_id != second.item_id) return first.item_id < second.item_id;
        return first.item_kind < second.item_kind;
    });
    report.aggregate_counts = calculate_counts(report.items);
}

MigrationContractValidation validate_soundswitch_corpus_manifest(
    const SoundSwitchCorpusManifest& manifest) {
    MigrationContractValidation validation;
    if (manifest.format_version != kSoundSwitchCorpusManifestFormatVersion) {
        add_error(validation, "manifest.formatVersion");
    }
    if (!bounded(manifest.bundle_id, kMaximumIdentifierLength)) {
        add_error(validation, "manifest.bundleId");
    }
    if (!bounded(manifest.source_version, kMaximumVersionLength, true)) {
        add_error(validation, "manifest.sourceVersion");
    }
    if (manifest.artifacts.size() > kMaximumMigrationArtifacts) {
        add_error(validation, "manifest.artifactLimit");
    }
    if (manifest.missing_dependency_codes.size() > kMaximumDependencyCodes) {
        add_error(validation, "manifest.dependencyLimit");
    }
    std::set<std::string> artifact_ids;
    for (const auto& artifact : manifest.artifacts) {
        if (!bounded(artifact.artifact_id, kMaximumIdentifierLength) ||
            artifact.artifact_id != artifact_identity(artifact) ||
            !artifact_ids.insert(artifact.artifact_id).second) {
            add_error(validation, "manifest.artifactId");
        }
        if (!bounded(artifact.relative_path, kMaximumPathLength) ||
            !portable_relative_path(artifact.relative_path)) {
            add_error(validation, "manifest.relativePath");
        }
        if (artifact.availability == MigrationSourceAvailability::PresentVerified) {
            if (!is_sha256_digest(artifact.sha256)) {
                add_error(validation, "manifest.sha256");
            }
        } else if (!artifact.sha256.empty() && !is_sha256_digest(artifact.sha256)) {
            add_error(validation, "manifest.sha256");
        }
    }
    for (const auto& code : manifest.missing_dependency_codes) {
        if (!stable_code(code)) add_error(validation, "manifest.dependencyCode");
    }
    auto normalized = manifest;
    normalize_soundswitch_corpus_manifest(normalized);
    if (manifest.bundle_id != normalized.bundle_id) {
        add_error(validation, "manifest.bundleIdentity");
    }
    sort_unique(validation.errors);
    return validation;
}

MigrationContractValidation validate_soundswitch_migration_report(
    const SoundSwitchMigrationReport& report) {
    MigrationContractValidation validation;
    if (report.format_version != kSoundSwitchMigrationReportFormatVersion) {
        add_error(validation, "report.formatVersion");
    }
    if (!bounded(report.source_bundle_id, kMaximumIdentifierLength)) {
        add_error(validation, "report.sourceBundleId");
    }
    if (!bounded(report.source_version, kMaximumVersionLength, true)) {
        add_error(validation, "report.sourceVersion");
    }
    if (report.items.size() > kMaximumMigrationItems) {
        add_error(validation, "report.itemLimit");
    }
    std::set<std::string> item_ids;
    for (const auto& item : report.items) {
        const auto status_index = static_cast<std::size_t>(item.status);
        if (status_index >= static_cast<std::size_t>(MigrationItemStatus::Count)) {
            add_error(validation, "report.status");
        }
        if (!bounded(item.item_id, kMaximumIdentifierLength) ||
            !item_ids.insert(item.item_id).second) {
            add_error(validation, "report.itemId");
        }
        if (!bounded(item.item_kind, kMaximumIdentifierLength) ||
            !bounded(item.source_label, kMaximumLabelLength, true) ||
            !bounded(item.destination_ref, kMaximumIdentifierLength, true) ||
            !bounded(item.rule_id, kMaximumIdentifierLength, true)) {
            add_error(validation, "report.itemField");
        }
        if (item.evidence.size() > kMaximumMigrationEvidencePerItem ||
            item.warnings.size() > kMaximumMigrationMessagesPerItem ||
            item.blockers.size() > kMaximumMigrationMessagesPerItem) {
            add_error(validation, "report.itemLimit");
        }
        if (item.status == MigrationItemStatus::Approximated && item.warnings.empty()) {
            add_error(validation, "report.approximationWarning");
        }
        if (item.status == MigrationItemStatus::PreservedOpaque &&
            std::none_of(item.evidence.begin(), item.evidence.end(), [](const auto& evidence) {
                return !evidence.artifact_id.empty();
            })) {
            add_error(validation, "report.opaqueEvidence");
        }
        if (item.status == MigrationItemStatus::Conflicted &&
            !item.destination_ref.empty()) {
            add_error(validation, "report.conflictDestination");
        }
        if (item.status == MigrationItemStatus::MissingDependency &&
            (item.blockers.empty() ||
             !std::all_of(item.blockers.begin(), item.blockers.end(), stable_code))) {
            add_error(validation, "report.missingDependencyBlocker");
        }
        for (const auto& evidence : item.evidence) {
            if (!bounded(evidence.artifact_id, kMaximumIdentifierLength) ||
                !bounded(evidence.decoder_id, kMaximumIdentifierLength) ||
                !bounded(evidence.decoder_version, kMaximumVersionLength) ||
                (evidence.has_byte_range &&
                 (evidence.length == 0U || evidence.offset >
                    std::numeric_limits<std::uint64_t>::max() - evidence.length)) ||
                (!evidence.has_byte_range &&
                 (evidence.offset != 0U || evidence.length != 0U))) {
                add_error(validation, "report.evidence");
            }
        }
        for (const auto& message : item.warnings) {
            if (!bounded(message, kMaximumMessageLength)) {
                add_error(validation, "report.warning");
            }
        }
        for (const auto& message : item.blockers) {
            if (!bounded(message, kMaximumMessageLength)) {
                add_error(validation, "report.blocker");
            }
        }
    }
    const auto calculated = calculate_counts(report.items);
    if (calculated.by_status != report.aggregate_counts.by_status) {
        add_error(validation, "report.aggregateCounts");
    }
    sort_unique(validation.errors);
    return validation;
}

std::string serialize_soundswitch_corpus_manifest(
    const SoundSwitchCorpusManifest& manifest) {
    auto normalized = manifest;
    normalize_soundswitch_corpus_manifest(normalized);
    if (!validate_soundswitch_corpus_manifest(normalized)) return {};

    std::ostringstream output;
    output << "{\n  \"format\": ";
    append_json_string(output, kSoundSwitchCorpusManifestFormat);
    output << ",\n  \"formatVersion\": " << normalized.format_version
           << ",\n  \"bundleId\": ";
    append_json_string(output, normalized.bundle_id);
    output << ",\n  \"sourceVersion\": ";
    append_json_string(output, normalized.source_version);
    output << ",\n  \"artifacts\": [";
    for (std::size_t index = 0U; index < normalized.artifacts.size(); ++index) {
        const auto& artifact = normalized.artifacts[index];
        output << (index == 0U ? "\n" : ",\n") << "    {\"artifactId\": ";
        append_json_string(output, artifact.artifact_id);
        output << ", \"relativePath\": ";
        append_json_string(output, artifact.relative_path);
        output << ", \"kind\": ";
        append_json_string(output, soundswitch_artifact_kind_name(artifact.kind));
        output << ", \"size\": " << artifact.size << ", \"sha256\": ";
        append_json_string(output, artifact.sha256);
        output << ", \"role\": ";
        append_json_string(output, migration_source_role_name(artifact.role));
        output << ", \"availability\": ";
        append_json_string(output,
            migration_source_availability_name(artifact.availability));
        output << '}';
    }
    if (!normalized.artifacts.empty()) output << '\n';
    output << "  ],\n  \"missingDependencyCodes\": ";
    append_string_array(output, normalized.missing_dependency_codes, "  ");
    output << "\n}\n";
    return output.str();
}

std::string serialize_soundswitch_migration_report(
    const SoundSwitchMigrationReport& report) {
    auto normalized = report;
    normalize_soundswitch_migration_report(normalized);
    if (!validate_soundswitch_migration_report(normalized)) return {};

    std::ostringstream output;
    output << "{\n  \"format\": ";
    append_json_string(output, kSoundSwitchMigrationReportFormat);
    output << ",\n  \"formatVersion\": " << normalized.format_version
           << ",\n  \"sourceBundleId\": ";
    append_json_string(output, normalized.source_bundle_id);
    output << ",\n  \"sourceVersion\": ";
    append_json_string(output, normalized.source_version);
    output << ",\n  \"items\": [";
    for (std::size_t item_index = 0U; item_index < normalized.items.size(); ++item_index) {
        const auto& item = normalized.items[item_index];
        output << (item_index == 0U ? "\n" : ",\n") << "    {\"itemId\": ";
        append_json_string(output, item.item_id);
        output << ", \"itemKind\": "; append_json_string(output, item.item_kind);
        output << ", \"status\": "; append_json_string(output,
            migration_item_status_name(item.status));
        output << ", \"sourceLabel\": "; append_json_string(output, item.source_label);
        output << ", \"destinationRef\": ";
        append_json_string(output, item.destination_ref);
        output << ", \"ruleId\": "; append_json_string(output, item.rule_id);
        output << ", \"evidence\": [";
        for (std::size_t evidence_index = 0U;
             evidence_index < item.evidence.size(); ++evidence_index) {
            const auto& evidence = item.evidence[evidence_index];
            if (evidence_index != 0U) output << ", ";
            output << "{\"artifactId\": "; append_json_string(output, evidence.artifact_id);
            output << ", \"hasByteRange\": "
                   << (evidence.has_byte_range ? "true" : "false")
                   << ", \"offset\": " << evidence.offset
                   << ", \"length\": " << evidence.length
                   << ", \"decoderId\": "; append_json_string(output, evidence.decoder_id);
            output << ", \"decoderVersion\": ";
            append_json_string(output, evidence.decoder_version);
            output << '}';
        }
        output << "], \"warnings\": "; append_string_array(output, item.warnings, "    ");
        output << ", \"blockers\": "; append_string_array(output, item.blockers, "    ");
        output << '}';
    }
    if (!normalized.items.empty()) output << '\n';
    output << "  ],\n  \"aggregateCounts\": {";
    for (std::size_t index = 0U;
         index < static_cast<std::size_t>(MigrationItemStatus::Count); ++index) {
        if (index != 0U) output << ", ";
        const auto status = static_cast<MigrationItemStatus>(index);
        append_json_string(output, migration_item_status_name(status));
        output << ": " << normalized.aggregate_counts.by_status[index];
    }
    output << "}\n}\n";
    return output.str();
}

const char* migration_source_role_name(MigrationSourceRole role) noexcept {
    switch (role) {
    case MigrationSourceRole::Required: return "required";
    case MigrationSourceRole::Conditional: return "conditional";
    case MigrationSourceRole::Optional: return "optional";
    }
    return "optional";
}

bool parse_migration_source_role(
    std::string_view text,
    MigrationSourceRole& role) noexcept {
    if (text == "required") role = MigrationSourceRole::Required;
    else if (text == "conditional") role = MigrationSourceRole::Conditional;
    else if (text == "optional") role = MigrationSourceRole::Optional;
    else return false;
    return true;
}

const char* migration_source_availability_name(
    MigrationSourceAvailability availability) noexcept {
    switch (availability) {
    case MigrationSourceAvailability::PresentVerified: return "presentVerified";
    case MigrationSourceAvailability::Missing: return "missing";
    case MigrationSourceAvailability::Unreadable: return "unreadable";
    case MigrationSourceAvailability::RejectedUnsafe: return "rejectedUnsafe";
    }
    return "rejectedUnsafe";
}

bool parse_migration_source_availability(
    std::string_view text,
    MigrationSourceAvailability& availability) noexcept {
    if (text == "presentVerified") {
        availability = MigrationSourceAvailability::PresentVerified;
    } else if (text == "missing") {
        availability = MigrationSourceAvailability::Missing;
    } else if (text == "unreadable") {
        availability = MigrationSourceAvailability::Unreadable;
    } else if (text == "rejectedUnsafe") {
        availability = MigrationSourceAvailability::RejectedUnsafe;
    } else return false;
    return true;
}

const char* migration_item_status_name(MigrationItemStatus status) noexcept {
    switch (status) {
    case MigrationItemStatus::Exact: return "exact";
    case MigrationItemStatus::DeterministicallyTranslated:
        return "deterministicallyTranslated";
    case MigrationItemStatus::Approximated: return "approximated";
    case MigrationItemStatus::PreservedOpaque: return "preservedOpaque";
    case MigrationItemStatus::Unsupported: return "unsupported";
    case MigrationItemStatus::Conflicted: return "conflicted";
    case MigrationItemStatus::MissingDependency: return "missingDependency";
    case MigrationItemStatus::RejectedUnsafe: return "rejectedUnsafe";
    case MigrationItemStatus::Count: break;
    }
    return "unsupported";
}

bool parse_migration_item_status(
    std::string_view text,
    MigrationItemStatus& status) noexcept {
    if (text == "exact") status = MigrationItemStatus::Exact;
    else if (text == "deterministicallyTranslated") {
        status = MigrationItemStatus::DeterministicallyTranslated;
    } else if (text == "approximated") status = MigrationItemStatus::Approximated;
    else if (text == "preservedOpaque") status = MigrationItemStatus::PreservedOpaque;
    else if (text == "unsupported") status = MigrationItemStatus::Unsupported;
    else if (text == "conflicted") status = MigrationItemStatus::Conflicted;
    else if (text == "missingDependency") status = MigrationItemStatus::MissingDependency;
    else if (text == "rejectedUnsafe") status = MigrationItemStatus::RejectedUnsafe;
    else return false;
    return true;
}

}  // namespace emberlights
