#include <oascpp/error.hpp>

namespace oascpp {

ErrorPath& ErrorPath::push_key(std::string_view key) {
  Token token;
  if (!key.empty()) {
    token.key.assign(key.data(), key.size());
  }
  tokens_.push_back(std::move(token));
  return *this;
}

ErrorPath& ErrorPath::push_index(std::size_t index) {
  Token token;
  token.index = index;
  token.is_index = true;
  tokens_.push_back(std::move(token));
  return *this;
}

void ErrorPath::pop_back() noexcept {
  if (!tokens_.empty()) {
    tokens_.pop_back();
  }
}

bool ErrorPath::empty() const noexcept { return tokens_.empty(); }

std::size_t ErrorPath::depth() const noexcept { return tokens_.size(); }

const std::vector<ErrorPath::Token>& ErrorPath::tokens() const noexcept { return tokens_; }

std::string ErrorPath::to_string() const {
  std::string out;
  for (const Token& token : tokens_) {
    out.push_back('/');
    if (token.is_index) {
      out.append(std::to_string(token.index));
    } else {
      append_escaped_pointer_token(out, token.key);
    }
  }
  return out;
}

bool ValidationResult::ok() const noexcept { return errors_.empty(); }

ValidationResult::operator bool() const noexcept { return ok(); }

std::size_t ValidationResult::error_count() const noexcept { return errors_.size(); }

bool ValidationResult::empty() const noexcept { return errors_.empty(); }

const std::vector<ValidationError>& ValidationResult::errors() const noexcept { return errors_; }

const ValidationError* ValidationResult::first_error() const noexcept {
  return errors_.empty() ? nullptr : &errors_.front();
}

ErrorCode ValidationResult::first_error_code() const noexcept {
  return errors_.empty() ? ErrorCode::none : errors_.front().code;
}

void ValidationResult::add_error(ValidationError error) { errors_.push_back(std::move(error)); }

void ValidationResult::reserve(std::size_t count) { errors_.reserve(count); }

void ValidationResult::add(ErrorCode code) {
  ValidationError error;
  error.code = code;
  errors_.push_back(std::move(error));
}

void ValidationResult::clear() noexcept { errors_.clear(); }

}  // namespace oascpp
