#include <cmath>
#include <oascpp/schema/validator.hpp>
#include <utility>

#include "detail/validation_run.hpp"

namespace oascpp::schema {
namespace detail {
namespace {

ErrorPath make_schema_path(std::string_view pointer) {
  ErrorPath path;
  std::size_t position = 0;
  while (position < pointer.size()) {
    if (pointer[position] != '/') {
      break;
    }
    const std::size_t start = position + 1;
    const std::size_t end = pointer.find('/', start);
    path.push_key(oascpp::unescape_pointer_token(
        pointer.substr(start, end == std::string_view::npos ? pointer.size() - start : end - start)));
    position = end == std::string_view::npos ? pointer.size() : end;
  }
  return path;
}

}  // namespace

std::size_t utf8_length(std::string_view text) noexcept {
  std::size_t count = 0;
  for (const char character : text) {
    const unsigned char byte = static_cast<unsigned char>(character);
    if ((byte & 0xC0u) != 0x80u) {
      ++count;
    }
  }
  return count;
}

ValidationRun::ValidationRun(const CompiledSchema& schema, const ValidationOptions& options)
    : schema_(schema), options_(options) {}

ValidationResult ValidationRun::execute(ValueView instance) {
  if (!schema_.valid()) {
    ValidationError error;
    error.code = ErrorCode::schema_invalid;
    error.message = "compiled schema is invalid";
    result_.add_error(std::move(error));
    return std::move(result_);
  }
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

bool ValidationRun::check(SchemaId id, ValueView instance, ErrorPath* path) {
  if (id == no_schema || !instance.valid()) {
    return true;
  }
  if (id >= schema_.size()) {
    add(path, ErrorCode::schema_invalid, "compiled schema reference is out of range");
    return false;
  }
  const SchemaNode& node = schema_.node(id);
  schema_paths_.push_back(make_schema_path(schema_.schema_path(node)));
  if (depth_ >= options_.max_depth) {
    add(path, ErrorCode::schema_invalid, "validation nesting is too deep");
    schema_paths_.pop_back();
    return false;
  }

  // A repeated schema/instance pair on the active path is a recursive cycle.
  const std::pair<SchemaId, std::uint32_t> visit(id, instance.index());
  for (const auto& active : active_) {
    if (active == visit) {
      schema_paths_.pop_back();
      return true;
    }
  }
  active_.push_back(visit);
  ++depth_;

  bool ok = true;
  if ((node.flags & schema_flag_false) != 0) {
    add(path, ErrorCode::invalid_value, "schema is false");
    ok = false;
  } else {
    ok = check_node(node, instance, path);
  }

  --depth_;
  active_.pop_back();
  schema_paths_.pop_back();
  return ok;
}

bool ValidationRun::check_node(const SchemaNode& node, ValueView instance, ErrorPath* path) {
  bool ok = true;
  if (has_keyword(node.present, Keyword::ref)) {
    ok = check(node.ref, instance, path) && ok;
    if (!schema_.profile().ref_siblings_apply) {
      return ok;
    }
  }
  if (has_keyword(node.present, Keyword::dynamic_ref)) {
    result_.mark_incomplete();
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

bool ValidationRun::type_matches(std::uint8_t types, ValueView instance) const {
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

}  // namespace detail

Validator::Validator(const CompiledSchema& schema, ValidationOptions options)
    : schema_(schema), options_(std::move(options)) {}

ValidationResult Validator::validate(ValueView instance) const {
  detail::ValidationRun run(schema_, options_);
  return run.execute(instance);
}

ValidationResult validate(const CompiledSchema& schema, ValueView instance, ValidationOptions options) {
  Validator validator(schema, std::move(options));
  return validator.validate(instance);
}

}  // namespace oascpp::schema
