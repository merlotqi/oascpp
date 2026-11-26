#include "validation_run.hpp"

namespace oascpp {
namespace jsonschema {
namespace detail {

bool ValidationRun::check_number(const SchemaNode& node, ValueView instance, ErrorPath* path) {
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
  if (has_keyword(node.present, Keyword::exclusive_minimum) && !(value > node.exclusive_minimum)) {
    add(path, ErrorCode::numeric_exclusive_minimum, "value is not above the exclusive minimum");
    ok = false;
  }
  if (has_keyword(node.present, Keyword::exclusive_maximum) && !(value < node.exclusive_maximum)) {
    add(path, ErrorCode::numeric_exclusive_maximum, "value is not below the exclusive maximum");
    ok = false;
  }
  if (has_keyword(node.present, Keyword::multiple_of) && !is_multiple_of(value, node.multiple_of)) {
    add(path, ErrorCode::numeric_multiple_of, "value is not a multiple");
    ok = false;
  }
  return ok;
}

bool ValidationRun::check_string(const SchemaNode& node, ValueView instance, ErrorPath* path) {
  if (!instance.is_string()) {
    return true;
  }
  const std::string_view value = instance.as_string();
  bool ok = true;
  if (has_keyword(node.present, Keyword::min_length) || has_keyword(node.present, Keyword::max_length)) {
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

bool ValidationRun::check_array(const SchemaNode& node, ValueView instance, ErrorPath* path) {
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

  const std::uint32_t prefix_count = has_keyword(node.present, Keyword::prefix_items) ? node.prefix_items.count : 0;
  if (has_keyword(node.present, Keyword::prefix_items)) {
    const std::size_t limit = std::min<std::size_t>(prefix_count, size);
    for (std::size_t index = 0; index < limit; ++index) {
      ok = check_element(schema_.subschema_at(node.prefix_items, static_cast<std::uint32_t>(index)), instance.at(index),
                         index, path) &&
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
    const std::size_t minimum = has_keyword(node.present, Keyword::min_contains) ? node.min_contains : 1;
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

}  // namespace detail
}  // namespace jsonschema
}  // namespace oascpp
