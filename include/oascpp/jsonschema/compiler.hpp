#pragma once

// Schema compiler: turns a JSON Schema document into the flat, immutable graph
// described in schema.hpp.
//
// Design notes:
//   * every source schema node is compiled exactly once and memoised by its
//     arena index, so recursive $ref graphs terminate;
//   * $ref is resolved eagerly at compile time; unresolvable references are
//     compile errors rather than runtime surprises;
//   * the source document is borrowed by pointer and must outlive the result;
//   * pools are appended to in contiguous batches, so a SchemaRun never
//     interleaves with a nested compile.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oascpp/error.hpp"
#include "oascpp/json_pointer.hpp"
#include "oascpp/jsonschema/dialect.hpp"
#include "oascpp/jsonschema/schema.hpp"
#include "oascpp/result.hpp"
#include "oascpp/value.hpp"

namespace oascpp {
namespace jsonschema {

struct CompileOptions {
  Dialect dialect = Dialect::draft2020_12;
  std::size_t max_schemas = 4096;
  std::size_t max_depth = 256;
};

class SchemaCompiler {
 public:
  [[nodiscard]] static Result<CompiledSchema> compile(
      const ParsedDocument& document, ValueView root,
      const CompileOptions& options = CompileOptions()) {
    SchemaCompiler compiler(document, options);
    compiler.compile_root(root);
    if (compiler.failed_) {
      return Error{compiler.error_code_, std::move(compiler.error_message_)};
    }
    return std::move(compiler.schema_);
  }

 private:
  SchemaCompiler(const ParsedDocument& document, const CompileOptions& options)
      : document_(document), options_(options) {
    schema_.document_ = &document_;
    schema_.profile_ = profile_for(options_.dialect);
    if (!schema_.profile_.supported) {
      fail(ErrorCode::unsupported_dialect, "this dialect is not supported yet");
    }
  }

  // --- driver -------------------------------------------------------------

  void compile_root(ValueView root) {
    if (failed_) {
      return;
    }
    if (!root.valid()) {
      fail(ErrorCode::schema_invalid, "schema document is empty");
      return;
    }
    const SchemaId root_id = compile_schema(root);
    if (failed_) {
      return;
    }
    schema_.root_ = root_id;
    // Sentinel so text() can always form a valid string_view from data() + off.
    schema_.text_.push_back('\0');
  }

  [[nodiscard]] SchemaId compile_schema(ValueView view) {
    if (failed_) {
      return no_schema;
    }
    if (!view.valid()) {
      fail(ErrorCode::schema_invalid, "schema is missing");
      return no_schema;
    }

    const std::uint32_t source = view.index();
    const auto existing = compiled_.find(source);
    if (existing != compiled_.end()) {
      return existing->second;
    }
    if (depth_ >= options_.max_depth) {
      fail(ErrorCode::schema_invalid, "schema nesting is too deep");
      return no_schema;
    }
    if (schema_.nodes_.size() >= options_.max_schemas) {
      fail(ErrorCode::schema_invalid, "schema has too many nodes");
      return no_schema;
    }

    // Allocate before descending so a recursive $ref can find this node.
    const SchemaId id = static_cast<SchemaId>(schema_.nodes_.size());
    schema_.nodes_.push_back(SchemaNode{});
    compiled_.emplace(source, id);

    SchemaNode node;
    node.source = source;
    ++depth_;
    fill_node(node, view);
    --depth_;
    if (failed_) {
      return no_schema;
    }
    schema_.nodes_[id] = node;
    return id;
  }

  void fill_node(SchemaNode& node, ValueView view) {
    if (view.is_boolean()) {
      if (!view.as_boolean()) {
        node.flags |= schema_flag_false;
      }
      return;
    }
    if (!view.is_object()) {
      fail(ErrorCode::schema_invalid, "schema must be an object or a boolean");
      return;
    }

    read_refs(node, view);
    read_type(node, view);
    read_numeric(node, view);
    read_string_keywords(node, view);
    read_array_keywords(node, view);
    read_object_keywords(node, view);
    read_composition(node, view);
    read_value_keywords(node, view);
  }

  // --- keyword readers ----------------------------------------------------

  void read_refs(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView ref = view.find("$ref");
    if (ref.valid()) {
      if (!ref.is_string()) {
        fail(ErrorCode::schema_invalid, "$ref must be a string");
        return;
      }
      node.ref = resolve_reference(ref.as_string());
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::ref);
    }

    const ValueView dynamic_ref = view.find("$dynamicRef");
    if (dynamic_ref.valid()) {
      if (!dynamic_ref.is_string()) {
        fail(ErrorCode::schema_invalid, "$dynamicRef must be a string");
        return;
      }
      node.dynamic_ref = resolve_reference(dynamic_ref.as_string());
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::dynamic_ref);
    }
  }

  void read_type(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView type = view.find("type");
    if (type.valid()) {
      std::uint8_t bits = 0;
      if (type.is_string()) {
        if (!type_bit(type.as_string(), bits)) {
          return;
        }
      } else if (type.is_array()) {
        if (type.empty()) {
          fail(ErrorCode::schema_invalid, "type array must not be empty");
          return;
        }
        for (const ValueView element : type) {
          if (!element.is_string()) {
            fail(ErrorCode::schema_invalid, "type entries must be strings");
            return;
          }
          std::uint8_t bit = 0;
          if (!type_bit(element.as_string(), bit)) {
            return;
          }
          bits |= bit;
        }
      } else {
        fail(ErrorCode::schema_invalid, "type must be a string or an array of strings");
        return;
      }
      node.types = bits;
      node.present |= keyword_bit(Keyword::type);
    }

    // OpenAPI 3.0 spelling of "this may also be null".
    const ValueView nullable = view.find("nullable");
    if (nullable.valid() && nullable.is_boolean() && nullable.as_boolean()) {
      if (node.types != 0) {
        node.types |= type_bit_null;
      }
    }
  }

  void read_numeric(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView multiple_of = view.find("multipleOf");
    if (multiple_of.valid()) {
      double value = 0.0;
      if (!read_number(multiple_of, "multipleOf", value)) {
        return;
      }
      if (!(value > 0.0)) {
        fail(ErrorCode::schema_invalid, "multipleOf must be greater than zero");
        return;
      }
      node.multiple_of = value;
      node.present |= keyword_bit(Keyword::multiple_of);
    }

    const ValueView minimum = view.find("minimum");
    if (minimum.valid()) {
      double value = 0.0;
      if (!read_number(minimum, "minimum", value)) {
        return;
      }
      node.minimum = value;
      node.present |= keyword_bit(Keyword::minimum);
    }

    const ValueView exclusive_minimum = view.find("exclusiveMinimum");
    if (exclusive_minimum.valid()) {
      if (exclusive_minimum.is_boolean()) {
        // draft4 / OpenAPI 3.0 spelling.
        if (exclusive_minimum.as_boolean()) {
          if (!has_keyword(node.present, Keyword::minimum)) {
            fail(ErrorCode::schema_invalid, "exclusiveMinimum: true requires minimum");
            return;
          }
          node.exclusive_minimum = node.minimum;
          node.present |= keyword_bit(Keyword::exclusive_minimum);
          node.present &= ~keyword_bit(Keyword::minimum);
        }
      } else {
        double value = 0.0;
        if (!read_number(exclusive_minimum, "exclusiveMinimum", value)) {
          return;
        }
        node.exclusive_minimum = value;
        node.present |= keyword_bit(Keyword::exclusive_minimum);
      }
    }

    const ValueView maximum = view.find("maximum");
    if (maximum.valid()) {
      double value = 0.0;
      if (!read_number(maximum, "maximum", value)) {
        return;
      }
      node.maximum = value;
      node.present |= keyword_bit(Keyword::maximum);
    }

    const ValueView exclusive_maximum = view.find("exclusiveMaximum");
    if (exclusive_maximum.valid()) {
      if (exclusive_maximum.is_boolean()) {
        if (exclusive_maximum.as_boolean()) {
          if (!has_keyword(node.present, Keyword::maximum)) {
            fail(ErrorCode::schema_invalid, "exclusiveMaximum: true requires maximum");
            return;
          }
          node.exclusive_maximum = node.maximum;
          node.present |= keyword_bit(Keyword::exclusive_maximum);
          node.present &= ~keyword_bit(Keyword::maximum);
        }
      } else {
        double value = 0.0;
        if (!read_number(exclusive_maximum, "exclusiveMaximum", value)) {
          return;
        }
        node.exclusive_maximum = value;
        node.present |= keyword_bit(Keyword::exclusive_maximum);
      }
    }
  }

  void read_string_keywords(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView min_length = view.find("minLength");
    if (min_length.valid()) {
      if (!read_unsigned(min_length, "minLength", node.min_length)) {
        return;
      }
      node.present |= keyword_bit(Keyword::min_length);
    }

    const ValueView max_length = view.find("maxLength");
    if (max_length.valid()) {
      if (!read_unsigned(max_length, "maxLength", node.max_length)) {
        return;
      }
      node.present |= keyword_bit(Keyword::max_length);
    }

    const ValueView pattern = view.find("pattern");
    if (pattern.valid()) {
      if (!pattern.is_string()) {
        fail(ErrorCode::schema_invalid, "pattern must be a string");
        return;
      }
      append_text(node.pattern_off, node.pattern_len, pattern.as_string());
      node.present |= keyword_bit(Keyword::pattern);
    }

    const ValueView format = view.find("format");
    if (format.valid()) {
      if (!format.is_string()) {
        fail(ErrorCode::schema_invalid, "format must be a string");
        return;
      }
      append_text(node.format_off, node.format_len, format.as_string());
      node.present |= keyword_bit(Keyword::format);
    }
  }

  void read_array_keywords(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView min_items = view.find("minItems");
    if (min_items.valid()) {
      if (!read_unsigned(min_items, "minItems", node.min_items)) {
        return;
      }
      node.present |= keyword_bit(Keyword::min_items);
    }

    const ValueView max_items = view.find("maxItems");
    if (max_items.valid()) {
      if (!read_unsigned(max_items, "maxItems", node.max_items)) {
        return;
      }
      node.present |= keyword_bit(Keyword::max_items);
    }

    const ValueView unique_items = view.find("uniqueItems");
    if (unique_items.valid()) {
      if (!unique_items.is_boolean()) {
        fail(ErrorCode::schema_invalid, "uniqueItems must be a boolean");
        return;
      }
      if (unique_items.as_boolean()) {
        node.present |= keyword_bit(Keyword::unique_items);
      }
    }

    const ValueView prefix_items = view.find("prefixItems");
    if (prefix_items.valid()) {
      node.prefix_items = compile_schema_run(prefix_items, "prefixItems");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::prefix_items);
    }

    const ValueView items = view.find("items");
    if (items.valid()) {
      if (items.is_array()) {
        // Legacy tuple form (draft4 through draft7).
        node.prefix_items = compile_schema_run(items, "items");
        if (failed_) {
          return;
        }
        node.present |= keyword_bit(Keyword::prefix_items);
        const ValueView additional_items = view.find("additionalItems");
        if (additional_items.valid()) {
          node.items = compile_schema(additional_items);
          if (failed_) {
            return;
          }
          node.present |= keyword_bit(Keyword::items);
        }
      } else {
        node.items = compile_schema(items);
        if (failed_) {
          return;
        }
        node.present |= keyword_bit(Keyword::items);
      }
    }

    const ValueView contains = view.find("contains");
    if (contains.valid()) {
      node.contains = compile_schema(contains);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::contains);
    }

    const ValueView min_contains = view.find("minContains");
    if (min_contains.valid()) {
      if (!read_unsigned(min_contains, "minContains", node.min_contains)) {
        return;
      }
      node.present |= keyword_bit(Keyword::min_contains);
    }

    const ValueView max_contains = view.find("maxContains");
    if (max_contains.valid()) {
      if (!read_unsigned(max_contains, "maxContains", node.max_contains)) {
        return;
      }
      node.present |= keyword_bit(Keyword::max_contains);
    }

    const ValueView unevaluated_items = view.find("unevaluatedItems");
    if (unevaluated_items.valid() && schema_.profile_.has_unevaluated) {
      node.unevaluated_items = compile_schema(unevaluated_items);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::unevaluated_items);
    }
  }

  void read_object_keywords(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView min_properties = view.find("minProperties");
    if (min_properties.valid()) {
      if (!read_unsigned(min_properties, "minProperties", node.min_properties)) {
        return;
      }
      node.present |= keyword_bit(Keyword::min_properties);
    }

    const ValueView max_properties = view.find("maxProperties");
    if (max_properties.valid()) {
      if (!read_unsigned(max_properties, "maxProperties", node.max_properties)) {
        return;
      }
      node.present |= keyword_bit(Keyword::max_properties);
    }

    const ValueView required = view.find("required");
    if (required.valid()) {
      node.required = compile_name_run(required, "required");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::required);
    }

    const ValueView properties = view.find("properties");
    if (properties.valid()) {
      node.properties = compile_member_map(properties, "properties");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::properties);
    }

    const ValueView pattern_properties = view.find("patternProperties");
    if (pattern_properties.valid()) {
      node.pattern_properties = compile_member_map(pattern_properties, "patternProperties");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::pattern_properties);
    }

    const ValueView additional_properties = view.find("additionalProperties");
    if (additional_properties.valid()) {
      node.additional_properties = compile_schema(additional_properties);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::additional_properties);
    }

    const ValueView property_names = view.find("propertyNames");
    if (property_names.valid()) {
      node.property_names = compile_schema(property_names);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::property_names);
    }

    const ValueView dependent_required = view.find("dependentRequired");
    if (dependent_required.valid()) {
      node.dependent_required = compile_dependent_names(dependent_required, "dependentRequired");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::dependent_required);
    }

    const ValueView dependent_schemas = view.find("dependentSchemas");
    if (dependent_schemas.valid()) {
      node.dependent_schemas = compile_dependent_schemas(dependent_schemas, "dependentSchemas");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::dependent_schemas);
    }

    const ValueView unevaluated_properties = view.find("unevaluatedProperties");
    if (unevaluated_properties.valid() && schema_.profile_.has_unevaluated) {
      node.unevaluated_properties = compile_schema(unevaluated_properties);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::unevaluated_properties);
    }
  }

  void read_composition(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView all_of = view.find("allOf");
    if (all_of.valid()) {
      node.all_of = compile_schema_run(all_of, "allOf");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::all_of);
    }

    const ValueView any_of = view.find("anyOf");
    if (any_of.valid()) {
      node.any_of = compile_schema_run(any_of, "anyOf");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::any_of);
    }

    const ValueView one_of = view.find("oneOf");
    if (one_of.valid()) {
      node.one_of = compile_schema_run(one_of, "oneOf");
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::one_of);
    }

    const ValueView not_schema = view.find("not");
    if (not_schema.valid()) {
      node.not_schema = compile_schema(not_schema);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::not_schema);
    }

    if (!schema_.profile_.has_if_then_else) {
      return;
    }

    const ValueView if_schema = view.find("if");
    if (if_schema.valid()) {
      node.if_schema = compile_schema(if_schema);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::if_schema);
    }

    const ValueView then_schema = view.find("then");
    if (then_schema.valid()) {
      node.then_schema = compile_schema(then_schema);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::then_schema);
    }

    const ValueView else_schema = view.find("else");
    if (else_schema.valid()) {
      node.else_schema = compile_schema(else_schema);
      if (failed_) {
        return;
      }
      node.present |= keyword_bit(Keyword::else_schema);
    }
  }

  void read_value_keywords(SchemaNode& node, ValueView view) {
    if (failed_) {
      return;
    }
    const ValueView enum_values = view.find("enum");
    if (enum_values.valid()) {
      if (!enum_values.is_array()) {
        fail(ErrorCode::schema_invalid, "enum must be an array");
        return;
      }
      std::vector<std::uint32_t> indices;
      indices.reserve(enum_values.size());
      for (const ValueView element : enum_values) {
        indices.push_back(element.index());
      }
      node.enum_values.off = static_cast<std::uint32_t>(schema_.values_.size());
      node.enum_values.count = static_cast<std::uint32_t>(indices.size());
      schema_.values_.insert(schema_.values_.end(), indices.begin(), indices.end());
      node.present |= keyword_bit(Keyword::enum_values);
    }

    const ValueView const_value = view.find("const");
    if (const_value.valid()) {
      node.const_value = const_value.index();
      node.present |= keyword_bit(Keyword::const_value);
    }
  }

  // --- reference resolution ----------------------------------------------

  [[nodiscard]] SchemaId resolve_reference(std::string_view ref) {
    if (ref.empty()) {
      fail(ErrorCode::unresolved_reference, "empty $ref");
      return no_schema;
    }
    if (ref.front() != '#') {
      fail(ErrorCode::unresolved_reference,
           "external references are not supported yet: " + std::string(ref));
      return no_schema;
    }
    if (ref.size() == 1) {
      return compile_schema(document_.root());
    }

    const std::string_view fragment = ref.substr(1);
    if (fragment.front() == '/') {
      ValueView target = document_.root();
      std::size_t position = 0;
      while (position < fragment.size()) {
        const std::size_t start = position + 1;
        std::size_t end = fragment.find('/', start);
        if (end == std::string_view::npos) {
          end = fragment.size();
        }
        const std::string token =
            unescape_pointer_token(fragment.substr(start, end - start));
        target = descend(target, token);
        if (!target.valid()) {
          fail(ErrorCode::unresolved_reference,
               "cannot resolve reference: " + std::string(ref));
          return no_schema;
        }
        position = end;
      }
      return compile_schema(target);
    }

    build_anchor_index();
    const auto found = anchors_.find(std::string(fragment));
    if (found == anchors_.end()) {
      fail(ErrorCode::unresolved_reference,
           "unknown anchor: " + std::string(ref));
      return no_schema;
    }
    return compile_schema(ValueView(&document_, found->second));
  }

  [[nodiscard]] ValueView descend(ValueView container, const std::string& token) {
    if (container.is_object()) {
      return container.find(token);
    }
    if (container.is_array()) {
      if (token.empty()) {
        return ValueView();
      }
      std::size_t index = 0;
      for (const char character : token) {
        if (character < '0' || character > '9') {
          return ValueView();
        }
        index = index * 10 + static_cast<std::size_t>(character - '0');
      }
      return container.at(index);
    }
    return ValueView();
  }

  void build_anchor_index() {
    if (anchors_built_) {
      return;
    }
    anchors_built_ = true;
    std::vector<ValueView> pending;
    pending.push_back(document_.root());
    while (!pending.empty()) {
      const ValueView current = pending.back();
      pending.pop_back();
      if (!current.valid()) {
        continue;
      }
      if (current.is_object()) {
        const ValueView anchor = current.find("$anchor");
        if (anchor.valid() && anchor.is_string()) {
          anchors_.emplace(std::string(anchor.as_string()), current.index());
        }
      }
      if (current.is_container()) {
        for (const ValueView child : current) {
          pending.push_back(child);
        }
      }
    }
  }

  // --- pools --------------------------------------------------------------

  [[nodiscard]] SchemaRun compile_schema_run(ValueView array, const char* keyword) {
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

  [[nodiscard]] SchemaRun compile_name_run(ValueView array, const char* keyword) {
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
        fail(ErrorCode::schema_invalid,
             std::string(keyword) + " entries must be strings");
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

  [[nodiscard]] SchemaRun compile_member_map(ValueView object, const char* keyword) {
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

  [[nodiscard]] SchemaRun compile_dependent_names(ValueView object, const char* keyword) {
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

  [[nodiscard]] SchemaRun compile_dependent_schemas(ValueView object, const char* keyword) {
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

  void append_text(std::uint32_t& off, std::uint32_t& len, std::string_view value) {
    off = static_cast<std::uint32_t>(schema_.text_.size());
    len = static_cast<std::uint32_t>(value.size());
    schema_.text_.insert(schema_.text_.end(), value.begin(), value.end());
  }

  // --- small conversions --------------------------------------------------

  [[nodiscard]] bool read_number(ValueView view, const char* keyword, double& out_value) {
    if (!view.is_number()) {
      fail(ErrorCode::schema_invalid, std::string(keyword) + " must be a number");
      return false;
    }
    out_value = view.as_number().as_real;
    return true;
  }

  [[nodiscard]] bool read_unsigned(ValueView view, const char* keyword,
                                   std::uint32_t& out_value) {
    if (!view.is_integer()) {
      fail(ErrorCode::schema_invalid,
           std::string(keyword) + " must be a non-negative integer");
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
      fail(ErrorCode::schema_invalid,
           std::string(keyword) + " must be a non-negative integer");
      return false;
    }
    out_value = static_cast<std::uint32_t>(number.as_signed);
    return true;
  }

  [[nodiscard]] bool type_bit(std::string_view name, std::uint8_t& out_bit) {
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

  void fail(ErrorCode code, std::string message) {
    if (failed_) {
      return;
    }
    failed_ = true;
    error_code_ = code;
    error_message_ = std::move(message);
  }

  const ParsedDocument& document_;
  CompileOptions options_;
  CompiledSchema schema_;

  std::unordered_map<std::uint32_t, SchemaId> compiled_;
  std::unordered_map<std::string, std::uint32_t> anchors_;
  bool anchors_built_ = false;

  bool failed_ = false;
  ErrorCode error_code_ = ErrorCode::none;
  std::string error_message_;
  std::size_t depth_ = 0;
};

// Convenience wrapper for the common case.
[[nodiscard]] inline Result<CompiledSchema> compile_schema(
    const ParsedDocument& document, ValueView root,
    const CompileOptions& options = CompileOptions()) {
  return SchemaCompiler::compile(document, root, options);
}

}  // namespace jsonschema
}  // namespace oascpp
