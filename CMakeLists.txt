cmake_minimum_required(VERSION 3.25)
project(gcnport VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GCPORT_BUILD_TESTS "Build gcnport focused tests" ON)

include(cmake/DolphinDependency.cmake)
gcnport_require_dolphin_checkout("${CMAKE_CURRENT_SOURCE_DIR}/extern/dolphin")

add_library(gcnport
  src/execution_session.cpp
  src/execution_types.cpp
  src/fallback_ledger.cpp
  src/native_hooks.cpp
  src/original_calls.cpp
)
add_library(gcnport::gcnport ALIAS gcnport)
target_include_directories(gcnport
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(gcnport PUBLIC cxx_std_20)

if(GCPORT_BUILD_TESTS)
  include(CTest)

  add_executable(gcnport_execution_session_test tests/execution_session_test.cpp)
  target_link_libraries(gcnport_execution_session_test PRIVATE gcnport::gcnport)
  target_include_directories(gcnport_execution_session_test PRIVATE tests)
  add_test(NAME execution_session COMMAND gcnport_execution_session_test)

  add_executable(gcnport_native_hooks_test tests/native_hooks_test.cpp)
  target_link_libraries(gcnport_native_hooks_test PRIVATE gcnport::gcnport)
  target_include_directories(gcnport_native_hooks_test PRIVATE tests)
  add_test(NAME native_hooks COMMAND gcnport_native_hooks_test)
endif()

install(TARGETS gcnport EXPORT gcnport-targets)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT gcnport-targets NAMESPACE gcnport:: DESTINATION lib/cmake/gcnport)
