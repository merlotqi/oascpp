#pragma once

// RFC 6901 JSON Pointer helpers. Pointer fragments are escaped by replacing
// '~' with "~0" and '/' with "~1"; the unescape direction leaves unknown
// escape sequences untouched instead of failing.

#include <string>
#include <string_view>

namespace oascpp {

void append_escaped_pointer_token(std::string& out, std::string_view token);
[[nodiscard]] std::string escape_pointer_token(std::string_view token);
[[nodiscard]] std::string unescape_pointer_token(std::string_view token);

}  // namespace oascpp
