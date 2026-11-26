#pragma once

// Schema compiler: turns a JSON Schema document into the flat, immutable graph
// described in compiled_schema.hpp.
//
// Design notes:
//   * every source schema node is compiled exactly once and memoised by its
//     arena index, so recursive $ref graphs terminate;
//   * $ref is resolved eagerly at compile time; unresolvable references are
//     compile errors rather than runtime surprises;
//   * the source document is borrowed by pointer and must outlive the result;
//   * pools are appended to in contiguous batches, so a SchemaRun never
//     interleaves with a nested compile.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "oascpp/error.hpp"
#include "oascpp/json_pointer.hpp"
#include "oascpp/result.hpp"
#include "oascpp/schema/jsonschema/compiled_schema.hpp"
#include "oascpp/schema/jsonschema/dialect.hpp"
#include "oascpp/value.hpp"

namespace oascpp {
namespace jsonschema {

struct CompileOptions {
  Dialect dialect = Dialect::draft2020_12;
  std::size_t max_schemas = 4096;
  std::size_t max_depth = 256;
};

class SchemaCompiler {
 public:
  [[nodiscard]] static Result<CompiledSchema> compile(const ParsedDocument& document, ValueView root,
                                                      const CompileOptions& options = CompileOptions()) {
    SchemaCompiler compiler(document, options);
    compiler.compile_root(root);
    if (compiler.failed_) {
      return Error{compiler.error_code_, std::move(compiler.error_message_)};
    }
    return std::move(compiler.schema_);
  }

 private:
  SchemaCompiler(const ParsedDocument& document, const CompileOptions& options)
      : document_(document), options_(options) {
    schema_.document_ = &document_;
    schema_.profile_ = profile_for(options_.dialect);
    if (!schema_.profile_.supported) {
      fail(ErrorCode::unsupported_dialect, "this dialect is not supported yet");
    }
  }

  // --- driver -------------------------------------------------------------

  void compile_root(ValueView root) {
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
    // Sentinel so text() can always form a valid string_view from data() + off.
    schema_.text_.push_back('\0');
  }

  [[nodiscard]] SchemaId compile_schema(ValueView view) {
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

    // Allocate before descending so a recursive $ref can find this node.
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

  void fill_node(SchemaNode& node, ValueView view) {
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

  // --- keyword readers ----------------------------------------------------

  void read_refs(SchemaNode& node, ValueView view);

  void read_type(SchemaNode& node, ValueView view);

  void read_numeric(SchemaNode& node, ValueView view);

  void read_string_keywords(SchemaNode& node, ValueView view);

  void read_array_keywords(SchemaNode& node, ValueView view);

  void read_object_keywords(SchemaNode& node, ValueView view);

  void read_composition(SchemaNode& node, ValueView view);

  void read_value_keywords(SchemaNode& node, ValueView view);

  // --- reference resolution ----------------------------------------------

  [[nodiscard]] SchemaId resolve_reference(std::string_view ref);

  [[nodiscard]] ValueView descend(ValueView container, const std::string& token);

  void build_anchor_index();

  // --- pools --------------------------------------------------------------

  [[nodiscard]] SchemaRun compile_schema_run(ValueView array, const char* keyword);

  [[nodiscard]] SchemaRun compile_name_run(ValueView array, const char* keyword);

  [[nodiscard]] SchemaRun compile_member_map(ValueView object, const char* keyword);

  [[nodiscard]] SchemaRun compile_dependent_names(ValueView object, const char* keyword);

  [[nodiscard]] SchemaRun compile_dependent_schemas(ValueView object, const char* keyword);

  void append_text(std::uint32_t& off, std::uint32_t& len, std::string_view value);

  // --- small conversions --------------------------------------------------

  [[nodiscard]] bool read_number(ValueView view, const char* keyword, double& out_value);

  [[nodiscard]] bool read_unsigned(ValueView view, const char* keyword, std::uint32_t& out_value);

  [[nodiscard]] bool type_bit(std::string_view name, std::uint8_t& out_bit);

  void fail(ErrorCode code, std::string message);

  const ParsedDocument& document_;
  CompileOptions options_;
  CompiledSchema schema_;

  std::unordered_map<std::uint32_t, SchemaId> compiled_;
  std::unordered_map<std::string, std::uint32_t> anchors_;
  bool anchors_built_ = false;

  bool failed_ = false;
  ErrorCode error_code_ = ErrorCode::none;
  std::string error_message_;
  std::size_t depth_ = 0;
};

// Convenience wrapper for the common case.
[[nodiscard]] inline Result<CompiledSchema> compile_schema(const ParsedDocument& document, ValueView root,
                                                           const CompileOptions& options = CompileOptions()) {
  return SchemaCompiler::compile(document, root, options);
}

}  // namespace jsonschema
}  // namespace oascpp

#include "oascpp/schema/jsonschema/detail/compiler_keywords.hpp"
#include "oascpp/schema/jsonschema/detail/compiler_references.hpp"
#include "oascpp/schema/jsonschema/detail/compiler_storage.hpp"
