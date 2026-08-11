#include "emberlights/soundswitch_v1.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

using showcore::Property;
using showcore::PropertyValue;

constexpr std::array<std::string_view, 32> kDefaultAutoloopNames{{
    "Medium", "Colorful", "Slow Dance", "Flashy", "Red - Smooth Pulse",
    "Blue - Smooth", "Purp+Yelo - Square", "Blue - Pulse",
    "Green+blu - Wave", "80s - Smooth", "Sunny - Smooth",
    "teal+Pink - Square", "Teal & Magenta", "All Color Pass Thru",
    "Bouncey", "Pastel Party", "NYC", "Red white pulse", "Dreamy Amber",
    "Contrast", "Slow Dance", "Fire & Ice", "Shiny Berries",
    "Dreamy Colors", "Color Pulse", "Neon Dream", "Dreamy Antique",
    "Super Flashy", "Slow Dance 2", "Reggae Dream", "Peachy Keen",
    "Vie En Rose"
}};

constexpr std::array<std::string_view, 4> kRequiredFixtureModels{{
    "6x18W RGBWA UV 6in1 Uplight (BO-S601)",
    "BO-Tube 192 360 Pixel Tube",
    "Wash FX HEX",
    "BO-IR4 LED Mini Spotlight"
}};

struct Color {
    float red{0.0F};
    float green{0.0F};
    float blue{0.0F};
    float white{0.0F};
    float amber{0.0F};
    float uv{0.0F};
    float intensity{0.72F};
};

struct PaletteLook {
    std::string id;
    std::string name;
    std::uint32_t fade_ms{500U};
    std::vector<Color> colors;
    bool blackout{false};
};

[[nodiscard]] bool read_bounded_file(
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes,
    std::vector<std::uint8_t>& bytes) {
    std::error_code filesystem_error;
    if (!std::filesystem::is_regular_file(path, filesystem_error) || filesystem_error) {
        return false;
    }
    const auto size = std::filesystem::file_size(path, filesystem_error);
    if (filesystem_error || size > maximum_bytes) {
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size));
    if (!bytes.empty() && !input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        return false;
    }
    return true;
}

[[nodiscard]] bool read_bounded_text(
    const std::filesystem::path& path,
    std::uint64_t maximum_bytes,
    std::string& text) {
    std::vector<std::uint8_t> bytes;
    if (!read_bounded_file(path, maximum_bytes, bytes)) {
        return false;
    }
    text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

[[nodiscard]] bool contains_utf16_ascii(
    const std::vector<std::uint8_t>& bytes,
    std::string_view text) {
    if (text.empty() || text.size() * 2U > bytes.size()) {
        return false;
    }
    for (std::size_t offset = 0U; offset + text.size() * 2U <= bytes.size(); ++offset) {
        bool matches = true;
        for (std::size_t index = 0U; index < text.size(); ++index) {
            if (bytes[offset + index * 2U] != static_cast<std::uint8_t>(text[index]) ||
                bytes[offset + index * 2U + 1U] != 0U) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<std::string> scan_utf16_ascii_strings(
    const std::vector<std::uint8_t>& bytes,
    std::size_t maximum_strings) {
    std::vector<std::string> strings;
    for (std::size_t offset = 0U;
         offset + 1U < bytes.size() && strings.size() < maximum_strings;) {
        const auto first = bytes[offset];
        if (first < 0x20U || first > 0x7EU || bytes[offset + 1U] != 0U) {
            ++offset;
            continue;
        }
        std::string value;
        auto cursor = offset;
        while (cursor + 1U < bytes.size() &&
               bytes[cursor] >= 0x20U && bytes[cursor] <= 0x7EU &&
               bytes[cursor + 1U] == 0U) {
            value.push_back(static_cast<char>(bytes[cursor]));
            cursor += 2U;
        }
        if (value.size() >= 3U) {
            strings.push_back(std::move(value));
        }
        offset = std::max(cursor, offset + 1U);
    }
    return strings;
}

[[nodiscard]] std::string manifest_id(std::string_view manifest) {
    const auto key = manifest.find("\"id\"");
    if (key == std::string_view::npos) {
        return {};
    }
    const auto colon = manifest.find(':', key + 4U);
    const auto first_quote = colon == std::string_view::npos
        ? std::string_view::npos
        : manifest.find('"', colon + 1U);
    const auto second_quote = first_quote == std::string_view::npos
        ? std::string_view::npos
        : manifest.find('"', first_quote + 1U);
    if (first_quote == std::string_view::npos || second_quote == std::string_view::npos) {
        return {};
    }
    return std::string(manifest.substr(first_quote + 1U, second_quote - first_quote - 1U));
}

[[nodiscard]] std::string lowercase(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return lowered;
}

[[nodiscard]] FixtureProfileDefinition make_profile(
    std::string id,
    std::string manufacturer,
    std::string model,
    std::string mode,
    std::vector<Property> properties) {
    FixtureProfileDefinition profile;
    profile.id = std::move(id);
    profile.manufacturer = std::move(manufacturer);
    profile.model = std::move(model);
    profile.mode = std::move(mode);
    profile.name = profile.manufacturer + " " + profile.model + " (" + profile.mode + ")";
    profile.source = showcore::FixtureProfileSource::Local;
    profile.source_revision = "soundswitch-2.10.3-safe-v1";
    profile.footprint = static_cast<std::uint16_t>(properties.size());
    for (std::size_t index = 0U; index < properties.size(); ++index) {
        profile.channels.push_back({
            properties[index],
            static_cast<std::uint16_t>(index),
            -1,
            showcore::ChannelEncoding::Linear8,
            0U,
            255U,
            0U});
    }
    return profile;
}

[[nodiscard]] bool profile_has_property(
    const FixtureProfileDefinition& profile,
    Property property) {
    return std::any_of(profile.channels.begin(), profile.channels.end(), [property](const auto& channel) {
        return channel.property == property;
    });
}

[[nodiscard]] float color_value(Property property, const Color& color) noexcept {
    switch (property) {
    case Property::Intensity: return color.intensity;
    case Property::Red: return color.red;
    case Property::Green: return color.green;
    case Property::Blue: return color.blue;
    case Property::White: return color.white;
    case Property::Amber: return color.amber;
    case Property::UV: return color.uv;
    default: return 0.0F;
    }
}

void append_palette_look(ProjectDocument& project, const PaletteLook& definition) {
    LookDefinition look;
    look.id = definition.id;
    look.name = definition.name;
    look.fade_ms = definition.fade_ms;
    std::unordered_map<std::string_view, const FixtureProfileDefinition*> profiles;
    for (const auto& profile : project.fixture_profiles) {
        profiles.emplace(profile.id, &profile);
    }
    for (std::size_t fixture_index = 0U; fixture_index < project.fixtures.size(); ++fixture_index) {
        const auto& fixture = project.fixtures[fixture_index];
        const auto found = profiles.find(fixture.profile_id);
        if (found == profiles.end()) {
            continue;
        }
        const auto& color = definition.colors[fixture_index % definition.colors.size()];
        for (const auto property : {
                 Property::Intensity, Property::Red, Property::Green, Property::Blue,
                 Property::White, Property::Amber, Property::UV, Property::Strobe,
                 Property::Custom1, Property::Custom2, Property::Custom3}) {
            if (!profile_has_property(*found->second, property)) {
                continue;
            }
            const bool force_zero = definition.blackout || property == Property::Strobe ||
                property == Property::Custom1 || property == Property::Custom2 ||
                property == Property::Custom3;
            look.assignments.push_back({
                fixture.id,
                property,
                force_zero ? PropertyValue::force_zero()
                           : PropertyValue::set(color_value(property, color))});
        }
    }
    project.looks.push_back(std::move(look));
}

[[nodiscard]] std::vector<std::string> normalized_autoloop_names(
    const std::vector<std::string>& names) {
    std::vector<std::string> result;
    result.reserve(kDefaultAutoloopNames.size());
    for (const auto& name : names) {
        if (!name.empty() && result.size() < kDefaultAutoloopNames.size()) {
            result.push_back(name);
        }
    }
    for (std::size_t index = result.size(); index < kDefaultAutoloopNames.size(); ++index) {
        result.emplace_back(kDefaultAutoloopNames[index]);
    }
    return result;
}

void append_loop(
    ProjectDocument& project,
    std::size_t slot,
    std::string name,
    float length,
    showcore::AutoloopTransition transition,
    std::initializer_list<std::string_view> look_ids) {
    AutoloopDefinition loop;
    loop.id = "soundswitch-v1-loop-" + std::to_string(slot + 1U);
    loop.name = std::move(name);
    loop.bank = 0U;
    loop.slot = static_cast<std::uint8_t>(slot);
    loop.length_beats = length;
    loop.repeat = showcore::AutoloopRepeat::Infinite;
    const auto beat_step = length / static_cast<float>(look_ids.size());
    std::size_t index = 0U;
    for (const auto look_id : look_ids) {
        loop.steps.push_back({beat_step * static_cast<float>(index++), std::string(look_id), transition});
    }
    project.autoloops.push_back(std::move(loop));
}

void append_named_loop(ProjectDocument& project, std::size_t slot, const std::string& name) {
    const auto lowered = lowercase(name);
    const bool smooth = lowered.find("smooth") != std::string::npos ||
        lowered.find("slow") != std::string::npos ||
        lowered.find("dreamy") != std::string::npos ||
        lowered.find("wave") != std::string::npos;
    const auto transition = smooth
        ? showcore::AutoloopTransition::Linear
        : showcore::AutoloopTransition::Cut;
    if (lowered.find("red white") != std::string::npos) {
        append_loop(project, slot, name, 4.0F, transition,
                    {"look.red", "look.blackout", "look.white", "look.blackout"});
    } else if (lowered.find("pulse") != std::string::npos) {
        const auto color = lowered.find("red") != std::string::npos ? "look.red" :
            lowered.find("blue") != std::string::npos ? "look.blue" : "look.colorful";
        append_loop(project, slot, name, 4.0F, transition,
                    {color, "look.blackout", color, "look.blackout"});
    } else if (lowered.find("purp") != std::string::npos ||
               lowered.find("yellow") != std::string::npos) {
        append_loop(project, slot, name, 4.0F, transition,
                    {"look.purple-yellow", "look.yellow"});
    } else if (lowered.find("teal") != std::string::npos ||
               lowered.find("magenta") != std::string::npos ||
               lowered.find("pink") != std::string::npos) {
        append_loop(project, slot, name, smooth ? 8.0F : 4.0F, transition,
                    {"look.teal-magenta", "look.rose"});
    } else if (lowered.find("green") != std::string::npos ||
               lowered.find("reggae") != std::string::npos) {
        append_loop(project, slot, name, 4.0F, transition,
                    {"look.green-blue", "look.amber", "look.red", "look.green"});
    } else if (lowered.find("fire") != std::string::npos ||
               lowered.find("ice") != std::string::npos) {
        append_loop(project, slot, name, 8.0F, transition,
                    {"look.fire-ice", "look.amber", "look.blue", "look.white"});
    } else if (lowered.find("pastel") != std::string::npos ||
               lowered.find("berries") != std::string::npos) {
        append_loop(project, slot, name, 8.0F, transition,
                    {"look.pastel", "look.rose", "look.teal-magenta", "look.purple-yellow"});
    } else if (lowered.find("amber") != std::string::npos ||
               lowered.find("antique") != std::string::npos ||
               lowered.find("sunny") != std::string::npos ||
               lowered.find("peach") != std::string::npos) {
        append_loop(project, slot, name, 8.0F, transition,
                    {"look.warm", "look.amber", "look.antique", "look.rose"});
    } else if (lowered.find("slow dance") != std::string::npos) {
        append_loop(project, slot, name, 8.0F, showcore::AutoloopTransition::Linear,
                    {"look.blue", "look.rose", "look.purple-yellow", "look.teal"});
    } else if (lowered.find("flash") != std::string::npos ||
               lowered.find("fast") != std::string::npos) {
        append_loop(project, slot, name, 4.0F, showcore::AutoloopTransition::Cut,
                    {"look.red", "look.blue", "look.green", "look.white",
                     "look.magenta", "look.teal", "look.yellow", "look.colorful"});
    } else {
        append_loop(project, slot, name, smooth ? 8.0F : 4.0F, transition,
                    {"look.colorful", "look.blue", "look.rose", "look.green-blue"});
    }
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream output;
    for (const auto character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (static_cast<unsigned char>(character) < 0x20U) {
                output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                       << static_cast<unsigned int>(static_cast<unsigned char>(character));
            } else {
                output << character;
            }
        }
    }
    return output.str();
}

}  // namespace

ProjectDocument make_safe_color_rig_v1_template(
    const std::vector<std::string>& autoloop_names) {
    auto project = make_starter_project();
    project.id = "emberlights-safe-color-rig-v1";
    project.name = "SoundSwitch 2026 Color Rig V1 - PATCH REVIEW REQUIRED";
    project.connections.os2l_enabled = true;
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.dmx_usb_pro_ports = {};
    project.connections.manual_bpm = 120.0;
    project.connections.frame_rate = 40U;
    project.safety.strobe_allowed = true;
    project.safety.max_strobe = 0.35F;
    project.safety.max_intensity = 0.75F;

    project.fixture_profiles.push_back(make_profile(
        "soundswitch.both-lighting.bo-s601.mode2",
        "Both Lighting", "6x18W RGBWA UV 6in1 Uplight (BO-S601)", "Mode 2",
        {Property::Intensity, Property::Red, Property::Green, Property::Blue,
         Property::Amber, Property::White, Property::UV, Property::Strobe,
         Property::Custom1, Property::Custom2}));
    project.fixture_profiles.push_back(make_profile(
        "soundswitch.chauvet.wash-fx-hex.mode1",
        "Chauvet", "Wash FX HEX", "Mode 1",
        {Property::Intensity, Property::Strobe, Property::Red, Property::Green,
         Property::Blue, Property::White, Property::Amber, Property::UV,
         Property::Custom1, Property::Custom2, Property::Custom3}));
    project.fixture_profiles.push_back(make_profile(
        "soundswitch.both-lighting.bo-ir4.mode1",
        "Both Lighting", "BO-IR4 LED Mini Spotlight", "Mode 1",
        {Property::Intensity, Property::Red, Property::Green, Property::Blue,
         Property::Amber, Property::White, Property::UV, Property::Strobe,
         Property::Custom1, Property::Custom2}));

    std::vector<std::string> all_color;
    std::vector<std::string> uplights;
    for (std::uint16_t index = 0U; index < 4U; ++index) {
        const auto id = "uplight-" + std::to_string(index + 1U);
        project.fixtures.push_back({
            id,
            "Uplight " + std::to_string(index + 1U),
            "soundswitch.both-lighting.bo-s601.mode2",
            1U,
            static_cast<std::uint16_t>(1U + index * 10U),
            {"uplight", "color", "event-wash"}});
        uplights.push_back(id);
        all_color.push_back(id);
    }

    std::vector<std::string> tubes;
    for (std::uint16_t tube = 0U; tube < 4U; ++tube) {
        std::vector<std::string> cells;
        const auto base = static_cast<std::uint16_t>(41U + tube * 48U);
        for (std::uint16_t cell = 0U; cell < 16U; ++cell) {
            const auto id = "tube-" + std::to_string(tube + 1U) +
                "-cell-" + std::to_string(cell + 1U);
            project.fixtures.push_back({
                id,
                "Tube " + std::to_string(tube + 1U) + " Cell " + std::to_string(cell + 1U),
                "builtin.generic.rgb-3ch",
                1U,
                static_cast<std::uint16_t>(base + cell * 3U),
                {"tube", "pixel", "color"}});
            cells.push_back(id);
            tubes.push_back(id);
            all_color.push_back(id);
        }
        project.groups.push_back({
            "group.tube-" + std::to_string(tube + 1U),
            "BO-Tube " + std::to_string(tube + 1U),
            std::move(cells)});
    }

    project.fixtures.push_back({
        "wash-fx-hex-1", "Dance Floor Wash FX HEX",
        "soundswitch.chauvet.wash-fx-hex.mode1", 1U, 233U,
        {"wash", "dance-floor", "color"}});
    all_color.push_back("wash-fx-hex-1");

    std::vector<std::string> ir4;
    for (std::uint16_t index = 0U; index < 2U; ++index) {
        const auto id = "ir4-" + std::to_string(index + 1U);
        project.fixtures.push_back({
            id,
            "BO-IR4 Spotlight " + std::to_string(index + 1U),
            "soundswitch.both-lighting.bo-ir4.mode1",
            1U,
            static_cast<std::uint16_t>(244U + index * 10U),
            {"spotlight", "color", "accent"}});
        ir4.push_back(id);
        all_color.push_back(id);
    }

    project.groups.push_back({"group.uplights", "4 Uplights", uplights});
    project.groups.push_back({"group.tubes", "All BO-Tubes", tubes});
    project.groups.push_back({"group.wash", "Wash (Dance Floor)", {"wash-fx-hex-1"}});
    project.groups.push_back({"group.ir4", "BO-IR4 Spotlights", ir4});
    project.groups.push_back({"group.all-color", "All Color Fixtures", all_color});

    const Color red{1.0F, 0.0F, 0.0F};
    const Color blue{0.0F, 0.08F, 1.0F};
    const Color green{0.0F, 1.0F, 0.12F};
    const Color amber{0.7F, 0.16F, 0.0F, 0.0F, 1.0F};
    const Color yellow{1.0F, 0.6F, 0.0F, 0.0F, 0.5F};
    const Color magenta{1.0F, 0.0F, 0.65F};
    const Color teal{0.0F, 0.75F, 0.72F};
    const Color white{0.18F, 0.18F, 0.18F, 1.0F, 0.0F, 0.0F, 0.68F};
    const Color warm{0.45F, 0.08F, 0.0F, 0.2F, 1.0F, 0.0F, 0.62F};
    const Color pastel{0.45F, 0.18F, 0.7F, 0.12F, 0.18F, 0.08F, 0.58F};
    const Color rose{0.9F, 0.03F, 0.32F, 0.04F, 0.12F, 0.0F, 0.62F};
    const Color antique{0.28F, 0.06F, 0.0F, 0.28F, 0.85F, 0.0F, 0.55F};
    const Color purple{0.42F, 0.0F, 0.85F};
    const Color ice{0.0F, 0.28F, 0.82F, 0.15F, 0.0F, 0.05F, 0.68F};

    const std::vector<PaletteLook> looks{
        {"look.blackout", "Blackout", 0U, {Color{}}, true},
        {"look.red", "Red", 350U, {red}},
        {"look.blue", "Blue", 500U, {blue}},
        {"look.green", "Green", 350U, {green}},
        {"look.amber", "Amber", 650U, {amber}},
        {"look.yellow", "Yellow", 400U, {yellow}},
        {"look.magenta", "Magenta", 400U, {magenta}},
        {"look.teal", "Teal", 500U, {teal}},
        {"look.white", "Photo White", 500U, {white}},
        {"look.warm", "Dinner Warm", 900U, {warm}},
        {"look.pastel", "Pastel Party", 800U, {pastel, rose, teal, purple}},
        {"look.rose", "Vie En Rose", 700U, {rose}},
        {"look.purple-yellow", "Purple + Yellow", 350U, {purple, yellow}},
        {"look.teal-magenta", "Teal + Magenta", 350U, {teal, magenta}},
        {"look.green-blue", "Green + Blue", 450U, {green, blue}},
        {"look.fire-ice", "Fire + Ice", 400U, {amber, red, ice, blue}},
        {"look.colorful", "All Color", 250U, {red, blue, green, magenta, teal, yellow}},
        {"look.antique", "Dreamy Antique", 900U, {antique, warm}}
    };
    for (const auto& look : looks) {
        append_palette_look(project, look);
    }

    const auto loop_names = normalized_autoloop_names(autoloop_names);
    for (std::size_t slot = 0U; slot < loop_names.size(); ++slot) {
        append_named_loop(project, slot, loop_names[slot]);
    }
    return project;
}

SoundSwitchV1MigrationResult create_soundswitch_v1_project(
    const std::filesystem::path& source_root) {
    SoundSwitchV1MigrationResult result;
    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(source_root, filesystem_error) || filesystem_error) {
        result.error = SoundSwitchV1MigrationError::MissingSource;
        result.message = "The SoundSwitch source must be an extracted .ssproj directory.";
        return result;
    }
    const auto manifest_path = source_root / ".ssproj";
    const auto venue_path = source_root / "SoundSwitchVenues.bin";
    const auto loops_path = source_root / "SoundSwitchAutoLoops.bin";
    std::string manifest;
    std::vector<std::uint8_t> venue;
    std::vector<std::uint8_t> loops;
    if (!read_bounded_text(manifest_path, 64U * 1024U, manifest) ||
        !read_bounded_file(venue_path, 64U * 1024U * 1024U, venue) ||
        !read_bounded_file(loops_path, 8U * 1024U * 1024U, loops)) {
        result.error = SoundSwitchV1MigrationError::ReadFailed;
        result.message = "The manifest, venue database, or active Autoloop database could not be read.";
        return result;
    }
    if (manifest.find("\"major\": 2") == std::string::npos ||
        manifest.find("\"minor\": 10") == std::string::npos) {
        result.error = SoundSwitchV1MigrationError::UnsupportedSource;
        result.message = "This conservative V1 converter is qualified only for SoundSwitch 2.10.x exports.";
        return result;
    }
    for (const auto model : kRequiredFixtureModels) {
        if (!contains_utf16_ascii(venue, model)) {
            result.error = SoundSwitchV1MigrationError::UnsupportedSource;
            result.message = "The source does not contain the complete recognized 2026 color rig.";
            return result;
        }
        result.recognized_fixture_models.emplace_back(model);
    }
    const auto venue_identity = identify_file_sha256(venue_path, 64U * 1024U * 1024U);
    const auto loops_identity = identify_file_sha256(loops_path, 8U * 1024U * 1024U);
    if (!venue_identity.success || !loops_identity.success) {
        result.error = SoundSwitchV1MigrationError::ReadFailed;
        result.message = "The migration inputs could not be hashed completely.";
        return result;
    }
    result.manifest_id = manifest_id(manifest);
    result.venue_sha256 = venue_identity.sha256;
    result.autoloops_sha256 = loops_identity.sha256;
    result.source_autoloop_names = scan_utf16_ascii_strings(loops, 32U);
    if (result.source_autoloop_names.size() < 32U) {
        result.error = SoundSwitchV1MigrationError::UnsupportedSource;
        result.message = "The active SoundSwitch Autoloop bank did not contain 32 readable names.";
        return result;
    }
    result.project = make_safe_color_rig_v1_template(result.source_autoloop_names);
    result.project.id = "soundswitch-v1-" + result.venue_sha256.substr(0U, 16U);
    result.project.unknown_records.push_back(
        "SOUNDSWITCH_SOURCE\t2.10.x\t" + result.manifest_id + "\t" +
        result.venue_sha256 + "\t" + result.autoloops_sha256 +
        "\tsemantic-v1-safe-patch");
    const auto validation = validate_project(result.project);
    const auto compilation = compile_project(result.project);
    if (!validation.ok() || !compilation) {
        result.error = SoundSwitchV1MigrationError::InvalidProject;
        result.message = "The staged EmberLights project did not pass validation and compilation.";
        return result;
    }
    result.warnings = {
        "All Art-Net, sACN, and USB-DMX outputs are disabled until the physical patch is checked.",
        "Universe 1 addresses 1-263 are a safe non-overlapping staging layout, not decoded SoundSwitch addresses.",
        "The 32 active Autoloop names were retained; their native semantic patterns were rebuilt from the names rather than claimed as binary cue decoding.",
        "Mover, GigBar, PartyBar, cold-spark, and purchased track-show payloads remain in the original archive and are not enabled in this first-pilot color rig.",
        "Keep SoundSwitch and the original export available as the rehearsed fallback for the first pilot."
    };
    result.message = "Created a validated, output-disabled first-pilot project with 4 uplights, "
        "4 x 16-cell tubes, 1 dance-floor wash, 2 IR-4 spotlights, 18 Static Looks, "
        "and 32 named Autoloops.";
    return result;
}

std::string serialize_soundswitch_v1_migration_report(
    const SoundSwitchV1MigrationResult& migration) {
    std::ostringstream output;
    output << "{\n"
           << "  \"format\": \"emberlights-soundswitch-v1-migration\",\n"
           << "  \"formatVersion\": 1,\n"
           << "  \"success\": " << (migration ? "true" : "false") << ",\n"
           << "  \"message\": \"" << json_escape(migration.message) << "\",\n"
           << "  \"manifestId\": \"" << json_escape(migration.manifest_id) << "\",\n"
           << "  \"venueSha256\": \"" << migration.venue_sha256 << "\",\n"
           << "  \"autoloopsSha256\": \"" << migration.autoloops_sha256 << "\",\n"
           << "  \"outputEnabled\": false,\n"
           << "  \"stagedPatch\": {\"universe\": 1, \"firstAddress\": 1, \"lastAddress\": 263},\n"
           << "  \"projectCounts\": {"
           << "\"profiles\": " << migration.project.fixture_profiles.size() << ", "
           << "\"fixtures\": " << migration.project.fixtures.size() << ", "
           << "\"groups\": " << migration.project.groups.size() << ", "
           << "\"looks\": " << migration.project.looks.size() << ", "
           << "\"autoloops\": " << migration.project.autoloops.size() << "},\n"
           << "  \"recognizedFixtureModels\": [";
    for (std::size_t index = 0U; index < migration.recognized_fixture_models.size(); ++index) {
        output << (index == 0U ? "" : ", ") << "\""
               << json_escape(migration.recognized_fixture_models[index]) << "\"";
    }
    output << "],\n  \"activeAutoloopNames\": [";
    for (std::size_t index = 0U; index < migration.source_autoloop_names.size(); ++index) {
        output << (index == 0U ? "" : ", ") << "\""
               << json_escape(migration.source_autoloop_names[index]) << "\"";
    }
    output << "],\n  \"warnings\": [";
    for (std::size_t index = 0U; index < migration.warnings.size(); ++index) {
        output << (index == 0U ? "" : ", ") << "\""
               << json_escape(migration.warnings[index]) << "\"";
    }
    output << "]\n}\n";
    return output.str();
}

}  // namespace emberlights
