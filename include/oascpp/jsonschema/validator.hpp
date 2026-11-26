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
#include "oascpp/jsonschema/schema.hpp"
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
  ValidationRun(const CompiledSchema& schema, const ValidationOptions& options)
      : schema_(schema), options_(options) {}

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
        return (types & type_bit_integer) != 0 && std::isfinite(value) &&
               std::floor(value) == value;
      }
    }
    return false;
  }

  bool check_number(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    if (!instance.is_number()) {
      return true;
    }
    const double value = instance.as_double();
    bool ok = true;
    if (has_keyword(node.present, Keyword::minimum) && value < node.minimum) {
      add(path, ErrorCode::numeric_minimum, "value is below the minimum");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::maximum) && value > node.maximum) {
      add(path, ErrorCode::numeric_maximum, "value is above the maximum");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::exclusive_minimum) &&
        !(value > node.exclusive_minimum)) {
      add(path, ErrorCode::numeric_exclusive_minimum,
          "value is not above the exclusive minimum");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::exclusive_maximum) &&
        !(value < node.exclusive_maximum)) {
      add(path, ErrorCode::numeric_exclusive_maximum,
          "value is not below the exclusive maximum");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::multiple_of) &&
        !is_multiple_of(value, node.multiple_of)) {
      add(path, ErrorCode::numeric_multiple_of, "value is not a multiple");
      ok = false;
    }
    return ok;
  }

  bool check_string(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    if (!instance.is_string()) {
      return true;
    }
    const std::string_view value = instance.as_string();
    bool ok = true;
    if (has_keyword(node.present, Keyword::min_length) ||
        has_keyword(node.present, Keyword::max_length)) {
      const std::size_t length = utf8_length(value);
      if (has_keyword(node.present, Keyword::min_length) && length < node.min_length) {
        add(path, ErrorCode::string_min_length, "string is too short");
        ok = false;
      }
      if (has_keyword(node.present, Keyword::max_length) && length > node.max_length) {
        add(path, ErrorCode::string_max_length, "string is too long");
        ok = false;
      }
    }
    if (has_keyword(node.present, Keyword::pattern)) {
      if (options_.pattern_matcher) {
        if (!options_.pattern_matcher(schema_.text(node.pattern_off, node.pattern_len), value)) {
          add(path, ErrorCode::string_pattern, "string does not match the pattern");
          ok = false;
        }
      }
    }
    if (has_keyword(node.present, Keyword::format)) {
      if (options_.format_checker) {
        if (!options_.format_checker(schema_.text(node.format_off, node.format_len), value)) {
          add(path, ErrorCode::string_format, "string does not match the format");
          ok = false;
        }
      }
    }
    return ok;
  }

  bool check_array(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    if (!instance.is_array()) {
      return true;
    }
    const std::size_t size = instance.size();
    bool ok = true;
    if (has_keyword(node.present, Keyword::min_items) && size < node.min_items) {
      add(path, ErrorCode::array_min_items, "array has too few items");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::max_items) && size > node.max_items) {
      add(path, ErrorCode::array_max_items, "array has too many items");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::unique_items)) {
      for (std::size_t left = 0; left < size && ok; ++left) {
        for (std::size_t right = left + 1; right < size; ++right) {
          if (deep_equal(instance.at(left), instance.at(right))) {
            add(path, ErrorCode::array_unique_items, "array items are not unique");
            ok = false;
            break;
          }
        }
      }
    }

    const std::uint32_t prefix_count =
        has_keyword(node.present, Keyword::prefix_items) ? node.prefix_items.count : 0;
    if (has_keyword(node.present, Keyword::prefix_items)) {
      const std::size_t limit = std::min<std::size_t>(prefix_count, size);
      for (std::size_t index = 0; index < limit; ++index) {
        ok = check_element(schema_.subschema_at(node.prefix_items, static_cast<std::uint32_t>(index)),
                           instance.at(index), index, path) &&
             ok;
      }
    }
    if (has_keyword(node.present, Keyword::items)) {
      for (std::size_t index = prefix_count; index < size; ++index) {
        ok = check_element(node.items, instance.at(index), index, path) && ok;
      }
    }
    if (has_keyword(node.present, Keyword::contains)) {
      std::size_t matches = 0;
      for (std::size_t index = 0; index < size; ++index) {
        if (check(node.contains, instance.at(index), nullptr)) {
          ++matches;
        }
      }
      const std::size_t minimum =
          has_keyword(node.present, Keyword::min_contains) ? node.min_contains : 1;
      if (matches < minimum) {
        add(path, ErrorCode::array_contains, "array contains too few matching items");
        ok = false;
      }
      if (has_keyword(node.present, Keyword::max_contains) && matches > node.max_contains) {
        add(path, ErrorCode::array_contains, "array contains too many matching items");
        ok = false;
      }
    }
    return ok;
  }

  bool check_object(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    if (!instance.is_object()) {
      return true;
    }
    const std::size_t size = instance.size();
    bool ok = true;
    if (has_keyword(node.present, Keyword::min_properties) && size < node.min_properties) {
      add(path, ErrorCode::object_min_properties, "object has too few properties");
      ok = false;
    }
    if (has_keyword(node.present, Keyword::max_properties) && size > node.max_properties) {
      add(path, ErrorCode::object_max_properties, "object has too many properties");
      ok = false;
    }

    if (has_keyword(node.present, Keyword::required)) {
      for (std::uint32_t index = 0; index < node.required.count; ++index) {
        const std::string_view name = schema_.name(schema_.name_at(node.required, index));
        if (!instance.contains(name)) {
          if (path != nullptr) {
            path->push_key(name);
          }
          add(path, ErrorCode::object_required, "required property is missing");
          if (path != nullptr) {
            path->pop_back();
          }
          ok = false;
        }
      }
    }

    const bool has_properties = has_keyword(node.present, Keyword::properties);
    const bool has_pattern_properties =
        has_keyword(node.present, Keyword::pattern_properties) && options_.pattern_matcher;
    for (const ValueView member : instance) {
      const std::string_view name = member.key();
      bool covered = false;
      if (has_properties) {
        const SchemaId declared = find_property(node.properties, name);
        if (declared != no_schema) {
          covered = true;
          ok = check_member(declared, member, name, path) && ok;
        }
      }
      if (has_pattern_properties) {
        for (std::uint32_t index = 0; index < node.pattern_properties.count; ++index) {
          const MemberEntry& entry = schema_.member_at(node.pattern_properties, index);
          if (options_.pattern_matcher(schema_.key(entry), name)) {
            covered = true;
            ok = check_member(entry.schema, member, name, path) && ok;
          }
        }
      }
      if (!covered && has_keyword(node.present, Keyword::additional_properties)) {
        ok = check_member(node.additional_properties, member, name, path) && ok;
      }
    }

    if (has_keyword(node.present, Keyword::property_names)) {
      for (const ValueView member : instance) {
        const std::string_view name = member.key();
        const ValueView name_instance = make_string_view(name);
        ok = check_member(node.property_names, name_instance, name, path) && ok;
      }
    }

    if (has_keyword(node.present, Keyword::dependent_required)) {
      for (std::uint32_t index = 0; index < node.dependent_required.count; ++index) {
        const DependentEntry& entry = schema_.dependent_at(node.dependent_required, index);
        const std::string_view trigger = schema_.text(entry.key_off, entry.key_len);
        if (!instance.contains(trigger)) {
          continue;
        }
        for (std::uint32_t name_index = 0; name_index < entry.run.count; ++name_index) {
          const std::string_view name =
              schema_.name(schema_.name_at(entry.run, name_index));
          if (!instance.contains(name)) {
            add(path, ErrorCode::object_dependent_required,
                "property required by dependentRequired is missing");
            ok = false;
          }
        }
      }
    }

    if (has_keyword(node.present, Keyword::dependent_schemas)) {
      for (std::uint32_t index = 0; index < node.dependent_schemas.count; ++index) {
        const DependentEntry& entry = schema_.dependent_at(node.dependent_schemas, index);
        const std::string_view trigger = schema_.text(entry.key_off, entry.key_len);
        if (!instance.contains(trigger)) {
          continue;
        }
        ok = check(schema_.subschema_at(entry.run, 0), instance, path) && ok;
      }
    }

    // unevaluatedProperties needs annotation tracking and is not interpreted yet.
    return ok;
  }

  bool check_composition(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    bool ok = true;
    if (has_keyword(node.present, Keyword::all_of)) {
      for (std::uint32_t index = 0; index < node.all_of.count; ++index) {
        ok = check(schema_.subschema_at(node.all_of, index), instance, path) && ok;
      }
    }
    if (has_keyword(node.present, Keyword::any_of)) {
      bool matched = false;
      for (std::uint32_t index = 0; index < node.any_of.count; ++index) {
        if (check(schema_.subschema_at(node.any_of, index), instance, nullptr)) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        add(path, ErrorCode::composition_any_of, "no anyOf branch matched");
        ok = false;
      }
    }
    if (has_keyword(node.present, Keyword::one_of)) {
      std::size_t matched = 0;
      for (std::uint32_t index = 0; index < node.one_of.count; ++index) {
        if (check(schema_.subschema_at(node.one_of, index), instance, nullptr)) {
          ++matched;
        }
      }
      if (matched != 1) {
        add(path, ErrorCode::composition_one_of, "exactly one oneOf branch must match");
        ok = false;
      }
    }
    if (has_keyword(node.present, Keyword::not_schema)) {
      if (check(node.not_schema, instance, nullptr)) {
        add(path, ErrorCode::composition_not, "not branch matched");
        ok = false;
      }
    }
    if (has_keyword(node.present, Keyword::if_schema)) {
      const bool condition = check(node.if_schema, instance, nullptr);
      if (condition) {
        if (has_keyword(node.present, Keyword::then_schema)) {
          ok = check(node.then_schema, instance, path) && ok;
        }
      } else if (has_keyword(node.present, Keyword::else_schema)) {
        ok = check(node.else_schema, instance, path) && ok;
      }
    }
    return ok;
  }

  bool check_values(const SchemaNode& node, ValueView instance, ErrorPath* path) {
    bool ok = true;
    if (has_keyword(node.present, Keyword::enum_values)) {
      bool found = false;
      for (std::uint32_t index = 0; index < node.enum_values.count; ++index) {
        if (deep_equal(schema_.value_at(node.enum_values, index), instance)) {
          found = true;
          break;
        }
      }
      if (!found) {
        add(path, ErrorCode::enum_mismatch, "value is not one of the allowed values");
        ok = false;
      }
    }
    if (has_keyword(node.present, Keyword::const_value)) {
      if (!deep_equal(schema_.const_value(node), instance)) {
        add(path, ErrorCode::const_mismatch, "value does not match the constant");
        ok = false;
      }
    }
    return ok;
  }

  // --- helpers ------------------------------------------------------------

  [[nodiscard]] SchemaId find_property(const SchemaRun& run, std::string_view key) const {
    for (std::uint32_t index = 0; index < run.count; ++index) {
      const MemberEntry& entry = schema_.member_at(run, index);
      if (schema_.key(entry) == key) {
        return entry.schema;
      }
    }
    return no_schema;
  }

  bool check_member(SchemaId id, ValueView value, std::string_view name, ErrorPath* path) {
    if (path != nullptr) {
      path->push_key(name);
    }
    const bool ok = check(id, value, path);
    if (path != nullptr) {
      path->pop_back();
    }
    return ok;
  }

  bool check_element(SchemaId id, ValueView value, std::size_t index, ErrorPath* path) {
    if (path != nullptr) {
      path->push_index(index);
    }
    const bool ok = check(id, value, path);
    if (path != nullptr) {
      path->pop_back();
    }
    return ok;
  }

  // propertyNames validates a bare string, which has no node in the instance
  // document; a scratch document keeps the interpreter single-path instead of
  // growing a second string-only validation route.
  [[nodiscard]] ValueView make_string_view(std::string_view text) {
    std::unique_ptr<ParsedDocument> document(new ParsedDocument());
    DocumentBuilder builder;
    static_cast<void>(builder.make_string(text));
    *document = builder.finish();
    scratch_.push_back(std::move(document));
    return scratch_.back()->root();
  }

  [[nodiscard]] bool is_multiple_of(double value, double divisor) const {
    if (!(divisor > 0.0) || !std::isfinite(value)) {
      return false;
    }
    const double quotient = value / divisor;
    if (!std::isfinite(quotient)) {
      return false;
    }
    const double nearest = std::round(quotient);
    const double tolerance = 1e-9 * std::max(1.0, std::fabs(quotient));
    return std::fabs(quotient - nearest) <= tolerance;
  }

  void add(ErrorPath* path, ErrorCode code, const char* message) {
    if (path == nullptr) {
      return;
    }
    if (result_.error_count() >= options_.max_errors) {
      return;
    }
    ValidationError error;
    error.code = code;
    error.message = message;
    error.instance_path = *path;
    result_.add_error(std::move(error));
  }

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
  explicit Validator(const CompiledSchema& schema,
                     ValidationOptions options = ValidationOptions())
      : schema_(schema), options_(std::move(options)) {}

  [[nodiscard]] ValidationResult validate(ValueView instance) const {
    detail::ValidationRun run(schema_, options_);
    return run.execute(instance);
  }

 private:
  const CompiledSchema& schema_;
  ValidationOptions options_;
};

[[nodiscard]] inline ValidationResult validate(
    const CompiledSchema& schema, ValueView instance,
    ValidationOptions options = ValidationOptions()) {
  Validator validator(schema, std::move(options));
  return validator.validate(instance);
}

}  // namespace jsonschema
}  // namespace oascpp
