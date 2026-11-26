#pragma once

// A tiny value DSL for tests. It builds JSON documents through DocumentBuilder
// instead of parsing text, which keeps the core library free of a parser while
// still letting tests spell documents in a readable shape.

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "oascpp/document_builder.hpp"
#include "oascpp/value.hpp"

namespace oascpp_test {

class Json {
 public:
  enum class Kind : std::uint8_t { null_value, boolean, integer, real, string, array, object };

  Json() = default;
  Json(std::nullptr_t) {}  // NOLINT: nullptr means JSON null
  Json(bool value) : kind_(Kind::boolean), boolean_(value) {}
  Json(int value) : kind_(Kind::integer), integer_(value) {}
  Json(long long value) : kind_(Kind::integer), integer_(value) {}
  Json(std::size_t value) : kind_(Kind::integer), integer_(static_cast<long long>(value)) {}
  Json(double value) : kind_(Kind::real), real_(value) {}
  Json(const char* value) : kind_(Kind::string), string_(value) {}
  Json(std::string_view value) : kind_(Kind::string), string_(value) {}
  Json(std::string value) : kind_(Kind::string), string_(std::move(value)) {}

  [[nodiscard]] static Json array(std::initializer_list<Json> items) {
    Json value;
    value.kind_ = Kind::array;
    value.items_.assign(items.begin(), items.end());
    return value;
  }

  [[nodiscard]] static Json object(std::initializer_list<std::pair<std::string_view, Json>> members) {
    Json value;
    value.kind_ = Kind::object;
    value.members_.reserve(members.size());
    for (const auto& member : members) {
      value.members_.emplace_back(std::string(member.first), member.second);
    }
    return value;
  }

  [[nodiscard]] oascpp::DocumentBuilder::NodeId build(oascpp::DocumentBuilder& builder) const {
    switch (kind_) {
      case Kind::null_value:
        return builder.make_null();
      case Kind::boolean:
        return builder.make_boolean(boolean_);
      case Kind::integer:
        return builder.make_integer(integer_);
      case Kind::real:
        return builder.make_real(real_);
      case Kind::string:
        return builder.make_string(string_);
      case Kind::array: {
        const oascpp::DocumentBuilder::NodeId id = builder.make_array();
        for (const Json& item : items_) {
          static_cast<void>(builder.append(id, item.build(builder)));
        }
        return id;
      }
      case Kind::object: {
        const oascpp::DocumentBuilder::NodeId id = builder.make_object();
        for (const auto& member : members_) {
          static_cast<void>(builder.append_member(id, member.first, member.second.build(builder)));
        }
        return id;
      }
    }
    return builder.make_null();
  }

 private:
  Kind kind_ = Kind::null_value;
  bool boolean_ = false;
  long long integer_ = 0;
  double real_ = 0.0;
  std::string string_;
  std::vector<Json> items_;
  std::vector<std::pair<std::string, Json>> members_;
};

[[nodiscard]] inline oascpp::ParsedDocument make_document(const Json& value) {
  oascpp::DocumentBuilder builder;
  static_cast<void>(value.build(builder));
  return builder.finish();
}

}  // namespace oascpp_test
