#include <oascpp/schema/compiler.hpp>

namespace oascpp::schema {

void SchemaCompiler::read_refs(SchemaNode& node, ValueView view) {
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

void SchemaCompiler::read_type(SchemaNode& node, ValueView view) {
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
      if (!schema_.profile_.type_may_be_array) {
        fail(ErrorCode::schema_invalid, "type arrays are not supported by this dialect");
        return;
      }
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
  if (nullable.valid() && schema_.profile_.allows_nullable && nullable.is_boolean() && nullable.as_boolean()) {
    if (node.types != 0) {
      node.types |= type_bit_null;
    }
  }
}

void SchemaCompiler::read_numeric(SchemaNode& node, ValueView view) {
  if (failed_) {
    return;
  }
  const ValueView multiple_of = view.find("multipleOf");
  if (multiple_of.valid()) {
    NumberView value;
    if (!read_number(multiple_of, "multipleOf", value)) {
      return;
    }
    if (oascpp::detail::compare_numbers(value, NumberView{}) != oascpp::detail::NumberOrder::greater) {
      fail(ErrorCode::schema_invalid, "multipleOf must be greater than zero");
      return;
    }
    node.multiple_of = value;
    node.present |= keyword_bit(Keyword::multiple_of);
  }

  const ValueView minimum = view.find("minimum");
  if (minimum.valid()) {
    NumberView value;
    if (!read_number(minimum, "minimum", value)) {
      return;
    }
    node.minimum = value;
    node.present |= keyword_bit(Keyword::minimum);
  }

  const ValueView exclusive_minimum = view.find("exclusiveMinimum");
  if (exclusive_minimum.valid()) {
    if (exclusive_minimum.is_boolean()) {
      if (!schema_.profile_.exclusive_is_boolean) {
        fail(ErrorCode::schema_invalid, "exclusiveMinimum must be a number in this dialect");
        return;
      }
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
      if (schema_.profile_.exclusive_is_boolean) {
        fail(ErrorCode::schema_invalid, "exclusiveMinimum must be a boolean in this dialect");
        return;
      }
      NumberView value;
      if (!read_number(exclusive_minimum, "exclusiveMinimum", value)) {
        return;
      }
      node.exclusive_minimum = value;
      node.present |= keyword_bit(Keyword::exclusive_minimum);
    }
  }

  const ValueView maximum = view.find("maximum");
  if (maximum.valid()) {
    NumberView value;
    if (!read_number(maximum, "maximum", value)) {
      return;
    }
    node.maximum = value;
    node.present |= keyword_bit(Keyword::maximum);
  }

  const ValueView exclusive_maximum = view.find("exclusiveMaximum");
  if (exclusive_maximum.valid()) {
    if (exclusive_maximum.is_boolean()) {
      if (!schema_.profile_.exclusive_is_boolean) {
        fail(ErrorCode::schema_invalid, "exclusiveMaximum must be a number in this dialect");
        return;
      }
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
      if (schema_.profile_.exclusive_is_boolean) {
        fail(ErrorCode::schema_invalid, "exclusiveMaximum must be a boolean in this dialect");
        return;
      }
      NumberView value;
      if (!read_number(exclusive_maximum, "exclusiveMaximum", value)) {
        return;
      }
      node.exclusive_maximum = value;
      node.present |= keyword_bit(Keyword::exclusive_maximum);
    }
  }
}

}  // namespace oascpp::schema
