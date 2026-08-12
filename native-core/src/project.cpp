#include "emberlights/project.hpp"

#include "emberlights/file_identity.hpp"
#include "emberlights/fixture_capabilities.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"

#include "showcore/dmx_usb_pro.hpp"
#include "showcore/fixture_library.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace emberlights {
namespace {

using PropertyName = std::pair<showcore::Property, std::string_view>;

constexpr std::array<PropertyName, showcore::kPropertyCount> kPropertyNames{{
    {showcore::Property::Intensity, "intensity"},
    {showcore::Property::Red, "red"},
    {showcore::Property::Green, "green"},
    {showcore::Property::Blue, "blue"},
    {showcore::Property::White, "white"},
    {showcore::Property::Amber, "amber"},
    {showcore::Property::UV, "uv"},
    {showcore::Property::Cyan, "cyan"},
    {showcore::Property::Magenta, "magenta"},
    {showcore::Property::Yellow, "yellow"},
    {showcore::Property::Lime, "lime"},
    {showcore::Property::Indigo, "indigo"},
    {showcore::Property::Pan, "pan"},
    {showcore::Property::Tilt, "tilt"},
    {showcore::Property::PanRotate, "panRotate"},
    {showcore::Property::TiltRotate, "tiltRotate"},
    {showcore::Property::PanTiltSpeed, "panTiltSpeed"},
    {showcore::Property::Strobe, "strobe"},
    {showcore::Property::Shutter, "shutter"},
    {showcore::Property::ColorWheel, "colorWheel"},
    {showcore::Property::Gobo, "gobo"},
    {showcore::Property::GoboRotation, "goboRotation"},
    {showcore::Property::Prism, "prism"},
    {showcore::Property::PrismRotation, "prismRotation"},
    {showcore::Property::Focus, "focus"},
    {showcore::Property::Zoom, "zoom"},
    {showcore::Property::Iris, "iris"},
    {showcore::Property::Frost, "frost"},
    {showcore::Property::Animation, "animation"},
    {showcore::Property::AnimationRotation, "animationRotation"},
    {showcore::Property::Effect, "effect"},
    {showcore::Property::EffectSpeed, "effectSpeed"},
    {showcore::Property::Fan, "fan"},
    {showcore::Property::Fog, "fog"},
    {showcore::Property::Haze, "haze"},
    {showcore::Property::Laser, "laser"},
    {showcore::Property::Spark, "spark"},
    {showcore::Property::Custom1, "custom1"},
    {showcore::Property::Custom2, "custom2"},
    {showcore::Property::Custom3, "custom3"},
    {showcore::Property::Custom4, "custom4"},
    {showcore::Property::Custom5, "custom5"},
    {showcore::Property::Custom6, "custom6"},
    {showcore::Property::Custom7, "custom7"},
    {showcore::Property::Custom8, "custom8"},
    {showcore::Property::Custom9, "custom9"},
    {showcore::Property::Custom10, "custom10"},
    {showcore::Property::Custom11, "custom11"},
    {showcore::Property::Custom12, "custom12"},
    {showcore::Property::Custom13, "custom13"},
    {showcore::Property::Custom14, "custom14"},
    {showcore::Property::Custom15, "custom15"},
    {showcore::Property::Custom16, "custom16"}
}};

void add_issue(
    ProjectValidation& result,
    ProjectIssueSeverity severity,
    std::string code,
    std::string subject,
    std::string message) {
    result.issues.push_back({severity, std::move(code), std::move(subject), std::move(message)});
}

[[nodiscard]] bool valid_identifier(std::string_view value) {
    return !value.empty() && value.size() <= showcore::kFixtureProfileTextLength;
}

[[nodiscard]] bool finite_normalized(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool valid_file_name(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 255U &&
        value.find('/') == std::string_view::npos &&
        value.find('\\') == std::string_view::npos;
}

FixtureProfileDefinition make_profile(
    std::string id,
    std::string model,
    std::string mode,
    std::vector<showcore::Property> properties) {
    FixtureProfileDefinition profile;
    profile.id = std::move(id);
    profile.manufacturer = "Generic";
    profile.model = std::move(model);
    profile.mode = std::move(mode);
    profile.name = profile.manufacturer + " " + profile.model + " (" + profile.mode + ")";
    profile.source = showcore::FixtureProfileSource::BuiltIn;
    profile.source_revision = "emberlights-v1";
    profile.footprint = static_cast<std::uint16_t>(properties.size());
    profile.channels.reserve(properties.size());
    for (std::size_t index = 0; index < properties.size(); ++index) {
        profile.channels.push_back({
            properties[index],
            static_cast<std::uint16_t>(index),
            -1,
            showcore::ChannelEncoding::Linear8,
            0,
            255,
            0});
    }
    return profile;
}

[[nodiscard]] std::string make_project_id() {
    static std::atomic<std::uint64_t> sequence{0U};
    const auto timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    return "show-" + std::to_string(timestamp) + "-" +
           std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed));
}

}  // namespace

bool ProjectValidation::ok() const noexcept {
    return error_count() == 0U;
}

std::size_t ProjectValidation::error_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(),
        issues.end(),
        [](const ProjectIssue& issue) {
            return issue.severity == ProjectIssueSeverity::Error;
        }));
}

std::size_t ProjectValidation::warning_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        issues.begin(),
        issues.end(),
        [](const ProjectIssue& issue) {
            return issue.severity == ProjectIssueSeverity::Warning;
        }));
}

ProjectDocument make_starter_project() {
    ProjectDocument project;
    project.id = make_project_id();
    project.name = "New EmberLights Show";
    project.fixture_profiles.push_back(make_profile(
        "builtin.generic.dimmer-1ch",
        "Dimmer",
        "1 channel",
        {showcore::Property::Intensity}));
    project.fixture_profiles.push_back(make_profile(
        "builtin.generic.rgb-3ch",
        "RGB",
        "3 channel",
        {showcore::Property::Red, showcore::Property::Green, showcore::Property::Blue}));
    project.fixture_profiles.push_back(make_profile(
        "builtin.generic.rgbd-4ch",
        "RGB Dimmer",
        "4 channel",
        {showcore::Property::Intensity, showcore::Property::Red,
         showcore::Property::Green, showcore::Property::Blue}));
    project.fixture_profiles.push_back(make_profile(
        "builtin.generic.rgbwd-5ch",
        "RGBW Dimmer",
        "5 channel",
        {showcore::Property::Intensity, showcore::Property::Red,
         showcore::Property::Green, showcore::Property::Blue,
         showcore::Property::White}));
    project.fixture_profiles.push_back(make_profile(
        "builtin.generic.rgbwauvd-7ch",
        "RGBWAUV Dimmer",
        "7 channel",
        {showcore::Property::Intensity, showcore::Property::Red,
         showcore::Property::Green, showcore::Property::Blue,
         showcore::Property::White, showcore::Property::Amber,
         showcore::Property::UV}));
    project.fixture_profiles.push_back(make_both_lighting_bo_ir4_6ch_profile());
    project.fixture_profiles.push_back(make_both_lighting_bo_ir4_10ch_profile());
    return project;
}

LookTargetExpansion expand_look_target(
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property,
    showcore::PropertyValue value,
    std::vector<LookAssignmentDefinition>& assignments) {
    const auto fixture = std::find_if(
        project.fixtures.begin(),
        project.fixtures.end(),
        [&](const auto& candidate) { return candidate.id == target_id; });
    if (fixture != project.fixtures.end()) {
        assignments.push_back({fixture->id, property, value});
        return {true, 1U};
    }
    const auto group = std::find_if(
        project.groups.begin(),
        project.groups.end(),
        [&](const auto& candidate) { return candidate.id == target_id; });
    if (group == project.groups.end()) {
        return {};
    }
    for (const auto& fixture_id : group->fixture_ids) {
        assignments.push_back({fixture_id, property, value});
    }
    return {true, group->fixture_ids.size()};
}

AutoloopPlacementResult move_autoloop(
    ProjectDocument& project,
    std::string_view autoloop_id,
    std::uint16_t target_bank,
    std::uint8_t target_slot,
    bool swap_if_occupied) noexcept {
    if (target_bank >= showcore::kMaxAutoloopBanks ||
        target_slot >= showcore::kAutoloopsPerBank) {
        return AutoloopPlacementResult::InvalidAddress;
    }
    const auto source = std::find_if(
        project.autoloops.begin(), project.autoloops.end(),
        [&](const AutoloopDefinition& loop) { return loop.id == autoloop_id; });
    if (source == project.autoloops.end()) {
        return AutoloopPlacementResult::SourceMissing;
    }
    if (source->bank == target_bank && source->slot == target_slot) {
        return AutoloopPlacementResult::Moved;
    }
    const auto target = std::find_if(
        project.autoloops.begin(), project.autoloops.end(),
        [&](const AutoloopDefinition& loop) {
            return loop.bank == target_bank && loop.slot == target_slot;
        });
    if (target == project.autoloops.end()) {
        source->bank = target_bank;
        source->slot = target_slot;
        return AutoloopPlacementResult::Moved;
    }
    if (!swap_if_occupied) {
        return AutoloopPlacementResult::TargetOccupied;
    }
    std::swap(source->bank, target->bank);
    std::swap(source->slot, target->slot);
    return AutoloopPlacementResult::Swapped;
}

AutoloopPlacementResult move_autoloop_to_next_empty_slot(
    ProjectDocument& project,
    std::string_view autoloop_id) noexcept {
    const auto source = std::find_if(
        project.autoloops.begin(), project.autoloops.end(),
        [&](const AutoloopDefinition& loop) { return loop.id == autoloop_id; });
    if (source == project.autoloops.end()) {
        return AutoloopPlacementResult::SourceMissing;
    }
    const auto source_address = static_cast<std::size_t>(source->bank) *
        showcore::kAutoloopsPerBank + source->slot;
    for (std::size_t offset = 1U; offset < showcore::kMaxAutoloops; ++offset) {
        const auto address = (source_address + offset) % showcore::kMaxAutoloops;
        const auto bank = static_cast<std::uint16_t>(address / showcore::kAutoloopsPerBank);
        const auto slot = static_cast<std::uint8_t>(address % showcore::kAutoloopsPerBank);
        const auto occupied = std::any_of(
            project.autoloops.begin(), project.autoloops.end(),
            [&](const AutoloopDefinition& loop) { return loop.bank == bank && loop.slot == slot; });
        if (!occupied) {
            source->bank = bank;
            source->slot = slot;
            return AutoloopPlacementResult::Moved;
        }
    }
    return AutoloopPlacementResult::LibraryFull;
}

ProjectValidation validate_project(const ProjectDocument& project) {
    ProjectValidation result;
    if (project.format_version != kProjectFormatVersion) {
        add_issue(result, ProjectIssueSeverity::Error, "project.version", project.id,
                  "Unsupported EmberLights project version.");
    }
    if (!valid_identifier(project.id)) {
        add_issue(result, ProjectIssueSeverity::Error, "project.id", project.id,
                  "Project ID is required and must be 96 characters or fewer.");
    }
    if (project.name.empty()) {
        add_issue(result, ProjectIssueSeverity::Error, "project.name", project.id,
                  "Project name is required.");
    }
    if (project.connections.frame_rate < 10U || project.connections.frame_rate > 60U) {
        add_issue(result, ProjectIssueSeverity::Error, "connections.frameRate", project.id,
                  "Frame rate must be between 10 and 60 Hz.");
    }
    if (!std::isfinite(project.connections.manual_bpm) ||
        project.connections.manual_bpm < 20.0 || project.connections.manual_bpm > 300.0) {
        add_issue(result, ProjectIssueSeverity::Error, "connections.manualBpm", project.id,
                  "Manual BPM must be between 20 and 300.");
    }
    std::array<std::uint16_t, showcore::kV1UniverseCount> dmx_usb_port_numbers{};
    for (std::size_t universe = 0;
         universe < project.connections.dmx_usb_pro_ports.size(); ++universe) {
        const auto& port = project.connections.dmx_usb_pro_ports[universe];
        if (!port.empty() && !showcore::parse_windows_com_port(
                port, dmx_usb_port_numbers[universe])) {
            add_issue(
                result,
                ProjectIssueSeverity::Error,
                "connections.dmxUsbProPort",
                project.id,
                "DMX USB Pro ports must use a COM number from COM1 through COM256.");
        }
    }
    if (dmx_usb_port_numbers[0] != 0U &&
        dmx_usb_port_numbers[0] == dmx_usb_port_numbers[1]) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.dmxUsbProDuplicate",
            project.id,
            "One serial DMX interface cannot drive both universes simultaneously.");
    }
    if (project.connections.frame_rate > 40U &&
        std::any_of(
            project.connections.dmx_usb_pro_ports.begin(),
            project.connections.dmx_usb_pro_ports.end(),
            [](const auto& port) { return !port.empty(); })) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.dmxUsbProFrameRate",
            project.id,
            "DMX USB Pro output is limited to a 40 Hz project frame rate.");
    }
    if (project.connections.soundswitch_micro_universe > showcore::kV1UniverseCount) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.soundSwitchMicroUniverse",
            project.id,
            "SoundSwitch Micro must be disabled or assigned to universe 1 or 2.");
    }
    if (showcore::soundswitch_micro_framing_value(
            project.connections.soundswitch_micro_framing) >
        showcore::soundswitch_micro_framing_value(
            showcore::SoundSwitchMicroFraming::NativeJls1)) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.soundSwitchMicroFraming",
            project.id,
            "SoundSwitch Micro framing selection is invalid.");
    }
    if (project.connections.soundswitch_micro_universe != 0U &&
        project.connections.frame_rate > 40U) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.soundSwitchMicroFrameRate",
            project.id,
            "SoundSwitch Micro output is limited to a 40 Hz project frame rate.");
    }
    if (project.connections.soundswitch_control_one_experimental &&
        project.connections.frame_rate > 40U) {
        add_issue(
            result,
            ProjectIssueSeverity::Error,
            "connections.soundSwitchControlOneFrameRate",
            project.id,
            "Experimental SoundSwitch Control One output is limited to a 40 Hz project frame rate.");
    }
    if (project.connections.soundswitch_control_one_experimental) {
        add_issue(
            result,
            ProjectIssueSeverity::Warning,
            "connections.soundSwitchControlOneUnqualified",
            project.id,
            "SoundSwitch Control One DMX is experimental and not physically qualified; jack 1 carries universe 1 and jack 2 carries universe 2.");
    }
    if (!finite_normalized(project.safety.max_strobe) ||
        !finite_normalized(project.safety.max_intensity)) {
        add_issue(result, ProjectIssueSeverity::Error, "safety.range", project.id,
                  "Safety limits must be normalized values from zero through one.");
    }
    if (project.fixture_profiles.size() > showcore::kMaxCompiledFixtureProfiles) {
        add_issue(result, ProjectIssueSeverity::Error, "profiles.capacity", project.id,
                  "The project exceeds the V1 fixture-profile capacity.");
    }
    if (project.fixtures.size() > showcore::kMaxFixtures) {
        add_issue(result, ProjectIssueSeverity::Error, "fixtures.capacity", project.id,
                  "The project exceeds the V1 fixture capacity.");
    }
    if (project.groups.size() > kMaximumFixtureGroups) {
        add_issue(result, ProjectIssueSeverity::Error, "groups.capacity", project.id,
                  "The project exceeds the compiled fixture-group capacity.");
    }
    if (project.looks.size() > kMaximumStaticLooks) {
        add_issue(result, ProjectIssueSeverity::Error, "looks.capacity", project.id,
                  "The project exceeds the compiled 256 Static Look capacity.");
    }

    std::unordered_map<std::string_view, const FixtureProfileDefinition*> profiles;
    std::size_t total_channels = 0;
    for (const auto& profile : project.fixture_profiles) {
        if (!valid_identifier(profile.id) || !profiles.emplace(profile.id, &profile).second) {
            add_issue(result, ProjectIssueSeverity::Error, "profile.id", profile.id,
                      "Fixture-profile IDs must be unique, non-empty, and 96 characters or fewer.");
            continue;
        }
        if (!valid_identifier(profile.manufacturer) ||
            !valid_identifier(profile.model) ||
            !valid_identifier(profile.mode) ||
            !valid_identifier(profile.name) ||
            !valid_identifier(profile.source_revision) ||
            profile.source == showcore::FixtureProfileSource::Unknown) {
            add_issue(result, ProjectIssueSeverity::Error, "profile.metadata", profile.id,
                      "Fixture profile metadata, source, and revision must be present and 96 characters or fewer.");
        }
        total_channels += profile.channels.size();
        std::vector<showcore::ChannelMapping> channels;
        channels.reserve(profile.channels.size());
        for (const auto& channel : profile.channels) {
            channels.push_back({
                channel.property,
                channel.coarse_offset,
                channel.fine_offset,
                channel.encoding,
                channel.dmx_min,
                channel.dmx_max,
                channel.default_value});
        }
        const showcore::FixtureProfile runtime{
            profile.name.c_str(), channels.data(), channels.size(), profile.footprint};
        const auto validation = showcore::validate_fixture_profile(runtime);
        if (!validation) {
            add_issue(result, ProjectIssueSeverity::Error, "profile.invalid", profile.id,
                      "Fixture profile failed channel and footprint validation.");
        }
    }
    if (total_channels > showcore::kMaxCompiledChannelMappings) {
        add_issue(result, ProjectIssueSeverity::Error, "profiles.channelCapacity", project.id,
                  "The project exceeds the compiled channel-mapping capacity.");
    }

    std::unordered_map<std::string_view, std::uint16_t> fixtures;
    std::array<std::array<std::string_view, showcore::kUniverseSlots>,
               showcore::kV1UniverseCount> occupancy{};
    for (std::size_t index = 0; index < project.fixtures.size(); ++index) {
        const auto& fixture = project.fixtures[index];
        if (!valid_identifier(fixture.id) ||
            !fixtures.emplace(fixture.id, static_cast<std::uint16_t>(index)).second) {
            add_issue(result, ProjectIssueSeverity::Error, "fixture.id", fixture.id,
                      "Fixture IDs must be unique, non-empty, and 96 characters or fewer.");
            continue;
        }
        std::unordered_set<std::string_view> roles;
        for (const auto& role : fixture.roles) {
            if (!valid_identifier(role)) {
                add_issue(result, ProjectIssueSeverity::Error, "fixture.role", fixture.id,
                          "Fixture roles must be non-empty and 96 characters or fewer.");
            } else if (!roles.insert(role).second) {
                add_issue(result, ProjectIssueSeverity::Warning, "fixture.roleDuplicate", fixture.id,
                          "Fixture contains the same role more than once.");
            }
        }
        const auto profile = profiles.find(fixture.profile_id);
        if (profile == profiles.end()) {
            add_issue(result, ProjectIssueSeverity::Error, "fixture.profile", fixture.id,
                      "Fixture references a missing profile.");
            continue;
        }
        if (fixture.universe < 1U || fixture.universe > showcore::kV1UniverseCount ||
            fixture.address < 1U ||
            static_cast<std::size_t>(fixture.address - 1U) + profile->second->footprint >
                showcore::kUniverseSlots) {
            add_issue(result, ProjectIssueSeverity::Error, "fixture.patch", fixture.id,
                      "Fixture universe/address and footprint do not fit the two-universe patch.");
            continue;
        }
        auto& universe = occupancy[fixture.universe - 1U];
        const auto start = static_cast<std::size_t>(fixture.address - 1U);
        for (std::size_t offset = 0; offset < profile->second->footprint; ++offset) {
            if (!universe[start + offset].empty()) {
                add_issue(result, ProjectIssueSeverity::Error, "fixture.overlap", fixture.id,
                          "Fixture overlaps another patched fixture.");
                break;
            }
            universe[start + offset] = fixture.id;
        }
    }

    std::unordered_set<std::string_view> group_ids;
    std::unordered_map<std::string_view, const GroupDefinition*> groups_by_id;
    for (const auto& group : project.groups) {
        if (!valid_identifier(group.id) || !group_ids.insert(group.id).second) {
            add_issue(result, ProjectIssueSeverity::Error, "group.id", group.id,
                      "Group IDs must be unique and valid.");
        } else {
            groups_by_id.emplace(group.id, &group);
        }
        std::unordered_set<std::string_view> members;
        for (const auto& fixture_id : group.fixture_ids) {
            if (fixtures.find(fixture_id) == fixtures.end()) {
                add_issue(result, ProjectIssueSeverity::Error, "group.fixture", group.id,
                          "Group references a missing fixture.");
            } else if (!members.insert(fixture_id).second) {
                add_issue(result, ProjectIssueSeverity::Warning, "group.duplicate", group.id,
                          "Duplicate fixture membership will be ignored.");
            }
        }
    }

    std::unordered_set<std::string_view> look_ids;
    std::unordered_set<std::string_view> look_names;
    std::size_t assignment_count = 0;
    for (const auto& look : project.looks) {
        if (!valid_identifier(look.id) || !look_ids.insert(look.id).second) {
            add_issue(result, ProjectIssueSeverity::Error, "look.id", look.id,
                      "Static Look IDs must be unique and valid.");
        }
        if (look.assignments.empty()) {
            add_issue(result, ProjectIssueSeverity::Error, "look.empty", look.id,
                      "A Static Look must contain at least one fixture property.");
        }
        if (look.name.empty() || look.name.size() > kMaximumStaticLookNameLength) {
            add_issue(result, ProjectIssueSeverity::Error, "look.name", look.id,
                      "A Static Look needs a display name of 255 characters or fewer.");
        } else if (!look_names.insert(look.name).second) {
            add_issue(result, ProjectIssueSeverity::Warning, "look.nameDuplicate", look.id,
                      "Static Look names should be unique so name-based control is unambiguous.");
        }
        if (look.fade_ms > kMaximumStaticLookFadeMs) {
            add_issue(result, ProjectIssueSeverity::Error, "look.fade", look.id,
                      "Static Look fade must be between 0 and 30000 milliseconds.");
        }
        assignment_count += look.assignments.size();
        std::unordered_set<std::string> assigned;
        bool owns_a_value = false;
        for (const auto& assignment : look.assignments) {
            const auto fixture = fixtures.find(assignment.fixture_id);
            if (fixture == fixtures.end()) {
                add_issue(result, ProjectIssueSeverity::Error, "look.fixture", look.id,
                          "Static Look references a missing fixture.");
            }
            if (assignment.property >= showcore::Property::Count ||
                (assignment.value.mode == showcore::ValueMode::Set &&
                 !finite_normalized(assignment.value.value))) {
                add_issue(result, ProjectIssueSeverity::Error, "look.value", look.id,
                          "Static Look contains an invalid property value.");
            } else if (fixture != fixtures.end()) {
                const auto& fixture_definition = project.fixtures[fixture->second];
                const auto profile = profiles.find(fixture_definition.profile_id);
                if (profile != profiles.end() &&
                    !fixture_profile_supports_property(
                        *profile->second, assignment.property)) {
                    add_issue(result, ProjectIssueSeverity::Error,
                              "look.unsupportedProperty", look.id,
                              "Static Look assigns " +
                                  std::string(property_name(assignment.property)) +
                                  " to fixture " + assignment.fixture_id +
                                  ", but its profile does not implement that property.");
                }
            }
            owns_a_value = owns_a_value ||
                assignment.value.mode != showcore::ValueMode::Release;
            const auto key = assignment.fixture_id + ":" +
                std::string(property_name(assignment.property));
            if (!assigned.insert(key).second) {
                add_issue(result, ProjectIssueSeverity::Error, "look.duplicate", look.id,
                          "Static Look assigns the same fixture property more than once.");
            }
        }
        if (!look.assignments.empty() && !owns_a_value) {
            add_issue(result, ProjectIssueSeverity::Warning, "look.noOp", look.id,
                      "Every Static Look assignment is RELEASE, so activating it changes no output.");
        }

        // Physical master channels are independent from emitter properties in
        // the semantic renderer. Warn when a look can therefore compile but
        // leave a fixture closed, which is especially important for IR-4 10CH.
        for (const auto& fixture : project.fixtures) {
            const auto profile = profiles.find(fixture.profile_id);
            if (profile == profiles.end() ||
                !fixture_profile_supports_property(
                    *profile->second, showcore::Property::Intensity)) {
                continue;
            }
            const auto emitter_on = std::any_of(
                look.assignments.begin(), look.assignments.end(),
                [&](const auto& assignment) {
                    return assignment.fixture_id == fixture.id &&
                        std::find(kDirectEmitterProperties.begin(),
                                  kDirectEmitterProperties.end(),
                                  assignment.property) != kDirectEmitterProperties.end() &&
                        assignment.value.mode == showcore::ValueMode::Set &&
                        assignment.value.value > 0.0F;
                });
            if (!emitter_on) {
                continue;
            }
            const auto intensity = std::find_if(
                look.assignments.begin(), look.assignments.end(),
                [&](const auto& assignment) {
                    return assignment.fixture_id == fixture.id &&
                        assignment.property == showcore::Property::Intensity;
                });
            if (intensity == look.assignments.end() ||
                intensity->value.mode != showcore::ValueMode::Set ||
                intensity->value.value <= 0.0F) {
                add_issue(result, ProjectIssueSeverity::Warning,
                          "look.masterClosed", look.id,
                          "Fixture " + fixture.id +
                              " has color output but no open master intensity in this Static Look.");
            }
        }
    }
    if (assignment_count > kMaximumStaticLookAssignments) {
        add_issue(result, ProjectIssueSeverity::Error, "looks.capacity", project.id,
                  "The project exceeds the V1 compiled Static Look assignment capacity.");
    }

    std::unordered_set<std::string_view> autoloop_ids;
    std::unordered_set<std::uint32_t> autoloop_slots;
    for (const auto& loop : project.autoloops) {
        if (!valid_identifier(loop.id) || !autoloop_ids.insert(loop.id).second) {
            add_issue(result, ProjectIssueSeverity::Error, "autoloop.id", loop.id,
                      "Autoloop IDs must be unique and valid.");
        }
        const auto slot_key = static_cast<std::uint32_t>(
            static_cast<std::size_t>(loop.bank) * showcore::kAutoloopsPerBank + loop.slot);
        if (loop.bank >= showcore::kMaxAutoloopBanks ||
            loop.slot >= showcore::kAutoloopsPerBank ||
            !autoloop_slots.insert(slot_key).second) {
            add_issue(result, ProjectIssueSeverity::Error, "autoloop.slot", loop.id,
                      "Autoloop bank/slot is outside the 64 by 32 library or already occupied.");
        }
        if (!std::isfinite(loop.length_beats) || loop.length_beats <= 0.0F ||
            loop.steps.empty() || loop.steps.size() > showcore::kMaxAutoloopSteps) {
            add_issue(result, ProjectIssueSeverity::Error, "autoloop.shape", loop.id,
                      "Autoloop length and step count are invalid.");
        }
        float previous = -1.0F;
        for (const auto& step : loop.steps) {
            if (!std::isfinite(step.at_beat) || step.at_beat < 0.0F ||
                step.at_beat >= loop.length_beats || step.at_beat < previous ||
                look_ids.find(step.look_id) == look_ids.end()) {
                add_issue(result, ProjectIssueSeverity::Error, "autoloop.step", loop.id,
                          "Autoloop steps must be ordered, inside the loop, and reference a Static Look.");
                break;
            }
            previous = step.at_beat;
        }
        if (!loop.steps.empty() && loop.steps.front().at_beat != 0.0F) {
            add_issue(result, ProjectIssueSeverity::Error, "autoloop.firstStep", loop.id,
                      "The first Autoloop step must start at beat zero.");
        }
    }

    if (project.audio_assets.size() > kMaximumAudioAssets) {
        add_issue(result, ProjectIssueSeverity::Error, "audio.capacity", project.id,
                  "The project exceeds the supported audio-asset capacity.");
    }
    std::unordered_set<std::string_view> audio_asset_ids;
    for (const auto& asset : project.audio_assets) {
        if (!valid_identifier(asset.id) || !audio_asset_ids.insert(asset.id).second) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.id", asset.id,
                      "Audio asset IDs must be unique, non-empty, and 96 characters or fewer.");
        }
        if (asset.name.empty() || asset.name.size() > 255U) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.name", asset.id,
                      "An audio asset needs a display name of 255 characters or fewer.");
        }
        if (!valid_file_name(asset.file_name)) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.fileName", asset.id,
                      "An audio asset needs a filename, not a directory path.");
        }
        if (!is_sha256_digest(asset.sha256)) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.sha256", asset.id,
                      "An audio asset must have a lowercase 64-character SHA-256 digest.");
        }
        if (asset.size_bytes == 0U) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.size", asset.id,
                      "An audio asset cannot be empty.");
        }
        if (asset.local_path_hint.size() > 4096U) {
            add_issue(result, ProjectIssueSeverity::Error, "audio.pathHint", asset.id,
                      "The audio path hint exceeds the safe project limit.");
        }
    }

    if (project.track_scripts.size() > kMaximumTrackScripts) {
        add_issue(result, ProjectIssueSeverity::Error, "track.capacity", project.id,
                  "The project exceeds the compiled track-script capacity.");
    }
    std::unordered_set<std::string_view> track_ids;
    std::size_t track_cue_count = 0;
    for (const auto& track : project.track_scripts) {
        if (!valid_identifier(track.id) || !track_ids.insert(track.id).second) {
            add_issue(result, ProjectIssueSeverity::Error, "track.id", track.id,
                      "Track-script IDs must be unique and valid.");
        }
        if (track.name.empty()) {
            add_issue(result, ProjectIssueSeverity::Error, "track.name", track.id,
                      "A track script needs a display name.");
        }
        if (!track.audio_key.empty() && !valid_identifier(track.audio_key)) {
            add_issue(result, ProjectIssueSeverity::Error, "track.audioKey", track.id,
                      "A track audio key must be 96 characters or fewer.");
        }
        if (!track.audio_asset_id.empty() &&
            audio_asset_ids.find(track.audio_asset_id) == audio_asset_ids.end()) {
            add_issue(result, ProjectIssueSeverity::Error, "track.audioAsset", track.id,
                      "Track script references a missing audio asset.");
        }
        track_cue_count += track.cues.size();
        float previous = -1.0F;
        for (const auto& cue : track.cues) {
            const bool references_look = cue.action == TrackCueAction::TriggerLook;
            const bool references_autoloop = cue.action == TrackCueAction::TriggerAutoloop;
            const bool clears = cue.action == TrackCueAction::ClearLook ||
                cue.action == TrackCueAction::ClearAutoloop;
            if (!std::isfinite(cue.at_beat) || cue.at_beat < 0.0F ||
                cue.at_beat < previous || cue.action >= TrackCueAction::Count ||
                (references_look && look_ids.find(cue.target_ref) == look_ids.end()) ||
                (references_autoloop && autoloop_ids.find(cue.target_ref) == autoloop_ids.end()) ||
                (clears && !cue.target_ref.empty())) {
                add_issue(result, ProjectIssueSeverity::Error, "track.cue", track.id,
                          "Track cues must be ordered, use supported actions, and reference a valid target.");
                break;
            }
            previous = cue.at_beat;
        }
    }
    if (track_cue_count > kMaximumTrackCues) {
        add_issue(result, ProjectIssueSeverity::Error, "track.cueCapacity", project.id,
                  "The project exceeds the compiled track-cue capacity.");
    }

    for (const auto& mapping : project.midi_mappings) {
        if (mapping.action.type >= showcore::ActionType::Count) {
            add_issue(result, ProjectIssueSeverity::Error, "midi.action", mapping.device_name,
                      "MIDI mapping action is outside the supported range.");
            continue;
        }
        if ((mapping.action.type == showcore::ActionType::SelectAutoloopBank ||
             mapping.action.type == showcore::ActionType::SetAutoloopBankEnabled) &&
            mapping.action.target_id >= showcore::kMaxAutoloopBanks) {
            add_issue(result, ProjectIssueSeverity::Error, "midi.bank", mapping.device_name,
                      "MIDI Autoloop-bank selection must reference a valid bank.");
            continue;
        }
        const bool needs_look = mapping.action.type == showcore::ActionType::TriggerLook;
        const bool needs_autoloop = mapping.action.type == showcore::ActionType::TriggerAutoloop;
        const bool needs_fixture = mapping.action.type == showcore::ActionType::SetProperty;
        const bool needs_group = mapping.action.type == showcore::ActionType::SetGroupProperty ||
            mapping.action.type == showcore::ActionType::BlackoutGroup;
        const bool needs_track = mapping.action.type == showcore::ActionType::TriggerTrackScript;
        if ((needs_look && look_ids.find(mapping.target_ref) == look_ids.end()) ||
            (needs_autoloop && autoloop_ids.find(mapping.target_ref) == autoloop_ids.end()) ||
            (needs_fixture && fixtures.find(mapping.target_ref) == fixtures.end()) ||
            (needs_group &&
             (groups_by_id.find(mapping.target_ref) == groups_by_id.end() ||
              groups_by_id.find(mapping.target_ref)->second->fixture_ids.empty())) ||
            (needs_track && track_ids.find(mapping.target_ref) == track_ids.end())) {
            add_issue(result, ProjectIssueSeverity::Error, "midi.target", mapping.device_name,
                      "MIDI mapping target is missing or no longer exists.");
        }
    }
    if (project.midi_mappings.size() > showcore::kMaxMidiMappings) {
        add_issue(result, ProjectIssueSeverity::Error, "midi.capacity", project.id,
                  "The project exceeds the V1 MIDI mapping capacity.");
    }
    return result;
}

std::string_view property_name(showcore::Property property) noexcept {
    if (property == showcore::Property::Count) {
        return "constant";
    }
    const auto index = static_cast<std::size_t>(property);
    return index < kPropertyNames.size() ? kPropertyNames[index].second : "invalid";
}

bool parse_property(std::string_view text, showcore::Property& property) noexcept {
    if (text == "constant") {
        property = showcore::Property::Count;
        return true;
    }
    const auto found = std::find_if(
        kPropertyNames.begin(),
        kPropertyNames.end(),
        [text](const PropertyName& candidate) { return candidate.second == text; });
    if (found == kPropertyNames.end()) {
        return false;
    }
    property = found->first;
    return true;
}

std::string_view channel_encoding_name(showcore::ChannelEncoding encoding) noexcept {
    switch (encoding) {
    case showcore::ChannelEncoding::Linear8: return "linear8";
    case showcore::ChannelEncoding::Linear16: return "linear16";
    case showcore::ChannelEncoding::Discrete8: return "discrete8";
    case showcore::ChannelEncoding::Ranged8: return "ranged8";
    case showcore::ChannelEncoding::Constant8: return "constant8";
    }
    return "invalid";
}

bool parse_channel_encoding(
    std::string_view text,
    showcore::ChannelEncoding& encoding) noexcept {
    if (text == "linear8") {
        encoding = showcore::ChannelEncoding::Linear8;
    } else if (text == "linear16") {
        encoding = showcore::ChannelEncoding::Linear16;
    } else if (text == "discrete8") {
        encoding = showcore::ChannelEncoding::Discrete8;
    } else if (text == "ranged8") {
        encoding = showcore::ChannelEncoding::Ranged8;
    } else if (text == "constant8") {
        encoding = showcore::ChannelEncoding::Constant8;
    } else {
        return false;
    }
    return true;
}

std::string_view track_cue_action_name(TrackCueAction action) noexcept {
    switch (action) {
    case TrackCueAction::TriggerLook: return "triggerLook";
    case TrackCueAction::ClearLook: return "clearLook";
    case TrackCueAction::TriggerAutoloop: return "triggerAutoloop";
    case TrackCueAction::ClearAutoloop: return "clearAutoloop";
    case TrackCueAction::Count: break;
    }
    return "invalid";
}

bool parse_track_cue_action(std::string_view text, TrackCueAction& action) noexcept {
    if (text == "triggerLook") {
        action = TrackCueAction::TriggerLook;
    } else if (text == "clearLook") {
        action = TrackCueAction::ClearLook;
    } else if (text == "triggerAutoloop") {
        action = TrackCueAction::TriggerAutoloop;
    } else if (text == "clearAutoloop") {
        action = TrackCueAction::ClearAutoloop;
    } else {
        return false;
    }
    return true;
}

}  // namespace emberlights
