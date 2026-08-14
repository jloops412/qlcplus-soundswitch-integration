#pragma once

#include "showcore/autoloop.hpp"
#include "showcore/fixture.hpp"
#include "showcore/layer_resolver.hpp"
#include "showcore/types.hpp"

namespace showcore {

class Engine {
public:
    [[nodiscard]] Patch& patch() noexcept { return patch_; }
    [[nodiscard]] const Patch& patch() const noexcept { return patch_; }
    [[nodiscard]] LayerStack& layers() noexcept { return layers_; }
    [[nodiscard]] SafetyPolicy& safety() noexcept { return safety_; }
    [[nodiscard]] const DmxFrames& frames() const noexcept { return frames_; }
    [[nodiscard]] const DmxFrameAttribution& frame_attribution() const noexcept {
        return frame_attribution_;
    }

    void tick() noexcept;

private:
    Patch patch_{};
    LayerStack layers_{};
    SafetyPolicy safety_{};
    DmxFrames frames_{};
    DmxFrameAttribution frame_attribution_{};
    FixtureRenderer renderer_{};
};

}  // namespace showcore
