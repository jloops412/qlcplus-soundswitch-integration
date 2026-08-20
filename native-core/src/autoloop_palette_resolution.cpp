#include "emberlights/autoloop_palette_resolution.hpp"

#include "emberlights/studio_color.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace emberlights {
namespace {

[[nodiscard]] showcore::CompiledAutoloopTargetKind compiled_target_kind(
    AutoloopTargetKind kind) noexcept {
    switch (kind) {
    case AutoloopTargetKind::Master:
        return showcore::CompiledAutoloopTargetKind::Master;
    case AutoloopTargetKind::Group:
        return showcore::CompiledAutoloopTargetKind::Group;
    case AutoloopTargetKind::Fixture:
        return showcore::CompiledAutoloopTargetKind::Fixture;
    case AutoloopTargetKind::RoleSelector:
        return showcore::CompiledAutoloopTargetKind::RoleSelector;
    case AutoloopTargetKind::Count:
        break;
    }
    return showcore::CompiledAutoloopTargetKind::Count;
}

struct TargetKey {
    showcore::CompiledAutoloopTargetKind kind{
        showcore::CompiledAutoloopTargetKind::Master};
    std::string stable_ref;

    [[nodiscard]] friend bool operator<(
        const TargetKey& first,
        const TargetKey& second) noexcept {
        return std::tie(first.kind, first.stable_ref) <
            std::tie(second.kind, second.stable_ref);
    }
};

struct ReferenceKey {
    std::string reference_id;
    showcore::CompiledAutoloopTargetKind target_kind{
        showcore::CompiledAutoloopTargetKind::Master};
    std::string target_stable_ref;

    [[nodiscard]] friend bool operator<(
        const ReferenceKey& first,
        const ReferenceKey& second) noexcept {
        return std::tie(
                   first.reference_id,
                   first.target_kind,
                   first.target_stable_ref) <
            std::tie(
                   second.reference_id,
                   second.target_kind,
                   second.target_stable_ref);
    }
};

[[nodiscard]] const FixtureProfileDefinition* find_profile(
    const ProjectDocument& project,
    std::string_view profile_id) noexcept {
    const auto profile = std::find_if(
        project.fixture_profiles.begin(), project.fixture_profiles.end(),
        [&](const auto& candidate) { return candidate.id == profile_id; });
    return profile == project.fixture_profiles.end() ? nullptr : &*profile;
}

[[nodiscard]] std::uint64_t profile_property_mask(
    const FixtureProfileDefinition* profile) noexcept {
    std::uint64_t mask = 0U;
    if (profile == nullptr) {
        return mask;
    }
    for (const auto& channel : profile->channels) {
        if (channel.encoding != showcore::ChannelEncoding::Constant8 &&
            channel.property < showcore::Property::Count) {
            mask |= showcore::autoloop_property_mask(channel.property);
        }
    }
    return mask;
}

[[nodiscard]] std::vector<std::uint16_t> resolve_target_fixtures(
    const ProjectDocument& project,
    showcore::CompiledAutoloopTargetKind kind,
    std::string_view stable_ref) {
    std::vector<std::uint16_t> fixtures;
    fixtures.reserve(std::min(
        project.fixtures.size(),
        static_cast<std::size_t>(showcore::kMaxFixtures)));
    const auto add_fixture = [&](std::size_t index) {
        if (index < showcore::kMaxFixtures &&
            index <= std::numeric_limits<std::uint16_t>::max()) {
            fixtures.push_back(static_cast<std::uint16_t>(index));
        }
    };

    switch (kind) {
    case showcore::CompiledAutoloopTargetKind::Master:
        for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
            add_fixture(index);
        }
        break;
    case showcore::CompiledAutoloopTargetKind::Group: {
        const auto group = std::find_if(
            project.groups.begin(), project.groups.end(),
            [&](const auto& candidate) { return candidate.id == stable_ref; });
        if (group == project.groups.end()) {
            break;
        }
        std::unordered_set<std::string_view> members;
        members.reserve(group->fixture_ids.size());
        for (const auto& fixture_id : group->fixture_ids) {
            members.insert(fixture_id);
        }
        for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
            if (members.contains(project.fixtures[index].id)) {
                add_fixture(index);
            }
        }
        break;
    }
    case showcore::CompiledAutoloopTargetKind::Fixture:
        for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
            if (project.fixtures[index].id == stable_ref) {
                add_fixture(index);
                break;
            }
        }
        break;
    case showcore::CompiledAutoloopTargetKind::RoleSelector:
        for (std::size_t index = 0U; index < project.fixtures.size(); ++index) {
            const auto& roles = project.fixtures[index].roles;
            if (std::find(roles.begin(), roles.end(), stable_ref) !=
                roles.end()) {
                add_fixture(index);
            }
        }
        break;
    case showcore::CompiledAutoloopTargetKind::Count:
        break;
    }
    return fixtures;
}

[[nodiscard]] bool same_target(
    showcore::CompiledAutoloopTargetKind kind,
    std::string_view stable_ref,
    showcore::CompiledAutoloopTargetKind candidate_kind,
    std::string_view candidate_ref) noexcept {
    return kind == candidate_kind && stable_ref == candidate_ref;
}

}  // namespace

struct AutoloopPaletteCompileEnvironment::Impl {
    struct TargetStorage {
        showcore::CompiledAutoloopTargetKind kind{
            showcore::CompiledAutoloopTargetKind::Master};
        std::string stable_ref;
        std::vector<std::uint16_t> fixture_ids;
        std::uint64_t supported_property_mask{0U};
    };

    struct ReferenceStorage {
        std::string stable_id;
        showcore::CompiledAutoloopTargetKind target_kind{
            showcore::CompiledAutoloopTargetKind::Master};
        std::string target_stable_ref;
        std::vector<showcore::LookAssignment> assignments;
    };

    const ProjectDocument& project;
    const AutoloopSourceDocument& source;
    showcore::AutoloopCompileLimits limits;
    std::vector<TargetStorage> targets;
    std::vector<showcore::AutoloopTargetBinding> target_bindings;
    std::vector<ReferenceStorage> references;
    std::vector<showcore::AutoloopReferenceBinding> reference_bindings;
    std::vector<AutoloopPaletteResolution> resolutions;
    std::vector<showcore::AutoloopCompileDiagnostic> diagnostics;
    std::size_t reference_assignment_count{0U};
    bool has_degraded{false};

    Impl(
        const ProjectDocument& project_value,
        const AutoloopSourceDocument& source_value,
        showcore::AutoloopCompileLimits limits_value)
        : project(project_value), source(source_value), limits(limits_value) {
        build();
    }

    void add_diagnostic(
        showcore::AutoloopCompileError error,
        showcore::AutoloopArenaKind arena,
        std::string code,
        std::string subject,
        std::string message) {
        diagnostics.push_back({
            error,
            arena,
            std::move(code),
            std::move(subject),
            std::move(message)});
    }

    [[nodiscard]] const TargetStorage* find_target(
        showcore::CompiledAutoloopTargetKind kind,
        std::string_view stable_ref) const noexcept {
        const auto found = std::find_if(
            targets.begin(), targets.end(), [&](const auto& target) {
                return same_target(
                    kind, stable_ref, target.kind, target.stable_ref);
            });
        return found == targets.end() ? nullptr : &*found;
    }

    void build_targets(const std::set<TargetKey>& target_keys) {
        targets.reserve(target_keys.size());
        std::size_t target_fixture_count = 0U;
        for (const auto& key : target_keys) {
            auto fixtures = resolve_target_fixtures(
                project, key.kind, key.stable_ref);
            const auto remaining = limits.maximum_target_fixture_ids -
                std::min(
                    target_fixture_count,
                    limits.maximum_target_fixture_ids);
            if (fixtures.size() > remaining) {
                add_diagnostic(
                    showcore::AutoloopCompileError::CapacityExceeded,
                    showcore::AutoloopArenaKind::TargetFixtureIds,
                    "autoloop.palette.capacity.targetFixtures",
                    key.stable_ref,
                    "Palette target resolution exceeds the bounded target-fixture arena.");
                return;
            }
            target_fixture_count += fixtures.size();
            auto supported = showcore::all_autoloop_property_mask();
            for (const auto fixture_id : fixtures) {
                const auto& fixture = project.fixtures[fixture_id];
                supported &= profile_property_mask(
                    find_profile(project, fixture.profile_id));
            }
            targets.push_back({
                key.kind,
                key.stable_ref,
                std::move(fixtures),
                supported});
        }

        target_bindings.reserve(targets.size());
        for (const auto& target : targets) {
            if (target.fixture_ids.empty()) {
                continue;
            }
            target_bindings.push_back({
                target.kind,
                target.stable_ref,
                std::span<const std::uint16_t>(target.fixture_ids),
                target.supported_property_mask});
        }
    }

    [[nodiscard]] std::vector<std::pair<
        std::string, const StudioColorSwatch*>> matching_swatches(
        std::string_view reference_id) const {
        std::vector<std::pair<std::string, const StudioColorSwatch*>> matches;
        for (const auto& palette : project.color_palettes) {
            for (const auto& swatch : palette.swatches) {
                if (swatch.id == reference_id) {
                    matches.emplace_back(palette.id, &swatch);
                }
            }
        }
        std::sort(matches.begin(), matches.end(), [](const auto& first, const auto& second) {
            return first.first < second.first;
        });
        return matches;
    }

    void resolve_reference(
        const ReferenceKey& key,
        std::string_view subject) {
        AutoloopPaletteResolution report;
        report.reference_id = key.reference_id;
        report.target_kind = key.target_kind;
        report.target_stable_ref = key.target_stable_ref;

        const auto* target = find_target(
            key.target_kind, key.target_stable_ref);
        if (target == nullptr || target->fixture_ids.empty()) {
            report.status = AutoloopPaletteResolutionStatus::Missing;
            report.warnings.push_back("autoloop.palette.target.missing");
            resolutions.push_back(std::move(report));
            add_diagnostic(
                showcore::AutoloopCompileError::MissingTarget,
                showcore::AutoloopArenaKind::TargetSpans,
                "autoloop.palette.target.missing",
                std::string(subject),
                "The Palette event target does not resolve to any project fixture.");
            return;
        }
        report.fixture_count = target->fixture_ids.size();

        const auto matches = matching_swatches(key.reference_id);
        report.palette_ids.reserve(matches.size());
        for (const auto& match : matches) {
            report.palette_ids.push_back(match.first);
        }
        if (matches.empty()) {
            report.status = AutoloopPaletteResolutionStatus::Missing;
            report.warnings.push_back("autoloop.palette.reference.missing");
            resolutions.push_back(std::move(report));
            add_diagnostic(
                showcore::AutoloopCompileError::MissingReference,
                showcore::AutoloopArenaKind::References,
                "autoloop.palette.reference.missing",
                std::string(subject),
                "No project palette swatch has this exact stable ID.");
            return;
        }
        if (matches.size() != 1U) {
            report.status = AutoloopPaletteResolutionStatus::Ambiguous;
            report.warnings.push_back("autoloop.palette.reference.ambiguous");
            resolutions.push_back(std::move(report));
            add_diagnostic(
                showcore::AutoloopCompileError::AmbiguousReference,
                showcore::AutoloopArenaKind::References,
                "autoloop.palette.reference.ambiguous",
                std::string(subject),
                "Multiple project palette swatches share this stable ID.");
            return;
        }

        std::vector<showcore::LookAssignment> assignments;
        assignments.reserve(
            target->fixture_ids.size() * showcore::kPropertyCount);
        const auto& color = matches.front().second->color;
        for (const auto fixture_id : target->fixture_ids) {
            const auto& fixture = project.fixtures[fixture_id];
            const auto* profile = find_profile(project, fixture.profile_id);
            if (profile == nullptr) {
                ++report.unsupported_fixture_count;
                report.warnings.push_back(
                    "autoloop.palette.fixture_profile.missing:" + fixture.id);
                continue;
            }
            auto realization = realize_studio_color(*profile, color);
            for (auto& warning : realization.warnings) {
                report.warnings.push_back(
                    "autoloop.palette.fixture:" + fixture.id + ':' + warning);
            }
            if (!realization.usable()) {
                ++report.unsupported_fixture_count;
                continue;
            }

            bool target_compatible = true;
            for (const auto& assignment : realization.assignments) {
                if ((target->supported_property_mask &
                     showcore::autoloop_property_mask(assignment.property)) ==
                    0U) {
                    target_compatible = false;
                    report.warnings.push_back(
                        "autoloop.palette.target_capability.nonuniform:" +
                        fixture.id + ':' +
                        std::string(property_name(
                            assignment.property)));
                }
            }
            if (!target_compatible || realization.assignments.empty()) {
                ++report.unsupported_fixture_count;
                continue;
            }
            if (realization.status == StudioColorRealizationStatus::Degraded) {
                ++report.degraded_fixture_count;
            } else {
                ++report.exact_fixture_count;
            }
            for (const auto& assignment : realization.assignments) {
                assignments.push_back({
                    fixture_id,
                    assignment.property,
                    assignment.value});
            }
        }

        if (report.unsupported_fixture_count != 0U || assignments.empty()) {
            report.status = AutoloopPaletteResolutionStatus::Unsupported;
            resolutions.push_back(std::move(report));
            add_diagnostic(
                showcore::AutoloopCompileError::MissingCapability,
                showcore::AutoloopArenaKind::ReferenceAssignments,
                "autoloop.palette.capability.unsupported",
                std::string(subject),
                "Every fixture in the Palette target must support its semantic "
                "realization without partial application.");
            return;
        }

        std::sort(
            assignments.begin(), assignments.end(),
            [](const auto& first, const auto& second) {
                return std::tie(first.fixture_id, first.property) <
                    std::tie(second.fixture_id, second.property);
            });
        report.status = report.degraded_fixture_count == 0U
            ? AutoloopPaletteResolutionStatus::Exact
            : AutoloopPaletteResolutionStatus::Degraded;
        const auto remaining = limits.maximum_reference_assignments -
            std::min(
                reference_assignment_count,
                limits.maximum_reference_assignments);
        if (assignments.size() > remaining) {
            report.status = AutoloopPaletteResolutionStatus::Unsupported;
            report.warnings.push_back(
                "autoloop.palette.capacity.assignments");
            resolutions.push_back(std::move(report));
            add_diagnostic(
                showcore::AutoloopCompileError::CapacityExceeded,
                showcore::AutoloopArenaKind::ReferenceAssignments,
                "autoloop.palette.capacity.assignments",
                std::string(subject),
                "Palette realization exceeds the bounded assignment arena.");
            return;
        }
        reference_assignment_count += assignments.size();
        has_degraded = has_degraded ||
            report.status == AutoloopPaletteResolutionStatus::Degraded;
        references.push_back({
            key.reference_id,
            key.target_kind,
            key.target_stable_ref,
            std::move(assignments)});
        resolutions.push_back(std::move(report));
    }

    void build() {
        std::set<TargetKey> target_keys;
        std::map<ReferenceKey, std::string> palette_requests;
        bool target_capacity_exceeded = false;
        bool reference_capacity_exceeded = false;

        for (const auto& program : source.programs) {
            std::unordered_map<std::string_view,
                const AutoloopTargetDefinition*> targets_by_id;
            targets_by_id.reserve(program.targets.size());
            for (const auto& target : program.targets) {
                targets_by_id.emplace(target.id, &target);
                const auto kind = compiled_target_kind(target.kind);
                if (kind == showcore::CompiledAutoloopTargetKind::Count) {
                    continue;
                }
                target_keys.insert({kind, target.stable_ref});
                if (target_keys.size() > limits.maximum_target_spans) {
                    target_capacity_exceeded = true;
                    break;
                }
            }
            if (target_capacity_exceeded) {
                break;
            }

            std::unordered_map<std::string_view,
                const AutoloopLaneDefinition*> lanes_by_id;
            lanes_by_id.reserve(program.lanes.size());
            for (const auto& lane : program.lanes) {
                lanes_by_id.emplace(lane.id, &lane);
            }
            for (const auto& event : program.events) {
                if (event.kind != AutoloopEventKind::Palette) {
                    continue;
                }
                const auto lane = lanes_by_id.find(event.lane_id);
                if (lane == lanes_by_id.end()) {
                    continue;
                }
                const auto target = targets_by_id.find(
                    lane->second->target_id);
                if (target == targets_by_id.end()) {
                    continue;
                }
                const auto kind = compiled_target_kind(target->second->kind);
                if (kind == showcore::CompiledAutoloopTargetKind::Count) {
                    continue;
                }
                ReferenceKey key{
                    event.reference_id,
                    kind,
                    target->second->stable_ref};
                const auto [existing, inserted] = palette_requests.emplace(
                    std::move(key), event.id);
                if (!inserted && event.id < existing->second) {
                    existing->second = event.id;
                }
                if (palette_requests.size() > limits.maximum_references) {
                    reference_capacity_exceeded = true;
                    break;
                }
            }
            if (reference_capacity_exceeded) {
                break;
            }
        }

        if (target_capacity_exceeded) {
            add_diagnostic(
                showcore::AutoloopCompileError::CapacityExceeded,
                showcore::AutoloopArenaKind::TargetSpans,
                "autoloop.palette.capacity.targets",
                "source",
                "Palette target resolution exceeds the bounded target arena.");
            return;
        }
        if (reference_capacity_exceeded) {
            add_diagnostic(
                showcore::AutoloopCompileError::CapacityExceeded,
                showcore::AutoloopArenaKind::References,
                "autoloop.palette.capacity.references",
                "source",
                "Palette resolution exceeds the bounded reference arena.");
            return;
        }

        build_targets(target_keys);
        if (!diagnostics.empty()) {
            return;
        }
        references.reserve(palette_requests.size());
        resolutions.reserve(palette_requests.size());
        for (const auto& [key, subject] : palette_requests) {
            resolve_reference(key, subject);
        }

        reference_bindings.reserve(references.size());
        for (const auto& reference : references) {
            reference_bindings.push_back({
                showcore::CompiledAutoloopReferenceKind::Palette,
                reference.stable_id,
                reference.target_kind,
                reference.target_stable_ref,
                std::span<const showcore::LookAssignment>(
                    reference.assignments),
                showcore::CompiledAutoloopGeneratorKind::None,
                1U});
        }
    }
};

AutoloopPaletteCompileEnvironment::AutoloopPaletteCompileEnvironment(
    const ProjectDocument& project,
    const AutoloopSourceDocument& source,
    showcore::AutoloopCompileLimits limits)
    : impl_(std::make_unique<Impl>(project, source, limits)) {}

AutoloopPaletteCompileEnvironment::~AutoloopPaletteCompileEnvironment() =
    default;

bool AutoloopPaletteCompileEnvironment::ok() const noexcept {
    return impl_->diagnostics.empty();
}

bool AutoloopPaletteCompileEnvironment::degraded() const noexcept {
    return impl_->has_degraded;
}

showcore::AutoloopCompileEnvironment
AutoloopPaletteCompileEnvironment::environment() const noexcept {
    return {
        std::span<const showcore::AutoloopTargetBinding>(
            impl_->target_bindings),
        std::span<const showcore::AutoloopReferenceBinding>(
            impl_->reference_bindings)};
}

std::span<const AutoloopPaletteResolution>
AutoloopPaletteCompileEnvironment::resolutions() const noexcept {
    return impl_->resolutions;
}

std::span<const showcore::AutoloopCompileDiagnostic>
AutoloopPaletteCompileEnvironment::diagnostics() const noexcept {
    return impl_->diagnostics;
}

const char* autoloop_palette_resolution_status_name(
    AutoloopPaletteResolutionStatus status) noexcept {
    switch (status) {
    case AutoloopPaletteResolutionStatus::Exact: return "exact";
    case AutoloopPaletteResolutionStatus::Degraded: return "degraded";
    case AutoloopPaletteResolutionStatus::Unsupported: return "unsupported";
    case AutoloopPaletteResolutionStatus::Missing: return "missing";
    case AutoloopPaletteResolutionStatus::Ambiguous: return "ambiguous";
    }
    return "unsupported";
}

}  // namespace emberlights
