#include <cstddef>
#include <cstdint>
#include <oascpp/document_builder.hpp>
#include <oascpp/value.hpp>
#include <stdexcept>
#include <string>
#include <string_view>

#include "json_dsl.hpp"
#include "test_framework.hpp"

namespace {

using oascpp::DocumentBuilder;
using oascpp::ParsedDocument;
using oascpp::ValueKind;
using oascpp::ValueView;

// Children of "tags" are created between the root members on purpose: child
// indices must not depend on nodes being laid out in document order.
ParsedDocument make_sample_document() {
  DocumentBuilder builder;

  const DocumentBuilder::NodeId root = builder.make_object();

  const DocumentBuilder::NodeId name = builder.make_string("oascpp");
  builder.append_member(root, "name", name);

  const DocumentBuilder::NodeId count = builder.make_integer(std::int64_t{3});
  builder.append_member(root, "count", count);

  const DocumentBuilder::NodeId tags = builder.make_array();
  const DocumentBuilder::NodeId first = builder.make_string("a");
  const DocumentBuilder::NodeId second = builder.make_string("b");
  builder.append(tags, first);
  builder.append(tags, second);
  builder.append_member(root, "tags", tags);

  const DocumentBuilder::NodeId ratio = builder.make_real(0.5);
  builder.append_member(root, "ratio", ratio);

  const DocumentBuilder::NodeId enabled = builder.make_boolean(true);
  builder.append_member(root, "enabled", enabled);

  const DocumentBuilder::NodeId nothing = builder.make_null();
  builder.append_member(root, "nothing", nothing);

  return builder.finish();
}

}  // namespace

OASCPP_TEST(value_root_exposes_the_first_node) {
  const ParsedDocument document = make_sample_document();
  const ValueView root = document.root();

  OASCPP_CHECK(root.valid());
  OASCPP_CHECK(root.is_object());
  OASCPP_CHECK(root.kind() == ValueKind::object);
  OASCPP_CHECK_EQ(root.size(), std::size_t{6});
  OASCPP_CHECK(!root.empty());
  OASCPP_CHECK_EQ(document.node_count(), std::size_t{9});
}

OASCPP_TEST(value_reads_scalars) {
  const ParsedDocument document = make_sample_document();
  const ValueView root = document.root();

  OASCPP_CHECK_EQ(root.find("name").as_string(), std::string_view("oascpp"));
  OASCPP_CHECK_EQ(root.find("count").as_integer(), std::int64_t{3});
  OASCPP_CHECK(root.find("ratio").as_double() == 0.5);
  OASCPP_CHECK(root.find("enabled").as_boolean());
  OASCPP_CHECK(root.find("nothing").is_null());
  OASCPP_CHECK(root.contains("name"));
  OASCPP_CHECK(!root.contains("absent"));
  OASCPP_CHECK(!root.find("absent").valid());
}

OASCPP_TEST(value_iterates_members_in_append_order) {
  const ParsedDocument document = make_sample_document();
  const ValueView root = document.root();

  std::string keys;
  std::size_t members = 0;
  for (const ValueView member : root) {
    keys.append(member.key());
    keys.push_back(',');
    ++members;
  }

  OASCPP_CHECK_EQ(members, std::size_t{6});
  OASCPP_CHECK_EQ(keys, std::string("name,count,tags,ratio,enabled,nothing,"));
}

OASCPP_TEST(value_reads_array_elements) {
  const ParsedDocument document = make_sample_document();
  const ValueView tags = document.root().find("tags");

  OASCPP_CHECK(tags.is_array());
  OASCPP_CHECK_EQ(tags.size(), std::size_t{2});
  OASCPP_CHECK_EQ(tags.at(0).as_string(), std::string_view("a"));
  OASCPP_CHECK_EQ(tags.at(1).as_string(), std::string_view("b"));
  OASCPP_CHECK(!tags.at(2).valid());

  std::string joined;
  for (const ValueView element : tags) {
    joined.append(element.as_string());
  }
  OASCPP_CHECK_EQ(joined, std::string("ab"));
}

OASCPP_TEST(value_invalid_views_are_safe) {
  const ValueView missing;

  OASCPP_CHECK(!missing.valid());
  OASCPP_CHECK(missing.kind() == ValueKind::null_value);
  OASCPP_CHECK_EQ(missing.size(), std::size_t{0});
  OASCPP_CHECK_EQ(missing.as_string(), std::string_view());
  OASCPP_CHECK(!missing.find("x").valid());
  OASCPP_CHECK(missing.begin() == missing.end());

  const ParsedDocument document = make_sample_document();
  const ValueView name = document.root().find("name");
  OASCPP_CHECK(!name.at(0).valid());
  OASCPP_CHECK_EQ(name.size(), std::size_t{0});
}

OASCPP_TEST(value_empty_document_has_no_root) {
  const ParsedDocument document;

  OASCPP_CHECK(document.empty());
  OASCPP_CHECK_EQ(document.node_count(), std::size_t{0});
  OASCPP_CHECK(!document.root().valid());
}

OASCPP_TEST(value_keeps_number_precision) {
  DocumentBuilder builder;
  const DocumentBuilder::NodeId array = builder.make_array();

  const DocumentBuilder::NodeId negative = builder.make_integer(std::int64_t{-7});
  builder.append(array, negative);

  const DocumentBuilder::NodeId wide = builder.make_unsigned(std::uint64_t{18446744073709551615ULL});
  builder.append(array, wide);

  const ParsedDocument document = builder.finish();
  const ValueView root = document.root();

  const oascpp::NumberView signed_number = root.at(0).as_number();
  OASCPP_CHECK(signed_number.is_integer);
  OASCPP_CHECK(!signed_number.is_unsigned);
  OASCPP_CHECK_EQ(signed_number.as_signed, std::int64_t{-7});

  const oascpp::NumberView wide_number = root.at(1).as_number();
  OASCPP_CHECK(wide_number.is_integer);
  OASCPP_CHECK(wide_number.is_unsigned);
  OASCPP_CHECK_EQ(wide_number.as_unsigned, std::uint64_t{18446744073709551615ULL});
  OASCPP_CHECK_EQ(wide_number.as_signed, std::int64_t{0});
}

OASCPP_TEST(value_stores_empty_keys_and_strings) {
  DocumentBuilder builder;
  const DocumentBuilder::NodeId root = builder.make_object();
  const DocumentBuilder::NodeId empty = builder.make_string("");
  builder.append_member(root, "", empty);

  const ParsedDocument document = builder.finish();
  const ValueView member = document.root().find("");

  OASCPP_CHECK(member.valid());
  OASCPP_CHECK_EQ(member.as_string(), std::string_view());
  OASCPP_CHECK_EQ(member.key(), std::string_view());
  OASCPP_CHECK_EQ(document.root().size(), std::size_t{1});
}

OASCPP_TEST(value_compares_by_identity) {
  const ParsedDocument document = make_sample_document();
  const ValueView root = document.root();

  OASCPP_CHECK(root == document.root());
  OASCPP_CHECK(root != root.find("name"));
  OASCPP_CHECK(ValueView() == ValueView());
}

OASCPP_TEST(value_compares_large_numbers_without_rounding_integers) {
  const ParsedDocument integer = make_document(oascpp_test::Json(9007199254740993LL));
  const ParsedDocument rounded_real = make_document(oascpp_test::Json(9007199254740992.0));

  OASCPP_CHECK(!oascpp::deep_equal(integer.root(), rounded_real.root()));
}

OASCPP_TEST(document_builder_rejects_reused_nodes_and_cycles) {
  DocumentBuilder builder;
  const auto root = builder.make_array();
  const auto object = builder.make_object();
  const auto nested = builder.make_array();

  bool wrong_parent_rejected = false;
  try {
    builder.append_member(root, "bad", object);
  } catch (const std::invalid_argument&) {
    wrong_parent_rejected = true;
  }
  OASCPP_CHECK(wrong_parent_rejected);

  builder.append(root, object);
  bool reused_node_rejected = false;
  try {
    builder.append(root, object);
  } catch (const std::logic_error&) {
    reused_node_rejected = true;
  }
  OASCPP_CHECK(reused_node_rejected);

  builder.append_member(object, "nested", nested);
  bool cycle_rejected = false;
  try {
    builder.append(nested, root);
  } catch (const std::logic_error&) {
    cycle_rejected = true;
  }
  OASCPP_CHECK(cycle_rejected);
}
