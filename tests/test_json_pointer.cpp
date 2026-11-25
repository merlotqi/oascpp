#include <string>

#include "oascpp/json_pointer.hpp"
#include "test_framework.hpp"

OASCPP_TEST(pointer_escapes_reserved_characters) {
  OASCPP_CHECK_EQ(oascpp::escape_pointer_token("a~b/c"), std::string("a~0b~1c"));
  OASCPP_CHECK_EQ(oascpp::escape_pointer_token("plain"), std::string("plain"));
  OASCPP_CHECK_EQ(oascpp::escape_pointer_token(""), std::string());
}

OASCPP_TEST(pointer_unescapes_reserved_characters) {
  OASCPP_CHECK_EQ(oascpp::unescape_pointer_token("a~0b~1c"), std::string("a~b/c"));
  OASCPP_CHECK_EQ(oascpp::unescape_pointer_token("~2"), std::string("~2"));
  OASCPP_CHECK_EQ(oascpp::unescape_pointer_token("trailing~"), std::string("trailing~"));
}

OASCPP_TEST(pointer_round_trips) {
  const std::string token = "a/b~c//~~";
  OASCPP_CHECK_EQ(oascpp::unescape_pointer_token(oascpp::escape_pointer_token(token)), token);
}
