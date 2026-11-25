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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
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

  void reserve(std::size_t node_count, std::size_t char_count = 0) {
    nodes_.reserve(node_count);
    first_child_.reserve(node_count);
    last_child_.reserve(node_count);
    next_sibling_.reserve(node_count);
    chars_.reserve(char_count);
  }

  [[nodiscard]] NodeId make_null() { return push(ValueKind::null_value); }

  [[nodiscard]] NodeId make_boolean(bool value) {
    const NodeId id = push(ValueKind::boolean);
    nodes_[id].scalar = value ? 1u : 0u;
    return id;
  }

  [[nodiscard]] NodeId make_integer(std::int64_t value) {
    const NodeId id = push(ValueKind::integer);
    nodes_[id].scalar = static_cast<std::uint64_t>(value);
    return id;
  }

  [[nodiscard]] NodeId make_unsigned(std::uint64_t value) {
    const NodeId id = push(ValueKind::integer);
    nodes_[id].flags |= detail::node_flag_unsigned;
    nodes_[id].scalar = value;
    return id;
  }

  [[nodiscard]] NodeId make_real(double value) {
    const NodeId id = push(ValueKind::real);
    nodes_[id].scalar = detail::encode_double(value);
    return id;
  }

  [[nodiscard]] NodeId make_string(std::string_view value) {
    const NodeId id = push(ValueKind::string);
    store_text(nodes_[id], value);
    return id;
  }

  [[nodiscard]] NodeId make_array() { return push(ValueKind::array); }
  [[nodiscard]] NodeId make_object() { return push(ValueKind::object); }

  // Appends an element to an array, preserving creation order.
  void append(NodeId parent, NodeId child) {
    assert(parent < nodes_.size());
    assert(child < nodes_.size());
    assert(nodes_[parent].kind == ValueKind::array);
    link(parent, child);
  }

  // Appends a member to an object. Empty member names are legal JSON, so the
  // array and object paths are separate overloads rather than a defaulted key.
  void append_member(NodeId parent, std::string_view key, NodeId child) {
    assert(parent < nodes_.size());
    assert(child < nodes_.size());
    assert(nodes_[parent].kind == ValueKind::object);
    detail::Node& target = nodes_[child];
    target.key_off = static_cast<std::uint32_t>(chars_.size());
    target.key_len = static_cast<std::uint32_t>(key.size());
    chars_.insert(chars_.end(), key.begin(), key.end());
    link(parent, child);
  }

  [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }
  [[nodiscard]] bool empty() const noexcept { return nodes_.empty(); }

  ParsedDocument finish() {
    ParsedDocument doc;
    doc.nodes_ = std::move(nodes_);
    doc.chars_ = std::move(chars_);
    doc.children_.clear();
    doc.children_.reserve(doc.nodes_.size());

    for (std::size_t id = 0; id < doc.nodes_.size(); ++id) {
      detail::Node& node = doc.nodes_[id];
      node.child_off = static_cast<std::uint32_t>(doc.children_.size());
      node.child_count = 0;
      for (NodeId child = first_child_[id]; child != invalid_node; child = next_sibling_[child]) {
        doc.children_.push_back(child);
        ++node.child_count;
      }
    }

    // NUL sentinel: every stored offset can then be dereferenced with data()
    // even for empty strings, and callers get NUL-terminated text for free.
    doc.chars_.push_back('\0');

    nodes_.clear();
    chars_.clear();
    first_child_.clear();
    last_child_.clear();
    next_sibling_.clear();
    return doc;
  }

 private:
  [[nodiscard]] NodeId push(ValueKind kind) {
    assert(nodes_.size() < static_cast<std::size_t>(invalid_node));
    const NodeId id = static_cast<NodeId>(nodes_.size());
    detail::Node node;
    node.kind = kind;
    nodes_.push_back(node);
    first_child_.push_back(invalid_node);
    last_child_.push_back(invalid_node);
    next_sibling_.push_back(invalid_node);
    return id;
  }

  void store_text(detail::Node& node, std::string_view text) {
    node.text_off = static_cast<std::uint32_t>(chars_.size());
    node.text_len = static_cast<std::uint32_t>(text.size());
    chars_.insert(chars_.end(), text.begin(), text.end());
  }

  void link(NodeId parent, NodeId child) {
    if (first_child_[parent] == invalid_node) {
      first_child_[parent] = child;
    } else {
      next_sibling_[last_child_[parent]] = child;
    }
    last_child_[parent] = child;
  }

  std::vector<detail::Node> nodes_;
  std::vector<char> chars_;
  std::vector<NodeId> first_child_;
  std::vector<NodeId> last_child_;
  std::vector<NodeId> next_sibling_;
};

}  // namespace oascpp
