#pragma once

#include "emberlights/static_look_authoring.hpp"

#include "showcore/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace emberlights {

enum class StaticLookPreviewError : std::uint8_t {
    None,
    LookNotFound,
    ValidationFailed,
    CompilationFailed,
    LookPlaybackFailed
};

struct StaticLookPreviewChannel {
    std::string fixture_id;
    std::string fixture_name;
    std::string profile_id;
    std::string profile_mode;
    std::string source_revision;
    std::uint8_t universe{1U};
    std::uint16_t address{1U};
    std::uint16_t channel{1U};
    showcore::Property property{showcore::Property::Count};
    showcore::ChannelEncoding encoding{showcore::ChannelEncoding::Linear8};
    showcore::PropertyValue authored_value{};
    showcore::LayerId winning_layer{showcore::LayerId::Idle};
    std::uint8_t rendered_byte{0U};
};

struct StaticLookPreview {
    StaticLookPreviewError error{StaticLookPreviewError::None};
    ProjectValidation validation;
    showcore::DmxFrames frames{};
    std::vector<StaticLookPreviewChannel> channels;
    std::vector<std::string> warnings;
    std::string frame_sha256;

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == StaticLookPreviewError::None;
    }
};

// Offline only: compiles the same immutable show and runs the same semantic
// renderer used by Runner. It opens no adapter and writes no hardware.
[[nodiscard]] StaticLookPreview preview_static_look(
    const ProjectDocument& project,
    std::string_view look_id);

[[nodiscard]] StaticLookPreview preview_static_look_draft(
    const ProjectDocument& project,
    const StaticLookDraft& draft);

[[nodiscard]] std::string format_static_look_preview(
    const StaticLookPreview& preview);

}  // namespace emberlights
