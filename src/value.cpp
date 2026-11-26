#include <cmath>
#include <cstring>
#include <limits>
#include <oascpp/value.hpp>

namespace oascpp {
namespace detail {

namespace {

NumberOrder compare_signed_real(std::int64_t integer, double real) noexcept {
  if (std::isnan(real)) {
    return NumberOrder::unordered;
  }
  if (real >= 0x1p63) {
    return NumberOrder::less;
  }
  if (real < -0x1p63) {
    return NumberOrder::greater;
  }

  const std::int64_t real_integer = static_cast<std::int64_t>(real);
  if (integer < real_integer) {
    return NumberOrder::less;
  }
  if (integer > real_integer) {
    return NumberOrder::greater;
  }
  if (real > static_cast<double>(real_integer)) {
    return NumberOrder::less;
  }
  if (real < static_cast<double>(real_integer)) {
    return NumberOrder::greater;
  }
  return NumberOrder::equal;
}

NumberOrder compare_unsigned_real(std::uint64_t integer, double real) noexcept {
  if (std::isnan(real)) {
    return NumberOrder::unordered;
  }
  if (real < 0.0) {
    return NumberOrder::greater;
  }
  if (real >= 0x1p64) {
    return NumberOrder::less;
  }

  const std::uint64_t real_integer = static_cast<std::uint64_t>(real);
  if (integer < real_integer) {
    return NumberOrder::less;
  }
  if (integer > real_integer) {
    return NumberOrder::greater;
  }
  if (real > static_cast<double>(real_integer)) {
    return NumberOrder::less;
  }
  if (real < static_cast<double>(real_integer)) {
    return NumberOrder::greater;
  }
  return NumberOrder::equal;
}

NumberOrder reverse(NumberOrder order) noexcept {
  if (order == NumberOrder::less) {
    return NumberOrder::greater;
  }
  if (order == NumberOrder::greater) {
    return NumberOrder::less;
  }
  return order;
}

}  // namespace

NumberOrder compare_numbers(const NumberView& lhs, const NumberView& rhs) noexcept {
  if (lhs.is_integer && rhs.is_integer) {
    if (!lhs.is_unsigned && !rhs.is_unsigned) {
      if (lhs.as_signed < rhs.as_signed) {
        return NumberOrder::less;
      }
      if (lhs.as_signed > rhs.as_signed) {
        return NumberOrder::greater;
      }
      return NumberOrder::equal;
    }
    if (lhs.is_unsigned && rhs.is_unsigned) {
      if (lhs.as_unsigned < rhs.as_unsigned) {
        return NumberOrder::less;
      }
      if (lhs.as_unsigned > rhs.as_unsigned) {
        return NumberOrder::greater;
      }
      return NumberOrder::equal;
    }
    if (lhs.is_unsigned) {
      if (rhs.as_signed < 0) {
        return NumberOrder::greater;
      }
      const std::uint64_t signed_value = static_cast<std::uint64_t>(rhs.as_signed);
      if (lhs.as_unsigned < signed_value) {
        return NumberOrder::less;
      }
      if (lhs.as_unsigned > signed_value) {
        return NumberOrder::greater;
      }
      return NumberOrder::equal;
    }
    return reverse(compare_numbers(rhs, lhs));
  }

  if (lhs.is_integer) {
    return lhs.is_unsigned ? compare_unsigned_real(lhs.as_unsigned, rhs.as_real)
                           : compare_signed_real(lhs.as_signed, rhs.as_real);
  }
  if (rhs.is_integer) {
    return reverse(rhs.is_unsigned ? compare_unsigned_real(rhs.as_unsigned, lhs.as_real)
                                   : compare_signed_real(rhs.as_signed, lhs.as_real));
  }
  if (std::isnan(lhs.as_real) || std::isnan(rhs.as_real)) {
    return NumberOrder::unordered;
  }
  if (lhs.as_real < rhs.as_real) {
    return NumberOrder::less;
  }
  if (lhs.as_real > rhs.as_real) {
    return NumberOrder::greater;
  }
  return NumberOrder::equal;
}

double decode_double(std::uint64_t bits) noexcept {
  double value = 0.0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::uint64_t encode_double(double value) noexcept {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::int64_t decode_signed(std::uint64_t bits) noexcept {
  std::int64_t value = 0;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace detail

std::size_t ParsedDocument::node_count() const noexcept { return nodes_.size(); }

bool ParsedDocument::empty() const noexcept { return nodes_.empty(); }

bool ValueView::valid() const noexcept { return doc_ != nullptr && index_ < doc_->nodes_.size(); }

bool ValueView::as_boolean() const noexcept {
  return valid() && node().kind == ValueKind::boolean && node().scalar != 0;
}

std::int64_t ValueView::as_integer() const noexcept { return as_number().as_signed; }

double ValueView::as_double() const noexcept { return as_number().as_real; }

std::string_view ValueView::as_string() const noexcept {
  if (!valid()) {
    return {};
  }
  const detail::Node& current = node();
  if (current.kind != ValueKind::string) {
    return {};
  }
  return std::string_view(doc_->chars_.data() + current.text_off, current.text_len);
}

std::string_view ValueView::key() const noexcept {
  if (!valid()) {
    return {};
  }
  const detail::Node& current = node();
  if (current.key_len == 0) {
    return {};
  }
  return std::string_view(doc_->chars_.data() + current.key_off, current.key_len);
}

std::size_t ValueView::size() const noexcept {
  if (!valid()) {
    return 0;
  }
  const detail::Node& current = node();
  if (current.kind != ValueKind::array && current.kind != ValueKind::object) {
    return 0;
  }
  return current.child_count;
}

bool ValueView::empty() const noexcept { return size() == 0; }

ValueView ValueView::at(std::size_t position) const noexcept {
  if (!valid()) {
    return {};
  }
  const detail::Node& current = node();
  if (current.kind != ValueKind::array || position >= current.child_count) {
    return {};
  }
  return child_at(position);
}

ValueView ValueView::find(std::string_view key_name) const noexcept {
  if (!valid()) {
    return {};
  }
  const detail::Node& current = node();
  if (current.kind != ValueKind::object) {
    return {};
  }
  for (std::uint32_t index = 0; index < current.child_count; ++index) {
    const std::uint32_t child = doc_->children_[static_cast<std::size_t>(current.child_off) + index];
    const detail::Node& candidate = doc_->nodes_[child];
    const std::string_view stored(doc_->chars_.data() + candidate.key_off, candidate.key_len);
    if (stored == key_name) {
      return ValueView(doc_, child);
    }
  }
  return {};
}

bool ValueView::contains(std::string_view key_name) const noexcept { return find(key_name).valid(); }

ValueView::Iterator ValueView::begin() const noexcept {
  if (!is_container()) {
    return Iterator();
  }
  return Iterator(*this, 0);
}

ValueView::Iterator ValueView::end() const noexcept {
  if (!is_container()) {
    return Iterator();
  }
  return Iterator(*this, size());
}

bool operator==(const ValueView& lhs, const ValueView& rhs) noexcept {
  return lhs.doc_ == rhs.doc_ && lhs.index_ == rhs.index_;
}

bool operator!=(const ValueView& lhs, const ValueView& rhs) noexcept { return !(lhs == rhs); }

const detail::Node& ValueView::node() const noexcept { return doc_->nodes_[index_]; }

ValueView ValueView::child_at(std::size_t position) const noexcept {
  const detail::Node& current = node();
  return ValueView(doc_, doc_->children_[static_cast<std::size_t>(current.child_off) + position]);
}

NumberView ValueView::as_number() const noexcept {
  NumberView out;
  if (!valid()) {
    return out;
  }
  const detail::Node& current = node();
  switch (current.kind) {
    case ValueKind::integer: {
      out.is_integer = true;
      out.is_unsigned = (current.flags & detail::node_flag_unsigned) != 0;
      out.as_unsigned = current.scalar;
      const bool fits_signed = current.scalar <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
      out.as_signed = out.is_unsigned ? (fits_signed ? static_cast<std::int64_t>(current.scalar) : 0)
                                      : detail::decode_signed(current.scalar);
      out.as_real = out.is_unsigned ? static_cast<double>(current.scalar) : static_cast<double>(out.as_signed);
      break;
    }
    case ValueKind::real:
      out.is_integer = false;
      out.is_unsigned = false;
      out.as_real = detail::decode_double(current.scalar);
      break;
    default:
      break;
  }
  return out;
}

ValueView ParsedDocument::root() const noexcept {
  if (nodes_.empty()) {
    return ValueView();
  }
  return ValueView(this, 0);
}

bool deep_equal(const ValueView& lhs, const ValueView& rhs) noexcept {
  const bool lhs_valid = lhs.valid();
  const bool rhs_valid = rhs.valid();
  if (!lhs_valid || !rhs_valid) {
    return lhs_valid == rhs_valid;
  }

  const ValueKind lhs_kind = lhs.kind();
  const ValueKind rhs_kind = rhs.kind();
  const bool lhs_number = lhs_kind == ValueKind::integer || lhs_kind == ValueKind::real;
  const bool rhs_number = rhs_kind == ValueKind::integer || rhs_kind == ValueKind::real;

  if (lhs_number || rhs_number) {
    if (!(lhs_number && rhs_number)) {
      return false;
    }
    const NumberView a = lhs.as_number();
    const NumberView b = rhs.as_number();
    return detail::compare_numbers(a, b) == detail::NumberOrder::equal;
  }

  if (lhs_kind != rhs_kind) {
    return false;
  }
  switch (lhs_kind) {
    case ValueKind::null_value:
      return true;
    case ValueKind::boolean:
      return lhs.as_boolean() == rhs.as_boolean();
    case ValueKind::string:
      return lhs.as_string() == rhs.as_string();
    case ValueKind::array:
      if (lhs.size() != rhs.size()) {
        return false;
      }
      for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!deep_equal(lhs.at(index), rhs.at(index))) {
          return false;
        }
      }
      return true;
    case ValueKind::object:
      if (lhs.size() != rhs.size()) {
        return false;
      }
      for (const ValueView member : lhs) {
        const ValueView other = rhs.find(member.key());
        if (!other.valid() || !deep_equal(member, other)) {
          return false;
        }
      }
      return true;
    default:
      return false;
  }
}

}  // namespace oascpp
