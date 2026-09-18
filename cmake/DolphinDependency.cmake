# SPDX-License-Identifier: GPL-2.0-or-later

function(gcnport_require_dolphin_checkout checkout)
  file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../dependencies.json" dependency_manifest)
  string(JSON expected_revision GET "${dependency_manifest}" dolphin revision)

  if(NOT EXISTS "${checkout}/.git")
    message(FATAL_ERROR
      "gcnport requires its maintained Dolphin submodule at ${checkout}; "
      "initialize extern/dolphin before configuring the Dolphin adapter")
  endif()

  execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${checkout}"
    RESULT_VARIABLE revision_result
    OUTPUT_VARIABLE actual_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT revision_result EQUAL 0 OR
     NOT "${actual_revision}" STREQUAL "${expected_revision}")
    message(FATAL_ERROR
      "gcnport Dolphin revision mismatch: expected ${expected_revision}, found ${actual_revision}")
  endif()
endfunction()

# Reads the embedded-build option names from dependencies.json into two output variables. An empty
# list would silently configure a full Dolphin build -- frontend, audio backends and all -- so a
# missing or empty entry is refused by name here rather than discovered as a twenty-minute compile.
function(gcnport_read_embedded_dolphin_options off_output linux_off_output)
  file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../dependencies.json" dependency_manifest)
  foreach(member_and_output IN ITEMS "off;${off_output}" "linux_off;${linux_off_output}")
    list(GET member_and_output 0 member)
    list(GET member_and_output 1 output)
    string(JSON option_array ERROR_VARIABLE json_error
      GET "${dependency_manifest}" dolphin embedded_build_options "${member}")
    if(json_error OR "${option_array}" STREQUAL "")
      message(FATAL_ERROR
        "dependencies.json has no dolphin.embedded_build_options.${member} array: ${json_error}")
    endif()
    string(JSON option_count LENGTH "${option_array}")
    if(option_count EQUAL 0)
      message(FATAL_ERROR "dolphin.embedded_build_options.${member} is empty")
    endif()
    set(option_names "")
    math(EXPR last_index "${option_count} - 1")
    foreach(index RANGE ${last_index})
      string(JSON option_name GET "${option_array}" ${index})
      list(APPEND option_names "${option_name}")
    endforeach()
    set(${output} "${option_names}" PARENT_SCOPE)
  endforeach()
endfunction()

# Adds the pinned Dolphin fork as a real CMake subdirectory and, with it, the `core`/`common`/
# `uicommon` targets the Dolphin adapter links. This is the single source of truth for which slice
# of Dolphin an embedding build compiles: only Core and its dependency graph, with no Qt/NoGUI/CLI
# frontend, no unrelated audio or video backends, and no Dolphin gtest binary. A consuming title
# calls this (or links `gcnport::dolphin`, which requires it) instead of carrying its own copy of
# the option list -- Sunbright's did, with a comment telling its reader to keep the two in sync by
# hand, which is the kind of instruction that is obeyed exactly until it is not.
#
# EXCLUDE_FROM_ALL: an ordinary `cmake --build` in the consuming project must not silently start
# compiling the whole Dolphin fork. Only a target that actually links it pulls it in.
function(gcnport_add_dolphin_core checkout binary_dir)
  gcnport_require_dolphin_checkout("${checkout}")

  if(NOT EXISTS "${checkout}/CMakeLists.txt")
    message(FATAL_ERROR
      "gcnport requires the maintained Dolphin fork's CMakeLists at ${checkout}; "
      "run 'git submodule update --init --recursive' first")
  endif()

  # dependencies.json is the one place the embedded option set is written down; this reads it
  # rather than restating it, so the adapter build and tools/gcnport_tools/dolphin_runtime.py
  # cannot drift apart.
  gcnport_read_embedded_dolphin_options(off_options linux_off_options)
  foreach(option_name IN LISTS off_options)
    set(${option_name} OFF CACHE BOOL "" FORCE)
  endforeach()
  # The adapter has no use for Dolphin's own gtest binary; the standalone runtime build turns this
  # on for exactly that reason, which is why it is not part of the shared list.
  set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(option_name IN LISTS linux_off_options)
      set(${option_name} OFF CACHE BOOL "" FORCE)
    endforeach()
  endif()

  # Dolphin resolves its own shipped data -- the GameCube IPL font substitutes, the DSP ROM and its
  # coefficient tables -- through File::GetSysDirectory(). On Linux that is a bare relative "sys/"
  # unless LINUX_LOCAL_DEV is set, in which case it is a Sys directory beside the executable, which
  # is how Dolphin's own from-source development builds work. Left unresolved, a booted title's
  # OSGetFontTexture path reads through null font pointers. Common is where that path compiles, so
  # this has to be set before Dolphin's own CMakeLists is processed.
  if(UNIX AND NOT APPLE)
    set(LINUX_LOCAL_DEV ON CACHE BOOL "" FORCE)
  endif()

  add_subdirectory("${checkout}" "${binary_dir}" EXCLUDE_FROM_ALL)

  # Cached rather than PARENT_SCOPE: a consumer that adds gcnport as a subdirectory is more than
  # one scope away from here, and these are the two paths it cannot derive for itself -- Dolphin's
  # shipped runtime data, and the frontend-neutral Host_* stub any executable linking `core` needs.
  set(GCPORT_DOLPHIN_SYS_DIR "${checkout}/Data/Sys" CACHE INTERNAL
    "Dolphin's shipped runtime data, staged beside an executable that boots a title")
  set(GCPORT_DOLPHIN_STUB_HOST "${checkout}/Source/UnitTests/StubHost.cpp" CACHE INTERNAL
    "Dolphin's reusable frontend-neutral Host_* implementation")
  set(GCPORT_DOLPHIN_SOURCE_CORE_DIR "${checkout}/Source/Core" PARENT_SCOPE)
  set(GCPORT_DOLPHIN_BINARY_CORE_DIR "${binary_dir}/Source/Core" PARENT_SCOPE)
endfunction()

# Links the Dolphin data `target` resolves at runtime next to its executable. The checkout's own
# Data/Sys is symlinked rather than copied, so it tracks the submodule and a stale copy of the DSP
# ROM or the font tables cannot go unnoticed.
function(gcnport_stage_dolphin_sys target sys_dir)
  if(NOT EXISTS "${sys_dir}/GC/font_western.bin")
    message(FATAL_ERROR
      "${target}: no Dolphin Sys data at '${sys_dir}'. Without it a booted title's font and DSP "
      "paths read through null pointers. Check out gcnport's extern/dolphin submodule.")
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E create_symlink
            "${sys_dir}" "$<TARGET_FILE_DIR:${target}>/Sys"
    VERBATIM)
endfunction()

# Dolphin's own top-level CMakeLists selects C++23 and defines the _ARCH_64/_M_X86_64/_M_ARM_64
# architecture macros through directory-scoped set()/add_definitions() calls. Those are directory
# properties, not usage requirements of `core`, so a target in another directory gets none of them
# from target_link_libraries() -- and Dolphin's public headers (Core/MachineContext.h among them)
# need the macros defined by the time they are parsed. Applying them PUBLIC here is what lets a
# consumer link one target and be done. The selection mirrors Dolphin's own rather than assuming a
# single architecture, and refuses anything Dolphin itself would refuse.
function(gcnport_apply_dolphin_architecture target)
  target_compile_features(${target} PUBLIC cxx_std_23)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x64|x86_64|amd64|AMD64")
    target_compile_definitions(${target} PUBLIC _ARCH_64=1 _M_X86_64=1)
  elseif(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_definitions(${target} PUBLIC _ARCH_64=1 _M_ARM_64=1)
  else()
    message(FATAL_ERROR
      "${target}: unsupported architecture '${CMAKE_SYSTEM_PROCESSOR}' with "
      "${CMAKE_SIZEOF_VOID_P}-byte pointers (mirrors Dolphin's own supported-architecture check)")
  endif()
endfunction()
