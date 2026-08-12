#pragma once

#include "emberlights/project.hpp"
#include "emberlights/ui_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

enum class LiveOverrideTargetKind : std::uint8_t {
    Fixture,
    Group
};

struct LiveOverrideTarget {
    LiveOverrideTargetKind kind{LiveOverrideTargetKind::Fixture};
    std::string id;
    std::string name;
    std::uint16_t fixture_count{0U};
    bool complete{false};
    std::array<std::uint16_t, showcore::kPropertyCount> property_support{};

    [[nodiscard]] std::uint16_t support_count(
        showcore::Property property) const noexcept;
    [[nodiscard]] bool supports_any(showcore::Property property) const noexcept;
    [[nodiscard]] bool supports_all(showcore::Property property) const noexcept;
};

struct LiveStaticLookPad {
    std::string id;
    std::string name;
    std::uint16_t compiled_index{0U};
    bool active{false};
    bool available{false};
};

struct LiveAutoloopPad {
    showcore::AutoloopAddress address{};
    std::string_view id;
    std::string_view name;
    showcore::AutoloopRepeat repeat{showcore::AutoloopRepeat::Once};
    float progress{0.0F};
    bool populated{false};
    bool selected{false};
    bool active{false};
    bool enabled_by_filter{true};
    bool available{false};
};

struct LiveAutoloopBank {
    std::uint16_t bank{0U};
    bool selected{false};
    bool enabled_by_filter{true};
    bool contains_active{false};
};

struct LiveTrackScriptItem {
    std::string id;
    std::string name;
    std::uint16_t compiled_index{0U};
    bool active{false};
    bool available{false};
};

struct LiveActiveContent {
    std::string_view static_look_id;
    std::string_view static_look_name;
    std::string_view autoloop_id;
    std::string_view autoloop_name;
    std::string_view track_script_id;
    std::string_view track_script_name;
};

// Toolkit-neutral, UI-thread-owned projection of one immutable active project
// plus Runner snapshots. It contains no Win32/toolkit handles and invokes no
// commands. Selection and bank paging are view-local only.
class LiveViewModel {
public:
    void load_project(const ProjectDocument& active_project);
    void update(const RunnerStatus& status) noexcept;

    [[nodiscard]] bool select_autoloop_bank(std::uint16_t bank) noexcept;
    [[nodiscard]] bool select_autoloop_page(std::uint16_t page) noexcept;
    [[nodiscard]] bool select_autoloop_slot(std::uint8_t slot) noexcept;
    void clear_autoloop_selection() noexcept;

    [[nodiscard]] const std::string& project_id() const noexcept { return project_id_; }
    [[nodiscard]] const std::string& project_name() const noexcept { return project_name_; }
    [[nodiscard]] const LiveCoreUiState& state() const noexcept { return state_; }
    [[nodiscard]] const SafetySettings& safety() const noexcept { return safety_; }
    [[nodiscard]] const std::vector<LiveStaticLookPad>& static_looks() const noexcept {
        return static_looks_;
    }
    [[nodiscard]] const std::array<LiveAutoloopPad, showcore::kAutoloopsPerBank>&
    autoloop_pads() const noexcept {
        return autoloop_pads_;
    }
    [[nodiscard]] const std::array<LiveAutoloopBank,
                                   showcore::kAutoloopBanksPerControlPage>&
    autoloop_bank_window() const noexcept {
        return autoloop_bank_window_;
    }
    [[nodiscard]] const std::vector<LiveTrackScriptItem>& track_scripts() const noexcept {
        return track_scripts_;
    }
    [[nodiscard]] const std::vector<LiveOverrideTarget>& override_targets() const noexcept {
        return override_targets_;
    }
    [[nodiscard]] const LiveActiveContent& active_content() const noexcept {
        return active_content_;
    }
    [[nodiscard]] std::uint16_t selected_autoloop_bank() const noexcept {
        return selected_autoloop_bank_;
    }
    [[nodiscard]] std::uint16_t autoloop_page() const noexcept {
        return static_cast<std::uint16_t>(
            selected_autoloop_bank_ / showcore::kAutoloopBanksPerControlPage);
    }
    [[nodiscard]] showcore::AutoloopAddress selected_autoloop_address() const noexcept;

private:
    struct AutoloopCatalogItem {
        std::string id;
        std::string name;
        showcore::AutoloopAddress address{};
        showcore::AutoloopRepeat repeat{showcore::AutoloopRepeat::Once};
    };

    void rebuild_autoloop_projection() noexcept;
    void rebuild_active_content() noexcept;

    std::string project_id_;
    std::string project_name_;
    SafetySettings safety_{};
    LiveCoreUiState state_{};
    std::vector<LiveStaticLookPad> static_looks_;
    std::vector<AutoloopCatalogItem> autoloop_catalog_;
    std::array<LiveAutoloopPad, showcore::kAutoloopsPerBank> autoloop_pads_{};
    std::array<LiveAutoloopBank,
               showcore::kAutoloopBanksPerControlPage> autoloop_bank_window_{};
    std::vector<LiveTrackScriptItem> track_scripts_;
    std::vector<LiveOverrideTarget> override_targets_;
    LiveActiveContent active_content_{};
    std::uint16_t selected_autoloop_bank_{0U};
    std::uint8_t selected_autoloop_slot_{0xFFU};
};

}  // namespace emberlights
