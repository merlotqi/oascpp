#pragma once

// JSON Schema and OpenAPI dialect profiles. One interpreter serves every
// dialect; what differs is data:
// which keywords exist, whether $ref siblings apply, how exclusive bounds are
// spelled, and whether nullable is a keyword. Keeping this as a table rather
// than a second compiler is deliberate.

#include <cstdint>
#include <string_view>

#include "oascpp/value.hpp"

namespace oascpp {
namespace jsonschema {

enum class Dialect : std::uint8_t {
  draft2020_12 = 0,
  draft2019_09,
  draft7,
  draft4,
  openapi30,
  openapi31,
};

// Every flag below is something the interpreter has to branch on at runtime, so
// the profile is plain data that can be selected per compiled schema.
struct DialectProfile {
  Dialect dialect = Dialect::draft2020_12;
  bool supported = false;

  // OpenAPI 3.0 lets a schema admit null through `nullable: true` instead of a
  // type union.
  bool allows_nullable = false;

  // In draft 4 through 7 and OpenAPI 3.0 a $ref replaces its siblings; in 3.1
  // and 2019-09+ the other keywords keep applying.
  bool ref_siblings_apply = false;

  // draft4 spells exclusive bounds as `exclusiveMinimum: true` next to
  // `minimum`, while draft6+ uses a numeric `exclusiveMinimum`.
  bool exclusive_is_boolean = false;

  // unevaluatedProperties / unevaluatedItems exist from 2019-09 on.
  bool has_unevaluated = false;

  // $dynamicRef and $dynamicAnchor exist from 2020-12 on.
  bool has_dynamic_ref = false;

  // 2020-12 renamed `definitions` to `$defs`.
  std::string_view defs_keyword = "$defs";
  std::string_view legacy_definitions_keyword = "definitions";

  // draft4 allowed `type` to hold only one name, but that restriction is not
  // worth a separate code path; the array form is accepted everywhere.
  bool type_may_be_array = true;

  // In 3.0 the `items` keyword must be an object, and tuple validation moved to
  // `additionalItems`; from 2020-12 `items` is a single schema and tuples use
  // `prefixItems`.
  bool has_prefix_items = true;
  bool has_if_then_else = true;
  bool has_contains = true;
  bool has_const = true;
  bool has_dependent_keywords = true;
};

[[nodiscard]] constexpr DialectProfile profile_for(Dialect dialect) noexcept {
  DialectProfile profile;
  profile.dialect = dialect;
  switch (dialect) {
    case Dialect::draft2020_12:
      profile.supported = true;
      profile.ref_siblings_apply = true;
      profile.has_unevaluated = true;
      profile.has_dynamic_ref = true;
      profile.defs_keyword = "$defs";
      break;
    case Dialect::draft2019_09:
      profile.ref_siblings_apply = true;
      profile.has_unevaluated = true;
      profile.defs_keyword = "$defs";
      break;
    case Dialect::draft7:
      profile.defs_keyword = "definitions";
      profile.has_dynamic_ref = false;
      break;
    case Dialect::draft4:
      profile.exclusive_is_boolean = true;
      profile.has_prefix_items = false;
      profile.has_if_then_else = false;
      profile.has_contains = false;
      profile.has_const = false;
      profile.has_dependent_keywords = false;
      profile.defs_keyword = "definitions";
      break;
    case Dialect::openapi30:
      profile.allows_nullable = true;
      profile.exclusive_is_boolean = true;
      profile.has_prefix_items = false;
      profile.has_if_then_else = false;
      profile.has_contains = false;
      profile.has_const = false;
      profile.has_dependent_keywords = false;
      profile.defs_keyword = "definitions";
      break;
    case Dialect::openapi31:
      profile.supported = true;
      profile.ref_siblings_apply = true;
      profile.has_unevaluated = true;
      profile.has_dynamic_ref = true;
      profile.defs_keyword = "$defs";
      break;
  }
  return profile;
}

}  // namespace jsonschema
}  // namespace oascpp
