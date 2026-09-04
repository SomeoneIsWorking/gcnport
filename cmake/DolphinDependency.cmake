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
