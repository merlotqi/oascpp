#pragma once

#include <cstddef>
#include <functional>
#include <oascpp/error.hpp>
#include <oascpp/schema/jsonschema/compiled_schema.hpp>
#include <oascpp/value.hpp>
#include <string_view>

namespace oascpp {
namespace jsonschema {

using PatternMatcher = std::function<bool(std::string_view, std::string_view)>;
using FormatChecker = std::function<bool(std::string_view, std::string_view)>;

struct ValidationOptions {
  std::size_t max_depth = 512;
  std::size_t max_errors = 100;
  PatternMatcher pattern_matcher;
  FormatChecker format_checker;
};

class Validator {
 public:
  explicit Validator(const CompiledSchema& schema, ValidationOptions options = ValidationOptions());

  [[nodiscard]] ValidationResult validate(ValueView instance) const;

 private:
  const CompiledSchema& schema_;
  ValidationOptions options_;
};

[[nodiscard]] ValidationResult validate(const CompiledSchema& schema, ValueView instance,
                                        ValidationOptions options = ValidationOptions());

}  // namespace jsonschema
}  // namespace oascpp
