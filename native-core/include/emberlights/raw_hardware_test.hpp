#pragma once

#include "emberlights/hardware_qualification.hpp"
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

inline constexpr std::uint32_t kRawHardwareTestPlanVersion = 1U;
inline constexpr std::uint32_t kRawHardwareTestAttemptVersion = 1U;

enum class RawHardwareTestError : std::uint8_t {
    None,
    InvalidConfiguration,
    InvalidProjectBasis,
    InvalidBinding,
    InvalidCriteria,
    InvalidMarkers,
    InvalidPlan,
    AlreadyStarted,
    OpenFailed,
    DeviceLost,
    BlackoutFailed,
    FrameWriteFailed,
    ObservationRejected,
    TimedOut,
    Cancelled,
    Incomplete,
    StaleProject,
    AttestationRejected,
    InvalidAuditRecord,
    Replay
};

enum class RawHardwareTestPhase : std::uint8_t {
    Idle,
    Opening,
    BlackoutBefore,
    Stimulus,
    AwaitingObservation,
    BlackoutAfter,
    Complete,
    Failed,
    Cancelled
};

struct RawHardwareTestCheck {
    RawHardwareTestError error{RawHardwareTestError::None};
    std::string subject;
    std::string message;

    [[nodiscard]] bool ok() const noexcept {
        return error == RawHardwareTestError::None;
    }
};

struct RawHardwareTestConfig {
    std::chrono::milliseconds observation_timeout{30000};
    std::chrono::milliseconds session_timeout{3600000};
    std::uint16_t blackout_frame_repetitions{3U};
};

// Criteria are deliberately caller-supplied and one-based. The workflow will
// not infer that a macro, strobe, color, default, or cell-boundary response is
// safe from a generic property name.
struct RawHardwareTestSlotCriterion {
    std::uint16_t relative_slot{0U};
    std::uint8_t one_hot_value{255U};
    std::string expected_behavior;
    bool require_no_spill{true};
};

struct RawHardwareTestPlan {
    std::uint32_t schema_version{kRawHardwareTestPlanVersion};
    std::string input_project_sha256;
    std::string candidate_project_sha256;
    std::string candidate_binding_sha256;
    FixtureQualificationBinding binding;
    std::uint16_t footprint{0U};
    RawHardwareTestConfig config{};
    std::vector<FixtureQualificationRequirement> requirements;
    std::vector<std::string> markers_to_supersede;
};

struct RawHardwareTestRunIdentity {
    std::string operator_id;
    std::string started_at_utc;
};

struct RawHardwareTestObservedResult {
    std::string observed_behavior;
    bool passed{false};
    bool no_spill_observed{false};
};

struct RawHardwareTestSnapshot {
    RawHardwareTestPhase phase{RawHardwareTestPhase::Idle};
    RawHardwareTestError error{RawHardwareTestError::None};
    std::string message;
    std::size_t current_requirement{0U};
    std::size_t completed_requirements{0U};
    std::size_t total_requirements{0U};
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    bool transport_open{false};
    bool shutdown_blackout_attempted{false};
    bool shutdown_blackout_succeeded{false};
};

struct RawHardwareTestAttempt {
    std::uint32_t schema_version{kRawHardwareTestAttemptVersion};
    FixtureQualificationAttestation attestation;
    std::string started_at_utc;
    std::string completed_at_utc;
    RawHardwareTestPhase terminal_phase{RawHardwareTestPhase::Idle};
    RawHardwareTestError terminal_error{RawHardwareTestError::None};
    std::string terminal_message;
    std::uint64_t frames_attempted{0U};
    std::uint64_t frames_accepted{0U};
    std::string content_sha256;
};

class RawHardwareTestTransport {
public:
    virtual ~RawHardwareTestTransport() = default;

    [[nodiscard]] virtual bool open(
        const FixtureQualificationBinding& binding) noexcept = 0;
    [[nodiscard]] virtual bool connected() const noexcept = 0;
    [[nodiscard]] virtual bool send(
        const showcore::DmxUniverse& frame) noexcept = 0;
    virtual void close() noexcept = 0;
};

// Production adapter for the already-owned SoundSwitch Micro session. It only
// receives frames produced by RawHardwareTestSession and refuses every other
// backend binding.
class SoundSwitchMicroRawHardwareTestTransport final
    : public RawHardwareTestTransport {
public:
    explicit SoundSwitchMicroRawHardwareTestTransport(
        showcore::SoundSwitchMicroSessionConfig config = {}) noexcept;

    [[nodiscard]] bool open(
        const FixtureQualificationBinding& binding) noexcept override;
    [[nodiscard]] bool connected() const noexcept override;
    [[nodiscard]] bool send(
        const showcore::DmxUniverse& frame) noexcept override;
    void close() noexcept override;

    [[nodiscard]] showcore::SoundSwitchMicroSessionStatus status() const noexcept;

private:
    showcore::SoundSwitchMicroSessionConfig config_{};
    showcore::SoundSwitchMicroSession session_{};
};

[[nodiscard]] const char* raw_hardware_test_phase_name(
    RawHardwareTestPhase phase) noexcept;
[[nodiscard]] const char* raw_hardware_test_error_name(
    RawHardwareTestError error) noexcept;

[[nodiscard]] RawHardwareTestCheck build_raw_hardware_test_plan(
    const ProjectDocument& candidate_project,
    std::string_view input_project_sha256,
    std::string_view fixture_id,
    std::string_view unit_label,
    std::string_view output_backend,
    std::span<const RawHardwareTestSlotCriterion> criteria,
    std::span<const std::string> markers_to_supersede,
    const RawHardwareTestConfig& config,
    RawHardwareTestPlan& plan);

[[nodiscard]] RawHardwareTestCheck validate_raw_hardware_test_plan(
    const RawHardwareTestPlan& plan);

[[nodiscard]] RawHardwareTestCheck make_raw_hardware_test_frame(
    const RawHardwareTestPlan& plan,
    std::size_t requirement_index,
    showcore::DmxUniverse& frame);

class RawHardwareTestSession {
public:
    using TimePoint = std::chrono::steady_clock::time_point;

    RawHardwareTestSession() = default;
    ~RawHardwareTestSession();

    RawHardwareTestSession(const RawHardwareTestSession&) = delete;
    RawHardwareTestSession& operator=(const RawHardwareTestSession&) = delete;

    [[nodiscard]] RawHardwareTestCheck begin(
        RawHardwareTestPlan plan,
        RawHardwareTestRunIdentity identity,
        RawHardwareTestTransport& transport,
        TimePoint now);

    [[nodiscard]] RawHardwareTestCheck poll(TimePoint now);

    [[nodiscard]] RawHardwareTestCheck submit_observation(
        RawHardwareTestObservedResult observation,
        TimePoint now);

    [[nodiscard]] RawHardwareTestCheck cancel(
        std::string_view reason,
        TimePoint now);

    [[nodiscard]] RawHardwareTestCheck make_attempt(
        const ProjectDocument& candidate_project,
        std::string_view completed_at_utc,
        RawHardwareTestAttempt& attempt) const;

    [[nodiscard]] RawHardwareTestSnapshot snapshot() const;
    [[nodiscard]] const RawHardwareTestPlan* plan() const noexcept;
    [[nodiscard]] const std::vector<FixtureQualificationObservation>& observations()
        const noexcept;

private:
    [[nodiscard]] RawHardwareTestCheck start_current(TimePoint now);
    [[nodiscard]] bool send_frame(const showcore::DmxUniverse& frame) noexcept;
    [[nodiscard]] bool send_blackout() noexcept;
    void finish_failure(
        RawHardwareTestError error,
        std::string message,
        bool timed_out,
        bool device_lost);
    void close_transport() noexcept;

    RawHardwareTestPlan plan_{};
    RawHardwareTestRunIdentity identity_{};
    RawHardwareTestTransport* transport_{nullptr};
    RawHardwareTestSnapshot snapshot_{};
    std::vector<FixtureQualificationObservation> observations_{};
    TimePoint session_deadline_{};
    TimePoint observation_deadline_{};
    bool has_plan_{false};
    bool current_blackout_before_{false};
};

[[nodiscard]] RawHardwareTestCheck seal_raw_hardware_test_attempt(
    RawHardwareTestAttempt& attempt);
[[nodiscard]] RawHardwareTestCheck validate_raw_hardware_test_attempt(
    const ProjectDocument& project,
    const RawHardwareTestAttempt& attempt);
[[nodiscard]] std::string serialize_raw_hardware_test_attempt_record(
    const RawHardwareTestAttempt& attempt);
[[nodiscard]] RawHardwareTestCheck parse_raw_hardware_test_attempt_record(
    std::string_view record,
    RawHardwareTestAttempt& attempt);

// Failed attempts are append-only audit evidence but never authorize output.
[[nodiscard]] RawHardwareTestCheck record_raw_hardware_test_attempt(
    ProjectDocument& project,
    const RawHardwareTestAttempt& attempt);

// A completed attempt adds its audit record, immutable attestation, and exact
// marker supersessions as one project transaction.
[[nodiscard]] RawHardwareTestCheck graduate_raw_hardware_test_attempt(
    ProjectDocument& project,
    const RawHardwareTestAttempt& attempt);

}  // namespace emberlights
