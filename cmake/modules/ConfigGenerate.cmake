# SPDX-FileCopyrightText: Copyright (C) 2021-2026 Software Radio Systems Limited
# SPDX-License-Identifier: BSD-3-Clause-Open-MPI

# ConfigGenerate.cmake
# Provides generate_config_schema() which registers an add_custom_command that
# re-runs the Python generator whenever the schema YAML file changes.
#
# Usage (in a component CMakeLists.txt):
#   include(ConfigGenerate)
#   generate_config_schema(SCHEMA ${CMAKE_SOURCE_DIR}/apps/config/schemas/foo.schema.yaml)
#
# The generated files are written in-source (next to the existing .h/.cpp files)
# and committed to git. The custom command only fires when the schema is newer.

find_package(Python3 COMPONENTS Interpreter QUIET)

function(generate_config_schema)
  cmake_parse_arguments(ARG "" "SCHEMA" "" ${ARGN})

  if(NOT ARG_SCHEMA)
    message(FATAL_ERROR "generate_config_schema: SCHEMA argument is required")
  endif()

  if(NOT Python3_FOUND)
    message(WARNING "generate_config_schema: Python3 not found, skipping auto-regeneration of ${ARG_SCHEMA}")
    return()
  endif()

  set(GENERATOR "${CMAKE_SOURCE_DIR}/tools/config_gen/generate.py")

  # Extract output paths from the schema at configure time.
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c
      "import yaml; d=yaml.safe_load(open('${ARG_SCHEMA}')); o=d['outputs']; print(o['header']); print(o['cli11_source']); print(o['cli11_header'])"
    OUTPUT_VARIABLE _schema_outputs_raw
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _parse_result
    ERROR_VARIABLE  _parse_error
  )

  if(NOT _parse_result EQUAL 0)
    message(FATAL_ERROR "generate_config_schema: failed to parse ${ARG_SCHEMA}:\n${_parse_error}")
  endif()

  string(REPLACE "\n" ";" _schema_outputs_list "${_schema_outputs_raw}")
  list(GET _schema_outputs_list 0 _out_header)
  list(GET _schema_outputs_list 1 _out_cli11_source)
  list(GET _schema_outputs_list 2 _out_cli11_header)

  set(_out_header      "${CMAKE_SOURCE_DIR}/${_out_header}")
  set(_out_cli11_src   "${CMAKE_SOURCE_DIR}/${_out_cli11_source}")
  set(_out_cli11_hdr   "${CMAKE_SOURCE_DIR}/${_out_cli11_header}")

  add_custom_command(
    OUTPUT  ${_out_header} ${_out_cli11_src} ${_out_cli11_hdr}
    COMMAND ${Python3_EXECUTABLE} ${GENERATOR} ${ARG_SCHEMA}
    DEPENDS ${ARG_SCHEMA} ${GENERATOR}
    COMMENT "Generating C++ from ${ARG_SCHEMA}"
    VERBATIM
  )

  set_source_files_properties(
    ${_out_header} ${_out_cli11_src} ${_out_cli11_hdr}
    PROPERTIES GENERATED TRUE
  )
endfunction()
