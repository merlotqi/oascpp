#pragma once

// Umbrella header for the JSON Schema engine, plus a small owning wrapper for
// the common case. The engine itself is usable without the wrapper: compile a
// schema against a document you own, then validate instances against it.

#include <memory>
#include <utility>

#include "oascpp/schema/jsonschema/compiled_schema.hpp"
#include "oascpp/schema/jsonschema/compiler.hpp"
#include "oascpp/schema/jsonschema/dialect.hpp"
#include "oascpp/schema/jsonschema/validator.hpp"

namespace oascpp {
namespace jsonschema {

// Owns the schema document together with its compiled graph, so a compiled
// schema outlives the document it was built from.
class Schema {
 public:
  Schema() = default;

  // Takes the document by value: the compiled graph borrows it, so it has to be
  // moved onto storage that outlives the graph.
  [[nodiscard]] static Result<Schema> compile(ParsedDocument document,
                                              const CompileOptions& options = CompileOptions()) {
    Schema schema;
    schema.document_ = std::make_shared<ParsedDocument>(std::move(document));
    Result<CompiledSchema> compiled = SchemaCompiler::compile(*schema.document_, schema.document_->root(), options);
    if (!compiled) {
      return compiled.error();
    }
    schema.compiled_ = std::move(compiled.value());
    return schema;
  }

  [[nodiscard]] bool valid() const noexcept { return compiled_.valid(); }
  [[nodiscard]] const CompiledSchema& compiled() const noexcept { return compiled_; }
  [[nodiscard]] const ParsedDocument& document() const noexcept { return *document_; }
  [[nodiscard]] ValueView root() const noexcept { return document_->root(); }

  [[nodiscard]] ValidationResult validate(ValueView instance, ValidationOptions options = ValidationOptions()) const {
    return jsonschema::validate(compiled_, instance, std::move(options));
  }

 private:
  std::shared_ptr<ParsedDocument> document_;
  CompiledSchema compiled_;
};

}  // namespace jsonschema
}  // namespace oascpp
