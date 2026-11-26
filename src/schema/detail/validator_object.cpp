#include "validation_run.hpp"

namespace oascpp::schema {
namespace detail {

bool ValidationRun::check_object(const SchemaNode& node, ValueView instance, ErrorPath* path) {
  if (!instance.is_object()) {
    return true;
  }
  const std::size_t size = instance.size();
  bool ok = true;
  if (has_keyword(node.present, Keyword::min_properties) && size < node.min_properties) {
    add(path, ErrorCode::object_min_properties, "object has too few properties");
    ok = false;
  }
  if (has_keyword(node.present, Keyword::max_properties) && size > node.max_properties) {
    add(path, ErrorCode::object_max_properties, "object has too many properties");
    ok = false;
  }

  if (has_keyword(node.present, Keyword::required)) {
    for (std::uint32_t index = 0; index < node.required.count; ++index) {
      const std::string_view name = schema_.name(schema_.name_at(node.required, index));
      if (!instance.contains(name)) {
        if (path != nullptr) {
          path->push_key(name);
        }
        add(path, ErrorCode::object_required, "required property is missing");
        if (path != nullptr) {
          path->pop_back();
        }
        ok = false;
      }
    }
  }

  const bool has_properties = has_keyword(node.present, Keyword::properties);
  const bool has_pattern_properties =
      has_keyword(node.present, Keyword::pattern_properties) && options_.pattern_matcher;
  for (const ValueView member : instance) {
    const std::string_view name = member.key();
    bool covered = false;
    if (has_properties) {
      const SchemaId declared = find_property(node.properties, name);
      if (declared != no_schema) {
        covered = true;
        ok = check_member(declared, member, name, path) && ok;
      }
    }
    if (has_pattern_properties) {
      for (std::uint32_t index = 0; index < node.pattern_properties.count; ++index) {
        const MemberEntry& entry = schema_.member_at(node.pattern_properties, index);
        if (options_.pattern_matcher(schema_.key(entry), name)) {
          covered = true;
          ok = check_member(entry.schema, member, name, path) && ok;
        }
      }
    }
    if (!covered && has_keyword(node.present, Keyword::additional_properties)) {
      ok = check_member(node.additional_properties, member, name, path) && ok;
    }
  }

  if (has_keyword(node.present, Keyword::property_names)) {
    for (const ValueView member : instance) {
      const std::string_view name = member.key();
      const ValueView name_instance = make_string_view(name);
      ok = check_member(node.property_names, name_instance, name, path) && ok;
    }
  }

  if (has_keyword(node.present, Keyword::dependent_required)) {
    for (std::uint32_t index = 0; index < node.dependent_required.count; ++index) {
      const DependentEntry& entry = schema_.dependent_at(node.dependent_required, index);
      const std::string_view trigger = schema_.text(entry.key_off, entry.key_len);
      if (!instance.contains(trigger)) {
        continue;
      }
      for (std::uint32_t name_index = 0; name_index < entry.run.count; ++name_index) {
        const std::string_view name = schema_.name(schema_.name_at(entry.run, name_index));
        if (!instance.contains(name)) {
          add(path, ErrorCode::object_dependent_required, "property required by dependentRequired is missing");
          ok = false;
        }
      }
    }
  }

  if (has_keyword(node.present, Keyword::dependent_schemas)) {
    for (std::uint32_t index = 0; index < node.dependent_schemas.count; ++index) {
      const DependentEntry& entry = schema_.dependent_at(node.dependent_schemas, index);
      const std::string_view trigger = schema_.text(entry.key_off, entry.key_len);
      if (!instance.contains(trigger)) {
        continue;
      }
      ok = check(schema_.subschema_at(entry.run, 0), instance, path) && ok;
    }
  }

  // unevaluatedProperties needs annotation tracking and is not interpreted yet.
  return ok;
}

}  // namespace detail
}  // namespace oascpp::schema
