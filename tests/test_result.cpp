#include <string>
#include <utility>

#include "oascpp/result.hpp"
#include "test_framework.hpp"

namespace {

oascpp::Result<int> make_result(bool success) {
  if (success) {
    return 42;
  }
  return oascpp::Error{oascpp::ErrorCode::invalid_value, "out of range"};
}

}  // namespace

OASCPP_TEST(result_holds_a_value) {
  const oascpp::Result<int> result = make_result(true);

  OASCPP_CHECK(result.has_value());
  OASCPP_CHECK(static_cast<bool>(result));
  OASCPP_CHECK_EQ(result.value(), 42);
}

OASCPP_TEST(result_holds_an_error) {
  const oascpp::Result<int> result = make_result(false);

  OASCPP_CHECK(!result.has_value());
  OASCPP_CHECK(!static_cast<bool>(result));
  OASCPP_CHECK(result.error().code == oascpp::ErrorCode::invalid_value);
  OASCPP_CHECK_EQ(result.error().message, std::string("out of range"));
}

OASCPP_TEST(result_moves_its_value) {
  oascpp::Result<std::string> result = std::string("payload");
  const std::string taken = std::move(result).value();

  OASCPP_CHECK_EQ(taken, std::string("payload"));
}

OASCPP_TEST(result_void_tracks_success) {
  const oascpp::Result<void> success;
  OASCPP_CHECK(success.has_value());
  OASCPP_CHECK(static_cast<bool>(success));

  const oascpp::Result<void> failure = oascpp::Error{oascpp::ErrorCode::parse_error, "bad input"};
  OASCPP_CHECK(!failure.has_value());
  OASCPP_CHECK(!static_cast<bool>(failure));
  OASCPP_CHECK(failure.error().code == oascpp::ErrorCode::parse_error);
}
