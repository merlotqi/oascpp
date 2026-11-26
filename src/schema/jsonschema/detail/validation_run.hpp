#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <oascpp/schema/jsonschema/validator.hpp>
#include <utility>
#include <vector>

namespace oascpp {
namespace jsonschema {
namespace detail {

[[nodiscard]] std::size_t utf8_length(std::string_view text) noexcept;

class ValidationRun {
 public:
  ValidationRun(const CompiledSchema& schema, const ValidationOptions& options);
  ValidationResult execute(ValueView instance);

 private:
  bool check(SchemaId id, ValueView instance, ErrorPath* path);
  bool check_node(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool type_matches(std::uint8_t types, ValueView instance) const;
  bool check_number(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool check_string(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool check_array(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool check_object(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool check_composition(const SchemaNode& node, ValueView instance, ErrorPath* path);
  bool check_values(const SchemaNode& node, ValueView instance, ErrorPath* path);

  [[nodiscard]] SchemaId find_property(const SchemaRun& run, std::string_view key) const;
  bool check_member(SchemaId id, ValueView value, std::string_view name, ErrorPath* path);
  bool check_element(SchemaId id, ValueView value, std::size_t index, ErrorPath* path);
  [[nodiscard]] ValueView make_string_view(std::string_view text);
  [[nodiscard]] bool is_multiple_of(double value, double divisor) const;
  void add(ErrorPath* path, ErrorCode code, const char* message);

  const CompiledSchema& schema_;
  ValidationOptions options_;
  ValidationResult result_;
  std::vector<std::pair<SchemaId, std::uint32_t>> active_;
  std::vector<std::unique_ptr<ParsedDocument>> scratch_;
  std::size_t depth_ = 0;
};

}  // namespace detail
}  // namespace jsonschema
}  // namespace oascpp
