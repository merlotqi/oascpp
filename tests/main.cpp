#include <iostream>

#include "test_framework.hpp"

int main() {
  const std::vector<oascpp_test::Case>& cases = oascpp_test::registry();
  int failed_cases = 0;

  for (const oascpp_test::Case& test_case : cases) {
    oascpp_test::failures() = 0;
    test_case.run();
    if (oascpp_test::failures() != 0) {
      ++failed_cases;
      std::cout << "[FAIL] " << test_case.name << '\n';
    } else {
      std::cout << "[ ok ] " << test_case.name << '\n';
    }
  }

  std::cout << cases.size() << " test(s), " << failed_cases << " failed\n";
  return failed_cases == 0 ? 0 : 1;
}
