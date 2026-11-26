#pragma once

// Internal inline validation routines.

namespace oascpp {
namespace jsonschema {
namespace detail {

inline bool ValidationRun::check_composition(const SchemaNode& node, ValueView instance, ErrorPath* path) {
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

inline bool ValidationRun::check_values(const SchemaNode& node, ValueView instance, ErrorPath* path) {
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

}  // namespace detail
}  // namespace jsonschema
}  // namespace oascpp
