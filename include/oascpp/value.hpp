#pragma once

// Core value model. A ParsedDocument owns one flat node arena; a ValueView is a
// two-field, trivially copyable view into it. Every layer above this one works
// exclusively with ValueView, so JSON backends only have to build an arena.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

#if (defined(_MSVC_LANG) && _MSVC_LANG < 201703L) || (!defined(_MSVC_LANG) && __cplusplus < 201703L)
#error "oascpp requires C++17 or later"
#endif

namespace oascpp {

enum class ValueKind : std::uint8_t {
  null_value = 0,
  boolean = 1,
  integer = 2,
  real = 3,
  string = 4,
  array = 5,
  object = 6,
};

[[nodiscard]] constexpr std::string_view to_string(ValueKind kind) noexcept {
  switch (kind) {
    case ValueKind::null_value:
      return "null";
    case ValueKind::boolean:
      return "boolean";
    case ValueKind::integer:
      return "integer";
    case ValueKind::real:
      return "real";
    case ValueKind::string:
      return "string";
    case ValueKind::array:
      return "array";
    case ValueKind::object:
      return "object";
  }
  return "unknown";
}

// Numeric access that never silently truncates. Integers wider than int64 keep
// is_unsigned == true and stay exact in as_unsigned; as_signed is then 0. Real
// values live in as_real.
struct NumberView {
  bool is_integer = false;
  bool is_unsigned = false;
  std::int64_t as_signed = 0;
  std::uint64_t as_unsigned = 0;
  double as_real = 0.0;
};

namespace detail {

// Node flags. Only node_flag_unsigned is produced by DocumentBuilder today; the
// remaining bits are reserved for exact number tokens and future needs.
enum NodeFlags : std::uint8_t {
  node_flag_none = 0,
  node_flag_unsigned = 1u << 0,
};

// Flat arena node. Reachable from adapters, but not part of the stable API.
struct Node {
  ValueKind kind = ValueKind::null_value;
  std::uint8_t flags = node_flag_none;
  std::uint32_t key_off = 0;
  std::uint32_t key_len = 0;
  std::uint32_t child_off = 0;
  std::uint32_t child_count = 0;
  std::uint32_t text_off = 0;
  std::uint32_t text_len = 0;
  std::uint64_t scalar = 0;
};

[[nodiscard]] inline double decode_double(std::uint64_t bits) noexcept {
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

[[nodiscard]] inline std::uint64_t encode_double(double value) noexcept {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

[[nodiscard]] inline std::int64_t decode_signed(std::uint64_t bits) noexcept {
  std::int64_t value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace detail

class ValueView;
class DocumentBuilder;

class ParsedDocument {
 public:
  ParsedDocument() = default;

  [[nodiscard]] ValueView root() const noexcept;

  [[nodiscard]] std::size_t node_count() const noexcept { return nodes_.size(); }
  [[nodiscard]] bool empty() const noexcept { return nodes_.empty(); }

 private:
  friend class ValueView;
  friend class DocumentBuilder;

  std::vector<detail::Node> nodes_;
  std::vector<std::uint32_t> children_;
  std::vector<char> chars_;
};

// A borrowed view. It stays valid as long as the owning ParsedDocument is alive
// (moving the document is fine, mutating it while a view exists is not).
class ValueView {
 public:
  constexpr ValueView() noexcept = default;
  constexpr ValueView(const ParsedDocument* doc, std::uint32_t index) noexcept : doc_(doc), index_(index) {}

  [[nodiscard]] bool valid() const noexcept { return doc_ != nullptr && index_ < doc_->nodes_.size(); }

  [[nodiscard]] ValueKind kind() const noexcept { return valid() ? node().kind : ValueKind::null_value; }

  [[nodiscard]] bool is_null() const noexcept { return kind() == ValueKind::null_value; }
  [[nodiscard]] bool is_boolean() const noexcept { return kind() == ValueKind::boolean; }
  [[nodiscard]] bool is_integer() const noexcept { return kind() == ValueKind::integer; }
  [[nodiscard]] bool is_real() const noexcept { return kind() == ValueKind::real; }
  [[nodiscard]] bool is_number() const noexcept { return kind() == ValueKind::integer || kind() == ValueKind::real; }
  [[nodiscard]] bool is_string() const noexcept { return kind() == ValueKind::string; }
  [[nodiscard]] bool is_array() const noexcept { return kind() == ValueKind::array; }
  [[nodiscard]] bool is_object() const noexcept { return kind() == ValueKind::object; }
  [[nodiscard]] bool is_container() const noexcept { return kind() == ValueKind::array || kind() == ValueKind::object; }

  // Preconditions are documented rather than enforced: callers check kind()
  // first. Accessors on a mismatched kind return a zero value.
  [[nodiscard]] bool as_boolean() const noexcept {
    return valid() && node().kind == ValueKind::boolean && node().scalar != 0;
  }

  [[nodiscard]] std::int64_t as_integer() const noexcept { return as_number().as_signed; }

  [[nodiscard]] double as_double() const noexcept { return as_number().as_real; }

  [[nodiscard]] NumberView as_number() const noexcept;

  [[nodiscard]] std::string_view as_string() const noexcept {
    if (!valid()) {
      return {};
    }
    const detail::Node& n = node();
    if (n.kind != ValueKind::string) {
      return {};
    }
    return std::string_view(doc_->chars_.data() + n.text_off, n.text_len);
  }

  // Member name when this view was reached by iterating an object; empty at the
  // root and for array elements.
  [[nodiscard]] std::string_view key() const noexcept {
    if (!valid()) {
      return {};
    }
    const detail::Node& n = node();
    if (n.key_len == 0) {
      return {};
    }
    return std::string_view(doc_->chars_.data() + n.key_off, n.key_len);
  }

  [[nodiscard]] std::size_t size() const noexcept {
    if (!valid()) {
      return 0;
    }
    const detail::Node& n = node();
    if (n.kind != ValueKind::array && n.kind != ValueKind::object) {
      return 0;
    }
    return n.child_count;
  }

  [[nodiscard]] bool empty() const noexcept { return size() == 0; }

  // Array element. Returns an invalid view when out of range or not an array.
  [[nodiscard]] ValueView at(std::size_t index) const noexcept {
    if (!valid()) {
      return {};
    }
    const detail::Node& n = node();
    if (n.kind != ValueKind::array || index >= n.child_count) {
      return {};
    }
    return child_at(index);
  }

  // Object member lookup. Linear today; becomes a sorted lookup once the arena
  // grows a key index. Returns an invalid view when absent.
  [[nodiscard]] ValueView find(std::string_view key_name) const noexcept {
    if (!valid()) {
      return {};
    }
    const detail::Node& n = node();
    if (n.kind != ValueKind::object) {
      return {};
    }
    for (std::uint32_t i = 0; i < n.child_count; ++i) {
      const std::uint32_t child = doc_->children_[static_cast<std::size_t>(n.child_off) + i];
      const detail::Node& candidate = doc_->nodes_[child];
      const std::string_view stored(doc_->chars_.data() + candidate.key_off, candidate.key_len);
      if (stored == key_name) {
        return ValueView(doc_, child);
      }
    }
    return {};
  }

  [[nodiscard]] bool contains(std::string_view key_name) const noexcept { return find(key_name).valid(); }

  class Iterator;

  // Iterates array elements and object members alike; use key() to read member
  // names. begin() and end() are both empty for non-containers.
  [[nodiscard]] Iterator begin() const noexcept;
  [[nodiscard]] Iterator end() const noexcept;

  friend bool operator==(const ValueView& lhs, const ValueView& rhs) noexcept {
    return lhs.doc_ == rhs.doc_ && lhs.index_ == rhs.index_;
  }
  friend bool operator!=(const ValueView& lhs, const ValueView& rhs) noexcept { return !(lhs == rhs); }

 private:
  [[nodiscard]] const detail::Node& node() const noexcept { return doc_->nodes_[index_]; }

  [[nodiscard]] ValueView child_at(std::size_t position) const noexcept {
    const detail::Node& n = node();
    return ValueView(doc_, doc_->children_[static_cast<std::size_t>(n.child_off) + position]);
  }

  const ParsedDocument* doc_ = nullptr;
  std::uint32_t index_ = 0;
};

// Forward iterator over the children of a container. It holds the container view
// plus a position, so it stays valid exactly as long as the parent view does.
class ValueView::Iterator {
 public:
  Iterator() noexcept = default;
  Iterator(ValueView container, std::size_t position) noexcept : container_(container), position_(position) {}

  [[nodiscard]] ValueView operator*() const noexcept { return container_.child_at(position_); }

  Iterator& operator++() noexcept {
    ++position_;
    return *this;
  }

  friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
    return lhs.container_ == rhs.container_ && lhs.position_ == rhs.position_;
  }
  friend bool operator!=(const Iterator& lhs, const Iterator& rhs) noexcept { return !(lhs == rhs); }

 private:
  ValueView container_;
  std::size_t position_ = 0;
};

inline ValueView::Iterator ValueView::begin() const noexcept {
  if (!is_container()) {
    return Iterator();
  }
  return Iterator(*this, 0);
}

inline ValueView::Iterator ValueView::end() const noexcept {
  if (!is_container()) {
    return Iterator();
  }
  return Iterator(*this, size());
}

inline NumberView ValueView::as_number() const noexcept {
  NumberView out;
  if (!valid()) {
    return out;
  }
  const detail::Node& n = node();
  switch (n.kind) {
    case ValueKind::integer: {
      out.is_integer = true;
      out.is_unsigned = (n.flags & detail::node_flag_unsigned) != 0;
      out.as_unsigned = n.scalar;
      const bool fits_signed = n.scalar <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
      out.as_signed =
          out.is_unsigned ? (fits_signed ? static_cast<std::int64_t>(n.scalar) : 0) : detail::decode_signed(n.scalar);
      out.as_real = out.is_unsigned ? static_cast<double>(n.scalar) : static_cast<double>(out.as_signed);
      break;
    }
    case ValueKind::real:
      out.is_integer = false;
      out.is_unsigned = false;
      out.as_real = detail::decode_double(n.scalar);
      break;
    default:
      break;
  }
  return out;
}

inline ValueView ParsedDocument::root() const noexcept {
  if (nodes_.empty()) {
    return ValueView();
  }
  return ValueView(this, 0);
}

}  // namespace oascpp
