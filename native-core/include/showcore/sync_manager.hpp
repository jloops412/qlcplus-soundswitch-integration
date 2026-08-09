#pragma once

#include <cstdint>

namespace showcore {

enum class SyncState : std::uint8_t {
    Waiting,
    Os2lHealthy,
    PredictiveHold,
    AudioFallback,
    Recovering,
    Manual,
    SafeUnsynchronized
};

enum class ClockSource : std::uint8_t {
    None,
    Os2l,
    Prediction,
    Audio,
    Manual
};

struct SyncConfig {
    std::uint64_t minimum_hold_ms{700};
    std::uint64_t minimum_fallback_ms{1800};
    double hold_beats{2.2};
    double fallback_beats{5.0};
    float minimum_audio_confidence{0.55F};
    std::uint64_t audio_freshness_ms{1200};
    std::uint8_t stable_beats_to_relock{3};
    double relock_bpm_tolerance{3.0};
};

struct AudioClockSample {
    bool valid{false};
    double bpm{0.0};
    double beat_position{0.0};
    float confidence{0.0F};
    std::uint64_t timestamp_ms{0};
};

struct ClockSnapshot {
    SyncState state{SyncState::Waiting};
    ClockSource source{ClockSource::None};
    double bpm{0.0};
    double beat_position{0.0};
    float confidence{0.0F};
    bool exact_transport{false};
};

class SyncManager {
public:
    explicit SyncManager(SyncConfig config = {}) noexcept : config_(config) {}

    void reset() noexcept;
    void on_os2l_beat(std::int64_t position, double bpm, std::uint64_t timestamp_ms) noexcept;
    void on_audio_clock(AudioClockSample sample) noexcept;
    void set_manual_bpm(double bpm, std::uint64_t timestamp_ms) noexcept;
    [[nodiscard]] ClockSnapshot tick(std::uint64_t now_ms) noexcept;

private:
    [[nodiscard]] double estimate(
        double anchor_position,
        double bpm,
        std::uint64_t anchor_ms,
        std::uint64_t now_ms) const noexcept;

    SyncConfig config_{};
    SyncState state_{SyncState::Waiting};
    bool has_os2l_{false};
    double os2l_bpm_{0.0};
    double os2l_position_{0.0};
    std::uint64_t os2l_timestamp_ms_{0};
    AudioClockSample audio_{};
    double manual_bpm_{0.0};
    double manual_position_{0.0};
    std::uint64_t manual_timestamp_ms_{0};
    std::uint8_t stable_recovery_beats_{0};
};

}  // namespace showcore
