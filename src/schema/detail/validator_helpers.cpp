#include <algorithm>
#include <cmath>
#include <limits>
#include <oascpp/document_builder.hpp>

#include "validation_run.hpp"

namespace oascpp::schema {
namespace detail {
namespace {

std::string_view keyword_for_error(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::type_mismatch:
      return "type";
    case ErrorCode::enum_mismatch:
      return "enum";
    case ErrorCode::const_mismatch:
      return "const";
    case ErrorCode::numeric_minimum:
      return "minimum";
    case ErrorCode::numeric_maximum:
      return "maximum";
    case ErrorCode::numeric_exclusive_minimum:
      return "exclusiveMinimum";
    case ErrorCode::numeric_exclusive_maximum:
      return "exclusiveMaximum";
    case ErrorCode::numeric_multiple_of:
      return "multipleOf";
    case ErrorCode::string_min_length:
      return "minLength";
    case ErrorCode::string_max_length:
      return "maxLength";
    case ErrorCode::string_pattern:
      return "pattern";
    case ErrorCode::string_format:
      return "format";
    case ErrorCode::array_min_items:
      return "minItems";
    case ErrorCode::array_max_items:
      return "maxItems";
    case ErrorCode::array_unique_items:
      return "uniqueItems";
    case ErrorCode::array_contains:
      return "contains";
    case ErrorCode::object_min_properties:
      return "minProperties";
    case ErrorCode::object_max_properties:
      return "maxProperties";
    case ErrorCode::object_required:
      return "required";
    case ErrorCode::object_dependent_required:
      return "dependentRequired";
    case ErrorCode::composition_any_of:
      return "anyOf";
    case ErrorCode::composition_one_of:
      return "oneOf";
    case ErrorCode::composition_not:
      return "not";
    default:
      return {};
  }
}

}  // namespace

[[nodiscard]] SchemaId ValidationRun::find_property(const SchemaRun& run, std::string_view key) const {
  for (std::uint32_t index = 0; index < run.count; ++index) {
    const MemberEntry& entry = schema_.member_at(run, index);
    if (schema_.key(entry) == key) {
      return entry.schema;
    }
  }
  return no_schema;
}

bool ValidationRun::check_member(SchemaId id, ValueView value, std::string_view name, ErrorPath* path) {
  if (path != nullptr) {
    path->push_key(name);
  }
  const bool ok = check(id, value, path);
  if (path != nullptr) {
    path->pop_back();
  }
  return ok;
}

bool ValidationRun::check_element(SchemaId id, ValueView value, std::size_t index, ErrorPath* path) {
  if (path != nullptr) {
    path->push_index(index);
  }
  const bool ok = check(id, value, path);
  if (path != nullptr) {
    path->pop_back();
  }
  return ok;
}

[[nodiscard]] ValueView ValidationRun::make_string_view(std::string_view text) {
  std::unique_ptr<ParsedDocument> document(new ParsedDocument());
  DocumentBuilder builder;
  static_cast<void>(builder.make_string(text));
  *document = builder.finish();
  scratch_.push_back(std::move(document));
  return scratch_.back()->root();
}

[[nodiscard]] bool ValidationRun::is_multiple_of(NumberView value, NumberView divisor) const {
  const oascpp::detail::NumberOrder divisor_order = oascpp::detail::compare_numbers(divisor, NumberView{});
  if (divisor_order != oascpp::detail::NumberOrder::greater) {
    return false;
  }

  if (value.is_integer && divisor.is_integer) {
    std::uint64_t divisor_magnitude = 0;
    if (divisor.is_unsigned) {
      divisor_magnitude = divisor.as_unsigned;
    } else {
      if (divisor.as_signed <= 0) {
        return false;
      }
      divisor_magnitude = static_cast<std::uint64_t>(divisor.as_signed);
    }

    std::uint64_t value_magnitude = 0;
    if (value.is_unsigned) {
      value_magnitude = value.as_unsigned;
    } else if (value.as_signed < 0) {
      value_magnitude = static_cast<std::uint64_t>(-(value.as_signed + 1)) + 1;
    } else {
      value_magnitude = static_cast<std::uint64_t>(value.as_signed);
    }
    return divisor_magnitude != 0 && value_magnitude % divisor_magnitude == 0;
  }

  const double numeric_value = value.as_real;
  const double numeric_divisor = divisor.as_real;
  if (!std::isfinite(numeric_value) || !std::isfinite(numeric_divisor)) {
    return false;
  }
  const double quotient = numeric_value / numeric_divisor;
  if (!std::isfinite(quotient)) {
    return false;
  }
  const double nearest = std::round(quotient);
  const double tolerance = 4.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, std::fabs(quotient));
  return std::fabs(quotient - nearest) <= tolerance;
}

void ValidationRun::add(ErrorPath* path, ErrorCode code, const char* message) {
  if (path == nullptr) {
    return;
  }
  result_.mark_invalid();
  if (result_.error_count() >= options_.max_errors) {
    return;
  }
  ValidationError error;
  error.code = code;
  error.message = message;
  error.instance_path = *path;
  if (!schema_paths_.empty()) {
    error.schema_path = schema_paths_.back();
    const std::string_view keyword = keyword_for_error(code);
    if (!keyword.empty()) {
      error.schema_path.push_key(keyword);
    }
  }
  result_.add_error(std::move(error));
}

}  // namespace detail
}  // namespace oascpp::schema
