cmake_minimum_required(VERSION 3.25)
project(gcnport VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GCPORT_BUILD_TESTS "Build gcnport focused tests" ON)
# OFF by default because turning it on compiles the whole pinned Dolphin fork. The pure gcnport
# library -- the execution policy, hook registry, original-call coordinator and their types -- has
# no Dolphin dependency and never will; the adapter that binds them to Dolphin's RuntimeSession is
# what this option builds.
option(GCPORT_BUILD_DOLPHIN_ADAPTER
  "Build the Dolphin-backed adapter target (compiles the pinned Dolphin fork)" OFF)

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

if(GCPORT_BUILD_DOLPHIN_ADAPTER)
  gcnport_add_dolphin_core(
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/dolphin" "${CMAKE_CURRENT_BINARY_DIR}/dolphin")

  add_library(gcnport_dolphin src/dolphin_adapter.cpp)
  add_library(gcnport::dolphin ALIAS gcnport_dolphin)
  target_link_libraries(gcnport_dolphin PUBLIC gcnport::gcnport core uicommon)
  target_include_directories(gcnport_dolphin PUBLIC
    "${GCPORT_DOLPHIN_SOURCE_CORE_DIR}"
    "${GCPORT_DOLPHIN_BINARY_CORE_DIR}"
  )
  gcnport_apply_dolphin_architecture(gcnport_dolphin)

  # Deliberately not installed or exported. It links Dolphin's `core`/`uicommon`, which Dolphin
  # does not install or put in any export set, so an exported gcnport_dolphin would name targets a
  # downstream find_package() could never resolve. Consumers take this target by adding gcnport as
  # a subdirectory, which is how they get the pinned Dolphin fork in the first place.
  if(GCPORT_BUILD_TESTS)
    include(CTest)
    # `core` calls back into a set of Host_* free functions a real frontend normally implements for
    # UI events. Dolphin's own test binary faces the identical requirement and answers it with its
    # reusable, frontend-neutral StubHost.cpp; this links the same file rather than carrying a
    # bespoke copy of it.
    add_executable(gcnport_dolphin_adapter_test
      tests/dolphin_adapter_test.cpp
      "${GCPORT_DOLPHIN_STUB_HOST}"
    )
    target_link_libraries(gcnport_dolphin_adapter_test PRIVATE gcnport::dolphin)
    target_include_directories(gcnport_dolphin_adapter_test PRIVATE tests)
    gcnport_stage_dolphin_sys(gcnport_dolphin_adapter_test "${GCPORT_DOLPHIN_SYS_DIR}")
    add_test(NAME dolphin_adapter COMMAND gcnport_dolphin_adapter_test)
  endif()
endif()

if(GCPORT_BUILD_TESTS)
  include(CTest)

  function(gcnport_add_test target test_name source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE gcnport::gcnport)
    target_include_directories(${target} PRIVATE tests)
    add_test(NAME ${test_name} COMMAND ${target})
  endfunction()

  gcnport_add_test(
    gcnport_execution_session_test execution_session tests/execution_session_test.cpp
  )
  gcnport_add_test(gcnport_native_hooks_test native_hooks tests/native_hooks_test.cpp)
  gcnport_add_test(gcnport_test_support_test test_support tests/test_support_test.cpp)
endif()

install(TARGETS gcnport EXPORT gcnport-targets)
install(DIRECTORY include/ DESTINATION include)
install(EXPORT gcnport-targets NAMESPACE gcnport:: DESTINATION lib/cmake/gcnport)
