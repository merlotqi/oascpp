#include <memory>
#include <oascpp/schema/schema.hpp>
#include <utility>

namespace oascpp::schema {

Result<Schema> Schema::compile(ParsedDocument document, const CompileOptions& options) {
  Schema schema;
  schema.document_ = std::make_shared<ParsedDocument>(std::move(document));
  Result<CompiledSchema> compiled = SchemaCompiler::compile(*schema.document_, schema.document_->root(), options);
  if (!compiled) {
    return compiled.error();
  }
  schema.compiled_ = std::move(compiled.value());
  return schema;
}

bool Schema::valid() const noexcept { return compiled_.valid(); }

const CompiledSchema& Schema::compiled() const noexcept { return compiled_; }

const ParsedDocument& Schema::document() const noexcept { return *document_; }

ValueView Schema::root() const noexcept { return document_->root(); }

ValidationResult Schema::validate(ValueView instance, ValidationOptions options) const {
  return schema::validate(compiled_, instance, std::move(options));
}

}  // namespace oascpp::schema
