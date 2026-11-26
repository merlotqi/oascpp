#include <oascpp/schema/compiler.hpp>

namespace oascpp::schema {

void SchemaCompiler::read_string_keywords(SchemaNode& node, ValueView view) {
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

void SchemaCompiler::read_array_keywords(SchemaNode& node, ValueView view) {
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

}  // namespace oascpp::schema
