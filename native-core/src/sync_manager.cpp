#include "showcore/sync_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace showcore {

void SyncManager::reset() noexcept {
    state_ = SyncState::Waiting;
    has_os2l_ = false;
    os2l_bpm_ = 0.0;
    os2l_position_ = 0.0;
    os2l_timestamp_ms_ = 0;
    audio_ = {};
    manual_bpm_ = 0.0;
    manual_position_ = 0.0;
    manual_timestamp_ms_ = 0;
    stable_recovery_beats_ = 0;
}

void SyncManager::on_os2l_beat(
    std::int64_t position,
    double bpm,
    std::uint64_t timestamp_ms) noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        return;
    }

    const bool needs_recovery =
        state_ == SyncState::AudioFallback ||
        state_ == SyncState::Manual ||
        state_ == SyncState::SafeUnsynchronized ||
        state_ == SyncState::Recovering;

    if (needs_recovery && has_os2l_) {
        if (std::abs(bpm - os2l_bpm_) <= config_.relock_bpm_tolerance) {
            stable_recovery_beats_ = static_cast<std::uint8_t>(stable_recovery_beats_ + 1U);
        } else {
            stable_recovery_beats_ = 1U;
        }
        state_ = stable_recovery_beats_ >= config_.stable_beats_to_relock
            ? SyncState::Os2lHealthy
            : SyncState::Recovering;
    } else {
        state_ = SyncState::Os2lHealthy;
        stable_recovery_beats_ = config_.stable_beats_to_relock;
    }

    has_os2l_ = true;
    os2l_bpm_ = bpm;
    os2l_position_ = static_cast<double>(position);
    os2l_timestamp_ms_ = timestamp_ms;
}

void SyncManager::on_audio_clock(AudioClockSample sample) noexcept {
    if (!sample.valid || !std::isfinite(sample.bpm) || sample.bpm <= 0.0 ||
        !std::isfinite(sample.beat_position)) {
        return;
    }
    sample.confidence = std::clamp(sample.confidence, 0.0F, 1.0F);
    audio_ = sample;
}

void SyncManager::set_manual_bpm(double bpm, std::uint64_t timestamp_ms) noexcept {
    if (!std::isfinite(bpm) || bpm <= 0.0) {
        manual_bpm_ = 0.0;
        return;
    }
    if (manual_bpm_ > 0.0) {
        manual_position_ = estimate(manual_position_, manual_bpm_, manual_timestamp_ms_, timestamp_ms);
    }
    manual_bpm_ = bpm;
    manual_timestamp_ms_ = timestamp_ms;
}

double SyncManager::estimate(
    double anchor_position,
    double bpm,
    std::uint64_t anchor_ms,
    std::uint64_t now_ms) const noexcept {
    if (now_ms <= anchor_ms || bpm <= 0.0) {
        return anchor_position;
    }
    return anchor_position + static_cast<double>(now_ms - anchor_ms) * bpm / 60000.0;
}

ClockSnapshot SyncManager::tick(std::uint64_t now_ms) noexcept {
    if (has_os2l_) {
        const auto elapsed = now_ms >= os2l_timestamp_ms_ ? now_ms - os2l_timestamp_ms_ : 0U;
        const auto beat_ms = 60000.0 / os2l_bpm_;
        const auto hold_ms = std::max(
            config_.minimum_hold_ms,
            static_cast<std::uint64_t>(std::ceil(beat_ms * config_.hold_beats)));
        const auto fallback_ms = std::max(
            config_.minimum_fallback_ms,
            static_cast<std::uint64_t>(std::ceil(beat_ms * config_.fallback_beats)));

        if (elapsed > fallback_ms) {
            const bool audio_is_fresh = audio_.valid && now_ms >= audio_.timestamp_ms &&
                now_ms - audio_.timestamp_ms <= config_.audio_freshness_ms &&
                audio_.confidence >= config_.minimum_audio_confidence;
            if (audio_is_fresh) {
                state_ = SyncState::AudioFallback;
            } else if (manual_bpm_ > 0.0) {
                state_ = SyncState::Manual;
            } else {
                state_ = SyncState::SafeUnsynchronized;
            }
            stable_recovery_beats_ = 0;
        } else if (elapsed > hold_ms && state_ != SyncState::AudioFallback &&
                   state_ != SyncState::Manual && state_ != SyncState::SafeUnsynchronized) {
            state_ = SyncState::PredictiveHold;
        }
    } else if (manual_bpm_ > 0.0) {
        state_ = SyncState::Manual;
    } else {
        state_ = SyncState::SafeUnsynchronized;
    }

    switch (state_) {
    case SyncState::Os2lHealthy:
        return {state_, ClockSource::Os2l, os2l_bpm_,
            estimate(os2l_position_, os2l_bpm_, os2l_timestamp_ms_, now_ms), 1.0F, true};
    case SyncState::Recovering:
        return {state_, ClockSource::Os2l, os2l_bpm_,
            estimate(os2l_position_, os2l_bpm_, os2l_timestamp_ms_, now_ms), 0.85F, false};
    case SyncState::PredictiveHold:
        return {state_, ClockSource::Prediction, os2l_bpm_,
            estimate(os2l_position_, os2l_bpm_, os2l_timestamp_ms_, now_ms), 0.75F, false};
    case SyncState::AudioFallback:
        return {state_, ClockSource::Audio, audio_.bpm,
            estimate(audio_.beat_position, audio_.bpm, audio_.timestamp_ms, now_ms),
            audio_.confidence, false};
    case SyncState::Manual:
        return {state_, ClockSource::Manual, manual_bpm_,
            estimate(manual_position_, manual_bpm_, manual_timestamp_ms_, now_ms), 0.5F, false};
    case SyncState::SafeUnsynchronized:
    case SyncState::Waiting:
    default:
        return {state_, ClockSource::None, 0.0, 0.0, 0.0F, false};
    }
}

}  // namespace showcore
