#include "emberlights/manual_dmx_test.hpp"

#include "emberlights/file_identity.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace emberlights {
namespace {

constexpr auto kMinimumHoldTimeout = std::chrono::milliseconds{1000};
constexpr auto kMaximumHoldTimeout = std::chrono::milliseconds{30000};
constexpr auto kMinimumSessionTimeout = std::chrono::milliseconds{60000};
constexpr auto kMaximumSessionTimeout = std::chrono::milliseconds{1800000};
constexpr std::uint16_t kMaximumBlackoutRepetitions = 16U;

[[nodiscard]] ManualDmxTestCheck check(
    ManualDmxTestError error,
    std::string message) {
    return {error, std::move(message)};
}

[[nodiscard]] bool valid_adapter_text(std::string_view adapter_id) noexcept {
    return !adapter_id.empty() && adapter_id.size() <= 128U &&
        std::all_of(adapter_id.begin(), adapter_id.end(), [](unsigned char value) {
            return std::isalnum(value) != 0 || value == '-' || value == '_' ||
                value == '.' || value == ':';
        });
}

[[nodiscard]] bool adapter_matches_universe(
    std::string_view adapter_id,
    std::uint8_t universe) noexcept {
    return adapter_id.size() > 3U && adapter_id[adapter_id.size() - 3U] == ':' &&
        adapter_id[adapter_id.size() - 2U] == 'u' &&
        adapter_id.back() == static_cast<char>('0' + universe);
}

[[nodiscard]] bool valid_config(const ManualDmxTestConfig& config) noexcept {
    return valid_adapter_text(config.adapter_id) &&
        config.universe >= 1U &&
        config.universe <= static_cast<std::uint8_t>(showcore::kV1UniverseCount) &&
        adapter_matches_universe(config.adapter_id, config.universe) &&
        config.hold_timeout >= kMinimumHoldTimeout &&
        config.hold_timeout <= kMaximumHoldTimeout &&
        config.session_timeout >= kMinimumSessionTimeout &&
        config.session_timeout <= kMaximumSessionTimeout &&
        config.session_timeout >= config.hold_timeout &&
        config.blackout_frame_repetitions > 0U &&
        config.blackout_frame_repetitions <= kMaximumBlackoutRepetitions &&
        config.maximum_active_channels > 0U &&
        config.maximum_active_channels <= kManualDmxTestMaximumActiveChannels;
}

[[nodiscard]] std::string canonical_plan_text(
    const ManualDmxTestPlan& plan) {
    std::ostringstream output;
    output << "EMBERLIGHTS_MANUAL_DMX_TEST_PLAN\t"
           << plan.schema_version << '\t'
           << plan.config.adapter_id << '\t'
           << static_cast<unsigned int>(plan.config.universe) << '\t'
           << plan.config.hold_timeout.count() << '\t'
           << plan.config.session_timeout.count() << '\t'
           << plan.config.blackout_frame_repetitions << '\t'
           << plan.config.maximum_active_channels;
    return output.str();
}

[[nodiscard]] std::chrono::milliseconds remaining(
    ManualDmxTestSession::TimePoint now,
    ManualDmxTestSession::TimePoint deadline) noexcept {
    if (deadline <= now) {
        return std::chrono::milliseconds{0};
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
}

}  // namespace

SoundSwitchMicroManualDmxTestTransport::
SoundSwitchMicroManualDmxTestTransport(
    showcore::SoundSwitchMicroSessionConfig config) noexcept
    : config_(config) {}

bool SoundSwitchMicroManualDmxTestTransport::open(
    std::string_view adapter_id,
    std::uint8_t universe) noexcept {
    const std::string_view expected = universe == 1U
        ? std::string_view{"soundswitch-micro:u1"}
        : std::string_view{"soundswitch-micro:u2"};
    if (adapter_id != expected || universe == 0U ||
        universe > static_cast<std::uint8_t>(showcore::kV1UniverseCount) ||
        !showcore::valid_soundswitch_micro_session_config(config_)) {
        return false;
    }
    return session_.open(config_);
}

bool SoundSwitchMicroManualDmxTestTransport::connected() const noexcept {
    const auto current = session_.status();
    return current.state == showcore::SoundSwitchMicroLifecycleState::Streaming &&
        current.device_present && current.handle_open && current.warmup_complete;
}

bool SoundSwitchMicroManualDmxTestTransport::send(
    const showcore::DmxUniverse& frame) noexcept {
    return connected() && session_.send(frame);
}

void SoundSwitchMicroManualDmxTestTransport::close() noexcept {
    session_.close();
}

showcore::SoundSwitchMicroSessionStatus
SoundSwitchMicroManualDmxTestTransport::status() const noexcept {
    return session_.status();
}

const char* manual_dmx_test_phase_name(ManualDmxTestPhase phase) noexcept {
    switch (phase) {
    case ManualDmxTestPhase::Idle: return "idle";
    case ManualDmxTestPhase::Opening: return "opening";
    case ManualDmxTestPhase::ArmedBlackout: return "armed-blackout";
    case ManualDmxTestPhase::Holding: return "holding";
    case ManualDmxTestPhase::Complete: return "complete";
    case ManualDmxTestPhase::TimedOut: return "timed-out";
    case ManualDmxTestPhase::Failed: return "failed";
    case ManualDmxTestPhase::Cancelled: return "cancelled";
    }
    return "unknown";
}

const char* manual_dmx_test_error_name(ManualDmxTestError error) noexcept {
    switch (error) {
    case ManualDmxTestError::None: return "none";
    case ManualDmxTestError::InvalidConfiguration:
        return "invalid-configuration";
    case ManualDmxTestError::InvalidAdapter: return "invalid-adapter";
    case ManualDmxTestError::InvalidValues: return "invalid-values";
    case ManualDmxTestError::InvalidAcknowledgement:
        return "invalid-acknowledgement";
    case ManualDmxTestError::AlreadyStarted: return "already-started";
    case ManualDmxTestError::NotActive: return "not-active";
    case ManualDmxTestError::OpenFailed: return "open-failed";
    case ManualDmxTestError::DeviceLost: return "device-lost";
    case ManualDmxTestError::BlackoutFailed: return "blackout-failed";
    case ManualDmxTestError::FrameWriteFailed: return "frame-write-failed";
    case ManualDmxTestError::SessionTimedOut: return "session-timed-out";
    case ManualDmxTestError::Cancelled: return "cancelled";
    }
    return "unknown";
}

ManualDmxTestCheck build_manual_dmx_test_plan(
    const ManualDmxTestConfig& config,
    ManualDmxTestPlan& plan) {
    if (!valid_adapter_text(config.adapter_id)) {
        return check(
            ManualDmxTestError::InvalidAdapter,
            "Select one bounded output adapter before arming raw DMX.");
    }
    if (!valid_config(config)) {
        return check(
            ManualDmxTestError::InvalidConfiguration,
            "The adapter/universe, hold timeout, session timeout, blackout count, or active-channel limit is outside the Advanced manual-DMX bounds.");
    }
    ManualDmxTestPlan candidate;
    candidate.config = config;
    candidate.plan_sha256 = sha256_text(canonical_plan_text(candidate));
    plan = std::move(candidate);
    return {};
}

ManualDmxTestCheck validate_manual_dmx_test_plan(
    const ManualDmxTestPlan& plan) {
    if (plan.schema_version != kManualDmxTestPlanVersion ||
        !valid_config(plan.config)) {
        return check(
            ManualDmxTestError::InvalidConfiguration,
            "The Advanced manual-DMX plan schema or bounded configuration is invalid.");
    }
    if (!is_sha256_digest(plan.plan_sha256) ||
        plan.plan_sha256 != sha256_text(canonical_plan_text(plan))) {
        return check(
            ManualDmxTestError::InvalidConfiguration,
            "The Advanced manual-DMX plan identity does not match its configuration.");
    }
    return {};
}

std::string manual_dmx_test_acknowledgement(
    const ManualDmxTestPlan& plan) {
    if (!validate_manual_dmx_test_plan(plan).ok()) {
        return {};
    }
    return "ARM RAW DMX U" + std::to_string(plan.config.universe) + " " +
        plan.plan_sha256;
}

bool manual_dmx_test_acknowledged(
    const ManualDmxTestPlan& plan,
    std::string_view acknowledgement) {
    return acknowledgement == manual_dmx_test_acknowledgement(plan);
}

ManualDmxTestCheck make_manual_dmx_test_frame(
    const ManualDmxTestPlan& plan,
    std::span<const ManualDmxChannelValue> values,
    showcore::DmxUniverse& frame,
    std::vector<ManualDmxChannelValue>& normalized_values) {
    const auto valid_plan = validate_manual_dmx_test_plan(plan);
    if (!valid_plan.ok()) {
        return valid_plan;
    }
    if (values.size() > plan.config.maximum_active_channels) {
        return check(
            ManualDmxTestError::InvalidValues,
            "The requested preset exceeds the armed active-channel limit.");
    }

    std::vector<bool> seen(showcore::kUniverseSlots, false);
    showcore::DmxUniverse candidate{};
    std::vector<ManualDmxChannelValue> normalized;
    normalized.reserve(values.size());
    for (const auto& item : values) {
        if (item.channel == 0U || item.channel > showcore::kUniverseSlots ||
            seen[item.channel - 1U]) {
            return check(
                ManualDmxTestError::InvalidValues,
                "Every manual DMX preset slot must be a unique channel from 1 through 512.");
        }
        seen[item.channel - 1U] = true;
        if (item.value == 0U) {
            continue;
        }
        candidate[item.channel - 1U] = item.value;
        normalized.push_back(item);
    }
    std::sort(
        normalized.begin(), normalized.end(),
        [](const auto& left, const auto& right) {
            return left.channel < right.channel;
        });
    frame = candidate;
    normalized_values = std::move(normalized);
    return {};
}

ManualDmxTestSession::~ManualDmxTestSession() {
    if (active()) {
        terminal_close();
    }
}

ManualDmxTestCheck ManualDmxTestSession::begin(
    ManualDmxTestPlan plan,
    std::string_view acknowledgement,
    ManualDmxTestTransport& transport,
    TimePoint now) {
    if (phase_ != ManualDmxTestPhase::Idle) {
        return check(
            ManualDmxTestError::AlreadyStarted,
            "This manual DMX session is single-use; create a new session to re-arm output.");
    }
    const auto valid_plan = validate_manual_dmx_test_plan(plan);
    if (!valid_plan.ok()) {
        return valid_plan;
    }
    if (!manual_dmx_test_acknowledged(plan, acknowledgement)) {
        return check(
            ManualDmxTestError::InvalidAcknowledgement,
            "The exact adapter/universe arming acknowledgement did not match; no output device was opened.");
    }

    plan_ = std::move(plan);
    transport_ = &transport;
    phase_ = ManualDmxTestPhase::Opening;
    session_deadline_ = now + plan_.config.session_timeout;
    if (!transport_->open(plan_.config.adapter_id, plan_.config.universe)) {
        transport_->close();
        transport_ = nullptr;
        phase_ = ManualDmxTestPhase::Failed;
        error_ = ManualDmxTestError::OpenFailed;
        message_ = "The selected output adapter could not be opened; no manual frame was sent.";
        return check(error_, message_);
    }
    transport_open_ = true;
    if (!transport_->connected()) {
        finish_failure(
            ManualDmxTestError::DeviceLost,
            "The selected output adapter did not reach a connected streaming state.");
        return check(error_, message_);
    }
    if (!send_blackout()) {
        finish_failure(
            ManualDmxTestError::BlackoutFailed,
            "The required blackout-before sequence was not accepted.");
        return check(error_, message_);
    }
    phase_ = ManualDmxTestPhase::ArmedBlackout;
    message_ = "Raw DMX is armed with every channel at zero. APPLY or SET remains bounded by automatic blackout.";
    return {};
}

ManualDmxTestCheck ManualDmxTestSession::apply(
    std::span<const ManualDmxChannelValue> values,
    TimePoint now) {
    const auto ready = ensure_active(now);
    if (!ready.ok()) {
        return ready;
    }
    showcore::DmxUniverse frame{};
    std::vector<ManualDmxChannelValue> normalized;
    const auto built = make_manual_dmx_test_frame(
        plan_, values, frame, normalized);
    if (!built.ok()) {
        return built;
    }
    if (normalized.empty()) {
        return blackout_now(now);
    }
    if (phase_ == ManualDmxTestPhase::Holding && !send_blackout()) {
        finish_failure(
            ManualDmxTestError::BlackoutFailed,
            "The required blackout-before-preset sequence was not accepted.");
        return check(error_, message_);
    }
    if (!send_frame(frame)) {
        finish_failure(
            ManualDmxTestError::FrameWriteFailed,
            "The explicit manual DMX preset was not accepted by the selected adapter.");
        return check(error_, message_);
    }
    held_values_ = std::move(normalized);
    held_frame_sha256_ = sha256_bytes(frame);
    hold_deadline_ = now + plan_.config.hold_timeout;
    phase_ = ManualDmxTestPhase::Holding;
    message_ = "The exact displayed manual DMX preset is active until its bounded hold timeout.";
    return {};
}

ManualDmxTestCheck ManualDmxTestSession::blackout_now(TimePoint now) {
    const auto ready = ensure_active(now);
    if (!ready.ok()) {
        return ready;
    }
    if (!send_blackout()) {
        finish_failure(
            ManualDmxTestError::BlackoutFailed,
            "Blackout Now was not accepted by the selected adapter.");
        return check(error_, message_);
    }
    ++explicit_blackouts_;
    held_values_.clear();
    held_frame_sha256_.clear();
    hold_deadline_ = {};
    phase_ = ManualDmxTestPhase::ArmedBlackout;
    message_ = "BLACKOUT NOW: every channel is zero; the bounded session remains armed.";
    return {};
}

ManualDmxTestCheck ManualDmxTestSession::poll(TimePoint now) {
    const auto ready = ensure_active(now);
    if (!ready.ok()) {
        return ready;
    }
    if (phase_ == ManualDmxTestPhase::Holding && now >= hold_deadline_) {
        if (!send_blackout()) {
            finish_failure(
                ManualDmxTestError::BlackoutFailed,
                "The automatic hold-timeout blackout was not accepted.");
            return check(error_, message_);
        }
        ++automatic_blackouts_;
        held_values_.clear();
        held_frame_sha256_.clear();
        hold_deadline_ = {};
        phase_ = ManualDmxTestPhase::ArmedBlackout;
        message_ = "Automatic blackout completed after the bounded hold timeout.";
    }
    return {};
}

ManualDmxTestCheck ManualDmxTestSession::stop(TimePoint now) {
    const auto ready = ensure_active(now);
    if (!ready.ok()) {
        return ready;
    }
    terminal_close();
    if (!terminal_blackout_succeeded_) {
        phase_ = ManualDmxTestPhase::Failed;
        error_ = ManualDmxTestError::BlackoutFailed;
        message_ = "The session closed, but its terminal blackout was not accepted.";
        return check(error_, message_);
    }
    phase_ = ManualDmxTestPhase::Complete;
    error_ = ManualDmxTestError::None;
    message_ = "Manual DMX session stopped after terminal blackout and adapter close.";
    return {};
}

ManualDmxTestCheck ManualDmxTestSession::cancel(
    std::string_view reason,
    TimePoint now) {
    const auto ready = ensure_active(now);
    if (!ready.ok()) {
        return ready;
    }
    const auto bounded_reason = reason.empty() || reason.size() > 512U
        ? std::string{"Operator cancelled the manual DMX session."}
        : std::string(reason);
    terminal_close();
    if (!terminal_blackout_succeeded_) {
        phase_ = ManualDmxTestPhase::Failed;
        error_ = ManualDmxTestError::BlackoutFailed;
        message_ = bounded_reason + " Terminal blackout was not accepted.";
        return check(error_, message_);
    }
    phase_ = ManualDmxTestPhase::Cancelled;
    error_ = ManualDmxTestError::Cancelled;
    message_ = bounded_reason;
    return check(error_, message_);
}

ManualDmxTestSnapshot ManualDmxTestSession::snapshot(TimePoint now) const {
    ManualDmxTestSnapshot result;
    result.phase = phase_;
    result.error = error_;
    result.message = message_;
    result.adapter_id = plan_.config.adapter_id;
    result.universe = plan_.config.universe;
    result.held_values = held_values_;
    result.held_frame_sha256 = held_frame_sha256_;
    result.hold_remaining = phase_ == ManualDmxTestPhase::Holding
        ? remaining(now, hold_deadline_)
        : std::chrono::milliseconds{0};
    result.session_remaining = active()
        ? remaining(now, session_deadline_)
        : std::chrono::milliseconds{0};
    result.frames_attempted = frames_attempted_;
    result.frames_accepted = frames_accepted_;
    result.explicit_blackouts = explicit_blackouts_;
    result.automatic_blackouts = automatic_blackouts_;
    result.transport_open = transport_open_;
    result.terminal_blackout_attempted = terminal_blackout_attempted_;
    result.terminal_blackout_succeeded = terminal_blackout_succeeded_;
    return result;
}

const ManualDmxTestPlan* ManualDmxTestSession::plan() const noexcept {
    return phase_ == ManualDmxTestPhase::Idle ? nullptr : &plan_;
}

bool ManualDmxTestSession::send_frame(
    const showcore::DmxUniverse& frame) noexcept {
    ++frames_attempted_;
    if (transport_ == nullptr || !transport_open_ || !transport_->connected() ||
        !transport_->send(frame)) {
        return false;
    }
    ++frames_accepted_;
    return true;
}

bool ManualDmxTestSession::send_blackout() noexcept {
    showcore::DmxUniverse blackout{};
    bool accepted = true;
    for (std::uint16_t index = 0U;
         index < plan_.config.blackout_frame_repetitions;
         ++index) {
        accepted = send_frame(blackout) && accepted;
    }
    return accepted;
}

bool ManualDmxTestSession::active() const noexcept {
    return phase_ == ManualDmxTestPhase::Opening ||
        phase_ == ManualDmxTestPhase::ArmedBlackout ||
        phase_ == ManualDmxTestPhase::Holding;
}

ManualDmxTestCheck ManualDmxTestSession::ensure_active(TimePoint now) {
    if (!active() || transport_ == nullptr || !transport_open_) {
        return check(
            ManualDmxTestError::NotActive,
            "The manual DMX session is not armed and active.");
    }
    if (now >= session_deadline_) {
        error_ = ManualDmxTestError::SessionTimedOut;
        message_ = "The bounded manual DMX session reached its safety timeout.";
        terminal_close();
        if (!terminal_blackout_succeeded_) {
            phase_ = ManualDmxTestPhase::Failed;
            error_ = ManualDmxTestError::BlackoutFailed;
            message_ += " Terminal blackout was not accepted.";
        } else {
            phase_ = ManualDmxTestPhase::TimedOut;
        }
        return check(error_, message_);
    }
    if (!transport_->connected()) {
        finish_failure(
            ManualDmxTestError::DeviceLost,
            "The selected output adapter disconnected during manual DMX testing.");
        return check(error_, message_);
    }
    return {};
}

void ManualDmxTestSession::finish_failure(
    ManualDmxTestError error,
    std::string message) noexcept {
    error_ = error;
    message_ = std::move(message);
    phase_ = ManualDmxTestPhase::Failed;
    terminal_close();
}

void ManualDmxTestSession::terminal_close() noexcept {
    if (transport_ == nullptr || !transport_open_) {
        return;
    }
    terminal_blackout_attempted_ = true;
    terminal_blackout_succeeded_ = send_blackout();
    transport_->close();
    transport_open_ = false;
    held_values_.clear();
    held_frame_sha256_.clear();
    hold_deadline_ = {};
}

}  // namespace emberlights
