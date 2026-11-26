#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <oascpp/adapters/nlohmann_json.hpp>
#include <string>
#include <string_view>

#if OASCPP_HAS_NLOHMANN_JSON

namespace {

const char* const k_document = R"({
  "name": "oascpp",
  "count": 7,
  "tags": ["a", "b"],
  "meta": {"draft": 3.1, "stable": false, "extra": null}
})";

}  // namespace

TEST(Oascpp, nlohmann_parses_a_nested_document) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse(k_document);

  const oascpp::ValueView root = document.root();
  EXPECT_TRUE(root.is_object());
  EXPECT_EQ(root.size(), std::size_t{4});
  EXPECT_EQ(root.find("name").as_string(), std::string_view("oascpp"));
  EXPECT_EQ(root.find("count").as_integer(), std::int64_t{7});
  EXPECT_EQ(root.find("tags").size(), std::size_t{2});
  EXPECT_EQ(root.find("tags").at(1).as_string(), std::string_view("b"));
  EXPECT_TRUE(root.find("meta").find("draft").as_double() == 3.1);
  EXPECT_TRUE(!root.find("meta").find("stable").as_boolean());
  EXPECT_TRUE(root.find("meta").find("extra").is_null());
}

TEST(Oascpp, nlohmann_keeps_array_order) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse(R"([3, 1, 2])");

  const oascpp::ValueView root = document.root();
  EXPECT_TRUE(root.is_array());
  EXPECT_EQ(root.at(0).as_integer(), std::int64_t{3});
  EXPECT_EQ(root.at(1).as_integer(), std::int64_t{1});
  EXPECT_EQ(root.at(2).as_integer(), std::int64_t{2});
}

TEST(Oascpp, nlohmann_parses_scalars_at_the_root) {
  EXPECT_TRUE(oascpp::adapters::nlohmann_json::parse("null").root().is_null());
  EXPECT_TRUE(oascpp::adapters::nlohmann_json::parse("true").root().as_boolean());
  EXPECT_EQ(oascpp::adapters::nlohmann_json::parse("\"text\"").root().as_string(), std::string_view("text"));
  EXPECT_TRUE(oascpp::adapters::nlohmann_json::parse("-1.5").root().is_real());
}

TEST(Oascpp, nlohmann_reports_parse_errors) {
  const oascpp::Result<oascpp::ParsedDocument> result = oascpp::adapters::nlohmann_json::try_parse("{not json");

  EXPECT_TRUE(!result.has_value());
  EXPECT_TRUE(result.error().code == oascpp::ErrorCode::parse_error);
  EXPECT_TRUE(!result.error().message.empty());
}

TEST(Oascpp, nlohmann_parse_throws_on_invalid_json) {
  bool threw = false;
  try {
    const oascpp::ParsedDocument parsed = oascpp::adapters::nlohmann_json::parse("{");
    static_cast<void>(parsed);
  } catch (const std::exception&) {
    threw = true;
  }
  EXPECT_TRUE(threw);
}

TEST(Oascpp, nlohmann_parses_an_empty_document_into_a_root) {
  const oascpp::ParsedDocument document = oascpp::adapters::nlohmann_json::parse("{}");
  EXPECT_EQ(document.node_count(), std::size_t{1});
  EXPECT_TRUE(document.root().is_object());
  EXPECT_TRUE(document.root().empty());
}

#else

TEST(Oascpp, nlohmann_adapter_is_compiled_out) {
  // nlohmann/json was not found on the include path, so the adapter is empty.
  EXPECT_TRUE(true);
}

#endif  // OASCPP_HAS_NLOHMANN_JSON
