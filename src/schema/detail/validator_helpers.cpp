#include <oascpp/document_builder.hpp>

#include "validation_run.hpp"

namespace oascpp::schema {
namespace detail {

[[nodiscard]] SchemaId ValidationRun::find_property(const SchemaRun& run, std::string_view key) const {
  for (std::uint32_t index = 0; index < run.count; ++index) {
    const MemberEntry& entry = schema_.member_at(run, index);
    if (schema_.key(entry) == key) {
      return entry.schema;
    }
  }
  return no_schema;
}

bool ValidationRun::check_member(SchemaId id, ValueView value, std::string_view name, ErrorPath* path) {
  if (path != nullptr) {
    path->push_key(name);
  }
  const bool ok = check(id, value, path);
  if (path != nullptr) {
    path->pop_back();
  }
  return ok;
}

bool ValidationRun::check_element(SchemaId id, ValueView value, std::size_t index, ErrorPath* path) {
  if (path != nullptr) {
    path->push_index(index);
  }
  const bool ok = check(id, value, path);
  if (path != nullptr) {
    path->pop_back();
  }
  return ok;
}

[[nodiscard]] ValueView ValidationRun::make_string_view(std::string_view text) {
  std::unique_ptr<ParsedDocument> document(new ParsedDocument());
  DocumentBuilder builder;
  static_cast<void>(builder.make_string(text));
  *document = builder.finish();
  scratch_.push_back(std::move(document));
  return scratch_.back()->root();
}

[[nodiscard]] bool ValidationRun::is_multiple_of(double value, double divisor) const {
  if (!(divisor > 0.0) || !std::isfinite(value)) {
    return false;
  }
  const double quotient = value / divisor;
  if (!std::isfinite(quotient)) {
    return false;
  }
  const double nearest = std::round(quotient);
  const double tolerance = 1e-9 * std::max(1.0, std::fabs(quotient));
  return std::fabs(quotient - nearest) <= tolerance;
}

void ValidationRun::add(ErrorPath* path, ErrorCode code, const char* message) {
  if (path == nullptr) {
    return;
  }
  if (result_.error_count() >= options_.max_errors) {
    return;
  }
  ValidationError error;
  error.code = code;
  error.message = message;
  error.instance_path = *path;
  result_.add_error(std::move(error));
}

}  // namespace detail
}  // namespace oascpp::schema
