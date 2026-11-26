#include "oascpp/schema/jsonschema/compiler.hpp"

namespace oascpp {
namespace jsonschema {

[[nodiscard]] SchemaRun SchemaCompiler::compile_schema_run(ValueView array, const char* keyword) {
  SchemaRun run;
  if (failed_) {
    return run;
  }
  if (!array.is_array()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be an array of schemas");
    return run;
  }
  std::vector<SchemaId> children;
  children.reserve(array.size());
  for (const ValueView element : array) {
    const SchemaId id = compile_schema(element);
    if (failed_) {
      return run;
    }
    children.push_back(id);
  }
  run.off = static_cast<std::uint32_t>(schema_.subschemas_.size());
  run.count = static_cast<std::uint32_t>(children.size());
  schema_.subschemas_.insert(schema_.subschemas_.end(), children.begin(), children.end());
  return run;
}

[[nodiscard]] SchemaRun SchemaCompiler::compile_name_run(ValueView array, const char* keyword) {
  SchemaRun run;
  if (failed_) {
    return run;
  }
  if (!array.is_array()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be an array of strings");
    return run;
  }
  std::vector<NameEntry> entries;
  entries.reserve(array.size());
  for (const ValueView element : array) {
    if (!element.is_string()) {
      fail(ErrorCode::schema_invalid, std::string(keyword) + " entries must be strings");
      return run;
    }
    NameEntry entry;
    append_text(entry.off, entry.len, element.as_string());
    entries.push_back(entry);
  }
  run.off = static_cast<std::uint32_t>(schema_.names_.size());
  run.count = static_cast<std::uint32_t>(entries.size());
  schema_.names_.insert(schema_.names_.end(), entries.begin(), entries.end());
  return run;
}

[[nodiscard]] SchemaRun SchemaCompiler::compile_member_map(ValueView object, const char* keyword) {
  SchemaRun run;
  if (failed_) {
    return run;
  }
  if (!object.is_object()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be an object of schemas");
    return run;
  }
  std::vector<MemberEntry> entries;
  for (const ValueView member : object) {
    MemberEntry entry;
    append_text(entry.key_off, entry.key_len, member.key());
    entry.schema = compile_schema(member);
    if (failed_) {
      return run;
    }
    entries.push_back(entry);
  }
  run.off = static_cast<std::uint32_t>(schema_.members_.size());
  run.count = static_cast<std::uint32_t>(entries.size());
  schema_.members_.insert(schema_.members_.end(), entries.begin(), entries.end());
  return run;
}

[[nodiscard]] SchemaRun SchemaCompiler::compile_dependent_names(ValueView object, const char* keyword) {
  SchemaRun run;
  if (failed_) {
    return run;
  }
  if (!object.is_object()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be an object");
    return run;
  }
  std::vector<DependentEntry> entries;
  for (const ValueView member : object) {
    const SchemaRun names = compile_name_run(member, keyword);
    if (failed_) {
      return run;
    }
    DependentEntry entry;
    append_text(entry.key_off, entry.key_len, member.key());
    entry.run = names;
    entries.push_back(entry);
  }
  run.off = static_cast<std::uint32_t>(schema_.dependents_.size());
  run.count = static_cast<std::uint32_t>(entries.size());
  schema_.dependents_.insert(schema_.dependents_.end(), entries.begin(), entries.end());
  return run;
}

[[nodiscard]] SchemaRun SchemaCompiler::compile_dependent_schemas(ValueView object, const char* keyword) {
  SchemaRun run;
  if (failed_) {
    return run;
  }
  if (!object.is_object()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be an object");
    return run;
  }
  std::vector<DependentEntry> entries;
  for (const ValueView member : object) {
    const SchemaId id = compile_schema(member);
    if (failed_) {
      return run;
    }
    DependentEntry entry;
    append_text(entry.key_off, entry.key_len, member.key());
    entry.run.off = static_cast<std::uint32_t>(schema_.subschemas_.size());
    entry.run.count = 1;
    schema_.subschemas_.push_back(id);
    entries.push_back(entry);
  }
  run.off = static_cast<std::uint32_t>(schema_.dependents_.size());
  run.count = static_cast<std::uint32_t>(entries.size());
  schema_.dependents_.insert(schema_.dependents_.end(), entries.begin(), entries.end());
  return run;
}

void SchemaCompiler::append_text(std::uint32_t& off, std::uint32_t& len, std::string_view value) {
  off = static_cast<std::uint32_t>(schema_.text_.size());
  len = static_cast<std::uint32_t>(value.size());
  schema_.text_.insert(schema_.text_.end(), value.begin(), value.end());
}

[[nodiscard]] bool SchemaCompiler::read_number(ValueView view, const char* keyword, double& out_value) {
  if (!view.is_number()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be a number");
    return false;
  }
  out_value = view.as_number().as_real;
  return true;
}

[[nodiscard]] bool SchemaCompiler::read_unsigned(ValueView view, const char* keyword, std::uint32_t& out_value) {
  if (!view.is_integer()) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be a non-negative integer");
    return false;
  }
  const NumberView number = view.as_number();
  if (number.is_unsigned) {
    if (number.as_unsigned > 4294967295ull) {
      fail(ErrorCode::schema_invalid, std::string(keyword) + " is too large");
      return false;
    }
    out_value = static_cast<std::uint32_t>(number.as_unsigned);
    return true;
  }
  if (number.as_signed < 0) {
    fail(ErrorCode::schema_invalid, std::string(keyword) + " must be a non-negative integer");
    return false;
  }
  out_value = static_cast<std::uint32_t>(number.as_signed);
  return true;
}

[[nodiscard]] bool SchemaCompiler::type_bit(std::string_view name, std::uint8_t& out_bit) {
  if (name == "null") {
    out_bit = type_bit_null;
    return true;
  }
  if (name == "boolean") {
    out_bit = type_bit_boolean;
    return true;
  }
  if (name == "object") {
    out_bit = type_bit_object;
    return true;
  }
  if (name == "array") {
    out_bit = type_bit_array;
    return true;
  }
  if (name == "number") {
    out_bit = type_bit_number;
    return true;
  }
  if (name == "string") {
    out_bit = type_bit_string;
    return true;
  }
  if (name == "integer") {
    out_bit = type_bit_integer;
    return true;
  }
  fail(ErrorCode::schema_invalid, "unknown type name: " + std::string(name));
  return false;
}

void SchemaCompiler::fail(ErrorCode code, std::string message) {
  if (failed_) {
    return;
  }
  failed_ = true;
  error_code_ = code;
  error_message_ = std::move(message);
}

}  // namespace jsonschema
}  // namespace oascpp
