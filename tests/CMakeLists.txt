include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
if(MSVC)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(oascpp_tests
  binding_fixture.cpp
  test_binding.cpp
  test_value.cpp
  test_json_pointer.cpp
  test_error.cpp
  test_result.cpp
  test_nlohmann_json.cpp
  test_schema.cpp)

target_link_libraries(oascpp_tests PRIVATE oascpp::oascpp GTest::gtest_main)

if(MSVC)
  target_compile_options(oascpp_tests PRIVATE /W4)
else()
  target_compile_options(oascpp_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Optional: the nlohmann/json adapter compiles out cleanly when the dependency is
# missing, so this lookup is not required for the build to succeed.
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
  target_link_libraries(oascpp_tests PRIVATE nlohmann_json::nlohmann_json)
endif()

add_test(NAME oascpp_tests COMMAND oascpp_tests)