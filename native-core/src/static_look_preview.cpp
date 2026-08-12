#include "emberlights/static_look_preview.hpp"

#include "emberlights/compiler.hpp"
#include "emberlights/file_identity.hpp"

#include "showcore/look.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace emberlights {
namespace {

[[nodiscard]] const LookAssignmentDefinition* find_assignment(
    const LookDefinition& look,
    std::string_view fixture_id,
    showcore::Property property) noexcept {
    const auto found = std::find_if(
        look.assignments.begin(), look.assignments.end(),
        [fixture_id, property](const auto& assignment) {
            return assignment.fixture_id == fixture_id &&
                assignment.property == property;
        });
    return found == look.assignments.end() ? nullptr : &*found;
}

[[nodiscard]] std::string mode_text(showcore::ValueMode mode) {
    switch (mode) {
    case showcore::ValueMode::Release: return "RELEASE";
    case showcore::ValueMode::Set: return "SET";
    case showcore::ValueMode::ForceZero: return "FORCE_ZERO";
    }
    return "INVALID";
}

[[nodiscard]] std::string layer_text(showcore::LayerId layer) {
    switch (layer) {
    case showcore::LayerId::Idle: return "Idle/default";
    case showcore::LayerId::Autonomous: return "Autonomous";
    case showcore::LayerId::TrackScript: return "TrackScript";
    case showcore::LayerId::ManualAutoloop: return "ManualAutoloop";
    case showcore::LayerId::EventMoment: return "EventMoment";
    case showcore::LayerId::ManualOverride: return "ManualOverride";
    case showcore::LayerId::Emergency: return "Emergency";
    case showcore::LayerId::Safety: return "Safety";
    case showcore::LayerId::Count: return "Invalid";
    }
    return "Invalid";
}

[[nodiscard]] bool emitter_on(
    const LookDefinition& look,
    std::string_view fixture_id) noexcept {
    return std::any_of(
        look.assignments.begin(), look.assignments.end(),
        [fixture_id](const auto& assignment) {
            return assignment.fixture_id == fixture_id &&
                std::find(kDirectEmitterProperties.begin(),
                          kDirectEmitterProperties.end(),
                          assignment.property) != kDirectEmitterProperties.end() &&
                assignment.value.mode == showcore::ValueMode::Set &&
                assignment.value.value > 0.0F;
        });
}

[[nodiscard]] std::string frame_digest(const showcore::DmxFrames& frames) {
    std::array<std::uint8_t,
               showcore::kV1UniverseCount * showcore::kUniverseSlots> bytes{};
    std::size_t offset = 0U;
    for (const auto& universe : frames.universes) {
        std::copy(universe.begin(), universe.end(), bytes.begin() + offset);
        offset += universe.size();
    }
    return sha256_bytes(bytes);
}

}  // namespace

StaticLookPreview preview_static_look(
    const ProjectDocument& project,
    std::string_view look_id) {
    StaticLookPreview result;
    const auto source = std::find_if(
        project.looks.begin(), project.looks.end(),
        [look_id](const auto& look) { return look.id == look_id; });
    if (source == project.looks.end()) {
        result.error = StaticLookPreviewError::LookNotFound;
        return result;
    }
    const auto look_index = static_cast<std::size_t>(source - project.looks.begin());
    auto compilation = compile_project(project);
    result.validation = compilation.validation;
    if (!compilation.validation.ok()) {
        result.error = StaticLookPreviewError::ValidationFailed;
        return result;
    }
    if (!compilation || compilation.show == nullptr) {
        result.error = StaticLookPreviewError::CompilationFailed;
        return result;
    }
    const auto* compiled_look = compilation.show->look(look_index);
    if (compiled_look == nullptr) {
        result.error = StaticLookPreviewError::CompilationFailed;
        return result;
    }
    showcore::StaticLookPlayer player{showcore::LayerId::EventMoment};
    auto& engine = compilation.show->engine();
    const auto playback = player.trigger(*compiled_look, 0U, 0U, engine.layers());
    if (!playback) {
        result.error = StaticLookPreviewError::LookPlaybackFailed;
        return result;
    }
    engine.tick();
    result.frames = engine.frames();
    result.frame_sha256 = frame_digest(result.frames);

    for (std::size_t fixture_index = 0U;
         fixture_index < project.fixtures.size(); ++fixture_index) {
        const auto& fixture = project.fixtures[fixture_index];
        const auto* profile = find_fixture_profile(project, fixture.profile_id);
        if (profile == nullptr || fixture.universe == 0U ||
            fixture.universe > showcore::kV1UniverseCount) {
            continue;
        }
        if (emitter_on(*source, fixture.id) &&
            fixture_profile_supports_property(
                *profile, showcore::Property::Intensity)) {
            const auto* intensity = find_assignment(
                *source, fixture.id, showcore::Property::Intensity);
            if (intensity == nullptr ||
                intensity->value.mode != showcore::ValueMode::Set ||
                intensity->value.value <= 0.0F) {
                result.warnings.push_back(
                    fixture.name + " is closed by its master intensity. "
                    "Set Intensity above zero to see the authored color.");
            }
        }
        for (const auto& channel : profile->channels) {
            StaticLookPreviewChannel trace;
            trace.fixture_id = fixture.id;
            trace.fixture_name = fixture.name;
            trace.profile_id = profile->id;
            trace.profile_mode = profile->mode;
            trace.source_revision = profile->source_revision;
            trace.universe = fixture.universe;
            trace.address = fixture.address;
            trace.channel = static_cast<std::uint16_t>(
                fixture.address + channel.coarse_offset);
            trace.property = channel.property;
            trace.encoding = channel.encoding;
            const auto* assignment = channel.property < showcore::Property::Count
                ? find_assignment(*source, fixture.id, channel.property)
                : nullptr;
            if (assignment != nullptr) {
                trace.authored_value = assignment->value;
                if (assignment->value.mode != showcore::ValueMode::Release) {
                    trace.winning_layer = showcore::LayerId::EventMoment;
                }
            }
            const auto slot = static_cast<std::size_t>(trace.channel - 1U);
            if (slot < showcore::kUniverseSlots) {
                trace.rendered_byte =
                    result.frames.universes[fixture.universe - 1U][slot];
            }
            result.channels.push_back(std::move(trace));
        }
    }
    return result;
}

StaticLookPreview preview_static_look_draft(
    const ProjectDocument& project,
    const StaticLookDraft& draft) {
    auto candidate = project;
    const auto existing = std::find_if(
        candidate.looks.begin(), candidate.looks.end(),
        [&](const auto& look) { return look.id == draft.look.id; });
    if (existing == candidate.looks.end()) {
        candidate.looks.push_back(draft.look);
    } else {
        *existing = draft.look;
    }
    return preview_static_look(candidate, draft.look.id);
}

std::string format_static_look_preview(const StaticLookPreview& preview) {
    std::ostringstream output;
    if (!preview) {
        output << "Offline preview failed (error "
               << static_cast<unsigned int>(preview.error) << ").\r\n";
        for (const auto& issue : preview.validation.issues) {
            output << (issue.severity == ProjectIssueSeverity::Error
                    ? "ERROR"
                    : "WARNING")
                   << " [" << issue.code << "] " << issue.subject << ": "
                   << issue.message << "\r\n";
        }
        return output.str();
    }
    output << "OFFLINE / NO ADAPTER OUTPUT\r\n"
           << "Frame SHA-256: " << preview.frame_sha256 << "\r\n";
    for (const auto& warning : preview.warnings) {
        output << "WARNING: " << warning << "\r\n";
    }
    output << "\r\nFixture | Profile mode | U/CH | Property | Ownership | Layer | DMX\r\n";
    for (const auto& channel : preview.channels) {
        output << channel.fixture_name << " | " << channel.profile_mode << " | U"
               << static_cast<unsigned int>(channel.universe) << "/"
               << channel.channel << " | " << property_name(channel.property) << " | "
               << mode_text(channel.authored_value.mode);
        if (channel.authored_value.mode == showcore::ValueMode::Set) {
            output << '(' << std::fixed << std::setprecision(3)
                   << channel.authored_value.value << ')';
        }
        output << " | " << layer_text(channel.winning_layer) << " | "
               << static_cast<unsigned int>(channel.rendered_byte) << "\r\n";
    }
    return output.str();
}

}  // namespace emberlights
