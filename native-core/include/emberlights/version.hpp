#pragma once

#include <string_view>

#ifndef EMBERLIGHTS_VERSION
#define EMBERLIGHTS_VERSION "0.1.0-dev"
#endif

#ifndef EMBERLIGHTS_COMMIT
#define EMBERLIGHTS_COMMIT "unknown"
#endif

namespace emberlights {

inline constexpr std::string_view kVersion = EMBERLIGHTS_VERSION;
inline constexpr std::string_view kCommit = EMBERLIGHTS_COMMIT;

}  // namespace emberlights
