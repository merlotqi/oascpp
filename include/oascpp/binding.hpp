#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <oascpp/error.hpp>
#include <oascpp/value.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace oascpp {

template <class T>
class DecodeResult {
 public:
  [[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] T& value() & { return std::get<0>(storage_); }
  [[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
  [[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
  [[nodiscard]] const ValidationResult& diagnostics() const& { return std::get<1>(storage_); }

 private:
  friend class Bindings;

  DecodeResult(std::in_place_index_t<0>, T value) : storage_(std::in_place_index<0>, std::move(value)) {}
  DecodeResult(std::in_place_index_t<1>, ValidationResult errors)
      : storage_(std::in_place_index<1>, std::move(errors)) {}

  std::variant<T, ValidationResult> storage_;
};

template <class Owner, class Member>
struct BoundField {
  std::string name;
  Member Owner::* member;
};

template <class Owner, class Member>
[[nodiscard]] BoundField<Owner, Member> field(std::string_view name, Member Owner::* member) {
  return {std::string(name), member};
}

namespace detail {

template <class T>
inline constexpr bool is_bound_optional = false;

template <class T>
inline constexpr bool is_bound_optional<std::optional<T>> = true;

template <class T>
inline constexpr bool is_bound_vector = false;

template <class T, class Allocator>
inline constexpr bool is_bound_vector<std::vector<T, Allocator>> = true;

struct DecodeContext {
  ErrorPath path;
  ValidationResult errors;

  bool report(ErrorCode code, std::string_view message, ValueView input, std::string_view expected = {}) {
    ValidationError error;
    error.code = code;
    error.instance_path = path;
    error.message = message;
    error.expected = expected;
    error.actual = input.valid() ? to_string(input.kind()) : std::string_view("missing");
    errors.add_error(std::move(error));
    return false;
  }
};

template <class T>
bool assign_integer(NumberView number, T& output) {
  if (!number.is_integer) {
    const double value = number.as_real;
    const double low = static_cast<double>(std::numeric_limits<T>::lowest());
    const double high_exclusive = std::ldexp(1.0, std::numeric_limits<T>::digits);
    if (!(value >= low && value < high_exclusive && std::trunc(value) == value)) {
      return false;
    }
    output = static_cast<T>(value);
    return true;
  }
  if (number.is_unsigned) {
    if (number.as_unsigned > static_cast<std::uintmax_t>(std::numeric_limits<T>::max())) {
      return false;
    }
    output = static_cast<T>(number.as_unsigned);
    return true;
  }
  if constexpr (std::is_signed_v<T>) {
    if (number.as_signed < std::numeric_limits<T>::lowest() || number.as_signed > std::numeric_limits<T>::max()) {
      return false;
    }
  } else {
    if (number.as_signed < 0 || static_cast<std::uintmax_t>(number.as_signed) > std::numeric_limits<T>::max()) {
      return false;
    }
  }
  output = static_cast<T>(number.as_signed);
  return true;
}

}  // namespace detail

// Populate the registry at application startup. Const decodes can share it
// once registration is complete.
class Bindings {
 public:
  template <class T, class... Fields>
  void bind(Fields... fields) {
    const auto decode = [fields...](const Bindings& bindings, detail::DecodeContext& context, ValueView input,
                                    void* destination) {
      auto& output = *static_cast<T*>(destination);
      static_cast<void>(output);
      bool ok = true;
      ((ok = bindings.decode_member(context, input, output, fields) && ok), ...);
      return ok;
    };
    if (!decoders_.emplace(typeid(T), decode).second) {
      throw std::logic_error("oascpp: C++ type is already registered");
    }
  }

  template <class T>
  [[nodiscard]] DecodeResult<T> decode(ValueView input) const {
    detail::DecodeContext context;
    T output{};
    if (decode_value(context, input, output)) {
      return DecodeResult<T>(std::in_place_index<0>, std::move(output));
    }
    return DecodeResult<T>(std::in_place_index<1>, std::move(context.errors));
  }

 private:
  template <class T, class Owner, class Member>
  bool decode_member(detail::DecodeContext& context, ValueView input, T& output,
                     const BoundField<Owner, Member>& field) const {
    context.path.push_key(field.name);
    const bool ok = decode_value(context, input.find(field.name), output.*field.member);
    context.path.pop_back();
    return ok;
  }

  template <class T>
  bool decode_value(detail::DecodeContext& context, ValueView input, T& output) const {
    if (!input.valid() && !detail::is_bound_optional<T>) {
      return context.report(ErrorCode::missing_field, "value needed by the C++ binding is missing", input);
    }
    if constexpr (detail::is_bound_optional<T>) {
      if (!input.valid() || input.is_null()) {
        output.reset();
        return true;
      }
      return decode_value(context, input, output.emplace());
    } else if constexpr (std::is_same_v<T, bool>) {
      if (!input.is_boolean()) {
        return context.report(ErrorCode::type_mismatch, "expected a boolean", input, "boolean");
      }
      output = input.as_boolean();
      return true;
    } else if constexpr (std::is_integral_v<T>) {
      if (!input.is_number()) {
        return context.report(ErrorCode::type_mismatch, "expected an integer", input, "integer");
      }
      if (!detail::assign_integer(input.as_number(), output)) {
        return context.report(ErrorCode::invalid_value, "value cannot be represented as the C++ integer type", input,
                              "integer in range");
      }
      return true;
    } else if constexpr (std::is_floating_point_v<T>) {
      if (!input.is_number()) {
        return context.report(ErrorCode::type_mismatch, "expected a number", input, "number");
      }
      const double value = input.as_double();
      if (!std::isfinite(value) || value < std::numeric_limits<T>::lowest() || value > std::numeric_limits<T>::max()) {
        return context.report(ErrorCode::invalid_value, "value is outside the C++ floating-point range", input,
                              "finite number in range");
      }
      output = static_cast<T>(value);
      return true;
    } else if constexpr (std::is_same_v<T, std::string>) {
      if (!input.is_string()) {
        return context.report(ErrorCode::type_mismatch, "expected a string", input, "string");
      }
      output = input.as_string();
      return true;
    } else if constexpr (detail::is_bound_vector<T>) {
      if (!input.is_array()) {
        return context.report(ErrorCode::type_mismatch, "expected an array", input, "array");
      }
      output.clear();
      output.reserve(input.size());
      bool ok = true;
      std::size_t index = 0;
      for (ValueView child : input) {
        context.path.push_index(index++);
        typename T::value_type element{};
        if (decode_value(context, child, element)) {
          output.push_back(std::move(element));
        } else {
          ok = false;
        }
        context.path.pop_back();
      }
      return ok;
    } else {
      static_assert(std::is_class_v<T>, "Unsupported oascpp binding type");
      const auto decoder = decoders_.find(typeid(T));
      if (decoder == decoders_.end()) {
        return context.report(ErrorCode::binding_not_registered, "C++ type has no registered object binding", input);
      }
      if (!input.is_object()) {
        return context.report(ErrorCode::type_mismatch, "expected an object", input, "object");
      }
      return decoder->second(*this, context, input, std::addressof(output));
    }
  }

  using Decoder = std::function<bool(const Bindings&, detail::DecodeContext&, ValueView, void*)>;
  std::unordered_map<std::type_index, Decoder> decoders_;
};

}  // namespace oascpp

#define OASCPP_BIND(registry, type, ...) (registry).bind<type>(__VA_ARGS__)
