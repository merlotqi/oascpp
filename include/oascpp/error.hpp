#pragma once

// Error contract. Code plus pointer paths are the stable machine surface;
// free-form messages are informational and may change between releases.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "oascpp/json_pointer.hpp"

namespace oascpp {

enum class ErrorCode : std::uint16_t {
  none = 0,

  // Structural problems: the input is not a well-formed document of the
  // expected shape, before any schema is applied.
  parse_error,
  missing_field,
  unexpected_field,
  invalid_type,
  invalid_value,
  unresolved_reference,
  circular_reference,
  schema_invalid,
  unsupported_dialect,

  // Schema keyword failures.
  type_mismatch,
  enum_mismatch,
  const_mismatch,
  numeric_minimum,
  numeric_maximum,
  numeric_exclusive_minimum,
  numeric_exclusive_maximum,
  numeric_multiple_of,
  string_min_length,
  string_max_length,
  string_pattern,
  string_format,
  array_min_items,
  array_max_items,
  array_unique_items,
  array_contains,
  array_unevaluated_item,
  object_min_properties,
  object_max_properties,
  object_required,
  object_additional_property,
  object_property_names,
  object_dependent_required,
  object_unevaluated_property,
  composition_all_of,
  composition_any_of,
  composition_one_of,
  composition_not,
  composition_if_then_else,

  // Runtime problems: routing, parameters, bodies, responses, security.
  route_not_found,
  method_not_allowed,
  parameter_missing,
  parameter_invalid,
  request_body_missing,
  request_body_invalid,
  media_type_unsupported,
  response_status_unmatched,
  response_body_invalid,
  security_unsatisfied,

  binding_not_registered,
};

[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::none:
      return "none";
    case ErrorCode::parse_error:
      return "parse_error";
    case ErrorCode::missing_field:
      return "missing_field";
    case ErrorCode::unexpected_field:
      return "unexpected_field";
    case ErrorCode::invalid_type:
      return "invalid_type";
    case ErrorCode::invalid_value:
      return "invalid_value";
    case ErrorCode::unresolved_reference:
      return "unresolved_reference";
    case ErrorCode::circular_reference:
      return "circular_reference";
    case ErrorCode::schema_invalid:
      return "schema_invalid";
    case ErrorCode::unsupported_dialect:
      return "unsupported_dialect";
    case ErrorCode::type_mismatch:
      return "type_mismatch";
    case ErrorCode::enum_mismatch:
      return "enum_mismatch";
    case ErrorCode::const_mismatch:
      return "const_mismatch";
    case ErrorCode::numeric_minimum:
      return "numeric_minimum";
    case ErrorCode::numeric_maximum:
      return "numeric_maximum";
    case ErrorCode::numeric_exclusive_minimum:
      return "numeric_exclusive_minimum";
    case ErrorCode::numeric_exclusive_maximum:
      return "numeric_exclusive_maximum";
    case ErrorCode::numeric_multiple_of:
      return "numeric_multiple_of";
    case ErrorCode::string_min_length:
      return "string_min_length";
    case ErrorCode::string_max_length:
      return "string_max_length";
    case ErrorCode::string_pattern:
      return "string_pattern";
    case ErrorCode::string_format:
      return "string_format";
    case ErrorCode::array_min_items:
      return "array_min_items";
    case ErrorCode::array_max_items:
      return "array_max_items";
    case ErrorCode::array_unique_items:
      return "array_unique_items";
    case ErrorCode::array_contains:
      return "array_contains";
    case ErrorCode::array_unevaluated_item:
      return "array_unevaluated_item";
    case ErrorCode::object_min_properties:
      return "object_min_properties";
    case ErrorCode::object_max_properties:
      return "object_max_properties";
    case ErrorCode::object_required:
      return "object_required";
    case ErrorCode::object_additional_property:
      return "object_additional_property";
    case ErrorCode::object_property_names:
      return "object_property_names";
    case ErrorCode::object_dependent_required:
      return "object_dependent_required";
    case ErrorCode::object_unevaluated_property:
      return "object_unevaluated_property";
    case ErrorCode::composition_all_of:
      return "composition_all_of";
    case ErrorCode::composition_any_of:
      return "composition_any_of";
    case ErrorCode::composition_one_of:
      return "composition_one_of";
    case ErrorCode::composition_not:
      return "composition_not";
    case ErrorCode::composition_if_then_else:
      return "composition_if_then_else";
    case ErrorCode::route_not_found:
      return "route_not_found";
    case ErrorCode::method_not_allowed:
      return "method_not_allowed";
    case ErrorCode::parameter_missing:
      return "parameter_missing";
    case ErrorCode::parameter_invalid:
      return "parameter_invalid";
    case ErrorCode::request_body_missing:
      return "request_body_missing";
    case ErrorCode::request_body_invalid:
      return "request_body_invalid";
    case ErrorCode::media_type_unsupported:
      return "media_type_unsupported";
    case ErrorCode::response_status_unmatched:
      return "response_status_unmatched";
    case ErrorCode::response_body_invalid:
      return "response_body_invalid";
    case ErrorCode::security_unsatisfied:
      return "security_unsatisfied";
    case ErrorCode::binding_not_registered:
      return "binding_not_registered";
  }
  return "unknown";
}

// JSON Pointer built incrementally while walking a document. Tokens are owned,
// so a path outlives the document it describes. Error paths are cold, so the
// allocation here is acceptable; the allocation-free variant only matters once
// validation becomes hot.
class ErrorPath {
 public:
  struct Token {
    std::string key;
    std::size_t index = 0;
    bool is_index = false;
  };

  ErrorPath() = default;

  ErrorPath& push_key(std::string_view key) {
    Token token;
    if (!key.empty()) {
      token.key.assign(key.data(), key.size());
    }
    tokens_.push_back(std::move(token));
    return *this;
  }

  ErrorPath& push_index(std::size_t index) {
    Token token;
    token.index = index;
    token.is_index = true;
    tokens_.push_back(std::move(token));
    return *this;
  }

  // Walks that push a token before descending have to pop it again.
  void pop_back() noexcept {
    if (!tokens_.empty()) {
      tokens_.pop_back();
    }
  }

  [[nodiscard]] bool empty() const noexcept { return tokens_.empty(); }
  [[nodiscard]] std::size_t depth() const noexcept { return tokens_.size(); }
  [[nodiscard]] const std::vector<Token>& tokens() const noexcept { return tokens_; }

  [[nodiscard]] std::string to_string() const {
    std::string out;
    for (const Token& token : tokens_) {
      out.push_back('/');
      if (token.is_index) {
        out.append(std::to_string(token.index));
      } else {
        append_escaped_pointer_token(out, token.key);
      }
    }
    return out;
  }

 private:
  std::vector<Token> tokens_;
};

struct ValidationError {
  ErrorCode code = ErrorCode::none;
  ErrorPath instance_path;
  ErrorPath schema_path;
  std::string message;
  std::string expected;
  std::string actual;
};

// Errors accumulate into a result instead of aborting on the first failure, so
// callers can report every problem in one pass.
class ValidationResult {
 public:
  ValidationResult() = default;

  [[nodiscard]] bool ok() const noexcept { return errors_.empty(); }
  explicit operator bool() const noexcept { return ok(); }

  [[nodiscard]] std::size_t error_count() const noexcept { return errors_.size(); }
  [[nodiscard]] bool empty() const noexcept { return errors_.empty(); }

  [[nodiscard]] const std::vector<ValidationError>& errors() const noexcept { return errors_; }

  [[nodiscard]] const ValidationError* first_error() const noexcept {
    return errors_.empty() ? nullptr : &errors_.front();
  }

  [[nodiscard]] ErrorCode first_error_code() const noexcept {
    return errors_.empty() ? ErrorCode::none : errors_.front().code;
  }

  void add_error(ValidationError error) { errors_.push_back(std::move(error)); }

  // Hints the size of the error list up front, which keeps the common "valid
  // document, no errors" case allocation free.
  void reserve(std::size_t count) { errors_.reserve(count); }


  void add(ErrorCode code) {
    ValidationError error;
    error.code = code;
    errors_.push_back(std::move(error));
  }

  void clear() noexcept { errors_.clear(); }

 private:
  std::vector<ValidationError> errors_;
};

}  // namespace oascpp
