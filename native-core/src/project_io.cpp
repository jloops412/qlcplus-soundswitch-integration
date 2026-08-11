#include "emberlights/project_io.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

inline constexpr std::size_t kMaximumProjectBytes = 64U * 1024U * 1024U;
std::atomic<std::uint64_t> g_history_sequence{0U};

struct ParsedRecord {
    std::size_t line{0};
    std::string raw;
    std::vector<std::string> fields;
};

[[nodiscard]] std::uint32_t crc32(std::string_view bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto character : bytes) {
        crc ^= static_cast<std::uint8_t>(character);
        for (std::uint8_t bit = 0; bit < 8U; ++bit) {
            const auto mask = static_cast<std::uint32_t>(
                -static_cast<std::int32_t>(crc & 1U));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

[[nodiscard]] char hex_digit(std::uint8_t value) noexcept {
    return value < 10U ? static_cast<char>('0' + value)
                       : static_cast<char>('A' + (value - 10U));
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
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2U >= value.size()) {
            return false;
        }
        std::uint8_t high = 0;
        std::uint8_t low = 0;
        if (!hex_value(value[index + 1U], high) || !hex_value(value[index + 2U], low)) {
            return false;
        }
        decoded.push_back(static_cast<char>((high << 4U) | low));
        index += 2U;
    }
    return true;
}

template <typename Value>
[[nodiscard]] std::string number_text(Value value) {
    std::array<char, 64> buffer{};
    auto result = [&]() {
        if constexpr (std::is_floating_point_v<Value>) {
            return std::to_chars(
                buffer.data(),
                buffer.data() + buffer.size(),
                value,
                std::chars_format::general,
                std::numeric_limits<Value>::max_digits10);
        } else {
            return std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
        }
    }();
    return result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : std::string{};
}

void append_record(std::string& output, std::initializer_list<std::string> fields) {
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

[[nodiscard]] std::string_view source_name(showcore::FixtureProfileSource source) noexcept {
    switch (source) {
    case showcore::FixtureProfileSource::BuiltIn: return "builtIn";
    case showcore::FixtureProfileSource::OpenFixtureLibrary: return "openFixtureLibrary";
    case showcore::FixtureProfileSource::QlcPlus: return "qlcPlus";
    case showcore::FixtureProfileSource::Local: return "local";
    case showcore::FixtureProfileSource::Migrated: return "migrated";
    case showcore::FixtureProfileSource::Unknown: return "unknown";
    }
    return "unknown";
}

[[nodiscard]] bool parse_source(
    std::string_view text,
    showcore::FixtureProfileSource& source) noexcept {
    if (text == "builtIn") {
        source = showcore::FixtureProfileSource::BuiltIn;
    } else if (text == "openFixtureLibrary") {
        source = showcore::FixtureProfileSource::OpenFixtureLibrary;
    } else if (text == "qlcPlus") {
        source = showcore::FixtureProfileSource::QlcPlus;
    } else if (text == "local") {
        source = showcore::FixtureProfileSource::Local;
    } else if (text == "migrated") {
        source = showcore::FixtureProfileSource::Migrated;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view value_mode_name(showcore::ValueMode mode) noexcept {
    switch (mode) {
    case showcore::ValueMode::Release: return "release";
    case showcore::ValueMode::Set: return "set";
    case showcore::ValueMode::ForceZero: return "forceZero";
    }
    return "invalid";
}

[[nodiscard]] bool parse_value_mode(std::string_view text, showcore::ValueMode& mode) noexcept {
    if (text == "release") {
        mode = showcore::ValueMode::Release;
    } else if (text == "set") {
        mode = showcore::ValueMode::Set;
    } else if (text == "forceZero") {
        mode = showcore::ValueMode::ForceZero;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view transition_name(showcore::AutoloopTransition value) noexcept {
    return value == showcore::AutoloopTransition::Linear ? "linear" : "cut";
}

[[nodiscard]] bool parse_transition(
    std::string_view text,
    showcore::AutoloopTransition& value) noexcept {
    if (text == "cut") {
        value = showcore::AutoloopTransition::Cut;
    } else if (text == "linear") {
        value = showcore::AutoloopTransition::Linear;
    } else {
        return false;
    }
    return true;
}

[[nodiscard]] std::string_view repeat_name(showcore::AutoloopRepeat value) noexcept {
    switch (value) {
    case showcore::AutoloopRepeat::Once: return "once";
    case showcore::AutoloopRepeat::Infinite: return "infinite";
    case showcore::AutoloopRepeat::TrackDuration: return "trackDuration";
    }
    return "invalid";
}

[[nodiscard]] bool parse_repeat(std::string_view text, showcore::AutoloopRepeat& value) noexcept {
    if (text == "once") {
        value = showcore::AutoloopRepeat::Once;
    } else if (text == "infinite") {
        value = showcore::AutoloopRepeat::Infinite;
    } else if (text == "trackDuration") {
        value = showcore::AutoloopRepeat::TrackDuration;
    } else {
        return false;
    }
    return true;
}

template <typename Enum>
[[nodiscard]] std::string enum_number(Enum value) {
    return number_text(static_cast<std::underlying_type_t<Enum>>(value));
}

template <typename Value>
[[nodiscard]] bool parse_number(std::string_view text, Value& value) noexcept {
    if (text.empty()) {
        return false;
    }
    Value parsed{};
    auto result = [&]() {
        if constexpr (std::is_floating_point_v<Value>) {
            return std::from_chars(
                text.data(), text.data() + text.size(), parsed, std::chars_format::general);
        } else {
            return std::from_chars(text.data(), text.data() + text.size(), parsed);
        }
    }();
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    value = parsed;
    return true;
}

[[nodiscard]] bool parse_bool(std::string_view text, bool& value) noexcept {
    if (text == "1") {
        value = true;
        return true;
    }
    if (text == "0") {
        value = false;
        return true;
    }
    return false;
}

[[nodiscard]] ProjectIoResult error(
    ProjectIoError code,
    std::size_t line,
    std::string message) {
    return {code, line, false, std::move(message)};
}

[[nodiscard]] bool is_known_primary(std::string_view type) noexcept {
    return type == "PROJECT" || type == "CONNECTIONS" || type == "SAFETY" ||
        type == "PROFILE" || type == "FIXTURE" || type == "GROUP" ||
        type == "LOOK" || type == "AUTOLOOP" || type == "AUDIO" ||
        type == "TRACK" || type == "MIDI";
}

[[nodiscard]] bool is_known_secondary(std::string_view type) noexcept {
    return type == "CHANNEL" || type == "ROLE" || type == "GROUP_MEMBER" ||
        type == "LOOK_VALUE" || type == "STEP" || type == "TRACK_CUE";
}

template <typename Collection>
[[nodiscard]] auto find_id(Collection& collection, std::string_view id) {
    return std::find_if(
        collection.begin(), collection.end(),
        [id](const auto& value) { return value.id == id; });
}

[[nodiscard]] ProjectIoResult parse_records(
    const std::vector<ParsedRecord>& records,
    ProjectDocument& project) {
    project = {};
    project.format_version = kProjectFormatVersion;

    for (const auto& record : records) {
        if (record.fields.empty() || !is_known_primary(record.fields[0])) {
            continue;
        }
        const auto& f = record.fields;
        if (f[0] == "PROJECT") {
            if (f.size() != 3U) {
                return error(ProjectIoError::InvalidRecord, record.line, "PROJECT needs ID and name.");
            }
            project.id = f[1];
            project.name = f[2];
        } else if (f[0] == "CONNECTIONS") {
            if ((f.size() != 17U && f.size() != 19U) ||
                !parse_bool(f[1], project.connections.os2l_enabled) ||
                !parse_number(f[3], project.connections.os2l_port) ||
                !parse_bool(f[4], project.connections.artnet_enabled) ||
                !parse_number(f[6], project.connections.artnet_base) ||
                !parse_bool(f[7], project.connections.sacn_enabled) ||
                !parse_number(f[9], project.connections.sacn_universe_base) ||
                !parse_number(f[10], project.connections.frame_rate) ||
                !parse_number(f[11], project.connections.manual_bpm) ||
                !parse_number(f[12], project.connections.midi_input_index) ||
                !parse_number(f[13], project.connections.midi_output_index)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid CONNECTIONS record.");
            }
            project.connections.os2l_bind = f[2];
            project.connections.artnet_destination = f[5];
            project.connections.sacn_destination = f[8];
            // Preview builds before native USB-DMX used literal zeroes in these
            // reserved fields. Treat those as disabled when opening older files.
            if (f[14] != "0") {
                project.connections.dmx_usb_pro_ports[0] = f[14];
            }
            if (f[15] != "0") {
                project.connections.dmx_usb_pro_ports[1] = f[15];
            }
            // Field 16 identifies the adapter contract and remains optional so
            // earlier project files continue to load without a format bump.
            if (f.size() == 19U) {
                std::uint8_t framing = 0U;
                if (!parse_number(f[17], project.connections.soundswitch_micro_universe) ||
                    !parse_number(f[18], framing) ||
                    framing > showcore::soundswitch_micro_framing_value(
                        showcore::SoundSwitchMicroFraming::EnttecUsbPro)) {
                    return error(
                        ProjectIoError::InvalidValue,
                        record.line,
                        "Invalid SoundSwitch Micro CONNECTIONS values.");
                }
                project.connections.soundswitch_micro_framing =
                    static_cast<showcore::SoundSwitchMicroFraming>(framing);
            }
        } else if (f[0] == "SAFETY") {
            if (f.size() != 8U ||
                !parse_bool(f[1], project.safety.fog_requires_arm) ||
                !parse_bool(f[2], project.safety.haze_requires_arm) ||
                !parse_bool(f[3], project.safety.laser_requires_arm) ||
                !parse_bool(f[4], project.safety.spark_requires_arm) ||
                !parse_bool(f[5], project.safety.strobe_allowed) ||
                !parse_number(f[6], project.safety.max_strobe) ||
                !parse_number(f[7], project.safety.max_intensity)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid SAFETY record.");
            }
        } else if (f[0] == "PROFILE") {
            if (f.size() != 9U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid PROFILE record.");
            }
            FixtureProfileDefinition profile;
            profile.id = f[1];
            profile.manufacturer = f[2];
            profile.model = f[3];
            profile.mode = f[4];
            profile.name = f[5];
            if (!parse_source(f[6], profile.source) ||
                !parse_number(f[8], profile.footprint)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid PROFILE value.");
            }
            profile.source_revision = f[7];
            project.fixture_profiles.push_back(std::move(profile));
        } else if (f[0] == "FIXTURE") {
            if (f.size() != 6U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid FIXTURE record.");
            }
            FixtureDefinition fixture;
            fixture.id = f[1];
            fixture.name = f[2];
            fixture.profile_id = f[3];
            if (!parse_number(f[4], fixture.universe) ||
                !parse_number(f[5], fixture.address)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid FIXTURE patch value.");
            }
            project.fixtures.push_back(std::move(fixture));
        } else if (f[0] == "GROUP") {
            if (f.size() != 3U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid GROUP record.");
            }
            project.groups.push_back({f[1], f[2], {}});
        } else if (f[0] == "LOOK") {
            if (f.size() != 4U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid LOOK record.");
            }
            LookDefinition look;
            look.id = f[1];
            look.name = f[2];
            if (!parse_number(f[3], look.fade_ms)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid LOOK fade.");
            }
            project.looks.push_back(std::move(look));
        } else if (f[0] == "AUTOLOOP") {
            if (f.size() != 7U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid AUTOLOOP record.");
            }
            AutoloopDefinition loop;
            loop.id = f[1];
            loop.name = f[2];
            if (!parse_number(f[3], loop.bank) || !parse_number(f[4], loop.slot) ||
                !parse_number(f[5], loop.length_beats) || !parse_repeat(f[6], loop.repeat)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid AUTOLOOP value.");
            }
            project.autoloops.push_back(std::move(loop));
        } else if (f[0] == "AUDIO") {
            if (f.size() != 7U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid AUDIO record.");
            }
            AudioAssetDefinition asset;
            asset.id = f[1];
            asset.name = f[2];
            asset.file_name = f[3];
            asset.sha256 = f[4];
            if (!parse_number(f[5], asset.size_bytes)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid AUDIO size.");
            }
            asset.local_path_hint = f[6];
            project.audio_assets.push_back(std::move(asset));
        } else if (f[0] == "TRACK") {
            if (f.size() != 4U && f.size() != 5U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid TRACK record.");
            }
            TrackScriptDefinition track;
            track.id = f[1];
            track.name = f[2];
            track.audio_key = f[3];
            if (f.size() == 5U) {
                track.audio_asset_id = f[4];
            }
            project.track_scripts.push_back(std::move(track));
        } else if (f[0] == "MIDI") {
            if (f.size() != 22U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid MIDI record.");
            }
            MidiMappingDefinition mapping;
            std::uint16_t message_type = 0;
            std::uint16_t input_mode = 0;
            std::uint16_t behavior = 0;
            std::uint16_t action_type = 0;
            std::uint16_t layer = 0;
            showcore::Property property{};
            if (!parse_number(f[2], mapping.preferred_input_index) ||
                !parse_number(f[3], message_type) || !parse_number(f[4], mapping.channel) ||
                !parse_number(f[5], mapping.number) || !parse_number(f[6], input_mode) ||
                !parse_number(f[7], behavior) || !parse_number(f[8], action_type) ||
                !parse_number(f[9], layer) || !parse_property(f[10], property) ||
                !parse_number(f[11], mapping.action.target_id) ||
                !parse_number(f[12], mapping.output_min) ||
                !parse_number(f[13], mapping.output_max) ||
                !parse_number(f[14], mapping.curve) || !parse_bool(f[15], mapping.inverted) ||
                !parse_bool(f[16], mapping.soft_takeover) ||
                !parse_number(f[17], mapping.takeover_tolerance)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid MIDI value.");
            }
            if (message_type > static_cast<std::uint16_t>(showcore::MidiMessageType::PitchBend) ||
                input_mode > static_cast<std::uint16_t>(showcore::MidiInputMode::RelativeTwosComplement) ||
                behavior > static_cast<std::uint16_t>(showcore::MappingBehavior::Relative) ||
                action_type >= static_cast<std::uint16_t>(showcore::ActionType::Count) ||
                layer >= static_cast<std::uint16_t>(showcore::LayerId::Count)) {
                return error(ProjectIoError::InvalidValue, record.line, "MIDI enum is outside its supported range.");
            }
            mapping.device_name = f[1];
            mapping.target_ref = f[18];
            mapping.message_type = static_cast<showcore::MidiMessageType>(message_type);
            mapping.input_mode = static_cast<showcore::MidiInputMode>(input_mode);
            mapping.behavior = static_cast<showcore::MappingBehavior>(behavior);
            mapping.action.type = static_cast<showcore::ActionType>(action_type);
            mapping.action.layer = static_cast<showcore::LayerId>(layer);
            mapping.action.property = property;
            project.midi_mappings.push_back(std::move(mapping));
        }
    }

    for (const auto& record : records) {
        if (record.fields.empty()) {
            continue;
        }
        const auto& f = record.fields;
        if (f[0] == "CHANNEL") {
            if (f.size() != 9U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid CHANNEL record.");
            }
            const auto profile = find_id(project.fixture_profiles, f[1]);
            if (profile == project.fixture_profiles.end()) {
                return error(ProjectIoError::MissingReference, record.line, "CHANNEL references a missing profile.");
            }
            ChannelDefinition channel;
            if (!parse_property(f[2], channel.property) ||
                !parse_number(f[3], channel.coarse_offset) ||
                !parse_number(f[4], channel.fine_offset) ||
                !parse_channel_encoding(f[5], channel.encoding) ||
                !parse_number(f[6], channel.dmx_min) ||
                !parse_number(f[7], channel.dmx_max) ||
                !parse_number(f[8], channel.default_value)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid CHANNEL value.");
            }
            profile->channels.push_back(channel);
        } else if (f[0] == "ROLE") {
            if (f.size() != 3U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid ROLE record.");
            }
            const auto fixture = find_id(project.fixtures, f[1]);
            if (fixture == project.fixtures.end()) {
                return error(ProjectIoError::MissingReference, record.line, "ROLE references a missing fixture.");
            }
            fixture->roles.push_back(f[2]);
        } else if (f[0] == "GROUP_MEMBER") {
            if (f.size() != 3U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid GROUP_MEMBER record.");
            }
            const auto group = find_id(project.groups, f[1]);
            if (group == project.groups.end()) {
                return error(ProjectIoError::MissingReference, record.line, "GROUP_MEMBER references a missing group.");
            }
            group->fixture_ids.push_back(f[2]);
        } else if (f[0] == "LOOK_VALUE") {
            if (f.size() != 6U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid LOOK_VALUE record.");
            }
            const auto look = find_id(project.looks, f[1]);
            if (look == project.looks.end()) {
                return error(ProjectIoError::MissingReference, record.line, "LOOK_VALUE references a missing look.");
            }
            LookAssignmentDefinition assignment;
            showcore::ValueMode mode{};
            if (!parse_property(f[3], assignment.property) || !parse_value_mode(f[4], mode) ||
                !parse_number(f[5], assignment.value.value)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid LOOK_VALUE value.");
            }
            assignment.fixture_id = f[2];
            assignment.value.mode = mode;
            if (mode != showcore::ValueMode::Set) {
                assignment.value.value = 0.0F;
            }
            look->assignments.push_back(std::move(assignment));
        } else if (f[0] == "STEP") {
            if (f.size() != 5U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid STEP record.");
            }
            const auto loop = find_id(project.autoloops, f[1]);
            if (loop == project.autoloops.end()) {
                return error(ProjectIoError::MissingReference, record.line, "STEP references a missing Autoloop.");
            }
            AutoloopStepDefinition step;
            if (!parse_number(f[2], step.at_beat) || !parse_transition(f[4], step.transition)) {
                return error(ProjectIoError::InvalidValue, record.line, "Invalid STEP value.");
            }
            step.look_id = f[3];
            loop->steps.push_back(std::move(step));
        } else if (f[0] == "TRACK_CUE") {
            if (f.size() != 5U) {
                return error(ProjectIoError::InvalidRecord, record.line, "Invalid TRACK_CUE record.");
            }
            const auto track = find_id(project.track_scripts, f[1]);
            if (track == project.track_scripts.end()) {
                return error(ProjectIoError::MissingReference, record.line,
                             "TRACK_CUE references a missing track script.");
            }
            TrackCueDefinition cue;
            if (!parse_number(f[2], cue.at_beat) || !parse_track_cue_action(f[3], cue.action)) {
                return error(ProjectIoError::InvalidValue, record.line,
                             "Invalid TRACK_CUE value.");
            }
            cue.target_ref = f[4];
            track->cues.push_back(std::move(cue));
        } else if (!is_known_primary(f[0]) && !is_known_secondary(f[0])) {
            project.unknown_records.push_back(record.raw);
        }
    }
    if (project.id.empty()) {
        return error(ProjectIoError::InvalidRecord, 0, "Project metadata record is missing.");
    }
    return {};
}

[[nodiscard]] ProjectIoResult read_file(
    const std::filesystem::path& path,
    std::string& bytes) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return error(ProjectIoError::OpenFailed, 0, "Unable to open the EmberLights project.");
    }
    const auto size = input.tellg();
    if (size < 0) {
        return error(ProjectIoError::ReadFailed, 0, "Unable to determine project size.");
    }
    if (static_cast<std::uintmax_t>(size) > kMaximumProjectBytes) {
        return error(ProjectIoError::TooLarge, 0, "Project exceeds the 64 MB safety limit.");
    }
    bytes.resize(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!bytes.empty() && !input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
        return error(ProjectIoError::ReadFailed, 0, "Unable to read the complete project.");
    }
    return {};
}

[[nodiscard]] std::filesystem::path make_history_snapshot_path(
    const std::filesystem::path& history_directory) {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#ifdef _WIN32
    if (::localtime_s(&local_time, &seconds) != 0) {
        return {};
    }
#else
    if (::localtime_r(&seconds, &local_time) == nullptr) {
        return {};
    }
#endif
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    const auto microsecond_part = static_cast<std::uint64_t>(micros >= 0 ? micros : -micros) %
        1'000'000U;
    for (std::uint32_t attempt = 0U; attempt < 32U; ++attempt) {
        std::ostringstream filename;
        filename << std::put_time(&local_time, "%Y%m%d-%H%M%S") << '-'
                 << std::setw(6) << std::setfill('0') << microsecond_part << '-'
                 << std::setw(8) << std::setfill('0')
                 << g_history_sequence.fetch_add(1U, std::memory_order_relaxed)
                 << emberlights::kProjectExtension;
        const auto candidate = history_directory / filename.str();
        std::error_code filesystem_error;
        if (!std::filesystem::exists(candidate, filesystem_error) && !filesystem_error) {
            return candidate;
        }
    }
    return {};
}

[[nodiscard]] ProjectIoResult save_history_snapshot(
    const std::filesystem::path& project_path) {
    const auto history_directory = project_history_directory(project_path);
    std::error_code filesystem_error;
    std::filesystem::create_directories(history_directory, filesystem_error);
    if (filesystem_error) {
        return error(ProjectIoError::WriteFailed, 0,
                     "Unable to create the automatic project history folder.");
    }
    const auto snapshot = make_history_snapshot_path(history_directory);
    if (snapshot.empty()) {
        return error(ProjectIoError::WriteFailed, 0,
                     "Unable to allocate a unique automatic project history entry.");
    }
    auto temporary = snapshot;
    temporary += ".tmp";
    std::filesystem::copy_file(project_path, temporary, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary, filesystem_error);
        return error(ProjectIoError::WriteFailed, 0,
                     "Unable to write the automatic project history entry.");
    }
    ProjectDocument verification;
    const auto verified = load_project(temporary, verification, false);
    if (!verified) {
        std::filesystem::remove(temporary, filesystem_error);
        return error(ProjectIoError::WriteFailed, verified.line,
                     "Automatic project history entry failed checksum verification.");
    }
    std::filesystem::rename(temporary, snapshot, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(temporary, filesystem_error);
        return error(ProjectIoError::WriteFailed, 0,
                     "Unable to activate the automatic project history entry.");
    }

    std::vector<ProjectHistoryEntry> entries;
    const auto listed = list_project_history(project_path, entries);
    if (!listed) {
        return error(ProjectIoError::RecoveryFailed, 0,
                     "Project was saved, but its automatic history could not be inspected.");
    }
    for (std::size_t index = kMaximumProjectHistoryEntries; index < entries.size(); ++index) {
        std::filesystem::remove(entries[index].path, filesystem_error);
        if (filesystem_error) {
            return error(ProjectIoError::RecoveryFailed, 0,
                         "Project was saved, but an old automatic history entry could not be pruned.");
        }
    }
    return {};
}

}  // namespace

std::string serialize_project(const ProjectDocument& project) {
    std::string payload;
    payload.reserve(4096U);
    append_record(payload, {"PROJECT", project.id, project.name});
    append_record(payload, {
        "CONNECTIONS",
        project.connections.os2l_enabled ? "1" : "0",
        project.connections.os2l_bind,
        number_text(project.connections.os2l_port),
        project.connections.artnet_enabled ? "1" : "0",
        project.connections.artnet_destination,
        number_text(project.connections.artnet_base),
        project.connections.sacn_enabled ? "1" : "0",
        project.connections.sacn_destination,
        number_text(project.connections.sacn_universe_base),
        number_text(project.connections.frame_rate),
        number_text(project.connections.manual_bpm),
        number_text(project.connections.midi_input_index),
        number_text(project.connections.midi_output_index),
        project.connections.dmx_usb_pro_ports[0],
        project.connections.dmx_usb_pro_ports[1],
        "dmxUsbProSerialV1+soundSwitchMicroWinUsbV1",
        number_text(project.connections.soundswitch_micro_universe),
        number_text(showcore::soundswitch_micro_framing_value(
            project.connections.soundswitch_micro_framing))});
    append_record(payload, {
        "SAFETY",
        project.safety.fog_requires_arm ? "1" : "0",
        project.safety.haze_requires_arm ? "1" : "0",
        project.safety.laser_requires_arm ? "1" : "0",
        project.safety.spark_requires_arm ? "1" : "0",
        project.safety.strobe_allowed ? "1" : "0",
        number_text(project.safety.max_strobe),
        number_text(project.safety.max_intensity)});

    for (const auto& profile : project.fixture_profiles) {
        append_record(payload, {
            "PROFILE", profile.id, profile.manufacturer, profile.model, profile.mode,
            profile.name, std::string(source_name(profile.source)), profile.source_revision,
            number_text(profile.footprint)});
        for (const auto& channel : profile.channels) {
            append_record(payload, {
                "CHANNEL", profile.id, std::string(property_name(channel.property)),
                number_text(channel.coarse_offset), number_text(channel.fine_offset),
                std::string(channel_encoding_name(channel.encoding)),
                number_text(channel.dmx_min), number_text(channel.dmx_max),
                number_text(channel.default_value)});
        }
    }
    for (const auto& fixture : project.fixtures) {
        append_record(payload, {
            "FIXTURE", fixture.id, fixture.name, fixture.profile_id,
            number_text(fixture.universe), number_text(fixture.address)});
        for (const auto& role : fixture.roles) {
            append_record(payload, {"ROLE", fixture.id, role});
        }
    }
    for (const auto& group : project.groups) {
        append_record(payload, {"GROUP", group.id, group.name});
        for (const auto& fixture_id : group.fixture_ids) {
            append_record(payload, {"GROUP_MEMBER", group.id, fixture_id});
        }
    }
    for (const auto& look : project.looks) {
        append_record(payload, {"LOOK", look.id, look.name, number_text(look.fade_ms)});
        for (const auto& assignment : look.assignments) {
            append_record(payload, {
                "LOOK_VALUE", look.id, assignment.fixture_id,
                std::string(property_name(assignment.property)),
                std::string(value_mode_name(assignment.value.mode)),
                number_text(assignment.value.value)});
        }
    }
    for (const auto& loop : project.autoloops) {
        append_record(payload, {
            "AUTOLOOP", loop.id, loop.name, number_text(loop.bank), number_text(loop.slot),
            number_text(loop.length_beats), std::string(repeat_name(loop.repeat))});
        for (const auto& step : loop.steps) {
            append_record(payload, {
                "STEP", loop.id, number_text(step.at_beat), step.look_id,
                std::string(transition_name(step.transition))});
        }
    }
    for (const auto& asset : project.audio_assets) {
        append_record(payload, {
            "AUDIO", asset.id, asset.name, asset.file_name, asset.sha256,
            number_text(asset.size_bytes), asset.local_path_hint});
    }
    for (const auto& track : project.track_scripts) {
        append_record(payload, {
            "TRACK", track.id, track.name, track.audio_key, track.audio_asset_id});
        for (const auto& cue : track.cues) {
            append_record(payload, {
                "TRACK_CUE", track.id, number_text(cue.at_beat),
                std::string(track_cue_action_name(cue.action)), cue.target_ref});
        }
    }
    for (const auto& mapping : project.midi_mappings) {
        append_record(payload, {
            "MIDI", mapping.device_name, number_text(mapping.preferred_input_index),
            enum_number(mapping.message_type), number_text(mapping.channel),
            number_text(mapping.number), enum_number(mapping.input_mode),
            enum_number(mapping.behavior), enum_number(mapping.action.type),
            enum_number(mapping.action.layer), std::string(property_name(mapping.action.property)),
            number_text(mapping.action.target_id), number_text(mapping.output_min),
            number_text(mapping.output_max), number_text(mapping.curve),
            mapping.inverted ? "1" : "0", mapping.soft_takeover ? "1" : "0",
            number_text(mapping.takeover_tolerance), mapping.target_ref, "0", "0", "0"});
    }
    for (const auto& unknown : project.unknown_records) {
        payload.append(unknown);
        if (unknown.empty() || unknown.back() != '\n') {
            payload.push_back('\n');
        }
    }

    std::ostringstream checksum;
    checksum << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
             << crc32(payload);
    return "EMBERLIGHTS_PROJECT\t" + number_text(kProjectFormatVersion) +
        "\nCHECKSUM\t" + checksum.str() + "\n" + payload;
}

ProjectIoResult parse_project(std::string_view serialized, ProjectDocument& project) {
    if (serialized.size() > kMaximumProjectBytes) {
        return error(ProjectIoError::TooLarge, 0, "Project exceeds the 64 MB safety limit.");
    }
    const auto first_end = serialized.find('\n');
    const auto second_end = first_end == std::string_view::npos
        ? std::string_view::npos
        : serialized.find('\n', first_end + 1U);
    if (first_end == std::string_view::npos || second_end == std::string_view::npos) {
        return error(ProjectIoError::InvalidHeader, 1, "Project header is incomplete.");
    }
    const auto header = serialized.substr(0, first_end);
    constexpr std::string_view prefix = "EMBERLIGHTS_PROJECT\t";
    if (!header.starts_with(prefix)) {
        return error(ProjectIoError::InvalidHeader, 1, "This is not an EmberLights project.");
    }
    std::uint32_t version = 0;
    if (!parse_number(header.substr(prefix.size()), version) ||
        version != kProjectFormatVersion) {
        return error(ProjectIoError::UnsupportedVersion, 1, "Project version is not supported by this build.");
    }
    const auto checksum_line = serialized.substr(first_end + 1U, second_end - first_end - 1U);
    constexpr std::string_view checksum_prefix = "CHECKSUM\t";
    if (!checksum_line.starts_with(checksum_prefix) ||
        checksum_line.size() != checksum_prefix.size() + 8U) {
        return error(ProjectIoError::MissingChecksum, 2, "Project checksum is missing or malformed.");
    }
    std::uint32_t expected = 0;
    const auto checksum_text = checksum_line.substr(checksum_prefix.size());
    const auto checksum_result = std::from_chars(
        checksum_text.data(), checksum_text.data() + checksum_text.size(), expected, 16);
    if (checksum_result.ec != std::errc{} ||
        checksum_result.ptr != checksum_text.data() + checksum_text.size()) {
        return error(ProjectIoError::MissingChecksum, 2, "Project checksum is malformed.");
    }
    const auto payload = serialized.substr(second_end + 1U);
    if (crc32(payload) != expected) {
        return error(ProjectIoError::ChecksumMismatch, 2, "Project checksum does not match its contents.");
    }

    std::vector<ParsedRecord> records;
    std::size_t offset = 0;
    std::size_t line_number = 3;
    while (offset < payload.size()) {
        auto line_end = payload.find('\n', offset);
        if (line_end == std::string_view::npos) {
            line_end = payload.size();
        }
        auto line = payload.substr(offset, line_end - offset);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (!line.empty()) {
            ParsedRecord record;
            record.line = line_number;
            record.raw.assign(line);
            std::size_t field_offset = 0;
            while (field_offset <= line.size()) {
                const auto tab = line.find('\t', field_offset);
                const auto end = tab == std::string_view::npos ? line.size() : tab;
                std::string decoded;
                if (!decode_field(line.substr(field_offset, end - field_offset), decoded)) {
                    return error(ProjectIoError::InvalidValue, line_number, "Invalid percent escape in project field.");
                }
                record.fields.push_back(std::move(decoded));
                if (tab == std::string_view::npos) {
                    break;
                }
                field_offset = tab + 1U;
            }
            records.push_back(std::move(record));
        }
        offset = line_end + (line_end < payload.size() ? 1U : 0U);
        ++line_number;
    }
    return parse_records(records, project);
}

std::filesystem::path project_backup_path(const std::filesystem::path& path) {
    auto backup = path;
    backup += ".bak";
    return backup;
}

std::filesystem::path project_active_path(const std::filesystem::path& path) {
    auto active = path;
    active += ".active";
    return active;
}

std::filesystem::path project_history_directory(const std::filesystem::path& path) {
    auto history = path;
    history += ".history";
    return history;
}

ProjectIoResult list_project_history(
    const std::filesystem::path& path,
    std::vector<ProjectHistoryEntry>& entries) {
    entries.clear();
    if (path.empty()) {
        return error(ProjectIoError::InvalidValue, 0, "Project path is empty.");
    }
    const auto history_directory = project_history_directory(path);
    std::error_code filesystem_error;
    const bool exists = std::filesystem::exists(history_directory, filesystem_error);
    if (filesystem_error) {
        return error(ProjectIoError::ReadFailed, 0,
                     "Unable to inspect the automatic project history folder.");
    }
    if (!exists) {
        return {};
    }
    if (!std::filesystem::is_directory(history_directory, filesystem_error) || filesystem_error) {
        return error(ProjectIoError::ReadFailed, 0,
                     "The automatic project history location is not a folder.");
    }
    std::filesystem::directory_iterator iterator(history_directory, filesystem_error);
    const std::filesystem::directory_iterator end;
    if (filesystem_error) {
        return error(ProjectIoError::ReadFailed, 0,
                     "Unable to enumerate automatic project history.");
    }
    while (iterator != end) {
        const auto entry = *iterator;
        const auto entry_path = entry.path();
        std::error_code entry_error;
        if (entry_path.extension() == kProjectExtension &&
            entry.is_regular_file(entry_error) && !entry_error) {
            const auto modified_at = entry.last_write_time(entry_error);
            if (entry_error) {
                return error(ProjectIoError::ReadFailed, 0,
                             "Unable to read an automatic project history timestamp.");
            }
            const auto size_bytes = entry.file_size(entry_error);
            if (entry_error) {
                return error(ProjectIoError::ReadFailed, 0,
                             "Unable to read an automatic project history entry size.");
            }
            entries.push_back({entry_path, modified_at, size_bytes});
        } else if (entry_error) {
            return error(ProjectIoError::ReadFailed, 0,
                         "Unable to inspect an automatic project history entry.");
        }
        iterator.increment(filesystem_error);
        if (filesystem_error) {
            return error(ProjectIoError::ReadFailed, 0,
                         "Unable to enumerate automatic project history.");
        }
    }
    std::sort(
        entries.begin(), entries.end(),
        [](const ProjectHistoryEntry& first, const ProjectHistoryEntry& second) {
            if (first.modified_at != second.modified_at) {
                return first.modified_at > second.modified_at;
            }
            return first.path.filename().native() > second.path.filename().native();
        });
    return {};
}

ProjectIoResult load_project(
    const std::filesystem::path& path,
    ProjectDocument& project,
    bool allow_backup_recovery) {
    std::string bytes;
    auto result = read_file(path, bytes);
    if (result) {
        result = parse_project(bytes, project);
    }
    if (result || !allow_backup_recovery) {
        return result;
    }

    const auto backup = project_backup_path(path);
    std::string backup_bytes;
    auto backup_result = read_file(backup, backup_bytes);
    if (backup_result) {
        backup_result = parse_project(backup_bytes, project);
    }
    if (!backup_result) {
        result.message += " Backup recovery also failed: " + backup_result.message;
        return result;
    }
    backup_result.recovered_from_backup = true;
    backup_result.message = "The primary project was invalid; its last-known-good backup was loaded.";
    return backup_result;
}

ProjectIoResult save_project_atomic(
    const std::filesystem::path& path,
    const ProjectDocument& project,
    bool capture_history) {
    if (path.empty()) {
        return error(ProjectIoError::WriteFailed, 0, "Project path is empty.");
    }
    std::error_code filesystem_error;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, filesystem_error);
        if (filesystem_error) {
            return error(ProjectIoError::WriteFailed, 0, "Unable to create the project folder.");
        }
    }
    auto temporary = path;
    temporary += ".tmp";
    auto previous = path;
    previous += ".previous";
    const auto backup = project_backup_path(path);

    const auto serialized = serialize_project(project);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output || !output.write(serialized.data(), static_cast<std::streamsize>(serialized.size())) ||
            !output.flush()) {
            output.close();
            std::filesystem::remove(temporary, filesystem_error);
            return error(ProjectIoError::WriteFailed, 0, "Unable to write the complete temporary project.");
        }
    }

    ProjectDocument verification;
    const auto verify_result = load_project(temporary, verification, false);
    if (!verify_result) {
        std::filesystem::remove(temporary, filesystem_error);
        return error(ProjectIoError::WriteFailed, verify_result.line,
                     "Temporary project failed its own checksum verification.");
    }

    const bool had_original = std::filesystem::exists(path, filesystem_error) && !filesystem_error;
    std::filesystem::remove(previous, filesystem_error);
    filesystem_error.clear();
    if (had_original) {
        std::filesystem::rename(path, previous, filesystem_error);
        if (filesystem_error) {
            std::filesystem::remove(temporary, filesystem_error);
            return error(ProjectIoError::ReplaceFailed, 0, "Unable to stage the previous project for replacement.");
        }
    }
    std::filesystem::rename(temporary, path, filesystem_error);
    if (filesystem_error) {
        if (had_original) {
            std::error_code restore_error;
            std::filesystem::rename(previous, path, restore_error);
        }
        std::filesystem::remove(temporary, filesystem_error);
        return error(ProjectIoError::ReplaceFailed, 0, "Unable to activate the verified project file.");
    }
    if (had_original) {
        std::filesystem::copy_file(
            previous, backup, std::filesystem::copy_options::overwrite_existing, filesystem_error);
        std::error_code remove_error;
        std::filesystem::remove(previous, remove_error);
        if (filesystem_error) {
            return error(ProjectIoError::RecoveryFailed, 0,
                         "Project was saved, but its recovery backup could not be refreshed.");
        }
    }
    if (!capture_history) {
        return {};
    }
    const auto history_result = save_history_snapshot(path);
    if (history_result) {
        return {};
    }
    return {ProjectIoError::None, 0, false,
            "Project was saved safely, but its automatic history could not be updated: " +
                history_result.message};
}

ProjectIoResult restore_project_history(
    const std::filesystem::path& path,
    const std::filesystem::path& history_path,
    ProjectDocument& restored_project) {
    if (path.empty() || history_path.empty()) {
        return error(ProjectIoError::InvalidValue, 0, "Project and restore-point paths are required.");
    }
    std::error_code filesystem_error;
    const auto history_directory = std::filesystem::weakly_canonical(
        project_history_directory(path), filesystem_error);
    if (filesystem_error) {
        return error(ProjectIoError::ReadFailed, 0,
                     "Unable to locate this project's automatic history folder.");
    }
    const auto candidate_path = std::filesystem::weakly_canonical(history_path, filesystem_error);
    if (filesystem_error || candidate_path.parent_path() != history_directory ||
        candidate_path.extension() != kProjectExtension) {
        return error(ProjectIoError::InvalidValue, 0,
                     "The selected restore point does not belong to this project's automatic history.");
    }

    std::vector<ProjectHistoryEntry> entries;
    const auto listed = list_project_history(path, entries);
    if (!listed) {
        return listed;
    }
    const auto is_listed = std::any_of(
        entries.begin(), entries.end(), [&](const ProjectHistoryEntry& entry) {
            std::error_code entry_error;
            return std::filesystem::equivalent(entry.path, candidate_path, entry_error) && !entry_error;
        });
    if (!is_listed) {
        return error(ProjectIoError::InvalidValue, 0,
                     "The selected restore point is no longer available in this project's history.");
    }

    ProjectDocument restored;
    auto result = load_project(candidate_path, restored, false);
    if (!result) {
        result.message = "The selected restore point failed integrity validation: " + result.message;
        return result;
    }
    result = save_project_atomic(path, restored, true);
    if (!result) {
        return result;
    }
    restored_project = std::move(restored);
    const auto warning = result.message;
    result.message = "Saved version restored safely.";
    if (!warning.empty()) {
        result.message += " " + warning;
    }
    return result;
}

}  // namespace emberlights
