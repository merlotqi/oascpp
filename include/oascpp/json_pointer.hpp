#pragma once

// RFC 6901 JSON Pointer helpers. Pointer fragments are escaped by replacing
// '~' with "~0" and '/' with "~1"; the unescape direction leaves unknown
// escape sequences untouched instead of failing.

#include <cstddef>
#include <string>
#include <string_view>

#include "oascpp/value.hpp"

namespace oascpp {

inline void append_escaped_pointer_token(std::string& out, std::string_view token) {
  for (const char character : token) {
    if (character == '~') {
      out.append("~0");
    } else if (character == '/') {
      out.append("~1");
    } else {
      out.push_back(character);
    }
  }
}

[[nodiscard]] inline std::string escape_pointer_token(std::string_view token) {
  std::string out;
  out.reserve(token.size());
  append_escaped_pointer_token(out, token);
  return out;
}

[[nodiscard]] inline std::string unescape_pointer_token(std::string_view token) {
  std::string out;
  out.reserve(token.size());
  std::size_t index = 0;
  while (index < token.size()) {
    if (token[index] == '~' && index + 1 < token.size()) {
      const char next = token[index + 1];
      if (next == '0') {
        out.push_back('~');
        index += 2;
        continue;
      }
      if (next == '1') {
        out.push_back('/');
        index += 2;
        continue;
      }
    }
    out.push_back(token[index]);
    ++index;
  }
  return out;
}

}  // namespace oascpp
