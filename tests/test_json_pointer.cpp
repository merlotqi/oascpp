#include <gtest/gtest.h>

#include <oascpp/json_pointer.hpp>
#include <string>

TEST(Oascpp, pointer_escapes_reserved_characters) {
  EXPECT_EQ(oascpp::escape_pointer_token("a~b/c"), std::string("a~0b~1c"));
  EXPECT_EQ(oascpp::escape_pointer_token("plain"), std::string("plain"));
  EXPECT_EQ(oascpp::escape_pointer_token(""), std::string());
}

TEST(Oascpp, pointer_unescapes_reserved_characters) {
  EXPECT_EQ(oascpp::unescape_pointer_token("a~0b~1c"), std::string("a~b/c"));
  EXPECT_EQ(oascpp::unescape_pointer_token("~2"), std::string("~2"));
  EXPECT_EQ(oascpp::unescape_pointer_token("trailing~"), std::string("trailing~"));
}

TEST(Oascpp, pointer_round_trips) {
  const std::string token = "a/b~c//~~";
  EXPECT_EQ(oascpp::unescape_pointer_token(oascpp::escape_pointer_token(token)), token);
}
