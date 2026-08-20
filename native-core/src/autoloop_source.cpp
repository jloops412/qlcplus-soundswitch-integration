#include "emberlights/autoloop_source.hpp"

#include "emberlights/file_identity.hpp"
#include "showcore/number_chars.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

struct ParsedRecord {
    std::size_t line{0U};
    std::vector<std::string> fields;
};

[[nodiscard]] bool valid_id(std::string_view value) noexcept {
    return !value.empty() && value.size() <= kMaximumAutoloopSourceIdentifierLength;
}

[[nodiscard]] bool valid_text(std::string_view value) noexcept {
    return value.size() <= kMaximumAutoloopSourceTextLength;
}

[[nodiscard]] bool finite_normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool valid_property_value(showcore::PropertyValue value) noexcept {
    switch (value.mode) {
    case showcore::ValueMode::Release:
    case showcore::ValueMode::ForceZero:
        return true;
    case showcore::ValueMode::Set:
        return finite_normalized(value.value);
    }
    return false;
}

[[nodiscard]] bool valid_generator(
    const AutoloopGeneratorParameters& value) noexcept {
    return std::isfinite(value.rate_start) && value.rate_start >= 0.0F &&
        value.rate_start <= 64.0F && std::isfinite(value.rate_end) &&
        value.rate_end >= 0.0F && value.rate_end <= 64.0F &&
        finite_normalized(value.size_start) && finite_normalized(value.size_end) &&
        finite_normalized(value.phase) && finite_normalized(value.spread) &&
        finite_normalized(value.base_primary) &&
        finite_normalized(value.base_secondary);
}

[[nodiscard]] bool emitter_property(showcore::Property property) noexcept {
    switch (property) {
    case showcore::Property::Red:
    case showcore::Property::Green:
    case showcore::Property::Blue:
    case showcore::Property::White:
    case showcore::Property::Amber:
    case showcore::Property::UV:
    case showcore::Property::Cyan:
    case showcore::Property::Magenta:
    case showcore::Property::Yellow:
    case showcore::Property::Lime:
    case showcore::Property::Indigo:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool curve_has_set_ramp(
    const AutoloopEventDefinition& event) noexcept {
    if (event.kind != AutoloopEventKind::PropertyCurve ||
        event.curve_points.size() < 2U) {
        return false;
    }
    for (std::size_t index = 1U; index < event.curve_points.size(); ++index) {
        const auto& first = event.curve_points[index - 1U].value;
        const auto& second = event.curve_points[index].value;
        if (first.mode == showcore::ValueMode::Set &&
            second.mode == showcore::ValueMode::Set &&
            std::abs(first.value - second.value) > 0.0001F) {
            return true;
        }
    }
    return false;
}

void add_issue(
    AutoloopSourceValidation& result,
    AutoloopSourceIssueSeverity severity,
    std::string code,
    std::string subject,
    std::string message) {
    result.issues.push_back({
        severity, std::move(code), std::move(subject), std::move(message)});
}

[[nodiscard]] char hex_digit(std::uint8_t value) noexcept {
    return value < 10U ? static_cast<char>('0' + value)
                       : static_cast<char>('A' + value - 10U);
}

[[nodiscard]] bool hex_value(char character, std::uint8_t& value) noexcept {
    if (character >= '0' && character <= '9') {
        value = static_cast<std::uint8_t>(character - '0');
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint8_t>(10 + character - 'A');
        return true;
    }
    if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint8_t>(10 + character - 'a');
        return true;
    }
    return false;
}

[[nodiscard]] std::string encode_field(std::string_view value) {
    std::string encoded;
    encoded.reserve(value.size());
    for (const auto character : value) {
        const auto byte = static_cast<std::uint8_t>(character);
        if (character == '%' || character == '\t' || character == '\r' ||
            character == '\n' || byte < 0x20U) {
            encoded.push_back('%');
            encoded.push_back(hex_digit(static_cast<std::uint8_t>(byte >> 4U)));
            encoded.push_back(hex_digit(static_cast<std::uint8_t>(byte & 0x0FU)));
        } else {
            encoded.push_back(character);
        }
    }
    return encoded;
}

[[nodiscard]] bool decode_field(std::string_view value, std::string& decoded) {
    decoded.clear();
    decoded.reserve(value.size());
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2U >= value.size()) {
            return false;
        }
        std::uint8_t high = 0U;
        std::uint8_t low = 0U;
        if (!hex_value(value[index + 1U], high) ||
            !hex_value(value[index + 2U], low)) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4U) | low));
        index += 2U;
    }
    return true;
}

template <typename Value>
[[nodiscard]] std::string number_text(Value value) {
    std::array<char, 64U> buffer{};
    const auto result = [&]() {
        if constexpr (std::is_floating_point_v<Value>) {
            return std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value,
                std::chars_format::general,
                std::numeric_limits<Value>::max_digits10);
        } else {
            return std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value);
        }
    }();
    return result.ec == std::errc{}
        ? std::string(buffer.data(), result.ptr)
        : std::string{};
}

template <typename Enum>
[[nodiscard]] std::string enum_text(Enum value) {
    return number_text(static_cast<std::underlying_type_t<Enum>>(value));
}

template <typename Value>
[[nodiscard]] bool parse_number(std::string_view text, Value& value) noexcept {
    if (text.empty()) {
        return false;
    }
    Value parsed{};
    const auto result = showcore::parse_number_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    value = parsed;
    return true;
}

template <typename Enum>
[[nodiscard]] bool parse_enum(
    std::string_view text,
    Enum count,
    Enum& value) noexcept {
    std::underlying_type_t<Enum> raw{};
    if (!parse_number(text, raw) ||
        raw >= static_cast<std::underlying_type_t<Enum>>(count)) {
        return false;
    }
    value = static_cast<Enum>(raw);
    return true;
}

[[nodiscard]] bool parse_value_mode(
    std::string_view text,
    showcore::ValueMode& value) noexcept {
    std::uint8_t raw = 0U;
    if (!parse_number(text, raw) ||
        raw > static_cast<std::uint8_t>(showcore::ValueMode::ForceZero)) {
        return false;
    }
    value = static_cast<showcore::ValueMode>(raw);
    return true;
}

[[nodiscard]] bool parse_repeat(
    std::string_view text,
    showcore::AutoloopRepeat& value) noexcept {
    std::uint8_t raw = 0U;
    if (!parse_number(text, raw) ||
        raw > static_cast<std::uint8_t>(showcore::AutoloopRepeat::TrackDuration)) {
        return false;
    }
    value = static_cast<showcore::AutoloopRepeat>(raw);
    return true;
}

[[nodiscard]] bool parse_transition(
    std::string_view text,
    showcore::AutoloopTransition& value) noexcept {
    std::uint8_t raw = 0U;
    if (!parse_number(text, raw) ||
        raw > static_cast<std::uint8_t>(showcore::AutoloopTransition::Linear)) {
        return false;
    }
    value = static_cast<showcore::AutoloopTransition>(raw);
    return true;
}

void append_record(
    std::string& output,
    std::initializer_list<std::string> fields) {
    bool first = true;
    for (const auto& field : fields) {
        if (!first) {
            output.push_back('\t');
        }
        output.append(encode_field(field));
        first = false;
    }
    output.push_back('\n');
}

[[nodiscard]] bool parse_records(
    std::string_view serialized,
    std::vector<ParsedRecord>& records,
    AutoloopSourceIoResult& result) {
    records.clear();
    std::size_t offset = 0U;
    std::size_t line_number = 1U;
    while (offset < serialized.size()) {
        auto end = serialized.find('\n', offset);
        if (end == std::string_view::npos) {
            end = serialized.size();
        }
        auto line = serialized.substr(offset, end - offset);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (!line.empty()) {
            ParsedRecord record;
            record.line = line_number;
            std::size_t field_offset = 0U;
            while (field_offset <= line.size()) {
                const auto tab = line.find('\t', field_offset);
                const auto field_end = tab == std::string_view::npos
                    ? line.size() : tab;
                std::string decoded;
                if (!decode_field(
                        line.substr(field_offset, field_end - field_offset),
                        decoded)) {
                    result = {AutoloopSourceIoError::InvalidValue, line_number,
                              "Invalid percent escape in Autoloop source field."};
                    return false;
                }
                record.fields.push_back(std::move(decoded));
                if (tab == std::string_view::npos) {
                    break;
                }
                field_offset = tab + 1U;
            }
            records.push_back(std::move(record));
        }
        offset = end + (end < serialized.size() ? 1U : 0U);
        ++line_number;
    }
    return true;
}

template <typename Collection>
[[nodiscard]] auto find_id(Collection& values, std::string_view id) {
    return std::find_if(values.begin(), values.end(), [id](auto& value) {
        return value.id == id;
    });
}

[[nodiscard]] bool overlaps(
    MusicalTick first_start,
    MusicalTick first_end,
    MusicalTick second_start,
    MusicalTick second_end) noexcept {
    return first_start < second_end && second_start < first_end;
}

[[nodiscard]] bool source_event_has_opaque_ownership(
    AutoloopEventKind kind) noexcept {
    return kind == AutoloopEventKind::LegacyLook ||
        kind == AutoloopEventKind::Palette ||
        kind == AutoloopEventKind::Position ||
        kind == AutoloopEventKind::Attribute ||
        kind == AutoloopEventKind::Movement;
}

[[nodiscard]] std::string derived_id(
    std::string_view source,
    std::string_view suffix) {
    std::string value(source);
    value.append(suffix);
    return value;
}

[[nodiscard]] bool known_record_kind(std::string_view kind) noexcept {
    return kind == "ASSET" || kind == "ASSET_TAG" ||
        kind == "PLACEMENT" || kind == "PROGRAM" || kind == "TARGET" ||
        kind == "LANE" || kind == "EVENT" || kind == "CURVE" ||
        kind == "LAUNCH" || kind == "PROVENANCE";
}

}  // namespace

bool AutoloopSourceValidation::ok() const noexcept {
    return error_count() == 0U;
}

std::size_t AutoloopSourceValidation::error_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const AutoloopSourceIssue& issue) {
            return issue.severity == AutoloopSourceIssueSeverity::Error;
        }));
}

std::size_t AutoloopSourceValidation::warning_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(), issues.end(), [](const AutoloopSourceIssue& issue) {
            return issue.severity == AutoloopSourceIssueSeverity::Warning;
        }));
}

bool format1_beat_to_musical_tick(float beat, MusicalTick& tick) noexcept {
    if (!std::isfinite(beat)) {
        return false;
    }
    const auto scaled = static_cast<long double>(beat) *
        static_cast<long double>(kMusicalTicksPerQuarter);
    const auto rounded = std::round(scaled);
    // Powers of two are exact in every supported floating format. Using an
    // upper-exclusive 2^63 bound avoids converting INT64_MAX to double, where
    // it rounds up to the first unrepresentable signed value.
    constexpr long double lowest = -0x1p63L;
    constexpr long double highest_exclusive = 0x1p63L;
    if (!std::isfinite(rounded) || rounded < lowest ||
        rounded >= highest_exclusive) {
        return false;
    }
    tick = static_cast<MusicalTick>(rounded);
    return true;
}

AutoloopSourceDocument adapt_format1_autoloops(const ProjectDocument& project) {
    AutoloopSourceDocument source;
    source.assets.reserve(project.autoloops.size());
    source.placements.reserve(project.autoloops.size());
    source.programs.reserve(project.autoloops.size());
    source.launch_profiles.reserve(project.autoloops.size());
    source.provenance.reserve(project.autoloops.size());

    for (const auto& legacy : project.autoloops) {
        const auto program_id = derived_id(legacy.id, ".program");
        const auto placement_id = derived_id(legacy.id, ".placement");
        const auto launch_id = derived_id(legacy.id, ".launch");
        const auto provenance_id = derived_id(legacy.id, ".provenance");
        const auto target_id = derived_id(program_id, ".target.master");
        const auto lane_id = derived_id(program_id, ".lane.legacy");

        source.assets.push_back({
            legacy.id,
            legacy.name,
            "Format-1 whole-Static-Look compatibility asset.",
            {"format1", "legacy-look-sequence"},
            "legacy",
            0.5F,
            program_id,
            launch_id,
            provenance_id,
            1U});
        source.placements.push_back({
            placement_id, legacy.bank, legacy.slot, legacy.id, {}});

        AutoloopProgramDefinition program;
        program.id = program_id;
        if (!format1_beat_to_musical_tick(
                legacy.length_beats, program.length_ticks)) {
            program.length_ticks = 0;
        }
        program.targets.push_back({
            target_id, AutoloopTargetKind::Master, {}, {}});
        program.lanes.push_back({lane_id, target_id, 0U});
        program.events.reserve(legacy.steps.size());
        for (std::size_t index = 0U; index < legacy.steps.size(); ++index) {
            MusicalTick start = 0;
            if (!format1_beat_to_musical_tick(
                    legacy.steps[index].at_beat, start)) {
                start = -1;
            }
            MusicalTick end = program.length_ticks;
            if (index + 1U < legacy.steps.size() &&
                !format1_beat_to_musical_tick(
                    legacy.steps[index + 1U].at_beat, end)) {
                end = -1;
            }
            AutoloopEventDefinition event;
            event.id = derived_id(
                program_id, ".event." + number_text(index));
            event.lane_id = lane_id;
            event.kind = AutoloopEventKind::LegacyLook;
            event.start_tick = start;
            event.end_tick = end;
            event.reference_id = legacy.steps[index].look_id;
            event.legacy_transition = legacy.steps[index].transition;
            program.events.push_back(std::move(event));
        }
        source.programs.push_back(std::move(program));
        source.launch_profiles.push_back({
            launch_id,
            legacy.repeat,
            AutoloopLaunchQuantization::Immediate,
            AutoloopPhaseOrigin::Launch,
            AutoloopPlaybackMode::Overlay,
            0,
            legacy.repeat == showcore::AutoloopRepeat::TrackDuration});
        source.provenance.push_back({
            provenance_id,
            AutoloopProvenanceOrigin::Native,
            "emberlights.format1-adapter",
            "1",
            0U,
            {},
            {},
            legacy.id,
            "deterministicallyTranslated"});
    }
    normalize_autoloop_source(source);
    return source;
}

AutoloopSourceValidation validate_autoloop_source(
    const AutoloopSourceDocument& source) {
    AutoloopSourceValidation result;
    if (source.format_version != kAutoloopSourceFormatVersion) {
        add_issue(result, AutoloopSourceIssueSeverity::Error,
                  "autoloop.source.version", {},
                  "Autoloop source format version is unsupported.");
    }

    std::unordered_set<std::string_view> asset_ids;
    std::unordered_set<std::string_view> placement_ids;
    std::unordered_set<std::uint32_t> placement_addresses;
    std::unordered_set<std::string_view> program_ids;
    std::unordered_set<std::string_view> launch_ids;
    std::unordered_set<std::string_view> provenance_ids;

    for (const auto& program : source.programs) {
        if (!valid_id(program.id) || !program_ids.insert(program.id).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.program.id", program.id,
                      "Program ID must be valid and unique.");
        }
    }
    for (const auto& launch : source.launch_profiles) {
        if (!valid_id(launch.id) || !launch_ids.insert(launch.id).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.launch.id", launch.id,
                      "Launch profile ID must be valid and unique.");
        }
        if (static_cast<std::uint8_t>(launch.repeat) >
                static_cast<std::uint8_t>(
                    showcore::AutoloopRepeat::TrackDuration) ||
            launch.launch >= AutoloopLaunchQuantization::Count ||
            launch.phase_origin >= AutoloopPhaseOrigin::Count ||
            launch.mode >= AutoloopPlaybackMode::Count ||
            launch.return_fade_ticks < 0) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.launch.value", launch.id,
                      "Launch profile contains an invalid bounded policy value.");
        }
        if (launch.repeat == showcore::AutoloopRepeat::TrackDuration &&
            !launch.track_boundary_required) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.launch.trackBoundary", launch.id,
                      "TrackDuration must explicitly require normalized track-boundary evidence.");
        }
    }
    for (const auto& provenance : source.provenance) {
        if (!valid_id(provenance.id) ||
            !provenance_ids.insert(provenance.id).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.provenance.id", provenance.id,
                      "Provenance ID must be valid and unique.");
        }
        if (provenance.origin >= AutoloopProvenanceOrigin::Count ||
            !valid_text(provenance.producer_id) ||
            !valid_text(provenance.producer_version) ||
            !valid_text(provenance.source_bundle_id) ||
            !valid_text(provenance.source_artifact_id) ||
            !valid_text(provenance.source_object_key) ||
            !valid_text(provenance.evidence_status)) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.provenance.value", provenance.id,
                      "Provenance contains an invalid origin or over-limit evidence field.");
        }
    }
    for (const auto& asset : source.assets) {
        if (!valid_id(asset.id) || !asset_ids.insert(asset.id).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.asset.id", asset.id,
                      "Asset ID must be valid and unique.");
        }
        if (asset.name.empty() || !valid_text(asset.name) ||
            !valid_text(asset.description) || !valid_text(asset.style) ||
            !finite_normalized(asset.energy) || asset.revision == 0U) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.asset.metadata", asset.id,
                      "Asset metadata is empty, invalid, or outside bounded limits.");
        }
        if (program_ids.find(asset.program_id) == program_ids.end()) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.asset.program.missing", asset.id,
                      "Asset references a missing program.");
        }
        if (launch_ids.find(asset.launch_profile_id) == launch_ids.end()) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.asset.launch.missing", asset.id,
                      "Asset references a missing launch profile.");
        }
        if (provenance_ids.find(asset.provenance_id) == provenance_ids.end()) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.asset.provenance.missing", asset.id,
                      "Asset references missing provenance.");
        }
        std::unordered_set<std::string_view> tags;
        for (const auto& tag : asset.tags) {
            if (tag.empty() || !valid_text(tag) || !tags.insert(tag).second) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.asset.tag", asset.id,
                          "Asset tags must be non-empty, bounded, and unique.");
                break;
            }
        }
    }
    for (const auto& placement : source.placements) {
        const auto address = static_cast<std::uint32_t>(
            static_cast<std::size_t>(placement.bank) *
                showcore::kAutoloopsPerBank + placement.slot);
        if (!valid_id(placement.id) ||
            !placement_ids.insert(placement.id).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.placement.id", placement.id,
                      "Placement ID must be valid and unique.");
        }
        if (placement.bank >= showcore::kMaxAutoloopBanks ||
            placement.slot >= showcore::kAutoloopsPerBank ||
            !placement_addresses.insert(address).second) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.placement.address", placement.id,
                      "Placement address is outside 64 by 32 or already occupied.");
        }
        if (asset_ids.find(placement.asset_id) == asset_ids.end()) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.placement.asset.missing", placement.id,
                      "Placement references a missing asset.");
        }
        if (!valid_text(placement.content_management_key)) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.placement.managementKey", placement.id,
                      "Placement management key exceeds the source limit.");
        }
    }

    for (const auto& program : source.programs) {
        if (program.length_ticks <= 0 || program.time_signature_numerator == 0U ||
            program.time_signature_denominator == 0U ||
            (program.time_signature_denominator &
             (program.time_signature_denominator - 1U)) != 0U) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.program.shape", program.id,
                      "Program length and musical signature must be positive and bounded.");
        }
        if (program.targets.empty() || program.lanes.empty() ||
            program.events.empty()) {
            add_issue(result, AutoloopSourceIssueSeverity::Error,
                      "autoloop.program.empty", program.id,
                      "Program needs at least one target, lane, and event.");
        }
        std::unordered_set<std::string_view> target_ids;
        for (const auto& target : program.targets) {
            if (!valid_id(target.id) || !target_ids.insert(target.id).second) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.target.id", target.id,
                          "Program target ID must be valid and unique.");
            }
            if (target.kind >= AutoloopTargetKind::Count ||
                (target.kind == AutoloopTargetKind::Master &&
                 !target.stable_ref.empty()) ||
                (target.kind != AutoloopTargetKind::Master &&
                 !valid_id(target.stable_ref))) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.target.value", target.id,
                          "Target kind and stable reference disagree.");
            }
            std::unordered_set<showcore::Property> properties;
            for (const auto property : target.required_properties) {
                if (property >= showcore::Property::Count ||
                    !properties.insert(property).second) {
                    add_issue(result, AutoloopSourceIssueSeverity::Error,
                              "autoloop.target.capability", target.id,
                              "Target capability requirements must be valid and unique.");
                    break;
                }
            }
        }
        std::unordered_map<std::string_view, const AutoloopLaneDefinition*> lanes;
        std::unordered_set<std::string_view> lane_ids;
        for (const auto& lane : program.lanes) {
            if (!valid_id(lane.id) || !lane_ids.insert(lane.id).second) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.lane.id", lane.id,
                          "Program lane ID must be valid and unique.");
            }
            if (target_ids.find(lane.target_id) == target_ids.end()) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.lane.target.missing", lane.id,
                          "Program lane references a missing target.");
            }
            lanes.emplace(lane.id, &lane);
        }
        std::unordered_set<std::string_view> event_ids;
        for (const auto& event : program.events) {
            if (!valid_id(event.id) || !event_ids.insert(event.id).second) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.id", event.id,
                          "Program event ID must be valid and unique.");
            }
            if (lanes.find(event.lane_id) == lanes.end()) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.lane.missing", event.id,
                          "Program event references a missing lane.");
            }
            if (event.kind >= AutoloopEventKind::Count ||
                event.start_tick < 0 || event.end_tick <= event.start_tick ||
                event.end_tick > program.length_ticks ||
                event.interpolation >= AutoloopInterpolation::Count ||
                event.payload_version == 0U) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.range", event.id,
                          "Event kind, range, interpolation, or payload version is invalid.");
            }
            const bool property_event =
                event.kind == AutoloopEventKind::PropertyBlock ||
                event.kind == AutoloopEventKind::PropertyCurve ||
                event.kind == AutoloopEventKind::Effect;
            if (property_event && event.property >= showcore::Property::Count) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.property", event.id,
                          "Property event requires a valid semantic property.");
            }
            if (!valid_property_value(event.value)) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.value", event.id,
                          "Event property value must be finite and normalized when SET.");
            }
            if (static_cast<std::uint8_t>(event.legacy_transition) >
                static_cast<std::uint8_t>(
                    showcore::AutoloopTransition::Linear)) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.legacyTransition", event.id,
                          "Legacy transition is outside the format-1 Cut/Linear range.");
            }
            const bool requires_reference =
                event.kind == AutoloopEventKind::LegacyLook ||
                event.kind == AutoloopEventKind::Palette ||
                event.kind == AutoloopEventKind::Position ||
                event.kind == AutoloopEventKind::Attribute ||
                event.kind == AutoloopEventKind::Movement ||
                event.kind == AutoloopEventKind::Effect;
            if (requires_reference && !valid_id(event.reference_id)) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.reference", event.id,
                          "Event requires a stable reference ID.");
            }
            if (!event.transition_reference_id.empty() &&
                !valid_id(event.transition_reference_id)) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.transitionReference", event.id,
                          "Transition reference ID is invalid.");
            }
            if ((event.kind == AutoloopEventKind::Movement ||
                 event.kind == AutoloopEventKind::Effect) &&
                !valid_generator(event.generator)) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.event.generator", event.id,
                          "Generator parameters are outside their bounded ranges.");
            }
            if (event.kind == AutoloopEventKind::PropertyCurve) {
                if (event.curve_points.size() < 2U ||
                    event.curve_points.size() >
                        kMaximumAutoloopCurvePointsPerEvent) {
                    add_issue(result, AutoloopSourceIssueSeverity::Error,
                              "autoloop.curve.capacity", event.id,
                              "Property curve needs 2..256 points.");
                } else {
                    MusicalTick previous = -1;
                    for (const auto& point : event.curve_points) {
                        if (point.tick < event.start_tick ||
                            point.tick > event.end_tick ||
                            point.tick <= previous ||
                            !valid_property_value(point.value)) {
                            add_issue(result, AutoloopSourceIssueSeverity::Error,
                                      "autoloop.curve.point", event.id,
                                      "Curve points must be ordered, bounded, and valid.");
                            break;
                        }
                        previous = point.tick;
                    }
                    if (event.curve_points.front().tick != event.start_tick ||
                        event.curve_points.back().tick != event.end_tick) {
                        add_issue(result, AutoloopSourceIssueSeverity::Error,
                                  "autoloop.curve.endpoints", event.id,
                                  "Curve must explicitly define event start and end values.");
                    }
                    if (event.property == showcore::Property::Intensity) {
                        bool large_delta = false;
                        bool low_level_crawl = false;
                        for (std::size_t index = 1U;
                             index < event.curve_points.size(); ++index) {
                            const auto& first = event.curve_points[index - 1U];
                            const auto& second = event.curve_points[index];
                            if (first.value.mode != showcore::ValueMode::Set ||
                                second.value.mode != showcore::ValueMode::Set) {
                                continue;
                            }
                            const auto delta = std::abs(
                                first.value.value - second.value.value);
                            large_delta = large_delta || delta >= 0.35F;
                            low_level_crawl = low_level_crawl || (
                                event.interpolation ==
                                    AutoloopInterpolation::Linear &&
                                first.value.value <= 0.20F &&
                                second.value.value <= 0.20F &&
                                delta >= 0.02F &&
                                second.tick - first.tick >=
                                    2 * kMusicalTicksPerQuarter);
                        }
                        if (large_delta && event.interpolation ==
                                AutoloopInterpolation::Linear) {
                            add_issue(
                                result, AutoloopSourceIssueSeverity::Warning,
                                "autoloop.quality.intensityLargeLinearDelta",
                                event.id,
                                "A large linear intensity change may expose DMX stepping; prefer SmoothStep or add musically intentional control points.");
                        }
                        if (low_level_crawl) {
                            add_issue(
                                result, AutoloopSourceIssueSeverity::Warning,
                                "autoloop.quality.intensityLowLevelCrawl",
                                event.id,
                                "A long low-level linear intensity crawl is likely to look stepped on real dimmers; use SmoothStep, a shorter transition, or fixture-qualified dimmer behavior.");
                        }
                    }
                }
            } else if (!event.curve_points.empty()) {
                add_issue(result, AutoloopSourceIssueSeverity::Error,
                          "autoloop.curve.unexpected", event.id,
                          "Only PropertyCurve events may contain curve points.");
            }
        }

        for (std::size_t first_index = 0U;
             first_index < program.events.size(); ++first_index) {
            const auto& first = program.events[first_index];
            const auto first_lane = lanes.find(first.lane_id);
            if (first_lane == lanes.end()) {
                continue;
            }
            for (std::size_t second_index = first_index + 1U;
                 second_index < program.events.size(); ++second_index) {
                const auto& second = program.events[second_index];
                const auto second_lane = lanes.find(second.lane_id);
                if (second_lane == lanes.end() ||
                    first_lane->second->target_id !=
                        second_lane->second->target_id ||
                    first_lane->second->priority !=
                        second_lane->second->priority ||
                    !overlaps(first.start_tick, first.end_tick,
                              second.start_tick, second.end_tick)) {
                    continue;
                }
                const bool same_property = first.property == second.property;
                if (same_property || source_event_has_opaque_ownership(first.kind) ||
                    source_event_has_opaque_ownership(second.kind)) {
                    add_issue(result, AutoloopSourceIssueSeverity::Error,
                              "autoloop.event.ownershipConflict", first.id,
                              "Equal-priority overlapping ownership is ambiguous; assign explicit lane priorities.");
                }
                const bool master_emitter_pair =
                    ((first.property == showcore::Property::Intensity &&
                      emitter_property(second.property)) ||
                     (second.property == showcore::Property::Intensity &&
                      emitter_property(first.property))) &&
                    curve_has_set_ramp(first) && curve_has_set_ramp(second);
                if (master_emitter_pair) {
                    add_issue(
                        result, AutoloopSourceIssueSeverity::Warning,
                        "autoloop.quality.simultaneousMasterEmitterRamps",
                        first.id,
                        "Overlapping master-intensity and emitter ramps can compound perceptual stepping; keep independent semantic lanes, then preview the combined curve on the target fixture.");
                }
            }
        }
    }
    return result;
}

void normalize_autoloop_source(AutoloopSourceDocument& source) {
    const auto by_id = [](const auto& first, const auto& second) {
        return first.id < second.id;
    };
    std::sort(source.assets.begin(), source.assets.end(), by_id);
    for (auto& asset : source.assets) {
        std::sort(asset.tags.begin(), asset.tags.end());
    }
    std::sort(source.placements.begin(), source.placements.end(), by_id);
    std::sort(source.programs.begin(), source.programs.end(), by_id);
    for (auto& program : source.programs) {
        std::sort(program.targets.begin(), program.targets.end(), by_id);
        for (auto& target : program.targets) {
            std::sort(
                target.required_properties.begin(),
                target.required_properties.end(),
                [](showcore::Property first, showcore::Property second) {
                    return static_cast<std::uint8_t>(first) <
                        static_cast<std::uint8_t>(second);
                });
        }
        std::sort(program.lanes.begin(), program.lanes.end(), by_id);
        std::sort(program.events.begin(), program.events.end(), by_id);
        for (auto& event : program.events) {
            std::sort(
                event.curve_points.begin(), event.curve_points.end(),
                [](const auto& first, const auto& second) {
                    return first.tick < second.tick;
                });
        }
    }
    std::sort(source.launch_profiles.begin(), source.launch_profiles.end(), by_id);
    std::sort(source.provenance.begin(), source.provenance.end(), by_id);
}

std::string serialize_autoloop_source(const AutoloopSourceDocument& source) {
    auto canonical = source;
    normalize_autoloop_source(canonical);
    if (!validate_autoloop_source(canonical).ok()) {
        return {};
    }
    std::string output;
    output.reserve(4096U);
    append_record(output, {
        "EMBERLIGHTS_AUTOLOOP_SOURCE",
        number_text(canonical.format_version)});
    for (const auto& asset : canonical.assets) {
        append_record(output, {
            "ASSET", asset.id, asset.name, asset.description, asset.style,
            number_text(asset.energy), asset.program_id, asset.launch_profile_id,
            asset.provenance_id, number_text(asset.revision)});
        for (const auto& tag : asset.tags) {
            append_record(output, {"ASSET_TAG", asset.id, tag});
        }
    }
    for (const auto& placement : canonical.placements) {
        append_record(output, {
            "PLACEMENT", placement.id, number_text(placement.bank),
            number_text(placement.slot), placement.asset_id,
            placement.content_management_key});
    }
    for (const auto& program : canonical.programs) {
        append_record(output, {
            "PROGRAM", program.id, number_text(program.length_ticks),
            number_text(program.time_signature_numerator),
            number_text(program.time_signature_denominator)});
        for (const auto& target : program.targets) {
            std::string requirements;
            for (std::size_t index = 0U;
                 index < target.required_properties.size(); ++index) {
                if (index != 0U) requirements.push_back(',');
                requirements.append(enum_text(target.required_properties[index]));
            }
            append_record(output, {
                "TARGET", program.id, target.id, enum_text(target.kind),
                target.stable_ref, requirements});
        }
        for (const auto& lane : program.lanes) {
            append_record(output, {
                "LANE", program.id, lane.id, lane.target_id,
                number_text(lane.priority)});
        }
        for (const auto& event : program.events) {
            append_record(output, {
                "EVENT", program.id, event.id, event.lane_id,
                enum_text(event.kind), number_text(event.start_tick),
                number_text(event.end_tick), enum_text(event.property),
                enum_text(event.value.mode), number_text(event.value.value),
                enum_text(event.interpolation), event.reference_id,
                event.transition_reference_id,
                number_text(event.payload_version),
                number_text(event.generator.rate_start),
                number_text(event.generator.rate_end),
                number_text(event.generator.size_start),
                number_text(event.generator.size_end),
                number_text(event.generator.phase),
                number_text(event.generator.spread),
                number_text(event.generator.base_primary),
                number_text(event.generator.base_secondary),
                number_text(event.generator.seed),
                enum_text(event.legacy_transition)});
            for (const auto& point : event.curve_points) {
                append_record(output, {
                    "CURVE", program.id, event.id, number_text(point.tick),
                    enum_text(point.value.mode), number_text(point.value.value)});
            }
        }
    }
    for (const auto& launch : canonical.launch_profiles) {
        append_record(output, {
            "LAUNCH", launch.id, enum_text(launch.repeat),
            enum_text(launch.launch), enum_text(launch.phase_origin),
            enum_text(launch.mode), number_text(launch.return_fade_ticks),
            launch.track_boundary_required ? "1" : "0"});
    }
    for (const auto& provenance : canonical.provenance) {
        append_record(output, {
            "PROVENANCE", provenance.id, enum_text(provenance.origin),
            provenance.producer_id, provenance.producer_version,
            number_text(provenance.seed), provenance.source_bundle_id,
            provenance.source_artifact_id, provenance.source_object_key,
            provenance.evidence_status});
    }
    return output;
}

AutoloopSourceIoResult parse_autoloop_source(
    std::string_view serialized,
    AutoloopSourceDocument& source) {
    std::vector<ParsedRecord> records;
    AutoloopSourceIoResult result;
    if (!parse_records(serialized, records, result)) {
        return result;
    }
    if (records.empty() || records.front().fields.size() != 2U ||
        records.front().fields[0] != "EMBERLIGHTS_AUTOLOOP_SOURCE") {
        return {AutoloopSourceIoError::InvalidHeader, 1U,
                "Autoloop source header is missing."};
    }
    std::uint32_t version = 0U;
    if (!parse_number(records.front().fields[1], version)) {
        return {AutoloopSourceIoError::InvalidHeader, 1U,
                "Autoloop source version is malformed."};
    }
    if (version != kAutoloopSourceFormatVersion) {
        return {AutoloopSourceIoError::UnsupportedVersion, 1U,
                "Autoloop source version is unsupported."};
    }
    AutoloopSourceDocument parsed;
    parsed.format_version = version;

    for (std::size_t index = 1U; index < records.size(); ++index) {
        const auto& record = records[index];
        if (record.fields.empty() || !known_record_kind(record.fields.front())) {
            return {AutoloopSourceIoError::InvalidRecord, record.line,
                    "Unknown Autoloop source record."};
        }
    }

    for (std::size_t index = 1U; index < records.size(); ++index) {
        const auto& record = records[index];
        const auto& f = record.fields;
        if (f.empty()) continue;
        if (f[0] == "ASSET") {
            if (f.size() != 10U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid ASSET record."};
            }
            AutoloopAssetDefinition asset;
            asset.id = f[1];
            asset.name = f[2];
            asset.description = f[3];
            asset.style = f[4];
            asset.program_id = f[6];
            asset.launch_profile_id = f[7];
            asset.provenance_id = f[8];
            if (!parse_number(f[5], asset.energy) ||
                !parse_number(f[9], asset.revision)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid ASSET value."};
            }
            parsed.assets.push_back(std::move(asset));
        } else if (f[0] == "PLACEMENT") {
            if (f.size() != 6U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid PLACEMENT record."};
            }
            AutoloopPlacementDefinition placement;
            placement.id = f[1];
            placement.asset_id = f[4];
            placement.content_management_key = f[5];
            if (!parse_number(f[2], placement.bank) ||
                !parse_number(f[3], placement.slot)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid PLACEMENT value."};
            }
            parsed.placements.push_back(std::move(placement));
        } else if (f[0] == "PROGRAM") {
            if (f.size() != 5U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid PROGRAM record."};
            }
            AutoloopProgramDefinition program;
            program.id = f[1];
            if (!parse_number(f[2], program.length_ticks) ||
                !parse_number(f[3], program.time_signature_numerator) ||
                !parse_number(f[4], program.time_signature_denominator)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid PROGRAM value."};
            }
            parsed.programs.push_back(std::move(program));
        } else if (f[0] == "LAUNCH") {
            if (f.size() != 8U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid LAUNCH record."};
            }
            AutoloopLaunchProfileDefinition launch;
            std::uint8_t track_boundary = 0U;
            launch.id = f[1];
            if (!parse_repeat(f[2], launch.repeat) ||
                !parse_enum(f[3], AutoloopLaunchQuantization::Count,
                            launch.launch) ||
                !parse_enum(f[4], AutoloopPhaseOrigin::Count,
                            launch.phase_origin) ||
                !parse_enum(f[5], AutoloopPlaybackMode::Count, launch.mode) ||
                !parse_number(f[6], launch.return_fade_ticks) ||
                !parse_number(f[7], track_boundary) || track_boundary > 1U) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid LAUNCH value."};
            }
            launch.track_boundary_required = track_boundary != 0U;
            parsed.launch_profiles.push_back(std::move(launch));
        } else if (f[0] == "PROVENANCE") {
            if (f.size() != 10U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid PROVENANCE record."};
            }
            AutoloopProvenanceDefinition provenance;
            provenance.id = f[1];
            provenance.producer_id = f[3];
            provenance.producer_version = f[4];
            provenance.source_bundle_id = f[6];
            provenance.source_artifact_id = f[7];
            provenance.source_object_key = f[8];
            provenance.evidence_status = f[9];
            if (!parse_enum(f[2], AutoloopProvenanceOrigin::Count,
                            provenance.origin) ||
                !parse_number(f[5], provenance.seed)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid PROVENANCE value."};
            }
            parsed.provenance.push_back(std::move(provenance));
        }
    }

    for (std::size_t index = 1U; index < records.size(); ++index) {
        const auto& record = records[index];
        const auto& f = record.fields;
        if (f.empty()) continue;
        if (f[0] == "ASSET_TAG") {
            if (f.size() != 3U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid ASSET_TAG record."};
            }
            const auto asset = find_id(parsed.assets, f[1]);
            if (asset == parsed.assets.end()) {
                return {AutoloopSourceIoError::MissingReference, record.line,
                        "ASSET_TAG references a missing asset."};
            }
            asset->tags.push_back(f[2]);
        } else if (f[0] == "TARGET") {
            if (f.size() != 6U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid TARGET record."};
            }
            const auto program = find_id(parsed.programs, f[1]);
            if (program == parsed.programs.end()) {
                return {AutoloopSourceIoError::MissingReference, record.line,
                        "TARGET references a missing program."};
            }
            AutoloopTargetDefinition target;
            target.id = f[2];
            target.stable_ref = f[4];
            if (!parse_enum(f[3], AutoloopTargetKind::Count, target.kind)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid TARGET kind."};
            }
            std::size_t offset = 0U;
            while (offset < f[5].size()) {
                const auto comma = f[5].find(',', offset);
                const auto end = comma == std::string::npos ? f[5].size() : comma;
                std::uint8_t property = 0U;
                if (!parse_number(
                        std::string_view(f[5]).substr(offset, end - offset),
                        property) || property >= showcore::kPropertyCount) {
                    return {AutoloopSourceIoError::InvalidValue, record.line,
                            "Invalid TARGET capability requirement."};
                }
                target.required_properties.push_back(
                    static_cast<showcore::Property>(property));
                if (comma == std::string::npos) break;
                offset = comma + 1U;
            }
            program->targets.push_back(std::move(target));
        } else if (f[0] == "LANE") {
            if (f.size() != 5U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid LANE record."};
            }
            const auto program = find_id(parsed.programs, f[1]);
            if (program == parsed.programs.end()) {
                return {AutoloopSourceIoError::MissingReference, record.line,
                        "LANE references a missing program."};
            }
            AutoloopLaneDefinition lane;
            lane.id = f[2];
            lane.target_id = f[3];
            if (!parse_number(f[4], lane.priority)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid LANE priority."};
            }
            program->lanes.push_back(std::move(lane));
        } else if (f[0] == "EVENT") {
            if (f.size() != 24U) {
                return {AutoloopSourceIoError::InvalidRecord, record.line,
                        "Invalid EVENT record."};
            }
            const auto program = find_id(parsed.programs, f[1]);
            if (program == parsed.programs.end()) {
                return {AutoloopSourceIoError::MissingReference, record.line,
                        "EVENT references a missing program."};
            }
            AutoloopEventDefinition event;
            event.id = f[2];
            event.lane_id = f[3];
            event.reference_id = f[11];
            event.transition_reference_id = f[12];
            std::uint8_t property = 0U;
            if (!parse_enum(f[4], AutoloopEventKind::Count, event.kind) ||
                !parse_number(f[5], event.start_tick) ||
                !parse_number(f[6], event.end_tick) ||
                !parse_number(f[7], property) || property >= showcore::kPropertyCount ||
                !parse_value_mode(f[8], event.value.mode) ||
                !parse_number(f[9], event.value.value) ||
                !parse_enum(f[10], AutoloopInterpolation::Count,
                            event.interpolation) ||
                !parse_number(f[13], event.payload_version) ||
                !parse_number(f[14], event.generator.rate_start) ||
                !parse_number(f[15], event.generator.rate_end) ||
                !parse_number(f[16], event.generator.size_start) ||
                !parse_number(f[17], event.generator.size_end) ||
                !parse_number(f[18], event.generator.phase) ||
                !parse_number(f[19], event.generator.spread) ||
                !parse_number(f[20], event.generator.base_primary) ||
                !parse_number(f[21], event.generator.base_secondary) ||
                !parse_number(f[22], event.generator.seed) ||
                !parse_transition(f[23], event.legacy_transition)) {
                return {AutoloopSourceIoError::InvalidValue, record.line,
                        "Invalid EVENT value."};
            }
            event.property = static_cast<showcore::Property>(property);
            program->events.push_back(std::move(event));
        }
    }

    for (std::size_t index = 1U; index < records.size(); ++index) {
        const auto& record = records[index];
        const auto& f = record.fields;
        if (f.empty() || f[0] != "CURVE") continue;
        if (f.size() != 6U) {
            return {AutoloopSourceIoError::InvalidRecord, record.line,
                    "Invalid CURVE record."};
        }
        const auto program = find_id(parsed.programs, f[1]);
        if (program == parsed.programs.end()) {
            return {AutoloopSourceIoError::MissingReference, record.line,
                    "CURVE references a missing program."};
        }
        const auto event = find_id(program->events, f[2]);
        if (event == program->events.end()) {
            return {AutoloopSourceIoError::MissingReference, record.line,
                    "CURVE references a missing event."};
        }
        AutoloopCurvePointDefinition point;
        if (!parse_number(f[3], point.tick) ||
            !parse_value_mode(f[4], point.value.mode) ||
            !parse_number(f[5], point.value.value)) {
            return {AutoloopSourceIoError::InvalidValue, record.line,
                    "Invalid CURVE value."};
        }
        event->curve_points.push_back(point);
    }

    normalize_autoloop_source(parsed);
    const auto validation = validate_autoloop_source(parsed);
    if (!validation.ok()) {
        return {AutoloopSourceIoError::ValidationFailed, 0U,
                validation.issues.front().code + ": " +
                    validation.issues.front().message};
    }
    source = std::move(parsed);
    return {};
}

std::string autoloop_source_digest(const AutoloopSourceDocument& source) {
    const auto serialized = serialize_autoloop_source(source);
    return serialized.empty() ? std::string{} : sha256_text(serialized);
}

const char* autoloop_target_kind_name(AutoloopTargetKind kind) noexcept {
    switch (kind) {
    case AutoloopTargetKind::Master: return "master";
    case AutoloopTargetKind::Group: return "group";
    case AutoloopTargetKind::Fixture: return "fixture";
    case AutoloopTargetKind::RoleSelector: return "roleSelector";
    case AutoloopTargetKind::Count: break;
    }
    return "invalid";
}

const char* autoloop_event_kind_name(AutoloopEventKind kind) noexcept {
    switch (kind) {
    case AutoloopEventKind::LegacyLook: return "legacyLook";
    case AutoloopEventKind::PropertyBlock: return "propertyBlock";
    case AutoloopEventKind::PropertyCurve: return "propertyCurve";
    case AutoloopEventKind::Palette: return "palette";
    case AutoloopEventKind::Position: return "position";
    case AutoloopEventKind::Attribute: return "attribute";
    case AutoloopEventKind::Movement: return "movement";
    case AutoloopEventKind::Effect: return "effect";
    case AutoloopEventKind::Count: break;
    }
    return "invalid";
}

const char* autoloop_reference_kind_name(AutoloopReferenceKind kind) noexcept {
    switch (kind) {
    case AutoloopReferenceKind::Palette: return "palette";
    case AutoloopReferenceKind::Position: return "position";
    case AutoloopReferenceKind::Attribute: return "attribute";
    case AutoloopReferenceKind::Movement: return "movement";
    case AutoloopReferenceKind::Effect: return "effect";
    case AutoloopReferenceKind::Transition: return "transition";
    case AutoloopReferenceKind::Count: break;
    }
    return "invalid";
}

}  // namespace emberlights
