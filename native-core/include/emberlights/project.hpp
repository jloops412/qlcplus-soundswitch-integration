#pragma once

#include "showcore/autoloop.hpp"
#include "showcore/fixture.hpp"
#include "showcore/fixture_library.hpp"
#include "showcore/midi.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kProjectFormatVersion = 1;
inline constexpr std::string_view kProjectExtension = ".emberlights";

struct ChannelDefinition {
    showcore::Property property{showcore::Property::Intensity};
    std::uint16_t coarse_offset{0};
    std::int16_t fine_offset{-1};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    std::uint8_t dmx_min{0};
    std::uint8_t dmx_max{255};
    std::uint16_t default_value{0};
};

struct FixtureProfileDefinition {
    std::string id;
    std::string manufacturer;
    std::string model;
    std::string mode;
    std::string name;
    showcore::FixtureProfileSource source{showcore::FixtureProfileSource::Local};
    std::string source_revision{"1"};
    std::uint16_t footprint{0};
    std::vector<ChannelDefinition> channels;
};

struct FixtureDefinition {
    std::string id;
    std::string name;
    std::string profile_id;
    std::uint8_t universe{1};
    std::uint16_t address{1};
    std::vector<std::string> roles;
};

struct GroupDefinition {
    std::string id;
    std::string name;
    std::vector<std::string> fixture_ids;
};

struct LookAssignmentDefinition {
    std::string fixture_id;
    showcore::Property property{showcore::Property::Intensity};
    showcore::PropertyValue value{};
};

struct LookDefinition {
    std::string id;
    std::string name;
    std::uint32_t fade_ms{750};
    std::vector<LookAssignmentDefinition> assignments;
};

struct AutoloopStepDefinition {
    float at_beat{0.0F};
    std::string look_id;
    showcore::AutoloopTransition transition{showcore::AutoloopTransition::Cut};
};

struct AutoloopDefinition {
    std::string id;
    std::string name;
    std::uint16_t bank{0};
    std::uint8_t slot{0};
    float length_beats{4.0F};
    showcore::AutoloopRepeat repeat{showcore::AutoloopRepeat::Infinite};
    std::vector<AutoloopStepDefinition> steps;
};

struct MidiMappingDefinition {
    std::string device_name;
    std::string target_ref;
    std::int32_t preferred_input_index{-1};
    showcore::MidiMessageType message_type{showcore::MidiMessageType::ControlChange};
    std::uint8_t channel{showcore::kAnyMidiChannel};
    std::uint8_t number{0};
    showcore::MidiInputMode input_mode{showcore::MidiInputMode::Absolute7};
    showcore::MappingBehavior behavior{showcore::MappingBehavior::Continuous};
    showcore::ActionDescriptor action{};
    float output_min{0.0F};
    float output_max{1.0F};
    float curve{1.0F};
    bool inverted{false};
    bool soft_takeover{false};
    float takeover_tolerance{0.025F};
};

struct ConnectionSettings {
    bool os2l_enabled{true};
    std::string os2l_bind{"127.0.0.1"};
    std::uint16_t os2l_port{9996};
    bool artnet_enabled{false};
    std::string artnet_destination{"127.0.0.1"};
    std::uint16_t artnet_base{0};
    bool sacn_enabled{false};
    std::string sacn_destination{"127.0.0.1"};
    std::uint16_t sacn_universe_base{1};
    std::uint16_t frame_rate{40};
    double manual_bpm{120.0};
    std::int32_t midi_input_index{-1};
    std::int32_t midi_output_index{-1};
};

struct SafetySettings {
    bool fog_requires_arm{true};
    bool haze_requires_arm{true};
    bool laser_requires_arm{true};
    bool spark_requires_arm{true};
    bool strobe_allowed{true};
    float max_strobe{1.0F};
    float max_intensity{1.0F};
};

struct ProjectDocument {
    std::uint32_t format_version{kProjectFormatVersion};
    std::string id;
    std::string name;
    ConnectionSettings connections{};
    SafetySettings safety{};
    std::vector<FixtureProfileDefinition> fixture_profiles;
    std::vector<FixtureDefinition> fixtures;
    std::vector<GroupDefinition> groups;
    std::vector<LookDefinition> looks;
    std::vector<AutoloopDefinition> autoloops;
    std::vector<MidiMappingDefinition> midi_mappings;
    std::vector<std::string> unknown_records;
};

struct LookTargetExpansion {
    bool target_found{false};
    std::size_t assignments_added{0};
};

enum class ProjectIssueSeverity : std::uint8_t {
    Warning,
    Error
};

struct ProjectIssue {
    ProjectIssueSeverity severity{ProjectIssueSeverity::Error};
    std::string code;
    std::string subject;
    std::string message;
};

struct ProjectValidation {
    std::vector<ProjectIssue> issues;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::size_t error_count() const noexcept;
    [[nodiscard]] std::size_t warning_count() const noexcept;
};

[[nodiscard]] ProjectDocument make_starter_project();
[[nodiscard]] LookTargetExpansion expand_look_target(
    const ProjectDocument& project,
    std::string_view target_id,
    showcore::Property property,
    showcore::PropertyValue value,
    std::vector<LookAssignmentDefinition>& assignments);
[[nodiscard]] ProjectValidation validate_project(const ProjectDocument& project);

[[nodiscard]] std::string_view property_name(showcore::Property property) noexcept;
[[nodiscard]] bool parse_property(std::string_view text, showcore::Property& property) noexcept;
[[nodiscard]] std::string_view channel_encoding_name(showcore::ChannelEncoding encoding) noexcept;
[[nodiscard]] bool parse_channel_encoding(
    std::string_view text,
    showcore::ChannelEncoding& encoding) noexcept;

}  // namespace emberlights
