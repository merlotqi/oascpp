#pragma once

// Umbrella header for the JSON Schema engine, plus a small owning wrapper for
// the common case. The engine itself is usable without the wrapper: compile a
// schema against a document you own, then validate instances against it.

#include <memory>
#include <oascpp/schema/compiled_schema.hpp>
#include <oascpp/schema/compiler.hpp>
#include <oascpp/schema/dialect.hpp>
#include <oascpp/schema/validator.hpp>

namespace oascpp::schema {

// Owns the schema document together with its compiled graph, so a compiled
// schema outlives the document it was built from.
class Schema {
 public:
  Schema() = default;

  // Takes the document by value: the compiled graph borrows it, so it has to be
  // moved onto storage that outlives the graph.
  [[nodiscard]] static Result<Schema> compile(ParsedDocument document,
                                              const CompileOptions& options = CompileOptions());

  [[nodiscard]] bool valid() const noexcept;
  [[nodiscard]] const CompiledSchema& compiled() const noexcept;
  [[nodiscard]] const ParsedDocument& document() const noexcept;
  [[nodiscard]] ValueView root() const noexcept;
  [[nodiscard]] ValidationResult validate(ValueView instance, ValidationOptions options = ValidationOptions()) const;

 private:
  std::shared_ptr<ParsedDocument> document_;
  CompiledSchema compiled_;
};

}  // namespace oascpp::schema
