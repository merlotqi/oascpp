add_executable(oascpp_tests
  main.cpp
  binding_fixture.cpp
  test_binding.cpp
  test_value.cpp
  test_json_pointer.cpp
  test_error.cpp
  test_result.cpp
  test_nlohmann_json.cpp
  test_schema.cpp)

target_link_libraries(oascpp_tests PRIVATE oascpp::oascpp)

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
