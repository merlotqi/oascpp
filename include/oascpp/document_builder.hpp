#pragma once

// Arena builder. JSON adapters translate their backend DOM into a ParsedDocument
// through this API, and tests construct documents directly with it.
//
// Usage contract:
//   * the first node created becomes the document root;
//   * nodes may be created in any order, so a child's subtree can be built
//     before the child is attached to its parent;
//   * every node may be attached at most once;
//   * finish() consumes the builder.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "oascpp/value.hpp"

namespace oascpp {

class DocumentBuilder {
 public:
  using NodeId = std::uint32_t;

  static constexpr NodeId invalid_node = static_cast<NodeId>(-1);

  DocumentBuilder() = default;

  DocumentBuilder(const DocumentBuilder&) = delete;
  DocumentBuilder& operator=(const DocumentBuilder&) = delete;
  DocumentBuilder(DocumentBuilder&&) = default;
  DocumentBuilder& operator=(DocumentBuilder&&) = default;

  void reserve(std::size_t node_count, std::size_t char_count = 0);

  [[nodiscard]] NodeId make_null();
  [[nodiscard]] NodeId make_boolean(bool value);
  [[nodiscard]] NodeId make_integer(std::int64_t value);
  [[nodiscard]] NodeId make_unsigned(std::uint64_t value);
  [[nodiscard]] NodeId make_real(double value);
  [[nodiscard]] NodeId make_string(std::string_view value);
  [[nodiscard]] NodeId make_array();
  [[nodiscard]] NodeId make_object();

  // Appends an element to an array, preserving creation order.
  void append(NodeId parent, NodeId child);

  // Appends a member to an object. Empty member names are legal JSON, so the
  // array and object paths are separate overloads rather than a defaulted key.
  void append_member(NodeId parent, std::string_view key, NodeId child);

  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  ParsedDocument finish();

 private:
  [[nodiscard]] NodeId push(ValueKind kind);
  void store_text(detail::Node& node, std::string_view text);
  void link(NodeId parent, NodeId child);

  std::vector<detail::Node> nodes_;
  std::vector<char> chars_;
  std::vector<NodeId> first_child_;
  std::vector<NodeId> last_child_;
  std::vector<NodeId> next_sibling_;
};

}  // namespace oascpp
