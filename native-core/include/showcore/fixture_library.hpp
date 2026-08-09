#pragma once

#include "showcore/fixture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace showcore {

inline constexpr std::size_t kMaxCompiledFixtureProfiles = kMaxFixtures;
inline constexpr std::size_t kMaxCompiledChannelMappings = 2048;
inline constexpr std::size_t kFixtureProfileTextLength = 96;

enum class FixtureProfileSource : std::uint8_t {
    Unknown,
    BuiltIn,
    OpenFixtureLibrary,
    QlcPlus,
    Local,
    Migrated
};

struct FixtureProfileDraft {
    std::string_view stable_id{};
    std::string_view manufacturer{};
    std::string_view model{};
    std::string_view mode{};
    std::string_view display_name{};
    FixtureProfileSource source{FixtureProfileSource::Unknown};
    std::string_view source_revision{};
    const ChannelMapping* channels{nullptr};
    std::size_t channel_count{0};
    std::uint16_t footprint{0};
};

struct CompiledFixtureProfile {
    FixtureProfile runtime{};
    std::array<char, kFixtureProfileTextLength + 1U> stable_id{};
    std::array<char, kFixtureProfileTextLength + 1U> manufacturer{};
    std::array<char, kFixtureProfileTextLength + 1U> model{};
    std::array<char, kFixtureProfileTextLength + 1U> mode{};
    std::array<char, kFixtureProfileTextLength + 1U> display_name{};
    std::array<char, kFixtureProfileTextLength + 1U> source_revision{};
    FixtureProfileSource source{FixtureProfileSource::Unknown};
    bool has_hazardous_channels{false};
};

enum class FixtureIngestError : std::uint8_t {
    None,
    Capacity,
    ChannelCapacity,
    MissingStableId,
    MissingManufacturer,
    MissingModel,
    MissingMode,
    MissingDisplayName,
    MissingSource,
    MissingSourceRevision,
    TextTooLong,
    DuplicateStableId,
    InvalidProfile
};

struct FixtureIngestResult {
    FixtureIngestError error{FixtureIngestError::None};
    std::size_t profile_index{0};
    ProfileResult profile_result{};

    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return error == FixtureIngestError::None;
    }
};

class CompiledFixtureLibrary {
public:
    CompiledFixtureLibrary() noexcept = default;
    CompiledFixtureLibrary(const CompiledFixtureLibrary&) = delete;
    CompiledFixtureLibrary& operator=(const CompiledFixtureLibrary&) = delete;
    CompiledFixtureLibrary(CompiledFixtureLibrary&&) = delete;
    CompiledFixtureLibrary& operator=(CompiledFixtureLibrary&&) = delete;

    void clear() noexcept;
    [[nodiscard]] FixtureIngestResult ingest(const FixtureProfileDraft& draft) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return profile_count_; }
    [[nodiscard]] std::size_t channel_mapping_count() const noexcept { return channel_count_; }
    [[nodiscard]] const CompiledFixtureProfile* at(std::size_t index) const noexcept;
    [[nodiscard]] const CompiledFixtureProfile* find(std::string_view stable_id) const noexcept;

private:
    std::array<CompiledFixtureProfile, kMaxCompiledFixtureProfiles> profiles_{};
    std::array<ChannelMapping, kMaxCompiledChannelMappings> channels_{};
    std::size_t profile_count_{0};
    std::size_t channel_count_{0};
};

}  // namespace showcore
