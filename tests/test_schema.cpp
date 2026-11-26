#include <gtest/gtest.h>

#include <oascpp/schema/compiled_schema.hpp>
#include <oascpp/schema/compiler.hpp>
#include <oascpp/schema/dialect.hpp>
#include <oascpp/schema/schema.hpp>
#include <oascpp/schema/validator.hpp>
#include <string_view>

#include "json_dsl.hpp"

namespace {

namespace js = oascpp::schema;
using oascpp::ErrorCode;
using oascpp_test::Json;
using oascpp_test::make_document;

[[nodiscard]] js::Schema compile_or_fail(const Json& schema_json) {
  auto compiled = js::Schema::compile(make_document(schema_json));
  EXPECT_TRUE(compiled.has_value());
  return compiled.has_value() ? compiled.value() : js::Schema();
}

TEST(Oascpp, schema_accepts_matching_object) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "object"},
      {"properties", Json::object({{"name", Json::object({{"type", "string"}})}})},
      {"required", Json::array({"name"})},
  }));

  const auto document = make_document(Json::object({{"name", "pet"}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  EXPECT_TRUE(result.ok());
}

TEST(Oascpp, schema_reports_missing_required_property) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "object"},
      {"required", Json::array({"name"})},
  }));

  const auto document = make_document(Json::object({{"age", 3}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  EXPECT_TRUE(!result.ok());
  EXPECT_TRUE(result.first_error_code() == ErrorCode::object_required);
  EXPECT_TRUE(result.first_error()->instance_path.to_string() == "/name");
}

TEST(Oascpp, schema_reports_nested_type_mismatch) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"properties", Json::object({{"age", Json::object({{"type", "integer"}})}})},
  }));

  const auto document = make_document(Json::object({{"age", "three"}}));
  const oascpp::ValidationResult result = schema.validate(document.root());
  EXPECT_TRUE(!result.ok());
  EXPECT_TRUE(result.first_error_code() == ErrorCode::type_mismatch);
  EXPECT_TRUE(result.first_error()->instance_path.to_string() == "/age");
  EXPECT_TRUE(result.first_error()->schema_path.to_string() == "/properties/age/type");
}

TEST(Oascpp, schema_integer_type_accepts_integral_real) {
  const js::Schema schema = compile_or_fail(Json::object({{"type", "integer"}}));

  const auto integral = make_document(Json(4.0));
  EXPECT_TRUE(schema.validate(integral.root()).ok());

  const auto fractional = make_document(Json(4.5));
  EXPECT_TRUE(!schema.validate(fractional.root()).ok());
}

TEST(Oascpp, schema_checks_numeric_bounds) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "number"},
      {"minimum", 0},
      {"exclusiveMaximum", 10},
      {"multipleOf", 2},
  }));

  const auto valid = make_document(Json(8));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto too_small = make_document(Json(-2));
  EXPECT_TRUE(schema.validate(too_small.root()).first_error_code() == ErrorCode::numeric_minimum);

  const auto at_exclusive_maximum = make_document(Json(10));
  EXPECT_TRUE(schema.validate(at_exclusive_maximum.root()).first_error_code() == ErrorCode::numeric_exclusive_maximum);

  const auto not_a_multiple = make_document(Json(7));
  EXPECT_TRUE(schema.validate(not_a_multiple.root()).first_error_code() == ErrorCode::numeric_multiple_of);
}

TEST(Oascpp, schema_compares_large_integer_bounds_exactly) {
  const js::Schema schema = compile_or_fail(Json::object({{"minimum", Json(9007199254740993LL)}}));
  const auto below = make_document(Json(9007199254740992LL));
  const auto equal = make_document(Json(9007199254740993LL));

  EXPECT_TRUE(schema.validate(below.root()).first_error_code() == ErrorCode::numeric_minimum);
  EXPECT_TRUE(schema.validate(equal.root()).ok());
}

TEST(Oascpp, schema_multiple_of_uses_exact_integer_arithmetic_and_tight_real_tolerance) {
  const js::Schema integer_schema = compile_or_fail(Json::object({{"multipleOf", 3}}));
  const auto large_non_multiple = make_document(Json(9007199254740994LL));
  EXPECT_TRUE(integer_schema.validate(large_non_multiple.root()).first_error_code() == ErrorCode::numeric_multiple_of);

  const js::Schema real_schema = compile_or_fail(Json::object({{"multipleOf", 1.0}}));
  const auto fractional = make_document(Json(1000000000.5));
  EXPECT_TRUE(real_schema.validate(fractional.root()).first_error_code() == ErrorCode::numeric_multiple_of);

  const js::Schema decimal_schema = compile_or_fail(Json::object({{"multipleOf", 0.1}}));
  const auto decimal_multiple = make_document(Json(0.3));
  EXPECT_TRUE(decimal_schema.validate(decimal_multiple.root()).ok());
}

TEST(Oascpp, schema_accepts_integral_real_and_wide_unsigned_limits) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"minItems", 1.0},
      {"maxItems", Json(4294967296LL)},
  }));
  const auto instance = make_document(Json::array({1}));
  EXPECT_TRUE(schema.validate(instance.root()).ok());
}

TEST(Oascpp, schema_draft_2020_rejects_openapi_30_and_legacy_keyword_shapes) {
  const auto nullable = js::Schema::compile(make_document(Json::object({
      {"type", "string"},
      {"nullable", true},
  })));
  EXPECT_TRUE(nullable.has_value());
  if (nullable) {
    const auto null_instance = make_document(Json(nullptr));
    EXPECT_TRUE(!nullable.value().validate(null_instance.root()).ok());
  }

  const auto boolean_exclusive = js::Schema::compile(make_document(Json::object({{"exclusiveMinimum", true}})));
  EXPECT_TRUE(!boolean_exclusive.has_value());

  const auto tuple_items = js::Schema::compile(make_document(Json::object({
      {"items", Json::array({Json::object({{"type", "string"}})})},
  })));
  EXPECT_TRUE(!tuple_items.has_value());

  js::CompileOptions openapi_options;
  openapi_options.dialect = js::Dialect::openapi31;
  const auto openapi_schema = js::Schema::compile(make_document(Json::object({
                                                      {"type", Json::array({"string", "null"})},
                                                      {"nullable", true},
                                                  })),
                                                  openapi_options);
  EXPECT_TRUE(openapi_schema.has_value());
  if (openapi_schema) {
    const auto null_instance = make_document(Json(nullptr));
    EXPECT_TRUE(openapi_schema.value().validate(null_instance.root()).ok());
  }
}

TEST(Oascpp, schema_respects_zero_error_limit_without_reporting_success) {
  const js::Schema schema = compile_or_fail(Json::object({{"type", "string"}}));
  const auto instance = make_document(Json(42));
  js::ValidationOptions options;
  options.max_errors = 0;
  const auto result = schema.validate(instance.root(), options);

  EXPECT_TRUE(!result.ok());
  EXPECT_TRUE(result.complete());
  EXPECT_TRUE(result.error_count() == 0);
}

TEST(Oascpp, schema_rejects_empty_enum_and_invalid_compiled_schema) {
  const auto empty_enum = js::Schema::compile(make_document(Json::object({{"enum", Json::array({})}})));
  EXPECT_TRUE(!empty_enum.has_value());

  const js::Schema invalid;
  EXPECT_TRUE(invalid.document().empty());
  EXPECT_TRUE(!invalid.root().valid());
  EXPECT_TRUE(invalid.compiled().document().empty());
  const auto instance = make_document(Json(1));
  const auto result = invalid.validate(instance.root());
  EXPECT_TRUE(!result.ok());
  EXPECT_TRUE(result.first_error_code() == ErrorCode::schema_invalid);
}

TEST(Oascpp, schema_counts_utf8_code_points) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "string"},
      {"minLength", 4},
      {"maxLength", 5},
  }));

  // Five code points, six bytes.
  const auto valid = make_document(Json("h\xC3\xA9llo"));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto too_long = make_document(Json("hello!"));
  EXPECT_TRUE(schema.validate(too_long.root()).first_error_code() == ErrorCode::string_max_length);
}

TEST(Oascpp, schema_checks_array_prefix_and_items) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "array"},
      {"prefixItems", Json::array({Json::object({{"type", "integer"}})})},
      {"items", Json::object({{"type", "string"}})},
      {"minItems", 2},
  }));

  const auto valid = make_document(Json::array({1, "a", "b"}));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto bad_prefix = make_document(Json::array({"a", "b"}));
  const auto prefix_result = schema.validate(bad_prefix.root());
  EXPECT_TRUE(!prefix_result.ok());
  EXPECT_TRUE(prefix_result.first_error()->instance_path.to_string() == "/0");

  const auto too_short = make_document(Json::array({1}));
  EXPECT_TRUE(schema.validate(too_short.root()).first_error_code() == ErrorCode::array_min_items);
}

TEST(Oascpp, schema_rejects_duplicate_items) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"type", "array"},
      {"uniqueItems", true},
  }));

  const auto duplicate = make_document(Json::array({1, 2, 1}));
  EXPECT_TRUE(schema.validate(duplicate.root()).first_error_code() == ErrorCode::array_unique_items);

  // 1 and 1.0 are the same JSON number.
  const auto numeric_duplicate = make_document(Json::array({1, 1.0}));
  EXPECT_TRUE(!schema.validate(numeric_duplicate.root()).ok());
}

TEST(Oascpp, schema_composition_keywords) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"allOf", Json::array({Json::object({{"type", "object"}})})},
      {"anyOf", Json::array({Json::object({{"required", Json::array({"a"})}}),
                             Json::object({{"required", Json::array({"b"})}})})},
      {"not", Json::object({{"required", Json::array({"forbidden"})}})},
  }));

  const auto valid = make_document(Json::object({{"a", 1}}));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto no_branch = make_document(Json::object({{"c", 1}}));
  EXPECT_TRUE(schema.validate(no_branch.root()).first_error_code() == ErrorCode::composition_any_of);

  const auto forbidden = make_document(Json::object({{"a", 1}, {"forbidden", true}}));
  EXPECT_TRUE(schema.validate(forbidden.root()).first_error_code() == ErrorCode::composition_not);
}

TEST(Oascpp, schema_enum_and_const) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"enum", Json::array({"red", "green", 3})},
  }));

  const auto valid = make_document(Json(3));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto invalid = make_document(Json("blue"));
  EXPECT_TRUE(schema.validate(invalid.root()).first_error_code() == ErrorCode::enum_mismatch);

  const js::Schema const_schema = compile_or_fail(Json::object({{"const", "fixed"}}));
  const auto wrong = make_document(Json("other"));
  EXPECT_TRUE(const_schema.validate(wrong.root()).first_error_code() == ErrorCode::const_mismatch);
}

TEST(Oascpp, schema_resolves_recursive_refs) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"$defs",
       Json::object({{"node", Json::object({
                                  {"type", "object"},
                                  {"properties", Json::object({{"next", Json::object({{"$ref", "#/$defs/node"}})}})},
                              })}})},
      {"$ref", "#/$defs/node"},
  }));

  const auto valid = make_document(Json::object({{"next", Json::object({{"next", Json::object({})}})}}));
  EXPECT_TRUE(schema.validate(valid.root()).ok());

  const auto invalid = make_document(Json::object({{"next", Json(3)}}));
  const auto result = schema.validate(invalid.root());
  EXPECT_TRUE(!result.ok());
  EXPECT_TRUE(result.first_error_code() == ErrorCode::type_mismatch);
  EXPECT_TRUE(result.first_error()->instance_path.to_string() == "/next");
}

TEST(Oascpp, schema_pattern_needs_a_matcher) {
  const Json pattern_schema = Json::object({
      {"type", "string"},
      {"pattern", "^a+$"},
  });

  const js::Schema without_matcher = compile_or_fail(pattern_schema);
  const auto instance = make_document(Json("bbb"));
  const auto incomplete = without_matcher.validate(instance.root());
  EXPECT_TRUE(!incomplete.ok());
  EXPECT_TRUE(!incomplete.complete());
  EXPECT_TRUE(incomplete.empty());

  js::ValidationOptions options;
  options.pattern_matcher = [](std::string_view pattern, std::string_view candidate) {
    static_cast<void>(pattern);
    return !candidate.empty() && candidate.front() == 'a' && candidate.find('b') == std::string_view::npos;
  };
  const auto with_matcher = without_matcher.validate(instance.root(), options);
  EXPECT_TRUE(with_matcher.first_error_code() == ErrorCode::string_pattern);
  EXPECT_TRUE(with_matcher.complete());
}

TEST(Oascpp, schema_marks_unavailable_pattern_properties_incomplete) {
  const js::Schema schema = compile_or_fail(Json::object({
      {"patternProperties", Json::object({{"^x", Json(true)}})},
      {"additionalProperties", Json(false)},
  }));
  const auto instance = make_document(Json::object({{"x-value", 1}}));

  const auto without_matcher = schema.validate(instance.root());
  EXPECT_TRUE(!without_matcher.ok());
  EXPECT_TRUE(!without_matcher.complete());
  EXPECT_TRUE(without_matcher.empty());

  js::ValidationOptions options;
  options.pattern_matcher = [](std::string_view pattern, std::string_view candidate) {
    return pattern == "^x" && !candidate.empty() && candidate.front() == 'x';
  };
  const auto with_matcher = schema.validate(instance.root(), options);
  EXPECT_TRUE(with_matcher.ok());
  EXPECT_TRUE(with_matcher.complete());
}

TEST(Oascpp, schema_marks_unimplemented_unevaluated_and_dynamic_keywords_incomplete) {
  const js::Schema properties_schema = compile_or_fail(Json::object({{"unevaluatedProperties", false}}));
  const auto object = make_document(Json::object({{"extra", 1}}));
  const auto properties_result = properties_schema.validate(object.root());
  EXPECT_TRUE(!properties_result.ok());
  EXPECT_TRUE(!properties_result.complete());
  EXPECT_TRUE(properties_result.empty());

  const js::Schema items_schema = compile_or_fail(Json::object({{"unevaluatedItems", false}}));
  const auto array = make_document(Json::array({1}));
  const auto items_result = items_schema.validate(array.root());
  EXPECT_TRUE(!items_result.ok());
  EXPECT_TRUE(!items_result.complete());
  EXPECT_TRUE(items_result.empty());

  const js::Schema dynamic_schema = compile_or_fail(Json::object({
      {"$defs", Json::object({{"node", Json::object({
                                           {"$dynamicAnchor", "node"},
                                           {"type", "string"},
                                       })}})},
      {"$dynamicRef", "#node"},
  }));
  const auto string = make_document(Json("value"));
  const auto dynamic_result = dynamic_schema.validate(string.root());
  EXPECT_TRUE(!dynamic_result.ok());
  EXPECT_TRUE(!dynamic_result.complete());
}

TEST(Oascpp, schema_compile_rejects_bad_input) {
  const auto unknown_type = js::Schema::compile(make_document(Json::object({{"type", "nope"}})));
  EXPECT_TRUE(!unknown_type.has_value());
  EXPECT_TRUE(unknown_type.error().code == ErrorCode::schema_invalid);

  const auto bad_reference = js::Schema::compile(make_document(Json::object({{"$ref", "#/$defs/missing"}})));
  EXPECT_TRUE(!bad_reference.has_value());
  EXPECT_TRUE(bad_reference.error().code == ErrorCode::unresolved_reference);

  js::CompileOptions options;
  options.dialect = js::Dialect::draft4;
  const auto unsupported = js::Schema::compile(make_document(Json::object({{"type", "string"}})), options);
  EXPECT_TRUE(!unsupported.has_value());
  EXPECT_TRUE(unsupported.error().code == ErrorCode::unsupported_dialect);
}

TEST(Oascpp, schema_boolean_schemas) {
  const js::Schema accept = compile_or_fail(Json(true));
  const auto instance = make_document(Json(1));
  EXPECT_TRUE(accept.validate(instance.root()).ok());

  const js::Schema reject = compile_or_fail(Json(false));
  EXPECT_TRUE(!reject.validate(instance.root()).ok());
}

}  // namespace
