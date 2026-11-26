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
#include <oascpp/error.hpp>
#include <oascpp/json_pointer.hpp>
#include <oascpp/result.hpp>
#include <oascpp/schema/compiled_schema.hpp>
#include <oascpp/schema/dialect.hpp>
#include <oascpp/value.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oascpp::schema {

struct CompileOptions {
  Dialect dialect = Dialect::draft2020_12;
  std::size_t max_schemas = 4096;
  std::size_t max_depth = 256;
};

class SchemaCompiler {
 public:
  [[nodiscard]] static Result<CompiledSchema> compile(const ParsedDocument& document, ValueView root,
                                                      const CompileOptions& options = CompileOptions());

 private:
  SchemaCompiler(const ParsedDocument& document, const CompileOptions& options);

  // --- driver -------------------------------------------------------------

  void compile_root(ValueView root);
  void build_source_locations(ValueView root);
  [[nodiscard]] std::string source_path(std::uint32_t source) const;
  [[nodiscard]] SchemaId compile_schema(ValueView view);
  void fill_node(SchemaNode& node, ValueView view);

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

  [[nodiscard]] bool read_number(ValueView view, const char* keyword, NumberView& out_value);

  [[nodiscard]] bool read_unsigned(ValueView view, const char* keyword, std::uint64_t& out_value);

  [[nodiscard]] bool type_bit(std::string_view name, std::uint8_t& out_bit);

  void fail(ErrorCode code, std::string message);

  const ParsedDocument& document_;
  CompileOptions options_;
  CompiledSchema schema_;

  std::unordered_map<std::uint32_t, SchemaId> compiled_;
  std::unordered_map<std::string, std::uint32_t> anchors_;
  struct SourceLocation {
    std::uint32_t parent = no_schema;
    std::uint32_t position = 0;
  };
  std::vector<SourceLocation> source_locations_;
  bool anchors_built_ = false;

  bool failed_ = false;
  ErrorCode error_code_ = ErrorCode::none;
  std::string error_message_;
  std::size_t depth_ = 0;
};

// Convenience wrapper for the common case.
[[nodiscard]] Result<CompiledSchema> compile_schema(const ParsedDocument& document, ValueView root,
                                                    const CompileOptions& options = CompileOptions());

}  // namespace oascpp::schema
