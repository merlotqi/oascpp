#include <cstddef>
#include <cstdint>
#include <exception>
#include <oascpp/adapters/nlohmann_json.hpp>
#include <string>
#include <string_view>

#include "test_framework.hpp"

#if OASCPP_HAS_NLOHMANN_JSON

namespace {

const char* const k_document = R"({
  "name": "oascpp",
  "count": 7,
  "tags": ["a", "b"],
  "meta": {"draft": 3.1, "stable": false, "extra": null}
})";

}  // namespace

OASCPP_TEST(nlohmann_parses_a_nested_document) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse(k_document);

  const oascpp::ValueView root = document.root();
  OASCPP_CHECK(root.is_object());
  OASCPP_CHECK_EQ(root.size(), std::size_t{4});
  OASCPP_CHECK_EQ(root.find("name").as_string(), std::string_view("oascpp"));
  OASCPP_CHECK_EQ(root.find("count").as_integer(), std::int64_t{7});
  OASCPP_CHECK_EQ(root.find("tags").size(), std::size_t{2});
  OASCPP_CHECK_EQ(root.find("tags").at(1).as_string(), std::string_view("b"));
  OASCPP_CHECK(root.find("meta").find("draft").as_double() == 3.1);
  OASCPP_CHECK(!root.find("meta").find("stable").as_boolean());
  OASCPP_CHECK(root.find("meta").find("extra").is_null());
}

OASCPP_TEST(nlohmann_keeps_array_order) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse(R"([3, 1, 2])");

  const oascpp::ValueView root = document.root();
  OASCPP_CHECK(root.is_array());
  OASCPP_CHECK_EQ(root.at(0).as_integer(), std::int64_t{3});
  OASCPP_CHECK_EQ(root.at(1).as_integer(), std::int64_t{1});
  OASCPP_CHECK_EQ(root.at(2).as_integer(), std::int64_t{2});
}

OASCPP_TEST(nlohmann_parses_scalars_at_the_root) {
  OASCPP_CHECK(oascpp::adapters::nlohmann_json::parse("null").root().is_null());
  OASCPP_CHECK(oascpp::adapters::nlohmann_json::parse("true").root().as_boolean());
  OASCPP_CHECK_EQ(oascpp::adapters::nlohmann_json::parse("\"text\"").root().as_string(), std::string_view("text"));
  OASCPP_CHECK(oascpp::adapters::nlohmann_json::parse("-1.5").root().is_real());
}

OASCPP_TEST(nlohmann_reports_parse_errors) {
  const oascpp::Result<oascpp::ParsedDocument> result = oascpp::adapters::nlohmann_json::try_parse("{not json");

  OASCPP_CHECK(!result.has_value());
  OASCPP_CHECK(result.error().code == oascpp::ErrorCode::parse_error);
  OASCPP_CHECK(!result.error().message.empty());
}

OASCPP_TEST(nlohmann_parse_throws_on_invalid_json) {
  bool threw = false;
  try {
    const oascpp::ParsedDocument parsed = oascpp::adapters::nlohmann_json::parse("{");
    static_cast<void>(parsed);
  } catch (const std::exception&) {
    threw = true;
  }
  OASCPP_CHECK(threw);
}

OASCPP_TEST(nlohmann_parses_an_empty_document_into_a_root) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse("{}");
  OASCPP_CHECK_EQ(document.node_count(), std::size_t{1});
  OASCPP_CHECK(document.root().is_object());
  OASCPP_CHECK(document.root().empty());
}

#else

OASCPP_TEST(nlohmann_adapter_is_compiled_out) {
  // nlohmann/json was not found on the include path, so the adapter is empty.
  OASCPP_CHECK(true);
}

#endif  // OASCPP_HAS_NLOHMANN_JSON
