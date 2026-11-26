#include <oascpp/schema/compiler.hpp>

namespace oascpp::schema {

void SchemaCompiler::read_object_keywords(SchemaNode& node, ValueView view) {
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

void SchemaCompiler::read_composition(SchemaNode& node, ValueView view) {
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

void SchemaCompiler::read_value_keywords(SchemaNode& node, ValueView view) {
  if (failed_) {
    return;
  }
  const ValueView enum_values = view.find("enum");
  if (enum_values.valid()) {
    if (!enum_values.is_array()) {
      fail(ErrorCode::schema_invalid, "enum must be an array");
      return;
    }
    if (enum_values.empty()) {
      fail(ErrorCode::schema_invalid, "enum must contain at least one value");
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

}  // namespace oascpp::schema
