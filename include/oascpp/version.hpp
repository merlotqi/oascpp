#pragma once

// Library version. The macro form is usable from preprocessor checks and CMake;
// the namespace constants are usable from code.

#include <string_view>

#define OASCPP_VERSION_MAJOR 0
#define OASCPP_VERSION_MINOR 3
#define OASCPP_VERSION_PATCH 0
#define OASCPP_VERSION_STRING "0.3.0"

namespace oascpp {

struct Version {
  int major = OASCPP_VERSION_MAJOR;
  int minor = OASCPP_VERSION_MINOR;
  int patch = OASCPP_VERSION_PATCH;
};

inline constexpr Version version{OASCPP_VERSION_MAJOR, OASCPP_VERSION_MINOR, OASCPP_VERSION_PATCH};
inline constexpr std::string_view version_string = OASCPP_VERSION_STRING;

}  // namespace oascpp
