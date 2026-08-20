#include "emberlights/compiler.hpp"

#include "emberlights/autoloop_persistence.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace emberlights {
namespace {

template <std::size_t Size>
void copy_name(std::array<char, Size>& destination, std::string_view source) noexcept {
    destination.fill('\0');
    const auto length = std::min(source.size(), destination.size() - 1U);
    std::copy_n(source.begin(), length, destination.begin());
}

void compilation_error(
    ProjectValidation& validation,
    std::string code,
    std::string subject,
    std::string message) {
    validation.issues.push_back({
        ProjectIssueSeverity::Error,
        std::move(code),
        std::move(subject),
        std::move(message)});
}

[[nodiscard]] std::size_t autoloop_index(showcore::AutoloopAddress address) noexcept {
    return static_cast<std::size_t>(address.bank) * showcore::kAutoloopsPerBank + address.slot;
}

[[nodiscard]] std::uint64_t fixture_property_mask(
    const showcore::Patch& patch,
    std::uint16_t fixture_id) noexcept {
    for (std::size_t index = 0U; index < patch.size(); ++index) {
        const auto& fixture = patch.at(index);
        if (fixture.id != fixture_id || fixture.profile == nullptr) {
            continue;
        }
        std::uint64_t mask = 0U;
        for (std::size_t channel = 0U;
             channel < fixture.profile->channel_count; ++channel) {
            const auto property = fixture.profile->channels[channel].property;
            if (property != showcore::Property::Count) {
                mask |= showcore::autoloop_property_mask(property);
            }
        }
        return mask;
    }
    return 0U;
}

[[nodiscard]] std::uint64_t intersection_property_mask(
    const showcore::Patch& patch,
    std::span<const std::uint16_t> fixture_ids) noexcept {
    if (fixture_ids.empty()) {
        return 0U;
    }
    auto mask = showcore::all_autoloop_property_mask();
    for (const auto fixture_id : fixture_ids) {
        mask &= fixture_property_mask(patch, fixture_id);
    }
    return mask;
}

}  // namespace

const showcore::StaticLook* CompiledShow::look(std::size_t index) const noexcept {
    return index < look_count_ ? &looks_[index] : nullptr;
}

std::uint32_t CompiledShow::look_fade_ms(std::size_t index) const noexcept {
    return index < look_count_ ? look_fades_[index] : 0U;
}

showcore::AutoloopRepeat CompiledShow::autoloop_repeat(
    showcore::AutoloopAddress address) const noexcept {
    return address.valid() ? repeats_[autoloop_index(address)] : showcore::AutoloopRepeat::Once;
}

const CompiledTrackScript* CompiledShow::track_script(std::size_t index) const noexcept {
    return index < track_script_count_ ? &track_scripts_[index] : nullptr;
}

const showcore::FixtureGroup* CompiledShow::group(std::size_t index) const noexcept {
    return index < group_count_ ? &groups_[index] : nullptr;
}

CompilationResult compile_project(const ProjectDocument& project) {
    CompilationResult result;
    result.validation = validate_project(project);
    if (!result.validation.ok()) {
        return result;
    }

    auto compiled = std::make_unique<CompiledShow>();
    std::unordered_map<std::string_view, const showcore::FixtureProfile*> profile_by_id;
    for (const auto& profile : project.fixture_profiles) {
        std::vector<std::vector<showcore::ChannelCapabilityMapping>>
            capability_storage(profile.channels.size());
        std::vector<showcore::ChannelMapping> mappings;
        mappings.reserve(profile.channels.size());
        for (std::size_t channel_index = 0U;
             channel_index < profile.channels.size();
             ++channel_index) {
            const auto& channel = profile.channels[channel_index];
            auto& capabilities = capability_storage[channel_index];
            capabilities.reserve(channel.capabilities.size());
            for (const auto& capability : channel.capabilities) {
                capabilities.push_back({
                    capability.property,
                    capability.dmx_min,
                    capability.dmx_max,
                    capability.preferred_value,
                    capability.behavior,
                    capability.access,
                    capability.reversed});
            }
            mappings.push_back({
                channel.property,
                channel.coarse_offset,
                channel.fine_offset,
                channel.encoding,
                channel.dmx_min,
                channel.dmx_max,
                channel.default_value,
                channel.blackout_value,
                channel.highlight_value,
                capabilities.empty() ? nullptr : capabilities.data(),
                capabilities.size()});
        }
        const showcore::FixtureProfileDraft draft{
            profile.id,
            profile.manufacturer,
            profile.model,
            profile.mode,
            profile.name,
            profile.source,
            profile.source_revision,
            mappings.data(),
            mappings.size(),
            profile.footprint};
        const auto ingest = compiled->fixture_library_.ingest(draft);
        if (!ingest) {
            compilation_error(result.validation, "compile.profile", profile.id,
                              "Fixture profile could not be copied into the immutable show.");
            return result;
        }
        const auto* stored = compiled->fixture_library_.at(ingest.profile_index);
        profile_by_id.emplace(profile.id, &stored->runtime);
    }

    std::unordered_map<std::string_view, std::uint16_t> fixture_by_id;
    for (std::size_t index = 0; index < project.fixtures.size(); ++index) {
        const auto& fixture = project.fixtures[index];
        const auto profile = profile_by_id.find(fixture.profile_id);
        if (profile == profile_by_id.end()) {
            compilation_error(result.validation, "compile.fixtureProfile", fixture.id,
                              "Fixture profile disappeared during compilation.");
            return result;
        }
        const auto runtime_id = static_cast<std::uint16_t>(index);
        const auto patch = compiled->engine_.patch().add({
            runtime_id,
            static_cast<std::uint8_t>(fixture.universe - 1U),
            fixture.address,
            profile->second});
        if (!patch) {
            compilation_error(result.validation, "compile.patch", fixture.id,
                              "Fixture failed immutable patch validation.");
            return result;
        }
        fixture_by_id.emplace(fixture.id, runtime_id);
    }

    std::unordered_map<std::string_view, std::size_t> group_by_id;
    for (std::size_t group_index = 0U; group_index < project.groups.size(); ++group_index) {
        const auto& source = project.groups[group_index];
        auto& target = compiled->groups_[group_index];
        std::array<bool, showcore::kMaxFixtures> included{};
        for (const auto& fixture_id : source.fixture_ids) {
            const auto fixture = fixture_by_id.find(fixture_id);
            if (fixture == fixture_by_id.end()) {
                compilation_error(result.validation, "compile.group", source.id,
                                  "Fixture group could not be compiled into the immutable show.");
                return result;
            }
            if (included[fixture->second]) {
                continue;
            }
            included[fixture->second] = true;
            if (!target.add(fixture->second)) {
                compilation_error(result.validation, "compile.group", source.id,
                                  "Fixture group exceeded the immutable membership capacity.");
                return result;
            }
        }
        compiled->group_count_ = group_index + 1U;
        group_by_id.emplace(source.id, group_index);
    }

    std::unordered_map<std::string_view, std::size_t> look_by_id;
    for (std::size_t look_index = 0; look_index < project.looks.size(); ++look_index) {
        const auto& source = project.looks[look_index];
        auto& target = compiled->looks_[look_index];
        copy_name(compiled->look_names_[look_index], source.name);
        const auto first_assignment = compiled->look_assignment_count_;
        for (const auto& assignment : source.assignments) {
            const auto fixture = fixture_by_id.find(assignment.fixture_id);
            if (fixture == fixture_by_id.end()) {
                compilation_error(result.validation, "compile.lookFixture", source.id,
                                  "Static Look fixture disappeared during compilation.");
                return result;
            }
            compiled->look_assignments_[compiled->look_assignment_count_++] = {
                fixture->second,
                assignment.property,
                assignment.value};
        }
        target = {
            compiled->look_names_[look_index].data(),
            compiled->look_assignments_.data() + first_assignment,
            source.assignments.size()};
        const auto validation = showcore::validate_static_look(target);
        if (!validation) {
            compilation_error(result.validation, "compile.look", source.id,
                              "Static Look failed immutable validation.");
            return result;
        }
        compiled->look_fades_[look_index] = source.fade_ms;
        compiled->look_count_ = look_index + 1U;
        look_by_id.emplace(source.id, look_index);
    }

    std::unordered_map<std::string_view, showcore::AutoloopAddress> autoloop_by_id;
    for (const auto& source : project.autoloops) {
        const showcore::AutoloopAddress address{source.bank, source.slot};
        const auto pattern_index = autoloop_index(address);
        auto& target = compiled->patterns_[pattern_index];
        copy_name(compiled->autoloop_names_[pattern_index], source.name);
        target.name = compiled->autoloop_names_[pattern_index].data();
        target.length_beats = source.length_beats;
        for (const auto& step : source.steps) {
            const auto look = look_by_id.find(step.look_id);
            if (look == look_by_id.end() ||
                !target.add_step({step.at_beat, &compiled->looks_[look->second], step.transition})) {
                compilation_error(result.validation, "compile.autoloopStep", source.id,
                                  "Autoloop step could not be compiled.");
                return result;
            }
        }
        if (!showcore::validate_autoloop_pattern(target) ||
            !compiled->catalog_.set(address, &target)) {
            compilation_error(result.validation, "compile.autoloop", source.id,
                              "Autoloop failed immutable validation.");
            return result;
        }
        compiled->repeats_[pattern_index] = source.repeat;
        autoloop_by_id.emplace(source.id, address);
    }

    std::unordered_map<std::string_view, std::size_t> track_by_id;
    for (std::size_t track_index = 0; track_index < project.track_scripts.size(); ++track_index) {
        const auto& source = project.track_scripts[track_index];
        const auto first_cue = compiled->track_cue_count_;
        for (const auto& cue : source.cues) {
            auto& target = compiled->track_cues_[compiled->track_cue_count_++];
            target.at_beat = cue.at_beat;
            target.action = cue.action;
            switch (cue.action) {
            case TrackCueAction::TriggerLook: {
                const auto look = look_by_id.find(cue.target_ref);
                if (look == look_by_id.end()) {
                    compilation_error(result.validation, "compile.trackLook", source.id,
                                      "Track cue references a missing Static Look.");
                    return result;
                }
                target.target = static_cast<std::uint16_t>(look->second);
                break;
            }
            case TrackCueAction::TriggerAutoloop: {
                const auto autoloop = autoloop_by_id.find(cue.target_ref);
                if (autoloop == autoloop_by_id.end()) {
                    compilation_error(result.validation, "compile.trackAutoloop", source.id,
                                      "Track cue references a missing Autoloop.");
                    return result;
                }
                target.target = static_cast<std::uint16_t>(autoloop_index(autoloop->second));
                break;
            }
            case TrackCueAction::ClearLook:
            case TrackCueAction::ClearAutoloop:
            case TrackCueAction::Count:
                break;
            }
        }
        compiled->track_scripts_[track_index] = {
            compiled->track_cues_.data() + first_cue,
            source.cues.size()};
        compiled->track_script_count_ = track_index + 1U;
        track_by_id.emplace(source.id, track_index);
    }

    for (const auto& source : project.midi_mappings) {
        auto action = source.action;
        if (!source.target_ref.empty()) {
            if (action.type == showcore::ActionType::TriggerLook) {
                const auto target = look_by_id.find(source.target_ref);
                if (target == look_by_id.end()) {
                    compilation_error(result.validation, "compile.midiLook", source.target_ref,
                                      "MIDI mapping references a missing Static Look.");
                    return result;
                }
                action.target_id = static_cast<std::uint16_t>(target->second);
            } else if (action.type == showcore::ActionType::TriggerAutoloop) {
                const auto target = autoloop_by_id.find(source.target_ref);
                if (target == autoloop_by_id.end()) {
                    compilation_error(result.validation, "compile.midiAutoloop", source.target_ref,
                                      "MIDI mapping references a missing Autoloop.");
                    return result;
                }
                action.target_id = static_cast<std::uint16_t>(autoloop_index(target->second));
            } else if (action.type == showcore::ActionType::SetProperty) {
                const auto target = fixture_by_id.find(source.target_ref);
                if (target == fixture_by_id.end()) {
                    compilation_error(result.validation, "compile.midiFixture", source.target_ref,
                                      "MIDI mapping references a missing fixture.");
                    return result;
                }
                action.target_id = target->second;
            } else if (action.type == showcore::ActionType::SetGroupProperty ||
                       action.type == showcore::ActionType::BlackoutGroup) {
                const auto target = group_by_id.find(source.target_ref);
                if (target == group_by_id.end()) {
                    compilation_error(result.validation, "compile.midiGroup", source.target_ref,
                                      "MIDI mapping references a missing fixture group.");
                    return result;
                }
                action.target_id = static_cast<std::uint16_t>(target->second);
            } else if (action.type == showcore::ActionType::TriggerTrackScript) {
                const auto target = track_by_id.find(source.target_ref);
                if (target == track_by_id.end()) {
                    compilation_error(result.validation, "compile.midiTrack", source.target_ref,
                                      "MIDI mapping references a missing track script.");
                    return result;
                }
                action.target_id = static_cast<std::uint16_t>(target->second);
            }
        }
        showcore::MidiMapping mapping;
        mapping.device_id = showcore::kAnyMidiDevice;
        mapping.message_type = source.message_type;
        mapping.channel = source.channel;
        mapping.number = source.number;
        mapping.input_mode = source.input_mode;
        mapping.behavior = source.behavior;
        mapping.action = action;
        mapping.output_min = source.output_min;
        mapping.output_max = source.output_max;
        mapping.curve = source.curve;
        mapping.inverted = source.inverted;
        mapping.soft_takeover = source.soft_takeover;
        mapping.takeover_tolerance = source.takeover_tolerance;
        if (!compiled->midi_mappings_.add(mapping)) {
            compilation_error(result.validation, "compile.midi", source.device_name,
                              "MIDI mapping capacity was exceeded during compilation.");
            return result;
        }
    }

    result.show = std::move(compiled);
    return result;
}

CompilationResult compile_project(
    const ProjectDocument& project,
    const AutoloopSourceDocument& source,
    const showcore::AutoloopCompileLimits& limits) {
    // Keep the format-1 compiler as the single patch/look/catalog authority.
    // This overload only adds a compiled V2 package after that immutable show
    // has completed successfully, entirely off the scheduler thread.
    auto result = compile_project(project);
    if (!result) {
        return result;
    }

    auto& show = *result.show;
    const auto& patch = show.engine().patch();
    std::vector<std::uint16_t> master_fixture_ids;
    master_fixture_ids.reserve(patch.size());
    for (std::size_t index = 0U; index < patch.size(); ++index) {
        master_fixture_ids.push_back(patch.at(index).id);
    }

    std::unordered_map<std::string_view, std::vector<std::uint16_t>>
        role_fixture_ids;
    for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
        const auto runtime_id = master_fixture_ids[index];
        for (const auto& role : project.fixtures[index].roles) {
            auto& fixtures = role_fixture_ids[role];
            if (std::find(fixtures.begin(), fixtures.end(), runtime_id) ==
                fixtures.end()) {
                fixtures.push_back(runtime_id);
            }
        }
    }

    std::vector<showcore::AutoloopTargetBinding> targets;
    targets.reserve(
        1U + project.fixtures.size() + project.groups.size() +
        role_fixture_ids.size());
    targets.push_back({
        showcore::CompiledAutoloopTargetKind::Master,
        {},
        master_fixture_ids,
        intersection_property_mask(patch, master_fixture_ids)});
    for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
        const std::span<const std::uint16_t> fixture(
            master_fixture_ids.data() + index, 1U);
        targets.push_back({
            showcore::CompiledAutoloopTargetKind::Fixture,
            project.fixtures[index].id,
            fixture,
            intersection_property_mask(patch, fixture)});
    }
    for (std::size_t index = 0U; index < project.groups.size(); ++index) {
        const auto* group = show.group(index);
        if (group == nullptr) {
            continue;
        }
        const std::span<const std::uint16_t> fixtures(
            group->fixture_ids.data(), group->count);
        targets.push_back({
            showcore::CompiledAutoloopTargetKind::Group,
            project.groups[index].id,
            fixtures,
            intersection_property_mask(patch, fixtures)});
    }
    for (const auto& [role, fixtures] : role_fixture_ids) {
        targets.push_back({
            showcore::CompiledAutoloopTargetKind::RoleSelector,
            role,
            fixtures,
            intersection_property_mask(patch, fixtures)});
    }

    // Format-1 Static Looks are the only semantic references currently owned
    // by ProjectDocument. Rich palette/movement/effect references fail closed
    // in the V2 compiler until their corresponding immutable assets exist.
    std::vector<showcore::AutoloopReferenceBinding> references;
    references.reserve(project.looks.size());
    for (std::size_t index = 0U; index < project.looks.size(); ++index) {
        const auto* look = show.look(index);
        if (look == nullptr) {
            continue;
        }
        references.push_back({
            showcore::CompiledAutoloopReferenceKind::LegacyLook,
            project.looks[index].id,
            showcore::CompiledAutoloopTargetKind::Master,
            {},
            std::span<const showcore::LookAssignment>(
                look->assignments, look->assignment_count),
            showcore::CompiledAutoloopGeneratorKind::None,
            1U});
    }

    auto compiled_v2 = showcore::compile_autoloop_programs(
        source, {targets, references}, limits);
    if (!compiled_v2) {
        if (compiled_v2.diagnostics.empty()) {
            compilation_error(
                result.validation,
                "autoloop.compile.internal",
                "autoloop-v2",
                "V2 Autoloop compilation failed without a diagnostic.");
        }
        for (auto& diagnostic : compiled_v2.diagnostics) {
            compilation_error(
                result.validation,
                std::move(diagnostic.code),
                std::move(diagnostic.subject),
                std::move(diagnostic.message));
        }
        result.show.reset();
        return result;
    }
    show.autoloop_v2_package_ = std::move(compiled_v2.package);
    return result;
}

CompilationResult compile_project_with_persisted_autoloops(
    const ProjectDocument& project,
    const showcore::AutoloopCompileLimits& limits) {
    const auto persisted = inspect_persisted_autoloop_source(project);
    if (!persisted) {
        CompilationResult result;
        result.validation = validate_project(project);
        compilation_error(
            result.validation,
            "autoloop.persistence.invalid",
            "autoloop-v2",
            persisted.message.empty()
                ? "The persisted V2 Autoloop source is invalid."
                : persisted.message);
        return result;
    }
    return persisted.stamp.present
        ? compile_project(project, persisted.source, limits)
        : compile_project(project);
}

}  // namespace emberlights
