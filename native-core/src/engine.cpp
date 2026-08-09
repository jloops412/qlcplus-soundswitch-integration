#include "showcore/engine.hpp"

namespace showcore {

void Engine::tick() noexcept {
    renderer_.render(patch_, layers_, safety_, frames_);
}

}  // namespace showcore
