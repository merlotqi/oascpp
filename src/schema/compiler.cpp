#include <oascpp/schema/compiler.hpp>
#include <utility>

namespace oascpp::schema {

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
  build_source_locations(root);
  const SchemaId root_id = compile_schema(root);
  if (failed_) {
    return;
  }
  schema_.root_ = root_id;
  // Sentinel so text() can always form a valid view from data() + offset.
  schema_.text_.push_back('\0');
}

void SchemaCompiler::build_source_locations(ValueView root) {
  struct Pending {
    ValueView view;
    std::uint32_t parent;
    std::uint32_t position;
  };

  source_locations_.resize(document_.node_count());
  std::vector<Pending> pending;
  pending.push_back({root, no_schema, 0});
  while (!pending.empty()) {
    Pending current = std::move(pending.back());
    pending.pop_back();
    source_locations_[current.view.index()] = {current.parent, current.position};

    if (current.view.is_object()) {
      std::uint32_t position = 0;
      for (const ValueView member : current.view) {
        pending.push_back({member, current.view.index(), position++});
      }
    } else if (current.view.is_array()) {
      for (std::size_t index = 0; index < current.view.size(); ++index) {
        pending.push_back({current.view.at(index), current.view.index(), static_cast<std::uint32_t>(index)});
      }
    }
  }
}

std::string SchemaCompiler::source_path(std::uint32_t source) const {
  std::vector<std::string> tokens;
  while (source < source_locations_.size()) {
    const SourceLocation& location = source_locations_[source];
    if (location.parent == no_schema) {
      break;
    }
    const ValueView parent(&document_, location.parent);
    if (parent.is_object()) {
      tokens.emplace_back(ValueView(&document_, source).key());
    } else {
      tokens.push_back(std::to_string(location.position));
    }
    source = location.parent;
  }

  std::string path;
  for (auto token = tokens.rbegin(); token != tokens.rend(); ++token) {
    path.push_back('/');
    append_escaped_pointer_token(path, *token);
  }
  return path;
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
  if (source < source_locations_.size()) {
    const std::string path = source_path(source);
    append_text(node.schema_path_off, node.schema_path_len, path);
  }
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

}  // namespace oascpp::schema
