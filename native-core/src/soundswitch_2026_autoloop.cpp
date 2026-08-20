#include "emberlights/soundswitch_2026_autoloop.hpp"

#include "emberlights/autoloop_persistence.hpp"
#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

inline constexpr std::uint32_t kSoundSwitchMagic = 0x5509AAAAU;
inline constexpr std::uint32_t kSoundSwitchFormatVersion = 3U;
inline constexpr std::uint32_t kMaximumCatalogEntries = 4096U;
inline constexpr std::uint32_t kMaximumCatalogPlacements = 4096U;
inline constexpr std::uint32_t kMaximumTimelineRecords = 4096U;
inline constexpr std::uint32_t kTimelineSentinel = 0x80808080U;
inline constexpr std::uint64_t kMaximumTimelineMilliseconds =
    24ULL * 60ULL * 60ULL * 1000ULL;

inline constexpr std::string_view kProjectManifestSha256 =
    "2e4f37da9a5da2af6f255525e63af725a1310b7e1efe4e7448314167e4e05b47";
inline constexpr std::string_view kVenueSha256 =
    "22c78d611b5d9a005a615d5d6d90a2063647badc974e41c05a595fce8366c508";
inline constexpr std::string_view kPrimaryCatalogSha256 =
    "27abb5c0d0232e79673ea09b7812bae1ac796a8f895aad0c742da69320a23560";
inline constexpr std::string_view kExtendedCatalogSha256 =
    "a2f57b68f945942a4083373d4983cca1232604e5218dd55926af2d3ff51a4209";

inline constexpr std::string_view kCatalogDecoderId =
    "soundswitch.autoloop-catalog-v3";
inline constexpr std::string_view kTimelineDecoderId =
    "soundswitch.autoloop-timeline-v3";
inline constexpr std::string_view kVenueDecoderId =
    "soundswitch.venue-current-2026-layout";
inline constexpr std::string_view kDecoderVersion = "1";

inline constexpr std::string_view kAssetId = "a1";
inline constexpr std::string_view kPlacementId = "x1";
inline constexpr std::string_view kProgramId = "p1";
inline constexpr std::string_view kLaunchId = "ss2026.launch";
inline constexpr std::string_view kProvenanceId = "v1";
inline constexpr std::string_view kProducerId =
    "soundswitch.timeline-v210.current-2026";

class ByteCursor {
public:
    explicit ByteCursor(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] bool at_end() const noexcept { return offset_ == bytes_.size(); }

    [[nodiscard]] bool read_u8(std::uint8_t& value) noexcept {
        if (offset_ >= bytes_.size()) return false;
        value = bytes_[offset_++];
        return true;
    }

    [[nodiscard]] bool read_u32(std::uint32_t& value) noexcept {
        if (offset_ > bytes_.size() || bytes_.size() - offset_ < 4U) {
            return false;
        }
        value = load_u32(bytes_, offset_);
        offset_ += 4U;
        return true;
    }

    [[nodiscard]] bool read_utf16_ascii(std::string& value) {
        std::uint32_t code_units = 0U;
        if (!read_u32(code_units) || code_units == 0U || code_units > 4096U ||
            static_cast<std::uint64_t>(code_units) * 2ULL >
                bytes_.size() - offset_) {
            return false;
        }
        value.clear();
        value.reserve(code_units - 1U);
        for (std::uint32_t index = 0U; index < code_units; ++index) {
            const auto low = bytes_[offset_++];
            const auto high = bytes_[offset_++];
            if (high != 0U || (index + 1U == code_units) != (low == 0U)) {
                return false;
            }
            if (low != 0U) value.push_back(static_cast<char>(low));
        }
        return true;
    }

    [[nodiscard]] static std::uint32_t load_u32(
        std::span<const std::uint8_t> bytes,
        std::size_t offset) noexcept {
        return static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
            (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
            (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{0U};
};

[[nodiscard]] MigrationEvidenceRef evidence(
    std::string artifact_id,
    std::uint64_t offset,
    std::uint64_t length,
    std::string_view decoder_id) {
    return {
        std::move(artifact_id), offset, length, true,
        std::string(decoder_id), std::string(kDecoderVersion)};
}

[[nodiscard]] bool bounded_range(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::size_t length) noexcept {
    return offset <= bytes.size() && length <= bytes.size() - offset;
}

[[nodiscard]] std::uint32_t u32_at(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    return ByteCursor::load_u32(bytes, offset);
}

[[nodiscard]] bool has_u32(
    std::span<const std::uint8_t> bytes,
    std::size_t offset,
    std::uint32_t expected) noexcept {
    return bounded_range(bytes, offset, 4U) && u32_at(bytes, offset) == expected;
}

[[nodiscard]] bool read_bounded_file(
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes,
    std::vector<std::uint8_t>& bytes,
    std::string& message) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        message = "A required SoundSwitch source artifact is missing.";
        return false;
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error || size > maximum_bytes ||
        size > static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max())) {
        message = "A required SoundSwitch source artifact exceeds its safe read limit.";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        message = "A required SoundSwitch source artifact could not be opened read-only.";
        return false;
    }
    bytes.assign(static_cast<std::size_t>(size), std::uint8_t{0});
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    }
    if ((!input && !input.eof()) ||
        static_cast<std::size_t>(input.gcount()) != bytes.size()) {
        message = "A required SoundSwitch source artifact could not be read completely.";
        bytes.clear();
        return false;
    }
    return true;
}

[[nodiscard]] std::string artifact_identity(
    SoundSwitchArtifactKind kind,
    std::string_view relative_path,
    std::uint64_t size,
    std::string_view digest) {
    std::string identity;
    identity.reserve(relative_path.size() + digest.size() + 64U);
    identity.append(soundswitch_artifact_kind_name(kind));
    identity.push_back('\n');
    identity.append(relative_path);
    identity.push_back('\n');
    identity.append(std::to_string(size));
    identity.push_back('\n');
    identity.append(digest);
    return "ssa1-" + sha256_text(identity);
}

[[nodiscard]] MigrationSourceArtifact make_artifact(
    std::string relative_path,
    SoundSwitchArtifactKind kind,
    const std::vector<std::uint8_t>& bytes,
    MigrationSourceRole role) {
    MigrationSourceArtifact artifact;
    artifact.relative_path = std::move(relative_path);
    artifact.kind = kind;
    artifact.size = bytes.size();
    artifact.sha256 = sha256_bytes(bytes);
    artifact.role = role;
    artifact.availability = MigrationSourceAvailability::PresentVerified;
    artifact.artifact_id = artifact_identity(
        artifact.kind, artifact.relative_path, artifact.size, artifact.sha256);
    return artifact;
}

[[nodiscard]] const MigrationSourceArtifact* find_artifact(
    const SoundSwitchCorpusManifest& manifest,
    std::string_view path) noexcept {
    const auto found = std::find_if(
        manifest.artifacts.begin(), manifest.artifacts.end(),
        [&](const auto& artifact) { return artifact.relative_path == path; });
    return found == manifest.artifacts.end() ? nullptr : &*found;
}

void add_report_item(
    SoundSwitchMigrationReport& report,
    MigrationItem item) {
    report.items.push_back(std::move(item));
}

[[nodiscard]] MigrationItem make_missing_item(
    std::string item_id,
    std::string item_kind,
    std::string source_label,
    std::string destination_ref,
    std::string blocker,
    std::vector<MigrationEvidenceRef> evidence_refs = {}) {
    MigrationItem item;
    item.item_id = std::move(item_id);
    item.item_kind = std::move(item_kind);
    item.status = MigrationItemStatus::MissingDependency;
    item.source_label = std::move(source_label);
    item.destination_ref = std::move(destination_ref);
    item.rule_id = "soundswitch.current-2026.fail-closed";
    item.evidence = std::move(evidence_refs);
    item.blockers.push_back(std::move(blocker));
    return item;
}

[[nodiscard]] std::vector<std::uint32_t> expected_source_targets() {
    std::vector<std::uint32_t> targets{197U, 198U, 199U, 200U, 201U};
    for (const auto group : {89U, 106U, 123U, 140U}) {
        targets.push_back(group);
        for (std::uint32_t cell = 1U; cell <= 16U; ++cell) {
            targets.push_back(group + cell);
        }
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    return targets;
}

[[nodiscard]] bool is_uplight(std::uint32_t target_id) noexcept {
    return target_id >= 198U && target_id <= 201U;
}

[[nodiscard]] bool is_tube_cell(std::uint32_t target_id) noexcept {
    return (target_id >= 90U && target_id <= 105U) ||
        (target_id >= 107U && target_id <= 122U) ||
        (target_id >= 124U && target_id <= 139U) ||
        (target_id >= 141U && target_id <= 156U);
}

[[nodiscard]] std::string default_destination_ref(std::uint32_t target_id) {
    if (is_uplight(target_id)) {
        return "uplight-" + std::to_string(target_id - 197U);
    }
    constexpr std::array<std::uint32_t, 4U> first_cells{{90U, 107U, 124U, 141U}};
    for (std::size_t tube = 0U; tube < first_cells.size(); ++tube) {
        if (target_id >= first_cells[tube] &&
            target_id < first_cells[tube] + 16U) {
            return "tube-" + std::to_string(tube + 1U) + "-cell-" +
                std::to_string(target_id - first_cells[tube] + 1U);
        }
    }
    return {};
}

[[nodiscard]] const FixtureDefinition* find_fixture(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto fixture = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [&](const auto& candidate) { return candidate.id == id; });
    return fixture == project.fixtures.end() ? nullptr : &*fixture;
}

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == id; });
    return profile == project.fixture_profiles.end() ? nullptr : &*profile;
}

[[nodiscard]] bool profile_has_property(
    const FixtureProfileDefinition* profile,
    showcore::Property property) noexcept {
    return profile != nullptr && std::any_of(
        profile->channels.begin(), profile->channels.end(),
        [&](const auto& channel) {
            return channel.property == property &&
                channel.encoding != showcore::ChannelEncoding::Constant8;
        });
}

[[nodiscard]] MusicalTick milliseconds_to_ticks(std::uint32_t value) noexcept {
    // 1200 ms / quarter and 960 PPQ reduce exactly to 4/5. Add half the
    // denominator for deterministic nearest-tick rounding.
    return static_cast<MusicalTick>(
        (static_cast<std::uint64_t>(value) * 4ULL + 2ULL) / 5ULL);
}

[[nodiscard]] float normalized_byte(std::uint32_t value) noexcept {
    return static_cast<float>(value & 0xFFU) / 255.0F;
}

[[nodiscard]] float red_from_rgb(std::uint32_t value) noexcept {
    return normalized_byte(value >> 16U);
}

[[nodiscard]] float green_from_rgb(std::uint32_t value) noexcept {
    return normalized_byte(value >> 8U);
}

[[nodiscard]] float blue_from_rgb(std::uint32_t value) noexcept {
    return normalized_byte(value);
}

[[nodiscard]] float intensity_at(
    const std::vector<SoundSwitchAutoloopARecord>& records,
    std::uint32_t timestamp_ms) noexcept {
    if (records.empty()) return 1.0F;
    const SoundSwitchAutoloopARecord* left = &records.front();
    std::size_t index = 0U;
    while (index < records.size() &&
           records[index].timestamp_a_ms <= timestamp_ms) {
        left = &records[index];
        ++index;
    }
    while (index < records.size() &&
           records[index].timestamp_a_ms == left->timestamp_a_ms) {
        left = &records[index++];
    }
    if (timestamp_ms <= left->timestamp_a_ms || index >= records.size()) {
        return left->normalized_value();
    }
    while (index < records.size() &&
           records[index].timestamp_a_ms == left->timestamp_a_ms) {
        ++index;
    }
    if (index >= records.size()) return left->normalized_value();
    const auto& right = records[index];
    if (right.timestamp_a_ms <= left->timestamp_a_ms) {
        return right.normalized_value();
    }
    const auto amount = static_cast<float>(timestamp_ms - left->timestamp_a_ms) /
        static_cast<float>(right.timestamp_a_ms - left->timestamp_a_ms);
    return std::clamp(
        left->normalized_value() +
            (right.normalized_value() - left->normalized_value()) * amount,
        0.0F, 1.0F);
}

void append_intensity_events(
    AutoloopProgramDefinition& program,
    std::string_view lane_id,
    std::uint32_t target_id,
    const std::vector<SoundSwitchAutoloopARecord>& records) {
    for (std::size_t index = 0U; index + 1U < records.size(); ++index) {
        const auto& first = records[index];
        const auto& second = records[index + 1U];
        if (second.timestamp_a_ms <= first.timestamp_a_ms) continue;
        const auto start = milliseconds_to_ticks(first.timestamp_a_ms);
        const auto end = std::min(
            program.length_ticks,
            milliseconds_to_ticks(second.timestamp_a_ms));
        if (start < 0 || end <= start || start >= program.length_ticks) continue;
        AutoloopEventDefinition event;
        event.id = "p1ei" + std::to_string(target_id) + "." +
            std::to_string(index);
        event.lane_id = std::string(lane_id);
        event.kind = AutoloopEventKind::PropertyCurve;
        event.start_tick = start;
        event.end_tick = end;
        event.property = showcore::Property::Intensity;
        event.value = showcore::PropertyValue::release();
        event.interpolation = AutoloopInterpolation::Linear;
        event.curve_points = {
            {start, showcore::PropertyValue::set(first.normalized_value())},
            {end, showcore::PropertyValue::set(second.normalized_value())}};
        program.events.push_back(std::move(event));
    }
}

void append_color_component_event(
    AutoloopProgramDefinition& program,
    std::string_view lane_id,
    std::uint32_t target_id,
    std::size_t record_index,
    showcore::Property property,
    std::string_view component,
    MusicalTick start,
    MusicalTick end,
    float first,
    float second) {
    if (end <= start) return;
    AutoloopEventDefinition event;
    event.id = "p1ec" + std::to_string(target_id) + "." +
        std::string(component) + "." + std::to_string(record_index);
    event.lane_id = std::string(lane_id);
    event.start_tick = start;
    event.end_tick = end;
    event.property = property;
    if (std::fabs(first - second) <= 0.000001F) {
        event.kind = AutoloopEventKind::PropertyBlock;
        event.value = showcore::PropertyValue::set(first);
        event.interpolation = AutoloopInterpolation::Hold;
    } else {
        event.kind = AutoloopEventKind::PropertyCurve;
        event.value = showcore::PropertyValue::release();
        event.interpolation = AutoloopInterpolation::Linear;
        event.curve_points = {
            {start, showcore::PropertyValue::set(first)},
            {end, showcore::PropertyValue::set(second)}};
    }
    program.events.push_back(std::move(event));
}

void append_tube_color_events(
    AutoloopProgramDefinition& program,
    std::string_view lane_id,
    const SoundSwitchAutoloopTargetRecords& target) {
    for (std::size_t index = 0U; index < target.color_records.size(); ++index) {
        const auto& record = target.color_records[index];
        const auto start = milliseconds_to_ticks(record.start_ms);
        const auto end = std::min(
            program.length_ticks, milliseconds_to_ticks(record.end_ms));
        if (start < 0 || end <= start || start >= program.length_ticks) continue;
        const auto first_intensity = intensity_at(
            target.intensity_records, record.start_ms);
        const auto second_intensity = intensity_at(
            target.intensity_records, record.end_ms);
        append_color_component_event(
            program, lane_id, target.source_target_id, index,
            showcore::Property::Red, "red", start, end,
            red_from_rgb(record.rgb_start_raw) * first_intensity,
            red_from_rgb(record.rgb_end_raw) * second_intensity);
        append_color_component_event(
            program, lane_id, target.source_target_id, index,
            showcore::Property::Green, "green", start, end,
            green_from_rgb(record.rgb_start_raw) * first_intensity,
            green_from_rgb(record.rgb_end_raw) * second_intensity);
        append_color_component_event(
            program, lane_id, target.source_target_id, index,
            showcore::Property::Blue, "blue", start, end,
            blue_from_rgb(record.rgb_start_raw) * first_intensity,
            blue_from_rgb(record.rgb_end_raw) * second_intensity);
    }
}

[[nodiscard]] bool launch_matches(
    const AutoloopLaunchProfileDefinition& first,
    const AutoloopLaunchProfileDefinition& second) noexcept {
    return first.id == second.id && first.repeat == second.repeat &&
        first.launch == second.launch &&
        first.phase_origin == second.phase_origin &&
        first.mode == second.mode &&
        first.return_fade_ticks == second.return_fade_ticks &&
        first.track_boundary_required == second.track_boundary_required;
}

template <typename Value>
[[nodiscard]] bool contains_id(
    const std::vector<Value>& values,
    std::string_view id) {
    return std::any_of(values.begin(), values.end(), [&](const auto& value) {
        return value.id == id;
    });
}

[[nodiscard]] bool merge_imported_source(
    AutoloopSourceDocument& base,
    AutoloopSourceDocument imported,
    std::string& message) {
    const auto provenance = std::find_if(
        base.provenance.begin(), base.provenance.end(),
        [](const auto& value) { return value.id == kProvenanceId; });
    const bool owns_existing_slice = provenance != base.provenance.end() &&
        provenance->origin == AutoloopProvenanceOrigin::Migrated &&
        provenance->source_artifact_id.find("SSAutoLoop1") !=
            std::string::npos;
    const bool has_slice_ids = contains_id(base.assets, kAssetId) ||
        contains_id(base.placements, kPlacementId) ||
        contains_id(base.programs, kProgramId) ||
        contains_id(base.provenance, kProvenanceId);
    if (has_slice_ids && !owns_existing_slice) {
        message = "The stable SoundSwitch slice IDs are already owned by unrelated content.";
        return false;
    }
    if (owns_existing_slice) {
        const auto shared_program = std::any_of(
            base.assets.begin(), base.assets.end(), [](const auto& asset) {
                return asset.id != kAssetId && asset.program_id == kProgramId;
            });
        const auto shared_asset = std::any_of(
            base.placements.begin(), base.placements.end(),
            [](const auto& placement) {
                return placement.id != kPlacementId &&
                    placement.asset_id == kAssetId;
            });
        const auto shared_provenance = std::any_of(
            base.assets.begin(), base.assets.end(), [](const auto& asset) {
                return asset.id != kAssetId &&
                    asset.provenance_id == kProvenanceId;
            });
        if (shared_program || shared_asset || shared_provenance) {
            message = "The existing SoundSwitch slice has dependencies outside its owned records.";
            return false;
        }
    }
    const auto occupied = std::find_if(
        base.placements.begin(), base.placements.end(),
        [](const auto& placement) {
            return placement.bank == 0U && placement.slot == 0U &&
                placement.id != kPlacementId;
        });
    if (occupied != base.placements.end()) {
        message = "Medium slot 1 is occupied by content outside this SoundSwitch slice.";
        return false;
    }

    auto expected_launch = imported.launch_profiles.front();
    const auto existing_launch = std::find_if(
        base.launch_profiles.begin(), base.launch_profiles.end(),
        [](const auto& launch) { return launch.id == kLaunchId; });
    if (existing_launch != base.launch_profiles.end() &&
        !launch_matches(*existing_launch, expected_launch)) {
        message = "The shared SoundSwitch launch profile has incompatible semantics.";
        return false;
    }

    std::erase_if(base.assets, [](const auto& value) { return value.id == kAssetId; });
    std::erase_if(base.placements, [](const auto& value) { return value.id == kPlacementId; });
    std::erase_if(base.programs, [](const auto& value) { return value.id == kProgramId; });
    std::erase_if(base.provenance, [](const auto& value) { return value.id == kProvenanceId; });
    if (existing_launch == base.launch_profiles.end()) {
        base.launch_profiles.push_back(std::move(expected_launch));
    }
    base.assets.push_back(std::move(imported.assets.front()));
    base.placements.push_back(std::move(imported.placements.front()));
    base.programs.push_back(std::move(imported.programs.front()));
    base.provenance.push_back(std::move(imported.provenance.front()));
    normalize_autoloop_source(base);
    return true;
}

}  // namespace

float SoundSwitchAutoloopARecord::normalized_value() const noexcept {
    return static_cast<float>(
        static_cast<long double>(value_raw) /
        static_cast<long double>(std::numeric_limits<std::uint32_t>::max()));
}

SoundSwitchAutoloopCatalogDecode decode_soundswitch_v3_autoloop_catalog(
    std::span<const std::uint8_t> bytes,
    std::string artifact_id) {
    SoundSwitchAutoloopCatalogDecode result;
    ByteCursor cursor(bytes);
    std::uint32_t magic = 0U;
    std::uint32_t version = 0U;
    std::uint32_t reserved_a = 0U;
    std::uint32_t reserved_b = 0U;
    std::uint32_t database_kind = 0U;
    std::uint32_t bank_count = 0U;
    if (!cursor.read_u32(magic) || !cursor.read_u32(version) ||
        !cursor.read_u32(reserved_a) || !cursor.read_u32(reserved_b) ||
        !cursor.read_u32(database_kind) || !cursor.read_u32(bank_count) ||
        magic != kSoundSwitchMagic || version != kSoundSwitchFormatVersion ||
        reserved_a != 0U || reserved_b != 0U ||
        (database_kind != 2U && database_kind != 3U) || bank_count != 4U) {
        result.message = "The Autoloop catalog header is not recognized format 3.";
        return result;
    }
    result.catalog.database_kind = database_kind;
    for (std::uint32_t bank = 0U; bank < bank_count; ++bank) {
        std::uint32_t tag = 0U;
        std::string name;
        if (!cursor.read_u32(tag) || tag != 1U ||
            !cursor.read_utf16_ascii(name)) {
            result.message = "An Autoloop bank label is malformed.";
            return result;
        }
        result.catalog.bank_names.push_back(std::move(name));
    }
    std::uint32_t entry_count = 0U;
    if (!cursor.read_u32(entry_count) || entry_count > kMaximumCatalogEntries) {
        result.message = "The Autoloop catalog entry count is unsafe.";
        return result;
    }
    std::map<std::uint32_t, std::uint32_t> entry_banks;
    std::uint32_t first_entry_index =
        std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t index = 0U; index < entry_count; ++index) {
        const auto offset = cursor.offset();
        std::uint32_t tag = 0U;
        std::uint32_t source_index = 0U;
        std::uint32_t bars = 0U;
        std::uint32_t record_version = 0U;
        std::uint32_t bank = 0U;
        std::string name;
        if (!cursor.read_u32(tag) || !cursor.read_u32(source_index) ||
            !cursor.read_u32(bars) || !cursor.read_u32(record_version) ||
            !cursor.read_utf16_ascii(name) || !cursor.read_u32(bank) ||
            tag != 2U || record_version != 1U || bars == 0U || bars > 64U ||
            bank >= bank_count ||
            !entry_banks.emplace(source_index, bank).second) {
            result.message = "An Autoloop catalog entry is malformed or duplicated.";
            return result;
        }
        result.catalog.entries.push_back({
            source_index, bars, bank, std::move(name),
            evidence(
                artifact_id, offset, cursor.offset() - offset,
                kCatalogDecoderId)});
        first_entry_index = std::min(first_entry_index, source_index);
    }
    std::set<std::uint32_t> placed_indices;
    for (std::uint32_t bank = 0U; bank < bank_count; ++bank) {
        std::uint32_t placement_count = 0U;
        if (!cursor.read_u32(placement_count) ||
            placement_count > kMaximumCatalogPlacements) {
            result.message = "An Autoloop placement count is unsafe.";
            return result;
        }
        std::set<std::uint32_t> bank_indices;
        for (std::uint32_t slot = 0U; slot < placement_count; ++slot) {
            const auto offset = cursor.offset();
            std::uint32_t source_index = 0U;
            if (!cursor.read_u32(source_index)) {
                result.message = "An Autoloop placement is malformed or duplicated.";
                return result;
            }
            const auto entry_bank = entry_banks.find(source_index);
            const bool inherited_primary_entry = database_kind == 3U &&
                entry_bank == entry_banks.end() &&
                source_index < first_entry_index;
            if (!bank_indices.insert(source_index).second ||
                !placed_indices.insert(source_index).second ||
                (entry_bank == entry_banks.end() && !inherited_primary_entry) ||
                (entry_bank != entry_banks.end() && entry_bank->second != bank)) {
                result.message = "An Autoloop placement is malformed or duplicated.";
                return result;
            }
            result.catalog.placements.push_back({
                bank, slot, source_index,
                evidence(artifact_id, offset, 4U, kCatalogDecoderId)});
        }
        std::uint8_t marker = 0U;
        if (!cursor.read_u8(marker) || marker != 1U) {
            result.message = "An Autoloop placement array terminator is malformed.";
            return result;
        }
    }
    const bool every_local_entry_placed = std::all_of(
        entry_banks.begin(), entry_banks.end(), [&](const auto& entry) {
            return placed_indices.contains(entry.first);
        });
    if (!cursor.at_end() || !every_local_entry_placed ||
        (database_kind == 2U && placed_indices.size() != entry_banks.size())) {
        result.message = "The Autoloop catalog has unclaimed trailing bytes.";
        return result;
    }
    result.success = true;
    result.message = "Decoded the SoundSwitch format-3 Autoloop catalog.";
    return result;
}

SoundSwitchAutoloopTimelineDecode decode_soundswitch_v3_autoloop_timeline(
    std::span<const std::uint8_t> bytes,
    std::span<const std::uint32_t> expected_target_ids,
    std::string artifact_id) {
    SoundSwitchAutoloopTimelineDecode result;
    if (!bounded_range(bytes, 0U, 16U) ||
        u32_at(bytes, 0U) != kSoundSwitchMagic ||
        u32_at(bytes, 4U) != kSoundSwitchFormatVersion ||
        u32_at(bytes, 8U) >= bytes.size() ||
        u32_at(bytes, 12U) >= bytes.size()) {
        result.message = "The Autoloop timeline header is not recognized format 3.";
        return result;
    }
    std::set<std::uint32_t> requested;
    for (const auto target_id : expected_target_ids) {
        if (!requested.insert(target_id).second) {
            result.message = "The requested target list contains duplicates.";
            return result;
        }
        SoundSwitchAutoloopTargetRecords target;
        target.source_target_id = target_id;
        std::vector<std::size_t> matches;
        for (std::size_t offset = 0U; offset + 24U <= bytes.size(); ++offset) {
            if (has_u32(bytes, offset, target_id) &&
                has_u32(bytes, offset + 4U, 4U) &&
                has_u32(bytes, offset + 8U, 1U) &&
                has_u32(bytes, offset + 12U, 1U) &&
                has_u32(bytes, offset + 16U, 0U)) {
                matches.push_back(offset);
            }
        }
        if (matches.empty()) {
            result.targets.push_back(std::move(target));
            continue;
        }
        if (matches.size() != 1U) {
            result.message = "A requested Autoloop target block is ambiguous.";
            return result;
        }
        const auto header = matches.front();
        const auto a_count = u32_at(bytes, header + 20U);
        if (a_count > kMaximumTimelineRecords ||
            static_cast<std::uint64_t>(a_count) * 17ULL >
                bytes.size() - (header + 24U)) {
            result.message = "An Autoloop A-record count is unsafe.";
            return result;
        }
        target.present = true;
        target.header_evidence = evidence(
            artifact_id, header, 24U, kTimelineDecoderId);
        auto record_offset = header + 24U;
        std::uint32_t previous_timestamp = 0U;
        bool has_previous = false;
        for (std::uint32_t index = 0U; index < a_count; ++index) {
            if (!bounded_range(bytes, record_offset, 17U)) {
                result.message = "An Autoloop A record is truncated.";
                return result;
            }
            SoundSwitchAutoloopARecord record;
            record.record_tag = u32_at(bytes, record_offset);
            record.timestamp_a_ms = u32_at(bytes, record_offset + 4U);
            record.timestamp_b_ms = u32_at(bytes, record_offset + 8U);
            record.value_raw = u32_at(bytes, record_offset + 12U);
            record.trailing_raw = bytes[record_offset + 16U];
            record.evidence = evidence(
                artifact_id, record_offset, 17U, kTimelineDecoderId);
            if (record.record_tag != 2U ||
                record.timestamp_a_ms != record.timestamp_b_ms ||
                record.timestamp_a_ms > kMaximumTimelineMilliseconds ||
                (has_previous && record.timestamp_a_ms < previous_timestamp)) {
                result.message = "An Autoloop A record violates the bounded timeline grammar.";
                return result;
            }
            previous_timestamp = record.timestamp_a_ms;
            has_previous = true;
            target.intensity_records.push_back(std::move(record));
            record_offset += 17U;
        }

        std::size_t b_count_offset = bytes.size();
        std::uint32_t b_count = 0U;
        const auto search_end = std::min(bytes.size(), record_offset + 80U);
        for (std::size_t sentinel = record_offset;
             sentinel + 12U <= search_end; ++sentinel) {
            if (!has_u32(bytes, sentinel, kTimelineSentinel) ||
                !has_u32(bytes, sentinel + 4U, 1U)) {
                continue;
            }
            const auto candidate_count = u32_at(bytes, sentinel + 8U);
            if (candidate_count > kMaximumTimelineRecords ||
                static_cast<std::uint64_t>(candidate_count) * 36ULL >
                    bytes.size() - (sentinel + 12U)) {
                continue;
            }
            b_count_offset = sentinel + 8U;
            b_count = candidate_count;
            break;
        }
        if (b_count_offset == bytes.size()) {
            result.message = "The Autoloop B-record table marker is missing.";
            return result;
        }
        record_offset = b_count_offset + 4U;
        std::uint32_t previous_start = 0U;
        has_previous = false;
        for (std::uint32_t index = 0U; index < b_count; ++index) {
            if (!bounded_range(bytes, record_offset, 36U)) {
                result.message = "An Autoloop B record is truncated.";
                return result;
            }
            SoundSwitchAutoloopBRecord record;
            record.record_tag = u32_at(bytes, record_offset);
            record.record_kind = u32_at(bytes, record_offset + 4U);
            record.record_version = u32_at(bytes, record_offset + 8U);
            record.start_ms = u32_at(bytes, record_offset + 12U);
            record.end_ms = u32_at(bytes, record_offset + 16U);
            record.rgb_start_raw = u32_at(bytes, record_offset + 20U);
            record.direct_start_raw = u32_at(bytes, record_offset + 24U);
            record.rgb_end_raw = u32_at(bytes, record_offset + 28U);
            record.direct_end_raw = u32_at(bytes, record_offset + 32U);
            record.evidence = evidence(
                artifact_id, record_offset, 36U, kTimelineDecoderId);
            if (record.record_tag != 1U || record.record_kind != 2U ||
                record.record_version != 1U || record.end_ms <= record.start_ms ||
                record.end_ms > kMaximumTimelineMilliseconds ||
                (has_previous && record.start_ms < previous_start)) {
                result.message = "An Autoloop B record violates the bounded timeline grammar.";
                return result;
            }
            previous_start = record.start_ms;
            has_previous = true;
            target.color_records.push_back(std::move(record));
            record_offset += 36U;
        }
        result.targets.push_back(std::move(target));
    }
    result.success = true;
    result.message = "Decoded the requested SoundSwitch format-3 target records.";
    return result;
}

std::vector<SoundSwitch2026DestinationBinding>
current_soundswitch_2026_destination_bindings() {
    std::vector<SoundSwitch2026DestinationBinding> bindings;
    for (const auto target_id : expected_source_targets()) {
        if (!is_uplight(target_id) && !is_tube_cell(target_id)) continue;
        bindings.push_back({target_id, default_destination_ref(target_id)});
    }
    return bindings;
}

SoundSwitch2026RedSmoothProposal build_soundswitch_2026_red_smooth_proposal(
    const std::filesystem::path& source_root,
    const ProjectDocument& destination_project,
    const SoundSwitch2026RedSmoothOptions& options) {
    SoundSwitch2026RedSmoothProposal proposal;
    const auto fail = [&](SoundSwitch2026RedSmoothError error,
                          std::string message) {
        proposal.error = error;
        proposal.message = std::move(message);
        return proposal;
    };
    if (options.archive_sha256 != kSoundSwitch2026ArchiveSha256) {
        return fail(
            SoundSwitch2026RedSmoothError::UnsupportedSource,
            "The source-bundle archive identity is not the reviewed current 2026 project.");
    }

    std::vector<std::uint8_t> marker;
    std::vector<std::uint8_t> venue;
    std::vector<std::uint8_t> primary_catalog_bytes;
    std::vector<std::uint8_t> extended_catalog_bytes;
    std::vector<std::uint8_t> timeline_bytes;
    std::string read_message;
    const std::array<std::pair<std::filesystem::path, std::uint64_t>, 5U> paths{{
        {source_root / ".ssproj", 64U * 1024U},
        {source_root / "SoundSwitchVenues.bin", 4U * 1024U * 1024U},
        {source_root / "SoundSwitchAutoLoops.bin", 256U * 1024U},
        {source_root / "SoundSwitchAutoLoopsEx.bin", 256U * 1024U},
        {source_root / "SSAutoLoop1.ssfile", 4U * 1024U * 1024U}}};
    std::array<std::vector<std::uint8_t>*, 5U> outputs{{
        &marker, &venue, &primary_catalog_bytes, &extended_catalog_bytes,
        &timeline_bytes}};
    for (std::size_t index = 0U; index < paths.size(); ++index) {
        if (!read_bounded_file(
                paths[index].first, paths[index].second,
                *outputs[index], read_message)) {
            return fail(
                std::filesystem::exists(paths[index].first)
                    ? SoundSwitch2026RedSmoothError::ReadFailed
                    : SoundSwitch2026RedSmoothError::MissingSource,
                read_message);
        }
    }
    const std::array<std::pair<const std::vector<std::uint8_t>*,
        std::string_view>, 5U> expected_digests{{
        {&marker, kProjectManifestSha256},
        {&venue, kVenueSha256},
        {&primary_catalog_bytes, kPrimaryCatalogSha256},
        {&extended_catalog_bytes, kExtendedCatalogSha256},
        {&timeline_bytes, kSoundSwitch2026RedSmoothSha256}}};
    for (const auto& [bytes, expected] : expected_digests) {
        if (sha256_bytes(*bytes) != expected) {
            return fail(
                SoundSwitch2026RedSmoothError::UnsupportedSource,
                "A required source artifact does not match the reviewed current-2026 identity.");
        }
    }
    const std::string marker_text(
        reinterpret_cast<const char*>(marker.data()), marker.size());
    if (marker_text.find(kSoundSwitch2026ProjectId) == std::string::npos ||
        marker_text.find("\"major\": 2") == std::string::npos ||
        marker_text.find("\"minor\": 10") == std::string::npos ||
        marker_text.find("\"hotfix\": 3") == std::string::npos ||
        marker_text.find("\"build\": 0") == std::string::npos) {
        return fail(
            SoundSwitch2026RedSmoothError::UnsupportedSource,
            "The current-2026 project marker is internally inconsistent.");
    }

    proposal.corpus_manifest.source_version =
        std::string(kSoundSwitch2026SourceVersion);
    proposal.corpus_manifest.artifacts = {
        make_artifact(
            ".ssproj", SoundSwitchArtifactKind::ProjectManifest,
            marker, MigrationSourceRole::Required),
        make_artifact(
            "SSAutoLoop1.ssfile", SoundSwitchArtifactKind::AutoloopScript,
            timeline_bytes, MigrationSourceRole::Required),
        make_artifact(
            "SoundSwitchAutoLoops.bin",
            SoundSwitchArtifactKind::AutoloopDatabase,
            primary_catalog_bytes, MigrationSourceRole::Required),
        make_artifact(
            "SoundSwitchAutoLoopsEx.bin",
            SoundSwitchArtifactKind::ExtendedAutoloopDatabase,
            extended_catalog_bytes, MigrationSourceRole::Conditional),
        make_artifact(
            "SoundSwitchVenues.bin", SoundSwitchArtifactKind::VenueDatabase,
            venue, MigrationSourceRole::Required)};
    normalize_soundswitch_corpus_manifest(proposal.corpus_manifest);
    proposal.manifest_validation =
        validate_soundswitch_corpus_manifest(proposal.corpus_manifest);
    if (!proposal.manifest_validation) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidProposal,
            "The exact source corpus manifest failed its contract.");
    }
    const auto* primary_artifact = find_artifact(
        proposal.corpus_manifest, "SoundSwitchAutoLoops.bin");
    const auto* extended_artifact = find_artifact(
        proposal.corpus_manifest, "SoundSwitchAutoLoopsEx.bin");
    const auto* timeline_artifact = find_artifact(
        proposal.corpus_manifest, "SSAutoLoop1.ssfile");
    const auto* venue_artifact = find_artifact(
        proposal.corpus_manifest, "SoundSwitchVenues.bin");
    if (primary_artifact == nullptr || extended_artifact == nullptr ||
        timeline_artifact == nullptr || venue_artifact == nullptr) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidProposal,
            "The source corpus manifest lost a required artifact identity.");
    }

    const auto primary_catalog = decode_soundswitch_v3_autoloop_catalog(
        primary_catalog_bytes, primary_artifact->artifact_id);
    const auto extended_catalog = decode_soundswitch_v3_autoloop_catalog(
        extended_catalog_bytes, extended_artifact->artifact_id);
    if (!primary_catalog || !extended_catalog) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidCatalog,
            !primary_catalog ? primary_catalog.message : extended_catalog.message);
    }
    const auto entry = std::find_if(
        primary_catalog.catalog.entries.begin(),
        primary_catalog.catalog.entries.end(),
        [](const auto& value) { return value.source_index == 0U; });
    const auto placement = std::find_if(
        primary_catalog.catalog.placements.begin(),
        primary_catalog.catalog.placements.end(),
        [](const auto& value) {
            return value.bank_index == 0U && value.slot_index == 0U;
        });
    if (primary_catalog.catalog.bank_names.size() != 4U ||
        primary_catalog.catalog.bank_names.front() != "Medium" ||
        entry == primary_catalog.catalog.entries.end() ||
        entry->name != kSoundSwitch2026RedSmoothName || entry->bars != 8U ||
        entry->bank_index != 0U ||
        placement == primary_catalog.catalog.placements.end() ||
        placement->source_index != entry->source_index) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidCatalog,
            "Medium slot 1 is not the exact authored Red - Smooth Pulse entry.");
    }

    const auto target_ids = expected_source_targets();
    auto decoded = decode_soundswitch_v3_autoloop_timeline(
        timeline_bytes, target_ids, timeline_artifact->artifact_id);
    if (!decoded) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidTimeline,
            decoded.message);
    }
    proposal.decoded_targets = std::move(decoded.targets);

    proposal.migration_report.source_bundle_id =
        proposal.corpus_manifest.bundle_id;
    proposal.migration_report.source_version =
        std::string(kSoundSwitch2026SourceVersion);
    MigrationItem identity;
    identity.item_id = "autoloop.medium.1.identity";
    identity.item_kind = "autoloopIdentity";
    identity.status = MigrationItemStatus::Exact;
    identity.source_label = "Medium / 1 / Red - Smooth Pulse / 8 bars";
    identity.destination_ref = std::string(kPlacementId);
    identity.rule_id = "soundswitch.catalog-v3.placement";
    identity.evidence = {entry->evidence, placement->evidence};
    add_report_item(proposal.migration_report, std::move(identity));

    auto bindings = options.destination_bindings.empty()
        ? current_soundswitch_2026_destination_bindings()
        : options.destination_bindings;
    std::map<std::uint32_t, std::string> destination_by_target;
    for (auto& binding : bindings) {
        if ((!is_uplight(binding.source_target_id) &&
             !is_tube_cell(binding.source_target_id)) ||
            binding.destination_ref.empty() ||
            !destination_by_target.emplace(
                binding.source_target_id,
                std::move(binding.destination_ref)).second) {
            return fail(
                SoundSwitch2026RedSmoothError::InvalidDestination,
                "Destination bindings must be unique current-2026 rig targets.");
        }
    }

    AutoloopSourceDocument imported;
    imported.assets.push_back({
        std::string(kAssetId), std::string(kSoundSwitch2026RedSmoothName),
        "Evidence-backed current-2026 SoundSwitch Medium slot 1 import.",
        {"current-2026", "imported", "soundswitch"}, "Medium", 0.5F,
        std::string(kProgramId), std::string(kLaunchId),
        std::string(kProvenanceId), 1U});
    imported.placements.push_back({
        std::string(kPlacementId), 0U, 0U, std::string(kAssetId),
        "soundswitch/2026/medium/1"});
    AutoloopProgramDefinition program;
    program.id = std::string(kProgramId);
    program.length_ticks = 32 * kMusicalTicksPerQuarter;
    program.time_signature_numerator = 4U;
    program.time_signature_denominator = 4U;

    const auto venue_evidence = evidence(
        venue_artifact->artifact_id, 0U, venue.size(), kVenueDecoderId);
    std::size_t translated_targets = 0U;
    for (const auto& target : proposal.decoded_targets) {
        if (!is_uplight(target.source_target_id) &&
            !is_tube_cell(target.source_target_id)) {
            if (target.present &&
                (!target.intensity_records.empty() ||
                 !target.color_records.empty())) {
                MigrationItem opaque;
                opaque.item_id = "target." +
                    std::to_string(target.source_target_id) + ".group-payload";
                opaque.item_kind = "sourceGroupTimeline";
                opaque.status = MigrationItemStatus::PreservedOpaque;
                opaque.source_label = "Source target " +
                    std::to_string(target.source_target_id);
                opaque.rule_id = "soundswitch.group-not-cell";
                opaque.evidence.push_back(target.header_evidence);
                add_report_item(proposal.migration_report, std::move(opaque));
            }
            continue;
        }
        const auto binding = destination_by_target.find(target.source_target_id);
        const auto destination_ref = binding == destination_by_target.end()
            ? std::string{} : binding->second;
        MigrationItem map_item;
        map_item.item_id = "target." +
            std::to_string(target.source_target_id) + ".venue-map";
        map_item.item_kind = "venueTargetMap";
        map_item.status = MigrationItemStatus::Exact;
        map_item.source_label = "Current-2026 Venue target " +
            std::to_string(target.source_target_id);
        map_item.destination_ref = destination_ref;
        map_item.rule_id = "soundswitch.current-2026.venue-layout";
        map_item.evidence = {venue_evidence};
        add_report_item(proposal.migration_report, std::move(map_item));

        std::vector<MigrationEvidenceRef> raw_evidence;
        raw_evidence.reserve(
            target.intensity_records.size() + target.color_records.size() + 1U);
        if (target.present) raw_evidence.push_back(target.header_evidence);
        for (const auto& record : target.intensity_records) {
            raw_evidence.push_back(record.evidence);
        }
        for (const auto& record : target.color_records) {
            raw_evidence.push_back(record.evidence);
        }
        if (!target.present ||
            (target.intensity_records.empty() && target.color_records.empty())) {
            add_report_item(
                proposal.migration_report,
                make_missing_item(
                    "target." + std::to_string(target.source_target_id) +
                        ".timeline",
                    "targetTimeline",
                    "Source target " + std::to_string(target.source_target_id),
                    destination_ref,
                    "soundswitch.target_timeline_missing",
                    std::move(raw_evidence)));
            continue;
        }
        if (binding == destination_by_target.end()) {
            add_report_item(
                proposal.migration_report,
                make_missing_item(
                    "target." + std::to_string(target.source_target_id) +
                        ".destination",
                    "destinationBinding",
                    "Source target " + std::to_string(target.source_target_id),
                    {}, "soundswitch.destination_binding_missing",
                    std::move(raw_evidence)));
            continue;
        }
        const auto* fixture = find_fixture(destination_project, destination_ref);
        const auto* profile = fixture == nullptr
            ? nullptr : find_profile(destination_project, fixture->profile_id);
        const bool capable = is_uplight(target.source_target_id)
            ? profile_has_property(profile, showcore::Property::Intensity)
            : profile_has_property(profile, showcore::Property::Red) &&
                profile_has_property(profile, showcore::Property::Green) &&
                profile_has_property(profile, showcore::Property::Blue);
        if (!capable) {
            add_report_item(
                proposal.migration_report,
                make_missing_item(
                    "target." + std::to_string(target.source_target_id) +
                        ".capability",
                    "destinationCapability",
                    "Source target " + std::to_string(target.source_target_id),
                    destination_ref,
                    "soundswitch.destination_capability_missing",
                    std::move(raw_evidence)));
            continue;
        }

        AutoloopTargetDefinition source_target;
        source_target.id = "p1t" + std::to_string(target.source_target_id);
        source_target.kind = AutoloopTargetKind::Fixture;
        source_target.stable_ref = destination_ref;
        AutoloopLaneDefinition lane;
        lane.id = std::string(is_uplight(target.source_target_id) ? "p1i" : "p1c") +
            std::to_string(target.source_target_id);
        lane.target_id = source_target.id;
        if (is_uplight(target.source_target_id)) {
            if (target.intensity_records.size() < 2U) {
                add_report_item(
                    proposal.migration_report,
                    make_missing_item(
                        "target." + std::to_string(target.source_target_id) +
                            ".intensity",
                        "intensityTimeline",
                        "Source target " +
                            std::to_string(target.source_target_id),
                        destination_ref,
                        "soundswitch.intensity_timeline_missing",
                        std::move(raw_evidence)));
                continue;
            }
            source_target.required_properties = {showcore::Property::Intensity};
            program.targets.push_back(std::move(source_target));
            program.lanes.push_back(lane);
            append_intensity_events(
                program, lane.id, target.source_target_id,
                target.intensity_records);
            MigrationItem translated;
            translated.item_id = "target." +
                std::to_string(target.source_target_id) + ".intensity";
            translated.item_kind = "intensityTimeline";
            translated.status = MigrationItemStatus::DeterministicallyTranslated;
            translated.source_label = "A records for target " +
                std::to_string(target.source_target_id);
            translated.destination_ref = destination_ref;
            translated.rule_id = "soundswitch.a-fixedpoint-linear-v1";
            translated.evidence = raw_evidence;
            translated.warnings = {
                "Linear interpolation remains provisional pending controlled-delta evidence."};
            add_report_item(proposal.migration_report, std::move(translated));
            add_report_item(
                proposal.migration_report,
                make_missing_item(
                    "target." + std::to_string(target.source_target_id) +
                        ".color",
                    "colorContext",
                    "Local color context for target " +
                        std::to_string(target.source_target_id),
                    destination_ref,
                    "soundswitch.missing_color_source",
                    raw_evidence));
        } else {
            if (target.color_records.empty()) {
                add_report_item(
                    proposal.migration_report,
                    make_missing_item(
                        "target." + std::to_string(target.source_target_id) +
                            ".color",
                        "colorTimeline",
                        "Source target " +
                            std::to_string(target.source_target_id),
                        destination_ref,
                        "soundswitch.color_timeline_missing",
                        std::move(raw_evidence)));
                continue;
            }
            source_target.required_properties = {
                showcore::Property::Red,
                showcore::Property::Green,
                showcore::Property::Blue};
            program.targets.push_back(std::move(source_target));
            program.lanes.push_back(lane);
            append_tube_color_events(program, lane.id, target);
            MigrationItem translated;
            translated.item_id = "target." +
                std::to_string(target.source_target_id) + ".rgb";
            translated.item_kind = "rgbTimeline";
            translated.status = MigrationItemStatus::DeterministicallyTranslated;
            translated.source_label = "A/B records for target " +
                std::to_string(target.source_target_id);
            translated.destination_ref = destination_ref;
            translated.rule_id = "soundswitch.ab-rgb-intensity-v1";
            translated.evidence = raw_evidence;
            translated.warnings = {
                "RGB endpoint and A-record interpolation remain provisional "
                "pending controlled-delta evidence."};
            add_report_item(proposal.migration_report, std::move(translated));

            MigrationItem opaque;
            opaque.item_id = "target." +
                std::to_string(target.source_target_id) + ".packed-unknown";
            opaque.item_kind = "packedColorUnknowns";
            opaque.status = MigrationItemStatus::PreservedOpaque;
            opaque.source_label = "Packed color high/direct bytes for target " +
                std::to_string(target.source_target_id);
            opaque.destination_ref = destination_ref;
            opaque.rule_id = "soundswitch.packed-bytes-opaque-v1";
            for (const auto& record : target.color_records) {
                opaque.evidence.push_back(record.evidence);
            }
            opaque.warnings = {
                "The first packed color high byte and unclaimed direct-emitter "
                "bytes are retained raw and do not control output."};
            add_report_item(proposal.migration_report, std::move(opaque));
        }
        ++translated_targets;
    }
    if (translated_targets == 0U || program.events.empty()) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidDestination,
            "No source target could be reconciled to a previewable destination fixture.");
    }
    imported.programs.push_back(std::move(program));
    AutoloopLaunchProfileDefinition launch;
    launch.id = std::string(kLaunchId);
    launch.repeat = showcore::AutoloopRepeat::Infinite;
    launch.launch = AutoloopLaunchQuantization::Immediate;
    launch.phase_origin = AutoloopPhaseOrigin::Launch;
    launch.mode = AutoloopPlaybackMode::Overlay;
    imported.launch_profiles.push_back(std::move(launch));
    AutoloopProvenanceDefinition provenance;
    provenance.id = std::string(kProvenanceId);
    provenance.origin = AutoloopProvenanceOrigin::Migrated;
    provenance.producer_id = std::string(kProducerId);
    provenance.producer_version = std::string(kDecoderVersion);
    provenance.source_bundle_id = proposal.corpus_manifest.bundle_id;
    provenance.source_artifact_id = "SSAutoLoop1.ssfile/" +
        timeline_artifact->artifact_id;
    provenance.source_object_key = "Medium/1/0";
    provenance.evidence_status =
        "deterministicallyTranslated;MissingColorSource;outputDisabled";
    imported.provenance.push_back(std::move(provenance));
    normalize_autoloop_source(imported);

    const auto persisted = inspect_persisted_autoloop_source(destination_project);
    if (!persisted) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidDestination,
            "The destination project has an invalid persisted Autoloop source.");
    }
    proposal.source = persisted.stamp.present
        ? persisted.source : AutoloopSourceDocument{};
    std::string merge_message;
    if (!merge_imported_source(proposal.source, std::move(imported), merge_message)) {
        MigrationItem conflict;
        conflict.item_id = "autoloop.medium.1.merge-conflict";
        conflict.item_kind = "sourceMerge";
        conflict.status = MigrationItemStatus::Conflicted;
        conflict.source_label = std::string(kSoundSwitch2026RedSmoothName);
        conflict.rule_id = "soundswitch.stable-slice-merge-v1";
        conflict.warnings.push_back(merge_message);
        add_report_item(proposal.migration_report, std::move(conflict));
        normalize_soundswitch_migration_report(proposal.migration_report);
        return fail(
            SoundSwitch2026RedSmoothError::MergeConflict,
            merge_message);
    }
    proposal.source_validation = validate_autoloop_source(proposal.source);
    normalize_soundswitch_migration_report(proposal.migration_report);
    proposal.report_validation =
        validate_soundswitch_migration_report(proposal.migration_report);
    proposal.source_digest = autoloop_source_digest(proposal.source);
    if (!proposal.source_validation.ok() || !proposal.report_validation ||
        proposal.source_digest.empty()) {
        return fail(
            SoundSwitch2026RedSmoothError::InvalidProposal,
            "The normalized V2 source or migration report failed validation.");
    }
    proposal.error = SoundSwitch2026RedSmoothError::None;
    proposal.message =
        "Built the output-disabled current-2026 Medium slot 1 V2 import proposal.";
    return proposal;
}

const char* soundswitch_2026_red_smooth_error_name(
    SoundSwitch2026RedSmoothError error) noexcept {
    switch (error) {
    case SoundSwitch2026RedSmoothError::None: return "none";
    case SoundSwitch2026RedSmoothError::MissingSource: return "missingSource";
    case SoundSwitch2026RedSmoothError::ReadFailed: return "readFailed";
    case SoundSwitch2026RedSmoothError::UnsupportedSource:
        return "unsupportedSource";
    case SoundSwitch2026RedSmoothError::InvalidCatalog: return "invalidCatalog";
    case SoundSwitch2026RedSmoothError::InvalidTimeline:
        return "invalidTimeline";
    case SoundSwitch2026RedSmoothError::InvalidDestination:
        return "invalidDestination";
    case SoundSwitch2026RedSmoothError::MergeConflict: return "mergeConflict";
    case SoundSwitch2026RedSmoothError::InvalidProposal:
        return "invalidProposal";
    }
    return "invalidProposal";
}

}  // namespace emberlights
