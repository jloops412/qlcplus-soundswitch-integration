#include "emberlights/compiler.hpp"
#include "emberlights/audio_assets.hpp"
#include "emberlights/file_identity.hpp"
#include "emberlights/fixture_profile_upgrade.hpp"
#include "emberlights/project.hpp"
#include "emberlights/project_edit_history.hpp"
#include "emberlights/project_io.hpp"
#include "emberlights/qlc_fixture_import.hpp"
#include "emberlights/runner.hpp"
#include "emberlights/soundswitch_import.hpp"
#include "emberlights/soundswitch_source_binding.hpp"
#include "emberlights/soundswitch_v1.hpp"
#include "emberlights/static_look_preview.hpp"
#include "showcore/artnet.hpp"
#include "showcore/autoloop.hpp"
#include "showcore/dmx_usb_pro.hpp"
#include "showcore/engine.hpp"
#include "showcore/fixture.hpp"
#include "showcore/fixture_library.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/midi.hpp"
#include "showcore/number_chars.hpp"
#include "showcore/os2l.hpp"
#include "showcore/os2l_server.hpp"
#include "showcore/output_backend.hpp"
#include "showcore/sacn.hpp"
#include "showcore/soundswitch_micro.hpp"
#include "showcore/spsc_queue.hpp"
#include "showcore/sync_manager.hpp"
#include "showcore/types.hpp"
#include "showcore/winmm_midi.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

std::atomic<std::size_t> g_allocations{0};
int g_failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ << " — " #condition "\n"; \
        ++g_failures; \
    } \
} while (false)

[[nodiscard]] bool nearly_equal(float first, float second, float tolerance = 0.001F) {
    return std::abs(first - second) <= tolerance;
}

#ifdef _WIN32
using TestSocket = SOCKET;
constexpr TestSocket kInvalidTestSocket = INVALID_SOCKET;
void close_test_socket(TestSocket socket) { ::closesocket(socket); }
[[nodiscard]] bool initialize_test_network() {
    WSADATA data{};
    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
void cleanup_test_network() { ::WSACleanup(); }
#else
using TestSocket = int;
constexpr TestSocket kInvalidTestSocket = -1;
void close_test_socket(TestSocket socket) { ::close(socket); }
[[nodiscard]] bool initialize_test_network() { return true; }
void cleanup_test_network() {}
#endif

[[nodiscard]] TestSocket connect_loopback(std::uint16_t port) {
    const auto socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == kInvalidTestSocket) {
        return kInvalidTestSocket;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        ::connect(
            socket,
            reinterpret_cast<const sockaddr*>(&address),
            static_cast<int>(sizeof(address))) != 0) {
        close_test_socket(socket);
        return kInvalidTestSocket;
    }
    return socket;
}

[[nodiscard]] std::uint16_t reserve_loopback_port() {
    const auto listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == kInvalidTestSocket) {
        return 0U;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = 0U;
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        ::bind(
            listener,
            reinterpret_cast<const sockaddr*>(&address),
            static_cast<int>(sizeof(address))) != 0) {
        close_test_socket(listener);
        return 0U;
    }
#ifdef _WIN32
    int address_size = sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    if (::getsockname(
            listener,
            reinterpret_cast<sockaddr*>(&address),
            &address_size) != 0) {
        close_test_socket(listener);
        return 0U;
    }
    const auto port = ntohs(address.sin_port);
    close_test_socket(listener);
    return port;
}

[[nodiscard]] bool send_all(TestSocket socket, std::string_view bytes) {
    std::size_t sent_total = 0;
    while (sent_total < bytes.size()) {
#ifdef _WIN32
        const auto sent = ::send(
            socket,
            bytes.data() + sent_total,
            static_cast<int>(bytes.size() - sent_total),
            0);
#else
        const auto sent = ::send(
            socket,
            bytes.data() + sent_total,
            bytes.size() - sent_total,
            0);
#endif
        if (sent <= 0) {
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

struct Os2lCapture {
    std::size_t beats{0};
    std::size_t buttons{0};
    std::size_t errors{0};
    double last_bpm{0.0};
};

void capture_os2l_event(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view,
    void* context) noexcept {
    auto& capture = *static_cast<Os2lCapture*>(context);
    if (error != showcore::Os2lParseError::None) {
        ++capture.errors;
    } else if (event.kind == showcore::Os2lKind::Beat) {
        ++capture.beats;
        capture.last_bpm = event.beat.bpm;
    } else if (event.kind == showcore::Os2lKind::Button) {
        ++capture.buttons;
    }
}

constexpr std::array<showcore::ChannelMapping, 6> kRgbFixtureChannels{{
    {showcore::Property::Intensity, 0, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Red, 1, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Green, 2, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Blue, 3, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Strobe, 4, -1, showcore::ChannelEncoding::Linear8, 0, 255},
    {showcore::Property::Fog, 5, -1, showcore::ChannelEncoding::Linear8, 0, 255}
}};

constexpr showcore::FixtureProfile kRgbFixture{
    "Test RGB",
    kRgbFixtureChannels.data(),
    kRgbFixtureChannels.size(),
    6};

constexpr std::array<showcore::ChannelMapping, 1> kPan16Channels{{
    {showcore::Property::Pan, 0, 1, showcore::ChannelEncoding::Linear16, 0, 255}
}};

constexpr showcore::FixtureProfile kPan16Fixture{
    "Test Pan16",
    kPan16Channels.data(),
    kPan16Channels.size(),
    2};

constexpr std::array<showcore::ChannelMapping, 7> kGenericFixtureChannels{{
    {showcore::Property::Intensity, 0, -1, showcore::ChannelEncoding::Linear8, 255, 0, 64},
    {showcore::Property::Shutter, 1, -1, showcore::ChannelEncoding::Discrete8, 10, 200, 42},
    {showcore::Property::Pan, 2, 3, showcore::ChannelEncoding::Linear16, 0, 255, 0x8000},
    {showcore::Property::Count, 4, -1, showcore::ChannelEncoding::Constant8, 0, 255, 77},
    {showcore::Property::Laser, 5, -1, showcore::ChannelEncoding::Linear8, 100, 200, 99},
    {showcore::Property::Custom1, 6, -1, showcore::ChannelEncoding::Linear8, 0, 255, 9},
    {showcore::Property::Focus, 7, -1, showcore::ChannelEncoding::Linear8, 0, 255, 128}
}};

constexpr showcore::FixtureProfile kGenericFixture{
    "Generic Profile",
    kGenericFixtureChannels.data(),
    kGenericFixtureChannels.size(),
    8};

void test_layers() {
    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    layers.set(showcore::LayerId::Idle, 1, showcore::Property::Intensity, showcore::PropertyValue::set(0.2F));
    auto resolved = layers.resolve(1, showcore::Property::Intensity);
    CHECK(resolved.owned && nearly_equal(resolved.value, 0.2F));
    CHECK(resolved.source == showcore::LayerId::Idle);

    layers.set(showcore::LayerId::TrackScript, 1, showcore::Property::Intensity, showcore::PropertyValue::set(0.7F));
    layers.set(showcore::LayerId::EventMoment, 1, showcore::Property::Intensity, showcore::PropertyValue::release());
    resolved = layers.resolve(1, showcore::Property::Intensity);
    CHECK(nearly_equal(resolved.value, 0.7F));
    CHECK(resolved.source == showcore::LayerId::TrackScript);

    layers.set(showcore::LayerId::ManualOverride, 1, showcore::Property::Intensity, showcore::PropertyValue::force_zero());
    resolved = layers.resolve(1, showcore::Property::Intensity);
    CHECK(resolved.owned && nearly_equal(resolved.value, 0.0F));
    CHECK(resolved.mode == showcore::ValueMode::ForceZero);
    CHECK(resolved.source == showcore::LayerId::ManualOverride);

    layers.set(showcore::LayerId::Safety, 1, showcore::Property::Intensity, showcore::PropertyValue::set(0.9F));
    resolved = layers.resolve(1, showcore::Property::Intensity);
    CHECK(nearly_equal(resolved.value, 0.9F));
    CHECK(resolved.source == showcore::LayerId::Safety);

    layers.clear_layer(showcore::LayerId::Safety);
    CHECK(layers.resolve(1, showcore::Property::Intensity).mode == showcore::ValueMode::ForceZero);
}

void test_safety() {
    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Fog, showcore::PropertyValue::set(1.0F));
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Strobe, showcore::PropertyValue::set(0.8F));
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Intensity, showcore::PropertyValue::set(0.9F));
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Haze, showcore::PropertyValue::set(1.0F));
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Laser, showcore::PropertyValue::set(1.0F));
    layers.set(showcore::LayerId::TrackScript, 0, showcore::Property::Spark, showcore::PropertyValue::set(1.0F));

    showcore::SafetyPolicy policy;
    policy.fog_armed = false;
    policy.strobe_allowed = true;
    policy.max_strobe = 0.25F;
    policy.max_intensity = 0.6F;

    const auto fog_locked = layers.resolve_safe(0, showcore::Property::Fog, policy);
    CHECK(fog_locked.mode == showcore::ValueMode::ForceZero);
    CHECK(fog_locked.source == showcore::LayerId::Safety);
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Strobe, policy).value, 0.25F));
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Intensity, policy).value, 0.6F));
    CHECK(layers.resolve_safe(0, showcore::Property::Haze, policy).mode == showcore::ValueMode::ForceZero);
    CHECK(layers.resolve_safe(0, showcore::Property::Laser, policy).mode == showcore::ValueMode::ForceZero);
    CHECK(layers.resolve_safe(0, showcore::Property::Spark, policy).mode == showcore::ValueMode::ForceZero);

    policy.fog_armed = true;
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Fog, policy).value, 1.0F));
    policy.haze_armed = true;
    policy.laser_armed = true;
    policy.spark_armed = true;
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Haze, policy).value, 1.0F));
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Laser, policy).value, 1.0F));
    CHECK(nearly_equal(layers.resolve_safe(0, showcore::Property::Spark, policy).value, 1.0F));
    policy.strobe_allowed = false;
    CHECK(layers.resolve_safe(0, showcore::Property::Strobe, policy).mode == showcore::ValueMode::ForceZero);
}

void test_fixture_profile_validation() {
    CHECK(showcore::validate_fixture_profile(kGenericFixture));

    auto profile = kGenericFixture;
    profile.name = "";
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::MissingName);

    profile = kGenericFixture;
    profile.channels = nullptr;
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::MissingChannels);

    profile = kGenericFixture;
    profile.footprint = 0;
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::InvalidFootprint);

    auto mappings = kGenericFixtureChannels;
    mappings[0].property = showcore::Property::Count;
    profile = {"Invalid property", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::InvalidProperty);

    mappings = kGenericFixtureChannels;
    mappings[3].property = showcore::Property::Intensity;
    profile = {"Constant property", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::ConstantHasProperty);

    mappings = kGenericFixtureChannels;
    mappings[0].coarse_offset = 8;
    profile = {"Outside", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::OffsetOutsideFootprint);

    mappings = kGenericFixtureChannels;
    mappings[2].fine_offset = -1;
    profile = {"No fine", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::FineOffsetRequired);

    mappings = kGenericFixtureChannels;
    mappings[1].fine_offset = 7;
    profile = {"Unexpected fine", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::FineOffsetNotAllowed);

    mappings = kGenericFixtureChannels;
    mappings[1].coarse_offset = 0;
    profile = {"Collision", mappings.data(), mappings.size(), 8};
    const auto collision = showcore::validate_fixture_profile(profile);
    CHECK(collision.error == showcore::ProfileError::DuplicateOffset);
    CHECK(collision.mapping_index == 1U && collision.conflicting_mapping_index == 0U);

    mappings = kGenericFixtureChannels;
    mappings[0].default_value = 256;
    profile = {"Default range", mappings.data(), mappings.size(), 8};
    CHECK(showcore::validate_fixture_profile(profile).error == showcore::ProfileError::DefaultOutOfRange);

    showcore::Patch patch;
    const auto rejected = patch.add({0, 0, 1, &profile});
    CHECK(rejected.error == showcore::PatchError::InvalidProfile);
    CHECK(rejected.profile_error == showcore::ProfileError::DefaultOutOfRange);
    CHECK(rejected.profile_mapping_index == 0U);
}

void test_generic_profile_rendering() {
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &kGenericFixture}));

    engine->tick();
    CHECK(engine->frames().universes[0][0] == 64U);
    CHECK(engine->frames().universes[0][1] == 42U);
    CHECK(engine->frames().universes[0][2] == 0x80U);
    CHECK(engine->frames().universes[0][3] == 0x00U);
    CHECK(engine->frames().universes[0][4] == 77U);
    CHECK(engine->frames().universes[0][5] == 0U);
    CHECK(engine->frames().universes[0][6] == 9U);
    CHECK(engine->frames().universes[0][7] == 128U);
    const auto& defaults = engine->frame_attribution().universes[0];
    CHECK(defaults[0].fixture_id == 0U);
    CHECK(defaults[0].mapping_index == 0U);
    CHECK(defaults[0].property == showcore::Property::Intensity);
    CHECK(defaults[0].origin == showcore::RenderValueOrigin::Default);
    CHECK(defaults[0].value_mode == showcore::ValueMode::Release);
    CHECK(defaults[0].winning_layer == showcore::LayerId::Count);
    CHECK(defaults[2].encoding == showcore::ChannelEncoding::Linear16);
    CHECK(!defaults[2].fine_channel);
    CHECK(defaults[3].fine_channel);
    CHECK(defaults[4].origin == showcore::RenderValueOrigin::Constant);
    CHECK(defaults[4].property == showcore::Property::Count);

    engine->layers().set(showcore::LayerId::TrackScript, 0,
        showcore::Property::Intensity, showcore::PropertyValue::set(0.0F));
    engine->layers().set(showcore::LayerId::TrackScript, 0,
        showcore::Property::Shutter, showcore::PropertyValue::set(0.5F));
    engine->layers().set(showcore::LayerId::TrackScript, 0,
        showcore::Property::Pan, showcore::PropertyValue::set(0.5F));
    engine->layers().set(showcore::LayerId::TrackScript, 0,
        showcore::Property::Laser, showcore::PropertyValue::set(0.0F));
    engine->safety().laser_armed = true;
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 255U);
    CHECK(engine->frames().universes[0][1] == 105U);
    CHECK(engine->frames().universes[0][2] == 0x80U);
    CHECK(engine->frames().universes[0][3] == 0x00U);
    CHECK(engine->frames().universes[0][5] == 100U);
    const auto& resolved = engine->frame_attribution().universes[0];
    CHECK(resolved[0].origin == showcore::RenderValueOrigin::Property);
    CHECK(resolved[0].value_mode == showcore::ValueMode::Set);
    CHECK(resolved[0].winning_layer == showcore::LayerId::TrackScript);
    CHECK(resolved[0].property == showcore::Property::Intensity);
    CHECK(resolved[2].origin == showcore::RenderValueOrigin::Property);
    CHECK(resolved[3].origin == showcore::RenderValueOrigin::Property);
    CHECK(resolved[3].fine_channel);

    engine->layers().set(showcore::LayerId::Emergency, 0,
        showcore::Property::Intensity, showcore::PropertyValue::force_zero());
    engine->layers().set(showcore::LayerId::Emergency, 0,
        showcore::Property::Shutter, showcore::PropertyValue::force_zero());
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 0U);
    CHECK(engine->frames().universes[0][1] == 0U);
    const auto& forced = engine->frame_attribution().universes[0];
    CHECK(forced[0].origin == showcore::RenderValueOrigin::Property);
    CHECK(forced[0].value_mode == showcore::ValueMode::ForceZero);
    CHECK(forced[0].winning_layer == showcore::LayerId::Emergency);
}

void test_ranged_channel_rendering() {
    constexpr std::array<showcore::ChannelMapping, 1> channels{{
        {showcore::Property::Strobe, 0, -1, showcore::ChannelEncoding::Ranged8,
         16, 127, 8}
    }};
    const showcore::FixtureProfile profile{
        "Ranged strobe", channels.data(), channels.size(), 1};
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &profile}));

    engine->tick();
    CHECK(engine->frames().universes[0][0] == 8U);
    engine->layers().set(showcore::LayerId::ManualOverride, 0,
        showcore::Property::Strobe, showcore::PropertyValue::set(0.0F));
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 8U);
    engine->layers().set(showcore::LayerId::ManualOverride, 0,
        showcore::Property::Strobe, showcore::PropertyValue::set(0.5F));
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 72U);
    engine->safety().strobe_allowed = false;
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 0U);
    const auto& safety = engine->frame_attribution().universes[0][0];
    CHECK(safety.origin == showcore::RenderValueOrigin::Safety);
    CHECK(safety.property == showcore::Property::Strobe);
    CHECK(safety.value_mode == showcore::ValueMode::ForceZero);
    CHECK(safety.winning_layer == showcore::LayerId::Safety);
}

void test_capability_frame_attribution() {
    const std::array<showcore::ChannelCapabilityMapping, 3U> capabilities{{
        {showcore::Property::Red, 0U, 63U, 32U,
         showcore::ChannelCapabilityBehavior::Continuous,
         showcore::ChannelCapabilityAccess::Selectable, false},
        {showcore::Property::Green, 64U, 127U, 96U,
         showcore::ChannelCapabilityBehavior::Continuous,
         showcore::ChannelCapabilityAccess::Selectable, false},
        {showcore::Property::Laser, 128U, 255U, 192U,
         showcore::ChannelCapabilityBehavior::Continuous,
         showcore::ChannelCapabilityAccess::SafetyGated, false}
    }};
    const std::array<showcore::ChannelMapping, 1U> channels{{
        {showcore::Property::Count, 0U, -1,
         showcore::ChannelEncoding::Discrete8, 0U, 255U, 7U, 0U, 255U,
         capabilities.data(), capabilities.size()}
    }};
    const showcore::FixtureProfile profile{
        "Capability attribution", channels.data(), channels.size(), 1U};
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({7U, 0U, 10U, &profile}));

    engine->tick();
    auto evidence = engine->frame_attribution().universes[0][9];
    CHECK(engine->frames().universes[0][9] == 7U);
    CHECK(evidence.fixture_id == 7U);
    CHECK(evidence.origin == showcore::RenderValueOrigin::Default);

    engine->layers().set(showcore::LayerId::ManualOverride, 7U,
        showcore::Property::Red, showcore::PropertyValue::set(0.5F));
    engine->tick();
    evidence = engine->frame_attribution().universes[0][9];
    CHECK(evidence.origin == showcore::RenderValueOrigin::Capability);
    CHECK(evidence.property == showcore::Property::Red);
    CHECK(evidence.capability_index == 0U);
    CHECK(evidence.winning_layer == showcore::LayerId::ManualOverride);

    engine->layers().set(showcore::LayerId::ManualOverride, 7U,
        showcore::Property::Green, showcore::PropertyValue::set(0.5F));
    engine->tick();
    evidence = engine->frame_attribution().universes[0][9];
    CHECK(engine->frames().universes[0][9] == 0U);
    CHECK(evidence.origin == showcore::RenderValueOrigin::Conflict);
    CHECK(evidence.winning_layer == showcore::LayerId::ManualOverride);

    engine->layers().set(showcore::LayerId::ManualOverride, 7U,
        showcore::Property::Red, showcore::PropertyValue::release());
    engine->layers().set(showcore::LayerId::ManualOverride, 7U,
        showcore::Property::Green, showcore::PropertyValue::release());
    engine->layers().set(showcore::LayerId::ManualOverride, 7U,
        showcore::Property::Laser, showcore::PropertyValue::set(1.0F));
    engine->tick();
    evidence = engine->frame_attribution().universes[0][9];
    CHECK(engine->frames().universes[0][9] == 0U);
    CHECK(evidence.origin == showcore::RenderValueOrigin::Safety);
    CHECK(evidence.property == showcore::Property::Laser);
    CHECK(evidence.value_mode == showcore::ValueMode::ForceZero);
    CHECK(evidence.winning_layer == showcore::LayerId::Safety);
}

void test_qlc_fixture_import() {
    constexpr std::string_view qxf = R"qxf(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE FixtureDefinition>
<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition">
 <Creator><Name>Q Light Controller Plus</Name><Version>5.2.2</Version><Author>Test</Author></Creator>
 <Manufacturer>Example &amp; Co</Manufacturer>
 <Model>Party Wash</Model>
 <Type>Effect</Type>
 <Channel Name="Dimmer" Preset="IntensityMasterDimmer" Default="1"/>
 <Channel Name="Dimmer Fine" Preset="IntensityMasterDimmerFine" Default="2"/>
 <Channel Name="Shutter">
  <Group Byte="0">Shutter</Group>
  <Capability Min="0" Max="7" Preset="ShutterClose">Closed</Capability>
  <Capability Min="8" Max="15" Preset="ShutterOpen">Open</Capability>
  <Capability Min="16" Max="127" Preset="StrobeSlowToFast">Strobe</Capability>
  <Capability Min="128" Max="255" Preset="StrobeRandomSlowToFast">Random</Capability>
 </Channel>
 <Channel Name="Haze Output"><Group Byte="0">Intensity</Group><Capability Min="0" Max="255">Output</Capability></Channel>
 <Channel Name="Fan Speed"><Group Byte="0">Intensity</Group><Capability Min="0" Max="255">Speed</Capability></Channel>
 <Channel Name="Red Emitter"><Group Byte="0">Intensity</Group><Colour>Red</Colour></Channel>
 <Channel Name="Reverse Zoom" Preset="BeamZoomBigSmall"/>
 <Channel Name="Mystery"><Group Byte="0">Maintenance</Group></Channel>
 <Channel Name="Switcher"><Group Byte="0">Effect</Group><Capability Min="0" Max="255">Program<Alias Mode="Switched" Channel="Mystery" With="Haze Output"/></Capability></Channel>
 <Mode Name="Safe 5ch">
  <Channel Number="0">Dimmer</Channel><Channel Number="1">Dimmer Fine</Channel>
  <Channel Number="2">Shutter</Channel><Channel Number="3">Haze Output</Channel>
  <Channel Number="4">Fan Speed</Channel><Channel Number="5">Red Emitter</Channel>
  <Channel Number="6">Reverse Zoom</Channel><Channel Number="7">Mystery</Channel>
 </Mode>
 <Mode Name="Switched"><Channel Number="0">Switcher</Channel></Mode>
</FixtureDefinition>)qxf";

    const auto imported = emberlights::import_qlc_fixture(qxf, "test.qxf");
    CHECK(imported);
    CHECK(imported.manufacturer == "Example & Co");
    CHECK(imported.model == "Party Wash");
    CHECK(imported.profiles.size() == 1U);
    CHECK(imported.error_count() >= 1U);
    CHECK(imported.warning_count() >= 2U);
    CHECK(imported.source_revision ==
        std::string(emberlights::kQlcFixtureAdapterVersion) +
            "#sha256:" + emberlights::sha256_text(qxf));
    CHECK(imported.source_revision.size() <= showcore::kFixtureProfileTextLength);
    const auto& profile = imported.profiles[0];
    CHECK(profile.source == showcore::FixtureProfileSource::QlcPlus);
    CHECK(profile.mode == "Safe 5ch");
    CHECK(profile.footprint == 8U);
    CHECK(profile.channels.size() == 7U);

    const auto dimmer = std::find_if(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Intensity; });
    CHECK(dimmer != profile.channels.end());
    if (dimmer != profile.channels.end()) {
        CHECK(dimmer->encoding == showcore::ChannelEncoding::Linear16);
        CHECK(dimmer->coarse_offset == 0U && dimmer->fine_offset == 1);
        CHECK(dimmer->default_value == 0x0102U);
    }
    const auto shutter_strobe = std::find_if(
        profile.channels.begin(), profile.channels.end(), [](const auto& channel) {
            return std::any_of(
                channel.capabilities.begin(),
                channel.capabilities.end(),
                [](const auto& capability) {
                    return capability.property == showcore::Property::Strobe;
                });
        });
    CHECK(shutter_strobe != profile.channels.end());
    if (shutter_strobe != profile.channels.end()) {
        CHECK(shutter_strobe->property == showcore::Property::Count);
        CHECK(shutter_strobe->capabilities.size() == 4U);
        CHECK(shutter_strobe->default_value == 8U);
        const auto strobe_range = std::find_if(
            shutter_strobe->capabilities.begin(),
            shutter_strobe->capabilities.end(),
            [](const auto& capability) {
                return capability.property == showcore::Property::Strobe;
            });
        CHECK(strobe_range != shutter_strobe->capabilities.end());
        if (strobe_range != shutter_strobe->capabilities.end()) {
            CHECK(strobe_range->dmx_min == 16U);
            CHECK(strobe_range->dmx_max == 127U);
            CHECK(strobe_range->access ==
                  showcore::ChannelCapabilityAccess::SafetyGated);
        }
    }
    CHECK(std::any_of(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Haze; }));
    CHECK(std::any_of(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Fan; }));
    CHECK(std::any_of(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Red; }));
    const auto zoom = std::find_if(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Zoom; });
    CHECK(zoom != profile.channels.end());
    if (zoom != profile.channels.end()) {
        CHECK(zoom->dmx_min == 255U && zoom->dmx_max == 0U);
    }
    CHECK(std::any_of(profile.channels.begin(), profile.channels.end(),
        [](const auto& channel) { return channel.property == showcore::Property::Custom1; }));

    std::vector<std::vector<showcore::ChannelCapabilityMapping>> capability_storage(
        profile.channels.size());
    std::vector<showcore::ChannelMapping> mappings;
    for (std::size_t channel_index = 0U;
         channel_index < profile.channels.size();
         ++channel_index) {
        const auto& channel = profile.channels[channel_index];
        auto& capabilities = capability_storage[channel_index];
        for (const auto& capability : channel.capabilities) {
            capabilities.push_back({
                capability.property,
                capability.dmx_min,
                capability.dmx_max,
                capability.preferred_value,
                capability.behavior,
                capability.access,
                capability.reversed});
        }
        mappings.push_back({channel.property, channel.coarse_offset, channel.fine_offset,
            channel.encoding, channel.dmx_min, channel.dmx_max, channel.default_value,
            channel.blackout_value, channel.highlight_value,
            capabilities.empty() ? nullptr : capabilities.data(),
            capabilities.size()});
    }
    const showcore::FixtureProfile runtime{
        profile.name.c_str(), mappings.data(), mappings.size(), profile.footprint};
    CHECK(showcore::validate_fixture_profile(runtime));
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &runtime}));
    engine->layers().set(showcore::LayerId::ManualOverride, 0,
        showcore::Property::Strobe, showcore::PropertyValue::set(0.0F));
    engine->layers().set(showcore::LayerId::ManualOverride, 0,
        showcore::Property::Haze, showcore::PropertyValue::set(1.0F));
    engine->tick();
    CHECK(engine->frames().universes[0][2] == 8U);
    CHECK(engine->frames().universes[0][3] == 0U);
    engine->safety().haze_armed = true;
    engine->tick();
    CHECK(engine->frames().universes[0][3] == 255U);

    auto project = emberlights::make_starter_project();
    project.fixture_profiles.push_back(profile);
    const auto serialized = emberlights::serialize_project(project);
    emberlights::ProjectDocument parsed;
    CHECK(emberlights::parse_project(serialized, parsed));
    CHECK(parsed.fixture_profiles.back().source == showcore::FixtureProfileSource::QlcPlus);
    CHECK(std::any_of(parsed.fixture_profiles.back().channels.begin(),
        parsed.fixture_profiles.back().channels.end(), [](const auto& channel) {
            return !channel.capabilities.empty();
        }));

    const auto external_entity = emberlights::import_qlc_fixture(
        "<!DOCTYPE FixtureDefinition SYSTEM \"file:///secret\"><FixtureDefinition/>",
        "unsafe.qxf");
    CHECK(!external_entity);
    CHECK(external_entity.error_count() == 1U);

    auto ofl_qxf = std::string(qxf);
    const auto creator = ofl_qxf.find("Q Light Controller Plus");
    CHECK(creator != std::string::npos);
    if (creator != std::string::npos) {
        ofl_qxf.replace(creator, std::string_view("Q Light Controller Plus").size(),
                        "OFL - https://open-fixture-library.org/example");
    }
    const auto ofl_import = emberlights::import_qlc_fixture(ofl_qxf, "ofl.qxf");
    CHECK(ofl_import);
    CHECK(ofl_import.source == showcore::FixtureProfileSource::OpenFixtureLibrary);
    CHECK(ofl_import.profiles[0].source == showcore::FixtureProfileSource::OpenFixtureLibrary);

    const auto qxf_path = std::filesystem::temp_directory_path() /
        "emberlights-qlc-import-test.qxf";
    std::error_code qxf_file_error;
    std::filesystem::remove(qxf_path, qxf_file_error);
    {
        std::ofstream qxf_file(qxf_path, std::ios::binary | std::ios::trunc);
        CHECK(qxf_file.good());
        qxf_file.write(qxf.data(), static_cast<std::streamsize>(qxf.size()));
        CHECK(qxf_file.good());
    }
    const auto loaded_import = emberlights::load_qlc_fixture(qxf_path);
    CHECK(loaded_import);
    CHECK(loaded_import.profiles.size() == imported.profiles.size());
    std::filesystem::remove(qxf_path, qxf_file_error);

    constexpr std::string_view laser_qxf = R"qxf(
<FixtureDefinition xmlns="http://www.qlcplus.org/FixtureDefinition">
 <Creator><Name>Q Light Controller Plus</Name><Version>5.2.2</Version><Author>Test</Author></Creator>
 <Manufacturer>SafeCo</Manufacturer><Model>Beam</Model><Type>Laser</Type>
 <Channel Name="Laser Shutter"><Group Byte="0">Shutter</Group>
  <Capability Min="0" Max="7" Preset="ShutterClose">Closed</Capability>
  <Capability Min="8" Max="15" Preset="ShutterOpen">Open</Capability>
 </Channel>
 <Mode Name="1ch"><Channel Number="0">Laser Shutter</Channel></Mode>
</FixtureDefinition>)qxf";
    const auto laser_import = emberlights::import_qlc_fixture(laser_qxf, "laser.qxf");
    CHECK(laser_import);
    CHECK(laser_import.profiles.size() == 1U);
    const auto& laser_profile = laser_import.profiles[0];
    CHECK(laser_profile.channels.size() == 1U);
    CHECK(laser_profile.channels[0].property == showcore::Property::Laser);
    CHECK(laser_profile.channels[0].encoding == showcore::ChannelEncoding::Ranged8);
    CHECK(laser_profile.channels[0].dmx_min == 8U);
    CHECK(laser_profile.channels[0].dmx_max == 15U);

    std::vector<showcore::ChannelCapabilityMapping> laser_capabilities;
    for (const auto& capability : laser_profile.channels[0].capabilities) {
        laser_capabilities.push_back({
            capability.property,
            capability.dmx_min,
            capability.dmx_max,
            capability.preferred_value,
            capability.behavior,
            capability.access,
            capability.reversed});
    }
    const showcore::ChannelMapping laser_mapping{
        laser_profile.channels[0].property,
        laser_profile.channels[0].coarse_offset,
        laser_profile.channels[0].fine_offset,
        laser_profile.channels[0].encoding,
        laser_profile.channels[0].dmx_min,
        laser_profile.channels[0].dmx_max,
        laser_profile.channels[0].default_value,
        laser_profile.channels[0].blackout_value,
        laser_profile.channels[0].highlight_value,
        laser_capabilities.data(),
        laser_capabilities.size()};
    const showcore::FixtureProfile laser_runtime{
        laser_profile.name.c_str(), &laser_mapping, 1U, laser_profile.footprint};
    auto laser_engine = std::make_unique<showcore::Engine>();
    CHECK(laser_engine->patch().add({0, 0, 1, &laser_runtime}));
    laser_engine->layers().set(showcore::LayerId::ManualOverride, 0,
        showcore::Property::Laser, showcore::PropertyValue::set(1.0F));
    laser_engine->tick();
    CHECK(laser_engine->frames().universes[0][0] == 3U);
    laser_engine->safety().laser_armed = true;
    laser_engine->tick();
    CHECK(laser_engine->frames().universes[0][0] == 11U);
}

void test_compiled_fixture_library() {
    auto library_storage = std::make_unique<showcore::CompiledFixtureLibrary>();
    auto& library = *library_storage;
    const showcore::FixtureProfileDraft draft{
        "testco/rgb/basic",
        "TestCo",
        "RGB Fixture",
        "Basic 6ch",
        "TestCo RGB Fixture — Basic 6ch",
        showcore::FixtureProfileSource::OpenFixtureLibrary,
        "ofl-schema-12.4.0@fixture-revision-1",
        kRgbFixtureChannels.data(),
        kRgbFixtureChannels.size(),
        6};
    const auto inserted = library.ingest(draft);
    CHECK(inserted && inserted.profile_index == 0U);
    CHECK(library.size() == 1U);
    CHECK(library.channel_mapping_count() == kRgbFixtureChannels.size());

    const auto* compiled = library.find("testco/rgb/basic");
    CHECK(compiled != nullptr);
    CHECK(std::string_view(compiled->manufacturer.data()) == "TestCo");
    CHECK(compiled->source == showcore::FixtureProfileSource::OpenFixtureLibrary);
    CHECK(compiled->has_hazardous_channels);
    CHECK(showcore::validate_fixture_profile(compiled->runtime));

    CHECK(library.ingest(draft).error == showcore::FixtureIngestError::DuplicateStableId);
    CHECK(library.size() == 1U);

    auto bad_channels = kRgbFixtureChannels;
    bad_channels[1] = bad_channels[0];
    auto invalid = draft;
    invalid.stable_id = "testco/rgb/bad";
    invalid.channels = bad_channels.data();
    const auto quarantined = library.ingest(invalid);
    CHECK(quarantined.error == showcore::FixtureIngestError::InvalidProfile);
    CHECK(quarantined.profile_result.error == showcore::ProfileError::DuplicateOffset);
    CHECK(library.size() == 1U);

    auto second = draft;
    second.stable_id = "testco/rgb/extended";
    second.mode = "Extended 6ch";
    second.display_name = "TestCo RGB Fixture — Extended 6ch";
    CHECK(library.ingest(second));
    CHECK(library.size() == 2U);

    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &compiled->runtime}));
    engine->layers().set(showcore::LayerId::Autonomous, 0, showcore::Property::Intensity,
        showcore::PropertyValue::set(1.0F));
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 255U);
}

void test_patch_and_render() {
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &kRgbFixture}));
    CHECK(engine->patch().add({1, 1, 507, &kRgbFixture}));

    const auto overlap = engine->patch().add({2, 0, 2, &kRgbFixture});
    CHECK(!overlap && overlap.error == showcore::PatchError::AddressOverlap);
    CHECK(overlap.conflicting_fixture_id == 0);
    CHECK(engine->patch().add({0, 0, 20, &kRgbFixture}).error == showcore::PatchError::DuplicateFixtureId);
    CHECK(engine->patch().add({3, 2, 1, &kRgbFixture}).error == showcore::PatchError::InvalidUniverse);
    CHECK(engine->patch().add({3, 0, 510, &kRgbFixture}).error == showcore::PatchError::AddressOverflow);

    for (const auto fixture_id : std::array<std::uint16_t, 2>{0U, 1U}) {
        engine->layers().set(showcore::LayerId::Autonomous, fixture_id, showcore::Property::Intensity, showcore::PropertyValue::set(0.5F));
        engine->layers().set(showcore::LayerId::Autonomous, fixture_id, showcore::Property::Red, showcore::PropertyValue::set(1.0F));
        engine->layers().set(showcore::LayerId::Autonomous, fixture_id, showcore::Property::Green, showcore::PropertyValue::set(0.25F));
        engine->layers().set(showcore::LayerId::Autonomous, fixture_id, showcore::Property::Blue, showcore::PropertyValue::set(0.0F));
        engine->layers().set(showcore::LayerId::Autonomous, fixture_id, showcore::Property::Fog, showcore::PropertyValue::set(1.0F));
    }
    engine->tick();
    CHECK(engine->frames().universes[0][0] == 128U);
    CHECK(engine->frames().universes[0][1] == 255U);
    CHECK(engine->frames().universes[0][2] == 64U);
    CHECK(engine->frames().universes[0][3] == 0U);
    CHECK(engine->frames().universes[0][5] == 0U);
    CHECK(engine->frames().universes[1][506] == 128U);

    engine->safety().fog_armed = true;
    engine->tick();
    CHECK(engine->frames().universes[0][5] == 255U);
}

void test_16bit_render() {
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 100, &kPan16Fixture}));
    engine->layers().set(showcore::LayerId::TrackScript, 0, showcore::Property::Pan, showcore::PropertyValue::set(0.5F));
    engine->tick();
    CHECK(engine->frames().universes[0][99] == 0x80U);
    CHECK(engine->frames().universes[0][100] == 0x00U);
}

void test_artnet() {
    showcore::DmxUniverse universe{};
    universe[0] = 0x12U;
    universe[511] = 0xABU;
    const auto packet = showcore::build_artdmx(universe, 0x1234U, 7U);
    CHECK(packet.length == 530U);
    CHECK(std::string_view(reinterpret_cast<const char*>(packet.bytes.data()), 8) == std::string_view("Art-Net\0", 8));
    CHECK(packet.bytes[8] == 0x00U && packet.bytes[9] == 0x50U);
    CHECK(packet.bytes[10] == 0x00U && packet.bytes[11] == 14U);
    CHECK(packet.bytes[12] == 7U);
    CHECK(packet.bytes[14] == 0x34U && packet.bytes[15] == 0x12U);
    CHECK(packet.bytes[16] == 0x02U && packet.bytes[17] == 0x00U);
    CHECK(packet.bytes[18] == 0x12U && packet.bytes[529] == 0xABU);

    const auto odd = showcore::build_artdmx(universe, 0, 0, 3);
    CHECK(odd.length == showcore::kArtDmxHeaderSize + 4U);

    const auto receiver = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(receiver != kInvalidTestSocket);
    if (receiver != kInvalidTestSocket) {
        sockaddr_in receiver_address{};
        receiver_address.sin_family = AF_INET;
        receiver_address.sin_port = 0;
        CHECK(::inet_pton(AF_INET, "127.0.0.1", &receiver_address.sin_addr) == 1);
        CHECK(::bind(
            receiver,
            reinterpret_cast<const sockaddr*>(&receiver_address),
            static_cast<int>(sizeof(receiver_address))) == 0);
#ifdef _WIN32
        int receiver_address_size = sizeof(receiver_address);
#else
        socklen_t receiver_address_size = sizeof(receiver_address);
#endif
        CHECK(::getsockname(
            receiver,
            reinterpret_cast<sockaddr*>(&receiver_address),
            &receiver_address_size) == 0);

        showcore::ArtNetSender sender;
        CHECK(sender.open_ipv4("127.0.0.1", ntohs(receiver_address.sin_port)));
        CHECK(sender.send(packet));

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(receiver, &read_set);
        timeval timeout{1, 0};
#ifdef _WIN32
        const auto selected = ::select(0, &read_set, nullptr, nullptr, &timeout);
#else
        const auto selected = ::select(receiver + 1, &read_set, nullptr, nullptr, &timeout);
#endif
        CHECK(selected == 1);
        if (selected == 1) {
            std::array<std::uint8_t, showcore::kMaxArtDmxPacketSize> received{};
#ifdef _WIN32
            const auto received_size = ::recv(
                receiver,
                reinterpret_cast<char*>(received.data()),
                static_cast<int>(received.size()),
                0);
#else
            const auto received_size = ::recv(
                receiver,
                received.data(),
                received.size(),
                0);
#endif
            CHECK(received_size >= 0 &&
                static_cast<std::size_t>(received_size) == packet.length);
            if (received_size >= 0 &&
                static_cast<std::size_t>(received_size) == packet.length) {
                CHECK(std::equal(
                    packet.bytes.begin(),
                    packet.bytes.begin() + static_cast<std::ptrdiff_t>(packet.length),
                    received.begin()));
            }
        }
        sender.close();
        CHECK(!sender.is_open());
        close_test_socket(receiver);
    }
}

void test_output_backend_contract_and_health() {
    const auto& micro = showcore::output_backend_descriptor(
        showcore::OutputBackendKind::SoundSwitchMicro);
    CHECK(micro.implementation == showcore::OutputImplementationStage::Implemented);
    CHECK(micro.evidence == showcore::OutputEvidenceStage::HostAccepted);
    CHECK(micro.emberlights_supported_universes == 1U);
    CHECK(showcore::output_backend_is_configurable(micro));
    CHECK(showcore::has_output_capability(
        micro.capabilities, showcore::OutputCapability::HotReconnect));
    CHECK(showcore::has_output_capability(
        micro.capabilities, showcore::OutputCapability::SafeBlackout));

    const auto& control_one = showcore::output_backend_descriptor(
        showcore::OutputBackendKind::SoundSwitchControlOne);
    CHECK(control_one.implementation ==
          showcore::OutputImplementationStage::Implemented);
    CHECK(control_one.evidence == showcore::OutputEvidenceStage::ContractTested);
    CHECK(control_one.hardware_max_universes == 2U);
    CHECK(control_one.emberlights_supported_universes == 2U);
    CHECK(control_one.configuration_policy ==
          showcore::OutputBackendDescriptor::ConfigurationPolicy::ExperimentalOptIn);
    CHECK(!showcore::output_backend_is_configurable(control_one));
    CHECK(showcore::output_backend_is_configurable(control_one, true));
    CHECK(showcore::has_output_capability(
        control_one.capabilities,
        showcore::OutputCapability::DirectHostOutput));
    CHECK(showcore::has_output_capability(
        control_one.capabilities,
        showcore::OutputCapability::SafeBlackout));

    const auto& wolfmix = showcore::output_backend_descriptor(
        showcore::OutputBackendKind::WolfmixDmxInputBridge);
    CHECK(wolfmix.implementation == showcore::OutputImplementationStage::BridgeOnly);
    CHECK(wolfmix.emberlights_supported_universes == 0U);
    CHECK(!showcore::output_backend_is_configurable(wolfmix, true));
    CHECK(showcore::has_output_capability(
        wolfmix.capabilities, showcore::OutputCapability::ExternalDmxInput));

    showcore::AtomicOutputBackendHealth health;
    health.configure(showcore::OutputBackendKind::SoundSwitchMicro, 1U, 1U, true);
    health.mark_opening();
    health.mark_ready();
    health.record_send(true, 0U, 6U);
    health.record_send(false, 1234U, 6U);
    auto snapshot = health.snapshot();
    CHECK(snapshot.configured);
    CHECK(snapshot.kind == showcore::OutputBackendKind::SoundSwitchMicro);
    CHECK(snapshot.state == showcore::OutputHealthState::Fault);
    CHECK(snapshot.open_attempts == 1U);
    CHECK(snapshot.open_successes == 1U);
    CHECK(snapshot.frames_attempted == 2U);
    CHECK(snapshot.frames_accepted == 1U);
    CHECK(snapshot.frames_failed == 1U);
    CHECK(snapshot.last_error == 1234U);
    CHECK(snapshot.last_nonzero_slots == 6U);

    health.mark_opening();
    CHECK(health.snapshot().state == showcore::OutputHealthState::Recovering);
    health.mark_ready();
    snapshot = health.snapshot();
    CHECK(snapshot.state == showcore::OutputHealthState::Ready);
    CHECK(snapshot.reconnects == 1U);
    CHECK(snapshot.open_attempts == 2U);
    CHECK(snapshot.open_successes == 2U);
    CHECK(snapshot.last_error == 0U);
    health.mark_stopping();
    CHECK(health.snapshot().state == showcore::OutputHealthState::Stopping);
    health.mark_disabled();
    CHECK(health.snapshot().state == showcore::OutputHealthState::Disabled);
}

void test_dmx_usb_pro() {
    showcore::DmxUniverse universe{};
    universe[0] = 0x11U;
    universe[1] = 0x22U;
    universe[511] = 0xFEU;
    const auto packet = showcore::build_dmx_usb_pro_packet(universe);
    static_assert(showcore::kDmxUsbProPacketSize == 518U);
    CHECK(packet.bytes[0] == 0x7EU);
    CHECK(packet.bytes[1] == 0x06U);
    CHECK(packet.bytes[2] == 0x01U);
    CHECK(packet.bytes[3] == 0x02U);
    CHECK(packet.bytes[4] == 0x00U);
    CHECK(packet.bytes[5] == 0x11U);
    CHECK(packet.bytes[6] == 0x22U);
    CHECK(packet.bytes[516] == 0xFEU);
    CHECK(packet.bytes[517] == 0xE7U);

    std::uint16_t port = 0U;
    CHECK(showcore::parse_windows_com_port("COM1", port) && port == 1U);
    CHECK(showcore::parse_windows_com_port("com256", port) && port == 256U);
    CHECK(!showcore::parse_windows_com_port("COM0", port));
    CHECK(!showcore::parse_windows_com_port("COM257", port));
    CHECK(!showcore::parse_windows_com_port("COM3extra", port));
    CHECK(!showcore::parse_windows_com_port("\\\\.\\COM3", port));
}

void test_soundswitch_micro_protocol() {
    showcore::DmxUniverse universe{};
    universe[0] = 0x11U;
    universe[1] = 0x22U;
    universe[511] = 0xFEU;

    const auto packet = showcore::build_soundswitch_micro_packet(
        universe, showcore::SoundSwitchMicroFraming::NativeJls1);
    CHECK(packet.length == 522U);
    CHECK(packet.bytes[0] == 's');
    CHECK(packet.bytes[1] == 'T');
    CHECK(packet.bytes[2] == 'R');
    CHECK(packet.bytes[3] == 't');
    CHECK(packet.bytes[4] == 0x01U);
    CHECK(packet.bytes[5] == 0x00U);
    CHECK(packet.bytes[6] == 0x02U);
    CHECK(packet.bytes[7] == 0x02U);
    CHECK(packet.bytes[8] == 0U);
    CHECK(packet.bytes[9] == 0U);
    CHECK(packet.bytes[10] == 0x11U);
    CHECK(packet.bytes[11] == 0x22U);
    CHECK(packet.bytes[521] == 0xFEU);

    CHECK(showcore::kSoundSwitchMicroInitializationPackets[0][0] == 's');
    CHECK(showcore::kSoundSwitchMicroInitializationPackets[0][4] == 0x02U);
    CHECK(showcore::kSoundSwitchMicroInitializationPackets[0][10] == 0x01U);
    CHECK(showcore::kSoundSwitchMicroInitializationPackets[1][8] == 0x01U);
    CHECK(showcore::kSoundSwitchMicroInitializationPackets[1][10] == 0xFFU);
    CHECK(showcore::kSoundSwitchMicroInitializationPackets[1][11] == 0xFFU);
}

void test_sacn() {
    showcore::DmxUniverse universe{};
    universe[0] = 0x21U;
    universe[511] = 0xFEU;
    const auto cid = showcore::make_sacn_cid("emberlights-test-show");
    CHECK(cid == showcore::make_sacn_cid("emberlights-test-show"));
    CHECK(cid != showcore::make_sacn_cid("another-show"));
    const auto packet = showcore::build_sacn_data_packet(
        universe, 0x1234U, 9U, cid, "EmberLights Test");
    CHECK(packet.length == 638U);
    CHECK(packet.bytes[0] == 0x00U && packet.bytes[1] == 0x10U);
    CHECK(std::string_view(
        reinterpret_cast<const char*>(packet.bytes.data() + 4), 9) == "ASC-E1.17");
    CHECK(packet.bytes[18] == 0x00U && packet.bytes[21] == 0x04U);
    CHECK(std::equal(cid.begin(), cid.end(), packet.bytes.begin() + 22));
    CHECK(packet.bytes[40] == 0x00U && packet.bytes[43] == 0x02U);
    CHECK(std::string_view(
        reinterpret_cast<const char*>(packet.bytes.data() + 44), 16) ==
        std::string_view("EmberLights Test\0", 16));
    CHECK(packet.bytes[108] == 100U && packet.bytes[111] == 9U);
    CHECK(packet.bytes[113] == 0x12U && packet.bytes[114] == 0x34U);
    CHECK(packet.bytes[117] == 0x02U && packet.bytes[118] == 0xA1U);
    CHECK(packet.bytes[123] == 0x02U && packet.bytes[124] == 0x01U);
    CHECK(packet.bytes[125] == 0U && packet.bytes[126] == 0x21U &&
        packet.bytes[637] == 0xFEU);

    const auto invalid = showcore::build_sacn_data_packet(universe, 0, 0, cid, "test");
    CHECK(invalid.length == 0U);
    CHECK(std::string_view(showcore::sacn_multicast_address(1).data()) == "239.255.0.1");
    CHECK(std::string_view(showcore::sacn_multicast_address(0x1234).data()) ==
        "239.255.18.52");

    const auto receiver = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(receiver != kInvalidTestSocket);
    if (receiver != kInvalidTestSocket) {
        sockaddr_in receiver_address{};
        receiver_address.sin_family = AF_INET;
        receiver_address.sin_port = 0;
        CHECK(::inet_pton(AF_INET, "127.0.0.1", &receiver_address.sin_addr) == 1);
        CHECK(::bind(
            receiver,
            reinterpret_cast<const sockaddr*>(&receiver_address),
            static_cast<int>(sizeof(receiver_address))) == 0);
#ifdef _WIN32
        int receiver_address_size = sizeof(receiver_address);
#else
        socklen_t receiver_address_size = sizeof(receiver_address);
#endif
        CHECK(::getsockname(
            receiver,
            reinterpret_cast<sockaddr*>(&receiver_address),
            &receiver_address_size) == 0);
        showcore::SacnSender sender;
        CHECK(sender.open_ipv4("127.0.0.1", ntohs(receiver_address.sin_port)));
        CHECK(sender.send(packet));

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(receiver, &read_set);
        timeval timeout{1, 0};
#ifdef _WIN32
        const auto selected = ::select(0, &read_set, nullptr, nullptr, &timeout);
#else
        const auto selected = ::select(receiver + 1, &read_set, nullptr, nullptr, &timeout);
#endif
        CHECK(selected == 1);
        if (selected == 1) {
            std::array<std::uint8_t, showcore::kMaxSacnPacketSize> received{};
#ifdef _WIN32
            const auto received_size = ::recv(
                receiver,
                reinterpret_cast<char*>(received.data()),
                static_cast<int>(received.size()),
                0);
#else
            const auto received_size = ::recv(
                receiver,
                received.data(),
                received.size(),
                0);
#endif
            CHECK(received_size >= 0 &&
                static_cast<std::size_t>(received_size) == packet.length);
            if (received_size >= 0 &&
                static_cast<std::size_t>(received_size) == packet.length) {
                CHECK(std::equal(
                    packet.bytes.begin(),
                    packet.bytes.begin() + static_cast<std::ptrdiff_t>(packet.length),
                    received.begin()));
            }
        }
        sender.close();
        close_test_socket(receiver);
    }
}

void test_os2l() {
    showcore::Os2lEvent event;
    auto error = showcore::parse_os2l(
        R"({"evt":"beat","change":false,"pos":42,"bpm":120.5,"strength":0.9})", event);
    CHECK(error == showcore::Os2lParseError::None);
    CHECK(event.kind == showcore::Os2lKind::Beat);
    CHECK(event.beat.position == 42 && std::abs(event.beat.bpm - 120.5) < 0.0001);
    CHECK(event.beat.has_strength && std::abs(event.beat.strength - 0.9) < 0.0001);

    error = showcore::parse_os2l(
        R"({"evt":"beat","change":false,"pos":-4,"bpm":1.3725e2})", event);
    CHECK(error == showcore::Os2lParseError::None);
    CHECK(event.beat.position == -4 && std::abs(event.beat.bpm - 137.25) < 0.0001);

    error = showcore::parse_os2l(R"({"evt":"btn","name":"blackout","state":"on"})", event);
    CHECK(error == showcore::Os2lParseError::None);
    CHECK(event.kind == showcore::Os2lKind::Button && event.button.on);
    CHECK(event.button.name.view() == "blackout" && event.button.page.view().empty());

    error = showcore::parse_os2l(R"({"evt":"cmd","id":42,"param":75.5})", event);
    CHECK(error == showcore::Os2lParseError::None);
    CHECK(event.kind == showcore::Os2lKind::Command && event.command.id == 42);
    CHECK(std::abs(event.command.parameter - 75.5) < 0.0001);

    error = showcore::parse_os2l(R"({"evt":"something-new","value":1})", event);
    CHECK(error == showcore::Os2lParseError::None && event.kind == showcore::Os2lKind::Unknown);
    CHECK(showcore::parse_os2l(R"({"evt":"beat","change":false,"pos":2})", event) == showcore::Os2lParseError::MissingField);
    CHECK(showcore::parse_os2l("not-json", event) == showcore::Os2lParseError::Malformed);
    CHECK(showcore::parse_os2l(R"({"evt":"cmd","id":1,"param":50)", event) == showcore::Os2lParseError::Malformed);
    const std::string oversized(4097, 'x');
    CHECK(showcore::parse_os2l(oversized, event) == showcore::Os2lParseError::Oversized);
}

struct StreamCapture {
    std::array<showcore::Os2lEvent, 4> events{};
    std::size_t count{0};
};

void capture_stream_event(
    const showcore::Os2lEvent& event,
    showcore::Os2lParseError error,
    std::string_view,
    void* context) noexcept {
    auto& capture = *static_cast<StreamCapture*>(context);
    if (error == showcore::Os2lParseError::None && capture.count < capture.events.size()) {
        capture.events[capture.count++] = event;
    }
}

void test_os2l_stream() {
    showcore::Os2lStreamDecoder decoder;
    StreamCapture capture;
    auto result = decoder.feed(R"({"evt":"be)", &capture_stream_event, &capture);
    CHECK(result.messages == 0U && result.errors == 0U);
    result = decoder.feed(
        R"(at","change":false,"pos":8,"bpm":128}{"evt":"btn","name":"brace } button","state":"on"})",
        &capture_stream_event,
        &capture);
    CHECK(result.messages == 2U && result.errors == 0U);
    CHECK(capture.count == 2U);
    CHECK(capture.events[0].kind == showcore::Os2lKind::Beat);
    CHECK(capture.events[1].button.name.view() == "brace } button");

    result = decoder.feed("junk", &capture_stream_event, &capture);
    CHECK(result.errors == 4U);

    std::string huge = "{\"evt\":\"btn\",\"name\":\"";
    huge.append(5000, 'x');
    huge += "\",\"state\":\"on\"}";
    result = decoder.feed(huge, &capture_stream_event, &capture);
    CHECK(result.errors == 1U);
}

void test_os2l_server_lifecycle() {
    showcore::Os2lTcpServer server;
    CHECK(server.open_ipv4("127.0.0.1", 0));
    CHECK(server.state() == showcore::Os2lServerState::Listening);
    CHECK(server.bound_port() != 0U);
#ifndef _WIN32
    CHECK(server.discovery_state() == showcore::Os2lDiscoveryState::Unavailable);
#endif

    auto client = connect_loopback(server.bound_port());
    CHECK(client != kInvalidTestSocket);
    CHECK(server.poll(&capture_os2l_event, nullptr, 1000) ==
        showcore::Os2lPollResult::ClientConnected);
    CHECK(server.state() == showcore::Os2lServerState::ClientConnected);

    Os2lCapture capture{};
    constexpr std::string_view messages =
        R"({"evt":"beat","change":true,"pos":17,"bpm":124.5})"
        R"({"evt":"btn","name":"blackout","state":"on"})";
    CHECK(send_all(client, messages));
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::EventsReceived);
    CHECK(capture.beats == 1U);
    CHECK(capture.buttons == 1U);
    CHECK(capture.errors == 0U);
    CHECK(std::abs(capture.last_bpm - 124.5) < 0.001);

    constexpr std::string_view partial = R"({"evt":"beat","change":true)";
    CHECK(send_all(client, partial));
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::Idle);
    close_test_socket(client);
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::ClientDisconnected);
    CHECK(server.state() == showcore::Os2lServerState::Listening);

    client = connect_loopback(server.bound_port());
    CHECK(client != kInvalidTestSocket);
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::ClientConnected);
    constexpr std::string_view second =
        R"({"evt":"beat","change":true,"pos":18,"bpm":125.0})";
    CHECK(send_all(client, second));
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::EventsReceived);
    CHECK(capture.beats == 2U);
    CHECK(capture.errors == 0U);
    CHECK(std::abs(capture.last_bpm - 125.0) < 0.001);
    close_test_socket(client);
    CHECK(server.poll(&capture_os2l_event, &capture, 1000) ==
        showcore::Os2lPollResult::ClientDisconnected);

    CHECK(server.stats().connections == 2U);
    CHECK(server.stats().disconnects == 2U);
    CHECK(server.stats().messages == 3U);
    CHECK(server.stats().decode_errors == 0U);
    CHECK(server.stats().client_errors == 0U);
    server.close();
    CHECK(server.state() == showcore::Os2lServerState::Closed);
}

void test_spsc_queue() {
    showcore::SpscQueue<std::uint32_t, 4> queue;
    static_assert(decltype(queue)::capacity == 3U);
    CHECK(queue.empty());
    CHECK(queue.try_push(10U));
    CHECK(queue.try_push(20U));
    CHECK(queue.try_push(30U));
    CHECK(!queue.try_push(40U));

    std::uint32_t value = 0;
    CHECK(queue.try_pop(value) && value == 10U);
    CHECK(queue.try_push(40U));
    CHECK(queue.try_pop(value) && value == 20U);
    CHECK(queue.try_pop(value) && value == 30U);
    CHECK(queue.try_pop(value) && value == 40U);
    CHECK(!queue.try_pop(value));
    CHECK(queue.empty());
    CHECK(queue.try_push(50U));
    queue.reset();
    CHECK(queue.empty());
    CHECK(!queue.try_pop(value));
    CHECK(queue.try_pop_latest(value) == 0U);
    CHECK(queue.try_push(60U));
    CHECK(queue.try_push(70U));
    CHECK(queue.try_push(80U));
    CHECK(queue.try_pop_latest(value) == 3U);
    CHECK(value == 80U);
    CHECK(queue.empty());
}

void test_sync_manager() {
    showcore::SyncConfig config;
    config.minimum_hold_ms = 100;
    config.minimum_fallback_ms = 300;
    config.hold_beats = 0.1;
    config.fallback_beats = 0.5;
    config.stable_beats_to_relock = 3;

    showcore::SyncManager sync(config);
    sync.on_os2l_beat(0, 120.0, 0);
    CHECK(sync.tick(50).state == showcore::SyncState::Os2lHealthy);
    CHECK(sync.tick(150).state == showcore::SyncState::PredictiveHold);

    sync.on_audio_clock({true, 121.0, 1.0, 0.9F, 350});
    auto snapshot = sync.tick(350);
    CHECK(snapshot.state == showcore::SyncState::AudioFallback);
    CHECK(snapshot.source == showcore::ClockSource::Audio && nearly_equal(snapshot.confidence, 0.9F));

    sync.on_os2l_beat(1, 120.0, 400);
    CHECK(sync.tick(400).state == showcore::SyncState::Recovering);
    sync.on_os2l_beat(2, 120.2, 500);
    CHECK(sync.tick(500).state == showcore::SyncState::Recovering);
    sync.on_os2l_beat(3, 120.1, 600);
    snapshot = sync.tick(600);
    CHECK(snapshot.state == showcore::SyncState::Os2lHealthy && snapshot.exact_transport);

    showcore::SyncManager manual(config);
    manual.set_manual_bpm(100.0, 1000);
    snapshot = manual.tick(1300);
    CHECK(snapshot.state == showcore::SyncState::Manual);
    CHECK(std::abs(snapshot.beat_position - 0.5) < 0.001);
}

void test_midi() {
    auto midi_storage = std::make_unique<showcore::MidiMappingEngine>();
    auto& midi = *midi_storage;
    showcore::MidiMapping soft;
    soft.message_type = showcore::MidiMessageType::ControlChange;
    soft.number = 10;
    soft.soft_takeover = true;
    soft.action = {showcore::ActionType::SetProperty, showcore::LayerId::ManualOverride,
        showcore::Property::Intensity, 7};
    CHECK(midi.add(soft));
    CHECK(midi.set_takeover_target(0, 0.5F));

    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> events{};
    CHECK(midi.process({1, showcore::MidiMessageType::ControlChange, 0, 10, 10}, events) == 0U);
    CHECK(midi.process({1, showcore::MidiMessageType::ControlChange, 0, 10, 50}, events) == 0U);
    CHECK(midi.process({1, showcore::MidiMessageType::ControlChange, 0, 10, 64}, events) == 1U);
    CHECK(nearly_equal(events[0].value, 64.0F / 127.0F));

    showcore::MidiMapping relative;
    relative.device_id = 2;
    relative.message_type = showcore::MidiMessageType::ControlChange;
    relative.number = 20;
    relative.input_mode = showcore::MidiInputMode::RelativeTwosComplement;
    relative.behavior = showcore::MappingBehavior::Relative;
    relative.action = {showcore::ActionType::SetProperty, showcore::LayerId::ManualOverride,
        showcore::Property::Pan, 3};
    CHECK(midi.add(relative));
    CHECK(midi.process({2, showcore::MidiMessageType::ControlChange, 0, 20, 1}, events) == 1U);
    CHECK(events[0].relative && events[0].value > 0.0F);
    CHECK(midi.process({2, showcore::MidiMessageType::ControlChange, 0, 20, 127}, events) == 1U);
    CHECK(events[0].relative && events[0].value < 0.0F);
}

void test_midi_short_codec() {
    showcore::MidiMessage message;
    CHECK(showcore::decode_short_midi(42, 0x007F3C90U, 1234, message));
    CHECK(message.device_id == 42U);
    CHECK(message.type == showcore::MidiMessageType::NoteOn);
    CHECK(message.channel == 0U);
    CHECK(message.number == 60U);
    CHECK(message.value == 127U);
    CHECK(message.timestamp_ms == 1234U);

    CHECK(showcore::decode_short_midi(7, 0x004001B3U, 55, message));
    CHECK(message.type == showcore::MidiMessageType::ControlChange);
    CHECK(message.channel == 3U);
    CHECK(message.number == 1U);
    CHECK(message.value == 64U);

    CHECK(showcore::decode_short_midi(9, 0x007F00E1U, 88, message));
    CHECK(message.type == showcore::MidiMessageType::PitchBend);
    CHECK(message.channel == 1U);
    CHECK(message.value == 16256U);
    CHECK(!showcore::decode_short_midi(9, 0x000000F8U, 88, message));

    const showcore::MidiMessage output{
        3,
        showcore::MidiMessageType::PitchBend,
        5,
        0,
        8192,
        500};
    const auto packet = showcore::encode_short_midi(output);
    CHECK(packet);
    CHECK(packet.packed == 0x004000E5U);
    CHECK(showcore::decode_short_midi(3, packet.packed, 501, message));
    CHECK(message.type == output.type);
    CHECK(message.channel == output.channel);
    CHECK(message.value == output.value);
    CHECK(message.timestamp_ms == 501U);

    auto invalid = output;
    invalid.channel = 16;
    CHECK(!showcore::encode_short_midi(invalid));
    invalid = output;
    invalid.value = 16384;
    CHECK(!showcore::encode_short_midi(invalid));
}

void test_winmm_midi_platform_boundary() {
#ifndef _WIN32
    CHECK(showcore::enumerate_winmm_midi_inputs().count == 0U);
    CHECK(showcore::enumerate_winmm_midi_outputs().count == 0U);
    showcore::WinMmMidiInput input;
    showcore::WinMmMidiOutput output;
    CHECK(!input.supported());
    CHECK(!output.supported());
    CHECK(!input.open(0, 1));
    CHECK(!output.open(0, 1));
    showcore::MidiMessage message;
    CHECK(!input.poll(message));
    CHECK(!output.send(1, message));
#endif
}

constexpr std::array<showcore::LookAssignment, 4> kRedLookAssignments{{
    {4, showcore::Property::Intensity, showcore::PropertyValue::set(0.2F)},
    {4, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
    {4, showcore::Property::Blue, showcore::PropertyValue::set(0.0F)},
    {4, showcore::Property::Strobe, showcore::PropertyValue::release()}
}};

constexpr std::array<showcore::LookAssignment, 4> kBlueLookAssignments{{
    {4, showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
    {4, showcore::Property::Red, showcore::PropertyValue::set(0.0F)},
    {4, showcore::Property::Blue, showcore::PropertyValue::set(1.0F)},
    {4, showcore::Property::Strobe, showcore::PropertyValue::release()}
}};

constexpr showcore::StaticLook kRedLook{
    "Red look",
    kRedLookAssignments.data(),
    kRedLookAssignments.size()};

constexpr showcore::StaticLook kBlueLook{
    "Blue look",
    kBlueLookAssignments.data(),
    kBlueLookAssignments.size()};

showcore::AutoloopPattern make_test_autoloop() {
    showcore::AutoloopPattern pattern;
    pattern.name = "Red to blue";
    pattern.length_beats = 4.0F;
    CHECK(pattern.add_step({0.0F, &kRedLook, showcore::AutoloopTransition::Linear}));
    CHECK(pattern.add_step({2.0F, &kBlueLook, showcore::AutoloopTransition::Cut}));
    return pattern;
}

void test_static_look_validation() {
    CHECK(showcore::validate_static_look(kRedLook));

    auto invalid = kRedLook;
    invalid.name = "";
    CHECK(showcore::validate_static_look(invalid).error == showcore::LookError::MissingName);

    auto assignments = kRedLookAssignments;
    assignments[1] = assignments[0];
    invalid = {"Duplicate", assignments.data(), assignments.size()};
    const auto duplicate = showcore::validate_static_look(invalid);
    CHECK(duplicate.error == showcore::LookError::DuplicateAssignment);
    CHECK(duplicate.conflicting_assignment_index == 0U);

    assignments = kRedLookAssignments;
    assignments[0].value = showcore::PropertyValue::set(
        std::numeric_limits<float>::quiet_NaN());
    invalid = {"NaN", assignments.data(), assignments.size()};
    CHECK(showcore::validate_static_look(invalid).error == showcore::LookError::InvalidValue);
}

void test_static_look_player() {
    constexpr std::array<showcore::LookAssignment, 3> assignments{{
        {7, showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
        {7, showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
        {7, showcore::Property::Blue, showcore::PropertyValue::force_zero()}
    }};
    const showcore::StaticLook look{"Formal moment", assignments.data(), assignments.size()};

    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    layers.set(showcore::LayerId::TrackScript, 7, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.2F));
    layers.set(showcore::LayerId::TrackScript, 7, showcore::Property::Red,
        showcore::PropertyValue::set(0.1F));
    layers.set(showcore::LayerId::TrackScript, 7, showcore::Property::Green,
        showcore::PropertyValue::set(0.3F));
    layers.set(showcore::LayerId::TrackScript, 7, showcore::Property::Blue,
        showcore::PropertyValue::set(0.4F));

    auto player_storage = std::make_unique<showcore::StaticLookPlayer>();
    auto& player = *player_storage;
    CHECK(player.trigger(look, 1000, 1000, layers));
    player.tick(1500, layers);
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.6F));
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Red).value, 0.55F));
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Blue).value, 0.2F));
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Green).value, 0.3F));
    CHECK(layers.resolve(7, showcore::Property::Green).source == showcore::LayerId::TrackScript);

    player.tick(2000, layers);
    CHECK(layers.resolve(7, showcore::Property::Blue).mode == showcore::ValueMode::ForceZero);
    CHECK(player.status(2000).active && !player.status(2000).transitioning);

    layers.set(showcore::LayerId::TrackScript, 7, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.4F));
    player.clear(2000, 1000, layers);
    player.tick(2500, layers);
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.7F));
    player.tick(3000, layers);
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.4F));
    CHECK(layers.resolve(7, showcore::Property::Intensity).source == showcore::LayerId::TrackScript);
    CHECK(!player.status(3000).active && !player.status(3000).transitioning);

    constexpr std::array<showcore::LookAssignment, 1> dark_assignments{{
        {7, showcore::Property::Intensity, showcore::PropertyValue::set(0.0F)}
    }};
    const showcore::StaticLook dark{"Dark", dark_assignments.data(), dark_assignments.size()};
    CHECK(player.trigger(look, 3000, 1000, layers));
    player.tick(3500, layers);
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.7F));
    CHECK(player.trigger(dark, 3500, 1000, layers));
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.7F));
    player.tick(4000, layers);
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.35F));
    player.clear(4000, 0, layers);

    auto invalid = look;
    invalid.name = nullptr;
    CHECK(!player.trigger(invalid, 3000, 0, layers));
    CHECK(nearly_equal(layers.resolve(7, showcore::Property::Intensity).value, 0.4F));
}

void test_autoloop() {
    auto pattern = make_test_autoloop();
    CHECK(showcore::validate_autoloop_pattern(pattern));
    CHECK(!pattern.add_step({1.0F, &kRedLook, showcore::AutoloopTransition::Cut}));

    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    auto autoloop_storage = std::make_unique<showcore::AutoloopEngine>();
    auto& autoloop = *autoloop_storage;
    CHECK(autoloop.apply(pattern, 1.0, showcore::LayerId::Autonomous, layers));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Red).value, 0.5F));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Blue).value, 0.5F));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Intensity).value, 0.6F));

    CHECK(autoloop.apply(pattern, 3.0, showcore::LayerId::Autonomous, layers));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Red).value, 0.0F));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Blue).value, 1.0F));

    auto invalid = pattern;
    invalid.name = nullptr;
    CHECK(!autoloop.apply(invalid, 0.0, showcore::LayerId::Autonomous, layers));
    CHECK(!layers.resolve(4, showcore::Property::Intensity).owned);
}

void test_autoloop_catalog_and_player() {
    auto pattern = make_test_autoloop();
    showcore::AutoloopCatalog catalog;
    CHECK(catalog.set({0, 0}, &pattern));
    CHECK(catalog.set({2, 5}, &pattern));
    CHECK(catalog.set({42, 7}, &pattern));
    CHECK(catalog.set({63, 31}, &pattern));
    CHECK(!catalog.set({64, 0}, &pattern));
    CHECK((catalog.next_available() == showcore::AutoloopAddress{0, 0}));
    CHECK((catalog.next_available({0, 0}) == showcore::AutoloopAddress{2, 5}));
    CHECK(catalog.select_exclusive_bank(42));
    CHECK(catalog.bank_enabled(42));
    CHECK(!catalog.bank_enabled(2));
    CHECK((catalog.next_available() == showcore::AutoloopAddress{42, 7}));
    CHECK((catalog.previous_available() == showcore::AutoloopAddress{42, 7}));
    CHECK(!catalog.select_exclusive_bank(64));
    CHECK(catalog.set_bank_enabled(63, true));
    CHECK(catalog.bank_enabled(63));
    CHECK(catalog.set_bank_enabled(42, false));
    CHECK((catalog.next_available() == showcore::AutoloopAddress{63, 31}));
    CHECK(!catalog.set_bank_enabled(64, true));
    catalog.select_all_banks();
    CHECK(catalog.bank_enabled(0));
    CHECK(catalog.bank_enabled(63));
    CHECK(catalog.duplicate({0, 0}, {1, 1}));
    CHECK(!catalog.duplicate({0, 0}, {1, 1}));
    CHECK(catalog.swap_slots({1, 1}, {33, 31}));
    CHECK(catalog.get({33, 31}) == &pattern);

    showcore::AutoloopBankWindow bank_window;
    CHECK((bank_window.address(0, 0) == showcore::AutoloopAddress{0, 0}));
    CHECK((bank_window.address(3, 31) == showcore::AutoloopAddress{3, 31}));
    CHECK(!bank_window.address(4, 0).valid());
    CHECK(bank_window.select_page(10));
    CHECK((bank_window.address(2, 5) == showcore::AutoloopAddress{42, 5}));
    CHECK(!bank_window.select_page(16));
    CHECK(bank_window.page() == 10U);
    bank_window.next_page();
    CHECK(bank_window.page() == 11U);
    CHECK((bank_window.address(0, 0) == showcore::AutoloopAddress{44, 0}));
    CHECK(bank_window.select_page(15));
    bank_window.next_page();
    CHECK(bank_window.page() == 0U);
    bank_window.previous_page();
    CHECK(bank_window.page() == 15U);
    CHECK((bank_window.address(3, 31) == showcore::AutoloopAddress{63, 31}));

    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    layers.set(showcore::LayerId::TrackScript, 4, showcore::Property::Intensity,
        showcore::PropertyValue::set(0.25F));
    auto player_storage = std::make_unique<showcore::AutoloopPlayer>();
    auto& player = *player_storage;
    CHECK(player.trigger(
        catalog,
        {0, 0},
        showcore::AutoloopRepeat::Once,
        10.0,
        true,
        layers));
    player.tick(11.0, true, layers);
    CHECK(player.status().active);
    CHECK(nearly_equal(player.status().progress, 0.25F));
    CHECK(layers.resolve(4, showcore::Property::Intensity).source ==
        showcore::LayerId::ManualAutoloop);
    player.tick(14.0, true, layers);
    CHECK(!player.status().active && nearly_equal(player.status().progress, 1.0F));
    CHECK(nearly_equal(layers.resolve(4, showcore::Property::Intensity).value, 0.25F));
    CHECK(layers.resolve(4, showcore::Property::Intensity).source ==
        showcore::LayerId::TrackScript);

    CHECK(player.trigger(
        catalog,
        {0, 0},
        showcore::AutoloopRepeat::Infinite,
        20.0,
        true,
        layers));
    player.tick(29.0, true, layers);
    CHECK(player.status().active && player.status().completed_cycles == 2U);
    CHECK(nearly_equal(player.status().progress, 0.25F));

    CHECK(player.trigger(
        catalog,
        {0, 0},
        showcore::AutoloopRepeat::TrackDuration,
        30.0,
        true,
        layers));
    player.tick(31.0, false, layers);
    CHECK(!player.status().active);
    CHECK(layers.resolve(4, showcore::Property::Intensity).source ==
        showcore::LayerId::TrackScript);
}

void test_render_has_no_allocations() {
    auto engine = std::make_unique<showcore::Engine>();
    CHECK(engine->patch().add({0, 0, 1, &kRgbFixture}));
    engine->layers().set(showcore::LayerId::Autonomous, 0, showcore::Property::Intensity, showcore::PropertyValue::set(0.5F));
    const auto before = g_allocations.load();
    for (int index = 0; index < 10000; ++index) {
        engine->tick();
    }
    const auto after = g_allocations.load();
    CHECK(after == before);
}

void test_performance_playback_has_no_allocations() {
    auto pattern = make_test_autoloop();
    showcore::AutoloopCatalog catalog;
    CHECK(catalog.set({0, 0}, &pattern));
    auto layers_storage = std::make_unique<showcore::LayerStack>();
    auto& layers = *layers_storage;
    auto autoloop_storage = std::make_unique<showcore::AutoloopPlayer>();
    auto& autoloop = *autoloop_storage;
    auto look_storage = std::make_unique<showcore::StaticLookPlayer>();
    auto& look = *look_storage;
    CHECK(autoloop.trigger(
        catalog,
        {0, 0},
        showcore::AutoloopRepeat::Infinite,
        0.0,
        true,
        layers));
    CHECK(look.trigger(kBlueLook, 0, 750, layers));

    const auto before = g_allocations.load();
    for (int tick = 0; tick < 10000; ++tick) {
        const auto now_ms = static_cast<std::uint64_t>(tick * 25);
        const auto beat = static_cast<double>(tick) / 20.0;
        autoloop.tick(beat, true, layers);
        look.tick(now_ms, layers);
    }
    const auto after = g_allocations.load();
    CHECK(after == before);
}

void test_deterministic_replay() {
    auto first = std::make_unique<showcore::Engine>();
    auto second = std::make_unique<showcore::Engine>();
    CHECK(first->patch().add({0, 0, 1, &kRgbFixture}));
    CHECK(second->patch().add({0, 0, 1, &kRgbFixture}));

    for (int tick = 0; tick < 1000; ++tick) {
        const auto value = static_cast<float>((tick * 37) % 256) / 255.0F;
        for (auto* engine : {first.get(), second.get()}) {
            engine->layers().set(showcore::LayerId::TrackScript, 0,
                showcore::Property::Intensity, showcore::PropertyValue::set(value));
            engine->layers().set(showcore::LayerId::TrackScript, 0,
                showcore::Property::Blue, showcore::PropertyValue::set(1.0F - value));
            engine->tick();
        }
        CHECK(first->frames().universes == second->frames().universes);
    }
}

emberlights::ProjectDocument make_test_project() {
    auto project = emberlights::make_starter_project();
    project.id = "test-show";
    project.name = "Test Show";
    project.fixtures.push_back({
        "wash-1", "Wash 1", "builtin.generic.rgbd-4ch", 1, 1, {"dance-floor"}});
    emberlights::LookDefinition red;
    red.id = "red";
    red.name = "Red";
    red.assignments = {
        {"wash-1", showcore::Property::Intensity, showcore::PropertyValue::set(1.0F)},
        {"wash-1", showcore::Property::Red, showcore::PropertyValue::set(1.0F)},
        {"wash-1", showcore::Property::Green, showcore::PropertyValue::force_zero()},
        {"wash-1", showcore::Property::Blue, showcore::PropertyValue::force_zero()}};
    emberlights::LookDefinition blue;
    blue.id = "blue";
    blue.name = "Blue";
    blue.assignments = {
        {"wash-1", showcore::Property::Intensity, showcore::PropertyValue::set(0.75F)},
        {"wash-1", showcore::Property::Red, showcore::PropertyValue::force_zero()},
        {"wash-1", showcore::Property::Green, showcore::PropertyValue::force_zero()},
        {"wash-1", showcore::Property::Blue, showcore::PropertyValue::set(1.0F)}};
    project.looks.push_back(std::move(red));
    project.looks.push_back(std::move(blue));
    project.autoloops.push_back({
        "red-blue", "Red / Blue", 7, 3, 4.0F, showcore::AutoloopRepeat::Infinite,
        {{0.0F, "red", showcore::AutoloopTransition::Linear},
         {2.0F, "blue", showcore::AutoloopTransition::Linear}}});
    project.track_scripts.push_back({
        "test-song", "Test Song", "audio:test-song", {
            {0.0F, emberlights::TrackCueAction::TriggerLook, "red"},
            {1.0F, emberlights::TrackCueAction::TriggerAutoloop, "red-blue"},
            {2.0F, emberlights::TrackCueAction::ClearLook, ""},
            {3.0F, emberlights::TrackCueAction::ClearAutoloop, ""}},
        {}});
    emberlights::MidiMappingDefinition start_track;
    start_track.device_name = "Test controller";
    start_track.target_ref = "test-song";
    start_track.message_type = showcore::MidiMessageType::NoteOn;
    start_track.channel = 0U;
    start_track.number = 24U;
    start_track.behavior = showcore::MappingBehavior::Toggle;
    start_track.action.type = showcore::ActionType::TriggerTrackScript;
    project.midi_mappings.push_back(std::move(start_track));
    emberlights::MidiMappingDefinition release_overrides;
    release_overrides.device_name = "Test controller";
    release_overrides.message_type = showcore::MidiMessageType::NoteOn;
    release_overrides.channel = 0U;
    release_overrides.number = 25U;
    release_overrides.behavior = showcore::MappingBehavior::Momentary;
    release_overrides.action.type = showcore::ActionType::ClearManualOverrides;
    project.midi_mappings.push_back(std::move(release_overrides));
    project.groups.push_back({"dance", "Dance Floor", {"wash-1"}});
    emberlights::MidiMappingDefinition set_group;
    set_group.device_name = "Test controller";
    set_group.target_ref = "dance";
    set_group.message_type = showcore::MidiMessageType::ControlChange;
    set_group.channel = 0U;
    set_group.number = 26U;
    set_group.behavior = showcore::MappingBehavior::Continuous;
    set_group.action.type = showcore::ActionType::SetGroupProperty;
    set_group.action.property = showcore::Property::Blue;
    project.midi_mappings.push_back(std::move(set_group));
    emberlights::MidiMappingDefinition select_bank;
    select_bank.device_name = "Test controller";
    select_bank.message_type = showcore::MidiMessageType::NoteOn;
    select_bank.channel = 0U;
    select_bank.number = 27U;
    select_bank.behavior = showcore::MappingBehavior::Momentary;
    select_bank.action.type = showcore::ActionType::SelectAutoloopBank;
    select_bank.action.target_id = 7U;
    project.midi_mappings.push_back(std::move(select_bank));
    emberlights::MidiMappingDefinition enable_bank;
    enable_bank.device_name = "Test controller";
    enable_bank.message_type = showcore::MidiMessageType::NoteOn;
    enable_bank.channel = 0U;
    enable_bank.number = 28U;
    enable_bank.behavior = showcore::MappingBehavior::Toggle;
    enable_bank.action.type = showcore::ActionType::SetAutoloopBankEnabled;
    enable_bank.action.target_id = 2U;
    project.midi_mappings.push_back(std::move(enable_bank));
    emberlights::MidiMappingDefinition select_all_banks;
    select_all_banks.device_name = "Test controller";
    select_all_banks.message_type = showcore::MidiMessageType::NoteOn;
    select_all_banks.channel = 0U;
    select_all_banks.number = 29U;
    select_all_banks.behavior = showcore::MappingBehavior::Momentary;
    select_all_banks.action.type = showcore::ActionType::SelectAllAutoloopBanks;
    project.midi_mappings.push_back(std::move(select_all_banks));
    emberlights::MidiMappingDefinition blackout_group;
    blackout_group.device_name = "Test controller";
    blackout_group.target_ref = "dance";
    blackout_group.message_type = showcore::MidiMessageType::NoteOn;
    blackout_group.channel = 0U;
    blackout_group.number = 30U;
    blackout_group.behavior = showcore::MappingBehavior::Momentary;
    blackout_group.action.type = showcore::ActionType::BlackoutGroup;
    project.midi_mappings.push_back(std::move(blackout_group));
    return project;
}

void test_project_edit_history() {
    emberlights::ProjectEditHistory history;
    auto project = make_test_project();
    const auto original_name = project.name;

    history.record_before_change(project);
    project.name = "First Revision";
    history.record_before_change(project);
    project.name = "Second Revision";
    CHECK(history.can_undo());
    CHECK(!history.can_redo());
    CHECK(history.undo_count() == 2U);
    CHECK(history.undo(project));
    CHECK(project.name == "First Revision");
    CHECK(history.undo(project));
    CHECK(project.name == original_name);
    CHECK(!history.can_undo());
    CHECK(history.can_redo());
    CHECK(history.redo(project));
    CHECK(project.name == "First Revision");

    history.record_before_change(project);
    project.name = "Branch Revision";
    CHECK(!history.can_redo());
    CHECK(history.redo_count() == 0U);

    history.clear();
    for (std::size_t index = 0; index < emberlights::kMaximumProjectUndoEntries + 3U; ++index) {
        history.record_before_change(project);
        project.name = "Revision " + std::to_string(index);
    }
    CHECK(history.undo_count() == emberlights::kMaximumProjectUndoEntries);
    CHECK(history.undo(project));
    CHECK(project.name == "Revision " +
        std::to_string(emberlights::kMaximumProjectUndoEntries + 1U));
}

void test_autoloop_placement_operations() {
    auto project = make_test_project();
    auto copy = project.autoloops.front();
    copy.id = "second-loop";
    copy.bank = 7U;
    copy.slot = 4U;
    project.autoloops.push_back(copy);

    CHECK(emberlights::move_autoloop(
              project, "red-blue", 7U, 4U) ==
        emberlights::AutoloopPlacementResult::TargetOccupied);
    CHECK(project.autoloops[0].slot == 3U);
    CHECK(emberlights::move_autoloop(
              project, "red-blue", 7U, 4U, true) ==
        emberlights::AutoloopPlacementResult::Swapped);
    CHECK(project.autoloops[0].slot == 4U);
    CHECK(project.autoloops[1].slot == 3U);
    CHECK(emberlights::move_autoloop_to_next_empty_slot(project, "red-blue") ==
        emberlights::AutoloopPlacementResult::Moved);
    CHECK(project.autoloops[0].bank == 7U);
    CHECK(project.autoloops[0].slot == 5U);
    CHECK(emberlights::move_autoloop(
              project, "missing", 0U, 0U) ==
        emberlights::AutoloopPlacementResult::SourceMissing);
    CHECK(emberlights::move_autoloop(
              project, "red-blue", 64U, 0U) ==
        emberlights::AutoloopPlacementResult::InvalidAddress);

    emberlights::ProjectDocument full;
    full.autoloops.reserve(showcore::kMaxAutoloops);
    for (std::size_t address = 0; address < showcore::kMaxAutoloops; ++address) {
        full.autoloops.push_back({
            "loop-" + std::to_string(address),
            "Loop",
            static_cast<std::uint16_t>(address / showcore::kAutoloopsPerBank),
            static_cast<std::uint8_t>(address % showcore::kAutoloopsPerBank),
            4.0F,
            showcore::AutoloopRepeat::Infinite,
            {}});
    }
    CHECK(emberlights::move_autoloop_to_next_empty_slot(full, "loop-0") ==
        emberlights::AutoloopPlacementResult::LibraryFull);
}

void test_audio_asset_identity_and_relinking() {
    const auto directory = std::filesystem::path("build/audio-asset-test");
    const auto first = directory / "Test Song.mp3";
    const auto moved = directory / "Moved Song.mp3";
    const auto changed = directory / "Different Song.mp3";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    CHECK(!ignored);
    auto write_file = [](const std::filesystem::path& path, std::string_view bytes) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    };
    CHECK(write_file(first, "audio-identity-source"));
    CHECK(write_file(changed, "different-audio-content"));

    emberlights::AudioAssetDefinition asset;
    const auto imported = emberlights::make_audio_asset(first, "audio.test-song", asset);
    CHECK(imported.available());
    CHECK(asset.id == "audio.test-song");
    CHECK(asset.file_name == "Test Song.mp3");
    CHECK(asset.size_bytes == 21U);
    CHECK(asset.sha256 == "eb96dbf9519f0d542b682de8c3cfbac3c1694426bbd11fcb46e4c7bbe16fb482");
    CHECK(emberlights::verify_audio_asset(asset).available());
    CHECK(emberlights::relink_audio_asset(asset, changed).status ==
        emberlights::AudioAssetFileStatus::Changed);
    CHECK(asset.file_name == "Test Song.mp3");
    CHECK(std::filesystem::copy_file(first, moved, std::filesystem::copy_options::overwrite_existing, ignored));
    CHECK(!ignored);
    CHECK(emberlights::relink_audio_asset(asset, moved).available());
    CHECK(asset.local_path_hint.find("Moved Song.mp3") != std::string::npos);
    std::filesystem::remove(moved, ignored);
    CHECK(emberlights::verify_audio_asset(asset).status == emberlights::AudioAssetFileStatus::Missing);

    auto project = make_test_project();
    project.audio_assets.push_back(asset);
    project.track_scripts.front().audio_asset_id = asset.id;
    CHECK(emberlights::validate_project(project).ok());
    const auto library = directory / "Recovered Music";
    std::filesystem::create_directories(library, ignored);
    CHECK(!ignored);
    const auto recovered = library / "Restored Song.mp3";
    CHECK(std::filesystem::copy_file(
        first, recovered, std::filesystem::copy_options::overwrite_existing, ignored));
    CHECK(!ignored);
    const auto resolve = emberlights::resolve_audio_assets_in_directory(project, library);
    CHECK(resolve.complete());
    CHECK(resolve.files_examined == 1U);
    CHECK(resolve.hash_candidates == 1U);
    CHECK(resolve.matched_assets == 1U);
    CHECK(resolve.updated_assets == 1U);
    CHECK(resolve.matched_asset_ids.size() == 1U);
    CHECK(resolve.matched_asset_ids.front() == asset.id);
    CHECK(project.audio_assets.front().local_path_hint.find("Restored Song.mp3") != std::string::npos);
    CHECK(emberlights::verify_audio_asset(project.audio_assets.front()).available());
    const auto serialized = emberlights::serialize_project(project);
    emberlights::ProjectDocument parsed;
    CHECK(emberlights::parse_project(serialized, parsed));
    CHECK(parsed.audio_assets.size() == 1U);
    CHECK(parsed.audio_assets.front().sha256 == asset.sha256);
    CHECK(parsed.track_scripts.front().audio_asset_id == asset.id);
    CHECK(emberlights::serialize_project(parsed) == serialized);
    parsed.track_scripts.front().audio_asset_id = "missing-audio";
    CHECK(!emberlights::validate_project(parsed).ok());

    std::filesystem::remove_all(directory, ignored);
}

void test_project_validation_io_and_compilation() {
    auto project = make_test_project();
    project.connections.os2l_enabled = true;
    project.connections.os2l_bind = "127.0.0.1";
    project.connections.os2l_port = 10096U;
    project.connections.artnet_enabled = true;
    project.connections.artnet_destination = "192.0.2.10";
    project.connections.artnet_base = 20U;
    project.connections.sacn_enabled = true;
    project.connections.sacn_destination = "multicast";
    project.connections.sacn_universe_base = 101U;
    project.connections.frame_rate = 40U;
    project.connections.manual_bpm = 127.5;
    project.connections.midi_input_index = 3;
    project.connections.midi_output_index = 4;
    project.connections.dmx_usb_pro_ports = {"COM3", "COM4"};
    project.connections.soundswitch_micro_universe = 2U;
    project.connections.soundswitch_micro_framing =
        showcore::SoundSwitchMicroFraming::NativeJls1;
    project.connections.soundswitch_control_one_experimental = true;
    const auto validation = emberlights::validate_project(project);
    CHECK(validation.ok());

    auto invalid_usb_port = project;
    invalid_usb_port.connections.dmx_usb_pro_ports[0] = "device-path";
    CHECK(!emberlights::validate_project(invalid_usb_port).ok());
    auto duplicate_usb_port = project;
    duplicate_usb_port.connections.dmx_usb_pro_ports[1] = "com3";
    CHECK(!emberlights::validate_project(duplicate_usb_port).ok());
    auto fast_usb_output = project;
    fast_usb_output.connections.frame_rate = 41U;
    CHECK(!emberlights::validate_project(fast_usb_output).ok());
    auto control_one_warning = emberlights::validate_project(project);
    CHECK(control_one_warning.ok());
    CHECK(control_one_warning.warning_count() >= 1U);

    std::vector<emberlights::LookAssignmentDefinition> expanded;
    const auto fixture_target = emberlights::expand_look_target(
        project,
        "wash-1",
        showcore::Property::White,
        showcore::PropertyValue::set(0.5F),
        expanded);
    CHECK(fixture_target.target_found && fixture_target.assignments_added == 1U);
    const auto group_target = emberlights::expand_look_target(
        project,
        "dance",
        showcore::Property::Blue,
        showcore::PropertyValue::force_zero(),
        expanded);
    CHECK(group_target.target_found && group_target.assignments_added == 1U);
    CHECK(expanded.size() == 2U);
    CHECK(expanded[0].fixture_id == "wash-1");
    CHECK(expanded[1].fixture_id == "wash-1");
    CHECK(expanded[1].property == showcore::Property::Blue);
    CHECK(!emberlights::expand_look_target(
               project,
               "missing-target",
               showcore::Property::Intensity,
               showcore::PropertyValue::set(1.0F),
               expanded)
               .target_found);
    auto invalid_role = project;
    invalid_role.fixtures[0].roles.push_back("");
    CHECK(!emberlights::validate_project(invalid_role).ok());
    auto duplicate_role = project;
    duplicate_role.fixtures[0].roles.push_back("dance-floor");
    const auto duplicate_role_validation = emberlights::validate_project(duplicate_role);
    CHECK(duplicate_role_validation.ok());
    CHECK(duplicate_role_validation.warning_count() >= 1U);
    auto invalid_track = project;
    invalid_track.track_scripts[0].cues[1].target_ref = "missing-loop";
    CHECK(!emberlights::validate_project(invalid_track).ok());
    auto invalid_track_mapping = project;
    invalid_track_mapping.midi_mappings[0].target_ref = "missing-track";
    CHECK(!emberlights::validate_project(invalid_track_mapping).ok());
    auto invalid_group_mapping = project;
    invalid_group_mapping.midi_mappings[2].target_ref = "missing-group";
    CHECK(!emberlights::validate_project(invalid_group_mapping).ok());
    auto invalid_bank_mapping = project;
    invalid_bank_mapping.midi_mappings[3].action.target_id =
        static_cast<std::uint16_t>(showcore::kMaxAutoloopBanks);
    CHECK(!emberlights::validate_project(invalid_bank_mapping).ok());
    invalid_bank_mapping = project;
    invalid_bank_mapping.midi_mappings[4].action.target_id =
        static_cast<std::uint16_t>(showcore::kMaxAutoloopBanks);
    CHECK(!emberlights::validate_project(invalid_bank_mapping).ok());
    auto invalid_blackout_mapping = project;
    invalid_blackout_mapping.midi_mappings[6].target_ref = "missing-group";
    CHECK(!emberlights::validate_project(invalid_blackout_mapping).ok());

    const auto serialized = emberlights::serialize_project(project);
    emberlights::ProjectDocument parsed;
    const auto parsed_result = emberlights::parse_project(serialized, parsed);
    CHECK(parsed_result);
    CHECK(parsed.id == project.id);
    CHECK(parsed.name == project.name);
    CHECK(parsed.connections == project.connections);
    CHECK(parsed.fixture_profiles.size() == project.fixture_profiles.size());
    CHECK(parsed.fixtures.size() == 1U);
    CHECK(parsed.connections.dmx_usb_pro_ports[0] == "COM3");
    CHECK(parsed.connections.dmx_usb_pro_ports[1] == "COM4");
    CHECK(parsed.connections.soundswitch_micro_universe == 2U);
    CHECK(parsed.connections.soundswitch_micro_framing ==
          showcore::SoundSwitchMicroFraming::NativeJls1);
    CHECK(parsed.connections.soundswitch_control_one_experimental);
    CHECK(parsed.fixtures[0].roles.size() == 1U);
    CHECK(parsed.looks.size() == 2U);
    CHECK(parsed.autoloops.size() == 1U);
    CHECK(parsed.autoloops[0].bank == 7U && parsed.autoloops[0].slot == 3U);
    CHECK(parsed.track_scripts.size() == 1U);
    CHECK(parsed.track_scripts[0].audio_key == "audio:test-song");
    CHECK(parsed.track_scripts[0].cues.size() == 4U);
    CHECK(parsed.track_scripts[0].cues[1].action ==
        emberlights::TrackCueAction::TriggerAutoloop);
    CHECK(parsed.midi_mappings.size() == 7U);
    CHECK(parsed.midi_mappings[1].action.type == showcore::ActionType::ClearManualOverrides);
    CHECK(parsed.midi_mappings[2].action.type == showcore::ActionType::SetGroupProperty);
    CHECK(parsed.midi_mappings[3].action.type == showcore::ActionType::SelectAutoloopBank);
    CHECK(parsed.midi_mappings[3].action.target_id == 7U);
    CHECK(parsed.midi_mappings[4].action.type == showcore::ActionType::SetAutoloopBankEnabled);
    CHECK(parsed.midi_mappings[4].action.target_id == 2U);
    CHECK(parsed.midi_mappings[5].action.type == showcore::ActionType::SelectAllAutoloopBanks);
    CHECK(parsed.midi_mappings[6].action.type == showcore::ActionType::BlackoutGroup);
    CHECK(emberlights::serialize_project(parsed) == serialized);

    // Preview.310 persisted discovery candidates 0, 1, or 2. All three values
    // now load as the established native JLS1 contract without a format bump.
    auto legacy_micro_project = project;
    legacy_micro_project.connections.soundswitch_micro_framing =
        static_cast<showcore::SoundSwitchMicroFraming>(2U);
    emberlights::ProjectDocument migrated_micro_project;
    CHECK(emberlights::parse_project(
        emberlights::serialize_project(legacy_micro_project), migrated_micro_project));
    CHECK(migrated_micro_project.connections.soundswitch_micro_universe == 2U);
    CHECK(migrated_micro_project.connections.soundswitch_micro_framing ==
          showcore::SoundSwitchMicroFraming::NativeJls1);

    auto corrupted = serialized;
    corrupted.back() = corrupted.back() == '\n' ? 'X' : '\n';
    CHECK(emberlights::parse_project(corrupted, parsed).error ==
        emberlights::ProjectIoError::ChecksumMismatch);

    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    CHECK(compilation.show->fixture_count() == 1U);
    CHECK(compilation.show->look_count() == 2U);
    CHECK(compilation.show->autoloops().get({7, 3}) != nullptr);
    CHECK(compilation.show->track_script_count() == 1U);
    CHECK(compilation.show->midi_mappings().size() == 7U);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> clear_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::NoteOn, 0U, 25U, 127U},
              clear_events) == 1U);
    CHECK(clear_events[0].action.type == showcore::ActionType::ClearManualOverrides);
    CHECK(clear_events[0].active);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> group_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::ControlChange, 0U, 26U, 64U},
              group_events) == 1U);
    CHECK(group_events[0].action.type == showcore::ActionType::SetGroupProperty);
    CHECK(group_events[0].action.target_id == 0U);
    const auto* compiled_group = compilation.show->group(0U);
    CHECK(compiled_group != nullptr);
    CHECK(compiled_group->count == 1U && compiled_group->fixture_ids[0] == 0U);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> bank_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::NoteOn, 0U, 27U, 127U},
              bank_events) == 1U);
    CHECK(bank_events[0].action.type == showcore::ActionType::SelectAutoloopBank);
    CHECK(bank_events[0].action.target_id == 7U);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> enable_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::NoteOn, 0U, 28U, 127U},
              enable_events) == 1U);
    CHECK(enable_events[0].action.type == showcore::ActionType::SetAutoloopBankEnabled);
    CHECK(enable_events[0].action.target_id == 2U);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> all_bank_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::NoteOn, 0U, 29U, 127U},
              all_bank_events) == 1U);
    CHECK(all_bank_events[0].action.type == showcore::ActionType::SelectAllAutoloopBanks);
    std::array<showcore::MidiActionEvent, showcore::kMaxMidiActionsPerMessage> blackout_events{};
    CHECK(compilation.show->midi_mappings().process(
              {showcore::kAnyMidiDevice, showcore::MidiMessageType::NoteOn, 0U, 30U, 127U},
              blackout_events) == 1U);
    CHECK(blackout_events[0].action.type == showcore::ActionType::BlackoutGroup);
    CHECK(blackout_events[0].action.target_id == 0U);
    const auto* compiled_track = compilation.show->track_script(0U);
    CHECK(compiled_track != nullptr);
    CHECK(compiled_track->cue_count == 4U);
    CHECK(compiled_track->cues[0].target == 0U);
    CHECK(compiled_track->cues[1].target ==
        static_cast<std::uint16_t>(7U * showcore::kAutoloopsPerBank + 3U));
    auto look_player_storage = std::make_unique<showcore::StaticLookPlayer>();
    auto& look_player = *look_player_storage;
    CHECK(look_player.trigger(
        *compilation.show->look(0),
        0,
        compilation.show->look_fade_ms(0),
        compilation.show->engine().layers()));
    look_player.tick(750, compilation.show->engine().layers());
    compilation.show->engine().tick();
    CHECK(compilation.show->engine().frames().universes[0][0] == 255U);
    CHECK(compilation.show->engine().frames().universes[0][1] == 255U);
    CHECK(compilation.show->engine().frames().universes[0][2] == 0U);
    CHECK(compilation.show->engine().frames().universes[0][3] == 0U);

    const auto path = std::filesystem::path("build/project-io-test.emberlights");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(emberlights::project_backup_path(path), ignored);
    std::filesystem::remove(emberlights::project_active_path(path), ignored);
    std::filesystem::remove_all(emberlights::project_history_directory(path), ignored);
    CHECK(emberlights::save_project_atomic(path, project));
    project.name = "Second Save";
    CHECK(emberlights::save_project_atomic(path, project));
    CHECK(emberlights::load_project(path, parsed));
    CHECK(parsed.name == "Second Save");
    CHECK(emberlights::save_project_atomic(
        emberlights::project_active_path(path), parsed, false));
    emberlights::ProjectDocument active_snapshot;
    CHECK(emberlights::load_project(
        emberlights::project_active_path(path), active_snapshot, false));
    CHECK(active_snapshot.name == "Second Save");
    CHECK(!std::filesystem::exists(
        emberlights::project_history_directory(emberlights::project_active_path(path))));

    std::vector<emberlights::ProjectHistoryEntry> history;
    CHECK(emberlights::list_project_history(path, history));
    CHECK(history.size() == 2U);
    const auto first_version = std::find_if(
        history.begin(), history.end(), [](const emberlights::ProjectHistoryEntry& entry) {
            emberlights::ProjectDocument version;
            return emberlights::load_project(entry.path, version, false) && version.name == "Test Show";
        });
    CHECK(first_version != history.end());
    if (first_version != history.end()) {
        emberlights::ProjectDocument restored;
        CHECK(emberlights::restore_project_history(path, first_version->path, restored));
        CHECK(restored.name == "Test Show");
        CHECK(emberlights::load_project(path, parsed));
        CHECK(parsed.name == "Test Show");
    }
    emberlights::ProjectDocument rejected_restore;
    CHECK(!emberlights::restore_project_history(path, path, rejected_restore));
    {
        std::ofstream damage(path, std::ios::binary | std::ios::trunc);
        damage << "damaged";
    }
    const auto recovered = emberlights::load_project(path, parsed);
    CHECK(recovered && recovered.recovered_from_backup);
    CHECK(parsed.name == "Second Save");
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(emberlights::project_backup_path(path), ignored);
    std::filesystem::remove(emberlights::project_active_path(path), ignored);
    std::filesystem::remove_all(emberlights::project_history_directory(path), ignored);

    const auto history_path = std::filesystem::path("build/project-history-prune-test.emberlights");
    std::filesystem::remove(history_path, ignored);
    std::filesystem::remove(emberlights::project_backup_path(history_path), ignored);
    std::filesystem::remove_all(emberlights::project_history_directory(history_path), ignored);
    auto history_project = make_test_project();
    for (std::size_t index = 0; index < emberlights::kMaximumProjectHistoryEntries + 3U; ++index) {
        history_project.name = "Checkpoint " + std::to_string(index);
        CHECK(emberlights::save_project_atomic(history_path, history_project));
    }
    history.clear();
    CHECK(emberlights::list_project_history(history_path, history));
    CHECK(history.size() == emberlights::kMaximumProjectHistoryEntries);
    emberlights::ProjectDocument newest_history;
    CHECK(emberlights::load_project(history.front().path, newest_history, false));
    CHECK(newest_history.name == "Checkpoint " +
        std::to_string(emberlights::kMaximumProjectHistoryEntries + 2U));
    std::filesystem::remove(history_path, ignored);
    std::filesystem::remove(emberlights::project_backup_path(history_path), ignored);
    std::filesystem::remove_all(emberlights::project_history_directory(history_path), ignored);
}

void test_runner_os2l_startup_without_button_trigger() {
    auto project = make_test_project();
    auto alternate_loop = project.autoloops.front();
    alternate_loop.id = "alternate-red-blue";
    alternate_loop.name = "Alternate Red / Blue";
    alternate_loop.slot = 4U;
    project.autoloops.push_back(std::move(alternate_loop));
    project.connections.os2l_enabled = true;
    project.connections.os2l_bind = "127.0.0.1";
    project.connections.os2l_port = reserve_loopback_port();
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.dmx_usb_pro_ports = {};
    project.connections.soundswitch_micro_universe = 0U;
    CHECK(project.connections.os2l_port != 0U);
    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    if (!compilation || project.connections.os2l_port == 0U) {
        return;
    }

    emberlights::RunnerService runner;
    emberlights::RunnerOutputSnapshot unavailable_output;
    CHECK(!runner.latest_output_snapshot(unavailable_output));
    CHECK(runner.start(std::move(compilation.show), project));
    const auto listen_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    auto status = runner.status();
    while ((status.state != emberlights::RunnerState::Running ||
            status.os2l != emberlights::AdapterState::Waiting) &&
           std::chrono::steady_clock::now() < listen_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        status = runner.status();
    }
    CHECK(status.state == emberlights::RunnerState::Running);
    CHECK(status.os2l == emberlights::AdapterState::Waiting);
    CHECK(status.os2l_listen_port == project.connections.os2l_port);
    CHECK(status.os2l_last_error == 0);

    const auto client = connect_loopback(project.connections.os2l_port);
    CHECK(client != kInvalidTestSocket);
    if (client != kInvalidTestSocket) {
        constexpr std::string_view beat_only =
            R"({"evt":"beat","change":true,"pos":23,"bpm":137.25})";
        CHECK(send_all(client, beat_only));
        const auto beat_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        status = runner.status();
        while ((status.os2l_messages < 1U ||
                std::abs(status.bpm - 137.25) >= 0.01) &&
               std::chrono::steady_clock::now() < beat_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            status = runner.status();
        }
        CHECK(status.os2l == emberlights::AdapterState::Ready);
        CHECK(status.os2l_connections == 1U);
        CHECK(status.os2l_messages == 1U);
        CHECK(status.os2l_decode_errors == 0U);
        CHECK(std::abs(status.bpm - 137.25) < 0.01);
        CHECK(status.clock_source == showcore::ClockSource::Os2l);

        auto wait_for_live_state = [&](std::int32_t look,
                                       showcore::AutoloopAddress loop,
                                       std::uint64_t messages) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            status = runner.status();
            while ((status.active_look != look || status.active_autoloop != loop ||
                    status.os2l_messages < messages) &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                status = runner.status();
            }
            return status.active_look == look && status.active_autoloop == loop &&
                status.os2l_messages >= messages;
        };

        constexpr std::string_view red_on =
            R"({"evt":"btn","name":"Red","state":"on"})";
        constexpr std::string_view blue_on =
            R"({"evt":"btn","name":"Look: Blue","state":"on"})";
        constexpr std::string_view red_off =
            R"({"evt":"btn","name":"Red","state":"off"})";
        constexpr std::string_view blue_off =
            R"({"evt":"btn","name":"Look: Blue","state":"off"})";
        constexpr std::string_view alternate_on =
            R"({"evt":"btn","name":"Autoloop: Alternate Red / Blue","state":"on"})";
        constexpr std::string_view alternate_off =
            R"({"evt":"btn","name":"Autoloop: Alternate Red / Blue","state":"off"})";
        constexpr std::string_view keepalive =
            R"({"evt":"btn","name":"EmberLights Keepalive","state":"off"})";

        CHECK(send_all(client, red_on));
        CHECK(wait_for_live_state(0, {7U, 3U}, 2U));
        CHECK(send_all(client, blue_on));
        CHECK(wait_for_live_state(1, {7U, 3U}, 3U));
        CHECK(send_all(client, red_on));
        CHECK(wait_for_live_state(1, {7U, 3U}, 4U));
        CHECK(send_all(client, red_off));
        CHECK(wait_for_live_state(1, {7U, 3U}, 5U));
        CHECK(send_all(client, blue_off));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 6U));
        CHECK(send_all(client, red_on));
        CHECK(wait_for_live_state(0, {7U, 3U}, 7U));
        CHECK(send_all(client, red_off));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 8U));
        CHECK(send_all(client, alternate_on));
        CHECK(wait_for_live_state(-1, {7U, 4U}, 9U));
        CHECK(send_all(client, alternate_off));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 10U));
        constexpr std::string_view blackout_on =
            R"({"evt":"btn","name":"blackout","state":"on"})";
        constexpr std::string_view blackout_off =
            R"({"evt":"btn","name":"blackout","state":"off"})";
        CHECK(send_all(client, blackout_on));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 11U));
        CHECK(runner.status().blackout);
        CHECK(send_all(client, keepalive));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 12U));
        CHECK(runner.status().blackout);
        CHECK(send_all(client, blackout_off));
        CHECK(wait_for_live_state(-1, {7U, 3U}, 13U));
        CHECK(!runner.status().blackout);
        CHECK(status.dropped_os2l_actions == 0U);
        close_test_socket(client);
    }
    runner.stop();
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
    CHECK(runner.status().os2l_listen_port == 0U);
}

void test_runner_service_lifecycle() {
    emberlights::StaticLookBindingLease adapter_lease;
    CHECK(adapter_lease.record_begin(0x77U, 4U, 12U));
    CHECK(adapter_lease.outstanding());
    CHECK(!adapter_lease.record_begin(0x77U, 4U, 13U));
    const auto adapter_release = adapter_lease.consume_release(
        emberlights::StaticLookOwnerKind::Midi, 0x77U);
    CHECK(adapter_release.expected_package_generation == 4U);
    CHECK(adapter_release.expected_activation_generation == 12U);
    CHECK(!adapter_lease.outstanding());
    CHECK(adapter_lease.record_begin(0x77U, 4U, 13U));
    adapter_lease.clear();
    CHECK(!adapter_lease.outstanding());
    CHECK(adapter_lease.consume_release(
              emberlights::StaticLookOwnerKind::Midi, 0x77U)
              .expected_activation_generation == 0U);

    auto project = make_test_project();
    project.connections.os2l_enabled = false;
    project.connections.artnet_enabled = false;
    project.connections.sacn_enabled = false;
    project.connections.frame_rate = 40;
    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);

    emberlights::RunnerService runner;
    CHECK(runner.start(std::move(compilation.show), project));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (runner.status().state != emberlights::RunnerState::Running &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(runner.status().state == emberlights::RunnerState::Running);
    auto wait_for_output_snapshot = [&](auto predicate) {
        const auto output_deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        emberlights::RunnerOutputSnapshot output;
        bool available = false;
        while (std::chrono::steady_clock::now() < output_deadline) {
            available = runner.latest_output_snapshot(output);
            if (available && predicate(output)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(available);
        CHECK(predicate(output));
        return output;
    };
    const auto first_output = wait_for_output_snapshot([](const auto& output) {
        return output.generation == 1U && output.sequence != 0U &&
            output.rendered_at_ms != 0U && !output.blackout_applied;
    });
    CHECK(first_output.pre_blackout_frames.universes ==
          first_output.routed_frames.universes);
    CHECK(first_output.attribution.universes[0][0].fixture_id == 0U);
    CHECK(std::all_of(
        first_output.routes.begin(), first_output.routes.end(),
        [](const auto& route) {
            return !route.configured && route.attempted_frames == 0U &&
                route.accepted_frames == 0U;
        }));
    for (std::size_t sample = 0U; sample < 256U; ++sample) {
        emberlights::RunnerOutputSnapshot coherent;
        CHECK(runner.latest_output_snapshot(coherent));
        CHECK(coherent.generation == 1U);
        CHECK(coherent.sequence != 0U);
        CHECK(coherent.rendered_at_ms != 0U);
        if (coherent.blackout_applied) {
            CHECK(std::all_of(
                coherent.routed_frames.universes.begin(),
                coherent.routed_frames.universes.end(),
                [](const auto& universe) {
                    return std::all_of(
                        universe.begin(), universe.end(),
                        [](std::uint8_t value) { return value == 0U; });
                }));
        } else {
            CHECK(coherent.pre_blackout_frames.universes ==
                  coherent.routed_frames.universes);
        }
    }
    auto wait_for_active_look = [&](std::int32_t expected) {
        const auto look_deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        while (runner.status().active_look != expected &&
               std::chrono::steady_clock::now() < look_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return runner.status().active_look == expected;
    };
    auto wait_for_static_look = [&](auto predicate) {
        const auto look_deadline = std::chrono::steady_clock::now() +
            std::chrono::seconds(2);
        auto snapshot = runner.status();
        while (!predicate(snapshot.static_look) &&
               std::chrono::steady_clock::now() < look_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            snapshot = runner.status();
        }
        return snapshot;
    };
    const emberlights::StaticLookOwnerContext owner_a{
        emberlights::StaticLookOwnerKind::Test, 0xA1U, 0U, 0U};
    const emberlights::StaticLookOwnerContext owner_b{
        emberlights::StaticLookOwnerKind::Controller, 0xB2U, 0U, 0U};
    CHECK(!runner.hold_look(0U, true, {}));
    CHECK(!runner.hold_look(0U, true, {
        emberlights::StaticLookOwnerKind::Test, 0U, 0U, 0U}));
    CHECK(!runner.hold_look(0U, false, owner_a));

    CHECK(runner.trigger_look(0U, owner_a));
    CHECK(wait_for_active_look(0));
    auto explicit_activation = runner.status().static_look;
    CHECK(explicit_activation.look_index == 0);
    CHECK(explicit_activation.package_generation == 1U);
    CHECK(explicit_activation.activation_generation != 0U);
    CHECK(explicit_activation.owner_kind == emberlights::StaticLookOwnerKind::Test);
    CHECK(explicit_activation.owner_feedback_token == owner_a.feedback_token);
    CHECK(explicit_activation.behavior == emberlights::StaticLookBehavior::Explicit);
    CHECK(explicit_activation.status ==
              emberlights::StaticLookActivationStatus::Activating ||
          explicit_activation.status ==
              emberlights::StaticLookActivationStatus::Active);
    const auto status_allocations_before =
        g_allocations.load(std::memory_order_relaxed);
    for (std::size_t sample = 0U; sample < 4096U; ++sample) {
        const auto coherent = runner.status();
        CHECK(coherent.active_look == coherent.static_look.look_index);
        if (coherent.static_look.status ==
            emberlights::StaticLookActivationStatus::None) {
            CHECK(coherent.static_look.look_index == -1);
            CHECK(coherent.static_look.activation_generation == 0U);
        } else {
            CHECK(coherent.static_look.look_index >= 0);
            CHECK(coherent.static_look.package_generation != 0U);
            CHECK(coherent.static_look.activation_generation != 0U);
            CHECK(coherent.static_look.owner_kind !=
                  emberlights::StaticLookOwnerKind::None);
            CHECK(coherent.static_look.behavior !=
                  emberlights::StaticLookBehavior::None);
            CHECK(coherent.static_look.transition_progress >= 0.0F);
            CHECK(coherent.static_look.transition_progress <= 1.0F);
        }
    }
    CHECK(g_allocations.load(std::memory_order_relaxed) ==
          status_allocations_before);

    CHECK(runner.toggle_look(0U, owner_a));
    CHECK(wait_for_active_look(-1));
    CHECK(runner.toggle_look(0U, owner_a));
    CHECK(wait_for_active_look(0));
    const auto toggled = runner.status().static_look;
    CHECK(toggled.behavior == emberlights::StaticLookBehavior::Latch);
    CHECK(toggled.activation_generation >
          explicit_activation.activation_generation);

    CHECK(runner.hold_look(1U, true, owner_a));
    auto held_a = wait_for_static_look([](const auto& current) {
        return current.look_index == 1 &&
            current.behavior == emberlights::StaticLookBehavior::Hold &&
            current.owner_feedback_token == 0xA1U;
    }).static_look;
    CHECK(held_a.activation_generation > toggled.activation_generation);
    CHECK(runner.hold_look(1U, true, owner_a));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().static_look.activation_generation ==
          held_a.activation_generation);

    CHECK(runner.hold_look(1U, true, owner_b));
    auto held_b = wait_for_static_look([](const auto& current) {
        return current.look_index == 1 &&
            current.behavior == emberlights::StaticLookBehavior::Hold &&
            current.owner_feedback_token == 0xB2U;
    }).static_look;
    CHECK(held_b.activation_generation > held_a.activation_generation);

    auto stale_owner_a = owner_a;
    stale_owner_a.expected_package_generation = held_a.package_generation;
    stale_owner_a.expected_activation_generation =
        held_a.activation_generation;
    CHECK(runner.hold_look(1U, false, stale_owner_a));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().static_look.activation_generation ==
          held_b.activation_generation);
    CHECK(runner.status().static_look.owner_feedback_token ==
          owner_b.feedback_token);

    auto owner_b_release = owner_b;
    owner_b_release.expected_package_generation = held_b.package_generation;
    owner_b_release.expected_activation_generation =
        held_b.activation_generation;
    CHECK(runner.hold_look(1U, false, owner_b_release));
    CHECK(wait_for_active_look(-1));

    CHECK(runner.hold_look(0U, true, owner_a));
    held_a = wait_for_static_look([](const auto& current) {
        return current.look_index == 0 &&
            current.behavior == emberlights::StaticLookBehavior::Hold;
    }).static_look;
    CHECK(runner.hold_look(1U, true, owner_b));
    held_b = wait_for_static_look([](const auto& current) {
        return current.look_index == 1 &&
            current.owner_feedback_token == 0xB2U;
    }).static_look;
    stale_owner_a.expected_package_generation = held_a.package_generation;
    stale_owner_a.expected_activation_generation =
        held_a.activation_generation;
    CHECK(runner.hold_look(0U, false, stale_owner_a));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().static_look.activation_generation ==
          held_b.activation_generation);

    CHECK(runner.toggle_look(1U, owner_a));
    const auto latch_replacement = wait_for_static_look([](const auto& current) {
        return current.look_index == 1 &&
            current.behavior == emberlights::StaticLookBehavior::Latch;
    }).static_look;
    CHECK(latch_replacement.activation_generation >
          held_b.activation_generation);
    owner_b_release.expected_package_generation = held_b.package_generation;
    owner_b_release.expected_activation_generation =
        held_b.activation_generation;
    CHECK(runner.hold_look(1U, false, owner_b_release));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().static_look.activation_generation ==
          latch_replacement.activation_generation);

    CHECK(runner.clear_look());
    CHECK(wait_for_active_look(-1));

    const auto command_allocations_before =
        g_allocations.load(std::memory_order_relaxed);
    CHECK(runner.hold_look(0U, true, owner_a));
    const auto allocation_hold = wait_for_static_look([](const auto& current) {
        return current.look_index == 0 &&
            current.behavior == emberlights::StaticLookBehavior::Hold;
    }).static_look;
    auto allocation_release = owner_a;
    allocation_release.expected_package_generation =
        allocation_hold.package_generation;
    allocation_release.expected_activation_generation =
        allocation_hold.activation_generation;
    CHECK(runner.hold_look(0U, false, allocation_release));
    CHECK(wait_for_active_look(-1));
    CHECK(g_allocations.load(std::memory_order_relaxed) ==
          command_allocations_before);

    CHECK(runner.trigger_autoloop({7U, 3U}));
    const auto autoloop_deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    auto before_cover = runner.status();
    while ((before_cover.active_autoloop !=
                showcore::AutoloopAddress{7U, 3U} ||
            before_cover.active_autoloop_progress < 0.01F) &&
           std::chrono::steady_clock::now() < autoloop_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        before_cover = runner.status();
    }
    CHECK((before_cover.active_autoloop ==
           showcore::AutoloopAddress{7U, 3U}));
    CHECK(runner.hold_look(0U, true, owner_a));
    held_a = wait_for_static_look([](const auto& current) {
        return current.look_index == 0 &&
            current.behavior == emberlights::StaticLookBehavior::Hold;
    }).static_look;
    const auto covered_progress = runner.status().active_autoloop_progress;
    std::this_thread::sleep_for(std::chrono::milliseconds(125));
    const auto still_covered = runner.status();
    CHECK((still_covered.active_autoloop ==
           showcore::AutoloopAddress{7U, 3U}));
    CHECK(still_covered.active_autoloop_progress != covered_progress);
    stale_owner_a.expected_package_generation = held_a.package_generation;
    stale_owner_a.expected_activation_generation = held_a.activation_generation;
    CHECK(runner.hold_look(0U, false, stale_owner_a));
    CHECK(wait_for_active_look(-1));
    const auto revealed = runner.status();
    CHECK((revealed.active_autoloop ==
           showcore::AutoloopAddress{7U, 3U}));
    CHECK(revealed.active_autoloop_progress != before_cover.active_autoloop_progress);
    const auto all_banks = ~std::uint64_t{0};
    auto wait_for_bank_mask = [&](std::uint64_t expected) {
        const auto mask_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (runner.status().active_autoloop_bank_mask != expected &&
               std::chrono::steady_clock::now() < mask_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return runner.status().active_autoloop_bank_mask == expected;
    };
    CHECK(wait_for_bank_mask(all_banks));
    CHECK(runner.select_exclusive_autoloop_bank(7U));
    CHECK(wait_for_bank_mask(std::uint64_t{1} << 7U));
    CHECK(!runner.select_exclusive_autoloop_bank(showcore::kMaxAutoloopBanks));
    CHECK(runner.set_autoloop_bank_enabled(2U, true));
    CHECK(wait_for_bank_mask((std::uint64_t{1} << 7U) | (std::uint64_t{1} << 2U)));
    CHECK(runner.set_autoloop_bank_enabled(7U, false));
    CHECK(wait_for_bank_mask(std::uint64_t{1} << 2U));
    CHECK(!runner.set_autoloop_bank_enabled(showcore::kMaxAutoloopBanks, true));
    CHECK(runner.select_all_autoloop_banks());
    CHECK(wait_for_bank_mask(all_banks));
    auto wait_for_override_count = [&](std::uint16_t expected) {
        const auto override_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (runner.status().manual_override_count != expected &&
               std::chrono::steady_clock::now() < override_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return runner.status().manual_override_count == expected;
    };
    CHECK(runner.set_property(0U, showcore::Property::Intensity, 0.75F));
    CHECK(wait_for_override_count(1U));
    CHECK(runner.set_property(0U, showcore::Property::Red, 0.25F));
    CHECK(wait_for_override_count(2U));
    CHECK(runner.set_property(0U, showcore::Property::Intensity, 0.0F, false));
    CHECK(wait_for_override_count(1U));
    CHECK(runner.clear_manual_overrides());
    CHECK(wait_for_override_count(0U));
    showcore::FixtureGroup manual_group;
    CHECK(manual_group.add(0U));
    CHECK(runner.set_group_property(manual_group, showcore::Property::Blue, 0.5F));
    CHECK(wait_for_override_count(1U));
    CHECK(runner.set_group_property(manual_group, showcore::Property::Blue, 0.0F, false));
    CHECK(wait_for_override_count(0U));
    CHECK(!runner.set_group_property(manual_group, showcore::Property::Count, 0.5F));
    CHECK(!runner.set_property(0U, showcore::Property::Count, 0.5F));
    CHECK(runner.hold_look(0U, true, owner_a));
    CHECK(runner.trigger_autoloop({7, 3}));
    CHECK(runner.trigger_track_script(0));
    CHECK(runner.set_manual_bpm(128.0));
    runner.set_blackout(true);
    runner.set_work_light(true);
    const auto blacked_out_output = wait_for_output_snapshot([](const auto& output) {
        return output.generation == 1U && output.blackout_applied;
    });
    CHECK(std::any_of(
        blacked_out_output.pre_blackout_frames.universes[0].begin(),
        blacked_out_output.pre_blackout_frames.universes[0].end(),
        [](std::uint8_t value) { return value != 0U; }));
    CHECK(std::all_of(
        blacked_out_output.routed_frames.universes.begin(),
        blacked_out_output.routed_frames.universes.end(),
        [](const auto& universe) {
            return std::all_of(
                universe.begin(), universe.end(),
                [](std::uint8_t value) { return value == 0U; });
        }));
    CHECK(blacked_out_output.attribution.universes[0][0].fixture_id == 0U);
    std::this_thread::sleep_for(std::chrono::milliseconds(125));
    const auto active = runner.status();
    CHECK(active.frames >= 3U);
    CHECK(active.output_frames >= 1U);
    CHECK(active.active_look == 0);
    CHECK(active.static_look.behavior == emberlights::StaticLookBehavior::Hold);
    CHECK(active.static_look.package_generation == 1U);
    CHECK((active.active_autoloop == showcore::AutoloopAddress{7, 3}));
    CHECK(active.active_autoloop_repeat == showcore::AutoloopRepeat::Infinite);
    CHECK(active.active_autoloop_progress >= 0.0F && active.active_autoloop_progress <= 1.0F);
    CHECK(active.active_autoloop_completed_cycles <= 1U);
    CHECK(active.active_track_script == 0);
    CHECK(active.active_track_script_beat >= 0.0);
    CHECK(active.active_track_script_beat < 1.0);
    CHECK(active.active_track_script_consumed_cues == 1U);
    CHECK(active.blackout && active.work_light);
    CHECK(active.uptime_ms >= 100U);
    CHECK(active.last_frame_age_ms < 250U);
    CHECK(active.jitter_samples >= 3U);
    CHECK(active.deadline_misses <= active.jitter_samples);
    CHECK(active.artnet == emberlights::AdapterState::Disabled);
    CHECK(active.sacn == emberlights::AdapterState::Disabled);
    CHECK(active.dmx_usb_pro[0] == emberlights::AdapterState::Disabled);
    CHECK(active.dmx_usb_pro[1] == emberlights::AdapterState::Disabled);
    CHECK(active.soundswitch_micro == emberlights::AdapterState::Disabled);
    CHECK(active.soundswitch_control_one == emberlights::AdapterState::Disabled);
    CHECK(active.soundswitch_micro_write_frames == 0U);
    CHECK(active.soundswitch_micro_write_failures == 0U);
    CHECK(active.soundswitch_micro_last_error == 0U);
    CHECK(active.soundswitch_micro_last_nonzero_slots == 0U);
    CHECK(active.output_backends[0U].kind == showcore::OutputBackendKind::ArtNet);
    CHECK(active.output_backends[1U].kind == showcore::OutputBackendKind::Sacn);
    CHECK(active.output_backends[2U].kind == showcore::OutputBackendKind::DmxUsbPro);
    CHECK(active.output_backends[2U].first_source_universe == 1U);
    CHECK(active.output_backends[3U].kind == showcore::OutputBackendKind::DmxUsbPro);
    CHECK(active.output_backends[3U].first_source_universe == 2U);
    CHECK(active.output_backends[4U].kind ==
          showcore::OutputBackendKind::SoundSwitchMicro);
    CHECK(active.output_backends[5U].kind ==
          showcore::OutputBackendKind::SoundSwitchControlOne);
    CHECK(std::all_of(
        active.output_backends.begin(), active.output_backends.end(),
        [](const auto& output) {
            return !output.configured &&
                output.state == showcore::OutputHealthState::Disabled &&
                output.frames_attempted == 0U;
        }));
    CHECK(runner.clear_track_script());
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().active_track_script == -1);
    CHECK(runner.status().active_track_script_beat == 0.0);
    CHECK(runner.status().active_track_script_consumed_cues == 0U);
    CHECK(runner.select_exclusive_autoloop_bank(7U));
    CHECK(wait_for_bank_mask(std::uint64_t{1} << 7U));

    auto updated_project = project;
    updated_project.name = "Activated Test Show";
    updated_project.safety.max_intensity = 0.75F;
    updated_project.looks[0].name = "Activated Red";
    auto updated_compilation = emberlights::compile_project(updated_project);
    CHECK(updated_compilation);
    const auto activation = runner.activate(
        std::move(updated_compilation.show), updated_project, 2000U);
    CHECK(activation);
    CHECK(activation.generation == 2U);
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    const auto activated = runner.status();
    CHECK(activated.state == emberlights::RunnerState::Running);
    CHECK(activated.package_generation == 2U);
    CHECK(activated.package_activations == 1U);
    CHECK(activated.package_activation_failures == 0U);
    CHECK(activated.frames > active.frames);
    CHECK(activated.active_look == -1);
    CHECK(activated.static_look.status ==
          emberlights::StaticLookActivationStatus::None);
    CHECK((activated.active_autoloop == showcore::AutoloopAddress{7, 3}));
    CHECK(activated.active_autoloop_repeat == showcore::AutoloopRepeat::Infinite);
    CHECK(activated.active_autoloop_progress >= 0.0F && activated.active_autoloop_progress <= 1.0F);
    CHECK(activated.active_autoloop_bank_mask == (std::uint64_t{1} << 7U));
    CHECK(activated.manual_override_count == 0U);
    const auto activated_output = wait_for_output_snapshot([](const auto& output) {
        return output.generation == 2U;
    });
    CHECK(activated_output.blackout_applied);
    CHECK(activated_output.rendered_at_ms >=
          blacked_out_output.rendered_at_ms);

    CHECK(runner.hold_look(0U, true, owner_a));
    const auto new_package_hold = wait_for_static_look([](const auto& current) {
        return current.look_index == 0 && current.package_generation == 2U;
    }).static_look;
    CHECK(new_package_hold.activation_generation >
          active.static_look.activation_generation);
    auto old_package_release = owner_a;
    old_package_release.expected_package_generation =
        active.static_look.package_generation;
    old_package_release.expected_activation_generation =
        active.static_look.activation_generation;
    CHECK(runner.hold_look(0U, false, old_package_release));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(runner.status().static_look.activation_generation ==
          new_package_hold.activation_generation);
    CHECK(runner.status().static_look.package_generation == 2U);

    auto restart_project = updated_project;
    restart_project.connections.frame_rate = 39U;
    auto restart_compilation = emberlights::compile_project(restart_project);
    CHECK(restart_compilation);
    const auto restart_required = runner.activate(
        std::move(restart_compilation.show), restart_project, 2000U);
    CHECK(restart_required.error == emberlights::RunnerActivationError::RestartRequired);
    const auto still_active = runner.status();
    CHECK(still_active.state == emberlights::RunnerState::Running);
    CHECK(still_active.package_generation == 2U);
    CHECK(still_active.package_activations == 1U);
    CHECK(still_active.package_activation_failures == 1U);
    runner.stop();
    CHECK(runner.status().state == emberlights::RunnerState::Stopped);
    CHECK(runner.status().manual_override_count == 0U);
}

void test_runner_output_snapshot_route_results() {
    auto project = make_test_project();
    project.connections.os2l_enabled = false;
    project.connections.artnet_enabled = true;
    project.connections.artnet_destination = "127.0.0.1";
    project.connections.sacn_enabled = false;
    project.connections.frame_rate = 40U;
    auto compilation = emberlights::compile_project(project);
    CHECK(compilation);
    emberlights::RunnerService runner;
    CHECK(runner.start(std::move(compilation.show), project));

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(2);
    emberlights::RunnerOutputSnapshot snapshot;
    bool observed = false;
    while (std::chrono::steady_clock::now() < deadline) {
        if (runner.latest_output_snapshot(snapshot)) {
            const auto& route = snapshot.routes[0U];
            if (route.configured && route.attempted_frames == 2U &&
                route.accepted_frames == 2U) {
                observed = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(observed);
    CHECK(snapshot.routes[0U].kind == showcore::OutputBackendKind::ArtNet);
    CHECK(snapshot.routes[0U].first_source_universe == 1U);
    CHECK(snapshot.routes[0U].source_universe_count == 2U);
    CHECK(snapshot.routes[0U].last_error == 0U);
    CHECK(std::all_of(
        snapshot.routes.begin() + 1, snapshot.routes.end(),
        [](const auto& route) {
            return !route.configured && route.attempted_frames == 0U &&
                route.accepted_frames == 0U;
        }));
    runner.stop();
}

void test_soundswitch_read_only_inspection_and_bundle() {
    const auto source = std::filesystem::path("build/soundswitch-inspection-source.ssproj");
    const auto after = std::filesystem::path("build/soundswitch-inspection-after.ssproj");
    const auto bundle = std::filesystem::path("build/soundswitch-inspection-bundle");
    const auto report = std::filesystem::path("build/soundswitch-inspection-report.json");
    const auto comparison_report =
        std::filesystem::path("build/soundswitch-inspection-comparison.json");
    std::error_code ignored;
    std::filesystem::remove_all(source, ignored);
    std::filesystem::remove_all(after, ignored);
    std::filesystem::remove_all(bundle, ignored);
    std::filesystem::remove(report, ignored);
    std::filesystem::remove(comparison_report, ignored);
    std::filesystem::create_directories(source / "recordable", ignored);
    CHECK(!ignored);
    auto write_file = [](const std::filesystem::path& path, std::string_view bytes) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    };
    CHECK(write_file(source / ".ssproj", "{\"project\":\"test\"}\n"));
    CHECK(write_file(source / "SoundSwitchVenues.bin", "venue"));
    CHECK(write_file(
        source / "01234567-89ab-cdef-0123-456789abcdef.ssfile",
        std::string_view{"\xAA\xAA\x09\x55payload", 11U}));
    CHECK(write_file(source / "recordable" / "opaque.dat", "abc"));
    CHECK(write_file(source / "future.payload", "preserve-me"));

    const auto inspection = emberlights::inspect_soundswitch_project(source);
    CHECK(inspection.complete());
    CHECK(inspection.format_version == 2U);
    CHECK(inspection.source_kind == emberlights::SoundSwitchSourceKind::ExportedProject);
    CHECK(inspection.inventory_format == "emberlights-soundswitch-inventory");
    CHECK(inspection.inventory_format_version == 1U);
    CHECK(inspection.inventory_sha256.size() == 64U);
    CHECK(inspection.artifacts.size() == 5U);
    CHECK(inspection.total_bytes == 49U);
    CHECK(inspection.known_artifacts == 4U);
    CHECK(inspection.unknown_artifacts == 1U);
    CHECK(inspection.recognized_ssfiles == 1U);
    const auto opaque = std::find_if(
        inspection.artifacts.begin(), inspection.artifacts.end(), [](const auto& artifact) {
            return artifact.relative_path == "recordable/opaque.dat";
        });
    CHECK(opaque != inspection.artifacts.end());
    if (opaque != inspection.artifacts.end()) {
        CHECK(opaque->sha256 ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    }
    const auto serialized = emberlights::serialize_soundswitch_inspection(inspection);
    CHECK(serialized.find("emberlights-soundswitch-inspection") != std::string::npos);
    CHECK(serialized.find("\"sourceKind\": \"exportedProject\"") != std::string::npos);
    CHECK(serialized.find("relativePathUtf8Bytewise") != std::string::npos);
    CHECK(serialized.find(inspection.inventory_sha256) != std::string::npos);
    CHECK(serialized.find("recognizedSsfileHeader\": true") != std::string::npos);
    CHECK(serialized.find("sourceRoot") == std::string::npos);
    CHECK(serialized.find(inspection.source_root) == std::string::npos);
    std::string report_error;
    CHECK(emberlights::save_soundswitch_inspection_atomic(
        report, inspection, report_error));
    CHECK(std::filesystem::file_size(report, ignored) > 0U);

    std::filesystem::create_directories(after, ignored);
    CHECK(!ignored);
    CHECK(write_file(after / ".ssproj", "{\"project\":\"test\"}\n"));
    CHECK(write_file(after / "SoundSwitchVenues.bin", "venue-updated"));
    CHECK(write_file(
        after / "01234567-89ab-cdef-0123-456789abcdef.ssfile",
        std::string_view{"\xAA\xAA\x09\x55payload", 11U}));
    CHECK(write_file(after / "future.payload", "preserve-me"));
    CHECK(write_file(after / "SoundSwitchTrackMap.bin", "track-map"));

    const auto comparison = emberlights::compare_soundswitch_projects(source, after);
    CHECK(comparison.complete());
    CHECK(comparison.artifacts.size() == 6U);
    CHECK(comparison.unchanged_artifacts == 3U);
    CHECK(comparison.added_artifacts == 1U);
    CHECK(comparison.removed_artifacts == 1U);
    CHECK(comparison.modified_artifacts == 1U);
    const auto changed_venues = std::find_if(
        comparison.artifacts.begin(), comparison.artifacts.end(), [](const auto& artifact) {
            return artifact.relative_path == "SoundSwitchVenues.bin";
        });
    CHECK(changed_venues != comparison.artifacts.end());
    if (changed_venues != comparison.artifacts.end()) {
        CHECK(changed_venues->change == emberlights::SoundSwitchArtifactChange::Modified);
        CHECK(changed_venues->before_size == 5U);
        CHECK(changed_venues->after_size == 13U);
        CHECK(changed_venues->changed_bytes >= 8U);
        CHECK(!changed_venues->changed_ranges.empty());
    }
    const auto comparison_json = emberlights::serialize_soundswitch_comparison(comparison);
    CHECK(comparison_json.find("emberlights-soundswitch-comparison") != std::string::npos);
    CHECK(comparison_json.find("\"formatVersion\": 2") != std::string::npos);
    CHECK(comparison_json.find("\"change\": \"modified\"") != std::string::npos);
    CHECK(comparison_json.find("venue-updated") == std::string::npos);
    CHECK(comparison_json.find("beforeSourceRoot") == std::string::npos);
    CHECK(comparison_json.find("afterSourceRoot") == std::string::npos);
    CHECK(comparison_json.find(comparison.before.source_root) == std::string::npos);
    CHECK(comparison_json.find(comparison.after.source_root) == std::string::npos);
    CHECK(emberlights::save_soundswitch_comparison_atomic(
        comparison_report, comparison, report_error));
    CHECK(std::filesystem::file_size(comparison_report, ignored) > 0U);

    const auto bundled = emberlights::create_soundswitch_source_bundle(source, bundle);
    CHECK(bundled);
    CHECK(std::filesystem::is_regular_file(bundle / "inventory.json"));
    CHECK(std::filesystem::is_regular_file(
        bundle / "payload" / "01234567-89ab-cdef-0123-456789abcdef.ssfile"));
    CHECK(std::filesystem::is_regular_file(bundle / "payload" / "future.payload"));
    {
        std::ifstream inventory(bundle / "inventory.json", std::ios::binary);
        std::string line;
        bool leaked_source_root = false;
        while (std::getline(inventory, line)) {
            leaked_source_root = leaked_source_root ||
                line.find(inspection.source_root) != std::string::npos;
        }
        CHECK(static_cast<bool>(inventory) || inventory.eof());
        CHECK(!leaked_source_root);
    }
    CHECK(write_file(source / "still-writable-after-inspection.txt", "untouched"));

    const auto duplicate = emberlights::create_soundswitch_source_bundle(source, bundle);
    CHECK(duplicate.error == emberlights::SoundSwitchBundleError::DestinationExists);

    std::filesystem::remove_all(source, ignored);
    std::filesystem::remove_all(after, ignored);
    std::filesystem::remove_all(bundle, ignored);
    std::filesystem::remove(report, ignored);
    std::filesystem::remove(comparison_report, ignored);
}

void test_soundswitch_source_binding_audit() {
    constexpr std::string_view claimed_venue =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    constexpr std::string_view claimed_loops =
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
    constexpr std::string_view available_venue =
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
    constexpr std::string_view available_loops =
        "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";

    auto project = emberlights::make_safe_color_rig_v1_template();
    project.unknown_records.push_back(
        "SOUNDSWITCH_SOURCE\t2.10.x\t{manifest}\t" +
        std::string(claimed_venue) + "\t" + std::string(claimed_loops) +
        "\tsemantic-v1-safe-patch");
    emberlights::SoundSwitchInspection inspection;
    inspection.source_kind = emberlights::SoundSwitchSourceKind::ApplicationDataBackup;
    inspection.inventory_sha256 =
        "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
    inspection.artifacts = {
        {"SoundSwitchVenues.bin", emberlights::SoundSwitchArtifactKind::VenueDatabase,
         10U, std::string(available_venue), false, false},
        {"SoundSwitchAutoLoops.bin", emberlights::SoundSwitchArtifactKind::AutoloopDatabase,
         11U, std::string(available_loops), false, false},
        {"track.ssfile", emberlights::SoundSwitchArtifactKind::TrackScript,
         12U, std::string(claimed_venue), false, true},
        {"Autoloops/1.ssfile", emberlights::SoundSwitchArtifactKind::AutoloopScript,
         13U, std::string(claimed_loops), false, true},
        {"Autoloops/1.ssfile.bak", emberlights::SoundSwitchArtifactKind::AutoloopScript,
         14U, std::string(available_venue), true, true},
        {"Fixture.plfix", emberlights::SoundSwitchArtifactKind::FixturePersonality,
         15U, std::string(available_loops), false, false}};

    const auto mismatch = emberlights::audit_soundswitch_source_binding(
        project, inspection);
    CHECK(mismatch.status ==
          emberlights::SoundSwitchSourceBindingStatus::SourceMismatch);
    CHECK(!mismatch.exact_artifact_hash_match);
    CHECK(!mismatch.semantic_import_qualified);
    CHECK(mismatch.available_backup_count == 1U);
    CHECK(mismatch.available_track_script_count == 1U);
    CHECK(mismatch.available_autoloop_script_count == 1U);
    CHECK(mismatch.available_fixture_personality_count == 1U);
    CHECK(mismatch.project_valid);
    CHECK(mismatch.outputs_disabled);
    CHECK(mismatch.review_state ==
          emberlights::SoundSwitchMigrationReviewState::SourceEvidenceBlocked);
    CHECK(std::find(
              mismatch.review_action_codes.begin(),
              mismatch.review_action_codes.end(),
              "migration.provide_matching_source") !=
          mismatch.review_action_codes.end());
    const auto patch_review = std::find_if(
        mismatch.review_areas.begin(), mismatch.review_areas.end(),
        [](const auto& area) { return area.area_id == "fixturePatch"; });
    CHECK(patch_review != mismatch.review_areas.end());
    if (patch_review != mismatch.review_areas.end()) {
        CHECK(patch_review->state ==
              emberlights::SoundSwitchMigrationAreaState::Approximated);
        CHECK(patch_review->project_item_count == project.fixtures.size());
    }
    const auto track_review = std::find_if(
        mismatch.review_areas.begin(), mismatch.review_areas.end(),
        [](const auto& area) { return area.area_id == "trackScripts"; });
    CHECK(track_review != mismatch.review_areas.end());
    if (track_review != mismatch.review_areas.end()) {
        CHECK(track_review->state ==
              emberlights::SoundSwitchMigrationAreaState::SourceEvidenceOnly);
        CHECK(track_review->source_item_count == 1U);
        CHECK(track_review->project_item_count == 0U);
    }

    auto evidenced = project;
    emberlights::record_soundswitch_source_binding_evidence(
        evidenced, mismatch,
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    const auto evidence_serialized = emberlights::serialize_project(evidenced);
    CHECK(evidence_serialized.find("SOUNDSWITCH_AVAILABLE_SOURCE") != std::string::npos);
    CHECK(evidence_serialized.find("sourceMismatch") != std::string::npos);
    CHECK(evidence_serialized.find("SOUNDSWITCH_ARCHIVE_SHA256") != std::string::npos);
    const auto records_before = evidenced.unknown_records.size();
    emberlights::record_soundswitch_source_binding_evidence(
        evidenced, mismatch,
        "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    CHECK(evidenced.unknown_records.size() == records_before);

    const auto report = emberlights::serialize_soundswitch_source_binding_audit(
        mismatch,
        "project.emberlights",
        std::string(claimed_venue),
        "SoundSwitch extracted backup",
        std::string(claimed_loops));
    CHECK(report.find("\"status\": \"sourceMismatch\"") != std::string::npos);
    CHECK(report.find("\"semanticImportQualified\": false") != std::string::npos);
    CHECK(report.find("\"trackScriptCount\": 1") != std::string::npos);
    CHECK(report.find("\"state\": \"sourceEvidenceBlocked\"") !=
          std::string::npos);
    CHECK(report.find("\"id\": \"fixturePatch\"") != std::string::npos);
    CHECK(report.find("\"state\": \"approximated\"") != std::string::npos);
    CHECK(report.find("safe non-overlapping staging layout") !=
          std::string::npos);
    CHECK(report.find("PRIVATE_SOURCE_BYTES") == std::string::npos);

    inspection.artifacts[0].sha256 = std::string(claimed_venue);
    inspection.artifacts[1].sha256 = std::string(claimed_loops);
    const auto exact = emberlights::audit_soundswitch_source_binding(project, inspection);
    CHECK(exact.status ==
          emberlights::SoundSwitchSourceBindingStatus::ExactArtifactHashMatch);
    CHECK(exact.exact_artifact_hash_match);
    CHECK(!exact.semantic_import_qualified);
    CHECK(exact.review_state ==
          emberlights::SoundSwitchMigrationReviewState::ReadyForManualReview);
    CHECK(exact.review_headline.find("semantic import is still not qualified") !=
          std::string::npos);

    auto output_enabled = project;
    output_enabled.connections.artnet_enabled = true;
    const auto unsafe_review = emberlights::audit_soundswitch_source_binding(
        output_enabled, inspection);
    CHECK(unsafe_review.review_state ==
          emberlights::SoundSwitchMigrationReviewState::OutputMustBeDisabled);
    CHECK(!unsafe_review.outputs_disabled);
    CHECK(std::find(
              unsafe_review.review_action_codes.begin(),
              unsafe_review.review_action_codes.end(),
              "migration.disable_output_before_review") !=
          unsafe_review.review_action_codes.end());

    auto invalid_project = project;
    invalid_project.id.clear();
    const auto invalid_review = emberlights::audit_soundswitch_source_binding(
        invalid_project, inspection);
    CHECK(invalid_review.review_state ==
          emberlights::SoundSwitchMigrationReviewState::ProjectValidationBlocked);
    CHECK(!invalid_review.project_valid);
    CHECK(invalid_review.project_validation_error_count > 0U);

    project.unknown_records.push_back(
        "SOUNDSWITCH_SOURCE\tduplicate\t{x}\t" + std::string(claimed_venue) +
        "\t" + std::string(claimed_loops) + "\tduplicate");
    const auto ambiguous = emberlights::audit_soundswitch_source_binding(
        project, inspection);
    CHECK(ambiguous.status ==
          emberlights::SoundSwitchSourceBindingStatus::ProjectClaimMalformed);
}

void test_soundswitch_application_data_backup_inspection_and_bundle() {
    const auto source = std::filesystem::path("build/soundswitch-appdata-source");
    const auto reordered = std::filesystem::path("build/soundswitch-appdata-reordered");
    const auto incomplete = std::filesystem::path("build/soundswitch-appdata-incomplete");
    const auto bundle = std::filesystem::path("build/soundswitch-appdata-bundle");
    const auto incomplete_bundle =
        std::filesystem::path("build/soundswitch-appdata-incomplete-bundle");
    std::error_code ignored;
    for (const auto& path : {source, reordered, incomplete, bundle, incomplete_bundle}) {
        std::filesystem::remove_all(path, ignored);
    }
    std::filesystem::create_directories(source, ignored);
    CHECK(!ignored);
    std::filesystem::create_directories(reordered, ignored);
    CHECK(!ignored);
    std::filesystem::create_directories(incomplete, ignored);
    CHECK(!ignored);

    auto write_file = [](const std::filesystem::path& path, std::string_view bytes) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    };
    const std::vector<std::pair<std::filesystem::path, std::string>> files{
        {"future.payload", "preserve-me"},
        {"Unknown-6x12w 6in1 LED.plfix", "fixture-personality"},
        {"SSAutoLoop1.ssfile.bak", std::string{"\xAA\xAA\x09\x55old-loop", 12U}},
        {"SoundSwitchAutoLoops.bin.bak", "old-autoloop-db"},
        {"SSAutoLoop1.ssfile", std::string{"\xAA\xAA\x09\x55new-loop", 12U}},
        {"SoundSwitchTrackMap.bin", "track-map"},
        {"SoundSwitchAutoLoops.bin", "autoloop-db"},
        {"SoundSwitchVenues.bin", "venues-db"}};
    for (const auto& [relative, bytes] : files) {
        CHECK(write_file(source / relative, bytes));
    }
    for (auto iterator = files.rbegin(); iterator != files.rend(); ++iterator) {
        CHECK(write_file(reordered / iterator->first, iterator->second));
    }

    const auto inspection = emberlights::inspect_soundswitch_project(source);
    CHECK(inspection.complete());
    CHECK(inspection.source_kind ==
        emberlights::SoundSwitchSourceKind::ApplicationDataBackup);
    CHECK(inspection.format_version == 2U);
    CHECK(inspection.inventory_format == "emberlights-soundswitch-inventory");
    CHECK(inspection.inventory_format_version == 1U);
    CHECK(inspection.inventory_sha256.size() == 64U);
    CHECK(inspection.artifacts.size() == files.size());
    CHECK(inspection.known_artifacts == 7U);
    CHECK(inspection.unknown_artifacts == 1U);
    CHECK(inspection.recognized_ssfiles == 2U);
    CHECK(std::is_sorted(
        inspection.artifacts.begin(), inspection.artifacts.end(),
        [](const auto& first, const auto& second) {
            return first.relative_path < second.relative_path;
        }));
    CHECK(std::any_of(
        inspection.issues.begin(), inspection.issues.end(), [](const auto& issue) {
            return issue.code == "format.applicationDataBackup";
        }));

    const auto find_artifact = [&](std::string_view path) {
        return std::find_if(
            inspection.artifacts.begin(), inspection.artifacts.end(),
            [path](const auto& artifact) { return artifact.relative_path == path; });
    };
    const auto personality = find_artifact("Unknown-6x12w 6in1 LED.plfix");
    CHECK(personality != inspection.artifacts.end());
    if (personality != inspection.artifacts.end()) {
        CHECK(personality->kind == emberlights::SoundSwitchArtifactKind::FixturePersonality);
        CHECK(!personality->is_backup);
    }
    const auto database_backup = find_artifact("SoundSwitchAutoLoops.bin.bak");
    CHECK(database_backup != inspection.artifacts.end());
    if (database_backup != inspection.artifacts.end()) {
        CHECK(database_backup->kind == emberlights::SoundSwitchArtifactKind::AutoloopDatabase);
        CHECK(database_backup->is_backup);
    }
    const auto script_backup = find_artifact("SSAutoLoop1.ssfile.bak");
    CHECK(script_backup != inspection.artifacts.end());
    if (script_backup != inspection.artifacts.end()) {
        CHECK(script_backup->kind == emberlights::SoundSwitchArtifactKind::AutoloopScript);
        CHECK(script_backup->is_backup);
        CHECK(script_backup->recognized_ssfile_header);
    }

    const auto same_content = emberlights::inspect_soundswitch_project(reordered);
    CHECK(same_content.complete());
    CHECK(same_content.source_kind == inspection.source_kind);
    CHECK(same_content.inventory_sha256 == inspection.inventory_sha256);
    CHECK(emberlights::serialize_soundswitch_inspection(same_content) ==
        emberlights::serialize_soundswitch_inspection(inspection));
    CHECK(write_file(reordered / "future.payload", "changed"));
    const auto changed_content = emberlights::inspect_soundswitch_project(reordered);
    CHECK(changed_content.complete());
    CHECK(changed_content.inventory_sha256 != inspection.inventory_sha256);

    const auto serialized = emberlights::serialize_soundswitch_inspection(inspection);
    CHECK(serialized.find("\"formatVersion\": 2") != std::string::npos);
    CHECK(serialized.find("\"sourceKind\": \"applicationDataBackup\"") !=
        std::string::npos);
    CHECK(serialized.find("emberlights-soundswitch-inventory") != std::string::npos);
    CHECK(serialized.find("\"backup\": true") != std::string::npos);
    CHECK(serialized.find(inspection.inventory_sha256) != std::string::npos);
    CHECK(serialized.find(inspection.source_root) == std::string::npos);

    const auto bundled = emberlights::create_soundswitch_source_bundle(source, bundle);
    CHECK(bundled);
    CHECK(bundled.inspection.inventory_sha256 == inspection.inventory_sha256);
    CHECK(std::filesystem::is_regular_file(bundle / "inventory.json"));
    for (const auto& [relative, _] : files) {
        CHECK(std::filesystem::is_regular_file(bundle / "payload" / relative));
    }
    const auto source_after_bundle = emberlights::inspect_soundswitch_project(source);
    CHECK(source_after_bundle.inventory_sha256 == inspection.inventory_sha256);

    CHECK(write_file(incomplete / "Example.ssproj.bak", "manifest-backup"));
    CHECK(write_file(incomplete / "SoundSwitchVenues.bin", "venues-db"));
    CHECK(write_file(incomplete / "SoundSwitchAutoLoops.bin", "autoloop-db"));
    const auto incomplete_inspection =
        emberlights::inspect_soundswitch_project(incomplete);
    CHECK(!incomplete_inspection.complete());
    CHECK(incomplete_inspection.source_kind == emberlights::SoundSwitchSourceKind::Unknown);
    CHECK(std::any_of(
        incomplete_inspection.issues.begin(), incomplete_inspection.issues.end(),
        [](const auto& issue) { return issue.code == "format.sourceLayoutUnknown"; }));
    const auto manifest_backup = std::find_if(
        incomplete_inspection.artifacts.begin(), incomplete_inspection.artifacts.end(),
        [](const auto& artifact) {
            return artifact.relative_path == "Example.ssproj.bak";
        });
    CHECK(manifest_backup != incomplete_inspection.artifacts.end());
    if (manifest_backup != incomplete_inspection.artifacts.end()) {
        CHECK(manifest_backup->kind ==
            emberlights::SoundSwitchArtifactKind::ProjectManifest);
        CHECK(manifest_backup->is_backup);
    }
    const auto refused = emberlights::create_soundswitch_source_bundle(
        incomplete, incomplete_bundle);
    CHECK(refused.error == emberlights::SoundSwitchBundleError::InspectionFailed);
    CHECK(!std::filesystem::exists(incomplete_bundle));

    for (const auto& path : {source, reordered, incomplete, bundle, incomplete_bundle}) {
        std::filesystem::remove_all(path, ignored);
    }
}

void test_ir4_fixture_profile_upgrade() {
    using showcore::ChannelEncoding;
    using showcore::FixtureProfileSource;
    using showcore::Property;

    const auto six_channel = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    const auto ten_channel = emberlights::make_both_lighting_bo_ir4_10ch_profile();
    const auto check_manual_profile = [](
        const emberlights::FixtureProfileDefinition& profile,
        const auto& expected_properties) {
        CHECK(profile.source == FixtureProfileSource::BuiltIn);
        CHECK(profile.source_revision == emberlights::kBothLightingBoIr4ManualRevision);
        CHECK(profile.footprint == expected_properties.size());
        CHECK(profile.channels.size() == expected_properties.size());
        for (std::size_t index = 0U;
             index < profile.channels.size() && index < expected_properties.size();
             ++index) {
            const auto& definition = profile.channels[index];
            CHECK(definition.property == expected_properties[index]);
            CHECK(definition.coarse_offset == index);
            CHECK(definition.fine_offset == -1);
            CHECK(definition.encoding == ChannelEncoding::Linear8);
            CHECK(definition.dmx_min == 0U);
            CHECK(definition.dmx_max == 255U);
            CHECK(definition.default_value == 0U);
        }
    };
    constexpr std::array<Property, 6U> expected_six{{
        Property::Red,
        Property::Green,
        Property::Blue,
        Property::White,
        Property::Amber,
        Property::UV}};
    CHECK(six_channel.id == emberlights::kBothLightingBoIr4SixChannelProfileId);
    CHECK(ten_channel.id == emberlights::kBothLightingBoIr4TenChannelProfileId);
    CHECK(six_channel.mode.find("manual-matched") != std::string::npos);
    CHECK(ten_channel.mode.find("manual-matched") != std::string::npos);
    check_manual_profile(six_channel, expected_six);
    CHECK(ten_channel.footprint == 10U);
    CHECK(ten_channel.channels.size() == 10U);
    constexpr std::array<Property, 7U> expected_ten_linear{{
        Property::Intensity, Property::Red, Property::Green, Property::Blue,
        Property::White, Property::Amber, Property::UV}};
    for (std::size_t index = 0U; index < expected_ten_linear.size(); ++index) {
        CHECK(ten_channel.channels[index].property == expected_ten_linear[index]);
        CHECK(ten_channel.channels[index].encoding == ChannelEncoding::Linear8);
        CHECK(ten_channel.channels[index].coarse_offset == index);
    }
    CHECK(ten_channel.channels[7].property == Property::Strobe);
    CHECK(ten_channel.channels[7].encoding == ChannelEncoding::Ranged8);
    CHECK(ten_channel.channels[7].dmx_min == 1U);
    CHECK(ten_channel.channels[7].dmx_max == 255U);
    CHECK(ten_channel.channels[7].default_value == 0U);
    for (std::size_t index = 8U; index < 10U; ++index) {
        CHECK(ten_channel.channels[index].property == Property::Count);
        CHECK(ten_channel.channels[index].encoding == ChannelEncoding::Constant8);
        CHECK(ten_channel.channels[index].default_value == 0U);
    }

    const auto make_stale_profile = [] {
        emberlights::FixtureProfileDefinition profile;
        profile.id = "soundswitch.both-lighting.bo-ir4.mode1";
        profile.manufacturer = "Both Lighting";
        profile.model = "BO-IR4 LED Mini Spotlight";
        profile.mode = "Mode 1";
        profile.name = "Both Lighting BO-IR4 LED Mini Spotlight (Mode 1)";
        profile.source = FixtureProfileSource::Local;
        profile.source_revision = "soundswitch-2.10.3-safe-v1";
        constexpr std::array<Property, 10U> properties{{
            Property::Intensity,
            Property::Red,
            Property::Green,
            Property::Blue,
            Property::Amber,
            Property::White,
            Property::UV,
            Property::Strobe,
            Property::Custom1,
            Property::Custom2}};
        profile.footprint = static_cast<std::uint16_t>(properties.size());
        for (std::size_t index = 0U; index < properties.size(); ++index) {
            profile.channels.push_back({
                properties[index],
                static_cast<std::uint16_t>(index),
                -1,
                ChannelEncoding::Linear8,
                0U,
                255U,
                0U});
        }
        return profile;
    };
    const auto make_upgrade_project = [&] {
        auto project = emberlights::make_starter_project();
        project.id = "ir4-profile-upgrade-test";
        project.name = "IR-4 Profile Upgrade Preservation Test";
        project.connections.artnet_destination = "192.0.2.45";
        project.safety.max_intensity = 0.63F;
        project.fixture_profiles.push_back(make_stale_profile());
        project.fixtures.push_back({
            "ir4-a", "IR-4 A", "soundswitch.both-lighting.bo-ir4.mode1",
            1U, 1U, {"spotlight", "keep-role"}});
        project.fixtures.push_back({
            "unrelated", "Unrelated Dimmer", "builtin.generic.dimmer-1ch",
            1U, 40U, {"unrelated"}});
        project.fixtures.push_back({
            "ir4-b", "IR-4 B", "soundswitch.both-lighting.bo-ir4.mode1",
            1U, 20U, {"spotlight"}});
        project.groups.push_back({
            "group.keep", "Preserved Group", {"ir4-a", "unrelated", "ir4-b"}});
        project.looks.push_back({
            "look.keep", "Preserved Look", 432U,
            {{"unrelated", Property::Intensity, showcore::PropertyValue::set(0.42F)}}});
        project.autoloops.push_back({
            "loop.keep", "Preserved Loop", 3U, 7U, 4.0F,
            showcore::AutoloopRepeat::Infinite,
            {{0.0F, "look.keep", showcore::AutoloopTransition::Cut}}});
        project.unknown_records.push_back("FUTURE_KEEP\topaque-value");
        return project;
    };

    auto exact_project = make_upgrade_project();
    CHECK(emberlights::validate_project(exact_project).ok());
    const auto exact_plan = emberlights::plan_known_fixture_profile_upgrades(exact_project);
    CHECK(exact_plan.changes.size() == 1U);
    if (!exact_plan.empty()) {
        CHECK(exact_plan.changes[0].source_profile_id ==
            "soundswitch.both-lighting.bo-ir4.mode1");
        CHECK(exact_plan.changes[0].replacement_profile_id == ten_channel.id);
        CHECK(exact_plan.changes[0].affected_fixture_ids ==
            std::vector<std::string>({"ir4-a", "ir4-b"}));
    }

    const auto stale_index = exact_plan.empty()
        ? 0U
        : exact_plan.changes[0].source_profile_index;
    const auto expect_signature_refused = [&](const auto& mutation) {
        auto candidate = make_upgrade_project();
        mutation(candidate.fixture_profiles.back());
        CHECK(emberlights::plan_known_fixture_profile_upgrades(candidate).empty());
    };
    expect_signature_refused([](auto& profile) { profile.id += ".copy"; });
    expect_signature_refused([](auto& profile) {
        profile.mode = "Mode 1 (10 channel)";
    });
    expect_signature_refused([](auto& profile) { profile.name += " edited"; });
    expect_signature_refused([](auto& profile) { profile.source_revision = "user-edit"; });
    expect_signature_refused([](auto& profile) {
        profile.channels[4].property = Property::White;
        profile.channels[5].property = Property::Amber;
    });
    expect_signature_refused([](auto& profile) {
        profile.channels[0].default_value = 1U;
    });

    if (!exact_plan.empty()) {
        auto changed_after_review = make_upgrade_project();
        changed_after_review.fixture_profiles[stale_index].channels[4].property = Property::White;
        const auto before = emberlights::serialize_project(changed_after_review);
        const auto rejected = emberlights::apply_fixture_profile_upgrade_plan(
            changed_after_review, exact_plan);
        CHECK(!rejected.applied);
        CHECK(emberlights::serialize_project(changed_after_review) == before);

        auto changed_fixture_set = make_upgrade_project();
        changed_fixture_set.fixtures.push_back({
            "ir4-added", "IR-4 Added After Review",
            "soundswitch.both-lighting.bo-ir4.mode1", 1U, 60U, {"spotlight"}});
        const auto fixture_set_before = emberlights::serialize_project(changed_fixture_set);
        const auto fixture_set_rejected = emberlights::apply_fixture_profile_upgrade_plan(
            changed_fixture_set, exact_plan);
        CHECK(!fixture_set_rejected.applied);
        CHECK(emberlights::serialize_project(changed_fixture_set) == fixture_set_before);

        auto occupied_replacement = make_upgrade_project();
        auto conflicting = ten_channel;
        conflicting.source_revision = "user-owned-collision";
        occupied_replacement.fixture_profiles.push_back(std::move(conflicting));
        const auto occupied_before = emberlights::serialize_project(occupied_replacement);
        const auto occupied_rejected = emberlights::apply_fixture_profile_upgrade_plan(
            occupied_replacement, exact_plan);
        CHECK(!occupied_rejected.applied);
        CHECK(emberlights::serialize_project(occupied_replacement) == occupied_before);
    }

    auto upgraded = make_upgrade_project();
    const auto upgrade_plan = emberlights::plan_known_fixture_profile_upgrades(upgraded);
    auto expected = upgraded;
    for (auto& fixture : expected.fixtures) {
        if (fixture.profile_id == "soundswitch.both-lighting.bo-ir4.mode1") {
            fixture.profile_id = ten_channel.id;
        }
    }
    if (!upgrade_plan.empty()) {
        expected.unknown_records.push_back(
            "FIXTURE_PROFILE_UPGRADE\tbo-ir4-stale-10ch\t" +
            upgrade_plan.changes[0].before_behavior_fingerprint + "\t" +
            upgrade_plan.changes[0].after_behavior_fingerprint +
            "\tmanual-review-candidate");
    }
    expected.unknown_records.push_back(
        "MIGRATED_PATCH_UNVERIFIED\tfixture-mode-address-universe-review-required");
    expected.unknown_records.push_back(
        "QUALIFICATION_INVALIDATED\tfixtureProfileUpgrade\tbo-ir4\t2026-08-11");
    const auto applied = emberlights::apply_fixture_profile_upgrade_plan(upgraded, upgrade_plan);
    CHECK(applied.applied);
    CHECK(applied.changes.size() == 1U);
    CHECK(emberlights::validate_project(upgraded).ok());
    CHECK(emberlights::serialize_project(upgraded) == emberlights::serialize_project(expected));
    CHECK(emberlights::plan_known_fixture_profile_upgrades(upgraded).empty());
    const auto old_profile = std::find_if(
        upgraded.fixture_profiles.begin(), upgraded.fixture_profiles.end(),
        [](const auto& profile) {
            return profile.id == "soundswitch.both-lighting.bo-ir4.mode1";
        });
    CHECK(old_profile != upgraded.fixture_profiles.end());
    const auto unrelated = std::find_if(
        upgraded.fixtures.begin(), upgraded.fixtures.end(),
        [](const auto& fixture) { return fixture.id == "unrelated"; });
    CHECK(unrelated != upgraded.fixtures.end());
    if (unrelated != upgraded.fixtures.end()) {
        CHECK(unrelated->profile_id == "builtin.generic.dimmer-1ch");
        CHECK(unrelated->address == 40U);
    }
    const auto report = emberlights::serialize_fixture_profile_upgrade_report(
        applied, "input.emberlights", "output.emberlights", "abc123");
    CHECK(report.find("\"outputSha256\": \"abc123\"") != std::string::npos);
    CHECK(report.find("Both Lighting IR-4 User Manual, PDF page 5") !=
        std::string::npos);
    CHECK(report.find("\"affectedFixtureIds\": [\"ir4-a\", \"ir4-b\"]") !=
        std::string::npos);

    const auto round_trip_path =
        std::filesystem::path("build/fixture-profile-upgrade-round-trip.emberlights");
    std::error_code ignored;
    std::filesystem::remove(round_trip_path, ignored);
    CHECK(emberlights::save_project_atomic(round_trip_path, upgraded, false));
    emberlights::ProjectDocument reopened;
    CHECK(emberlights::load_project(round_trip_path, reopened, false));
    CHECK(emberlights::serialize_project(reopened) == emberlights::serialize_project(upgraded));
    CHECK(emberlights::plan_known_fixture_profile_upgrades(reopened).empty());
    CHECK(std::any_of(
        reopened.unknown_records.begin(), reopened.unknown_records.end(),
        [](const auto& record) {
            return record.starts_with("FIXTURE_PROFILE_UPGRADE\tbo-ir4-stale-10ch");
        }));
    std::filesystem::remove(round_trip_path, ignored);
}

void test_fixture_profile_management() {
    using emberlights::FixtureProfileWhiteAmberCorrectionError;
    using emberlights::Ir4ProfileAvailabilityError;
    using showcore::ChannelEncoding;
    using showcore::FixtureProfileSource;
    using showcore::Property;

    const auto same_channel = [](const auto& first, const auto& second) {
        return first.property == second.property &&
            first.coarse_offset == second.coarse_offset &&
            first.fine_offset == second.fine_offset &&
            first.encoding == second.encoding &&
            first.dmx_min == second.dmx_min &&
            first.dmx_max == second.dmx_max &&
            first.default_value == second.default_value;
    };
    const auto same_profile = [&](const auto& first, const auto& second) {
        return first.id == second.id &&
            first.manufacturer == second.manufacturer &&
            first.model == second.model &&
            first.mode == second.mode &&
            first.name == second.name &&
            first.source == second.source &&
            first.source_revision == second.source_revision &&
            first.footprint == second.footprint &&
            first.channels.size() == second.channels.size() &&
            std::equal(
                first.channels.begin(), first.channels.end(),
                second.channels.begin(), same_channel);
    };
    const auto without_ir4_profiles = [] {
        auto project = emberlights::make_starter_project();
        project.fixture_profiles.erase(
            std::remove_if(
                project.fixture_profiles.begin(), project.fixture_profiles.end(),
                [](const auto& profile) {
                    return profile.id == emberlights::kBothLightingBoIr4SixChannelProfileId ||
                        profile.id == emberlights::kBothLightingBoIr4TenChannelProfileId;
                }),
            project.fixture_profiles.end());
        return project;
    };
    const auto find_profile = [](auto& project, std::string_view id) {
        return std::find_if(
            project.fixture_profiles.begin(), project.fixture_profiles.end(),
            [id](const auto& profile) { return profile.id == id; });
    };

    auto available = without_ir4_profiles();
    const auto availability =
        emberlights::ensure_manual_backed_both_lighting_bo_ir4_profiles(available);
    CHECK(availability);
    CHECK(availability.error == Ir4ProfileAvailabilityError::None);
    CHECK(availability.six_channel_added);
    CHECK(availability.ten_channel_added);
    const auto six = find_profile(
        available, emberlights::kBothLightingBoIr4SixChannelProfileId);
    const auto ten = find_profile(
        available, emberlights::kBothLightingBoIr4TenChannelProfileId);
    CHECK(six != available.fixture_profiles.end());
    CHECK(ten != available.fixture_profiles.end());
    if (six != available.fixture_profiles.end()) {
        const auto summary = emberlights::summarize_fixture_profile_mapping(*six);
        CHECK(summary.profile_valid);
        CHECK(summary.white_mapping_count == 1U);
        CHECK(summary.amber_mapping_count == 1U);
        CHECK(summary.white_channel == 4U);
        CHECK(summary.amber_channel == 5U);
        CHECK(summary.correction_error ==
            FixtureProfileWhiteAmberCorrectionError::NotUserOwned);
        CHECK(summary.channels.size() == 6U);
        CHECK(summary.channels[3].line.find("CH4 | White") != std::string::npos);
        CHECK(summary.channels[4].line.find("CH5 | Amber") != std::string::npos);
        CHECK(summary.text.find("Footprint: 6 channels") != std::string::npos);
        CHECK(summary.text.find("read-only") != std::string::npos);
    }
    if (ten != available.fixture_profiles.end()) {
        const auto summary = emberlights::summarize_fixture_profile_mapping(*ten);
        CHECK(summary.profile_valid);
        CHECK(summary.white_channel == 5U);
        CHECK(summary.amber_channel == 6U);
        CHECK(summary.channels.size() == 10U);
        CHECK(summary.channels[4].line.find("CH5 | White") != std::string::npos);
        CHECK(summary.channels[5].line.find("CH6 | Amber") != std::string::npos);
        CHECK(summary.channels[8].line.find("CH9 | Constant") != std::string::npos);
        CHECK(summary.channels[8].line.find("Safe constant") != std::string::npos);
    }

    const auto available_size = available.fixture_profiles.size();
    const auto already_available =
        emberlights::ensure_manual_backed_both_lighting_bo_ir4_profiles(available);
    CHECK(already_available);
    CHECK(!already_available.six_channel_added);
    CHECK(!already_available.ten_channel_added);
    CHECK(available.fixture_profiles.size() == available_size);

    auto conflict = without_ir4_profiles();
    auto conflicting_six = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    conflicting_six.source = FixtureProfileSource::Local;
    conflicting_six.source_revision = "user-owned";
    conflict.fixture_profiles.push_back(conflicting_six);
    const auto conflict_before = conflict.fixture_profiles;
    const auto conflict_result =
        emberlights::ensure_manual_backed_both_lighting_bo_ir4_profiles(conflict);
    CHECK(!conflict_result);
    CHECK(conflict_result.error ==
        Ir4ProfileAvailabilityError::ConflictingSixChannelId);
    CHECK(conflict.fixture_profiles.size() == conflict_before.size());
    CHECK(std::equal(
        conflict.fixture_profiles.begin(), conflict.fixture_profiles.end(),
        conflict_before.begin(), same_profile));
    CHECK(find_profile(conflict, emberlights::kBothLightingBoIr4TenChannelProfileId) ==
        conflict.fixture_profiles.end());

    auto full = without_ir4_profiles();
    const auto seed = full.fixture_profiles.front();
    while (full.fixture_profiles.size() < showcore::kMaxCompiledFixtureProfiles) {
        auto profile = seed;
        profile.id = "capacity." + std::to_string(full.fixture_profiles.size());
        full.fixture_profiles.push_back(std::move(profile));
    }
    const auto full_size = full.fixture_profiles.size();
    const auto full_result =
        emberlights::ensure_manual_backed_both_lighting_bo_ir4_profiles(full);
    CHECK(!full_result);
    CHECK(full_result.error == Ir4ProfileAvailabilityError::ProfileCapacity);
    CHECK(full.fixture_profiles.size() == full_size);

    auto local = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    local.id = "local.ir4.user-profile";
    local.name = "My IR-4 6CH";
    local.source = FixtureProfileSource::Local;
    local.source_revision = "user-v1";
    std::swap(local.channels[3].property, local.channels[4].property);
    local.channels[3].encoding = ChannelEncoding::Discrete8;
    local.channels[3].dmx_min = 10U;
    local.channels[3].dmx_max = 240U;
    local.channels[3].default_value = 7U;
    local.channels[4].encoding = ChannelEncoding::Ranged8;
    local.channels[4].dmx_min = 20U;
    local.channels[4].dmx_max = 220U;
    local.channels[4].default_value = 0U;
    const auto local_before = local;
    const auto reversed_summary =
        emberlights::summarize_fixture_profile_mapping(local);
    CHECK(reversed_summary.profile_valid);
    CHECK(reversed_summary.can_correct_white_amber());
    CHECK(reversed_summary.white_channel == 5U);
    CHECK(reversed_summary.amber_channel == 4U);
    CHECK(reversed_summary.text.find("White is CH5 and Amber is CH4") !=
        std::string::npos);
    const auto corrected = emberlights::correct_fixture_profile_white_amber(local);
    CHECK(corrected.applied);
    CHECK(corrected.error == FixtureProfileWhiteAmberCorrectionError::None);
    CHECK(corrected.white_channel_before == 5U);
    CHECK(corrected.amber_channel_before == 4U);
    CHECK(corrected.white_channel_after == 4U);
    CHECK(corrected.amber_channel_after == 5U);
    CHECK(corrected.before_behavior_fingerprint !=
        corrected.after_behavior_fingerprint);
    CHECK(local.channels[3].property == Property::White);
    CHECK(local.channels[4].property == Property::Amber);
    for (std::size_t index = 0U; index < local.channels.size(); ++index) {
        auto expected = local_before.channels[index];
        if (index == 3U) {
            expected.property = Property::White;
        } else if (index == 4U) {
            expected.property = Property::Amber;
        }
        CHECK(same_channel(local.channels[index], expected));
    }
    CHECK(local.id == local_before.id);
    CHECK(local.name == local_before.name);
    CHECK(local.source == local_before.source);
    CHECK(local.source_revision == local_before.source_revision);

    const auto expect_correction_rejected = [&](auto profile, auto expected_error) {
        const auto before = profile;
        const auto result =
            emberlights::correct_fixture_profile_white_amber(profile);
        CHECK(!result.applied);
        CHECK(result.error == expected_error);
        CHECK(same_profile(profile, before));
    };
    auto missing_white = local_before;
    missing_white.channels[4].property = Property::UV;
    expect_correction_rejected(
        missing_white, FixtureProfileWhiteAmberCorrectionError::MissingWhite);
    auto missing_amber = local_before;
    missing_amber.channels[3].property = Property::UV;
    expect_correction_rejected(
        missing_amber, FixtureProfileWhiteAmberCorrectionError::MissingAmber);
    auto ambiguous_white = local_before;
    ambiguous_white.channels[0].property = Property::White;
    expect_correction_rejected(
        ambiguous_white, FixtureProfileWhiteAmberCorrectionError::AmbiguousWhite);
    auto ambiguous_amber = local_before;
    ambiguous_amber.channels[0].property = Property::Amber;
    expect_correction_rejected(
        ambiguous_amber, FixtureProfileWhiteAmberCorrectionError::AmbiguousAmber);
    auto invalid = local_before;
    invalid.channels[4].coarse_offset = invalid.channels[3].coarse_offset;
    const auto invalid_summary =
        emberlights::summarize_fixture_profile_mapping(invalid);
    CHECK(!invalid_summary.profile_valid);
    CHECK(invalid_summary.profile_error == showcore::ProfileError::DuplicateOffset);
    CHECK(invalid_summary.text.find("two mappings claim the same channel") !=
        std::string::npos);
    expect_correction_rejected(
        invalid, FixtureProfileWhiteAmberCorrectionError::InvalidProfile);
    expect_correction_rejected(
        emberlights::make_both_lighting_bo_ir4_6ch_profile(),
        FixtureProfileWhiteAmberCorrectionError::NotUserOwned);
    auto imported = local_before;
    imported.source = FixtureProfileSource::QlcPlus;
    expect_correction_rejected(
        imported, FixtureProfileWhiteAmberCorrectionError::NotUserOwned);

    // The project-level path fixes the usability failure in the old draft-only
    // workflow: immutable source truth remains present, every actively patched
    // fixture is rebound to one corrected Local snapshot, and the whole
    // candidate must validate and compile before the caller's document moves.
    auto atomic = emberlights::make_starter_project();
    atomic.id = "atomic-white-amber-correction";
    atomic.name = "Atomic White Amber Correction";
    atomic.fixtures.push_back({
        "ir4-front",
        "IR-4 Front",
        std::string(emberlights::kBothLightingBoIr4SixChannelProfileId),
        1U,
        1U,
        {"wash"}});
    atomic.fixtures.push_back({
        "ir4-rear",
        "IR-4 Rear",
        std::string(emberlights::kBothLightingBoIr4SixChannelProfileId),
        1U,
        20U,
        {"wash"}});
    atomic.looks.push_back({
        "white-only",
        "White Only",
        0U,
        {{"ir4-front", Property::White,
          showcore::PropertyValue::set(1.0F)}}});
    atomic.looks.push_back({
        "amber-only",
        "Amber Only",
        0U,
        {{"ir4-front", Property::Amber,
          showcore::PropertyValue::set(1.0F)}}});
    CHECK(emberlights::validate_project(atomic).ok());

    const auto atomic_plan =
        emberlights::plan_fixture_profile_white_amber_correction(
            atomic,
            emberlights::kBothLightingBoIr4SixChannelProfileId);
    CHECK(atomic_plan);
    CHECK(atomic_plan.error ==
        emberlights::FixtureProfileWhiteAmberProjectCorrectionError::None);
    CHECK(atomic_plan.plan.creates_local_copy);
    CHECK(atomic_plan.plan.source_profile_id ==
        emberlights::kBothLightingBoIr4SixChannelProfileId);
    CHECK(atomic_plan.plan.replacement_profile_id !=
        atomic_plan.plan.source_profile_id);
    CHECK(atomic_plan.plan.white_channel_before == 4U);
    CHECK(atomic_plan.plan.amber_channel_before == 5U);
    CHECK(atomic_plan.plan.white_channel_after == 5U);
    CHECK(atomic_plan.plan.amber_channel_after == 4U);
    CHECK(atomic_plan.plan.before_mapping.text.find("CH4 | White") !=
        std::string::npos);
    CHECK(atomic_plan.plan.after_mapping.text.find("CH5 | White") !=
        std::string::npos);
    CHECK(atomic_plan.plan.after_mapping.text.find("CH4 | Amber") !=
        std::string::npos);
    CHECK(atomic_plan.plan.affected_fixtures.size() == 2U);
    if (atomic_plan.plan.affected_fixtures.size() == 2U) {
        CHECK(atomic_plan.plan.affected_fixtures[0].fixture_id == "ir4-front");
        CHECK(atomic_plan.plan.affected_fixtures[1].fixture_id == "ir4-rear");
        CHECK(atomic_plan.plan.affected_fixtures[1].address == 20U);
    }

    // Any post-review source or patch edit rejects without leaking a partial
    // profile or fixture rebind into the document.
    auto stale_profile = atomic;
    const auto stale_profile_before = emberlights::serialize_project(stale_profile);
    const auto stale_source = find_profile(
        stale_profile, emberlights::kBothLightingBoIr4SixChannelProfileId);
    if (stale_source != stale_profile.fixture_profiles.end()) {
        stale_source->source_revision += "-changed";
    }
    const auto stale_source_state = emberlights::serialize_project(stale_profile);
    const auto stale_profile_result =
        emberlights::apply_fixture_profile_white_amber_correction(
            stale_profile, atomic_plan.plan);
    CHECK(!stale_profile_result.applied);
    CHECK(stale_profile_result.error ==
        emberlights::FixtureProfileWhiteAmberProjectCorrectionError::StalePlan);
    CHECK(emberlights::serialize_project(stale_profile) == stale_source_state);
    CHECK(emberlights::serialize_project(stale_profile) != stale_profile_before);

    auto stale_patch = atomic;
    stale_patch.fixtures[0].name += " renamed";
    const auto stale_patch_state = emberlights::serialize_project(stale_patch);
    const auto stale_patch_result =
        emberlights::apply_fixture_profile_white_amber_correction(
            stale_patch, atomic_plan.plan);
    CHECK(!stale_patch_result.applied);
    CHECK(stale_patch_result.error ==
        emberlights::FixtureProfileWhiteAmberProjectCorrectionError::StalePlan);
    CHECK(emberlights::serialize_project(stale_patch) == stale_patch_state);

    const auto profile_count_before = atomic.fixture_profiles.size();
    const auto applied_atomic =
        emberlights::apply_fixture_profile_white_amber_correction(
            atomic, atomic_plan.plan);
    CHECK(applied_atomic.applied);
    CHECK(applied_atomic.error ==
        emberlights::FixtureProfileWhiteAmberProjectCorrectionError::None);
    CHECK(atomic.fixture_profiles.size() == profile_count_before + 1U);
    CHECK(emberlights::validate_project(atomic).ok());
    CHECK(emberlights::compile_project_with_persisted_autoloops(atomic));
    CHECK(std::all_of(
        atomic.fixtures.begin(), atomic.fixtures.end(),
        [&](const auto& fixture) {
            return fixture.profile_id ==
                atomic_plan.plan.replacement_profile_id;
        }));
    const auto preserved_source = find_profile(
        atomic, emberlights::kBothLightingBoIr4SixChannelProfileId);
    CHECK(preserved_source != atomic.fixture_profiles.end());
    if (preserved_source != atomic.fixture_profiles.end()) {
        CHECK(preserved_source->source == FixtureProfileSource::BuiltIn);
        CHECK(emberlights::summarize_fixture_profile_mapping(*preserved_source)
                  .white_channel == 4U);
    }
    const auto corrected_copy = find_profile(
        atomic, atomic_plan.plan.replacement_profile_id);
    CHECK(corrected_copy != atomic.fixture_profiles.end());
    if (corrected_copy != atomic.fixture_profiles.end()) {
        CHECK(corrected_copy->source == FixtureProfileSource::Local);
        const auto corrected_summary =
            emberlights::summarize_fixture_profile_mapping(*corrected_copy);
        CHECK(corrected_summary.white_channel == 5U);
        CHECK(corrected_summary.amber_channel == 4U);
    }
    CHECK(std::any_of(
        atomic.unknown_records.begin(), atomic.unknown_records.end(),
        [](const auto& record) {
            return record.starts_with(
                "FIXTURE_PROFILE_CORRECTION\twhite-amber-v1\t");
        }));

    // This is the full authoring/compiler/renderer proof for the reported
    // symptom. EmberLights' canonical semantics send each property to the
    // profile offset. After the reviewed correction, White moves to physical
    // CH5 and Amber moves to physical CH4 for both patched fixtures.
    const auto white_preview =
        emberlights::preview_static_look(atomic, "white-only");
    const auto amber_preview =
        emberlights::preview_static_look(atomic, "amber-only");
    CHECK(white_preview);
    CHECK(amber_preview);
    if (white_preview && amber_preview) {
        CHECK(white_preview.frames.universes[0][3] == 0U);
        CHECK(white_preview.frames.universes[0][4] == 255U);
        CHECK(amber_preview.frames.universes[0][3] == 255U);
        CHECK(amber_preview.frames.universes[0][4] == 0U);
    }

    // Auto keeps an already-Local ID in place, so no repatch or profile-count
    // growth is required. Force-in-place correctly refuses immutable sources.
    auto local_project = emberlights::make_starter_project();
    local_project.id = "local-white-amber-correction";
    local_project.name = "Local White Amber Correction";
    auto local_profile = emberlights::make_both_lighting_bo_ir4_6ch_profile();
    local_profile.id = "local.active-ir4";
    local_profile.name = "Local Active IR-4";
    local_profile.source = FixtureProfileSource::Local;
    local_profile.source_revision = "operator-profile-v1";
    local_project.fixture_profiles.push_back(local_profile);
    local_project.fixtures.push_back({
        "local-ir4", "Local IR-4", local_profile.id, 1U, 100U, {}});
    const auto local_count = local_project.fixture_profiles.size();
    const auto local_result =
        emberlights::correct_and_rebind_fixture_profile_white_amber(
            local_project, local_profile.id);
    CHECK(local_result.applied);
    CHECK(!local_result.plan.creates_local_copy);
    CHECK(local_project.fixture_profiles.size() == local_count);
    CHECK(local_project.fixtures.back().profile_id == local_profile.id);
    const auto local_corrected = find_profile(local_project, local_profile.id);
    CHECK(local_corrected != local_project.fixture_profiles.end());
    if (local_corrected != local_project.fixture_profiles.end()) {
        const auto summary =
            emberlights::summarize_fixture_profile_mapping(*local_corrected);
        CHECK(summary.white_channel == 5U);
        CHECK(summary.amber_channel == 4U);
    }

    const auto read_only_in_place =
        emberlights::plan_fixture_profile_white_amber_correction(
            emberlights::make_starter_project(),
            emberlights::kBothLightingBoIr4SixChannelProfileId,
            emberlights::FixtureProfileWhiteAmberProjectCorrectionMode::
                UpdateLocalInPlace);
    CHECK(!read_only_in_place);
    CHECK(read_only_in_place.error ==
        emberlights::FixtureProfileWhiteAmberProjectCorrectionError::
            SourceProfileReadOnly);
}

void test_soundswitch_v1_semantic_conversion() {
    const auto source = std::filesystem::path("build/soundswitch-v1-source.ssproj");
    const auto project_path = std::filesystem::path("build/soundswitch-v1.emberlights");
    std::error_code ignored;
    std::filesystem::remove_all(source, ignored);
    std::filesystem::remove(project_path, ignored);
    std::filesystem::create_directories(source, ignored);
    CHECK(!ignored);

    auto write_bytes = [](const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(output);
    };
    auto append_u32 = [](std::vector<std::uint8_t>& bytes, std::uint32_t value) {
        for (std::size_t index = 0U; index < 4U; ++index) {
            bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8U)));
        }
    };
    auto append_utf16 = [&](std::vector<std::uint8_t>& bytes, std::string_view value) {
        append_u32(bytes, static_cast<std::uint32_t>(value.size() + 1U));
        for (const auto character : value) {
            bytes.push_back(static_cast<std::uint8_t>(character));
            bytes.push_back(0U);
        }
        bytes.push_back(0U);
        bytes.push_back(0U);
    };

    {
        std::ofstream manifest(source / ".ssproj", std::ios::binary | std::ios::trunc);
        manifest << "{\"id\":\"{TEST-V1}\",\"version\":{\"major\": 2,\"minor\": 10}}\n";
        CHECK(static_cast<bool>(manifest));
    }
    std::vector<std::uint8_t> venue{0xAAU, 0xAAU, 0x09U, 0x55U};
    for (const auto model : {
             std::string_view{"6x18W RGBWA UV 6in1 Uplight (BO-S601)"},
             std::string_view{"BO-Tube 192 360 Pixel Tube"},
             std::string_view{"Wash FX HEX"},
             std::string_view{"BO-IR4 LED Mini Spotlight"}}) {
        append_utf16(venue, model);
    }
    CHECK(write_bytes(source / "SoundSwitchVenues.bin", venue));

    std::vector<std::uint8_t> loops{0xAAU, 0xAAU, 0x09U, 0x55U};
    for (std::size_t index = 0U; index < 32U; ++index) {
        append_utf16(loops, "Test Loop " + std::to_string(index + 1U));
    }
    CHECK(write_bytes(source / "SoundSwitchAutoLoops.bin", loops));

    const auto migration = emberlights::create_soundswitch_v1_project(source);
    CHECK(migration);
    CHECK(migration.manifest_id == "{TEST-V1}");
    CHECK(migration.source_autoloop_names.size() == 32U);
    CHECK(migration.project.fixtures.size() == 71U);
    CHECK(migration.project.groups.size() == 9U);
    CHECK(migration.project.looks.size() == 18U);
    CHECK(migration.project.autoloops.size() == 32U);
    const auto ir4_profile = std::find_if(
        migration.project.fixture_profiles.begin(),
        migration.project.fixture_profiles.end(),
        [](const auto& profile) {
            return profile.id == emberlights::kBothLightingBoIr4TenChannelProfileId;
        });
    CHECK(ir4_profile != migration.project.fixture_profiles.end());
    if (ir4_profile != migration.project.fixture_profiles.end()) {
        CHECK(ir4_profile->mode.find("10 Channel (manual-matched") == 0U);
        CHECK(ir4_profile->channels.size() == 10U);
        if (ir4_profile->channels.size() == 10U) {
            CHECK(ir4_profile->channels[0].property == showcore::Property::Intensity);
            CHECK(ir4_profile->channels[1].property == showcore::Property::Red);
            CHECK(ir4_profile->channels[2].property == showcore::Property::Green);
            CHECK(ir4_profile->channels[3].property == showcore::Property::Blue);
            CHECK(ir4_profile->channels[4].property == showcore::Property::White);
            CHECK(ir4_profile->channels[5].property == showcore::Property::Amber);
            CHECK(ir4_profile->channels[6].property == showcore::Property::UV);
            CHECK(ir4_profile->channels[7].property == showcore::Property::Strobe);
        }
    }
    CHECK(!migration.project.connections.artnet_enabled);
    CHECK(!migration.project.connections.sacn_enabled);
    CHECK(migration.project.connections.dmx_usb_pro_ports[0].empty());
    CHECK(migration.project.connections.dmx_usb_pro_ports[1].empty());
    CHECK(emberlights::validate_project(migration.project).ok());
    CHECK(emberlights::compile_project(migration.project));
    const auto report = emberlights::serialize_soundswitch_v1_migration_report(migration);
    CHECK(report.find("emberlights-soundswitch-v1-migration") != std::string::npos);
    CHECK(report.find("\"outputEnabled\": false") != std::string::npos);
    CHECK(report.find("Test Loop 1") != std::string::npos);
    CHECK(report.find("Confirm the fixture display is 10CH") != std::string::npos);
    CHECK(report.find("source-qualified approximations") != std::string::npos);
    CHECK(emberlights::save_project_atomic(project_path, migration.project, false));
    emberlights::ProjectDocument loaded;
    CHECK(emberlights::load_project(project_path, loaded, false));
    CHECK(loaded.fixtures.size() == migration.project.fixtures.size());
    CHECK(loaded.autoloops.size() == migration.project.autoloops.size());

    std::filesystem::remove_all(source, ignored);
    std::filesystem::remove(project_path, ignored);
}

}  // namespace

#if defined(__GNUC__) && !defined(_MSC_VER)
#define EMBERLIGHTS_NOINLINE __attribute__((noinline))
#else
#define EMBERLIGHTS_NOINLINE
#endif

EMBERLIGHTS_NOINLINE void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

EMBERLIGHTS_NOINLINE void* operator new[](std::size_t size) {
    return ::operator new(size);
}

EMBERLIGHTS_NOINLINE void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size);
}

EMBERLIGHTS_NOINLINE void* operator new[](
    std::size_t size,
    const std::nothrow_t& tag) noexcept {
    return ::operator new(size, tag);
}

EMBERLIGHTS_NOINLINE void operator delete(void* memory) noexcept {
    std::free(memory);
}

EMBERLIGHTS_NOINLINE void operator delete[](void* memory) noexcept {
    std::free(memory);
}

EMBERLIGHTS_NOINLINE void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

EMBERLIGHTS_NOINLINE void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

EMBERLIGHTS_NOINLINE void operator delete(void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}

EMBERLIGHTS_NOINLINE void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    std::free(memory);
}

#undef EMBERLIGHTS_NOINLINE

int main() {
    if (!initialize_test_network()) {
        std::cerr << "Unable to initialize test networking\n";
        return EXIT_FAILURE;
    }
    test_layers();
    test_safety();
    test_fixture_profile_validation();
    test_generic_profile_rendering();
    test_ranged_channel_rendering();
    test_capability_frame_attribution();
    test_qlc_fixture_import();
    test_compiled_fixture_library();
    test_patch_and_render();
    test_16bit_render();
    test_artnet();
    test_output_backend_contract_and_health();
    test_dmx_usb_pro();
    test_soundswitch_micro_protocol();
    test_sacn();
    test_os2l();
    test_os2l_stream();
    test_os2l_server_lifecycle();
    test_spsc_queue();
    test_sync_manager();
    test_midi();
    test_midi_short_codec();
    test_winmm_midi_platform_boundary();
    test_static_look_validation();
    test_static_look_player();
    test_autoloop();
    test_autoloop_catalog_and_player();
    test_render_has_no_allocations();
    test_performance_playback_has_no_allocations();
    test_deterministic_replay();
    test_project_edit_history();
    test_autoloop_placement_operations();
    test_audio_asset_identity_and_relinking();
    test_project_validation_io_and_compilation();
    test_runner_os2l_startup_without_button_trigger();
    test_runner_service_lifecycle();
    test_runner_output_snapshot_route_results();
    test_soundswitch_read_only_inspection_and_bundle();
    test_soundswitch_source_binding_audit();
    test_soundswitch_application_data_backup_inspection_and_bundle();
    test_ir4_fixture_profile_upgrade();
    test_fixture_profile_management();
    test_soundswitch_v1_semantic_conversion();

    cleanup_test_network();

    if (g_failures == 0) {
        std::cout << "All core tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cerr << g_failures << " test(s) failed\n";
    return EXIT_FAILURE;
}
