#pragma once

#include "emberlights/autoloop_source.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace emberlights {

// Rich source is carried additively inside a format-1 project's unknown-record
// channel. The typed format-1 AUTOLOOP/STEP contract remains unchanged.
inline constexpr std::string_view kPersistedAutoloopSourceRecordKind =
    "EMBERLIGHTS_AUTOLOOP_SOURCE_RECORD";
inline constexpr std::uint32_t kPersistedAutoloopSourceRecordVersion = 1U;
inline constexpr std::size_t kMaximumPersistedAutoloopSourceBytes =
    8U * 1024U * 1024U;
inline constexpr std::size_t kNoPersistedAutoloopSourceRecord =
    std::numeric_limits<std::size_t>::max();

struct PersistedAutoloopSourceStamp {
    bool present{false};
    std::uint32_t record_version{0U};
    std::uint32_t source_format_version{0U};
    std::string source_digest;

    [[nodiscard]] friend bool operator==(
        const PersistedAutoloopSourceStamp&,
        const PersistedAutoloopSourceStamp&) = default;
};

enum class AutoloopPersistenceError : std::uint8_t {
    None,
    DuplicateRecord,
    MalformedRecord,
    UnsupportedRecordVersion,
    UnsupportedSourceVersion,
    SourceTooLarge,
    DigestMismatch,
    NonCanonicalSource,
    InvalidSource
};

struct PersistedAutoloopSourceResult {
    AutoloopPersistenceError error{AutoloopPersistenceError::None};
    bool changed{false};
    std::size_t record_index{kNoPersistedAutoloopSourceRecord};
    PersistedAutoloopSourceStamp stamp;
    AutoloopSourceDocument source;
    std::string message;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == AutoloopPersistenceError::None;
    }
};

// Recognized records are strict: duplicates, non-canonical envelopes, stale
// versions, invalid source bytes, and digest mismatches all fail closed.
// Unrecognized project records are ignored and remain byte-for-byte owned by
// their original subsystem.
[[nodiscard]] PersistedAutoloopSourceResult inspect_persisted_autoloop_source(
    const ProjectDocument& project);

// Replaces the one recognized record in place, or appends it when absent.
// Mutation occurs only after both the existing record set and the new source
// have passed validation. Every unrelated unknown record retains its bytes and
// relative order.
[[nodiscard]] PersistedAutoloopSourceResult upsert_persisted_autoloop_source(
    ProjectDocument& project,
    const AutoloopSourceDocument& source);

[[nodiscard]] const char* autoloop_persistence_error_name(
    AutoloopPersistenceError error) noexcept;

}  // namespace emberlights
