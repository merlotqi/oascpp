#pragma once

// Minimal self-contained test harness: one registry, one main, no third-party
// dependency. Tests register themselves through OASCPP_TEST.

#include <iostream>
#include <vector>

namespace oascpp_test {

struct Case {
  const char* name;
  void (*run)();
};

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

inline int& failures() {
  static int count = 0;
  return count;
}

struct Registrar {
  Registrar(const char* name, void (*run)()) { registry().push_back({name, run}); }
};

template <class Actual, class Expected>
void check_equal(const Actual& actual, const Expected& expected, const char* file, int line, const char* expression) {
  if (!(actual == expected)) {
    ++failures();
    std::cerr << file << ':' << line << ": expectation failed: " << expression << '\n'
              << "  actual:   " << actual << '\n'
              << "  expected: " << expected << '\n';
  }
}

}  // namespace oascpp_test

#define OASCPP_TEST(name)                                                                 \
  static void name();                                                                     \
  [[maybe_unused]] static ::oascpp_test::Registrar oascpp_registrar_##name(#name, &name); \
  static void name()

#define OASCPP_CHECK(condition)                                                          \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      ++::oascpp_test::failures();                                                       \
      std::cerr << __FILE__ << ':' << __LINE__ << ": check failed: " #condition << '\n'; \
    }                                                                                    \
  } while (false)

#define OASCPP_CHECK_EQ(actual, expected) \
  ::oascpp_test::check_equal((actual), (expected), __FILE__, __LINE__, #actual " == " #expected)
