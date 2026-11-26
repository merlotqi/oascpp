#include <gtest/gtest.h>

#include <cstddef>
#include <oascpp/error.hpp>
#include <string>
#include <string_view>
#include <utility>

TEST(Oascpp, error_path_formats_a_json_pointer) {
  oascpp::ErrorPath path;
  path.push_key("paths").push_key("/pets").push_index(0).push_key("get").push_key("");

  EXPECT_TRUE(!path.empty());
  EXPECT_EQ(path.depth(), std::size_t{5});
  EXPECT_EQ(path.tokens().size(), std::size_t{5});
  EXPECT_EQ(path.to_string(), std::string("/paths/~1pets/0/get/"));
}

TEST(Oascpp, error_path_empty_formats_as_the_root_pointer) {
  const oascpp::ErrorPath path;
  EXPECT_TRUE(path.empty());
  EXPECT_EQ(path.depth(), std::size_t{0});
  EXPECT_EQ(path.to_string(), std::string());
}

TEST(Oascpp, validation_result_accumulates_errors) {
  oascpp::ValidationResult result;
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.complete());
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(result.error_count(), std::size_t{0});
  EXPECT_TRUE(result.first_error() == nullptr);
  EXPECT_TRUE(result.first_error_code() == oascpp::ErrorCode::none);

  oascpp::ValidationError error;
  error.code = oascpp::ErrorCode::object_required;
  error.instance_path.push_key("name");
  error.message = "missing required member";
  error.expected = "string";
  result.add_error(std::move(error));
  result.add(oascpp::ErrorCode::type_mismatch);

  EXPECT_TRUE(!result.ok());
  EXPECT_EQ(result.error_count(), std::size_t{2});
  EXPECT_TRUE(result.first_error() != nullptr);
  EXPECT_TRUE(result.first_error_code() == oascpp::ErrorCode::object_required);
  EXPECT_EQ(result.first_error()->instance_path.to_string(), std::string("/name"));
  EXPECT_TRUE(result.errors().back().code == oascpp::ErrorCode::type_mismatch);

  result.clear();
  EXPECT_TRUE(result.ok());
  EXPECT_TRUE(result.complete());
  EXPECT_TRUE(result.first_error() == nullptr);
}

TEST(Oascpp, error_codes_have_stable_names) {
  EXPECT_EQ(oascpp::to_string(oascpp::ErrorCode::none), std::string_view("none"));
  EXPECT_EQ(oascpp::to_string(oascpp::ErrorCode::type_mismatch), std::string_view("type_mismatch"));
  EXPECT_EQ(oascpp::to_string(oascpp::ErrorCode::security_unsatisfied), std::string_view("security_unsatisfied"));
  EXPECT_TRUE(oascpp::to_string(static_cast<oascpp::ErrorCode>(60000)) == std::string_view("unknown"));
}
