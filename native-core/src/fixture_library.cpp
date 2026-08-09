#include "showcore/fixture_library.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace showcore {

namespace {

template <std::size_t Size>
void copy_text(std::array<char, Size>& destination, std::string_view source) noexcept {
    destination.fill('\0');
    std::copy(source.begin(), source.end(), destination.begin());
}

[[nodiscard]] bool is_hazardous(Property property) noexcept {
    return property == Property::Fog || property == Property::Haze ||
        property == Property::Laser || property == Property::Spark;
}

[[nodiscard]] bool text_too_long(std::string_view value) noexcept {
    return value.size() > kFixtureProfileTextLength;
}

}  // namespace

void CompiledFixtureLibrary::clear() noexcept {
    profile_count_ = 0;
    channel_count_ = 0;
}

FixtureIngestResult CompiledFixtureLibrary::ingest(
    const FixtureProfileDraft& draft) noexcept {
    if (profile_count_ >= profiles_.size()) {
        return {FixtureIngestError::Capacity, profile_count_, {}};
    }
    if (draft.stable_id.empty()) {
        return {FixtureIngestError::MissingStableId, profile_count_, {}};
    }
    if (draft.manufacturer.empty()) {
        return {FixtureIngestError::MissingManufacturer, profile_count_, {}};
    }
    if (draft.model.empty()) {
        return {FixtureIngestError::MissingModel, profile_count_, {}};
    }
    if (draft.mode.empty()) {
        return {FixtureIngestError::MissingMode, profile_count_, {}};
    }
    if (draft.display_name.empty()) {
        return {FixtureIngestError::MissingDisplayName, profile_count_, {}};
    }
    if (draft.source == FixtureProfileSource::Unknown) {
        return {FixtureIngestError::MissingSource, profile_count_, {}};
    }
    if (draft.source_revision.empty()) {
        return {FixtureIngestError::MissingSourceRevision, profile_count_, {}};
    }
    if (text_too_long(draft.stable_id) || text_too_long(draft.manufacturer) ||
        text_too_long(draft.model) || text_too_long(draft.mode) ||
        text_too_long(draft.display_name) || text_too_long(draft.source_revision)) {
        return {FixtureIngestError::TextTooLong, profile_count_, {}};
    }
    if (find(draft.stable_id) != nullptr) {
        return {FixtureIngestError::DuplicateStableId, profile_count_, {}};
    }
    if (draft.channel_count > channels_.size() - channel_count_) {
        return {FixtureIngestError::ChannelCapacity, profile_count_, {}};
    }

    const FixtureProfile candidate{
        draft.display_name.data(),
        draft.channels,
        draft.channel_count,
        draft.footprint};
    const auto profile_result = validate_fixture_profile(candidate);
    if (!profile_result) {
        return {FixtureIngestError::InvalidProfile, profile_count_, profile_result};
    }

    auto& profile = profiles_[profile_count_];
    copy_text(profile.stable_id, draft.stable_id);
    copy_text(profile.manufacturer, draft.manufacturer);
    copy_text(profile.model, draft.model);
    copy_text(profile.mode, draft.mode);
    copy_text(profile.display_name, draft.display_name);
    copy_text(profile.source_revision, draft.source_revision);
    profile.source = draft.source;
    profile.has_hazardous_channels = false;

    const auto first_channel = channel_count_;
    for (std::size_t index = 0; index < draft.channel_count; ++index) {
        channels_[channel_count_++] = draft.channels[index];
        profile.has_hazardous_channels =
            profile.has_hazardous_channels || is_hazardous(draft.channels[index].property);
    }
    profile.runtime = {
        profile.display_name.data(),
        channels_.data() + first_channel,
        draft.channel_count,
        draft.footprint};

    const auto inserted_index = profile_count_++;
    return {FixtureIngestError::None, inserted_index, {}};
}

const CompiledFixtureProfile* CompiledFixtureLibrary::at(std::size_t index) const noexcept {
    return index < profile_count_ ? &profiles_[index] : nullptr;
}

const CompiledFixtureProfile* CompiledFixtureLibrary::find(
    std::string_view stable_id) const noexcept {
    for (std::size_t index = 0; index < profile_count_; ++index) {
        if (std::string_view(profiles_[index].stable_id.data()) == stable_id) {
            return &profiles_[index];
        }
    }
    return nullptr;
}

}  // namespace showcore
