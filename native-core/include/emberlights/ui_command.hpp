#pragma once

#include "emberlights/generated/ui_registry.generated.hpp"
#include "emberlights/runner.hpp"

#include <cstdint>
#include <string_view>

namespace emberlights {

enum class UiStaticLookPreviewMode : std::uint8_t {
    None,
    Simulation,
    Physical
};

struct UiCommandInvocation {
    UiCommandId command{UiCommandId::ShowStart};
    bool bool_value{false};
    double number_value{0.0};
    showcore::Property property{showcore::Property::Count};
    std::string_view target_id{};
    std::string_view secondary_target_id{};
    UiStaticLookPreviewMode static_look_preview_mode{
        UiStaticLookPreviewMode::None};
    showcore::AutoloopAddress autoloop_address{};
    std::uint16_t bank{static_cast<std::uint16_t>(showcore::kMaxAutoloopBanks)};
    // Trusted invocation metadata, intentionally outside the generated/public
    // command argument registry. Hold controls must supply a stable nonzero
    // feedback token and echo both observed generations on release.
    StaticLookOwnerContext static_look_owner{
        StaticLookOwnerKind::Ui, 0U, 0U, 0U};
};

class UiAppCommandHost {
public:
    virtual ~UiAppCommandHost() = default;
    [[nodiscard]] virtual UiInvocationResult ui_start_show() noexcept = 0;
    [[nodiscard]] virtual UiInvocationResult ui_stop_show() noexcept = 0;
    [[nodiscard]] virtual UiInvocationResult ui_start_static_look_preview(
        std::string_view look_id,
        std::string_view target_id,
        UiStaticLookPreviewMode mode) noexcept {
        static_cast<void>(look_id);
        static_cast<void>(target_id);
        static_cast<void>(mode);
        return UiInvocationResult::Unsupported;
    }
    [[nodiscard]] virtual UiInvocationResult
    ui_stop_static_look_preview() noexcept {
        return UiInvocationResult::Unsupported;
    }
};

class UiCommandFacade {
public:
    UiCommandFacade(RunnerService& runner, UiAppCommandHost& app) noexcept
        : runner_(runner), app_(app) {}

    // The caller supplies the immutable ProjectDocument used to compile the
    // currently active Runner package. Draft/Studio documents must not be set
    // here because stable IDs are resolved to compiled indices before posting.
    void set_active_project(const ProjectDocument* project) noexcept {
        active_project_ = project;
    }

    [[nodiscard]] UiInvocationResult invoke(
        const UiCommandInvocation& invocation) noexcept;

private:
    RunnerService& runner_;
    UiAppCommandHost& app_;
    const ProjectDocument* active_project_{nullptr};
};

[[nodiscard]] const UiCommandDefinition* find_ui_command(
    std::string_view id) noexcept;
[[nodiscard]] const char* ui_invocation_result_name(
    UiInvocationResult result) noexcept;

}  // namespace emberlights
