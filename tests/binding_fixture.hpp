#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace oascpp {
class Bindings;
}

namespace oascpp_test {

struct User {
  struct Address {
    std::int32_t postcode;
  };

  std::string name;
  Address home;
  std::vector<Address> addresses;
  std::vector<std::vector<std::int32_t>> scores;
  std::optional<Address> office;
  std::vector<bool> flags;
};

void register_user_bindings(oascpp::Bindings& bindings);

}  // namespace oascpp_test
