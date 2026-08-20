#pragma once

#include "showcore/soundswitch_micro.hpp"
#include "showcore/types.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

inline constexpr std::uint32_t kManualDmxTestPlanVersion = 1U;
inline constexpr std::size_t kManualDmxTestMaximumActiveChannels = 64U;

enum class ManualDmxTestError : std::uint8_t {
    None,
    InvalidConfiguration,
    InvalidAdapter,
    InvalidValues,
    InvalidAcknowledgement,
    AlreadyStarted,
    NotActive,
    OpenFailed,
    DeviceLost,
    BlackoutFailed,
    FrameWriteFailed,
    SessionTimedOut,
    Cancelled
};

enum class ManualDmxTestPhase : std::uint8_t {
    Idle,
    Opening,
    ArmedBlackout,
    Holding,
    Complete,
    TimedOut,
    Failed,
    Cancelled
};

struct ManualDmxTestCheck {
    ManualDmxTestError error{ManualDmxTestError::None};
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return error == ManualDmxTestError::None;
    }
};

struct ManualDmxChannelValue {
    std::uint16_t channel{0U};
    std::uint8_t value{0U};
};

struct ManualDmxTestConfig {
    std::string adapter_id{"soundswitch-micro:u1"};
    std::uint8_t universe{1U};
    std::chrono::milliseconds hold_timeout{5000};
    std::chrono::milliseconds session_timeout{600000};
    std::uint16_t blackout_frame_repetitions{3U};
    std::size_t maximum_active_channels{32U};
};

// The plan arms only one explicit adapter/universe and bounded timing policy.
// Every nonzero frame still requires a separate operator APPLY/SET action and
// remains visible through the coherent session snapshot until auto-blackout.
struct ManualDmxTestPlan {
    std::uint32_t schema_version{kManualDmxTestPlanVersion};
    ManualDmxTestConfig config{};
    std::string plan_sha256;
};

struct ManualDmxTestSnapshot {
    ManualDmxTestPhase phase{ManualDmxTestPhase::Idle};
    ManualDmxTestError error{ManualDmxTestError::None};
    std::string message;
    std::string adapter_id;
    std::uint8_t universe{0U};
    std::vector<ManualDmxChannelValue> held_values;
    std::string held_frame_sha256;
    std::chrono::milliseconds hold_remaining{0};
    std::chrono::milliseconds session_remaining{0};
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    std::uint32_t explicit_blackouts{0U};
    std::uint32_t automatic_blackouts{0U};
    bool transport_open{false};
    bool terminal_blackout_attempted{false};
    bool terminal_blackout_succeeded{false};
};

class ManualDmxTestTransport {
public:
    virtual ~ManualDmxTestTransport() = default;

    [[nodiscard]] virtual bool open(
        std::string_view adapter_id,
        std::uint8_t universe) noexcept = 0;
    [[nodiscard]] virtual bool connected() const noexcept = 0;
    [[nodiscard]] virtual bool send(
        const showcore::DmxUniverse& frame) noexcept = 0;
    virtual void close() noexcept = 0;
};

// Production adapter over the same SoundSwitch Micro session used by Runner
// and the evidence-bound fixture qualification workflow. This is not another
// USB driver and it cannot reinterpret raw values as fixture properties.
class SoundSwitchMicroManualDmxTestTransport final
    : public ManualDmxTestTransport {
public:
    explicit SoundSwitchMicroManualDmxTestTransport(
        showcore::SoundSwitchMicroSessionConfig config = {}) noexcept;

    [[nodiscard]] bool open(
        std::string_view adapter_id,
        std::uint8_t universe) noexcept override;
    [[nodiscard]] bool connected() const noexcept override;
    [[nodiscard]] bool send(
        const showcore::DmxUniverse& frame) noexcept override;
    void close() noexcept override;

    [[nodiscard]] showcore::SoundSwitchMicroSessionStatus status() const noexcept;

private:
    showcore::SoundSwitchMicroSessionConfig config_{};
    showcore::SoundSwitchMicroSession session_{};
};

[[nodiscard]] const char* manual_dmx_test_phase_name(
    ManualDmxTestPhase phase) noexcept;
[[nodiscard]] const char* manual_dmx_test_error_name(
    ManualDmxTestError error) noexcept;

[[nodiscard]] ManualDmxTestCheck build_manual_dmx_test_plan(
    const ManualDmxTestConfig& config,
    ManualDmxTestPlan& plan);
[[nodiscard]] ManualDmxTestCheck validate_manual_dmx_test_plan(
    const ManualDmxTestPlan& plan);
[[nodiscard]] std::string manual_dmx_test_acknowledgement(
    const ManualDmxTestPlan& plan);
[[nodiscard]] bool manual_dmx_test_acknowledged(
    const ManualDmxTestPlan& plan,
    std::string_view acknowledgement);

// Values are one-based absolute DMX slots. Zero values are accepted and
// removed from the held-output projection; duplicate or out-of-range slots are
// rejected. The normalized projection is sorted for deterministic display.
[[nodiscard]] ManualDmxTestCheck make_manual_dmx_test_frame(
    const ManualDmxTestPlan& plan,
    std::span<const ManualDmxChannelValue> values,
    showcore::DmxUniverse& frame,
    std::vector<ManualDmxChannelValue>& normalized_values);

class ManualDmxTestSession {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    ManualDmxTestSession() = default;
    ~ManualDmxTestSession();

    ManualDmxTestSession(const ManualDmxTestSession&) = delete;
    ManualDmxTestSession& operator=(const ManualDmxTestSession&) = delete;

    [[nodiscard]] ManualDmxTestCheck begin(
        ManualDmxTestPlan plan,
        std::string_view acknowledgement,
        ManualDmxTestTransport& transport,
        TimePoint now);

    [[nodiscard]] ManualDmxTestCheck apply(
        std::span<const ManualDmxChannelValue> values,
        TimePoint now);
    [[nodiscard]] ManualDmxTestCheck blackout_now(TimePoint now);
    [[nodiscard]] ManualDmxTestCheck poll(TimePoint now);
    [[nodiscard]] ManualDmxTestCheck stop(TimePoint now);
    [[nodiscard]] ManualDmxTestCheck cancel(
        std::string_view reason,
        TimePoint now);

    [[nodiscard]] ManualDmxTestSnapshot snapshot(TimePoint now) const;
    [[nodiscard]] const ManualDmxTestPlan* plan() const noexcept;

private:
    [[nodiscard]] bool send_frame(
        const showcore::DmxUniverse& frame) noexcept;
    [[nodiscard]] bool send_blackout() noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] ManualDmxTestCheck ensure_active(TimePoint now);
    void finish_failure(
        ManualDmxTestError error,
        std::string message) noexcept;
    void terminal_close() noexcept;

    ManualDmxTestPlan plan_{};
    ManualDmxTestTransport* transport_{nullptr};
    ManualDmxTestPhase phase_{ManualDmxTestPhase::Idle};
    ManualDmxTestError error_{ManualDmxTestError::None};
    std::string message_;
    std::vector<ManualDmxChannelValue> held_values_;
    std::string held_frame_sha256_;
    TimePoint hold_deadline_{};
    TimePoint session_deadline_{};
    std::uint64_t frames_attempted_{0U};
    std::uint64_t frames_accepted_{0U};
    std::uint32_t explicit_blackouts_{0U};
    std::uint32_t automatic_blackouts_{0U};
    bool transport_open_{false};
    bool terminal_blackout_attempted_{false};
    bool terminal_blackout_succeeded_{false};
};

}  // namespace emberlights
