#pragma once

#include <string_view>

namespace emberlights {

// Canonical source-backed profile identities for the first owned-fixture
// qualification family. Keep these in a dependency-light header so Studio,
// migration, and hardware qualification cannot create competing IDs.
inline constexpr std::string_view kBothLightingBoIr4SixChannelProfileId =
    "builtin.both-lighting.bo-ir4.6ch.manual-v1";
inline constexpr std::string_view kBothLightingBoIr4TenChannelProfileId =
    "builtin.both-lighting.bo-ir4.10ch.manual-v1";

// Manufacturer manual downloaded from Both Lighting USA support on
// 2026-08-11. The digest covers the exact PDF bytes; the DMX table is PDF page
// 5 (zero-independent human page numbering).
inline constexpr std::string_view kBothLightingBoIr4ManualRevision =
    "manual-sha256:1267e289b2c0577ec749f0de5265105db5e86b6ae3b2e12414cc00777fd3c03a#p5";

// Legacy public name retained as a source-compatible alias. It intentionally
// resolves to the canonical ID and must not be used to mint another profile.
inline constexpr std::string_view kBothLightingIr4SixChannelProfileId =
    kBothLightingBoIr4SixChannelProfileId;

}  // namespace emberlights
