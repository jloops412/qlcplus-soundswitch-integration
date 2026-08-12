#pragma once

#include "showcore/autoloop.hpp"
#include "showcore/fixture.hpp"
#include "showcore/fixture_library.hpp"
#include "showcore/midi.hpp"
#include "showcore/soundswitch_micro.hpp"

#include "emberlights/studio_color_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kProjectFormatVersion = 1;
inline constexpr std::string_view kProjectExtension = ".emberlights";
inline constexpr std::size_t kMaximumTrackScripts = 1024;
inline constexpr std::size_t kMaximumTrackCues = 32768;
inline constexpr std::size_t kMaximumAudioAssets = 4096;
inline constexpr std::size_t kMaximumFixtureGroups = showcore::kMaxFixtures;
inline constexpr std::size_t kMaximumStaticLooks = 256;
inline constexpr std::size_t kMaximumStaticLookAssignments = 32768;
inline constexpr std::size_t kMaximumStaticLookNameLength = 255;
inline constexpr std::uint32_t kMaximumStaticLookFadeMs = 30000U;

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

// Audio stays external to the project package so users retain control of their
// music library. Identity is content-based; the local path is only a relinkable
// hint and never a substitute for the recorded digest.
struct AudioAssetDefinition {
    std::string id;
    std::string name;
    std::string file_name;
    std::string sha256;
    std::uint64_t size_bytes{0};
    std::string local_path_hint;
};

enum class AutoloopPlacementResult : std::uint8_t {
    Moved,
    Swapped,
    SourceMissing,
    InvalidAddress,
    TargetOccupied,
    LibraryFull
};

// Track scripts deliberately carry semantic references rather than fixture-channel
// data. A cue starts at the supplied beat relative to the script trigger, making
// the same authored show portable across compatible DJ timing sources.
enum class TrackCueAction : std::uint8_t {
    TriggerLook,
    ClearLook,
    TriggerAutoloop,
    ClearAutoloop,
    Count
};

struct TrackCueDefinition {
    float at_beat{0.0F};
    TrackCueAction action{TrackCueAction::TriggerLook};
    std::string target_ref;
};

struct TrackScriptDefinition {
    std::string id;
    std::string name;
    // Optional portable association key. It is intentionally not a local path;
    // content hashing and media relinking will build on this stable hook.
    std::string audio_key;
    std::vector<TrackCueDefinition> cues;
    std::string audio_asset_id;
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
    std::array<std::string, showcore::kV1UniverseCount> dmx_usb_pro_ports{};
    std::uint8_t soundswitch_micro_universe{0};
    showcore::SoundSwitchMicroFraming soundswitch_micro_framing{
        showcore::SoundSwitchMicroFraming::NativeJls1};
    // Explicit, default-off opt-in. The transport is contract-tested but not
    // physically qualified on an owned Control One yet.
    bool soundswitch_control_one_experimental{false};
    std::uint16_t frame_rate{40};
    double manual_bpm{120.0};
    std::int32_t midi_input_index{-1};
    std::int32_t midi_output_index{-1};

    [[nodiscard]] friend bool operator==(
        const ConnectionSettings&,
        const ConnectionSettings&) = default;
};

struct SafetySettings {
    bool fog_requires_arm{true};
    bool haze_requires_arm{true};
    bool laser_requires_arm{true};
    bool spark_requires_arm{true};
    bool strobe_allowed{true};
    float max_strobe{1.0F};
    float max_intensity{1.0F};

    [[nodiscard]] friend bool operator==(
        const SafetySettings&,
        const SafetySettings&) = default;
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
    std::vector<StudioColorPaletteAsset> color_palettes;
    std::vector<LookDefinition> looks;
    std::vector<AutoloopDefinition> autoloops;
    std::vector<AudioAssetDefinition> audio_assets;
    std::vector<TrackScriptDefinition> track_scripts;
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
[[nodiscard]] AutoloopPlacementResult move_autoloop(
    ProjectDocument& project,
    std::string_view autoloop_id,
    std::uint16_t target_bank,
    std::uint8_t target_slot,
    bool swap_if_occupied = false) noexcept;
[[nodiscard]] AutoloopPlacementResult move_autoloop_to_next_empty_slot(
    ProjectDocument& project,
    std::string_view autoloop_id) noexcept;
[[nodiscard]] ProjectValidation validate_project(const ProjectDocument& project);

[[nodiscard]] std::string_view property_name(showcore::Property property) noexcept;
[[nodiscard]] bool parse_property(std::string_view text, showcore::Property& property) noexcept;
[[nodiscard]] std::string_view channel_encoding_name(showcore::ChannelEncoding encoding) noexcept;
[[nodiscard]] bool parse_channel_encoding(
    std::string_view text,
    showcore::ChannelEncoding& encoding) noexcept;
[[nodiscard]] std::string_view track_cue_action_name(TrackCueAction action) noexcept;
[[nodiscard]] bool parse_track_cue_action(
    std::string_view text,
    TrackCueAction& action) noexcept;

}  // namespace emberlights
