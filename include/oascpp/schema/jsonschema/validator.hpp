#pragma once

// Interpreter for the compiled schema graph. The validator walks the instance
// tree instead of compiling a program, which keeps annotation-carrying keywords
// (unevaluated*, dynamic references) expressible; those are the reason the byte
// code tier is deferred rather than adopted up front.
//
// Two keywords depend on an engine this library deliberately does not ship:
// `pattern` and `format`. Both are exposed as callables in ValidationOptions so
// the core stays free of <regex> and of locale-sensitive parsing.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "oascpp/document_builder.hpp"
#include "oascpp/error.hpp"
#include "oascpp/schema/jsonschema/compiled_schema.hpp"
#include "oascpp/value.hpp"

namespace oascpp {
namespace jsonschema {

// (pattern, candidate) -> match. An empty matcher means pattern constraints are
// skipped, which is reported by CompiledSchema introspection rather than hidden.
using PatternMatcher = std::function<bool(std::string_view, std::string_view)>;

// (format name, candidate) -> valid. An empty checker means formats are skipped.
using FormatChecker = std::function<bool(std::string_view, std::string_view)>;

struct ValidationOptions {
  std::size_t max_depth = 512;
  std::size_t max_errors = 100;
  PatternMatcher pattern_matcher;
  FormatChecker format_checker;
};

namespace detail {

// JSON Schema lengths count code points, not bytes.
[[nodiscard]] inline std::size_t utf8_length(std::string_view text) noexcept {
  std::size_t count = 0;
  for (const char character : text) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if ((byte & 0xC0u) != 0x80u) {
      ++count;
    }
  }
  return count;
}

class ValidationRun {
 public:
  ValidationRun(const CompiledSchema& schema, const ValidationOptions& options) : schema_(schema), options_(options) {}

  ValidationResult execute(ValueView instance) {
    if (!instance.valid()) {
      ValidationError error;
      error.code = ErrorCode::invalid_value;
      error.message = "instance is empty";
      result_.add_error(std::move(error));
      return std::move(result_);
    }
    ErrorPath path;
    check(schema_.root(), instance, &path);
    return std::move(result_);
  }

 private:
  bool check(SchemaId id, ValueView instance, ErrorPath* path) {
    if (id == no_schema || !instance.valid()) {
      return true;
    }
    if (depth_ >= options_.max_depth) {
      add(path, ErrorCode::schema_invalid, "validation nesting is too deep");
      return false;
    }

    // Recursive references can revisit the same schema for the same instance;
    // the second visit cannot learn anything new.
    const std::pair<SchemaId, std::uint32_t> visit(id, instance.index());
    for (const auto& active : active_) {
      if (active == visit) {
        return true;
      }
    }
    active_.push_back(visit);
    ++depth_;

    const SchemaNode& node = schema_.node(id);
    bool ok = true;
    if ((node.flags & schema_flag_false) != 0) {
      add(path, ErrorCode::invalid_value, "schema is false");
      ok = false;
    } else {
      ok = check_node(node, instance, path);
    }

    --depth_;
    active_.pop_back();
    return ok;
  }

  bool check_node(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    bool ok = true;

    if (has_keyword(node.present, Keyword::ref)) {
      ok = check(node.ref, instance, path) && ok;
      if (!schema_.profile().ref_siblings_apply) {
        return ok;
      }
    }
    if (has_keyword(node.present, Keyword::dynamic_ref)) {
      ok = check(node.dynamic_ref, instance, path) && ok;
    }

    if (node.types != 0 && !type_matches(node.types, instance)) {
      add(path, ErrorCode::type_mismatch, "type does not match");
      return false;
    }

    ok = check_number(node, instance, path) && ok;
    ok = check_string(node, instance, path) && ok;
    ok = check_array(node, instance, path) && ok;
    ok = check_object(node, instance, path) && ok;
    ok = check_composition(node, instance, path) && ok;
    ok = check_values(node, instance, path) && ok;
    return ok;
  }

  bool type_matches(std::uint8_t types, ValueView instance) const {
    switch (instance.kind()) {
      case ValueKind::null_value:
        return (types & type_bit_null) != 0;
      case ValueKind::boolean:
        return (types & type_bit_boolean) != 0;
      case ValueKind::object:
        return (types & type_bit_object) != 0;
      case ValueKind::array:
        return (types & type_bit_array) != 0;
      case ValueKind::string:
        return (types & type_bit_string) != 0;
      case ValueKind::integer:
        return (types & (type_bit_integer | type_bit_number)) != 0;
      case ValueKind::real: {
        if ((types & type_bit_number) != 0) {
          return true;
        }
        const double value = instance.as_double();
        return (types & type_bit_integer) != 0 && std::isfinite(value) && std::floor(value) == value;
      }
    }
    return false;
  }

  bool check_number(const SchemaNode& node, ValueView instance, ErrorPath* path);

  bool check_string(const SchemaNode& node, ValueView instance, ErrorPath* path);

  bool check_array(const SchemaNode& node, ValueView instance, ErrorPath* path);

  bool check_object(const SchemaNode& node, ValueView instance, ErrorPath* path);

  bool check_composition(const SchemaNode& node, ValueView instance, ErrorPath* path);

  bool check_values(const SchemaNode& node, ValueView instance, ErrorPath* path);

  // --- helpers ------------------------------------------------------------

  [[nodiscard]] SchemaId find_property(const SchemaRun& run, std::string_view key) const;

  bool check_member(SchemaId id, ValueView value, std::string_view name, ErrorPath* path);

  bool check_element(SchemaId id, ValueView value, std::size_t index, ErrorPath* path);

  // propertyNames validates a bare string, which has no node in the instance
  // document; a scratch document keeps the interpreter single-path instead of
  // growing a second string-only validation route.
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

class Validator {
 public:
  explicit Validator(const CompiledSchema& schema, ValidationOptions options = ValidationOptions())
      : schema_(schema), options_(std::move(options)) {}

  [[nodiscard]] ValidationResult validate(ValueView instance) const {
    detail::ValidationRun run(schema_, options_);
    return run.execute(instance);
  }

 private:
  const CompiledSchema& schema_;
  ValidationOptions options_;
};

[[nodiscard]] inline ValidationResult validate(const CompiledSchema& schema, ValueView instance,
                                               ValidationOptions options = ValidationOptions()) {
  Validator validator(schema, std::move(options));
  return validator.validate(instance);
}

}  // namespace jsonschema
}  // namespace oascpp

#include "oascpp/schema/jsonschema/detail/validator_composition.hpp"
#include "oascpp/schema/jsonschema/detail/validator_helpers.hpp"
#include "oascpp/schema/jsonschema/detail/validator_number_string_array.hpp"
#include "oascpp/schema/jsonschema/detail/validator_object.hpp"
