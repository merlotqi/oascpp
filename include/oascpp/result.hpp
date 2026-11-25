#pragma once

// Result<T>: a value or a single error. Used by cold, fallible entry points such
// as parsing, document loading and schema compilation, where accumulating a
// ValidationResult would be noise.

#include <cassert>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "oascpp/error.hpp"

namespace oascpp {

struct Error {
  ErrorCode code = ErrorCode::none;
  std::string message;
};

template <class T>
class Result {
 public:
  Result(T value) : storage_(std::move(value)) {}
  Result(Error error) : storage_(std::move(error)) {}

  [[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] T& value() & {
    assert(has_value());
    return std::get<T>(storage_);
  }

  [[nodiscard]] const T& value() const& {
    assert(has_value());
    return std::get<T>(storage_);
  }

  [[nodiscard]] T&& value() && {
    assert(has_value());
    return std::get<T>(std::move(storage_));
  }

  [[nodiscard]] const Error& error() const& {
    assert(!has_value());
    return std::get<Error>(storage_);
  }

 private:
  std::variant<T, Error> storage_;
};

template <>
class Result<void> {
 public:
  Result() noexcept = default;
  Result(Error error) : error_(std::move(error)) {}

  [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }
  explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] const Error& error() const& {
    assert(!has_value());
    return *error_;
  }

 private:
  std::optional<Error> error_;
};

}  // namespace oascpp
