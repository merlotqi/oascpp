#include <gtest/gtest.h>

#include <oascpp/result.hpp>
#include <string>
#include <utility>

namespace {

oascpp::Result<int> make_result(bool success) {
  if (success) {
    return 42;
  }
  return oascpp::Error{oascpp::ErrorCode::invalid_value, "out of range"};
}

}  // namespace

TEST(Oascpp, result_holds_a_value) {
  const oascpp::Result<int> result = make_result(true);

  EXPECT_TRUE(result.has_value());
  EXPECT_TRUE(static_cast<bool>(result));
  EXPECT_EQ(result.value(), 42);
}

TEST(Oascpp, result_holds_an_error) {
  const oascpp::Result<int> result = make_result(false);

  EXPECT_TRUE(!result.has_value());
  EXPECT_TRUE(!static_cast<bool>(result));
  EXPECT_TRUE(result.error().code == oascpp::ErrorCode::invalid_value);
  EXPECT_EQ(result.error().message, std::string("out of range"));
}

TEST(Oascpp, result_moves_its_value) {
  oascpp::Result<std::string> result = std::string("payload");
  const std::string taken = std::move(result).value();

  EXPECT_EQ(taken, std::string("payload"));
}

TEST(Oascpp, result_void_tracks_success) {
  const oascpp::Result<void> success;
  EXPECT_TRUE(success.has_value());
  EXPECT_TRUE(static_cast<bool>(success));

  const oascpp::Result<void> failure = oascpp::Error{oascpp::ErrorCode::parse_error, "bad input"};
  EXPECT_TRUE(!failure.has_value());
  EXPECT_TRUE(!static_cast<bool>(failure));
  EXPECT_TRUE(failure.error().code == oascpp::ErrorCode::parse_error);
}
