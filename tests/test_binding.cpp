#include <cstdint>
#include <limits>
#include <utility>

#include "binding_fixture.hpp"
#include "json_dsl.hpp"
#include "oascpp/binding.hpp"
#include "test_framework.hpp"

using oascpp_test::Json;
using oascpp_test::User;
using oascpp_test::make_document;
using oascpp_test::register_user_bindings;

OASCPP_TEST(binding_decodes_nested_objects_and_arrays_registered_in_another_cpp) {
  oascpp::Bindings bindings;
  register_user_bindings(bindings);
  auto result = [&bindings] {
    const Json home = Json::object({{"postal-code", 10001}});
    const Json other = Json::object({{"postal-code", 10002}});
    const auto document = make_document(Json::object({
        {"name", "Alice"},
        {"home", home},
        {"addresses", Json::array({home, other})},
        {"scores", Json::array({Json::array({1, 2}), Json::array({3})})},
        {"flags", Json::array({true, false})},
    }));
    return bindings.decode<User>(document.root());
  }();

  OASCPP_CHECK(result.has_value());
  if (!result) {
    return;
  }
  const User user = std::move(result).value();
  OASCPP_CHECK_EQ(user.name, "Alice");
  OASCPP_CHECK_EQ(user.home.postcode, 10001);
  OASCPP_CHECK_EQ(user.addresses.at(1).postcode, 10002);
  OASCPP_CHECK_EQ(user.scores.at(0).at(1), 2);
  OASCPP_CHECK(!user.office);
  OASCPP_CHECK(user.flags.at(0) && !user.flags.at(1));
}

OASCPP_TEST(binding_reports_nested_field_and_array_element_paths) {
  oascpp::Bindings bindings;
  register_user_bindings(bindings);
  const auto document = make_document(Json::object({
      {"name", "Alice"},
      {"home", Json::object({})},
      {"addresses", Json::array({Json::object({{"postal-code", 10001}}),
                                  Json::object({{"postal-code", 2147483648LL}})})},
      {"scores", Json::array({Json::array({true})})},
      {"office", nullptr},
      {"flags", Json::array({})},
  }));
  const auto result = bindings.decode<User>(document.root());
  OASCPP_CHECK(!result);
  if (result) {
    return;
  }
  const auto& errors = result.diagnostics().errors();
  OASCPP_CHECK_EQ(errors.size(), 3u);
  if (errors.size() != 3) {
    return;
  }
  OASCPP_CHECK_EQ(errors[0].instance_path.to_string(), "/home/postal-code");
  OASCPP_CHECK(errors[0].code == oascpp::ErrorCode::missing_field);
  OASCPP_CHECK_EQ(errors[1].instance_path.to_string(), "/addresses/1/postal-code");
  OASCPP_CHECK(errors[1].code == oascpp::ErrorCode::invalid_value);
  OASCPP_CHECK_EQ(errors[2].instance_path.to_string(), "/scores/0/0");
  OASCPP_CHECK(errors[2].code == oascpp::ErrorCode::type_mismatch);
}

OASCPP_TEST(binding_checks_integer_limits_and_missing_registration) {
  const oascpp::Bindings bindings;
  oascpp::DocumentBuilder builder;
  static_cast<void>(builder.make_unsigned(std::numeric_limits<std::uint64_t>::max()));
  const auto high = builder.finish();
  const auto wide = bindings.decode<std::uint64_t>(high.root());
  OASCPP_CHECK(wide && wide.value() == std::numeric_limits<std::uint64_t>::max());
  OASCPP_CHECK(!bindings.decode<std::int64_t>(high.root()));

  const auto exact = make_document(Json(-9223372036854775808.0));
  const auto low = bindings.decode<std::int64_t>(exact.root());
  OASCPP_CHECK(low && low.value() == std::numeric_limits<std::int64_t>::lowest());
  const auto above = make_document(Json(9223372036854775808.0));
  OASCPP_CHECK(!bindings.decode<std::int64_t>(above.root()));

  const auto object = make_document(Json::object({}));
  const auto missing = bindings.decode<User>(object.root());
  OASCPP_CHECK(!missing &&
               missing.diagnostics().first_error_code() == oascpp::ErrorCode::binding_not_registered);
}
