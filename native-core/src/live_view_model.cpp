#include "emberlights/live_view_model.hpp"

#include "emberlights/fixture_capabilities.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <utility>

namespace emberlights {
namespace {

inline constexpr std::size_t kMissingIndex = std::numeric_limits<std::size_t>::max();

[[nodiscard]] std::size_t fixture_index(
    const ProjectDocument& project,
    std::string_view id) noexcept {
    const auto found = std::find_if(
        project.fixtures.begin(), project.fixtures.end(),
        [id](const auto& fixture) { return fixture.id == id; });
    return found == project.fixtures.end()
        ? kMissingIndex
        : static_cast<std::size_t>(found - project.fixtures.begin());
}

}  // namespace

std::uint16_t LiveOverrideTarget::support_count(
    showcore::Property property) const noexcept {
    if (property >= showcore::Property::Count) {
        return 0U;
    }
    return property_support[static_cast<std::size_t>(property)];
}

bool LiveOverrideTarget::supports_any(showcore::Property property) const noexcept {
    return support_count(property) != 0U;
}

bool LiveOverrideTarget::supports_all(showcore::Property property) const noexcept {
    return fixture_count != 0U && support_count(property) == fixture_count;
}

void LiveViewModel::load_project(const ProjectDocument& active_project) {
    project_id_ = active_project.id;
    project_name_ = active_project.name;
    safety_ = active_project.safety;
    state_ = {};
    active_content_ = {};

    static_looks_.clear();
    static_looks_.reserve(active_project.looks.size());
    for (std::size_t index = 0U; index < active_project.looks.size(); ++index) {
        const auto& look = active_project.looks[index];
        static_looks_.push_back({
            look.id,
            look.name,
            static_cast<std::uint16_t>(index),
            false,
            false,
            StaticLookBehavior::None,
            StaticLookActivationStatus::None,
            StaticLookOwnerKind::None,
            0U,
            0U,
            0U,
            0.0F});
    }

    autoloop_catalog_.clear();
    autoloop_catalog_.reserve(active_project.autoloops.size());
    for (const auto& loop : active_project.autoloops) {
        const showcore::AutoloopAddress address{loop.bank, loop.slot};
        if (address.valid()) {
            autoloop_catalog_.push_back({loop.id, loop.name, address, loop.repeat});
        }
    }

    track_scripts_.clear();
    track_scripts_.reserve(active_project.track_scripts.size());
    for (std::size_t index = 0U; index < active_project.track_scripts.size(); ++index) {
        const auto& script = active_project.track_scripts[index];
        track_scripts_.push_back({
            script.id,
            script.name,
            static_cast<std::uint16_t>(index),
            false,
            false});
    }

    std::vector<FixtureCapabilityView> capabilities;
    capabilities.reserve(active_project.fixtures.size());
    for (std::size_t index = 0U; index < active_project.fixtures.size(); ++index) {
        capabilities.push_back(inspect_fixture_capabilities(active_project, index));
    }

    override_targets_.clear();
    override_targets_.reserve(
        active_project.groups.size() + active_project.fixtures.size());
    for (const auto& group : active_project.groups) {
        LiveOverrideTarget target;
        target.kind = LiveOverrideTargetKind::Group;
        target.id = group.id;
        target.name = group.name;
        target.complete = !group.fixture_ids.empty();
        std::array<bool, showcore::kMaxFixtures> included{};
        for (const auto& fixture_id : group.fixture_ids) {
            const auto index = fixture_index(active_project, fixture_id);
            if (index == kMissingIndex || index >= included.size() ||
                index >= capabilities.size() || !capabilities[index].complete) {
                target.complete = false;
                continue;
            }
            if (included[index]) {
                continue;
            }
            included[index] = true;
            ++target.fixture_count;
            for (std::size_t property = 0U;
                 property < showcore::kPropertyCount;
                 ++property) {
                if (capabilities[index].properties[property]) {
                    ++target.property_support[property];
                }
            }
        }
        if (target.fixture_count == 0U) {
            target.complete = false;
        }
        override_targets_.push_back(std::move(target));
    }
    for (std::size_t index = 0U; index < active_project.fixtures.size(); ++index) {
        const auto& fixture = active_project.fixtures[index];
        LiveOverrideTarget target;
        target.kind = LiveOverrideTargetKind::Fixture;
        target.id = fixture.id;
        target.name = fixture.name;
        target.fixture_count = 1U;
        target.complete = capabilities[index].complete;
        for (std::size_t property = 0U;
             property < showcore::kPropertyCount;
             ++property) {
            target.property_support[property] =
                capabilities[index].properties[property] ? 1U : 0U;
        }
        override_targets_.push_back(std::move(target));
    }

    if (selected_autoloop_bank_ >= showcore::kMaxAutoloopBanks) {
        selected_autoloop_bank_ = 0U;
    }
    if (selected_autoloop_slot_ >= showcore::kAutoloopsPerBank) {
        selected_autoloop_slot_ = 0xFFU;
    }
    rebuild_autoloop_projection();
    rebuild_active_content();
}

void LiveViewModel::update(const RunnerStatus& status) noexcept {
    state_ = make_live_core_ui_state(status);
    const bool available = state_.runner == RunnerState::Running;
    for (auto& look : static_looks_) {
        look.active = state_.active_look == static_cast<std::int32_t>(look.compiled_index);
        look.available = available;
        if (look.active) {
            look.behavior = state_.static_look.behavior;
            look.status = state_.static_look.status;
            look.owner_kind = state_.static_look.owner_kind;
            look.owner_feedback_token =
                state_.static_look.owner_feedback_token;
            look.package_generation =
                state_.static_look.package_generation;
            look.activation_generation =
                state_.static_look.activation_generation;
            look.transition_progress = state_.static_look.transition_progress;
        } else {
            look.behavior = StaticLookBehavior::None;
            look.status = StaticLookActivationStatus::None;
            look.owner_kind = StaticLookOwnerKind::None;
            look.owner_feedback_token = 0U;
            look.package_generation = 0U;
            look.activation_generation = 0U;
            look.transition_progress = 0.0F;
        }
    }
    for (auto& script : track_scripts_) {
        script.active =
            state_.active_track_script == static_cast<std::int32_t>(script.compiled_index);
        script.available = available;
    }
    rebuild_autoloop_projection();
    rebuild_active_content();
}

bool LiveViewModel::select_autoloop_bank(std::uint16_t bank) noexcept {
    if (bank >= showcore::kMaxAutoloopBanks) {
        return false;
    }
    selected_autoloop_bank_ = bank;
    rebuild_autoloop_projection();
    return true;
}

bool LiveViewModel::select_autoloop_page(std::uint16_t page) noexcept {
    if (page >= showcore::kAutoloopControlPageCount) {
        return false;
    }
    const auto offset = static_cast<std::uint16_t>(
        selected_autoloop_bank_ % showcore::kAutoloopBanksPerControlPage);
    selected_autoloop_bank_ = static_cast<std::uint16_t>(
        page * showcore::kAutoloopBanksPerControlPage + offset);
    rebuild_autoloop_projection();
    return true;
}

bool LiveViewModel::select_autoloop_slot(std::uint8_t slot) noexcept {
    if (slot >= showcore::kAutoloopsPerBank) {
        return false;
    }
    selected_autoloop_slot_ = slot;
    rebuild_autoloop_projection();
    return true;
}

void LiveViewModel::clear_autoloop_selection() noexcept {
    selected_autoloop_slot_ = 0xFFU;
    rebuild_autoloop_projection();
}

showcore::AutoloopAddress LiveViewModel::selected_autoloop_address() const noexcept {
    return selected_autoloop_slot_ < showcore::kAutoloopsPerBank
        ? showcore::AutoloopAddress{selected_autoloop_bank_, selected_autoloop_slot_}
        : showcore::AutoloopAddress{};
}

void LiveViewModel::rebuild_autoloop_projection() noexcept {
    const bool runner_available = state_.runner == RunnerState::Running;
    const bool bank_enabled = (state_.active_autoloop_bank_mask &
                               (std::uint64_t{1} << selected_autoloop_bank_)) != 0U;
    for (std::size_t slot = 0U; slot < autoloop_pads_.size(); ++slot) {
        auto& pad = autoloop_pads_[slot];
        pad = {};
        pad.address = {
            selected_autoloop_bank_,
            static_cast<std::uint8_t>(slot)};
        pad.selected = selected_autoloop_slot_ == slot;
        pad.enabled_by_filter = bank_enabled;
        const auto found = std::find_if(
            autoloop_catalog_.begin(), autoloop_catalog_.end(),
            [&](const auto& loop) { return loop.address == pad.address; });
        if (found == autoloop_catalog_.end()) {
            continue;
        }
        pad.id = found->id;
        pad.name = found->name;
        pad.repeat = found->repeat;
        pad.populated = true;
        pad.available = runner_available;
        pad.active = state_.active_autoloop == pad.address;
        if (pad.active) {
            pad.progress = state_.active_autoloop_progress;
            pad.repeat = state_.active_autoloop_repeat;
        }
    }

    const auto first_bank = static_cast<std::uint16_t>(
        autoloop_page() * showcore::kAutoloopBanksPerControlPage);
    for (std::size_t offset = 0U; offset < autoloop_bank_window_.size(); ++offset) {
        auto& bank = autoloop_bank_window_[offset];
        bank.bank = static_cast<std::uint16_t>(first_bank + offset);
        bank.selected = bank.bank == selected_autoloop_bank_;
        bank.enabled_by_filter = (state_.active_autoloop_bank_mask &
                                  (std::uint64_t{1} << bank.bank)) != 0U;
        bank.contains_active = state_.active_autoloop.valid() &&
            state_.active_autoloop.bank == bank.bank;
    }
}

void LiveViewModel::rebuild_active_content() noexcept {
    active_content_ = {};
    if (state_.active_look >= 0) {
        const auto index = static_cast<std::size_t>(state_.active_look);
        if (index < static_looks_.size()) {
            active_content_.static_look_id = static_looks_[index].id;
            active_content_.static_look_name = static_looks_[index].name;
            active_content_.static_look_behavior =
                state_.static_look.behavior;
            active_content_.static_look_status = state_.static_look.status;
            active_content_.static_look_owner_kind =
                state_.static_look.owner_kind;
            active_content_.static_look_owner_feedback_token =
                state_.static_look.owner_feedback_token;
            active_content_.static_look_package_generation =
                state_.static_look.package_generation;
            active_content_.static_look_activation_generation =
                state_.static_look.activation_generation;
            active_content_.static_look_transition_progress =
                state_.static_look.transition_progress;
        }
    }
    if (state_.active_autoloop.valid()) {
        const auto found = std::find_if(
            autoloop_catalog_.begin(), autoloop_catalog_.end(),
            [&](const auto& loop) { return loop.address == state_.active_autoloop; });
        if (found != autoloop_catalog_.end()) {
            active_content_.autoloop_id = found->id;
            active_content_.autoloop_name = found->name;
        }
    }
    if (state_.active_track_script >= 0) {
        const auto index = static_cast<std::size_t>(state_.active_track_script);
        if (index < track_scripts_.size()) {
            active_content_.track_script_id = track_scripts_[index].id;
            active_content_.track_script_name = track_scripts_[index].name;
        }
    }
}

}  // namespace emberlights
