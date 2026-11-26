#include "binding_fixture.hpp"
#include "oascpp/binding.hpp"

namespace oascpp_test {

void register_user_bindings(oascpp::Bindings& bindings) {
  using oascpp::field;
  OASCPP_BIND(bindings, User::Address, field("postal-code", &User::Address::postcode));
  OASCPP_BIND(bindings, User, field("name", &User::name), field("home", &User::home),
              field("addresses", &User::addresses), field("scores", &User::scores),
              field("office", &User::office), field("flags", &User::flags));
}

}  // namespace oascpp_test
