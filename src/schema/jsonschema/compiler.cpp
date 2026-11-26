#include <oascpp/schema/jsonschema/compiler.hpp>
#include <utility>

namespace oascpp {
namespace jsonschema {

Result<CompiledSchema> SchemaCompiler::compile(const ParsedDocument& document, ValueView root,
                                               const CompileOptions& options) {
  SchemaCompiler compiler(document, options);
  compiler.compile_root(root);
  if (compiler.failed_) {
    return Error{compiler.error_code_, std::move(compiler.error_message_)};
  }
  return std::move(compiler.schema_);
}

SchemaCompiler::SchemaCompiler(const ParsedDocument& document, const CompileOptions& options)
    : document_(document), options_(options) {
  schema_.document_ = &document_;
  schema_.profile_ = profile_for(options_.dialect);
  if (!schema_.profile_.supported) {
    fail(ErrorCode::unsupported_dialect, "this dialect is not supported yet");
  }
}

void SchemaCompiler::compile_root(ValueView root) {
  if (failed_) {
    return;
  }
  if (!root.valid()) {
    fail(ErrorCode::schema_invalid, "schema document is empty");
    return;
  }
  const SchemaId root_id = compile_schema(root);
  if (failed_) {
    return;
  }
  schema_.root_ = root_id;
  // Sentinel so text() can always form a valid view from data() + offset.
  schema_.text_.push_back('\0');
}

SchemaId SchemaCompiler::compile_schema(ValueView view) {
  if (failed_) {
    return no_schema;
  }
  if (!view.valid()) {
    fail(ErrorCode::schema_invalid, "schema is missing");
    return no_schema;
  }

  const std::uint32_t source = view.index();
  const auto existing = compiled_.find(source);
  if (existing != compiled_.end()) {
    return existing->second;
  }
  if (depth_ >= options_.max_depth) {
    fail(ErrorCode::schema_invalid, "schema nesting is too deep");
    return no_schema;
  }
  if (schema_.nodes_.size() >= options_.max_schemas) {
    fail(ErrorCode::schema_invalid, "schema has too many nodes");
    return no_schema;
  }

  // Allocate before descending so recursive references can find this node.
  const SchemaId id = static_cast<SchemaId>(schema_.nodes_.size());
  schema_.nodes_.push_back(SchemaNode{});
  compiled_.emplace(source, id);

  SchemaNode node;
  node.source = source;
  ++depth_;
  fill_node(node, view);
  --depth_;
  if (failed_) {
    return no_schema;
  }
  schema_.nodes_[id] = node;
  return id;
}

void SchemaCompiler::fill_node(SchemaNode& node, ValueView view) {
  if (view.is_boolean()) {
    if (!view.as_boolean()) {
      node.flags |= schema_flag_false;
    }
    return;
  }
  if (!view.is_object()) {
    fail(ErrorCode::schema_invalid, "schema must be an object or a boolean");
    return;
  }

  read_refs(node, view);
  read_type(node, view);
  read_numeric(node, view);
  read_string_keywords(node, view);
  read_array_keywords(node, view);
  read_object_keywords(node, view);
  read_composition(node, view);
  read_value_keywords(node, view);
}

Result<CompiledSchema> compile_schema(const ParsedDocument& document, ValueView root, const CompileOptions& options) {
  return SchemaCompiler::compile(document, root, options);
}

}  // namespace jsonschema
}  // namespace oascpp
