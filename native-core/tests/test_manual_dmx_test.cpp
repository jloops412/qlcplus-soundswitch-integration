#include "emberlights/file_identity.hpp"
#include "emberlights/manual_dmx_test.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL " << __FILE__ << ':' << __LINE__ \
                  << " - " #condition "\n"; \
        ++failures; \
    } \
} while (false)

class FakeTransport final : public emberlights::ManualDmxTestTransport {
public:
    [[nodiscard]] bool open(
        std::string_view adapter_id,
        std::uint8_t universe) noexcept override {
        ++open_calls;
        opened_adapter = adapter_id;
        opened_universe = universe;
        opened = open_succeeds;
        return opened;
    }

    [[nodiscard]] bool connected() const noexcept override {
        return opened && connected_value;
    }

    [[nodiscard]] bool send(
        const showcore::DmxUniverse& frame) noexcept override {
        ++send_calls;
        attempted_frames.push_back(frame);
        if (!connected() || send_calls == fail_send_call) {
            return false;
        }
        accepted_frames.push_back(frame);
        return true;
    }

    void close() noexcept override {
        ++close_calls;
        opened = false;
    }

    bool open_succeeds{true};
    bool connected_value{true};
    bool opened{false};
    std::uint64_t fail_send_call{0U};
    std::uint64_t send_calls{0U};
    std::uint64_t open_calls{0U};
    std::uint64_t close_calls{0U};
    std::string opened_adapter;
    std::uint8_t opened_universe{0U};
    std::vector<showcore::DmxUniverse> attempted_frames;
    std::vector<showcore::DmxUniverse> accepted_frames;
};

[[nodiscard]] emberlights::ManualDmxTestConfig make_config() {
    emberlights::ManualDmxTestConfig config;
    config.adapter_id = "soundswitch-micro:u1";
    config.universe = 1U;
    config.hold_timeout = std::chrono::milliseconds{1000};
    config.session_timeout = std::chrono::milliseconds{60000};
    config.blackout_frame_repetitions = 2U;
    config.maximum_active_channels = 4U;
    return config;
}

[[nodiscard]] emberlights::ManualDmxTestPlan make_plan() {
    emberlights::ManualDmxTestPlan plan;
    const auto result = emberlights::build_manual_dmx_test_plan(
        make_config(), plan);
    CHECK(result.ok());
    return plan;
}

[[nodiscard]] bool blackout(const showcore::DmxUniverse& frame) {
    return std::all_of(
        frame.begin(), frame.end(), [](std::uint8_t value) {
            return value == 0U;
        });
}

}  // namespace

int main() {
    using Session = emberlights::ManualDmxTestSession;
    const auto epoch = Session::TimePoint{};

    CHECK(std::string_view(emberlights::manual_dmx_test_phase_name(
              emberlights::ManualDmxTestPhase::ArmedBlackout)) ==
          "armed-blackout");
    CHECK(std::string_view(emberlights::manual_dmx_test_error_name(
              emberlights::ManualDmxTestError::InvalidAcknowledgement)) ==
          "invalid-acknowledgement");

    auto plan = make_plan();
    CHECK(plan.schema_version == emberlights::kManualDmxTestPlanVersion);
    CHECK(emberlights::is_sha256_digest(plan.plan_sha256));
    CHECK(emberlights::validate_manual_dmx_test_plan(plan).ok());
    const auto acknowledgement =
        emberlights::manual_dmx_test_acknowledgement(plan);
    CHECK(acknowledgement.starts_with("ARM RAW DMX U1 "));
    CHECK(emberlights::manual_dmx_test_acknowledged(plan, acknowledgement));
    CHECK(!emberlights::manual_dmx_test_acknowledged(plan, "ARM RAW DMX U1"));

    {
        auto invalid = make_config();
        invalid.adapter_id = "soundswitch-micro:u2";
        emberlights::ManualDmxTestPlan rejected;
        CHECK(emberlights::build_manual_dmx_test_plan(invalid, rejected).error ==
              emberlights::ManualDmxTestError::InvalidConfiguration);
        invalid = make_config();
        invalid.hold_timeout = std::chrono::milliseconds{999};
        CHECK(emberlights::build_manual_dmx_test_plan(invalid, rejected).error ==
              emberlights::ManualDmxTestError::InvalidConfiguration);
        invalid = make_config();
        invalid.session_timeout = std::chrono::milliseconds{30000};
        CHECK(emberlights::build_manual_dmx_test_plan(invalid, rejected).error ==
              emberlights::ManualDmxTestError::InvalidConfiguration);
        invalid = make_config();
        invalid.maximum_active_channels = 65U;
        CHECK(emberlights::build_manual_dmx_test_plan(invalid, rejected).error ==
              emberlights::ManualDmxTestError::InvalidConfiguration);
        invalid = make_config();
        invalid.adapter_id = "bad adapter:u1";
        CHECK(emberlights::build_manual_dmx_test_plan(invalid, rejected).error ==
              emberlights::ManualDmxTestError::InvalidAdapter);

        auto tampered = plan;
        tampered.config.hold_timeout = std::chrono::milliseconds{2000};
        CHECK(emberlights::validate_manual_dmx_test_plan(tampered).error ==
              emberlights::ManualDmxTestError::InvalidConfiguration);
    }

    {
        showcore::DmxUniverse frame{};
        std::vector<emberlights::ManualDmxChannelValue> normalized;
        const std::vector<emberlights::ManualDmxChannelValue> values{
            {5U, 12U}, {1U, 255U}, {3U, 0U}};
        CHECK(emberlights::make_manual_dmx_test_frame(
                  plan, values, frame, normalized).ok());
        CHECK(normalized.size() == 2U);
        CHECK(normalized[0U].channel == 1U);
        CHECK(normalized[0U].value == 255U);
        CHECK(normalized[1U].channel == 5U);
        CHECK(frame[0U] == 255U);
        CHECK(frame[4U] == 12U);
        CHECK(frame[2U] == 0U);

        const std::vector<emberlights::ManualDmxChannelValue> duplicate{
            {1U, 10U}, {1U, 20U}};
        CHECK(emberlights::make_manual_dmx_test_frame(
                  plan, duplicate, frame, normalized).error ==
              emberlights::ManualDmxTestError::InvalidValues);
        const std::vector<emberlights::ManualDmxChannelValue> outside{
            {513U, 10U}};
        CHECK(emberlights::make_manual_dmx_test_frame(
                  plan, outside, frame, normalized).error ==
              emberlights::ManualDmxTestError::InvalidValues);
        const std::vector<emberlights::ManualDmxChannelValue> too_many{
            {1U, 1U}, {2U, 2U}, {3U, 3U}, {4U, 4U}, {5U, 5U}};
        CHECK(emberlights::make_manual_dmx_test_frame(
                  plan, too_many, frame, normalized).error ==
              emberlights::ManualDmxTestError::InvalidValues);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, "wrong", transport, epoch).error ==
              emberlights::ManualDmxTestError::InvalidAcknowledgement);
        CHECK(transport.open_calls == 0U);
        CHECK(session.snapshot(epoch).phase ==
              emberlights::ManualDmxTestPhase::Idle);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        auto status = session.snapshot(epoch);
        CHECK(status.phase == emberlights::ManualDmxTestPhase::ArmedBlackout);
        CHECK(status.transport_open);
        CHECK(status.frames_attempted == 2U);
        CHECK(status.frames_accepted == 2U);
        CHECK(status.session_remaining == std::chrono::milliseconds{60000});
        CHECK(transport.open_calls == 1U);
        CHECK(transport.opened_adapter == "soundswitch-micro:u1");
        CHECK(transport.opened_universe == 1U);
        CHECK(std::all_of(
            transport.accepted_frames.begin(), transport.accepted_frames.end(),
            blackout));

        const std::vector<emberlights::ManualDmxChannelValue> first{
            {5U, 12U}, {1U, 255U}};
        CHECK(session.apply(first, epoch).ok());
        status = session.snapshot(epoch + std::chrono::milliseconds{250});
        CHECK(status.phase == emberlights::ManualDmxTestPhase::Holding);
        CHECK(status.held_values.size() == 2U);
        CHECK(status.held_values.front().channel == 1U);
        CHECK(status.hold_remaining == std::chrono::milliseconds{750});
        CHECK(emberlights::is_sha256_digest(status.held_frame_sha256));
        CHECK(transport.accepted_frames.back()[0U] == 255U);
        CHECK(transport.accepted_frames.back()[4U] == 12U);

        const std::vector<emberlights::ManualDmxChannelValue> replacement{
            {8U, 64U}};
        CHECK(session.apply(
                  replacement,
                  epoch + std::chrono::milliseconds{300}).ok());
        CHECK(transport.accepted_frames.size() == 6U);
        CHECK(blackout(transport.accepted_frames[3U]));
        CHECK(blackout(transport.accepted_frames[4U]));
        CHECK(transport.accepted_frames.back()[7U] == 64U);
        CHECK(session.snapshot(epoch).held_values.size() == 1U);

        CHECK(session.blackout_now(
                  epoch + std::chrono::milliseconds{400}).ok());
        status = session.snapshot(epoch + std::chrono::milliseconds{400});
        CHECK(status.phase == emberlights::ManualDmxTestPhase::ArmedBlackout);
        CHECK(status.held_values.empty());
        CHECK(status.held_frame_sha256.empty());
        CHECK(status.explicit_blackouts == 1U);

        CHECK(session.apply(
                  first,
                  epoch + std::chrono::milliseconds{500}).ok());
        CHECK(session.poll(
                  epoch + std::chrono::milliseconds{1499}).ok());
        CHECK(session.snapshot(epoch).phase ==
              emberlights::ManualDmxTestPhase::Holding);
        CHECK(session.poll(
                  epoch + std::chrono::milliseconds{1500}).ok());
        status = session.snapshot(epoch + std::chrono::milliseconds{1500});
        CHECK(status.phase == emberlights::ManualDmxTestPhase::ArmedBlackout);
        CHECK(status.automatic_blackouts == 1U);
        CHECK(status.held_values.empty());

        CHECK(session.stop(
                  epoch + std::chrono::milliseconds{1600}).ok());
        status = session.snapshot(epoch + std::chrono::milliseconds{1600});
        CHECK(status.phase == emberlights::ManualDmxTestPhase::Complete);
        CHECK(!status.transport_open);
        CHECK(status.terminal_blackout_attempted);
        CHECK(status.terminal_blackout_succeeded);
        CHECK(transport.close_calls == 1U);
        CHECK(session.apply(first, epoch).error ==
              emberlights::ManualDmxTestError::NotActive);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        CHECK(session.poll(epoch + std::chrono::milliseconds{60000}).error ==
              emberlights::ManualDmxTestError::SessionTimedOut);
        const auto status = session.snapshot(
            epoch + std::chrono::milliseconds{60000});
        CHECK(status.phase == emberlights::ManualDmxTestPhase::TimedOut);
        CHECK(status.error == emberlights::ManualDmxTestError::SessionTimedOut);
        CHECK(status.terminal_blackout_succeeded);
        CHECK(!status.transport_open);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        transport.connected_value = false;
        CHECK(session.poll(epoch).error ==
              emberlights::ManualDmxTestError::DeviceLost);
        const auto status = session.snapshot(epoch);
        CHECK(status.phase == emberlights::ManualDmxTestPhase::Failed);
        CHECK(status.terminal_blackout_attempted);
        CHECK(!status.terminal_blackout_succeeded);
        CHECK(transport.close_calls == 1U);
    }

    {
        FakeTransport transport;
        transport.open_succeeds = false;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).error ==
              emberlights::ManualDmxTestError::OpenFailed);
        CHECK(transport.close_calls == 1U);
    }

    {
        FakeTransport transport;
        transport.fail_send_call = 1U;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).error ==
              emberlights::ManualDmxTestError::BlackoutFailed);
        CHECK(session.snapshot(epoch).phase ==
              emberlights::ManualDmxTestPhase::Failed);
        CHECK(transport.close_calls == 1U);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        transport.fail_send_call = 3U;
        const std::vector<emberlights::ManualDmxChannelValue> values{{1U, 1U}};
        CHECK(session.apply(values, epoch).error ==
              emberlights::ManualDmxTestError::FrameWriteFailed);
        CHECK(session.snapshot(epoch).phase ==
              emberlights::ManualDmxTestPhase::Failed);
        CHECK(transport.close_calls == 1U);
    }

    {
        FakeTransport transport;
        Session session;
        CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        CHECK(session.cancel("Operator pressed Escape.", epoch).error ==
              emberlights::ManualDmxTestError::Cancelled);
        const auto status = session.snapshot(epoch);
        CHECK(status.phase == emberlights::ManualDmxTestPhase::Cancelled);
        CHECK(status.terminal_blackout_succeeded);
    }

    {
        FakeTransport transport;
        {
            Session session;
            CHECK(session.begin(plan, acknowledgement, transport, epoch).ok());
        }
        CHECK(transport.close_calls == 1U);
        CHECK(transport.accepted_frames.size() == 4U);
        CHECK(std::all_of(
            transport.accepted_frames.begin(), transport.accepted_frames.end(),
            blackout));
    }

    if (failures == 0) {
        std::cout << "Advanced manual DMX test workflow tests passed\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
