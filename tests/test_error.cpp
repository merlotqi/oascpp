#include <cstddef>
#include <oascpp/error.hpp>
#include <string>
#include <string_view>
#include <utility>

#include "test_framework.hpp"

OASCPP_TEST(error_path_formats_a_json_pointer) {
  oascpp::ErrorPath path;
  path.push_key("paths").push_key("/pets").push_index(0).push_key("get").push_key("");

  OASCPP_CHECK(!path.empty());
  OASCPP_CHECK_EQ(path.depth(), std::size_t{5});
  OASCPP_CHECK_EQ(path.tokens().size(), std::size_t{5});
  OASCPP_CHECK_EQ(path.to_string(), std::string("/paths/~1pets/0/get/"));
}

OASCPP_TEST(error_path_empty_formats_as_the_root_pointer) {
  const oascpp::ErrorPath path;
  OASCPP_CHECK(path.empty());
  OASCPP_CHECK_EQ(path.depth(), std::size_t{0});
  OASCPP_CHECK_EQ(path.to_string(), std::string());
}

OASCPP_TEST(validation_result_accumulates_errors) {
  oascpp::ValidationResult result;
  OASCPP_CHECK(result.ok());
  OASCPP_CHECK(result.empty());
  OASCPP_CHECK_EQ(result.error_count(), std::size_t{0});
  OASCPP_CHECK(result.first_error() == nullptr);
  OASCPP_CHECK(result.first_error_code() == oascpp::ErrorCode::none);

  oascpp::ValidationError error;
  error.code = oascpp::ErrorCode::object_required;
  error.instance_path.push_key("name");
  error.message = "missing required member";
  error.expected = "string";
  result.add_error(std::move(error));
  result.add(oascpp::ErrorCode::type_mismatch);

  OASCPP_CHECK(!result.ok());
  OASCPP_CHECK_EQ(result.error_count(), std::size_t{2});
  OASCPP_CHECK(result.first_error() != nullptr);
  OASCPP_CHECK(result.first_error_code() == oascpp::ErrorCode::object_required);
  OASCPP_CHECK_EQ(result.first_error()->instance_path.to_string(), std::string("/name"));
  OASCPP_CHECK(result.errors().back().code == oascpp::ErrorCode::type_mismatch);

  result.clear();
  OASCPP_CHECK(result.ok());
  OASCPP_CHECK(result.first_error() == nullptr);
}

OASCPP_TEST(error_codes_have_stable_names) {
  OASCPP_CHECK_EQ(oascpp::to_string(oascpp::ErrorCode::none), std::string_view("none"));
  OASCPP_CHECK_EQ(oascpp::to_string(oascpp::ErrorCode::type_mismatch), std::string_view("type_mismatch"));
  OASCPP_CHECK_EQ(oascpp::to_string(oascpp::ErrorCode::security_unsatisfied), std::string_view("security_unsatisfied"));
  OASCPP_CHECK(oascpp::to_string(static_cast<oascpp::ErrorCode>(60000)) == std::string_view("unknown"));
}
