#pragma once

#include "emberlights/ember_action_compiler.hpp"
#include "emberlights/generated/ui_registry.generated.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace emberlights {

// Read-only, authoring/compiler-side bridge from the accepted generated UI
// registry into Ember Action validation. Construction reads only compiled
// const metadata; it performs no file, network, device, Runner, or dispatch
// work. Planned registry entries are deliberately not exposed as callable.
class GeneratedUiRegistryEmberActionView final : public EmberActionRegistryView {
public:
    GeneratedUiRegistryEmberActionView();

    [[nodiscard]] std::string_view registry_digest() const noexcept override;
    [[nodiscard]] const EmberActionCommandContract* find_command(
        std::string_view id) const noexcept override;
    [[nodiscard]] const EmberActionStateContract* find_state(
        std::string_view id) const noexcept override;
    [[nodiscard]] const EmberActionCapabilityContract* find_capability(
        std::string_view id) const noexcept override;
    [[nodiscard]] const EmberActionDependencyContract* find_action(
        std::string_view id,
        std::string_view version_range) const noexcept override;
    [[nodiscard]] const EmberActionValueContract* find_context_value(
        std::string_view path) const noexcept override;
    [[nodiscard]] bool supports_curve(std::string_view id) const noexcept override;
    [[nodiscard]] bool supports_unit_conversion(
        std::string_view source,
        std::string_view target) const noexcept override;

    [[nodiscard]] std::optional<UiCommandId> native_command_id(
        std::string_view id) const noexcept;
    [[nodiscard]] std::optional<std::size_t> native_state_ordinal(
        std::string_view id) const noexcept;

private:
    std::vector<EmberActionCommandContract> commands_;
    std::vector<EmberActionStateContract> states_;
};

inline constexpr std::uint32_t kEmberActionIrCompilerGeneration = 1U;

struct EmberActionIrCacheKey {
    std::uint32_t compiler_generation{kEmberActionIrCompilerGeneration};
    std::string source_hash;
    std::string registry_digest;
    std::string dependency_digest;
    std::string cache_digest;
};

struct EmberActionResolvedCommandReference {
    std::string id;
    UiCommandId command{UiCommandId::ShowStart};
};

struct EmberActionResolvedStateReference {
    std::string id;
    std::size_t native_ordinal{0U};
};

// This is an immutable, non-executable IR foundation. It resolves prepared
// dependencies to accepted generated native handles and defines cache identity,
// but intentionally contains no instruction stream, callback, dispatcher,
// queue, timer, state writer, or activation behavior.
struct EmberActionIrFoundation {
    std::shared_ptr<const EmberActionPreparedSource> prepared;
    std::vector<EmberActionResolvedCommandReference> commands;
    std::vector<EmberActionResolvedStateReference> states;
    EmberActionIrCacheKey cache_key;
};

struct EmberActionIrFoundationResult {
    std::shared_ptr<const EmberActionIrFoundation> ir;
    std::vector<EmberActionDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return ir != nullptr && diagnostics.empty();
    }
};

[[nodiscard]] EmberActionIrFoundationResult compile_ember_action_ir_foundation(
    std::shared_ptr<const EmberActionPreparedSource> prepared,
    const GeneratedUiRegistryEmberActionView& registry);

}  // namespace emberlights
