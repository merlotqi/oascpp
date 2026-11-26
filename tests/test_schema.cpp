#include <oascpp/schema/compiled_schema.hpp>
#include <oascpp/schema/compiler.hpp>
#include <oascpp/schema/dialect.hpp>
#include <oascpp/schema/schema.hpp>
#include <oascpp/schema/validator.hpp>
#include <string_view>

#include "json_dsl.hpp"
#include "test_framework.hpp"

namespace {

namespace js = oascpp::schema;
using oascpp::ErrorCode;
using oascpp_test::Json;
using oascpp_test::make_document;

[[nodiscard]] js::Schema compile_or_fail(const Json& schema_json) {
  auto compiled = js::Schema::compile(make_document(schema_json));
  OASCPP_CHECK(compiled.has_value());
  return compiled.has_value() ? compiled.value() : js::Schema();
}

OASCPP_TEST(schema_accepts_matching_object) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "object"},
      {"properties", Json::object({{"name", Json::object({{"type", "string"}})}})},
      {"required", Json::array({"name"})},
  }));

  const auto document = make_document(Json::object({{"name", "pet"}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  OASCPP_CHECK(result.ok());
}

OASCPP_TEST(schema_reports_missing_required_property) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "object"},
      {"required", Json::array({"name"})},
  }));

  const auto document = make_document(Json::object({{"age", 3}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  OASCPP_CHECK(!result.ok());
  OASCPP_CHECK(result.first_error_code() == ErrorCode::object_required);
  OASCPP_CHECK(result.first_error()->instance_path.to_string() == "/name");
}

OASCPP_TEST(schema_reports_nested_type_mismatch) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"properties", Json::object({{"age", Json::object({{"type", "integer"}})}})},
  }));

  const auto document = make_document(Json::object({{"age", "three"}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  OASCPP_CHECK(!result.ok());
  OASCPP_CHECK(result.first_error_code() == ErrorCode::type_mismatch);
  OASCPP_CHECK(result.first_error()->instance_path.to_string() == "/age");
}

OASCPP_TEST(schema_integer_type_accepts_integral_real) {
  const js::Schema schema = compile_or_fail(Json::object({{"type", "integer"}}));

  const auto integral = make_document(Json(4.0));
  OASCPP_CHECK(schema.validate(integral.root()).ok());

  const auto fractional = make_document(Json(4.5));
  OASCPP_CHECK(!schema.validate(fractional.root()).ok());
}

OASCPP_TEST(schema_checks_numeric_bounds) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "number"},
      {"minimum", 0},
      {"exclusiveMaximum", 10},
      {"multipleOf", 2},
  }));

  const auto valid = make_document(Json(8));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto too_small = make_document(Json(-2));
  OASCPP_CHECK(schema.validate(too_small.root()).first_error_code() == ErrorCode::numeric_minimum);

  const auto at_exclusive_maximum = make_document(Json(10));
  OASCPP_CHECK(schema.validate(at_exclusive_maximum.root()).first_error_code() == ErrorCode::numeric_exclusive_maximum);

  const auto not_a_multiple = make_document(Json(7));
  OASCPP_CHECK(schema.validate(not_a_multiple.root()).first_error_code() == ErrorCode::numeric_multiple_of);
}

OASCPP_TEST(schema_counts_utf8_code_points) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "string"},
      {"minLength", 4},
      {"maxLength", 5},
  }));

  // Five code points, six bytes.
  const auto valid = make_document(Json("h\xC3\xA9llo"));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto too_long = make_document(Json("hello!"));
  OASCPP_CHECK(schema.validate(too_long.root()).first_error_code() == ErrorCode::string_max_length);
}

OASCPP_TEST(schema_checks_array_prefix_and_items) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "array"},
      {"prefixItems", Json::array({Json::object({{"type", "integer"}})})},
      {"items", Json::object({{"type", "string"}})},
      {"minItems", 2},
  }));

  const auto valid = make_document(Json::array({1, "a", "b"}));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto bad_prefix = make_document(Json::array({"a", "b"}));
  const auto prefix_result = schema.validate(bad_prefix.root());
  OASCPP_CHECK(!prefix_result.ok());
  OASCPP_CHECK(prefix_result.first_error()->instance_path.to_string() == "/0");

  const auto too_short = make_document(Json::array({1}));
  OASCPP_CHECK(schema.validate(too_short.root()).first_error_code() == ErrorCode::array_min_items);
}

OASCPP_TEST(schema_rejects_duplicate_items) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "array"},
      {"uniqueItems", true},
  }));

  const auto duplicate = make_document(Json::array({1, 2, 1}));
  OASCPP_CHECK(schema.validate(duplicate.root()).first_error_code() == ErrorCode::array_unique_items);

  // 1 and 1.0 are the same JSON number.
  const auto numeric_duplicate = make_document(Json::array({1, 1.0}));
  OASCPP_CHECK(!schema.validate(numeric_duplicate.root()).ok());
}

OASCPP_TEST(schema_composition_keywords) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"allOf", Json::array({Json::object({{"type", "object"}})})},
      {"anyOf", Json::array({Json::object({{"required", Json::array({"a"})}}),
                             Json::object({{"required", Json::array({"b"})}})})},
      {"not", Json::object({{"required", Json::array({"forbidden"})}})},
  }));

  const auto valid = make_document(Json::object({{"a", 1}}));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto no_branch = make_document(Json::object({{"c", 1}}));
  OASCPP_CHECK(schema.validate(no_branch.root()).first_error_code() == ErrorCode::composition_any_of);

  const auto forbidden = make_document(Json::object({{"a", 1}, {"forbidden", true}}));
  OASCPP_CHECK(schema.validate(forbidden.root()).first_error_code() == ErrorCode::composition_not);
}

OASCPP_TEST(schema_enum_and_const) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"enum", Json::array({"red", "green", 3})},
  }));

  const auto valid = make_document(Json(3));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto invalid = make_document(Json("blue"));
  OASCPP_CHECK(schema.validate(invalid.root()).first_error_code() == ErrorCode::enum_mismatch);

  const js::Schema const_schema = compile_or_fail(Json::object({{"const", "fixed"}}));
  const auto wrong = make_document(Json("other"));
  OASCPP_CHECK(const_schema.validate(wrong.root()).first_error_code() == ErrorCode::const_mismatch);
}

OASCPP_TEST(schema_resolves_recursive_refs) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"$defs",
       Json::object({{"node", Json::object({
                                  {"type", "object"},
                                  {"properties", Json::object({{"next", Json::object({{"$ref", "#/$defs/node"}})}})},
                              })}})},
      {"$ref", "#/$defs/node"},
  }));

  const auto valid = make_document(Json::object({{"next", Json::object({{"next", Json::object({})}})}}));
  OASCPP_CHECK(schema.validate(valid.root()).ok());

  const auto invalid = make_document(Json::object({{"next", Json(3)}}));
  const auto result = schema.validate(invalid.root());
  OASCPP_CHECK(!result.ok());
  OASCPP_CHECK(result.first_error_code() == ErrorCode::type_mismatch);
  OASCPP_CHECK(result.first_error()->instance_path.to_string() == "/next");
}

OASCPP_TEST(schema_pattern_needs_a_matcher) {
  const Json pattern_schema = Json::object({
      {"type", "string"},
      {"pattern", "^a+$"},
  });

  const js::Schema without_matcher = compile_or_fail(pattern_schema);
  const auto instance = make_document(Json("bbb"));
  // The keyword is preserved but not interpreted without an engine.
  OASCPP_CHECK(without_matcher.validate(instance.root()).ok());

  js::ValidationOptions options;
  options.pattern_matcher = [](std::string_view pattern, std::string_view candidate) {
    static_cast<void>(pattern);
    return !candidate.empty() && candidate.front() == 'a' && candidate.find('b') == std::string_view::npos;
  };
  const auto with_matcher = without_matcher.validate(instance.root(), options);
  OASCPP_CHECK(with_matcher.first_error_code() == ErrorCode::string_pattern);
}

OASCPP_TEST(schema_compile_rejects_bad_input) {
  const auto unknown_type = js::Schema::compile(make_document(Json::object({{"type", "nope"}})));
  OASCPP_CHECK(!unknown_type.has_value());
  OASCPP_CHECK(unknown_type.error().code == ErrorCode::schema_invalid);

  const auto bad_reference = js::Schema::compile(make_document(Json::object({{"$ref", "#/$defs/missing"}})));
  OASCPP_CHECK(!bad_reference.has_value());
  OASCPP_CHECK(bad_reference.error().code == ErrorCode::unresolved_reference);

  js::CompileOptions options;
  options.dialect = js::Dialect::draft4;
  const auto unsupported = js::Schema::compile(make_document(Json::object({{"type", "string"}})), options);
  OASCPP_CHECK(!unsupported.has_value());
  OASCPP_CHECK(unsupported.error().code == ErrorCode::unsupported_dialect);
}

OASCPP_TEST(schema_boolean_schemas) {
  const js::Schema accept = compile_or_fail(Json(true));
  const auto instance = make_document(Json(1));
  OASCPP_CHECK(accept.validate(instance.root()).ok());

  const js::Schema reject = compile_or_fail(Json(false));
  OASCPP_CHECK(!reject.validate(instance.root()).ok());
}

}  // namespace
