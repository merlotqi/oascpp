#pragma once

// nlohmann/json adapter. The header compiles to nothing unless nlohmann/json is
// reachable, so the core library keeps zero third-party dependencies.

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

#include "oascpp/document_builder.hpp"
#include "oascpp/error.hpp"
#include "oascpp/result.hpp"
#include "oascpp/value.hpp"

#if defined(__has_include)
#if __has_include(<nlohmann/json.hpp>)
#define OASCPP_HAS_NLOHMANN_JSON 1
#endif
#endif

#if !defined(OASCPP_HAS_NLOHMANN_JSON)
#define OASCPP_HAS_NLOHMANN_JSON 0
#endif

#if OASCPP_HAS_NLOHMANN_JSON

#include <nlohmann/json.hpp>

namespace oascpp {
namespace adapters {
namespace nlohmann_json {

// Translates one backend value into arena nodes, recursively. Arrays and objects
// keep backend order; object member names are copied into the arena.
inline DocumentBuilder::NodeId build_node(DocumentBuilder& builder, const ::nlohmann::json& value) {
  switch (value.type()) {
    case ::nlohmann::json::value_t::null:
      return builder.make_null();
    case ::nlohmann::json::value_t::boolean:
      return builder.make_boolean(value.get<bool>());
    case ::nlohmann::json::value_t::number_unsigned:
      return builder.make_unsigned(value.get<std::uint64_t>());
    case ::nlohmann::json::value_t::number_integer:
      return builder.make_integer(value.get<std::int64_t>());
    case ::nlohmann::json::value_t::number_float:
      return builder.make_real(value.get<double>());
    case ::nlohmann::json::value_t::string: {
      const ::nlohmann::json::string_t& text = value.get_ref<const ::nlohmann::json::string_t&>();
      return builder.make_string(std::string_view(text.data(), text.size()));
    }
    case ::nlohmann::json::value_t::array: {
      const DocumentBuilder::NodeId id = builder.make_array();
      for (const ::nlohmann::json& element : value) {
        builder.append(id, build_node(builder, element));
      }
      return id;
    }
    case ::nlohmann::json::value_t::object: {
      const DocumentBuilder::NodeId id = builder.make_object();
      for (auto member = value.begin(); member != value.end(); ++member) {
        const ::nlohmann::json::string_t& key = member.key();
        builder.append_member(id, std::string_view(key.data(), key.size()), build_node(builder, member.value()));
      }
      return id;
    }
    default:
      break;
  }
  return builder.make_null();
}

// Parses JSON text into an arena document. Backend exceptions propagate; use
// try_parse to get a Result instead.
inline ParsedDocument parse(std::string_view input) {
  const ::nlohmann::json parsed = ::nlohmann::json::parse(input.begin(), input.end());

  DocumentBuilder builder;
  builder.reserve(input.size() / 16 + 4, input.size());
  const DocumentBuilder::NodeId root = build_node(builder, parsed);
  static_cast<void>(root);
  return builder.finish();
}

// Non-throwing variant. Only backend failures are reported; a successful parse of
// valid JSON always produces a document.
inline Result<ParsedDocument> try_parse(std::string_view input) noexcept {
  try {
    return parse(input);
  } catch (const std::exception& failure) {
    return Error{ErrorCode::parse_error, failure.what()};
  } catch (...) {
    return Error{ErrorCode::parse_error, "unknown parse failure"};
  }
}

}  // namespace nlohmann_json
}  // namespace adapters
}  // namespace oascpp

#endif  // OASCPP_HAS_NLOHMANN_JSON
