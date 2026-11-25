cmake_minimum_required(VERSION 3.16)

project(oascpp
  VERSION 0.1.0
  DESCRIPTION "Header-only OpenAPI 3.0/3.1 validation library for modern C++"
  LANGUAGES CXX)

include(GNUInstallDirs)

add_library(oascpp INTERFACE)
add_library(oascpp::oascpp ALIAS oascpp)

target_compile_features(oascpp INTERFACE cxx_std_17)
target_include_directories(oascpp INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

option(OASCPP_BUILD_TESTS "Build the oascpp test suite" ON)

if(OASCPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)

install(TARGETS oascpp EXPORT oascppTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT oascppTargets
  FILE oascppTargets.cmake
  NAMESPACE oascpp::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oascpp)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/oascpp-config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/oascpp-config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oascpp)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/oascpp-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/oascpp-config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/oascpp-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oascpp)
