#pragma once

// Compiled schema representation: structure-of-arrays, no per-schema allocation
// beyond the pools owned by CompiledSchema. The schema document itself is kept
// alive by the caller and referenced by pointer, so compiling never copies the
// JSON tree.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "oascpp/schema/jsonschema/dialect.hpp"
#include "oascpp/value.hpp"

namespace oascpp {
namespace jsonschema {

using SchemaId = std::uint32_t;

inline constexpr SchemaId no_schema = static_cast<SchemaId>(-1);

// One bit per keyword the interpreter understands. Keywords that only produce
// annotations are not tracked here yet.
enum class Keyword : std::uint8_t {
  type = 0,
  enum_values,
  const_value,
  multiple_of,
  maximum,
  exclusive_maximum,
  minimum,
  exclusive_minimum,
  max_length,
  min_length,
  pattern,
  format,
  max_items,
  min_items,
  unique_items,
  max_contains,
  min_contains,
  items,
  prefix_items,
  contains,
  unevaluated_items,
  max_properties,
  min_properties,
  required,
  dependent_required,
  dependent_schemas,
  properties,
  pattern_properties,
  additional_properties,
  property_names,
  unevaluated_properties,
  all_of,
  any_of,
  one_of,
  not_schema,
  if_schema,
  then_schema,
  else_schema,
  ref,
  dynamic_ref,
  count,
};

inline constexpr std::uint64_t keyword_bit(Keyword keyword) noexcept {
  return std::uint64_t{1} << static_cast<unsigned>(keyword);
}

[[nodiscard]] constexpr bool has_keyword(std::uint64_t mask, Keyword keyword) noexcept {
  return (mask & keyword_bit(keyword)) != 0;
}

// Instance type bits. A node with types == 0 constrains nothing.
enum TypeBit : std::uint8_t {
  type_bit_null = 1u << 0,
  type_bit_boolean = 1u << 1,
  type_bit_object = 1u << 2,
  type_bit_array = 1u << 3,
  type_bit_number = 1u << 4,
  type_bit_string = 1u << 5,
  type_bit_integer = 1u << 6,
};

// Per-node flags.
enum SchemaFlags : std::uint8_t {
  schema_flag_none = 0,
  // The boolean schema `false`: every instance is rejected.
  schema_flag_false = 1u << 0,
};

// A slice into one of CompiledSchema's pools.
struct SchemaRun {
  std::uint32_t off = 0;
  std::uint32_t count = 0;
};

// Object member plus the schema that applies to its value.
struct MemberEntry {
  std::uint32_t key_off = 0;
  std::uint32_t key_len = 0;
  SchemaId schema = no_schema;
};

// Property name without a schema, used by `required` and `dependentRequired`.
struct NameEntry {
  std::uint32_t off = 0;
  std::uint32_t len = 0;
};

// One entry of `dependentRequired` (run of names) or `dependentSchemas` (run of
// one schema).
struct DependentEntry {
  std::uint32_t key_off = 0;
  std::uint32_t key_len = 0;
  SchemaRun run;
};

struct SchemaNode {
  std::uint64_t present = 0;
  std::uint8_t types = 0;
  std::uint8_t flags = 0;

  // Index of the node this schema was compiled from, in the schema document.
  std::uint32_t source = 0;

  SchemaId ref = no_schema;
  SchemaId dynamic_ref = no_schema;
  SchemaId not_schema = no_schema;
  SchemaId if_schema = no_schema;
  SchemaId then_schema = no_schema;
  SchemaId else_schema = no_schema;
  SchemaId items = no_schema;
  SchemaId contains = no_schema;
  SchemaId additional_properties = no_schema;
  SchemaId property_names = no_schema;
  SchemaId unevaluated_properties = no_schema;
  SchemaId unevaluated_items = no_schema;

  double minimum = 0.0;
  double maximum = 0.0;
  double exclusive_minimum = 0.0;
  double exclusive_maximum = 0.0;
  double multiple_of = 0.0;

  SchemaRun all_of;
  SchemaRun any_of;
  SchemaRun one_of;
  SchemaRun prefix_items;
  SchemaRun properties;
  SchemaRun pattern_properties;
  SchemaRun required;
  SchemaRun dependent_required;
  SchemaRun dependent_schemas;
  SchemaRun enum_values;

  std::uint32_t const_value = 0;
  std::uint32_t min_length = 0;
  std::uint32_t max_length = 0;
  std::uint32_t min_items = 0;
  std::uint32_t max_items = 0;
  std::uint32_t min_properties = 0;
  std::uint32_t max_properties = 0;
  std::uint32_t min_contains = 0;
  std::uint32_t max_contains = 0;

  std::uint32_t pattern_off = 0;
  std::uint32_t pattern_len = 0;
  std::uint32_t format_off = 0;
  std::uint32_t format_len = 0;
};

// A compiled schema graph. It borrows the source document: the document must
// outlive both the schema and every validation run that uses it.
class CompiledSchema {
 public:
  CompiledSchema() = default;

  [[nodiscard]] bool valid() const noexcept { return root_ != no_schema; }
  [[nodiscard]] SchemaId root() const noexcept { return root_; }
  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }
  [[nodiscard]] const DialectProfile& profile() const noexcept { return profile_; }

  [[nodiscard]] const SchemaNode& node(SchemaId id) const noexcept { return nodes_[id]; }

  [[nodiscard]] const ParsedDocument& document() const noexcept { return *document_; }

  [[nodiscard]] std::string_view text(std::uint32_t off, std::uint32_t len) const noexcept {
    return std::string_view(text_.data() + off, static_cast<std::size_t>(len));
  }

  [[nodiscard]] std::string_view key(const MemberEntry& entry) const noexcept {
    return text(entry.key_off, entry.key_len);
  }

  [[nodiscard]] std::string_view name(const NameEntry& entry) const noexcept { return text(entry.off, entry.len); }

  [[nodiscard]] SchemaId subschema_at(const SchemaRun& run, std::uint32_t index) const noexcept {
    return subschemas_[static_cast<std::size_t>(run.off) + index];
  }

  [[nodiscard]] const MemberEntry& member_at(const SchemaRun& run, std::uint32_t index) const noexcept {
    return members_[static_cast<std::size_t>(run.off) + index];
  }

  [[nodiscard]] const NameEntry& name_at(const SchemaRun& run, std::uint32_t index) const noexcept {
    return names_[static_cast<std::size_t>(run.off) + index];
  }

  [[nodiscard]] const DependentEntry& dependent_at(const SchemaRun& run, std::uint32_t index) const noexcept {
    return dependents_[static_cast<std::size_t>(run.off) + index];
  }

  // enum and const values are materialised views into the schema document.
  [[nodiscard]] ValueView value_at(const SchemaRun& run, std::uint32_t index) const noexcept {
    return ValueView(document_, values_[static_cast<std::size_t>(run.off) + index]);
  }

  [[nodiscard]] ValueView const_value(const SchemaNode& node) const noexcept {
    return ValueView(document_, node.const_value);
  }

 private:
  friend class SchemaCompiler;

  const ParsedDocument* document_ = nullptr;
  SchemaId root_ = no_schema;
  DialectProfile profile_;

  std::vector<SchemaNode> nodes_;
  std::vector<SchemaId> subschemas_;
  std::vector<MemberEntry> members_;
  std::vector<NameEntry> names_;
  std::vector<DependentEntry> dependents_;
  std::vector<std::uint32_t> values_;
  std::vector<char> text_;
};

}  // namespace jsonschema
}  // namespace oascpp
